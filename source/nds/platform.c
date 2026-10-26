#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include "platform.h"

static u32 last_tick;
static double seconds;

void Platform_Init(int argc, char **argv)
{
    cpuStartTiming(0);
    last_tick = cpuGetTiming();
}

double Platform_Now(void)
{
    u32 tick = cpuGetTiming();
    seconds += (u32)(tick - last_tick) / (double)BUS_CLOCK;
    last_tick = tick;
    return seconds;
}

int Platform_ExitRequested(void)
{
    return pmMainLoop() ? 0 : 1;
}

void Platform_RequestExit(int request)
{
}

void Platform_Shutdown(void)
{
}
