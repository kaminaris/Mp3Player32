#include "Spdif.h"

#define SPDIF_DATA_PIN 27

#define I2S_NUM I2S_NUM_0

#define I2S_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT
#define I2S_CHANNELS 2
#define BMC_BITS_PER_SAMPLE 64
#define BMC_BITS_FACTOR (BMC_BITS_PER_SAMPLE / I2S_BITS_PER_SAMPLE)
#define SPDIF_BLOCK_SAMPLES 192
#define SPDIF_BUF_DIV 2	 // double buffering
#define DMA_BUF_COUNT 2
#define DMA_BUF_LEN (SPDIF_BLOCK_SAMPLES * BMC_BITS_PER_SAMPLE / I2S_BITS_PER_SAMPLE / SPDIF_BUF_DIV)
#define I2S_BUG_MAGIC (26 * 1000 * 1000)  // magic number for avoiding I2S bug
#define SPDIF_BLOCK_SIZE (SPDIF_BLOCK_SAMPLES * (BMC_BITS_PER_SAMPLE / 8) * I2S_CHANNELS)
#define SPDIF_BUF_SIZE (SPDIF_BLOCK_SIZE / SPDIF_BUF_DIV)
#define SPDIF_BUF_ARRAY_SIZE (SPDIF_BUF_SIZE / sizeof(uint32_t))

static uint32_t spdif_buf[SPDIF_BUF_ARRAY_SIZE];
static uint32_t* spdif_ptr;

/*
 * 8bit PCM to 16bit BMC conversion table, LSb first, 1 end
 */
static const uint16_t bmc_tab[256] = {
	0x3333, 0xb333, 0xd333, 0x5333, 0xcb33, 0x4b33, 0x2b33, 0xab33, 0xcd33, 0x4d33, 0x2d33, 0xad33, 0x3533, 0xb533,
	0xd533, 0x5533, 0xccb3, 0x4cb3, 0x2cb3, 0xacb3, 0x34b3, 0xb4b3, 0xd4b3, 0x54b3, 0x32b3, 0xb2b3, 0xd2b3, 0x52b3,
	0xcab3, 0x4ab3, 0x2ab3, 0xaab3, 0xccd3, 0x4cd3, 0x2cd3, 0xacd3, 0x34d3, 0xb4d3, 0xd4d3, 0x54d3, 0x32d3, 0xb2d3,
	0xd2d3, 0x52d3, 0xcad3, 0x4ad3, 0x2ad3, 0xaad3, 0x3353, 0xb353, 0xd353, 0x5353, 0xcb53, 0x4b53, 0x2b53, 0xab53,
	0xcd53, 0x4d53, 0x2d53, 0xad53, 0x3553, 0xb553, 0xd553, 0x5553, 0xcccb, 0x4ccb, 0x2ccb, 0xaccb, 0x34cb, 0xb4cb,
	0xd4cb, 0x54cb, 0x32cb, 0xb2cb, 0xd2cb, 0x52cb, 0xcacb, 0x4acb, 0x2acb, 0xaacb, 0x334b, 0xb34b, 0xd34b, 0x534b,
	0xcb4b, 0x4b4b, 0x2b4b, 0xab4b, 0xcd4b, 0x4d4b, 0x2d4b, 0xad4b, 0x354b, 0xb54b, 0xd54b, 0x554b, 0x332b, 0xb32b,
	0xd32b, 0x532b, 0xcb2b, 0x4b2b, 0x2b2b, 0xab2b, 0xcd2b, 0x4d2b, 0x2d2b, 0xad2b, 0x352b, 0xb52b, 0xd52b, 0x552b,
	0xccab, 0x4cab, 0x2cab, 0xacab, 0x34ab, 0xb4ab, 0xd4ab, 0x54ab, 0x32ab, 0xb2ab, 0xd2ab, 0x52ab, 0xcaab, 0x4aab,
	0x2aab, 0xaaab, 0xcccd, 0x4ccd, 0x2ccd, 0xaccd, 0x34cd, 0xb4cd, 0xd4cd, 0x54cd, 0x32cd, 0xb2cd, 0xd2cd, 0x52cd,
	0xcacd, 0x4acd, 0x2acd, 0xaacd, 0x334d, 0xb34d, 0xd34d, 0x534d, 0xcb4d, 0x4b4d, 0x2b4d, 0xab4d, 0xcd4d, 0x4d4d,
	0x2d4d, 0xad4d, 0x354d, 0xb54d, 0xd54d, 0x554d, 0x332d, 0xb32d, 0xd32d, 0x532d, 0xcb2d, 0x4b2d, 0x2b2d, 0xab2d,
	0xcd2d, 0x4d2d, 0x2d2d, 0xad2d, 0x352d, 0xb52d, 0xd52d, 0x552d, 0xccad, 0x4cad, 0x2cad, 0xacad, 0x34ad, 0xb4ad,
	0xd4ad, 0x54ad, 0x32ad, 0xb2ad, 0xd2ad, 0x52ad, 0xcaad, 0x4aad, 0x2aad, 0xaaad, 0x3335, 0xb335, 0xd335, 0x5335,
	0xcb35, 0x4b35, 0x2b35, 0xab35, 0xcd35, 0x4d35, 0x2d35, 0xad35, 0x3535, 0xb535, 0xd535, 0x5535, 0xccb5, 0x4cb5,
	0x2cb5, 0xacb5, 0x34b5, 0xb4b5, 0xd4b5, 0x54b5, 0x32b5, 0xb2b5, 0xd2b5, 0x52b5, 0xcab5, 0x4ab5, 0x2ab5, 0xaab5,
	0xccd5, 0x4cd5, 0x2cd5, 0xacd5, 0x34d5, 0xb4d5, 0xd4d5, 0x54d5, 0x32d5, 0xb2d5, 0xd2d5, 0x52d5, 0xcad5, 0x4ad5,
	0x2ad5, 0xaad5, 0x3355, 0xb355, 0xd355, 0x5355, 0xcb55, 0x4b55, 0x2b55, 0xab55, 0xcd55, 0x4d55, 0x2d55, 0xad55,
	0x3555, 0xb555, 0xd555, 0x5555,
};

// BMC preamble
#define BMC_B 0x33173333  // block start
#define BMC_M 0x331d3333  // left ch
#define BMC_W 0x331b3333  // right ch
#define BMC_MW_DIF (BMC_M ^ BMC_W)
#define SYNC_OFFSET 2  // byte offset of SYNC
#define SYNC_FLIP ((BMC_B ^ BMC_M) >> (SYNC_OFFSET * 8))

// initialize S/PDIF buffer
static void spdif_buf_init() {
	int i;
	uint32_t bmc_mw = BMC_W;

	for (i = 0; i < SPDIF_BUF_ARRAY_SIZE; i += 2) {
		spdif_buf[i] = bmc_mw ^= BMC_MW_DIF;
	}
}

void Spdif::init(int rate) {
	uint32_t sample_rate = rate * BMC_BITS_FACTOR;
	int bclk = sample_rate * I2S_BITS_PER_SAMPLE * I2S_CHANNELS;
	int mclk = (I2S_BUG_MAGIC / bclk) * bclk;  // use mclk for avoiding I2S bug
	i2s_config_t i2s_config = {
		.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
		.sample_rate = sample_rate,
		.bits_per_sample = I2S_BITS_PER_SAMPLE,
		.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
		.communication_format = I2S_COMM_FORMAT_STAND_I2S,
		.intr_alloc_flags = 0,
		.dma_buf_count = DMA_BUF_COUNT,
		.dma_buf_len = DMA_BUF_LEN,
		.use_apll = true,
		.tx_desc_auto_clear = true,
		.fixed_mclk = mclk,	 // avoiding I2S bug
	};
	Serial.printf(" MCLK %d\n", mclk);
	Serial.printf("  sample_rate %u\n", sample_rate);

	i2s_pin_config_t pin_config = {
		.bck_io_num = -1,
		.ws_io_num = -1,
		.data_out_num = SPDIF_DATA_PIN,
		.data_in_num = -1,
	};

	ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM, &i2s_config, 0, nullptr));
	ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM, &pin_config));

	// initialize S/PDIF buffer
	spdif_buf_init();
	spdif_ptr = spdif_buf;
}

size_t Spdif::write(const void* src, size_t size) {
	const uint8_t* p = (uint8_t*)src;
	size_t result = 0;
	while (p < (uint8_t*)src + size) {
		// convert PCM 16bit data to BMC 32bit pulse pattern
		*(spdif_ptr + 1) = (uint32_t)(((bmc_tab[*p] << 16) ^ bmc_tab[*(p + 1)]) << 1) >> 1;

		p += 2;
		spdif_ptr += 2;	 // advance to next audio data

		if (spdif_ptr >= &spdif_buf[SPDIF_BUF_ARRAY_SIZE]) {
			size_t i2s_write_len;

			// set block start preamble
			((uint8_t*)spdif_buf)[SYNC_OFFSET] ^= SYNC_FLIP;

			i2s_write(I2S_NUM, spdif_buf, sizeof(spdif_buf), &i2s_write_len, portMAX_DELAY);
			result += i2s_write_len;
			spdif_ptr = spdif_buf;
		}
	}

	return result;
}
#define LEFTCHANNEL 0
#define RIGHTCHANNEL 1

static const uint16_t spdif_bmclookup[256] PROGMEM = {
	0xcccc, 0x4ccc, 0x2ccc, 0xaccc, 0x34cc, 0xb4cc, 0xd4cc, 0x54cc, 0x32cc, 0xb2cc, 0xd2cc, 0x52cc, 0xcacc, 0x4acc,
	0x2acc, 0xaacc, 0x334c, 0xb34c, 0xd34c, 0x534c, 0xcb4c, 0x4b4c, 0x2b4c, 0xab4c, 0xcd4c, 0x4d4c, 0x2d4c, 0xad4c,
	0x354c, 0xb54c, 0xd54c, 0x554c, 0x332c, 0xb32c, 0xd32c, 0x532c, 0xcb2c, 0x4b2c, 0x2b2c, 0xab2c, 0xcd2c, 0x4d2c,
	0x2d2c, 0xad2c, 0x352c, 0xb52c, 0xd52c, 0x552c, 0xccac, 0x4cac, 0x2cac, 0xacac, 0x34ac, 0xb4ac, 0xd4ac, 0x54ac,
	0x32ac, 0xb2ac, 0xd2ac, 0x52ac, 0xcaac, 0x4aac, 0x2aac, 0xaaac, 0x3334, 0xb334, 0xd334, 0x5334, 0xcb34, 0x4b34,
	0x2b34, 0xab34, 0xcd34, 0x4d34, 0x2d34, 0xad34, 0x3534, 0xb534, 0xd534, 0x5534, 0xccb4, 0x4cb4, 0x2cb4, 0xacb4,
	0x34b4, 0xb4b4, 0xd4b4, 0x54b4, 0x32b4, 0xb2b4, 0xd2b4, 0x52b4, 0xcab4, 0x4ab4, 0x2ab4, 0xaab4, 0xccd4, 0x4cd4,
	0x2cd4, 0xacd4, 0x34d4, 0xb4d4, 0xd4d4, 0x54d4, 0x32d4, 0xb2d4, 0xd2d4, 0x52d4, 0xcad4, 0x4ad4, 0x2ad4, 0xaad4,
	0x3354, 0xb354, 0xd354, 0x5354, 0xcb54, 0x4b54, 0x2b54, 0xab54, 0xcd54, 0x4d54, 0x2d54, 0xad54, 0x3554, 0xb554,
	0xd554, 0x5554, 0x3332, 0xb332, 0xd332, 0x5332, 0xcb32, 0x4b32, 0x2b32, 0xab32, 0xcd32, 0x4d32, 0x2d32, 0xad32,
	0x3532, 0xb532, 0xd532, 0x5532, 0xccb2, 0x4cb2, 0x2cb2, 0xacb2, 0x34b2, 0xb4b2, 0xd4b2, 0x54b2, 0x32b2, 0xb2b2,
	0xd2b2, 0x52b2, 0xcab2, 0x4ab2, 0x2ab2, 0xaab2, 0xccd2, 0x4cd2, 0x2cd2, 0xacd2, 0x34d2, 0xb4d2, 0xd4d2, 0x54d2,
	0x32d2, 0xb2d2, 0xd2d2, 0x52d2, 0xcad2, 0x4ad2, 0x2ad2, 0xaad2, 0x3352, 0xb352, 0xd352, 0x5352, 0xcb52, 0x4b52,
	0x2b52, 0xab52, 0xcd52, 0x4d52, 0x2d52, 0xad52, 0x3552, 0xb552, 0xd552, 0x5552, 0xccca, 0x4cca, 0x2cca, 0xacca,
	0x34ca, 0xb4ca, 0xd4ca, 0x54ca, 0x32ca, 0xb2ca, 0xd2ca, 0x52ca, 0xcaca, 0x4aca, 0x2aca, 0xaaca, 0x334a, 0xb34a,
	0xd34a, 0x534a, 0xcb4a, 0x4b4a, 0x2b4a, 0xab4a, 0xcd4a, 0x4d4a, 0x2d4a, 0xad4a, 0x354a, 0xb54a, 0xd54a, 0x554a,
	0x332a, 0xb32a, 0xd32a, 0x532a, 0xcb2a, 0x4b2a, 0x2b2a, 0xab2a, 0xcd2a, 0x4d2a, 0x2d2a, 0xad2a, 0x352a, 0xb52a,
	0xd52a, 0x552a, 0xccaa, 0x4caa, 0x2caa, 0xacaa, 0x34aa, 0xb4aa, 0xd4aa, 0x54aa, 0x32aa, 0xb2aa, 0xd2aa, 0x52aa,
	0xcaaa, 0x4aaa, 0x2aaa, 0xaaaa};
const uint32_t VUCP_PREAMBLE_B = 0xCCE80000;  // 11001100 11101000
const uint32_t VUCP_PREAMBLE_M = 0xCCE20000;  // 11001100 11100010
const uint32_t VUCP_PREAMBLE_W = 0xCCE40000;  // 11001100 11100100
uint8_t frame_num = 0;
uint8_t gainF2P6 = (uint8_t)(1.0 * (1 << 6));
inline int16_t Amplify(int16_t s) {
	int32_t v = (s * gainF2P6) >> 6;
	if (v < -32767)
		return -32767;
	else if (v > 32767)
		return 32767;
	else
		return (int16_t)(v & 0xffff);
}

bool SetGain(float f) {
	if (f > 4.0) f = 4.0;
	if (f < 0.0) f = 0.0;
	gainF2P6 = (uint8_t)(f * (1 << 6));
	return true;
}

bool Spdif::WriteSamples(int16_t* samples, size_t samplesNumber) {
	int16_t sample[2] = {0};
	for (int i = 0; i < samplesNumber; i += 2) {
		sample[0] = samples[i];
		sample[1] = samples[i + 1];
		// Serial.printf("L: %d R: %d\n", sample[0], sample[1]);
		if (!ConsumeSample(sample)) {
			// Serial.printf("FAIL L: %d R: %d\n", sample[0], sample[1]);
		}
	}
	return true;
}

bool Spdif::ConsumeSample(int16_t sample[2]) {
	int16_t ms[2];
	uint16_t hi, lo, aux;
	uint32_t buf[4];

	ms[0] = sample[0];
	ms[1] = sample[1];
	// MakeSampleStereo16(ms);

	// S/PDIF encoding:
	//   http://www.hardwarebook.info/S/PDIF
	// Original sources: Teensy Audio Library
	//   https://github.com/PaulStoffregen/Audio/blob/master/output_spdif2.cpp
	//
	// Order of bits, before BMC encoding, from the definition of SPDIF format
	//   PPPP AAAA  SSSS SSSS  SSSS SSSS  SSSS VUCP
	// are sent rearanged as
	//   VUCP PPPP  AAAA 0000  SSSS SSSS  SSSS SSSS
	// This requires a bit less shifting as 16 sample bits align and can be
	// BMC encoded with two table lookups (and at the same time flipped to LSB first).
	// There is no separate word-clock, so hopefully the receiver won't notice.

	uint16_t sample_left = Amplify(ms[LEFTCHANNEL]) / 16;
	// BMC encode and flip left channel bits
	hi = pgm_read_word(&spdif_bmclookup[(uint8_t)(sample_left >> 8)]);
	lo = pgm_read_word(&spdif_bmclookup[(uint8_t)sample_left]);
	// Low word is inverted depending on first bit of high word
	lo ^= (~((int16_t)hi) >> 16);
	buf[0] = ((uint32_t)lo << 16) | hi;
	// Fixed 4 bits auxillary-audio-databits, the first used as parity
	// Depending on first bit of low word, invert the bits
	aux = 0xb333 ^ (((uint32_t)((int16_t)lo)) >> 17);
	// Send 'B' preamble only for the first frame of data-block
	if (frame_num == 0) {
		buf[1] = VUCP_PREAMBLE_B | aux;
	}
	else {
		buf[1] = VUCP_PREAMBLE_M | aux;
	}

	uint16_t sample_right = Amplify(ms[RIGHTCHANNEL]) / 16;
	// BMC encode right channel, similar as above
	hi = pgm_read_word(&spdif_bmclookup[(uint8_t)(sample_right >> 8)]);
	lo = pgm_read_word(&spdif_bmclookup[(uint8_t)sample_right]);
	lo ^= (~((int16_t)hi) >> 16);
	buf[2] = ((uint32_t)lo << 16) | hi;
	aux = 0xb333 ^ (((uint32_t)((int16_t)lo)) >> 17);
	buf[3] = VUCP_PREAMBLE_W | aux;
	// Serial.printf("l: %d, r: %d\n", sample_left, sample_right);
#if defined(ESP32)
	// Assume DMA buffers are multiples of 16 bytes. Either we write all bytes or none.
	size_t bytes_written;
	// Serial.printf("ret: %d, %d, %d, %d\n", buf[0], buf[1], buf[2], buf[3]);
	esp_err_t ret = i2s_write(I2S_NUM, (const char*)&buf, 8 * channels, &bytes_written, portMAX_DELAY);
	// If we didn't write all bytes, return false early and do not increment frame_num
	if ((ret != ESP_OK) || (bytes_written != (8 * channels))) {
		Serial.printf("ret: %d, bw: %d\n", ret, bytes_written);
		return false;
	}
#elif defined(ESP8266)
	if (!I2SDriver.writeInterleaved(buf)) return false;
#endif
	// Increment and rotate frame number
	if (++frame_num > 191) frame_num = 0;
	return true;
}
//
size_t Spdif::writePcm2(const uint8_t* src, size_t size) {
	const uint8_t* p = src;
	size_t result = 0;

	while (p < (uint8_t*)src + size) {
		// convert PCM 16bit data to BMC 32bit pulse pattern
		if (channels == 2) {
			*(spdif_ptr + 1) = (uint32_t)(((bmc_tab[*p] << 16) ^ bmc_tab[*(p + 1)]) << 1) >> 1;
			p += 2;
		}
		else {
			// must be one channels -> use the same value for both
			*(spdif_ptr + 1) = (uint32_t)(((bmc_tab[*p] << 16) ^ bmc_tab[*(p)]) << 1) >> 1;
			p++;
		}
		result += 2;
		spdif_ptr += 2;	 // advance to next audio data

		if (spdif_ptr >= &spdif_buf[SPDIF_BUF_ARRAY_SIZE]) {
			// set block start preamble
			((uint8_t*)spdif_buf)[SYNC_OFFSET] ^= SYNC_FLIP;
			size_t resultx = 0;
			// i2s.write((uint8_t *)spdif_buf, sizeof(spdif_buf));
			// i2s_write((uint8_t *)spdif_buf, sizeof(spdif_buf));
			i2s_write(I2S_NUM, spdif_buf, sizeof(spdif_buf), &resultx, portMAX_DELAY);
			spdif_ptr = spdif_buf;
		}
	}

	return result;
}

void Spdif::writePcm(const int16_t* samples, int frames) {
	// this will contain the prepared samples for sending to the I2S device
	int frame_index = 0;
	while (frame_index < frames) {
		// fill up the frames buffer with the next NUM_FRAMES_TO_SEND frames
		int frames_to_send = 0;
		for (int i = 0; i < NUM_FRAMES_TO_SEND && frame_index < frames; i++) {
			// int16_t left_sample = process_sample(volume * float(samples[frame_index * 2]));
			// int16_t right_sample = process_sample(volume * float(samples[frame_index * 2 + 1]));
			// frames_buffer[i * 2] = left_sample;
			// frames_buffer[i * 2 + 1] = right_sample;

			frames_buffer[i * 2] = samples[frame_index * 2];
			frames_buffer[i * 2 + 1] = samples[frame_index * 2 + 1];
			frames_to_send++;
			frame_index++;
		}
		// write data to the i2s peripheral - this will block until the data is sent
		// i2s_write(I2S_NUM, frames_buffer, frames_to_send * sizeof(int16_t) * 2, &bytes_written, portMAX_DELAY);
		size_t bytes_written = write(frames_buffer, frames_to_send * sizeof(int16_t) * 2);
		if (bytes_written != frames_to_send * sizeof(int16_t) * 2) {
			// Serial.printf("Did not write all bytes %d %d \n", bytes_written, frames_to_send * sizeof(int16_t) * 2);
		}
	}
};

void Spdif::setSampleRates(int rate) {
	// uninstall and reinstall I2S driver for avoiding I2S bug
	i2s_driver_uninstall(I2S_NUM);
	init(rate);
}