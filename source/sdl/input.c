// Controller and touch input through SDL2: GamePad, Pro Controller and
// Joy-Cons as game controllers, and the touch screen as a pointer.

#include <math.h>
#include <SDL.h>
#include "gfx.h"
#include "input.h"
#include "platform.h"

#define MAX_PADS 4

static SDL_GameController *pads[MAX_PADS];
static uint32_t prev_held;
static bool touching;
static float touch_x, touch_y;

static void open_pad(int index)
{
    if (!SDL_IsGameController(index))
        return;
    for (int i = 0; i < MAX_PADS; i++)
    {
        if (!pads[i])
        {
            pads[i] = SDL_GameControllerOpen(index);
            return;
        }
    }
}

static void close_pad(SDL_JoystickID id)
{
    for (int i = 0; i < MAX_PADS; i++)
    {
        if (pads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == id)
        {
            SDL_GameControllerClose(pads[i]);
            pads[i] = NULL;
        }
    }
}

void Input_Init(void)
{
    SDL_JoystickEventState(SDL_ENABLE);
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        open_pad(i);
}

void Input_Shutdown(void)
{
    for (int i = 0; i < MAX_PADS; i++)
    {
        if (pads[i])
            SDL_GameControllerClose(pads[i]);
        pads[i] = NULL;
    }
}

static uint32_t read_pad(SDL_GameController *pad, float *sx, float *sy)
{
    static const struct { SDL_GameControllerButton from; uint32_t to; } map[] = {
        {SDL_CONTROLLER_BUTTON_A, BTN_A},
        {SDL_CONTROLLER_BUTTON_B, BTN_B},
        {SDL_CONTROLLER_BUTTON_Y, BTN_1},
        {SDL_CONTROLLER_BUTTON_X, BTN_2},
        {SDL_CONTROLLER_BUTTON_DPAD_UP, BTN_UP},
        {SDL_CONTROLLER_BUTTON_DPAD_DOWN, BTN_DOWN},
        {SDL_CONTROLLER_BUTTON_DPAD_LEFT, BTN_LEFT},
        {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, BTN_RIGHT},
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, BTN_MINUS},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, BTN_PLUS},
        {SDL_CONTROLLER_BUTTON_BACK, BTN_MINUS},
        {SDL_CONTROLLER_BUTTON_START, BTN_HOME},
    };
    uint32_t held = 0;
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (SDL_GameControllerGetButton(pad, map[i].from))
            held |= map[i].to;

    float x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
    float y = -SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
    if (hypotf(x, y) > hypotf(*sx, *sy))
    {
        *sx = x;
        *sy = y;
    }
    return held;
}

void Input_Scan(InputState *state)
{
    SDL_Event e;
    bool touch_began = false;

    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_QUIT:
            Platform_RequestExit(1);
            break;
        case SDL_CONTROLLERDEVICEADDED:
            open_pad(e.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            close_pad(e.cdevice.which);
            break;
        case SDL_FINGERDOWN:
            touching = true;
            touch_began = true;
            touch_x = e.tfinger.x * SCREEN_W;
            touch_y = e.tfinger.y * SCREEN_H;
            break;
        case SDL_FINGERMOTION:
            touch_x = e.tfinger.x * SCREEN_W;
            touch_y = e.tfinger.y * SCREEN_H;
            break;
        case SDL_FINGERUP:
            touching = false;
            break;
        }
    }

    float sx = 0, sy = 0;
    uint32_t held = 0;
    for (int i = 0; i < MAX_PADS; i++)
        if (pads[i])
            held |= read_pad(pads[i], &sx, &sy);

    // The touch screen acts as the pointer: touching is holding B.
    state->ir_valid = touching;
    state->ir_x = touch_x;
    state->ir_y = touch_y;
    if (touching)
        held |= BTN_B;

    state->held = held;
    state->down = (held & ~prev_held) | (touch_began ? BTN_B : 0);
    prev_held = held;

    state->gforce = 0.0f;
    state->stick_valid = hypotf(sx, sy) >= 0.6f;
    state->stick_ang = state->stick_valid ? atan2f(sx, sy) * (180.0f / M_PI) : 0.0f;
}
