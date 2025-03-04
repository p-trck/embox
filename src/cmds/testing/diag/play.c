/**
 * @file
 * @brief Simple audio player
 * @author Denis Deryugin <deryugin.denis@gmail.com>
 * @version 0.1
 * @date 2015-11-26
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#include <portaudio.h>
#include <fs/file_format.h>
#include <util/math.h>
#include <framework/mod/options.h>

#define USE_LOCAL_BUFFER false
#define FRAMES_PER_BUFFER 4096

/* Lengths of fields in WAV. */
#define WAV_CHUNK_ID     4
#define WAV_CHUNK_SIZE   4
#define WAV_CHUNK_DESCR  (WAV_CHUNK_ID + WAV_CHUNK_SIZE)

static double _sin(double x) {
	double m = 1.;
	while (x > 2. * 3.14)
		x -= 2. * 3.14;

	if (x > 3.14) {
		x -= 3.14;
		m = -1.;
	}
	return m * (x - x * x * x / 6. + x * x * x * x * x / 120.
		- x * x * x * x * x * x * x / (120 * 6 * 7)
		+ x * x * x * x * x * x * x * x * x/ (120 * 6 * 7 * 8 * 9));
}

static int _sin_w = 100;
static int _sin_h = 10000;
static int sin_callback(const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData) {
	uint16_t *data;

	data = outputBuffer;

	for (int i = 0; i < framesPerBuffer; i++) {
		double x = 2 * 3.14 * (i % _sin_w) / _sin_w;
		*data++ = (uint16_t) ((1. + _sin(x)) * _sin_h); /* Left channel  */
		*data++ = (uint16_t) ((1. + _sin(x)) * _sin_h); /* Right channel */
	}

	return 0;
}

int play_sin(int freq, int volume) {
	int err;
	int chan_n = 2;
	int sample_rate = 44100;
	int bits_per_sample = 16;
	int fdata_len = 0x100000;
	int sleep_msec;
	int devid;

	PaStreamCallback *callback;
	PaStream *stream = NULL;

	struct PaStreamParameters out_par;

	callback = &sin_callback;
	/*
	 * Calculate wavelength (samples per cycle) for sine wave generation.
	 * Wavelength = sample rate / frequency
	 * e.g., if sample_rate = 44100Hz and freq = 440Hz,
	 * one sine wave cycle will take 100.227 samples
	 */
	_sin_w = sample_rate / freq;
	_sin_h = volume;

	/* Initialize PA */
	if (paNoError != (err = Pa_Initialize())) {
		printf("Portaudio error: could not initialize!\n");
		goto err_close_fd;
	}

	if (paNoDevice == (devid = Pa_GetDefaultOutputDevice())) {
		printf("No default output device!\n");
		goto err_terminate_pa;
	}

	out_par = (PaStreamParameters) {
		.device                    = devid,
		.channelCount              = chan_n,
		.sampleFormat              = paInt16,
		.suggestedLatency          = 10,
		.hostApiSpecificStreamInfo = NULL,
	};

	err = Pa_OpenStream(&stream,
			NULL,
			&out_par,
			sample_rate,
			FRAMES_PER_BUFFER,
			0,
			callback,
			NULL);

	if (err != paNoError) {
		printf("Portaudio error: could not open stream!\n");
		goto err_terminate_pa;
	}

	if (paNoError != (err = Pa_StartStream(stream))) {
		printf("Portaudio error: could not start stream!\n");
		goto err_terminate_pa;
	}

	sleep_msec = 1000 * (fdata_len /
			((bits_per_sample / 8) * sample_rate * chan_n));
	if (0 == sleep_msec) {
		sleep_msec = (1000 * fdata_len) /
				((bits_per_sample / 8) * sample_rate * chan_n);
	}

	printf("playing..\n");
	Pa_Sleep(sleep_msec);
	printf("stop\n");

	if (paNoError != (err = Pa_StopStream(stream))) {
		printf("Portaudio error: could not stop stream!\n");
		goto err_terminate_pa;
	}

	if (paNoError != (err = Pa_CloseStream(stream))) {
		printf("Portaudio error: could not close stream!\n");
		goto err_terminate_pa;
	}

err_terminate_pa:
	if (paNoError != (err = Pa_Terminate()))
		printf("Portaudio error: could not terminate!\n");

err_close_fd:
	return 0;
}
