// SDL2 platform glue for Wii U and Switch.

#include <SDL.h>
#ifdef PLATFORM_SWITCH
#include <switch.h>
#endif
#include "platform.h"

static volatile int exit_request;

void Platform_Init(int argc, char **argv)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
}

double Platform_Now(void)
{
    return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
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
    SDL_Quit();
}
