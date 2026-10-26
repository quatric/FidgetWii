// Nintendo 3DS backend using citro2d. The top screen (400x240) shows the
// whole scene with the HUD; the bottom screen (320x240) shows just the
// spinner and is the touch surface.

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>
#include <citro2d.h>
#include "gfx.h"
#include "font_data.h"
#include "body_t3x.h"
#include "detail_t3x.h"
#include "font_small_t3x.h"
#include "font_large_t3x.h"

#define DISC_SEGMENTS 32

typedef struct
{
    const FontInfo *info;
    C3D_Tex tex;
    Tex3DS_SubTexture glyphs[95];
} Font;

static C3D_RenderTarget *targets[2];
static C3D_Tex body_tex, detail_tex;
static Tex3DS_SubTexture body_sub, detail_sub;
static Font font_small, font_large;
static float sx = 0.625f, sy = 0.5f; // virtual 640x480 -> screen pixels
static float aspect_x = 0.8f;

static const u32 clear_color = 0xFF1C1010; // ABGR: (16, 16, 28)

static inline u32 col(Color c)
{
    return C2D_Color32(c.r, c.g, c.b, c.a);
}

float Gfx_AspectX(void)
{
    return aspect_x;
}

float Gfx_PointerAspectX(void)
{
    return 1.0f; // the touch screen is 4:3
}

int Gfx_NumTargets(void)
{
    return 2;
}

bool Gfx_TargetHasHud(int target)
{
    return target == 0;
}

static void load_image(const void *data, size_t size, C3D_Tex *tex, Tex3DS_SubTexture *sub)
{
    Tex3DS_Texture t3x = Tex3DS_TextureImport(data, size, tex, NULL, false);
    if (!t3x)
        return;
    *sub = *Tex3DS_GetSubTexture(t3x, 0);
    Tex3DS_TextureFree(t3x);
    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
}

static void load_font(Font *font, const FontInfo *info, const void *data, size_t size)
{
    Tex3DS_SubTexture whole;
    load_image(data, size, &font->tex, &whole);
    font->info = info;
    float tw = font->tex.width, th = font->tex.height;
    for (int i = 0; i < 95; i++)
    {
        const FontGlyph *g = &info->glyphs[i];
        Tex3DS_SubTexture *s = &font->glyphs[i];
        s->width = g->w;
        s->height = g->h;
        s->left = g->x / tw;
        s->right = (g->x + g->w) / tw;
        s->top = 1.0f - g->y / th;
        s->bottom = 1.0f - (g->y + g->h) / th;
    }
}

void Gfx_Init(void)
{
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    targets[0] = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    targets[1] = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    load_image(body_t3x, body_t3x_size, &body_tex, &body_sub);
    load_image(detail_t3x, detail_t3x_size, &detail_tex, &detail_sub);
    load_font(&font_small, &font_small_info, font_small_t3x, font_small_t3x_size);
    load_font(&font_large, &font_large_info, font_large_t3x, font_large_t3x_size);
}

void Gfx_Shutdown(void)
{
    C3D_TexDelete(&body_tex);
    C3D_TexDelete(&detail_tex);
    C3D_TexDelete(&font_small.tex);
    C3D_TexDelete(&font_large.tex);
    C2D_Fini();
    C3D_Fini();
}

void Gfx_BeginFrame(void)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
}

void Gfx_SetTarget(int target)
{
    C2D_TargetClear(targets[target], clear_color);
    C2D_SceneBegin(targets[target]);
    C2D_SetTintMode(C2D_TintMult);
    if (target == 0)
    {
        sx = 400.0f / SCREEN_W;
        sy = 240.0f / SCREEN_H;
    }
    else
    {
        sx = 320.0f / SCREEN_W;
        sy = 240.0f / SCREEN_H;
    }
    aspect_x = sy / sx;
}

void Gfx_EndFrame(void)
{
    C3D_FrameEnd(0);
}

void Gfx_Rect(float x, float y, float w, float h, Color top, Color bottom)
{
    u32 t = col(top), b = col(bottom);
    C2D_DrawRectangle(x * sx, y * sy, 0, w * sx, h * sy, t, t, b, b);
}

void Gfx_Disc(float cx, float cy, float radius, Color inner, Color outer)
{
    u32 ci = col(inner), co = col(outer);
    float px = cx + radius * aspect_x, py = cy;
    for (int i = 1; i <= DISC_SEGMENTS; i++)
    {
        float a = i * (2.0f * M_PI / DISC_SEGMENTS);
        float nx = cx + cosf(a) * radius * aspect_x, ny = cy + sinf(a) * radius;
        C2D_DrawTriangle(cx * sx, cy * sy, ci, px * sx, py * sy, co, nx * sx, ny * sy, co, 0);
        px = nx;
        py = ny;
    }
}

void Gfx_RotatedTexture(int which, float cx, float cy, float size, float angle, Color tint)
{
    C2D_ImageTint t;
    C2D_PlainImageTint(&t, col(tint), 1.0f);

    C2D_Image img;
    if (which == TEX_BODY)
        img = (C2D_Image){&body_tex, &body_sub};
    else
        img = (C2D_Image){&detail_tex, &detail_sub};

    // sx * aspect_x == sy, so the spinner is scaled uniformly and stays round
    float scale = size * sy / img.subtex->width;
    // citro2d rotates counterclockwise, the game clockwise
    C2D_DrawImageAtRotated(img, cx * sx, cy * sy, 0, -angle, &t, scale, scale);
}

// scale 1.0 is body text (16px em); larger sizes use the large atlas
static const Font *pick_font(float scale, float *px)
{
    const Font *font = scale >= 1.5f ? &font_large : &font_small;
    *px = 16.0f * scale * 1.15f / font->info->size;
    return font;
}

float Gfx_TextWidth(const char *text, float scale)
{
    float k;
    const Font *font = pick_font(scale, &k);
    float w = 0.0f;
    for (; *text; text++)
    {
        uint8_t c = (uint8_t)*text;
        if (c < 32 || c > 126)
            c = '?';
        w += font->info->glyphs[c - 32].advance * 0.25f;
    }
    return w * k * aspect_x;
}

void Gfx_Text(float x, float y, float scale, Color color, const char *text)
{
    float k;
    const Font *font = pick_font(scale, &k);
    const FontInfo *info = font->info;

    float baseline = y + 16.0f * scale * 0.95f;
    float pen = x;

    C2D_ImageTint t;
    C2D_PlainImageTint(&t, col(color), 1.0f);

    for (; *text; text++)
    {
        uint8_t c = (uint8_t)*text;
        if (c < 32 || c > 126)
            c = '?';
        const FontGlyph *g = &info->glyphs[c - 32];
        if (c != ' ' && g->w && g->h)
        {
            float gx = pen + g->left * k * aspect_x, gy = baseline + g->top * k;
            C2D_Image img = {(C3D_Tex *)&font->tex, &font->glyphs[c - 32]};
            C2D_DrawImageAt(img, gx * sx, gy * sy, 0, &t, k * sy, k * sy);
        }
        pen += g->advance * 0.25f * k * aspect_x;
    }
}

void Gfx_TextShadow(float x, float y, float scale, Color color, const char *text)
{
    Color shadow = {0, 0, 0, (uint8_t)(color.a * 3 / 4)};
    float off = scale >= 2.0f ? 2.0f : 1.0f;
    Gfx_Text(x + off, y + off, scale, shadow, text);
    Gfx_Text(x, y, scale, color, text);
}

void Gfx_Pointer(float x, float y)
{
    Gfx_Disc(x, y, 12, (Color){0, 0, 0, 120}, (Color){0, 0, 0, 60});
    Gfx_Disc(x, y, 8, (Color){255, 255, 255, 240}, (Color){210, 220, 255, 240});
    Gfx_Disc(x, y, 3, (Color){40, 120, 255, 255}, (Color){40, 120, 255, 255});
}
