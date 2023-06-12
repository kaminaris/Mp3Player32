#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "SdFat.h"

#include <random>

#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioTools.h"

typedef int16_t sound_t;  // sound will be represented as int16_t (with 2 bytes)
AudioInfo info(44100, 2, 16);
SPDIFOutput* out;
StreamCopy* copier;	// copies sound into i2s
EncodedAudioStream* dec;

SdFat SDx;
FsFile audioFile;
auto songList = new std::vector<String>();
auto listenedSongs = new std::vector<String>();

class UniformTrueRandom {
	public:
	using result_type = unsigned int;
	static constexpr result_type min() { return 0; }
	static constexpr result_type max() { return UINT32_MAX; }
	result_type operator()() { return esp_random(); }
};

void play(const char* phrase) {
	Serial.printf("Playing\n");
	audioFile = SDx.open(phrase);
	copier->begin(*dec, audioFile);
}

// Arduino Setup
void setup() {
	// Open Serial
	Serial.begin(115200);
	AudioLogger::instance().begin(Serial, AudioLogger::Info);

	if (!SDx.begin(SdSpiConfig(5, DEDICATED_SPI, SD_SCK_MHZ(10)))) {
		SDx.initErrorHalt(&Serial);
	}

	auto root = SDx.open("/");
	while (true) {
		auto f = root.openNextFile();
		if (!f) {
			// no more files
			break;
		}

		char fileName[256];
		f.getName(fileName, sizeof(fileName));

		auto name = String(fileName);
		if (name.endsWith(".mp3")) {
			songList->push_back(name);
		}
	}

	auto rng = UniformTrueRandom {};
	std::shuffle(songList->begin(), songList->end(), rng);

	for (auto& song : *songList) {
		Serial.println(song);
	}

	// start I2S
	Serial.println("starting SPDIF...");
	out = new SPDIFOutput();
	auto config = out->defaultConfig();
	config.copyFrom(info);
	config.pin_data = 25;
	out->begin(config);

	dec = new EncodedAudioStream(out, new MP3DecoderHelix());
	// dec->setNotifyAudioChange(*out);
	dec->begin();

	copier = new StreamCopy();
	Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() {
	if (copier->copy()) {
	}
	else {
		Serial.println("MP3 next song");
		delay(1000);

		if (songList->empty()) {
			for (auto& song : *listenedSongs) {
				songList->push_back(song);
			}
			listenedSongs->clear();
			auto rng = UniformTrueRandom {};
			std::shuffle(songList->begin(), songList->end(), rng);
		}

		auto fName = songList->back();
		songList->pop_back();
		listenedSongs->push_back(fName);

		play(fName.c_str());
		Serial.printf("Playback of '%s' begins...\n", fName.c_str());

		delay(200);
	}
}