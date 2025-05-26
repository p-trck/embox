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
#define AUDIO_BUFFER_SIZE 0x800000

static int sample_rate = 16000;
static int chan_n = 1;

static void write_wave_addr(uint32_t addr, uint8_t *buf, int len) {
	struct wave_header hdr;

	_wave_header_fill(&hdr, chan_n, sample_rate, 16, len);

	/* Since we already have audio in memory just prepend it with header */
	memcpy((void *)addr, &hdr, sizeof(hdr));
}


double calculate_rms(const int16_t *buffer, size_t size) {
	double sum = 0.0;
	for (size_t i = 0; i < size; i++) {
		sum += buffer[i] * buffer[i];
	}
	return sqrt(sum / size);
}

double convert_to_dbfs(double rms, double max_amplitude) {
	double dbfs = (rms > 0) ? 20.0 * log10(rms / max_amplitude) : -INFINITY;
	if (dbfs > 0) {
		dbfs = 0;
	}
	return dbfs;
}

#define AUDIO_ADDR_UNINITIALIZED ((uint32_t)-1)

static uint32_t audio_memory_addr = AUDIO_ADDR_UNINITIALIZED;
static uint16_t *in_buf = NULL;
static int cur_ptr;
static char msg[64];

static int record_callback(const void *inputBuffer, void *outputBuffer,
    unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo,
    PaStreamCallbackFlags statusFlags, void *userData) {
	int i;
	uint16_t *in_data16 = (uint16_t *)inputBuffer;
	double rms, dbfs;
	const double max_amplitude = MAX_AMPLITUDE;

	assert(in_data16 && in_buf);

	for (i = 0; i < framesPerBuffer; i++) {
		if (cur_ptr >= AUDIO_BUFFER_SIZE) {
			break;
		}
		memcpy(&in_buf[cur_ptr], &in_data16[chan_n * i], 2 * chan_n);
		cur_ptr += chan_n;
	}

	// RMS 및 dBFS 계산
	rms = calculate_rms((const int16_t *)in_data16,
	    (size_t)(framesPerBuffer * chan_n));
	dbfs = convert_to_dbfs(rms, max_amplitude);

	snprintf(msg, sizeof(msg), "%d.%02d dBFS", (int)dbfs, abs((int)(dbfs * 100)) % 100);
    send_message(msg);
    //printf("dBFS: %d.%02d\n", (int)dbfs, abs((int)(dbfs * 100)) % 100);
	if (cur_ptr >= AUDIO_BUFFER_SIZE) {
		printf("\n");
		return paComplete;
	}
	return paContinue;
}

// 5초동안 mic입력의 dbfs를 측정한다.

int proc_testMIC_()
{
	int err;
	int devid;
	int sleep_msec = MAX_REC_DURATION;
	PaStream *stream = NULL;

	struct PaStreamParameters in_par;
	PaStreamCallback *callback;

	sample_rate = 16000;
	chan_n = 1;
	callback = &record_callback;
    audio_memory_addr = 0x61000000;
    in_buf = (uint16_t *) (audio_memory_addr + sizeof (struct wave_header));

    cur_ptr = 0;
	/* Initialize PA */
	if (paNoError != (err = Pa_Initialize())) {
		printf("Portaudio error: could not initialize!\n");
		goto err_exit;
	}

	if (paNoDevice == (devid = Pa_GetDefaultInputDevice())) {
		printf("No default input device!\n");
		goto err_terminate_pa;
	}

	in_par = (PaStreamParameters){
	    .device = devid,
	    .channelCount = chan_n,
	    .sampleFormat = paInt16,
	    .suggestedLatency = 10,
	    .hostApiSpecificStreamInfo = NULL,
	};

	err = Pa_OpenStream(&stream, &in_par, NULL, sample_rate, 256 * 1024 / 4, 0,
	    callback, NULL);

	if (err != paNoError) {
		printf("Portaudio error: could not open stream!\n");
		goto err_terminate_pa;
	}

    printf("Recording! Speak to the microphone\n");
	printf("Recording wav parameters:\n");
    printf("  File memory address: 0x%x\n", audio_memory_addr);
	printf("  Duration - %d msec, rate - %d hz, %s wav\n\n", sleep_msec,
	    sample_rate, chan_n == 1 ? "mono" : "stereo");

	if (paNoError != (err = Pa_StartStream(stream))) {
		printf("Portaudio error: could not start stream!\n");
		goto err_terminate_pa;
	}

	//Pa_Sleep(sleep_msec);
    sleep(sleep_msec / 1000);

	if (paNoError != (err = Pa_StopStream(stream))) {
		printf("Portaudio error: could not stop stream!\n");
		goto err_terminate_pa;
	}

	if (paNoError != (err = Pa_CloseStream(stream))) {
		printf("Portaudio error: could not close stream!\n");
		goto err_terminate_pa;
	}

    write_wave_addr(audio_memory_addr, (uint8_t *)in_buf, cur_ptr * 2);

err_terminate_pa:
	if (paNoError != (err = Pa_Terminate()))
		printf("Portaudio error: could not terminate!\n");
err_exit:
	return 0;
}

#include <portaudio.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

typedef struct {
    int currentLevel;
    int peakLevel;
    int sampleRate;
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
        float sample = (float)abs(input[i]) / 32768.0f;
        sum += sample * sample;
    }
    
    // RMS 계산 후 0-100 범위로 변환
    float rmsLevel = sqrt(sum / framesPerBuffer);
    data->currentLevel = (int)(rmsLevel * 1000);
    
    // 피크 레벨 업데이트 (0-1000 범위)
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
        Pa_StartStream(stream);
        
        safe_running_average(-1);  // 초기화
		printf("마이크 레벨 모니터링 중... (Max:1000)\n");
		printf(" val | avrg | peak\n"); 
        for(int i = 0; i < 100; i++) {
            Pa_Sleep(100);
            printf("\r %-4d   %-4d   %-4d", 
                   levelData.currentLevel, (int)safe_running_average(levelData.currentLevel), levelData.peakLevel);
            fflush(stdout);
        }
        printf("\n");
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
    
    Pa_Terminate();
    return 0;
}
