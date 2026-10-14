#include <malloc.h>
#include <math.h>
#include <string.h>
#include <gccore.h>
#include "gfx.h"
#include "font_data.h"
#include "font_regular_bin.h"
#include "font_bold_bin.h"

#define FIFO_SIZE (256 * 1024)

typedef struct
{
    const FontInfo *info;
    GXTexObj tex;
} Font;

static void *xfb[2];
static int fb;
static GXRModeObj *rmode;
static Font font_regular, font_bold;
static float aspect_x = 1.0f;

float Gfx_AspectX(void)
{
    return aspect_x;
}

static void load_font(Font *font, const FontInfo *info, const u8 *data, u32 size)
{
    font->info = info;
    Gfx_LoadTexture(&font->tex, data, size, info->atlas_w, info->atlas_h, GX_TF_I8, true);
}

void Gfx_LoadTexture(GXTexObj *obj, const void *data, u32 size, u16 w, u16 h, u8 fmt, bool smooth)
{
    DCFlushRange((void *)data, size);
    GX_InitTexObj(obj, (void *)data, w, h, fmt, GX_CLAMP, GX_CLAMP, GX_FALSE);
    if (smooth)
        GX_InitTexObjLOD(obj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_TRUE, GX_ANISO_4);
    else
        GX_InitTexObjLOD(obj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
}

void Gfx_Init(void)
{
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);

    if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
        aspect_x = 0.75f;

    xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb[0]);
    VIDEO_SetBlack(TRUE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    void *fifo = memalign(32, FIFO_SIZE);
    memset(fifo, 0, FIFO_SIZE);
    GX_Init(fifo, FIFO_SIZE);

    GXColor clear = {16, 16, 28, 255};
    GX_SetCopyClear(clear, GX_MAX_Z24);

    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering,
                    ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
    GX_SetPixelFmt(rmode->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetCullMode(GX_CULL_NONE);
    GX_CopyDisp(xfb[0], GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetNumTevStages(1);

    Mtx44 proj;
    guOrtho(proj, 0, SCREEN_H, 0, SCREEN_W, 0, 300);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

    Mtx view;
    guMtxIdentity(view);
    guMtxTransApply(view, view, 0, 0, -50.0f);
    GX_LoadPosMtxImm(view, GX_PNMTX0);

    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);

    load_font(&font_regular, &font_regular_info, font_regular_bin, font_regular_bin_size);
    load_font(&font_bold, &font_bold_info, font_bold_bin, font_bold_bin_size);
}

void Gfx_BeginFrame(void)
{
    GX_InvVtxCache();
    GX_InvalidateTexAll();
}

void Gfx_EndFrame(void)
{
    static bool first = true;

    GX_DrawDone();
    fb ^= 1;
    GX_CopyDisp(xfb[fb], GX_TRUE);
    GX_DrawDone();
    VIDEO_SetNextFramebuffer(xfb[fb]);
    if (first)
    {
        VIDEO_SetBlack(FALSE);
        first = false;
    }
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

static void use_color_only(void)
{
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
}

static void use_texture(GXTexObj *tex)
{
    GX_LoadTexObj(tex, GX_TEXMAP0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
}

static inline void vtx(float x, float y, Color c, float s, float t)
{
    GX_Position2f32(x, y);
    GX_Color4u8(c.r, c.g, c.b, c.a);
    GX_TexCoord2f32(s, t);
}

void Gfx_Rect(float x, float y, float w, float h, Color top, Color bottom)
{
    use_color_only();
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    vtx(x, y, top, 0, 0);
    vtx(x + w, y, top, 1, 0);
    vtx(x + w, y + h, bottom, 1, 1);
    vtx(x, y + h, bottom, 0, 1);
    GX_End();
}

void Gfx_Disc(float cx, float cy, float radius, Color inner, Color outer)
{
    const int segments = 48;
    use_color_only();
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, segments + 2);
    vtx(cx, cy, inner, 0, 0);
    for (int i = 0; i <= segments; i++)
    {
        float a = i * (2.0f * M_PI / segments);
        vtx(cx + cosf(a) * radius * aspect_x, cy + sinf(a) * radius, outer, 0, 0);
    }
    GX_End();
}

void Gfx_RotatedTexture(GXTexObj *tex, float cx, float cy, float size, float angle, Color tint)
{
    static const float corners[4][4] = {
        {-0.5f, -0.5f, 0, 0}, {0.5f, -0.5f, 1, 0}, {0.5f, 0.5f, 1, 1}, {-0.5f, 0.5f, 0, 1}};
    float c = cosf(angle), s = sinf(angle);

    use_texture(tex);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    for (int i = 0; i < 4; i++)
    {
        float x = corners[i][0] * size, y = corners[i][1] * size;
        vtx(cx + (x * c - y * s) * aspect_x, cy + (x * s + y * c), tint, corners[i][2], corners[i][3]);
    }
    GX_End();
}

// scale 1.0 is body text (16px em); larger sizes use the bold face
static const Font *pick_font(float scale, float *px)
{
    const Font *font = scale >= 1.5f ? &font_bold : &font_regular;
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
        u8 c = (u8)*text;
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
    int quads = 0;
    for (const char *p = text; *p; p++)
        if (*p != ' ')
            quads++;
    if (!quads)
        return;

    float baseline = y + 16.0f * scale * 0.95f;
    float pen = x;
    float iw = 1.0f / info->atlas_w, ih = 1.0f / info->atlas_h;

    use_texture((GXTexObj *)&font->tex);
    GX_Begin(GX_QUADS, GX_VTXFMT0, quads * 4);
    for (; *text; text++)
    {
        u8 c = (u8)*text;
        if (c < 32 || c > 126)
            c = '?';
        const FontGlyph *g = &info->glyphs[c - 32];
        if (c != ' ')
        {
            float gx = pen + g->left * k * aspect_x, gy = baseline + g->top * k;
            float gw = g->w * k * aspect_x, gh = g->h * k;
            float s0 = g->x * iw, t0 = g->y * ih, s1 = (g->x + g->w) * iw, t1 = (g->y + g->h) * ih;
            vtx(gx, gy, color, s0, t0);
            vtx(gx + gw, gy, color, s1, t0);
            vtx(gx + gw, gy + gh, color, s1, t1);
            vtx(gx, gy + gh, color, s0, t1);
        }
        pen += g->advance * 0.25f * k * aspect_x;
    }
    GX_End();
}

void Gfx_TextShadow(float x, float y, float scale, Color color, const char *text)
{
    Color shadow = {0, 0, 0, (u8)(color.a * 3 / 4)};
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
