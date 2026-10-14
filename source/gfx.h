#ifndef GFX_H
#define GFX_H

#include <gccore.h>

typedef struct
{
    u8 r, g, b, a;
} Color;

#define SCREEN_W 640
#define SCREEN_H 480

void Gfx_Init(void);
void Gfx_BeginFrame(void);
void Gfx_EndFrame(void);

// 1.0 on 4:3 TVs, 0.75 on 16:9 so circles stay round
float Gfx_AspectX(void);

void Gfx_LoadTexture(GXTexObj *obj, const void *data, u32 size, u16 w, u16 h, u8 fmt, bool smooth);

// Solid quad / gradient (top colour -> bottom colour)
void Gfx_Rect(float x, float y, float w, float h, Color top, Color bottom);
void Gfx_Disc(float cx, float cy, float radius, Color inner, Color outer);

// Square texture centred on (cx, cy), rotated clockwise by angle (radians)
void Gfx_RotatedTexture(GXTexObj *tex, float cx, float cy, float size, float angle, Color tint);

// Text using the libogc 8x16 console font
float Gfx_TextWidth(const char *text, float scale);
void Gfx_Text(float x, float y, float scale, Color color, const char *text);
void Gfx_TextShadow(float x, float y, float scale, Color color, const char *text);

void Gfx_Pointer(float x, float y);

#endif
