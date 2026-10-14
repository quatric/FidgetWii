#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>

#include "gfx.h"
#include "input.h"
#include "save.h"
#include "sound.h"

#include "body_bin.h"
#include "detail_bin.h"

#define VERSION "1.0"

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
    u8 r, g, b;
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

static volatile int exit_request; // 1 = return to loader, 2 = power off

static void reset_cb(u32 irq, void *ctx) { exit_request = 1; }
static void power_cb(void) { exit_request = 2; }
static void wpad_power_cb(s32 chan) { exit_request = 2; }

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
    return (Color){(u8)(r * 255), (u8)(g * 255), (u8)(b * 255), 255};
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

static void format_count(char *buf, size_t len, double n)
{
    char raw[32];
    snprintf(raw, sizeof(raw), "%.0f", n);
    int digits = strlen(raw), out = 0;
    for (int i = 0; i < digits && out < (int)len - 1; i++)
    {
        if (i && (digits - i) % 3 == 0 && out < (int)len - 2)
            buf[out++] = ',';
        buf[out++] = raw[i];
    }
    buf[out] = 0;
}

int main(int argc, char **argv)
{
    Gfx_Init();
    Input_Init();
    Sound_Init();

    SYS_SetResetCallback(reset_cb);
    SYS_SetPowerCallback(power_cb);
    WPAD_SetPowerButtonCallback(wpad_power_cb);

    SaveData save;
    bool have_sd = Save_Init(&save);
    if (save.color < 0 || save.color >= NUM_COLORS)
        save.color = 0;

    GXTexObj body_tex, detail_tex;
    Gfx_LoadTexture(&body_tex, body_bin, body_bin_size, TEX_SIZE, TEX_SIZE, GX_TF_IA8, true);
    Gfx_LoadTexture(&detail_tex, detail_bin, detail_bin_size, TEX_SIZE, TEX_SIZE, GX_TF_RGBA8, true);

    const float ax = Gfx_AspectX();
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

    if (!have_sd)
    {
        snprintf(toast, sizeof(toast), "No SD card - records won't be saved");
        toast_timer = 5.0f;
    }

    u64 last = gettime();

    while (!exit_request)
    {
        u64 now = gettime();
        float dt = ticks_to_microsecs(diff_ticks(last, now)) / 1000000.0f;
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;

        InputState in;
        Input_Scan(&in);
        bool pushed = false;

        if (in.down & WPAD_BUTTON_HOME)
            break;

        // --- colour selection ---
        if (in.down & (WPAD_BUTTON_RIGHT | WPAD_BUTTON_PLUS))
        {
            save.color = (save.color + 1) % NUM_COLORS;
            color_save_timer = 2.0f;
        }
        if (in.down & (WPAD_BUTTON_LEFT | WPAD_BUTTON_MINUS))
        {
            save.color = (save.color + NUM_COLORS - 1) % NUM_COLORS;
            color_save_timer = 2.0f;
        }

        if (in.down & WPAD_BUTTON_UP)
        {
            save.sound = !save.sound;
            color_save_timer = 2.0f;
            snprintf(toast, sizeof(toast), "Sound %s", save.sound ? "on" : "off");
            toast_timer = 2.0f;
        }
        if (in.down & WPAD_BUTTON_DOWN)
            show_help = !show_help;

        if (in.down & WPAD_BUTTON_2)
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
                snprintf(toast, sizeof(toast), "Press 2 again to reset records");
                toast_timer = 3.0f;
            }
        }

        // --- spinning ---
        float dir = omega < 0.0f ? -1.0f : 1.0f;

        if (in.down & WPAD_BUTTON_A)
        {
            omega += FLICK_RPM * 6.0f;
            pushed = true;
        }
        if (in.down & WPAD_BUTTON_1)
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
            pdx = (in.ir_x - SPINNER_X) / ax;
            pdy = in.ir_y - SPINNER_Y;
            pdist = sqrtf(pdx * pdx + pdy * pdy);
        }

        if ((in.down & WPAD_BUTTON_B) && pdist < radius * 0.95f && pdist > 8.0f)
        {
            grabbed = true;
            grab_prev = atan2f(pdy, pdx) * (180.0f / M_PI);
            omega = 0.0f;
        }

        if (grabbed)
        {
            if (!(in.held & WPAD_BUTTON_B) || !in.ir_valid)
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
        else if (in.held & WPAD_BUTTON_B)
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

        Gfx_Rect(0, 0, SCREEN_W, SCREEN_H, (Color){22, 26, 44, 255}, (Color){8, 8, 16, 255});

        float glow = 0.25f + fminf(rpm / 600.0f, 1.0f) * 0.45f;
        Gfx_Disc(SPINNER_X, SPINNER_Y, radius * 1.15f,
                 (Color){tint.r, tint.g, tint.b, (u8)(glow * 110)}, (Color){tint.r, tint.g, tint.b, 0});

        // Motion blur: draw the newest copy solid, then older copies averaged in
        float per_frame = fabsf(omega) / 60.0f;
        float span = per_frame > 120.0f ? 120.0f : per_frame;
        int copies = (int)(span / 2.0f) + 1;
        if (copies > 32) copies = 32;
        float back = omega < 0 ? -1.0f : 1.0f;

        Gfx_RotatedTexture(&body_tex, SPINNER_X + 8, SPINNER_Y + 12, SPINNER_SIZE,
                           angle * (M_PI / 180.0f), (Color){0, 0, 0, 90});

        for (int k = 0; k < copies; k++)
        {
            float a = angle - back * (copies > 1 ? span * k / (copies - 1) : 0.0f);
            float rad = a * (M_PI / 180.0f);
            u8 alpha = (u8)(255 / (k + 1));
            Color body = tint;
            body.a = alpha;
            Gfx_RotatedTexture(&body_tex, SPINNER_X, SPINNER_Y, SPINNER_SIZE, rad, body);
            Gfx_RotatedTexture(&detail_tex, SPINNER_X, SPINNER_Y, SPINNER_SIZE, rad, (Color){255, 255, 255, alpha});
        }

        // HUD
        const Color white = {255, 255, 255, 255};
        const Color dim = {170, 178, 200, 255};
        const Color accent = {255, 210, 60, 255};
        char buf[64], buf2[32];

        Gfx_TextShadow(36, 26, 2.0f, white, "FidgetWii");
        Gfx_TextShadow(36, 60, 1.0f, dim, "v" VERSION);

        snprintf(buf, sizeof(buf), "%d", (int)(rpm + 0.5f));
        Gfx_TextShadow(604 - Gfx_TextWidth(buf, 3.0f), 20, 3.0f, white, buf);
        Gfx_TextShadow(604 - Gfx_TextWidth("RPM", 1.0f), 70, 1.0f, dim, "RPM");
        if (spinning)
        {
            format_time(buf2, sizeof(buf2), spin_time);
            snprintf(buf, sizeof(buf), "Spin %s", buf2);
            Gfx_TextShadow(604 - Gfx_TextWidth(buf, 1.0f), 90, 1.0f, dim, buf);
        }

        // Records panel
        float px = 32, py = 352;
        Gfx_Rect(px, py, 236 * ax, 84, (Color){0, 0, 0, 120}, (Color){0, 0, 0, 150});
        Gfx_Text(px + 10, py + 6, 1.0f, accent, "RECORDS");
        snprintf(buf, sizeof(buf), "Top speed  %d RPM", (int)(save.best_rpm + 0.5f));
        Gfx_Text(px + 10, py + 26, 1.0f, white, buf);
        format_time(buf2, sizeof(buf2), save.longest_spin);
        snprintf(buf, sizeof(buf), "Longest    %s", buf2);
        Gfx_Text(px + 10, py + 44, 1.0f, white, buf);
        format_count(buf2, sizeof(buf2), save.total_spins);
        snprintf(buf, sizeof(buf), "Total spins %s", buf2);
        Gfx_Text(px + 10, py + 62, 1.0f, white, buf);

        // Colour picker
        snprintf(buf, sizeof(buf), "< %s >", colors[save.color].name);
        float cw = Gfx_TextWidth(buf, 1.0f);
        float cx = 608 - cw;
        Gfx_Rect(cx - 44 * ax, py + 30, cw + 52 * ax, 36, (Color){0, 0, 0, 120}, (Color){0, 0, 0, 150});
        Gfx_Disc(cx - 22 * ax, py + 48, 11, (Color){tint.r, tint.g, tint.b, 255}, (Color){tint.r, tint.g, tint.b, 255});
        Gfx_Text(cx, py + 40, 1.0f, white, buf);
        Gfx_Text(608 - Gfx_TextWidth("COLOR", 1.0f), py + 8, 1.0f, accent, "COLOR");

        if (show_help)
        {
            const char *help = "A Flick  B Grab/Brake  Shake Spin  +/- Color  HOME Quit";
            Gfx_TextShadow((SCREEN_W - Gfx_TextWidth(help, 1.0f)) / 2, 448, 1.0f, dim, help);
        }

        if (record_banner > 0.0f && fmodf(record_banner, 0.5f) > 0.15f)
        {
            const char *msg = "NEW RECORD!";
            Gfx_TextShadow((SCREEN_W - Gfx_TextWidth(msg, 2.0f)) / 2, 96, 2.0f, accent, msg);
        }

        if (toast_timer > 0.0f)
        {
            u8 a = toast_timer > 0.5f ? 255 : (u8)(toast_timer * 2 * 255);
            float tw = Gfx_TextWidth(toast, 1.0f);
            Gfx_Rect((SCREEN_W - tw) / 2 - 12, 412, tw + 24, 26, (Color){0, 0, 0, (u8)(a * 0.6f)}, (Color){0, 0, 0, (u8)(a * 0.6f)});
            Gfx_Text((SCREEN_W - tw) / 2, 417, 1.0f, (Color){255, 255, 255, a}, toast);
        }

        if (in.ir_valid)
            Gfx_Pointer(in.ir_x, in.ir_y);

        Gfx_EndFrame();
    }

    Sound_Update(0, false);
    Save_Write(&save);
    Save_Shutdown();
    Sound_Shutdown();
    Input_Shutdown();

    if (exit_request == 2)
        SYS_ResetSystem(SYS_POWEROFF, 0, 0);
    return 0;
}
