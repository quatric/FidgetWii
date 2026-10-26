// SDL2 renderer backend (Wii U and Switch).

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "gfx.h"
#include "platform.h"
#include "font_data.h"
#include "body_ia_bin.h"
#include "detail_rgba_bin.h"
#include "font_small_a8_bin.h"
#include "font_large_a8_bin.h"

#define TEX_SIZE 512
#define MAX_VERTS 4096

typedef struct
{
    const FontInfo *info;
    SDL_Texture *tex;
} Font;

SDL_Window *gfx_window;
static SDL_Renderer *renderer;
static SDL_Texture *body_tex, *detail_tex;
static Font font_small, font_large;
static float sx = 1.0f, sy = 1.0f;
static float aspect_x = 1.0f;

float Gfx_AspectX(void)
{
    return aspect_x;
}

float Gfx_PointerAspectX(void)
{
    return aspect_x;
}

int Gfx_NumTargets(void)
{
    return 1;
}

void Gfx_SetTarget(int target)
{
}

bool Gfx_TargetHasHud(int target)
{
    return true;
}

static SDL_Texture *make_texture(const uint8_t *rgba, int w, int h)
{
    SDL_Texture *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t)
        return NULL;
    SDL_UpdateTexture(t, NULL, rgba, w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
    return t;
}

static void load_font(Font *font, const FontInfo *info, const uint8_t *a8)
{
    int n = info->atlas_w * info->atlas_h;
    uint8_t *rgba = malloc(n * 4);
    for (int i = 0; i < n; i++)
    {
        // The atlas is intensity-only: it supplies both colour and alpha.
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = rgba[i * 4 + 3] = a8[i];
    }
    font->info = info;
    font->tex = make_texture(rgba, info->atlas_w, info->atlas_h);
    free(rgba);
}

void Gfx_Init(void)
{
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    gfx_window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  1280, 720, SDL_WINDOW_FULLSCREEN);
    renderer = SDL_CreateRenderer(gfx_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        renderer = SDL_CreateRenderer(gfx_window, -1, SDL_RENDERER_SOFTWARE);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int w = 1280, h = 720;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    sx = w / (float)SCREEN_W;
    sy = h / (float)SCREEN_H;
    aspect_x = sy / sx; // 0.75 on a 16:9 screen

    // body: IA8 -> white shade map with alpha
    int n = TEX_SIZE * TEX_SIZE;
    uint8_t *rgba = malloc(n * 4);
    for (int i = 0; i < n; i++)
    {
        uint8_t shade = body_ia_bin[i * 2], alpha = body_ia_bin[i * 2 + 1];
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = shade;
        rgba[i * 4 + 3] = alpha;
    }
    body_tex = make_texture(rgba, TEX_SIZE, TEX_SIZE);
    free(rgba);
    detail_tex = make_texture(detail_rgba_bin, TEX_SIZE, TEX_SIZE);

    load_font(&font_small, &font_small_info, font_small_a8_bin);
    load_font(&font_large, &font_large_info, font_large_a8_bin);
}

void Gfx_Shutdown(void)
{
    SDL_DestroyTexture(body_tex);
    SDL_DestroyTexture(detail_tex);
    SDL_DestroyTexture(font_small.tex);
    SDL_DestroyTexture(font_large.tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(gfx_window);
}

void Gfx_BeginFrame(void)
{
    SDL_SetRenderDrawColor(renderer, 16, 16, 28, 255);
    SDL_RenderClear(renderer);
}

void Gfx_EndFrame(void)
{
    SDL_RenderPresent(renderer);
}

static SDL_Vertex verts[MAX_VERTS];
static int indices[MAX_VERTS * 3 / 2];
static int nverts, nindices;

static void batch_begin(void)
{
    nverts = nindices = 0;
}

static void batch_vertex(float x, float y, Color c, float s, float t)
{
    SDL_Vertex *v = &verts[nverts++];
    v->position.x = x * sx;
    v->position.y = y * sy;
    v->color = (SDL_Color){c.r, c.g, c.b, c.a};
    v->tex_coord.x = s;
    v->tex_coord.y = t;
}

static void batch_quad(int first)
{
    static const int q[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++)
        indices[nindices++] = first + q[i];
}

static void batch_flush(SDL_Texture *tex)
{
    if (nindices)
        SDL_RenderGeometry(renderer, tex, verts, nverts, indices, nindices);
    batch_begin();
}

void Gfx_Rect(float x, float y, float w, float h, Color top, Color bottom)
{
    batch_begin();
    batch_vertex(x, y, top, 0, 0);
    batch_vertex(x + w, y, top, 1, 0);
    batch_vertex(x + w, y + h, bottom, 1, 1);
    batch_vertex(x, y + h, bottom, 0, 1);
    batch_quad(0);
    batch_flush(NULL);
}

void Gfx_Disc(float cx, float cy, float radius, Color inner, Color outer)
{
    const int segments = 48;
    batch_begin();
    batch_vertex(cx, cy, inner, 0, 0);
    for (int i = 0; i <= segments; i++)
    {
        float a = i * (2.0f * M_PI / segments);
        batch_vertex(cx + cosf(a) * radius * aspect_x, cy + sinf(a) * radius, outer, 0, 0);
    }
    for (int i = 0; i < segments; i++)
    {
        indices[nindices++] = 0;
        indices[nindices++] = i + 1;
        indices[nindices++] = i + 2;
    }
    batch_flush(NULL);
}

void Gfx_RotatedTexture(int which, float cx, float cy, float size, float angle, Color tint)
{
    static const float corners[4][4] = {
        {-0.5f, -0.5f, 0, 0}, {0.5f, -0.5f, 1, 0}, {0.5f, 0.5f, 1, 1}, {-0.5f, 0.5f, 0, 1}};
    float c = cosf(angle), s = sinf(angle);

    batch_begin();
    for (int i = 0; i < 4; i++)
    {
        float x = corners[i][0] * size, y = corners[i][1] * size;
        batch_vertex(cx + (x * c - y * s) * aspect_x, cy + (x * s + y * c), tint, corners[i][2], corners[i][3]);
    }
    batch_quad(0);
    batch_flush(which == TEX_BODY ? body_tex : detail_tex);
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
    float iw = 1.0f / info->atlas_w, ih = 1.0f / info->atlas_h;

    batch_begin();
    for (; *text && nverts + 4 <= MAX_VERTS; text++)
    {
        uint8_t c = (uint8_t)*text;
        if (c < 32 || c > 126)
            c = '?';
        const FontGlyph *g = &info->glyphs[c - 32];
        if (c != ' ')
        {
            float gx = pen + g->left * k * aspect_x, gy = baseline + g->top * k;
            float gw = g->w * k * aspect_x, gh = g->h * k;
            float s0 = g->x * iw, t0 = g->y * ih, s1 = (g->x + g->w) * iw, t1 = (g->y + g->h) * ih;
            batch_vertex(gx, gy, color, s0, t0);
            batch_vertex(gx + gw, gy, color, s1, t0);
            batch_vertex(gx + gw, gy + gh, color, s1, t1);
            batch_vertex(gx, gy + gh, color, s0, t1);
            batch_quad(nverts - 4);
        }
        pen += g->advance * 0.25f * k * aspect_x;
    }
    batch_flush(font->tex);
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
