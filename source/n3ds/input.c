// Nintendo 3DS input: buttons, circle pad, touch screen pointer and the
// accelerometer for shaking.

#include <math.h>
#include <3ds.h>
#include "gfx.h"
#include "input.h"

void Input_Init(void)
{
    HIDUSER_EnableAccelerometer();
}

void Input_Shutdown(void)
{
    HIDUSER_DisableAccelerometer();
}

static uint32_t map_keys(u32 keys)
{
    static const struct { u32 from; uint32_t to; } map[] = {
        {KEY_A, BTN_A}, {KEY_B, BTN_B}, {KEY_Y, BTN_1}, {KEY_X, BTN_2},
        {KEY_DUP, BTN_UP}, {KEY_DDOWN, BTN_DOWN}, {KEY_DLEFT, BTN_LEFT}, {KEY_DRIGHT, BTN_RIGHT},
        {KEY_L, BTN_MINUS}, {KEY_R, BTN_PLUS}, {KEY_START, BTN_HOME},
    };
    uint32_t out = 0;
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (keys & map[i].from)
            out |= map[i].to;
    return out;
}

void Input_Scan(InputState *state)
{
    hidScanInput();
    u32 down = hidKeysDown(), held = hidKeysHeld();

    state->down = map_keys(down);
    state->held = map_keys(held);

    // The touch screen is the pointer; touching is holding B
    state->ir_valid = false;
    if (held & KEY_TOUCH)
    {
        touchPosition touch;
        hidTouchRead(&touch);
        state->ir_valid = true;
        state->ir_x = touch.px * (SCREEN_W / 320.0f);
        state->ir_y = touch.py * (SCREEN_H / 240.0f);
        state->held |= BTN_B;
        if (down & KEY_TOUCH)
            state->down |= BTN_B;
    }

    accelVector accel;
    hidAccelRead(&accel);
    state->gforce = sqrtf((float)(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z)) / 512.0f;

    circlePosition pad;
    hidCircleRead(&pad);
    float x = pad.dx / 156.0f, y = pad.dy / 156.0f;
    state->stick_valid = hypotf(x, y) >= 0.6f;
    state->stick_ang = state->stick_valid ? atan2f(x, y) * (180.0f / M_PI) : 0.0f;
}
