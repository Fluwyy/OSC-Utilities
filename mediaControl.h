#ifndef MEDIA_CONTROL_H
#define MEDIA_CONTROL_H

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>

extern int messagePrintingEnabled;

typedef enum {
    MEDIA_STATE_UNKNOWN,
    MEDIA_STATE_PLAYING,
    MEDIA_STATE_PAUSED,
    MEDIA_STATE_STOPPED
} MediaState;

extern MediaState currentMediaState;

void mediaStartup(void);
void mediaShutdown(void);  
void mediaPlayPause(void);
void mediaPlay(void);
void mediaPause(void);
void mediaStop(void);
void mediaNext(void);
void mediaPrevious(void);
MediaState getMediaState(void);
void updateMediaState(void);

int emit(int fd, int type, int code, int value);
int setupUinputDevice(void);
void cleanupUinputDevice(int fd);
int sendMediaKey(int keycode);

#endif
