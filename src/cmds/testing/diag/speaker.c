#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "diagtest.h"

int proc_testSpeaker(char *buf) {
    int freq = 1000;
    int volume = 1000;

    sscanf(buf, "spk %d %d", &freq, &volume);

    printf("Speaker diagnostic test\n");
    printf("freq: %d, volume: %d\n", freq, volume);

    play_sin(freq, volume);

    return 0;
}