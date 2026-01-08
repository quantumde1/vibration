#ifdef WIN32

#include "windows.h"
#include "mmsystem.h"
#include "../include/vibration.h"

LPCTSTR filename;

int LoadMusic(const char* filePath) {
    filename = TEXT(filePath);
    return 0;
}
int PlayMusic() {
    PlaySound(filename, NULL, SND_ASYNC | SND_LOOP);
    return 0;
}

int StopMusic() {
    PlaySound(NULL, NULL, 0);
    return 0;
}

int UnloadMusic() {
    return 0;
}
#endif