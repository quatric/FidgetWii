#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <wiiupdate.h>
#include "platform.h"
#include "version.h"

static volatile int exit_request; // 1 = return to loader, 2 = power off

static void reset_cb(u32 irq, void *ctx) { exit_request = 1; }
static void power_cb(void) { exit_request = 2; }
static void wpad_power_cb(s32 chan) { exit_request = 2; }

void Platform_Init(int argc, char **argv)
{
    WIIUPDATE_SetAppPath(argc > 0 ? argv[0] : NULL);
    WIIUPDATE_Start("quatric", "FidgetWii", FIDGET_VERSION);

    SYS_SetResetCallback(reset_cb);
    SYS_SetPowerCallback(power_cb);
    WPAD_SetPowerButtonCallback(wpad_power_cb);
}

double Platform_Now(void)
{
    return ticks_to_microsecs(gettime()) / 1000000.0;
}

int Platform_ExitRequested(void)
{
    return exit_request;
}

void Platform_RequestExit(int request)
{
    exit_request = request;
}

void Platform_Shutdown(void)
{
    if (exit_request == 2)
        SYS_ResetSystem(SYS_POWEROFF, 0, 0);
}
