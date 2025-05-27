#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "diagtest.h"

int proc_testSpeaker(char *buf) {
    int freq = 1000;
    int volume = 100;

    if(2 == sscanf(buf, "spk %d %d", &freq, &volume))
    {
        printf("Speaker diagnostic test\n");
        printf("freq: %d, volume: %d\n", freq, volume);

        play_sin(freq, volume, 5000);
        return 0;
    }

    return -1;
}

int proc_setVolume(char *buffer) {
    int result = -1;

    if (strlen(buffer) > 7) {
        if (isdigit(buffer[7])) {
            int vol = atoi(buffer + 7);
            if (vol >= 0 && vol <= 100) {
                play_setVolume(vol);
                result = 0;
            }
        }
    }

    return result;
}