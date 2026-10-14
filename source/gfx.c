#include <malloc.h>
#include <math.h>
#include <string.h>
#include <gccore.h>
#include "gfx.h"

#define FIFO_SIZE (256 * 1024)

extern u8 console_font_8x16[];

static void *xfb[2];
static int fb;
static GXRModeObj *rmode;
static GXTexObj font_tex;
static float aspect_x = 1.0f;

float Gfx_AspectX(void)
{
    return aspect_x;
}

static void build_font(void)
{
    // 16x16 grid of 8x16 glyphs -> 128x256 I8 texture (8x4 texel blocks)
    const int w = 128, h = 256;
    u8 *linear = malloc(w * h);
    u8 *tiled = memalign(32, w * h);

    for (int c = 0; c < 256; c++)
    {
        int ox = (c % 16) * 8, oy = (c / 16) * 16;
        for (int y = 0; y < 16; y++)
        {
            u8 bits = console_font_8x16[c * 16 + y];
            for (int x = 0; x < 8; x++)
                linear[(oy + y) * w + ox + x] = (bits & (0x80 >> x)) ? 0xFF : 0x00;
        }
    }

    u8 *dst = tiled;
    for (int ty = 0; ty < h; ty += 4)
        for (int tx = 0; tx < w; tx += 8)
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 8; x++)
                    *dst++ = linear[(ty + y) * w + tx + x];

    free(linear);
    Gfx_LoadTexture(&font_tex, tiled, w * h, w, h, GX_TF_I8, false);
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

    build_font();
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

float Gfx_TextWidth(const char *text, float scale)
{
    return strlen(text) * 8.0f * scale * aspect_x;
}

void Gfx_Text(float x, float y, float scale, Color color, const char *text)
{
    int len = strlen(text);
    if (!len)
        return;

    float cw = 8.0f * scale * aspect_x, ch = 16.0f * scale;
    use_texture(&font_tex);
    GX_Begin(GX_QUADS, GX_VTXFMT0, len * 4);
    for (int i = 0; i < len; i++)
    {
        u8 c = (u8)text[i];
        float s0 = (c % 16) / 16.0f, t0 = (c / 16) / 16.0f;
        float s1 = s0 + 1.0f / 16.0f, t1 = t0 + 1.0f / 16.0f;
        float px = x + i * cw;
        vtx(px, y, color, s0, t0);
        vtx(px + cw, y, color, s1, t0);
        vtx(px + cw, y + ch, color, s1, t1);
        vtx(px, y + ch, color, s0, t1);
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
