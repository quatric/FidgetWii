#include <3ds.h>
#include "platform.h"

void Platform_Init(int argc, char **argv)
{
    gfxInitDefault();
}

double Platform_Now(void)
{
    return svcGetSystemTick() / (double)SYSCLOCK_ARM11;
}

int Platform_ExitRequested(void)
{
    return aptMainLoop() ? 0 : 1;
}

void Platform_RequestExit(int request)
{
}

void Platform_Shutdown(void)
{
    gfxExit();
}
