// Nintendo DS backend. The 3D engine draws the spinner on the bottom screen
// (the touch screen); the HUD is printed on the top screen's text console
// (see hud.c).

#include <math.h>
#include <nds.h>
#include "gfx.h"
#include "body_a3i5_bin.h"
#include "body_pal_bin.h"
#include "detail_rgba5551_bin.h"

#define UNIT (0.4f / 32.0f) // virtual 640x480 pixel -> GL units (screen is 8x6)
#define DISC_SEGMENTS 20

static int body_id, detail_id;
static float depth;
static int poly_id;

float Gfx_AspectX(void)
{
    return 1.0f;
}

float Gfx_PointerAspectX(void)
{
    return 1.0f;
}

int Gfx_NumTargets(void)
{
    return 1;
}

void Gfx_SetTarget(int target)
{
}

// The HUD is not drawn with Gfx_Text on this platform.
bool Gfx_TargetHasHud(int target)
{
    return false;
}

void Gfx_Init(void)
{
    lcdMainOnBottom();
    videoSetMode(MODE_0_3D);
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);

    glInit();
    glEnable(GL_TEXTURE_2D);
    glClearColor(3, 3, 6, 31);
    glClearPolyID(63);
    glClearDepth(0x7FFF);
    glViewport(0, 0, 255, 191);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 8, 6, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glGenTextures(1, &body_id);
    glBindTexture(0, body_id);
    glTexImage2D(0, 0, GL_RGB32_A3, TEXTURE_SIZE_128, TEXTURE_SIZE_128, 0,
                 TEXGEN_TEXCOORD, body_a3i5_bin);
    glColorTableEXT(0, 0, 32, 0, 0, (const u16 *)body_pal_bin);

    glGenTextures(1, &detail_id);
    glBindTexture(0, detail_id);
    glTexImage2D(0, 0, GL_RGBA, TEXTURE_SIZE_128, TEXTURE_SIZE_128, 0,
                 TEXGEN_TEXCOORD, detail_rgba5551_bin);
}

void Gfx_Shutdown(void)
{
}

void Gfx_BeginFrame(void)
{
    depth = -0.95f;
    poly_id = 0;
}

void Gfx_EndFrame(void)
{
    glFlush(GL_TRANS_MANUALSORT);
    swiWaitForVBlank();
}

// Each draw sits slightly nearer than the previous one so they stack in
// submission order; translucent polygons need distinct IDs to blend.
static bool begin_poly(uint8_t alpha)
{
    int a = (alpha * 31 + 127) / 255;
    if (a == 0)
        return false; // alpha 0 would be wireframe
    poly_id = poly_id % 60 + 1;
    glPolyFmt(POLY_ALPHA(a) | POLY_ID(poly_id) | POLY_CULL_NONE);
    depth += 0.004f;
    return true;
}

static inline void color(Color c)
{
    glColor3b(c.r, c.g, c.b);
}

static inline void vertex(float x, float y)
{
    glVertex3f(x * UNIT, y * UNIT, depth);
}

void Gfx_Rect(float x, float y, float w, float h, Color top, Color bottom)
{
    if (!begin_poly(top.a > bottom.a ? top.a : bottom.a))
        return;
    glBindTexture(0, 0);
    glBegin(GL_QUADS);
    color(top);
    vertex(x, y);
    vertex(x + w, y);
    color(bottom);
    vertex(x + w, y + h);
    vertex(x, y + h);
    glEnd();
}

void Gfx_Disc(float cx, float cy, float radius, Color inner, Color outer)
{
    if (!begin_poly(inner.a > outer.a ? inner.a : outer.a))
        return;
    glBindTexture(0, 0);
    glBegin(GL_TRIANGLES);
    float px = cx + radius, py = cy;
    for (int i = 1; i <= DISC_SEGMENTS; i++)
    {
        float a = i * (2.0f * M_PI / DISC_SEGMENTS);
        float nx = cx + cosf(a) * radius, ny = cy + sinf(a) * radius;
        color(inner);
        vertex(cx, cy);
        color(outer);
        vertex(px, py);
        vertex(nx, ny);
        px = nx;
        py = ny;
    }
    glEnd();
}

void Gfx_RotatedTexture(int which, float cx, float cy, float size, float angle, Color tint)
{
    static const float corners[4][4] = {
        {-0.5f, -0.5f, 0, 0}, {0.5f, -0.5f, 128, 0}, {0.5f, 0.5f, 128, 128}, {-0.5f, 0.5f, 0, 128}};
    float c = cosf(angle), s = sinf(angle);

    if (!begin_poly(tint.a))
        return;
    glBindTexture(0, which == TEX_BODY ? body_id : detail_id);
    color(tint);
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; i++)
    {
        float x = corners[i][0] * size, y = corners[i][1] * size;
        glTexCoord2t16(inttot16((int)corners[i][2]), inttot16((int)corners[i][3]));
        vertex(cx + (x * c - y * s), cy + (x * s + y * c));
    }
    glEnd();
}

// Text is shown on the top screen, not drawn here.
float Gfx_TextWidth(const char *text, float scale)
{
    return 0.0f;
}

void Gfx_Text(float x, float y, float scale, Color color, const char *text)
{
}

void Gfx_TextShadow(float x, float y, float scale, Color color, const char *text)
{
}

void Gfx_Pointer(float x, float y)
{
    Gfx_Disc(x, y, 12, (Color){0, 0, 0, 120}, (Color){0, 0, 0, 60});
    Gfx_Disc(x, y, 8, (Color){255, 255, 255, 240}, (Color){210, 220, 255, 240});
    Gfx_Disc(x, y, 3, (Color){40, 120, 255, 255}, (Color){40, 120, 255, 255});
}
