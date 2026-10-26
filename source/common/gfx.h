#ifndef GFX_H
#define GFX_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint8_t r, g, b, a;
} Color;

// All drawing happens in a virtual 640x480 space; each backend scales it to
// the real screen.
#define SCREEN_W 640
#define SCREEN_H 480

enum
{
    TEX_BODY,   // plastic shade map, tinted by the vertex colour
    TEX_DETAIL, // bearings, untinted
};

void Gfx_Init(void);
void Gfx_Shutdown(void);

// A frame is drawn once per target (one per screen). The HUD is drawn on the
// target that returns true from Gfx_TargetHasHud.
void Gfx_BeginFrame(void);
int Gfx_NumTargets(void);
void Gfx_SetTarget(int target);
bool Gfx_TargetHasHud(int target);
void Gfx_EndFrame(void);

// 1.0 on 4:3 surfaces, 0.75 on 16:9 so circles stay round
float Gfx_AspectX(void);
// Same, for the surface the pointer / touch screen lives on
float Gfx_PointerAspectX(void);

// Solid quad / gradient (top colour -> bottom colour)
void Gfx_Rect(float x, float y, float w, float h, Color top, Color bottom);
void Gfx_Disc(float cx, float cy, float radius, Color inner, Color outer);

// Square texture centred on (cx, cy), rotated clockwise by angle (radians)
void Gfx_RotatedTexture(int tex, float cx, float cy, float size, float angle, Color tint);

// Text using the pre-rendered HUD font
float Gfx_TextWidth(const char *text, float scale);
void Gfx_Text(float x, float y, float scale, Color color, const char *text);
void Gfx_TextShadow(float x, float y, float scale, Color color, const char *text);

void Gfx_Pointer(float x, float y);

#endif
