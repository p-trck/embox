#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <portaudio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fs/file_format.h>
#include <util/math.h>

#include "diagtest.h"

/* Maximum recording duration in milliseconds */
#define MAX_REC_DURATION  10000
#define MAX_AMPLITUDE 32768.0 // 16비트 오디오의 최대 진폭
#define MSG_BUFFER_SIZE 0x100
#define SIZEOF_RXBUFFER 32

double convert_to_dbfs(double rms, double max_amplitude) {
	double dbfs = (rms > 0) ? 20.0 * log10(rms / max_amplitude) : -INFINITY;
	if (dbfs > 0) {
		dbfs = 0;
	}
	return dbfs;
}

typedef struct {
    int currentLevel;
    int peakLevel;
    float dbfs;
} AudioLevelData;

static int levelCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData) {
    
    const int16_t *input = (const int16_t*)inputBuffer;
    AudioLevelData *data = (AudioLevelData*)userData;
    
    float sum = 0.0f;
    
    for(unsigned long i = 0; i < framesPerBuffer; i++) {
        // int16을 -1.0~1.0 범위로 정규화
        float sample = (float)abs(input[i]) / MAX_AMPLITUDE;
        sum += sample * sample;
    }
    
    // RMS 계산 후 0-100 범위로 변환
    float rmsLevel = sqrt(sum / framesPerBuffer);
    data->currentLevel = (int)(rmsLevel * 100);
    //data->dbfs = convert_to_dbfs(rmsLevel, MAX_AMPLITUDE);
    
    // 피크 레벨 업데이트 (0-100 범위)
    if(data->currentLevel > data->peakLevel) {
        data->peakLevel = data->currentLevel;
    }
    
    return paContinue;
}

#include <stdio.h>
#include <limits.h>

float safe_running_average(int value) {
    static long long sum = 0;
    static int count = 0;
    
    // -1이 입력되면 초기화
    if (value == -1) {
        sum = 0;
        count = 0;
        return 0.0f;
    }
    
    // 오버플로우 체크
    if (count >= INT_MAX || 
        (value > 0 && sum > LLONG_MAX - value) ||
        (value < 0 && sum < LLONG_MIN - value)) {
        printf("Warning: Overflow risk detected\n");
        return (float)sum / count;  // 현재 평균 반환
    }
    
    sum += value;
    count++;
    
    return (float)sum / count;
}


int proc_testMIC() {
    PaError err = Pa_Initialize();
    if(err != paNoError) return -1;
    
    AudioLevelData levelData = {0, 0, 8000};
    
    PaStreamParameters inputParams;
    inputParams.device = Pa_GetDefaultInputDevice();
    inputParams.channelCount = 1;  // 모노
    inputParams.sampleFormat = paInt16;  // paInt16으로 변경
    inputParams.suggestedLatency = 0.1;  // 100ms
    inputParams.hostApiSpecificStreamInfo = NULL;
    
    PaStream *stream;
    err = Pa_OpenStream(&stream, &inputParams, NULL, 8000, 256,
                       paClipOff, levelCallback, &levelData);
    
    if(err == paNoError) {
		char *msgbuf = malloc(MSG_BUFFER_SIZE);
		if (!msgbuf) {
			fprintf(stderr, "Memory allocation failed\n");
			Pa_Terminate();
			return -1;
		}
        Pa_StartStream(stream);
        
        safe_running_average(-1);  // 초기화
		printf("마이크 레벨 모니터링 중..\n");
		snprintf(msgbuf, MSG_BUFFER_SIZE, "마이크 레벨 모니터링 중..\n");
		send_message(msgbuf);
        for(;;) {
            char buffer[SIZEOF_RXBUFFER];
            int received_bytes;
            int lv;
            extern int recv_message(char *message, int len, int waitmsec);

            #if 0
            lv = (int)levelData.currentLevel - 36;
            lv = (lv < 0) ? 0 : lv;
            #else
            lv = (int)levelData.currentLevel;
            #endif
            snprintf(msgbuf, MSG_BUFFER_SIZE, " %-4d RMS", lv);
			send_message(msgbuf);
            received_bytes = recv_message(buffer, SIZEOF_RXBUFFER, 1000);
            if (received_bytes > 0) {
                printf("Received message: %s\n", buffer);
                if (strcmp(buffer, "stop") == 0) {
                    break;
                }
            }
        }

		Pa_StopStream(stream);
        Pa_CloseStream(stream);
		free(msgbuf);
	}
    
	Pa_Terminate();
	sleep(1);

	return 0;
}
