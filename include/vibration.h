#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int LoadMusic(const char* filename);
int PlayMusic(void);
int StopMusic(void);
int UnloadMusic(void);

#ifdef __cplusplus
}
#endif