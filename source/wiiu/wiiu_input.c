// Controller input via libwiiuinput.
// Wii U GamePad (with its touch screen), Wii Remotes (+ Nunchuk / Classic
// Controller), Wii U Pro Controllers, the GameCube controller adapter, USB
// game controllers and USB keyboards, through the same calls as the Wii build.

#include <math.h>
#include <string.h>
#include <SDL.h>
#include <coreinit/systeminfo.h>
#include <input_wiiu.h>
#include "input.h"
#include "platform.h"

void Input_Init(void)
{
    // HOME is a button here (it exits), not the system's HOME menu.
    OSEnableHomeButtonMenu(FALSE);

    // No mouse: the spinner is driven by the Wii Remote pointer or the touch screen.
    INPUT_InitEx(INPUT_INIT_NO_MOUSE);
    INPUT_SetScreenSize(640, 480);

    // Touching the GamePad is holding B (grab), as the pointer is on the Wii.
    INPUT_SetTouchButton(WPAD_BUTTON_B);

    INPUT_KeyboardMapSet(0x2C, WPAD_BUTTON_A);     // Space
    INPUT_KeyboardMapSet(0x2D, WPAD_BUTTON_MINUS); // -
    INPUT_KeyboardMapSet(0x2E, WPAD_BUTTON_PLUS);  // =/+
}

void Input_Shutdown(void)
{
    INPUT_Shutdown();
    OSEnableHomeButtonMenu(TRUE);
}

void Input_Scan(InputState *state)
{
    float x, y;
    int tx, ty;
    const char *src;

    SDL_Event e;
    while (SDL_PollEvent(&e))
        if (e.type == SDL_QUIT)
            Platform_RequestExit(1);

    INPUT_Update();

    state->down = INPUT_ButtonsDown();
    state->held = INPUT_ButtonsHeld();

    // The touch screen is the pointer while it is touched, else a Wii Remote's.
    if (INPUT_Touch(&tx, &ty))
    {
        state->ir_valid = true;
        state->ir_x = tx;
        state->ir_y = ty;
    }
    else
    {
        state->ir_valid = INPUT_Pointer(&x, &y);
        state->ir_x = x;
        state->ir_y = y;
    }

    state->gforce = INPUT_Gforce();

    // The library scales the GameCube stick by 70; this game reads it by 80.
    float mag = INPUT_Stick(INPUT_STICK_LEFT, &x, &y, &src);
    if (src && strcmp(src, "GameCube") == 0)
        mag *= 70.0f / 80.0f;
    state->stick_valid = mag >= 0.6f;
    state->stick_ang = state->stick_valid ? atan2f(x, y) * (180.0f / M_PI) : 0.0f;
}
