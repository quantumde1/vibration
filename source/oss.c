#ifdef __FreeBSD__

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <stdbool.h>
#include <pthread.h>

#include "../include/vibration.h"

#define DEVICE "/dev/dsp"
#define BUFFER_SIZE 4096

#pragma pack(push, 1)
typedef struct {
    char chunkID[4];
    uint32_t chunkSize;
    char format[4];
} RiffHeader;

typedef struct {
    char subchunkID[4];
    uint32_t subchunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} FmtChunk;

typedef struct {
    char subchunkID[4];
    uint32_t subchunkSize;
} DataChunk;
#pragma pack(pop)

static FILE* wavFile = NULL;
static int audioFd = -1;
static bool isPlaying = false;
static bool shouldStop = false;
static pthread_t playThread;
static uint32_t dataSize = 0;
static long dataStartPos = 0;

// Прототипы функций
void* play_thread_func(void* arg);
int configure_audio_device(int fd, FmtChunk* fmt);

int LoadMusic(const char* filename) {
    if (wavFile != NULL) {
        fclose(wavFile);
        wavFile = NULL;
    }
    
    wavFile = fopen(filename, "rb");
    if (!wavFile) {
        perror("error opening file");
        return -1;
    }
    
    RiffHeader riffHeader;
    if (fread(&riffHeader, sizeof(RiffHeader), 1, wavFile) != 1) {
        perror("error reading RIFF header");
        fclose(wavFile);
        wavFile = NULL;
        return -1;
    }
    
    if (memcmp(riffHeader.chunkID, "RIFF", 4) != 0 ||
        memcmp(riffHeader.format, "WAVE", 4) != 0) {
        fprintf(stderr, "not a valid WAV file\n");
        fclose(wavFile);
        wavFile = NULL;
        return -1;
    }
    
    FmtChunk fmtChunk;
    int foundFmt = 0;
    while (!foundFmt) {
        char chunkID[4];
        uint32_t chunkSize;
        
        if (fread(chunkID, 1, 4, wavFile) != 4) {
            fprintf(stderr, "error reading chunk ID\n");
            fclose(wavFile);
            wavFile = NULL;
            return -1;
        }
        
        if (fread(&chunkSize, 4, 1, wavFile) != 1) {
            fprintf(stderr, "error reading chunk size\n");
            fclose(wavFile);
            wavFile = NULL;
            return -1;
        }
        
        chunkSize = le32toh(chunkSize);
        
        if (memcmp(chunkID, "fmt ", 4) == 0) {
            if (fread(&fmtChunk.audioFormat, sizeof(FmtChunk) - 8, 1, wavFile) != 1) {
                fprintf(stderr, "error reading fmt chunk\n");
                fclose(wavFile);
                wavFile = NULL;
                return -1;
            }
            
            foundFmt = 1;
            
            if (chunkSize > sizeof(FmtChunk) - 8) {
                fseek(wavFile, chunkSize - (sizeof(FmtChunk) - 8), SEEK_CUR);
            }
        } else {
            fseek(wavFile, chunkSize, SEEK_CUR);
        }
    }
    
    DataChunk dataChunk;
    int foundData = 0;
    while (!foundData) {
        if (fread(&dataChunk, sizeof(DataChunk), 1, wavFile) != 1) {
            fprintf(stderr, "error reading data chunk header\n");
            fclose(wavFile);
            wavFile = NULL;
            return -1;
        }
        
        dataChunk.subchunkSize = le32toh(dataChunk.subchunkSize);
        
        if (memcmp(dataChunk.subchunkID, "data", 4) == 0) {
            dataSize = dataChunk.subchunkSize;
            dataStartPos = ftell(wavFile);
            foundData = 1;
        } else {
            fseek(wavFile, dataChunk.subchunkSize, SEEK_CUR);
        }
    }
    return 0;
}

int configure_audio_device(int fd, FmtChunk* fmt) {
    int format;
    if (fmt->bitsPerSample == 16) {
        format = AFMT_S16_LE;
    } else if (fmt->bitsPerSample == 8) {
        format = AFMT_U8;
    } else {
        fprintf(stderr, "unsupported bits per sample: %u\n", fmt->bitsPerSample);
        return -1;
    }
    
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) == -1) {
        perror("error setting format");
        return -1;
    }
    
    int channels = fmt->numChannels;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
        perror("error setting channels");
        return -1;
    }
    
    int speed = fmt->sampleRate;
    if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) == -1) {
        perror("error setting sample rate");
        return -1;
    }
    
    return 0;
}

void* play_thread_func(void* arg) {
    if (wavFile == NULL) {
        fprintf(stderr, "No file loaded\n");
        return NULL;
    }
    
    audioFd = open(DEVICE, O_WRONLY);
    if (audioFd == -1) {
        perror("error opening audio device");
        isPlaying = false;
        return NULL;
    }
    
    fseek(wavFile, sizeof(RiffHeader), SEEK_SET);
    
    FmtChunk fmtChunk;
    int foundFmt = 0;
    while (!foundFmt) {
        char chunkID[4];
        uint32_t chunkSize;
        
        fread(chunkID, 1, 4, wavFile);
        fread(&chunkSize, 4, 1, wavFile);
        chunkSize = le32toh(chunkSize);
        
        if (memcmp(chunkID, "fmt ", 4) == 0) {
            fread(&fmtChunk.audioFormat, sizeof(FmtChunk) - 8, 1, wavFile);
            foundFmt = 1;
            
            if (chunkSize > sizeof(FmtChunk) - 8) {
                fseek(wavFile, chunkSize - (sizeof(FmtChunk) - 8), SEEK_CUR);
            }
        } else {
            fseek(wavFile, chunkSize, SEEK_CUR);
        }
    }
    
    fmtChunk.audioFormat = le16toh(fmtChunk.audioFormat);
    fmtChunk.numChannels = le16toh(fmtChunk.numChannels);
    fmtChunk.sampleRate = le32toh(fmtChunk.sampleRate);
    fmtChunk.byteRate = le32toh(fmtChunk.byteRate);
    fmtChunk.blockAlign = le16toh(fmtChunk.blockAlign);
    fmtChunk.bitsPerSample = le16toh(fmtChunk.bitsPerSample);
    
    if (configure_audio_device(audioFd, &fmtChunk) == -1) {
        close(audioFd);
        audioFd = -1;
        isPlaying = false;
        return NULL;
    }

    fseek(wavFile, dataStartPos, SEEK_SET);
    
    unsigned char buffer[BUFFER_SIZE];
    uint32_t totalBytes = 0;
    shouldStop = false;
    
    while (!shouldStop && totalBytes < dataSize) {
        uint32_t toRead = dataSize - totalBytes;
        if (toRead > BUFFER_SIZE) {
            toRead = BUFFER_SIZE;
        }
        
        size_t bytesRead = fread(buffer, 1, toRead, wavFile);
        if (bytesRead == 0) {
            if (feof(wavFile)) {
                break;
            } else {
                perror("error reading audio data");
                break;
            }
        }
        
        ssize_t bytesWritten = write(audioFd, buffer, bytesRead);
        if (bytesWritten == -1) {
            perror("error writing to audio device");
            break;
        }
        
        totalBytes += bytesRead;
    }
    
    if (audioFd != -1) {
        ioctl(audioFd, SNDCTL_DSP_SYNC, NULL);
        close(audioFd);
        audioFd = -1;
    }
    
    isPlaying = false;
    
    return NULL;
}

int PlayMusic() {    
    if (wavFile == NULL) {
        fprintf(stderr, "No music loaded. Call LoadMusic() first.\n");
        return -1;
    }
    
    if (isPlaying) {
        return 0;
    }
    
    isPlaying = true;
    shouldStop = false;
    
    if (pthread_create(&playThread, NULL, play_thread_func, NULL) != 0) {
        perror("error creating play thread");
        isPlaying = false;
        return -1;
    }
    
    pthread_detach(playThread);
    
    return 0;
}

int StopMusic() {
    if (!isPlaying) {
        return 0;
    }
    shouldStop = true;
    
    void* retval;
    pthread_join(playThread, &retval);
    
    if (audioFd != -1) {
        ioctl(audioFd, SNDCTL_DSP_SYNC, NULL);
        close(audioFd);
        audioFd = -1;
    }
    
    isPlaying = false;
    
    return 0;
}

int UnloadMusic() {
    StopMusic();
    
    if (wavFile != NULL) {
        fclose(wavFile);
        wavFile = NULL;
    }
    
    dataSize = 0;
    dataStartPos = 0;
    
    return 0;
}

#endif