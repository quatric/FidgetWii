#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "save.h"

#if defined(PLATFORM_WII)
#include <fat.h>
#include <sdcard/wiisd_io.h>
#define SAVE_ROOT "sd:"
#define SAVE_DIR_1 "sd:/apps"
#define SAVE_DIR_2 "sd:/apps/FidgetWii"
#elif defined(PLATFORM_WIIU)
#include <whb/sdcard.h>
#define SAVE_REL 1
#elif defined(PLATFORM_SWITCH)
#define SAVE_DIR_1 "sdmc:/switch"
#define SAVE_DIR_2 "sdmc:/switch/FidgetSwitch"
#elif defined(PLATFORM_3DS)
#define SAVE_DIR_1 "sdmc:/3ds"
#define SAVE_DIR_2 "sdmc:/3ds/Fidget3DS"
#elif defined(PLATFORM_NDS)
#include <fat.h>
#define SAVE_DIR_1 "fat:/_nds"
#define SAVE_DIR_2 "fat:/_nds/FidgetDS"
#endif

#ifdef SAVE_REL
// Wii U: the SD card mount path is only known at run time.
static char save_root[160], save_dir[256], save_file[320];
#define SAVE_FILE save_file
#else
#define SAVE_FILE SAVE_DIR_2 "/record.txt"
#endif

static bool mounted;

const char *Save_Path(void)
{
    return SAVE_FILE;
}

static bool mount_storage(void)
{
#if defined(PLATFORM_WII)
    return fatMountSimple("sd", &__io_wiisd);
#elif defined(PLATFORM_WIIU)
    if (!WHBMountSdCard())
        return false;
    const char *mount = WHBGetSdCardMountPath();
    snprintf(save_root, sizeof(save_root), "%s%s", mount[0] == '/' ? "fs:" : "", mount);
    snprintf(save_dir, sizeof(save_dir), "%s/wiiu/apps/FidgetWiiU", save_root);
    snprintf(save_file, sizeof(save_file), "%s/record.txt", save_dir);
    return true;
#elif defined(PLATFORM_NDS)
    return fatInitDefault();
#else
    return true; // sdmc: is mounted by the system libraries
#endif
}

static void make_dirs(void)
{
#if defined(PLATFORM_WIIU)
    char path[300];
    snprintf(path, sizeof(path), "%s/wiiu", save_root);
    mkdir(path, 0777);
    snprintf(path, sizeof(path), "%s/wiiu/apps", save_root);
    mkdir(path, 0777);
    mkdir(save_dir, 0777);
#else
    mkdir(SAVE_DIR_1, 0777);
    mkdir(SAVE_DIR_2, 0777);
#endif
}

bool Save_Init(SaveData *data)
{
    memset(data, 0, sizeof(*data));
    data->sound = true;

    mounted = mount_storage();
    if (!mounted)
        return false;

    FILE *f = fopen(SAVE_FILE, "r");
    if (!f)
    {
        // Nothing saved yet: make sure the card is writable at all.
        make_dirs();
        return true;
    }

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

    make_dirs();

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
    {
#if defined(PLATFORM_WII)
        fatUnmount("sd:/");
#elif defined(PLATFORM_WIIU)
        WHBUnmountSdCard();
#endif
    }
    mounted = false;
}
