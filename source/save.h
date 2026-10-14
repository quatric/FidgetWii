#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>

typedef struct
{
    float best_rpm;        // fastest speed ever reached
    float longest_spin;    // longest single spin, in seconds
    double total_spins;    // lifetime revolutions
    int color;             // selected colour index
    bool sound;            // spinning sound enabled
} SaveData;

// Mounts the SD card and loads the record file. Returns false if no SD card.
bool Save_Init(SaveData *data);
bool Save_Write(const SaveData *data);
const char *Save_Path(void);
void Save_Shutdown(void);

#endif
