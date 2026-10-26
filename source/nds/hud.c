// The DS HUD: the top screen is a text console.

#include <stdio.h>
#include <string.h>
#include <nds.h>
#include "platform.h"

static bool console_ready;
static char shown[24][33];

static void format_time(char *buf, size_t len, float seconds)
{
    int m = (int)(seconds / 60.0f);
    float s = seconds - m * 60;
    snprintf(buf, len, "%d:%04.1f", m, s);
}

// Rewrites a row only when its text changed
static void row(int y, const char *text)
{
    char line[33];
    snprintf(line, sizeof(line), "%-32.32s", text);
    if (!strcmp(line, shown[y]))
        return;
    memcpy(shown[y], line, sizeof(line));
    iprintf("\x1b[%d;0H%s", y, line);
}

void Hud_Native(const HudInfo *hud)
{
    if (!console_ready)
    {
        videoSetModeSub(MODE_0_2D);
        vramSetBankC(VRAM_C_SUB_BG);
        consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
        console_ready = true;
    }

    char buf[48], t[16];

    row(1, "          " APP_NAME);

    format_time(t, sizeof(t), hud->spin_time);
    snprintf(buf, sizeof(buf), "  SPIN TIME   %s%s", t, hud->spinning ? "  >>" : "");
    row(4, buf);

    row(7, "  RECORDS");
    snprintf(buf, sizeof(buf), "  Top speed     %d RPM", (int)(hud->best_rpm + 0.5f));
    row(8, buf);
    format_time(t, sizeof(t), hud->longest_spin);
    snprintf(buf, sizeof(buf), "  Longest spin  %s", t);
    row(9, buf);

    snprintf(buf, sizeof(buf), "  COLOR   < %s >", hud->color);
    row(12, buf);

    row(15, hud->banner ? "        NEW RECORD!" : "");
    row(18, hud->toast);

    if (hud->help)
    {
        row(21, "A Flick  Y Flick back  B Brake");
        row(22, "Touch Grab  L/R Color  Up Sound");
        row(23, "Down Help  X Reset records");
    }
    else
    {
        row(21, "");
        row(22, "");
        row(23, "");
    }
}
