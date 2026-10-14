#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>

void Sound_Init(void);
void Sound_Update(float rpm, bool enabled);
void Sound_Shutdown(void);

#endif
