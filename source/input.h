#ifndef INPUT_H
#define INPUT_H

#include <gccore.h>
#include <wiiuse/wpad.h>

typedef struct
{
    u32 down;        // buttons newly pressed this frame (WPAD_BUTTON_* bits)
    u32 held;        // buttons held this frame (WPAD_BUTTON_* bits)

    bool ir_valid;   // a Wii Remote is pointing at the screen
    float ir_x;      // pointer position in 640x480 screen space
    float ir_y;

    float gforce;    // strongest Wii Remote acceleration magnitude (g)

    bool stick_valid; // an analog stick is tilted far enough to read an angle
    float stick_ang;  // stick angle in degrees, clockwise from up
} InputState;

void Input_Init(void);
void Input_Scan(InputState *state);
void Input_Shutdown(void);

#endif
