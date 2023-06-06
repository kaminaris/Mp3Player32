#ifndef MP3PLAYER_CUSTOMPRINT_H
#define MP3PLAYER_CUSTOMPRINT_H

#include <Arduino.h>

#include "Spdif.h"

class CustomPrint : public Print {
	Spdif* spdif;

	public:
	CustomPrint() {
		spdif = new Spdif();
		spdif->init(44100);
	};

	size_t write(int16_t* buffer, int size) {
		// Serial.printf("trying to write mp3 %d\n", size);
		spdif->WriteSamples(buffer, size);
		return size;
	};

	// size_t write2(int16_t* buffer, int size) {
	// 	Serial.printf("trying to write mp3 %d\n", size);
	// 	spdif->write(buffer, size);
	// 	return size;
	// };

	size_t write(uint8_t ch) override {
		Serial.printf("FAILLLLLLLL to write mp3 single %d\n", 1);
		return 1;
	};
};

#endif
