#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include "save.h"

#define SAVE_DIR "sd:/apps/FidgetWii"
#define SAVE_FILE SAVE_DIR "/record.txt"

static bool mounted;

const char *Save_Path(void)
{
    return SAVE_FILE;
}

bool Save_Init(SaveData *data)
{
    memset(data, 0, sizeof(*data));
    data->sound = true;

    mounted = fatMountSimple("sd", &__io_wiisd);
    if (!mounted)
        return false;

    FILE *f = fopen(SAVE_FILE, "r");
    if (!f)
        return true;

    char line[128];
    while (fgets(line, sizeof(line), f))
    {
        char key[64];
        double value;
        if (sscanf(line, "%63[^=]=%lf", key, &value) != 2)
            continue;
        if (!strcmp(key, "best_rpm"))
            data->best_rpm = value;
        else if (!strcmp(key, "longest_spin"))
            data->longest_spin = value;
        else if (!strcmp(key, "total_spins"))
            data->total_spins = value;
        else if (!strcmp(key, "color"))
            data->color = (int)value;
        else if (!strcmp(key, "sound"))
            data->sound = value != 0;
    }
    fclose(f);
    return true;
}

bool Save_Write(const SaveData *data)
{
    if (!mounted)
        return false;

    mkdir("sd:/apps", 0777);
    mkdir(SAVE_DIR, 0777);

    FILE *f = fopen(SAVE_FILE, "w");
    if (!f)
        return false;

    fprintf(f, "# FidgetWii records\n");
    fprintf(f, "best_rpm=%.1f\n", data->best_rpm);
    fprintf(f, "longest_spin=%.2f\n", data->longest_spin);
    fprintf(f, "total_spins=%.0f\n", data->total_spins);
    fprintf(f, "color=%d\n", data->color);
    fprintf(f, "sound=%d\n", data->sound ? 1 : 0);
    bool ok = fclose(f) == 0;
    return ok;
}

void Save_Shutdown(void)
{
    if (mounted)
        fatUnmount("sd:/");
    mounted = false;
}
