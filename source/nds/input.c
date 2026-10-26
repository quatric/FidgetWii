// Nintendo DS input: buttons plus the touch screen as the pointer.

#include <nds.h>
#include "gfx.h"
#include "input.h"

void Input_Init(void)
{
}

void Input_Shutdown(void)
{
}

static uint32_t map_keys(u32 keys)
{
    static const struct { u32 from; uint32_t to; } map[] = {
        {KEY_A, BTN_A}, {KEY_B, BTN_B}, {KEY_Y, BTN_1}, {KEY_X, BTN_2},
        {KEY_UP, BTN_UP}, {KEY_DOWN, BTN_DOWN}, {KEY_LEFT, BTN_LEFT}, {KEY_RIGHT, BTN_RIGHT},
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
    scanKeys();
    u32 down = keysDown(), held = keysHeld();

    state->down = map_keys(down);
    state->held = map_keys(held);

    // The touch screen is the pointer; touching is holding B
    state->ir_valid = false;
    if (held & KEY_TOUCH)
    {
        touchPosition touch;
        touchRead(&touch);
        state->ir_valid = true;
        state->ir_x = touch.px * (SCREEN_W / 256.0f);
        state->ir_y = touch.py * (SCREEN_H / 192.0f);
        state->held |= BTN_B;
        if (down & KEY_TOUCH)
            state->down |= BTN_B;
    }

    state->gforce = 0.0f;
    state->stick_valid = false;
    state->stick_ang = 0.0f;
}
