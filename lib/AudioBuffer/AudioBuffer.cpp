#include "AudioBuffer.h"

AudioBuffer::AudioBuffer(size_t maxBlockSize) {
	// if maxBlockSize isn't set use defaultspace (1600 bytes) is enough for aac and mp3 player
	if (maxBlockSize) m_resBuffSizeRAM = maxBlockSize;
	if (maxBlockSize) m_maxBlockSize = maxBlockSize;
}

AudioBuffer::~AudioBuffer() {
	if (m_buffer) free(m_buffer);
	m_buffer = nullptr;
}

void AudioBuffer::setBufsize(int ram, int psram) {
	if (ram > -1)  // -1 == default / no change
		m_buffSizeRAM = ram;
	if (psram > -1) m_buffSizePSRAM = psram;
}

size_t AudioBuffer::init() {
	if (m_buffer) free(m_buffer);
	m_buffer = nullptr;

	if (psramInit() && m_buffSizePSRAM > 0) {
		// PSRAM found, AudioBuffer will be allocated in PSRAM
		m_f_psram = true;
		m_buffSize = m_buffSizePSRAM;
		m_buffer = (uint8_t*)ps_calloc(m_buffSize, sizeof(uint8_t));
		m_buffSize = m_buffSizePSRAM - m_resBuffSizePSRAM;
	}

	if (m_buffer == nullptr) {
		// PSRAM not found, not configured or not enough available
		m_f_psram = false;
		m_buffer = (uint8_t*)heap_caps_calloc(m_buffSizeRAM, sizeof(uint8_t), MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
		m_buffSize = m_buffSizeRAM - m_resBuffSizeRAM;
	}
	if (!m_buffer) return 0;
	m_f_init = true;
	resetBuffer();
	return m_buffSize;
}

void AudioBuffer::changeMaxBlockSize(uint16_t mbs) {
	m_maxBlockSize = mbs;
}

uint16_t AudioBuffer::getMaxBlockSize() {
	return m_maxBlockSize;
}

size_t AudioBuffer::freeSpace() {
	if (m_readPtr >= m_writePtr) {
		m_freeSpace = (m_readPtr - m_writePtr);
	}
	else {
		m_freeSpace = (m_endPtr - m_writePtr) + (m_readPtr - m_buffer);
	}
	if (m_f_start) m_freeSpace = m_buffSize;
	return m_freeSpace - 1;
}

size_t AudioBuffer::writeSpace() {
	if (m_readPtr >= m_writePtr) {
		m_writeSpace = (m_readPtr - m_writePtr - 1);  // readPtr must not be overtaken
	}
	else {
		if (getReadPos() == 0)
			m_writeSpace = (m_endPtr - m_writePtr - 1);
		else
			m_writeSpace = (m_endPtr - m_writePtr);
	}
	if (m_f_start) m_writeSpace = m_buffSize - 1;
	return m_writeSpace;
}

size_t AudioBuffer::bufferFilled() {
	if (m_writePtr >= m_readPtr) {
		m_dataLength = (m_writePtr - m_readPtr);
	}
	else {
		m_dataLength = (m_endPtr - m_readPtr) + (m_writePtr - m_buffer);
	}
	return m_dataLength;
}

void AudioBuffer::bytesWritten(size_t bw) {
	m_writePtr += bw;
	if (m_writePtr == m_endPtr) {
		m_writePtr = m_buffer;
	}
	if (bw && m_f_start) m_f_start = false;
}

void AudioBuffer::bytesWasRead(size_t br) {
	m_readPtr += br;
	if (m_readPtr >= m_endPtr) {
		size_t tmp = m_readPtr - m_endPtr;
		m_readPtr = m_buffer + tmp;
	}
}

uint8_t* AudioBuffer::getWritePtr() {
	return m_writePtr;
}

uint8_t* AudioBuffer::getReadPtr() {
	size_t len = m_endPtr - m_readPtr;
	if (len < m_maxBlockSize) {							   // be sure the last frame is completed
		memcpy(m_endPtr, m_buffer, m_maxBlockSize - len);  // cpy from m_buffer to m_endPtr with len
	}
	return m_readPtr;
}

void AudioBuffer::resetBuffer() {
	m_writePtr = m_buffer;
	m_readPtr = m_buffer;
	m_endPtr = m_buffer + m_buffSize;
	m_f_start = true;
	// memset(m_buffer, 0, m_buffSize); //Clear Inputbuffer
}

uint32_t AudioBuffer::getWritePos() {
	return m_writePtr - m_buffer;
}

uint32_t AudioBuffer::getReadPos() {
	return m_readPtr - m_buffer;
}