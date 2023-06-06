#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "AudioTools.h"

typedef int16_t sound_t;  // sound will be represented as int16_t (with 2 bytes)
AudioInfo info(44100, 2, 16);
SineWaveGenerator<sound_t> sineWave(32000);		// subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<sound_t> sound(sineWave);	// Stream generated from sine wave
SPDIFOutput out;
StreamCopy copier(out, sound);	// copies sound into i2s

// Arduino Setup
void setup() {
	// Open Serial
	Serial.begin(115200);
	AudioLogger::instance().begin(Serial, AudioLogger::Info);

	// start I2S
	Serial.println("starting SPDIF...");
	auto config = out.defaultConfig();
	config.copyFrom(info);
	config.pin_data = 27;
	out.begin(config);

	// Setup sine wave
	sineWave.begin(info, N_B4);
	Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() {
	copier.copy();
}