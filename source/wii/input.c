// Controller input via libwiiinput.
// Supports Wii Remote (+ Nunchuk / Classic Controller), GameCube controller
// and USB keyboard, plus Wii U GamePad, Wii U Pro Controller and DualShock 3
// when built with the optional controller libraries.

#include <math.h>
#include <string.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <input_wii.h>
#include "input.h"

void Input_Init(void)
{
    // No mouse: the spinner is driven by the Wii Remote pointer.
    INPUT_InitEx(INPUT_INIT_NO_MOUSE);
    INPUT_SetScreenSize(640, 480);

    INPUT_KeyboardMapSet(0x2C, WPAD_BUTTON_A);     // Space
    INPUT_KeyboardMapSet(0x2D, WPAD_BUTTON_MINUS); // -
    INPUT_KeyboardMapSet(0x2E, WPAD_BUTTON_PLUS);  // =/+
}

void Input_Shutdown(void)
{
    INPUT_Shutdown();
}

void Input_Scan(InputState *state)
{
    float x, y;
    const char *src;

    INPUT_Update();

    state->down = INPUT_ButtonsDown();
    state->held = INPUT_ButtonsHeld();

    state->ir_valid = INPUT_Pointer(&x, &y);
    state->ir_x = x;
    state->ir_y = y;

    state->gforce = INPUT_Gforce();

    // The library scales the GameCube stick by 70; this game read it by 80.
    float mag = INPUT_Stick(INPUT_STICK_LEFT, &x, &y, &src);
    if (strcmp(src, "GameCube") == 0)
        mag *= 70.0f / 80.0f;
    state->stick_valid = mag >= 0.6f;
    state->stick_ang = state->stick_valid ? atan2f(x, y) * (180.0f / M_PI) : 0.0f;
}
