#ifndef MP3PLAYER_SPDIF_H
#define MP3PLAYER_SPDIF_H

#include <Arduino.h>
#include <FS.h>
#include <driver/i2s.h>
const int NUM_FRAMES_TO_SEND = 256;

class Spdif {
	public:
	uint8_t channels = 2;
	float volume = 1.0f;
	int16_t *frames_buffer;

	Spdif() {
		frames_buffer = (int16_t *)malloc(2 * sizeof(int16_t) * NUM_FRAMES_TO_SEND);
	}
	/*
	 * initialize S/PDIF driver
	 *   rate: sampling rate, 44100Hz, 48000Hz etc.
	 */
	void init(int rate);

	/*
	 * send PCM data to S/PDIF transmitter
	 *   src: pointer to 16bit PCM stereo data
	 *   size: number of data bytes
	 */
	size_t write(const void* src, size_t size);

	/*
	 * change sampling rate
	 *   rate: sampling rate, 44100Hz, 48000Hz etc.
	 */
	void setSampleRates(int rate);
	void writePcm(const int16_t* src, int frames);
	virtual int16_t process_sample(int16_t sample) { return sample; }
	size_t writePcm2(const uint8_t* src, size_t size);
	size_t writeTest(const int16_t* src, size_t size);
	bool ConsumeSample(int16_t* sample);
	bool WriteSamples(int16_t* samples, size_t samplesNumber);
};

#endif
