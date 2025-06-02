#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "diagtest.h"

int proc_outputSource(char *buf) {
    while(1) {
        char cmd[32];
        int f, v;

        if (fgets(cmd, sizeof(cmd), stdin) != NULL) {
            if (sscanf(cmd, "spk %d %d", &f, &v) == 2) {
                if (f >= 500 && f <= 2000 && v >= 0 && v <= 100) {
                    printf("Output source diagnostic test\n");
                    printf("freq: %d, volume: %d\n", f, v);

                    play_sin(f, v, 3000);
                    usleep(500000);
                }
            }
        }
    }

    return -1;
}
