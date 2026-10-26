#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "gfx.h"
#include "input.h"
#include "platform.h"
#include "save.h"
#include "sound.h"

#ifdef HAVE_UPDATER
#include <wiiupdate.h>
#endif

#include "version.h"

#define TEX_SIZE 512
#define SPINNER_X 320.0f
#define SPINNER_Y 228.0f
#define SPINNER_SIZE 360.0f

#define MAX_RPM 3000.0f
#define FLICK_RPM 120.0f
#define STOP_DEG 4.0f // deg/s below which the spinner stops

typedef struct
{
    const char *name;
    uint8_t r, g, b;
} Preset;

static const Preset colors[] = {
    {"Yellow", 255, 214, 10},
    {"Red", 235, 38, 38},
    {"Orange", 255, 128, 16},
    {"Lime", 140, 225, 40},
    {"Green", 30, 170, 72},
    {"Cyan", 30, 210, 232},
    {"Blue", 40, 110, 255},
    {"Purple", 140, 64, 230},
    {"Pink", 255, 105, 180},
    {"White", 245, 245, 245},
    {"Black", 52, 52, 58},
    {"Gold", 212, 172, 52},
    {"Rainbow", 0, 0, 0},
};
#define NUM_COLORS (int)(sizeof(colors) / sizeof(colors[0]))
#define RAINBOW (NUM_COLORS - 1)

static Color hsv(float h, float s, float v)
{
    h = fmodf(h, 1.0f) * 6.0f;
    int i = (int)h;
    float f = h - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    float r, g, b;
    switch (i)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return (Color){(uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255), 255};
}

static float wrap_deg(float a)
{
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

static void format_time(char *buf, size_t len, float seconds)
{
    int m = (int)(seconds / 60.0f);
    float s = seconds - m * 60;
    snprintf(buf, len, "%d:%04.1f", m, s);
}

int main(int argc, char **argv)
{
    Platform_Init(argc, argv);
    Gfx_Init();
    Input_Init();
    Sound_Init();

    SaveData save;
    bool have_sd = Save_Init(&save);
    if (save.color < 0 || save.color >= NUM_COLORS)
        save.color = 0;

    const float radius = SPINNER_SIZE * 0.5f;

    float angle = 0.0f;   // degrees, clockwise
    float omega = 0.0f;   // degrees per second
    float spin_time = 0.0f;
    bool spinning = false;
    bool grabbed = false;
    float grab_prev = 0.0f;
    bool stick_prev_valid = false;
    float stick_prev = 0.0f;
    float rainbow_hue = 0.0f;
    bool show_help = true;
    bool dirty = false;
    bool in_session = false;
    float session_best_rpm = 0.0f, session_longest = 0.0f;
    bool rpm_banner_shown = false, time_banner_shown = false;
    float record_banner = 0.0f;
    float color_save_timer = 0.0f;
    float reset_confirm = 0.0f;
    char toast[64] = "";
    float toast_timer = 0.0f;
#ifdef HAVE_UPDATER
    bool update_popup = false;
#endif

    if (!have_sd)
    {
        snprintf(toast, sizeof(toast), "No SD card - records won't be saved");
        toast_timer = 5.0f;
    }

    double last = Platform_Now();

    while (!Platform_ExitRequested())
    {
        double now = Platform_Now();
        float dt = (float)(now - last);
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;

        InputState in;
        Input_Scan(&in);
        bool pushed = false;

        // The update popup takes every button but HOME until it is closed:
        // A installs the update (or just closes it when there is nothing to
        // install), B closes it or cancels the download.
#ifdef HAVE_UPDATER
        if (WIIUPDATE_TakeNotice())
            update_popup = true;
        if (update_popup)
        {
            switch (WIIUPDATE_InstallState())
            {
            case WIIUPDATE_INSTALL_IDLE:
                if ((in.down & BTN_A) && WIIUPDATE_CanInstall())
                    WIIUPDATE_Install();
                else if (in.down & (BTN_A | BTN_B))
                    update_popup = false;
                break;
            case WIIUPDATE_INSTALL_DOWNLOADING:
            case WIIUPDATE_INSTALL_EXTRACTING:
                if (in.down & BTN_B)
                    WIIUPDATE_Cancel();
                break;
            case WIIUPDATE_INSTALL_DONE:
                if (in.down & BTN_A)
                    Platform_RequestExit(1);
                break;
            case WIIUPDATE_INSTALL_FAILED:
                if (in.down & (BTN_A | BTN_B))
                    update_popup = false;
                break;
            }
            in.down &= BTN_HOME;
            in.held = 0;
            in.gforce = 0.0f;
            in.stick_valid = false;
        }
#endif

        if (in.down & BTN_HOME)
            break;

        // --- colour selection ---
        if (in.down & (BTN_RIGHT | BTN_PLUS))
        {
            save.color = (save.color + 1) % NUM_COLORS;
            color_save_timer = 2.0f;
        }
        if (in.down & (BTN_LEFT | BTN_MINUS))
        {
            save.color = (save.color + NUM_COLORS - 1) % NUM_COLORS;
            color_save_timer = 2.0f;
        }

        if (in.down & BTN_UP)
        {
            save.sound = !save.sound;
            color_save_timer = 2.0f;
            snprintf(toast, sizeof(toast), "Sound %s", save.sound ? "on" : "off");
            toast_timer = 2.0f;
        }
        if (in.down & BTN_DOWN)
            show_help = !show_help;

        if (in.down & BTN_2)
        {
            if (reset_confirm > 0.0f)
            {
                save.best_rpm = 0;
                save.longest_spin = 0;
                save.total_spins = 0;
                dirty = true;
                reset_confirm = 0.0f;
                snprintf(toast, sizeof(toast), "Records reset");
                toast_timer = 2.5f;
            }
            else
            {
                reset_confirm = 3.0f;
                snprintf(toast, sizeof(toast), RESET_PROMPT);
                toast_timer = 3.0f;
            }
        }

        // --- spinning ---
        float dir = omega < 0.0f ? -1.0f : 1.0f;

        if (in.down & BTN_A)
        {
            omega += FLICK_RPM * 6.0f;
            pushed = true;
        }
        if (in.down & BTN_1)
        {
            omega -= FLICK_RPM * 6.0f;
            pushed = true;
        }

        // Shake the Wii Remote to spin it faster
        if (in.gforce > 1.7f && !grabbed)
        {
            omega += dir * (in.gforce - 1.4f) * 1400.0f * dt;
            pushed = true;
        }

        // Rotate an analog stick to spin it
        if (in.stick_valid && stick_prev_valid && !grabbed)
        {
            float delta = wrap_deg(in.stick_ang - stick_prev);
            if (fabsf(delta) > 1.5f)
            {
                float target = delta / dt * 2.5f;
                if (target * omega >= 0.0f && fabsf(target) > fabsf(omega))
                {
                    float k = 6.0f * dt;
                    if (k > 1.0f) k = 1.0f;
                    omega += (target - omega) * k;
                    pushed = true;
                }
            }
        }
        stick_prev_valid = in.stick_valid;
        stick_prev = in.stick_ang;

        // Point at the spinner and hold B to grab it and spin it by hand,
        // or hold B anywhere else to brake.
        float pdx = 0, pdy = 0, pdist = 1e9f;
        if (in.ir_valid)
        {
            pdx = (in.ir_x - SPINNER_X) / Gfx_PointerAspectX();
            pdy = in.ir_y - SPINNER_Y;
            pdist = sqrtf(pdx * pdx + pdy * pdy);
        }

        if ((in.down & BTN_B) && pdist < radius * 0.95f && pdist > 8.0f)
        {
            grabbed = true;
            grab_prev = atan2f(pdy, pdx) * (180.0f / M_PI);
            omega = 0.0f;
        }

        if (grabbed)
        {
            if (!(in.held & BTN_B) || !in.ir_valid)
            {
                grabbed = false;
                pushed = true;
            }
            else if (pdist > 8.0f)
            {
                float a = atan2f(pdy, pdx) * (180.0f / M_PI);
                float delta = wrap_deg(a - grab_prev);
                grab_prev = a;
                angle += delta;
                omega += (delta / dt - omega) * 0.5f;
            }
        }
        else if (in.held & BTN_B)
        {
            omega *= expf(-2.5f * dt);
        }

        if (omega > MAX_RPM * 6.0f) omega = MAX_RPM * 6.0f;
        if (omega < -MAX_RPM * 6.0f) omega = -MAX_RPM * 6.0f;

        if (!grabbed)
        {
            // Bearing friction: proportional drag plus a little constant drag
            float decel = (0.045f * fabsf(omega) + 7.0f) * dt;
            if (fabsf(omega) <= decel || fabsf(omega) < STOP_DEG)
                omega = 0.0f;
            else
                omega -= (omega > 0 ? 1.0f : -1.0f) * decel;
            angle += omega * dt;
        }
        angle = fmodf(angle, 360.0f);

        save.total_spins += fabsf(omega) * dt / 360.0f;

        // --- records ---
        float rpm = fabsf(omega) / 6.0f;

        // A session lasts from the first push until the spinner stops. The
        // spin timer measures how long it has coasted since the last push.
        bool moving = omega != 0.0f || grabbed;
        if (moving && !in_session)
        {
            in_session = true;
            session_best_rpm = save.best_rpm;
            session_longest = save.longest_spin;
            rpm_banner_shown = time_banner_shown = false;
        }

        if (pushed || grabbed)
        {
            spin_time = 0.0f;
            spinning = false;
        }
        else if (omega != 0.0f)
        {
            spinning = true;
            spin_time += dt;
        }
        else
            spinning = false;

        if (!grabbed && rpm > save.best_rpm + 0.05f)
        {
            save.best_rpm = rpm;
            dirty = true;
            if (session_best_rpm > 0.0f && rpm > session_best_rpm && !rpm_banner_shown)
            {
                record_banner = 3.0f;
                rpm_banner_shown = true;
            }
        }
        if (spinning && spin_time > save.longest_spin)
        {
            save.longest_spin = spin_time;
            dirty = true;
            if (session_longest > 0.0f && !time_banner_shown)
            {
                record_banner = 3.0f;
                time_banner_shown = true;
            }
        }

        if (!moving && in_session)
        {
            in_session = false;
            if (dirty)
            {
                Save_Write(&save);
                dirty = false;
            }
        }

        if (color_save_timer > 0.0f)
        {
            color_save_timer -= dt;
            if (color_save_timer <= 0.0f && !in_session)
            {
                Save_Write(&save);
                dirty = false;
            }
            else if (color_save_timer <= 0.0f)
                dirty = true;
        }

        if (reset_confirm > 0.0f) reset_confirm -= dt;
        if (toast_timer > 0.0f) toast_timer -= dt;
        if (record_banner > 0.0f) record_banner -= dt;

        Sound_Update(grabbed ? 0.0f : rpm, save.sound);

        // --- colour ---
        Color tint;
        if (save.color == RAINBOW)
        {
            rainbow_hue += dt * (0.12f + rpm / 1500.0f);
            tint = hsv(rainbow_hue, 0.85f, 1.0f);
        }
        else
            tint = (Color){colors[save.color].r, colors[save.color].g, colors[save.color].b, 255};

        // --- draw ---
        Gfx_BeginFrame();
        for (int target = 0; target < Gfx_NumTargets(); target++)
        {
        Gfx_SetTarget(target);
        const float ax = Gfx_AspectX();

        Gfx_Rect(0, 0, SCREEN_W, SCREEN_H, (Color){22, 26, 44, 255}, (Color){8, 8, 16, 255});

        float glow = 0.25f + fminf(rpm / 600.0f, 1.0f) * 0.45f;
        Gfx_Disc(SPINNER_X, SPINNER_Y, radius * 1.15f,
                 (Color){tint.r, tint.g, tint.b, (uint8_t)(glow * 110)}, (Color){tint.r, tint.g, tint.b, 0});

        // Motion blur: draw the newest copy solid, then older copies averaged in
        float per_frame = fabsf(omega) / 60.0f;
        float span = per_frame > 120.0f ? 120.0f : per_frame;
        int copies = (int)(span / 2.0f) + 1;
        if (copies > 32) copies = 32;
        float back = omega < 0 ? -1.0f : 1.0f;

        Gfx_RotatedTexture(TEX_BODY, SPINNER_X + 8, SPINNER_Y + 12, SPINNER_SIZE,
                           angle * (M_PI / 180.0f), (Color){0, 0, 0, 90});

        for (int k = 0; k < copies; k++)
        {
            float a = angle - back * (copies > 1 ? span * k / (copies - 1) : 0.0f);
            float rad = a * (M_PI / 180.0f);
            uint8_t alpha = (uint8_t)(255 / (k + 1));
            Color body = tint;
            body.a = alpha;
            Gfx_RotatedTexture(TEX_BODY, SPINNER_X, SPINNER_Y, SPINNER_SIZE, rad, body);
            Gfx_RotatedTexture(TEX_DETAIL, SPINNER_X, SPINNER_Y, SPINNER_SIZE, rad, (Color){255, 255, 255, alpha});
        }

        // The HUD lives on one target only (the bottom screen of a handheld
        // is just the spinner and a touch surface).
        if (Gfx_TargetHasHud(target))
        {
        // HUD
        const Color white = {255, 255, 255, 255};
        const Color accent = {255, 200, 0, 255};
        const Color cyan = {0, 220, 255, 255};
        char buf[64];

        Gfx_TextShadow(36, 26, 2.0f, white, "FidgetWii");

        // Current spin time (keeps showing the last spin after it stops)
        format_time(buf, sizeof(buf), spin_time);
        float tw = Gfx_TextWidth(buf, 2.0f);
        float lw = Gfx_TextWidth("SPIN TIME", 1.0f);
        // Size the box for "00:00.0" so it doesn't jitter as the digits change
        float bw = Gfx_TextWidth("00:00.0", 2.0f);
        if (tw > bw) bw = tw;
        if (lw > bw) bw = lw;
        bw += 20 * ax;
        Gfx_Rect(608 - bw, 22, bw, 70, (Color){0, 0, 0, 120}, (Color){0, 0, 0, 150});
        Gfx_Text(598 - lw, 28, 1.0f, accent, "SPIN TIME");
        Gfx_Text(598 - tw, 50, 2.0f, spinning ? white : cyan, buf);


        // Records panel
        float px = 32, py = 340;
        Gfx_Rect(px, py + 20, 236 * ax, 78, (Color){0, 0, 0, 120}, (Color){0, 0, 0, 150});
        Gfx_Text(px + 10, py + 28, 1.0f, accent, "RECORDS");
        float vx = px + 226 * ax;
        snprintf(buf, sizeof(buf), "%d RPM", (int)(save.best_rpm + 0.5f));
        Gfx_Text(px + 10, py + 52, 1.0f, white, "Top speed");
        Gfx_Text(vx - Gfx_TextWidth(buf, 1.0f), py + 52, 1.0f, cyan, buf);
        format_time(buf, sizeof(buf), save.longest_spin);
        Gfx_Text(px + 10, py + 72, 1.0f, white, "Longest spin");
        Gfx_Text(vx - Gfx_TextWidth(buf, 1.0f), py + 72, 1.0f, cyan, buf);

        // Colour picker
        snprintf(buf, sizeof(buf), "< %s >", colors[save.color].name);
        float cw = Gfx_TextWidth(buf, 1.0f);
        float cx = 608 - cw;
        Gfx_Rect(cx - 44 * ax, py + 52, cw + 52 * ax, 40, (Color){0, 0, 0, 120}, (Color){0, 0, 0, 150});
        Gfx_Disc(cx - 22 * ax, py + 72, 11, (Color){tint.r, tint.g, tint.b, 255}, (Color){tint.r, tint.g, tint.b, 255});
        Gfx_Text(cx, py + 62, 1.0f, white, buf);
        Gfx_Text(608 - Gfx_TextWidth("COLOR", 1.0f), py + 28, 1.0f, accent, "COLOR");

        if (show_help && toast_timer <= 0.0f)
        {
            const char *help = HELP_TEXT;
            Gfx_TextShadow((SCREEN_W - Gfx_TextWidth(help, 1.0f)) / 2, 448, 1.0f, white, help);
        }

        if (record_banner > 0.0f && fmodf(record_banner, 0.5f) > 0.15f)
        {
            const char *msg = "NEW RECORD!";
            Gfx_TextShadow((SCREEN_W - Gfx_TextWidth(msg, 2.0f)) / 2, 96, 2.0f, accent, msg);
        }

        if (toast_timer > 0.0f)
        {
            uint8_t a = toast_timer > 0.5f ? 255 : (uint8_t)(toast_timer * 2 * 255);
            float tw = Gfx_TextWidth(toast, 1.0f);
            Gfx_Rect((SCREEN_W - tw) / 2 - 12, 443, tw + 24, 28, (Color){0, 0, 0, (uint8_t)(a * 0.6f)}, (Color){0, 0, 0, (uint8_t)(a * 0.6f)});
            Gfx_Text((SCREEN_W - tw) / 2, 448, 1.0f, (Color){255, 255, 255, a}, toast);
        }

#ifdef HAVE_UPDATER
        if (update_popup)
        {
            wiiupdate_install_state ist = WIIUPDATE_InstallState();
            bool busy = ist == WIIUPDATE_INSTALL_DOWNLOADING || ist == WIIUPDATE_INSTALL_EXTRACTING;
            char l2[160];
            const char *l1 = "Update available", *l3 = WIIUPDATE_Url(), *l4 = "Press A to continue";
            snprintf(l2, sizeof(l2), "%s", WIIUPDATE_Message());
            if (ist == WIIUPDATE_INSTALL_IDLE && WIIUPDATE_CanInstall())
            {
                l3 = "";
                l4 = "A: Update now    B: Later";
            }
            else if (busy)
            {
                l1 = "Updating...";
                l4 = "B: Cancel";
                snprintf(l2, sizeof(l2), "%s %s", ist == WIIUPDATE_INSTALL_DOWNLOADING ? "Downloading" : "Installing",
                         WIIUPDATE_Latest());
            }
            else if (ist == WIIUPDATE_INSTALL_DONE)
            {
                l1 = "Updated";
                snprintf(l2, sizeof(l2), "FidgetWii %s is installed.", WIIUPDATE_Latest());
                l3 = "";
                l4 = "Press A to return to the loader";
            }
            else if (ist == WIIUPDATE_INSTALL_FAILED)
            {
                l1 = "Update failed";
                snprintf(l2, sizeof(l2), "%s", WIIUPDATE_InstallError());
            }
            float w = Gfx_TextWidth(l2, 1.0f);
            if (Gfx_TextWidth(l3, 1.0f) > w) w = Gfx_TextWidth(l3, 1.0f);
            if (Gfx_TextWidth(l4, 1.0f) > w) w = Gfx_TextWidth(l4, 1.0f);
            if (w < 300) w = 300;
            w += 48;
            float x = (SCREEN_W - w) / 2, y = 160;
            Gfx_Rect(0, 0, SCREEN_W, SCREEN_H, (Color){0, 0, 0, 120}, (Color){0, 0, 0, 120});
            Gfx_Rect(x, y, w, 150, (Color){34, 40, 66, 245}, (Color){16, 18, 32, 245});
            Gfx_TextShadow((SCREEN_W - Gfx_TextWidth(l1, 2.0f)) / 2, y + 16, 2.0f, accent, l1);
            Gfx_Text((SCREEN_W - Gfx_TextWidth(l2, 1.0f)) / 2, y + 62, 1.0f, white, l2);
            if (busy)
            {
                float bw = w - 48;
                Gfx_Rect(x + 24, y + 86, bw, 14, (Color){0, 0, 0, 160}, (Color){0, 0, 0, 160});
                Gfx_Rect(x + 24, y + 86, bw * WIIUPDATE_Progress() / 100.0f, 14, cyan, cyan);
            }
            else
                Gfx_Text((SCREEN_W - Gfx_TextWidth(l3, 1.0f)) / 2, y + 84, 1.0f, cyan, l3);
            Gfx_Text((SCREEN_W - Gfx_TextWidth(l4, 1.0f)) / 2, y + 120, 1.0f, white, l4);
        }

#endif
        }

        if (in.ir_valid)
            Gfx_Pointer(in.ir_x, in.ir_y);
        }

#ifdef PLATFORM_NDS
        {
            HudInfo hud = {spin_time, save.best_rpm, save.longest_spin, spinning,
                           record_banner > 0.0f && fmodf(record_banner, 0.5f) > 0.15f,
                           show_help, colors[save.color].name, toast_timer > 0.0f ? toast : ""};
            Hud_Native(&hud);
        }
#endif

        Gfx_EndFrame();
    }

    Sound_Update(0, false);
    Save_Write(&save);
    Save_Shutdown();
    Sound_Shutdown();
    Input_Shutdown();

    Gfx_Shutdown();
    Platform_Shutdown();
    return 0;
}
