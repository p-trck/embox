/**
 * @file memtest.c
 * @brief Simple utility to measure memory performance
 * @author Denis Deryugin <deryugin.denis@gmail.com>
 * @version
 * @date 21.03.2019
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#include <framework/mod/options.h>

static void print_help(char **argv) {
	assert(argv);
	assert(argv[0]);
	printf("Usage: %s [-nh] [-l LENGHT]\n", argv[0]);
	printf("\tOptions: -n Test non-cached memory\n");
	printf("\t         -l LENGTH Number of bytes to be operated "
			"(default 0x1000000 for memcpy() and 0x2000 for heap)\n");
	printf("\t         -r REPEAT Number of repeats (default 64 for memcpy() and 2048 for heap)\n");
	printf("\t         -p Run test for heap (a.k.a. pyramid)\n");
	printf("\t         -q Maximum number of heap regions\n");
	printf("\t         -s Random seed\n");
}

#define STT_FILE  OPTION_STRING_GET(diagstatus)

int main(int argc, char **argv) {
	int opt;

	while (-1 != (opt = getopt(argc, argv, "ph"))) {
		switch (opt) {
			case 'p': {
				FILE *sttFile;
				sttFile = fopen(STT_FILE, "w+");
				if (sttFile == NULL) {
					fprintf(stderr, "Error: Unable to open file %s\n", STT_FILE);
					return -1;
				}
				printf("Hello world\n");
				if (fprintf(sttFile, "Hello world\n") < 0) {
					fprintf(stderr, "Error: Unable to write to file %s\n", STT_FILE);
					fclose(sttFile);
					return -1;
				}
				fclose(sttFile);
            }
			break;
			case 'h':
				print_help(argv);
				return 0;
			default:
				printf("default\n");
				break;
		}
	}

	return 0;
}
