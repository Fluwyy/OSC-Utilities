#include "mediaControl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>

static int uinput_fd = -1;
MediaState currentMediaState = MEDIA_STATE_UNKNOWN;

int emit(int fd, int type, int code, int value) {
    struct input_event ie;
    memset(&ie, 0, sizeof(ie));
    ie.type = type;
    ie.code = code;
    ie.value = value;
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;
    return write(fd, &ie, sizeof(ie));
}

int setupUinputDevice(void) {
    if (uinput_fd >= 0) {
        return uinput_fd; 
    }
    
    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd < 0) {
        perror("Failed to open /dev/uinput - you may need to run as root or add user to input group");
        return -1;
    }

    ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(uinput_fd, UI_SET_KEYBIT, KEY_PLAYPAUSE);
    ioctl(uinput_fd, UI_SET_KEYBIT, KEY_PLAY);
    ioctl(uinput_fd, UI_SET_KEYBIT, KEY_PAUSE);
    ioctl(uinput_fd, UI_SET_KEYBIT, KEY_STOPCD);
    ioctl(uinput_fd, UI_SET_KEYBIT, KEY_NEXTSONG);
    ioctl(uinput_fd, UI_SET_KEYBIT, KEY_PREVIOUSSONG);

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "OSC-Media-Control");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x1234;
    uidev.id.product = 0x5678;
    uidev.id.version = 1;

    write(uinput_fd, &uidev, sizeof(uidev));
    ioctl(uinput_fd, UI_DEV_CREATE);


    if (messagePrintingEnabled) {
        printf("Media control device initialized\n");
    }

    return uinput_fd;
}

void cleanupUinputDevice(int fd) {
    if (fd >= 0) {
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
        if (messagePrintingEnabled) {
            printf("Media control device cleaned up\n");
        }
    }
}

void mediaShutdown(void) {
    if (uinput_fd >= 0) {
        cleanupUinputDevice(uinput_fd);
        uinput_fd = -1;
    }
}

int sendMediaKey(int keycode) {
    int fd = setupUinputDevice();
    if (fd < 0) {
        return 0;
    }

    emit(fd, EV_KEY, keycode, 1);
    emit(fd, EV_SYN, SYN_REPORT, 0);
    
    
    emit(fd, EV_KEY, keycode, 0);
    emit(fd, EV_SYN, SYN_REPORT, 0);

    return 1;
}

void mediaStartup(void) {
    if (messagePrintingEnabled) {
        printf("Initializing media control system...\n");
    }
    
    setupUinputDevice();
    
    updateMediaState();
    
    if (messagePrintingEnabled) {
        const char* stateStr;
        switch (currentMediaState) {
            case MEDIA_STATE_PLAYING: stateStr = "PLAYING"; break;
            case MEDIA_STATE_PAUSED: stateStr = "PAUSED"; break;
            case MEDIA_STATE_STOPPED: stateStr = "STOPPED"; break;
            default: stateStr = "UNKNOWN"; break;
        }
        printf("Media startup complete. Current state: %s\n", stateStr);
    }
}

void updateMediaState(void) {
    FILE *fp = popen("playerctl status 2>/dev/null", "r");
    if (fp != NULL) {
        char status[32];
        if (fgets(status, sizeof(status), fp) != NULL) {
            status[strcspn(status, "\n")] = 0;
            
            if (strcmp(status, "Playing") == 0) {
                currentMediaState = MEDIA_STATE_PLAYING;
            } else if (strcmp(status, "Paused") == 0) {
                currentMediaState = MEDIA_STATE_PAUSED;
            } else if (strcmp(status, "Stopped") == 0) {
                currentMediaState = MEDIA_STATE_STOPPED;
            } else {
                currentMediaState = MEDIA_STATE_UNKNOWN;
            }
        } else {
            currentMediaState = MEDIA_STATE_UNKNOWN;
        }
        pclose(fp);
    } else {
        currentMediaState = MEDIA_STATE_UNKNOWN;
    }
}

MediaState getMediaState(void) {
    return currentMediaState;
}

void mediaPlayPause(void) {
    updateMediaState();
    
    if (currentMediaState == MEDIA_STATE_PLAYING) {
        mediaPause();
    } else {
        mediaPlay();
    }
}

void mediaPlay(void) {
    if (sendMediaKey(KEY_PLAY)) {
        if (messagePrintingEnabled) printf("Media: Play (uinput)\n");
        currentMediaState = MEDIA_STATE_PLAYING;
    } else {
        system("playerctl play 2>/dev/null");
        if (messagePrintingEnabled) printf("Media: Play (fallback)\n");
        currentMediaState = MEDIA_STATE_PLAYING;
    }
}

void mediaPause(void) {
    if (sendMediaKey(KEY_PAUSE)) {
        if (messagePrintingEnabled) printf("Media: Pause (uinput)\n");
        currentMediaState = MEDIA_STATE_PAUSED;
    } else {
        system("playerctl pause 2>/dev/null");
        if (messagePrintingEnabled) printf("Media: Pause (fallback)\n");
        currentMediaState = MEDIA_STATE_PAUSED;
    }
}

void mediaStop(void) {
    if (sendMediaKey(KEY_STOPCD)) {
        if (messagePrintingEnabled) printf("Media: Stop (uinput)\n");
        currentMediaState = MEDIA_STATE_STOPPED;
    } else {
        system("playerctl stop 2>/dev/null");
        if (messagePrintingEnabled) printf("Media: Stop (fallback)\n");
        currentMediaState = MEDIA_STATE_STOPPED;
    }
}

void mediaNext(void) {
    if (sendMediaKey(KEY_NEXTSONG)) {
        if (messagePrintingEnabled) printf("Media: Next track (uinput)\n");
    } else {
        system("playerctl next 2>/dev/null");
        if (messagePrintingEnabled) printf("Media: Next track (fallback)\n");
    }
}

void mediaPrevious(void) {
    if (sendMediaKey(KEY_PREVIOUSSONG)) {
        if (messagePrintingEnabled) printf("Media: Previous track (uinput)\n");
    } else {
        system("playerctl previous 2>/dev/null");
        if (messagePrintingEnabled) printf("Media: Previous track (fallback)\n");
    }
}
