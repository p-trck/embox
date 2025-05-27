#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "diagtest.h"

int proc_outputSource(char *buf) {
    int freq[] = {500, 1000, 2000};
    int volume = 100;

    while(1) {
        for(int i = 0; i < sizeof(freq) / sizeof(freq[0]); i++) {
            printf("Output source diagnostic test\n");
            printf("freq: %d, volume: %d\n", freq[i], volume);
            play_sin(freq[i], volume, 3000);
        }

        usleep(500000);
    }

    return -1;
}
