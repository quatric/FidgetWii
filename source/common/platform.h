#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

#if defined(PLATFORM_WII)
#define APP_NAME "FidgetWii"
#define HELP_TEXT "A Flick  B Grab/Brake  Shake Spin  +/- Color  HOME Quit"
#define RESET_PROMPT "Press 2 again to reset records"
#elif defined(PLATFORM_WIIU)
#define APP_NAME "FidgetWiiU"
#define HELP_TEXT "A Flick  B Brake  Touch Grab  L/R Color  + Quit"
#define RESET_PROMPT "Press X again to reset records"
#elif defined(PLATFORM_SWITCH)
#define APP_NAME "FidgetSwitch"
#define HELP_TEXT "A Flick  B Brake  Touch Grab  L/R Color  + Quit"
#define RESET_PROMPT "Press X again to reset records"
#elif defined(PLATFORM_3DS)
#define APP_NAME "Fidget3DS"
#define HELP_TEXT "A Flick  B Brake  Touch Grab  L/R Color  START Quit"
#define RESET_PROMPT "Press X again to reset records"
#else
#define APP_NAME "FidgetDS"
#define HELP_TEXT "A Flick  B Brake  Touch Grab  L/R Color  + Quit"
#define RESET_PROMPT "Press X again to reset records"
#endif

// Called first thing in main(); sets up exit/power handling.
void Platform_Init(int argc, char **argv);
// Seconds from an arbitrary monotonic origin.
double Platform_Now(void);
// 0 = keep running, 1 = return to the loader, 2 = power off.
int Platform_ExitRequested(void);
void Platform_RequestExit(int request);
void Platform_Shutdown(void);

// Platforms whose HUD is not drawn with the Gfx_* text API (the DS prints it
// on the top screen) implement this instead.
typedef struct
{
    float spin_time, best_rpm, longest_spin;
    bool spinning, banner, help;
    const char *color;
    const char *toast;
} HudInfo;
void Hud_Native(const HudInfo *hud);

#endif
