// Spinning sound: one short "whoosh" is looped, and its playback rate is set
// so that it repeats once per arm passing by (three times per revolution).

#include <malloc.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <asndlib.h>
#include "sound.h"

#define WHOOSH_LEN 512
#define VOICE 0

static s16 *whoosh;
static bool playing;

void Sound_Init(void)
{
    ASND_Init();
    ASND_Pause(0);

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
    DCFlushRange(whoosh, WHOOSH_LEN * sizeof(s16));
}

void Sound_Update(float rpm, bool enabled)
{
    float arms_per_sec = fabsf(rpm) * 3.0f / 60.0f;
    int volume = 0;
    if (enabled && arms_per_sec > 0.5f)
    {
        float v = fabsf(rpm) / 900.0f;
        if (v > 1.0f) v = 1.0f;
        volume = (int)(sqrtf(v) * 150.0f);
    }

    if (volume <= 0)
    {
        if (playing)
        {
            ASND_StopVoice(VOICE);
            playing = false;
        }
        return;
    }

    int pitch = (int)(arms_per_sec * WHOOSH_LEN);
    if (pitch < 1) pitch = 1;
    if (pitch > 144000) pitch = 144000;

    if (!playing)
    {
        ASND_SetInfiniteVoice(VOICE, VOICE_MONO_16BIT, pitch, 0, whoosh,
                              WHOOSH_LEN * sizeof(s16), volume, volume);
        playing = true;
    }
    else
    {
        ASND_ChangePitchVoice(VOICE, pitch);
        ASND_ChangeVolumeVoice(VOICE, volume, volume);
    }
}

void Sound_Shutdown(void)
{
    ASND_StopVoice(VOICE);
    ASND_End();
}
