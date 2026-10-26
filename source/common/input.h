#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef PLATFORM_WII
#include <wiiuse/wpad.h>
#define BTN_A WPAD_BUTTON_A
#define BTN_B WPAD_BUTTON_B
#define BTN_1 WPAD_BUTTON_1
#define BTN_2 WPAD_BUTTON_2
#define BTN_UP WPAD_BUTTON_UP
#define BTN_DOWN WPAD_BUTTON_DOWN
#define BTN_LEFT WPAD_BUTTON_LEFT
#define BTN_RIGHT WPAD_BUTTON_RIGHT
#define BTN_PLUS WPAD_BUTTON_PLUS
#define BTN_MINUS WPAD_BUTTON_MINUS
#define BTN_HOME WPAD_BUTTON_HOME
#else
// Buttons are named after the Wii Remote ones the game was written for.
#define BTN_A (1u << 0)     // flick clockwise
#define BTN_B (1u << 1)     // grab / brake
#define BTN_1 (1u << 2)     // flick counterclockwise
#define BTN_2 (1u << 3)     // reset records
#define BTN_UP (1u << 4)    // sound on/off
#define BTN_DOWN (1u << 5)  // help on/off
#define BTN_LEFT (1u << 6)
#define BTN_RIGHT (1u << 7)
#define BTN_PLUS (1u << 8)  // next colour
#define BTN_MINUS (1u << 9) // previous colour
#define BTN_HOME (1u << 10) // exit
#endif

typedef struct
{
    uint32_t down;   // buttons newly pressed this frame (BTN_* bits)
    uint32_t held;   // buttons held this frame (BTN_* bits)

    bool ir_valid;   // a pointer (Wii Remote, touch screen) is on the screen
    float ir_x;      // pointer position in 640x480 screen space
    float ir_y;

    float gforce;    // strongest acceleration magnitude (g), for shaking

    bool stick_valid; // an analog stick is tilted far enough to read an angle
    float stick_ang;  // stick angle in degrees, clockwise from up
} InputState;

void Input_Init(void);
void Input_Scan(InputState *state);
void Input_Shutdown(void);

#endif
