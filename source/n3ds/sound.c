// Spinning sound: one short "whoosh" is looped, and its playback rate is set
// so that it repeats once per arm passing by (three times per revolution).
// Needs the DSP firmware dump (sdmc:/3ds/dspfirm.cdc); without it the game
// just runs silently.

#include <math.h>
#include <string.h>
#include <3ds.h>
#include "sound.h"

#define WHOOSH_LEN 512
#define CHANNEL 0

static s16 *whoosh;
static ndspWaveBuf wave;
static bool ready;

void Sound_Init(void)
{
    if (R_FAILED(ndspInit()))
        return;

    whoosh = linearAlloc(WHOOSH_LEN * sizeof(s16));
    if (!whoosh)
        return;

    // Low-passed noise shaped by a smooth envelope, plus a faint bearing hum
    float lp = 0.0f;
    u32 seed = 0x1234567;
    for (int i = 0; i < WHOOSH_LEN; i++)
    {
        seed = seed * 1103515245 + 12345;
        float noise = ((seed >> 16) & 0x7FFF) / 16384.0f - 1.0f;
        lp += (noise - lp) * 0.18f;
        float t = (float)i / WHOOSH_LEN;
        float env = sinf(t * M_PI);
        env *= env;
        float hum = sinf(t * 2.0f * M_PI * 6.0f) * 0.15f;
        float v = (lp * 2.2f * env + hum) * 0.8f;
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        whoosh[i] = (s16)(v * 32767.0f);
    }
    DSP_FlushDataCache(whoosh, WHOOSH_LEN * sizeof(s16));

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(CHANNEL, NDSP_INTERP_LINEAR);
    ndspChnSetFormat(CHANNEL, NDSP_FORMAT_MONO_PCM16);
    ndspChnSetRate(CHANNEL, 8000.0f);

    float mix[12] = {0};
    ndspChnSetMix(CHANNEL, mix);

    memset(&wave, 0, sizeof(wave));
    wave.data_vaddr = whoosh;
    wave.nsamples = WHOOSH_LEN;
    wave.looping = true;
    ndspChnWaveBufAdd(CHANNEL, &wave);
    ready = true;
}

void Sound_Update(float rpm, bool enabled)
{
    if (!ready)
        return;

    float arms_per_sec = fabsf(rpm) * 3.0f / 60.0f;
    float volume = 0.0f;
    if (enabled && arms_per_sec > 0.5f)
    {
        float v = fabsf(rpm) / 900.0f;
        if (v > 1.0f) v = 1.0f;
        volume = sqrtf(v) * 150.0f / 255.0f;
    }

    float pitch = arms_per_sec * WHOOSH_LEN;
    if (pitch < 1000.0f) pitch = 1000.0f;
    if (pitch > 48000.0f) pitch = 48000.0f;

    float mix[12] = {0};
    mix[0] = mix[1] = volume;
    ndspChnSetMix(CHANNEL, mix);
    ndspChnSetRate(CHANNEL, pitch);
}

void Sound_Shutdown(void)
{
    if (!ready)
        return;
    ndspChnWaveBufClear(CHANNEL);
    ndspExit();
    linearFree(whoosh);
    ready = false;
}
