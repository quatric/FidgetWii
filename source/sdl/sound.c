// Spinning sound: one short "whoosh" is looped, and its playback rate is set
// so that it repeats once per arm passing by (three times per revolution).

#include <math.h>
#include <stdlib.h>
#include <SDL.h>
#include "sound.h"

#define WHOOSH_LEN 512
#define RATE 48000

static int16_t whoosh[WHOOSH_LEN];
static SDL_AudioDeviceID dev;
static volatile float target_pitch;  // whoosh samples per second
static volatile float target_volume; // 0..1

static float phase, cur_volume;

static void callback(void *user, Uint8 *stream, int len)
{
    int16_t *out = (int16_t *)stream;
    int frames = len / (2 * sizeof(int16_t));
    float step = target_pitch / RATE;
    float vol_target = target_volume;

    for (int i = 0; i < frames; i++)
    {
        // Ease the volume so stopping and starting doesn't click
        cur_volume += (vol_target - cur_volume) * 0.002f;
        int idx = (int)phase;
        float frac = phase - idx;
        float a = whoosh[idx % WHOOSH_LEN], b = whoosh[(idx + 1) % WHOOSH_LEN];
        int16_t v = (int16_t)((a + (b - a) * frac) * cur_volume);
        out[i * 2] = out[i * 2 + 1] = v;
        phase += step;
        if (phase >= WHOOSH_LEN)
            phase -= WHOOSH_LEN;
    }
}

void Sound_Init(void)
{
    // Low-passed noise shaped by a smooth envelope, plus a faint bearing hum
    float lp = 0.0f;
    uint32_t seed = 0x1234567;
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
        whoosh[i] = (int16_t)(v * 32767.0f);
    }

    SDL_AudioSpec want = {0}, have;
    want.freq = RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = callback;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev)
        SDL_PauseAudioDevice(dev, 0);
}

void Sound_Update(float rpm, bool enabled)
{
    float arms_per_sec = fabsf(rpm) * 3.0f / 60.0f;
    float volume = 0.0f;
    if (enabled && arms_per_sec > 0.5f)
    {
        float v = fabsf(rpm) / 900.0f;
        if (v > 1.0f) v = 1.0f;
        volume = sqrtf(v) * 150.0f / 255.0f;
    }

    float pitch = arms_per_sec * WHOOSH_LEN;
    if (pitch < 1.0f) pitch = 1.0f;
    if (pitch > 144000.0f) pitch = 144000.0f;

    target_pitch = pitch;
    target_volume = volume;
}

void Sound_Shutdown(void)
{
    if (dev)
        SDL_CloseAudioDevice(dev);
    dev = 0;
}
