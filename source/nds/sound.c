// Spinning sound: one short "whoosh" is looped, and its playback rate is set
// so that it repeats once per arm passing by (three times per revolution).

#include <malloc.h>
#include <math.h>
#include <nds.h>
#include "sound.h"

#define WHOOSH_LEN 256

static s16 *whoosh;
static int voice = -1;

void Sound_Init(void)
{
    soundEnable();
    whoosh = memalign(32, WHOOSH_LEN * sizeof(s16));

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
    DC_FlushRange(whoosh, WHOOSH_LEN * sizeof(s16));
}

void Sound_Update(float rpm, bool enabled)
{
    float arms_per_sec = fabsf(rpm) * 3.0f / 60.0f;
    int volume = 0;
    if (enabled && arms_per_sec > 0.5f)
    {
        float v = fabsf(rpm) / 900.0f;
        if (v > 1.0f) v = 1.0f;
        volume = (int)(sqrtf(v) * 100.0f);
    }

    if (volume <= 0)
    {
        if (voice >= 0)
        {
            soundKill(voice);
            voice = -1;
        }
        return;
    }

    // The hardware timer limits the rate: 3000 RPM would need 38 kHz
    int freq = (int)(arms_per_sec * WHOOSH_LEN);
    if (freq < 512) freq = 512;
    if (freq > 32000) freq = 32000;

    if (voice < 0)
        voice = soundPlaySample(whoosh, SoundFormat_16Bit, WHOOSH_LEN * sizeof(s16), freq, volume, 64, true, 0);
    else
    {
        soundSetFreq(voice, freq);
        soundSetVolume(voice, volume);
    }
}

void Sound_Shutdown(void)
{
    if (voice >= 0)
        soundKill(voice);
    voice = -1;
}
