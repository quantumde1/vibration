#ifdef _WIN32
#include "../include/vibration.h"
#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>

static char* gFilename = NULL;

int LoadMusic(const char* filePath) {
    if (gFilename) {
        free(gFilename);
    }
    
    gFilename = _strdup(filePath);
    return (gFilename != NULL) ? 0 : -1;
}

int PlayMusic(void) {
    if (gFilename) {
        return PlaySoundA(gFilename, NULL, SND_ASYNC | SND_LOOP) ? 0 : -1;
    }
    return -1;
}

int StopMusic(void) {
    PlaySoundA(NULL, NULL, 0);
    return 0;
}

int UnloadMusic(void) {
    if (gFilename) {
        free(gFilename);
        gFilename = NULL;
    }
    return 0;
}
#endif