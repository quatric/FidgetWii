// Controller input, based on the input code from GlowWii.
// Supports Wii Remote (+ Nunchuk / Classic Controller), GameCube controller
// and USB keyboard, plus Wii U GamePad, Wii U Pro Controller and DualShock 3
// when built with the optional controller libraries.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <wiikeyboard/usbkeyboard.h>
#ifdef HAVE_WIIDRC
#include <wiidrc/wiidrc.h>
#endif
#ifdef HAVE_WUPC
#include <wupc/wupc.h>
#endif
#ifdef HAVE_SICKSAXIS
#include <sicksaxis.h>
#endif
#include "input.h"

#ifdef HAVE_SICKSAXIS
static struct ss_device ds3;
static int ds3_initialized;
static u32 ds3_previous;
#endif

static u32 map_classic(u32 buttons)
{
    u32 out = 0;
    if (buttons & WPAD_CLASSIC_BUTTON_ZR) out |= WPAD_BUTTON_PLUS;
    if (buttons & WPAD_CLASSIC_BUTTON_ZL) out |= WPAD_BUTTON_MINUS;
    if (buttons & WPAD_CLASSIC_BUTTON_PLUS)  out |= WPAD_BUTTON_PLUS;
    if (buttons & WPAD_CLASSIC_BUTTON_MINUS) out |= WPAD_BUTTON_MINUS;
    if (buttons & WPAD_CLASSIC_BUTTON_A)     out |= WPAD_BUTTON_A;
    if (buttons & WPAD_CLASSIC_BUTTON_B)     out |= WPAD_BUTTON_B;
    if (buttons & WPAD_CLASSIC_BUTTON_X)     out |= WPAD_BUTTON_2;
    if (buttons & WPAD_CLASSIC_BUTTON_Y)     out |= WPAD_BUTTON_1;
    if (buttons & WPAD_CLASSIC_BUTTON_HOME)  out |= WPAD_BUTTON_HOME;
    if (buttons & WPAD_CLASSIC_BUTTON_UP)    out |= WPAD_BUTTON_UP;
    if (buttons & WPAD_CLASSIC_BUTTON_DOWN)  out |= WPAD_BUTTON_DOWN;
    if (buttons & WPAD_CLASSIC_BUTTON_LEFT)  out |= WPAD_BUTTON_LEFT;
    if (buttons & WPAD_CLASSIC_BUTTON_RIGHT) out |= WPAD_BUTTON_RIGHT;
    return out;
}

static u32 map_gc(u32 gcpressed)
{
    u32 pressed = 0;
    if (gcpressed & PAD_TRIGGER_R) pressed |= WPAD_BUTTON_PLUS;
    if (gcpressed & PAD_TRIGGER_L) pressed |= WPAD_BUTTON_MINUS;
    if (gcpressed & PAD_BUTTON_A) pressed |= WPAD_BUTTON_A;
    if (gcpressed & PAD_BUTTON_B) pressed |= WPAD_BUTTON_B;
    if (gcpressed & PAD_BUTTON_X) pressed |= WPAD_BUTTON_2;
    if (gcpressed & PAD_BUTTON_Y) pressed |= WPAD_BUTTON_1;
    if (gcpressed & PAD_BUTTON_MENU) pressed |= WPAD_BUTTON_HOME;
    if (gcpressed & PAD_BUTTON_UP) pressed |= WPAD_BUTTON_UP;
    if (gcpressed & PAD_BUTTON_DOWN) pressed |= WPAD_BUTTON_DOWN;
    if (gcpressed & PAD_BUTTON_LEFT) pressed |= WPAD_BUTTON_LEFT;
    if (gcpressed & PAD_BUTTON_RIGHT) pressed |= WPAD_BUTTON_RIGHT;
    return pressed;
}

#ifdef HAVE_WIIDRC
static u32 map_drc(u32 buttons)
{
    u32 out = 0;
    if (buttons & (WIIDRC_BUTTON_R | WIIDRC_BUTTON_ZR)) out |= WPAD_BUTTON_PLUS;
    if (buttons & (WIIDRC_BUTTON_L | WIIDRC_BUTTON_ZL)) out |= WPAD_BUTTON_MINUS;
    if (buttons & WIIDRC_BUTTON_PLUS)  out |= WPAD_BUTTON_PLUS;
    if (buttons & WIIDRC_BUTTON_MINUS) out |= WPAD_BUTTON_MINUS;
    if (buttons & WIIDRC_BUTTON_A)     out |= WPAD_BUTTON_A;
    if (buttons & WIIDRC_BUTTON_B)     out |= WPAD_BUTTON_B;
    if (buttons & WIIDRC_BUTTON_X)     out |= WPAD_BUTTON_2;
    if (buttons & WIIDRC_BUTTON_Y)     out |= WPAD_BUTTON_1;
    if (buttons & WIIDRC_BUTTON_HOME)  out |= WPAD_BUTTON_HOME;
    if (buttons & WIIDRC_BUTTON_UP)    out |= WPAD_BUTTON_UP;
    if (buttons & WIIDRC_BUTTON_DOWN)  out |= WPAD_BUTTON_DOWN;
    if (buttons & WIIDRC_BUTTON_LEFT)  out |= WPAD_BUTTON_LEFT;
    if (buttons & WIIDRC_BUTTON_RIGHT) out |= WPAD_BUTTON_RIGHT;
    return out;
}
#endif

#ifdef HAVE_SICKSAXIS
static u32 poll_ds3(u32 *pressed_out)
{
    *pressed_out = 0;
    if (!ds3_initialized) return 0;
    if (!ss_is_connected(&ds3)) {
        ds3_previous = 0;
        if (ss_open(&ds3) > 0) ss_start_reading(&ds3);
        else return 0;
    }
    const struct SS_BUTTONS *b = &ds3.pad.buttons;
    u32 held = 0;
    if (b->start || b->R1 || b->R2) held |= WPAD_BUTTON_PLUS;
    if (b->select || b->L1 || b->L2) held |= WPAD_BUTTON_MINUS;
    if (b->cross) held |= WPAD_BUTTON_A;
    if (b->circle) held |= WPAD_BUTTON_B;
    if (b->triangle) held |= WPAD_BUTTON_2;
    if (b->square) held |= WPAD_BUTTON_1;
    if (b->PS) held |= WPAD_BUTTON_HOME;
    if (b->up) held |= WPAD_BUTTON_UP;
    if (b->down) held |= WPAD_BUTTON_DOWN;
    if (b->left) held |= WPAD_BUTTON_LEFT;
    if (b->right) held |= WPAD_BUTTON_RIGHT;
    *pressed_out = held & ~ds3_previous;
    ds3_previous = held;
    return held;
}
#endif

/* USB Keyboard support */
static lwp_t kbd_thread_hndl = LWP_THREAD_NULL;
static volatile bool kbd_thread_should_run = false;
static volatile u32 kbd_held;
static volatile u32 kbd_down;

// from Priiloader (/tools/Dacoslove/source/Input.cpp)
static void KBEventHandler(USBKeyboard_event event)
{
    if (event.type != USBKEYBOARD_PRESSED && event.type != USBKEYBOARD_RELEASED)
        return;

    u32 button = 0;

    switch (event.keyCode)
    {
    case 0x52: // Up
        button = WPAD_BUTTON_UP;
        break;
    case 0x51: // Down
        button = WPAD_BUTTON_DOWN;
        break;
    case 0x50: // Left
        button = WPAD_BUTTON_LEFT;
        break;
    case 0x4F: // Right
        button = WPAD_BUTTON_RIGHT;
        break;
    case 0x28: // Enter
    case 0x58: // Enter (Numpad)
    case 0x2C: // Space
        button = WPAD_BUTTON_A;
        break;
    case 0x2A: // Backspace
        button = WPAD_BUTTON_B;
        break;
    case 0x1B: // X
        button = WPAD_BUTTON_1;
        break;
    case 0x1C: // Y
        button = WPAD_BUTTON_2;
        break;
    case 0x4C: // Delete
    case 0x2D: // -
        button = WPAD_BUTTON_MINUS;
        break;
    case 0x2E: // =/+
        button = WPAD_BUTTON_PLUS;
        break;
    case 0x29: // ESC
    case 0x4A: // Home
        button = WPAD_BUTTON_HOME;
        break;

    default:
        break;
    }

    if (event.type == USBKEYBOARD_PRESSED)
    {
        kbd_held |= button;
        kbd_down |= button;
    }
    else
        kbd_held &= ~button;
}

static void *kbd_thread(void *userp)
{
    while (kbd_thread_should_run)
    {
        if (!USBKeyboard_IsConnected() && USBKeyboard_Open(KBEventHandler))
        {
            for (int i = 0; i < 3; i++)
            {
                USBKeyboard_SetLed(i, 1);
                usleep(250000);
            }
        }

        USBKeyboard_Scan();
        usleep(400);
    }

    return NULL;
}

void Input_Init(void)
{
    PAD_Init();
#ifdef HAVE_WUPC
    WUPC_Init();
#endif
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_ALL, 640, 480);
#ifdef HAVE_WIIDRC
    WiiDRC_Init();
#endif
    USB_Initialize();
#ifdef HAVE_SICKSAXIS
    if (ss_init() >= 0 && ss_initialize(&ds3) >= 0) {
        ds3_initialized = 1;
        if (ss_open(&ds3) > 0) ss_start_reading(&ds3);
    }
#endif

    USBKeyboard_Initialize();
    kbd_thread_should_run = true;
    LWP_CreateThread(&kbd_thread_hndl, kbd_thread, 0, 0, 0x4000, 0x7F);
}

void Input_Shutdown(void)
{
    kbd_thread_should_run = false;
    usleep(400);
    USBKeyboard_Close();
    USBKeyboard_Deinitialize();
    if (kbd_thread_hndl != LWP_THREAD_NULL)
        LWP_JoinThread(kbd_thread_hndl, 0);
    kbd_thread_hndl = LWP_THREAD_NULL;

#ifdef HAVE_SICKSAXIS
    if (ds3_initialized) {
        if (ss_is_connected(&ds3)) ss_stop_reading(&ds3);
        ss_close(&ds3);
    }
#endif
#ifdef HAVE_WUPC
    WUPC_Shutdown();
#endif
    WPAD_Shutdown();
}

static void use_stick(InputState *state, float ang, float mag)
{
    if (mag < 0.6f || state->stick_valid)
        return;
    state->stick_valid = true;
    state->stick_ang = ang;
}

void Input_Scan(InputState *state)
{
    u32 down = 0, held = 0, buttons_d, buttons_h;

    state->ir_valid = false;
    state->gforce = 0;
    state->stick_valid = false;

#ifdef HAVE_WUPC
    WUPC_UpdateButtonStats();
    buttons_d = buttons_h = 0;
    for (int chan = 0; chan < 4; chan++)
    {
        buttons_d |= WUPC_ButtonsDown(chan);
        buttons_h |= WUPC_ButtonsHeld(chan);
    }
    down |= map_classic(buttons_d);
    held |= map_classic(buttons_h);
#endif

    if (WPAD_ScanPads() > WPAD_ERR_NONE) {
        buttons_d = buttons_h = 0;
        for (int chan = 0; chan < 4; chan++)
        {
            buttons_d |= WPAD_ButtonsDown(chan);
            buttons_h |= WPAD_ButtonsHeld(chan);

            WPADData *wd = WPAD_Data(chan);
            if (!wd || wd->err != WPAD_ERR_NONE)
                continue;

            if (!state->ir_valid && wd->ir.valid)
            {
                state->ir_valid = true;
                state->ir_x = wd->ir.x;
                state->ir_y = wd->ir.y;
            }

            float g = sqrtf(wd->gforce.x * wd->gforce.x + wd->gforce.y * wd->gforce.y +
                            wd->gforce.z * wd->gforce.z);
            if (g > state->gforce)
                state->gforce = g;

            if (wd->exp.type == WPAD_EXP_NUNCHUK)
                use_stick(state, wd->exp.nunchuk.js.ang, wd->exp.nunchuk.js.mag);
            else if (wd->exp.type == WPAD_EXP_CLASSIC)
                use_stick(state, wd->exp.classic.ljs.ang, wd->exp.classic.ljs.mag);
        }
        down |= buttons_d | map_classic(buttons_d);
        held |= buttons_h | map_classic(buttons_h);
    }

#ifdef HAVE_WIIDRC
    if (WiiDRC_Inited() && WiiDRC_Connected() && WiiDRC_ScanPads()) {
        down |= map_drc(WiiDRC_ButtonsDown());
        held |= map_drc(WiiDRC_ButtonsHeld());
    }
#endif
#ifdef HAVE_SICKSAXIS
    {
        u32 ds3_down;
        held |= poll_ds3(&ds3_down);
        down |= ds3_down;
    }
#endif

    if (PAD_ScanPads() > PAD_ERR_NONE) {
        u32 gc_d = 0, gc_h = 0;
        for (int chan = 0; chan < 4; chan++)
        {
            gc_d |= PAD_ButtonsDown(chan);
            gc_h |= PAD_ButtonsHeld(chan);

            int sx = PAD_StickX(chan), sy = PAD_StickY(chan);
            float mag = sqrtf((float)(sx * sx + sy * sy)) / 80.0f;
            if (mag > 0.0f)
                use_stick(state, atan2f((float)sx, (float)sy) * (180.0f / M_PI), mag);
        }
        down |= map_gc(gc_d);
        held |= map_gc(gc_h);
    }

    // USB keyboard: presses are latched so a quick tap between two frames
    // still registers once.
    down |= kbd_down;
    held |= kbd_held;
    kbd_down = 0;

    state->down = down;
    state->held = held;
}
