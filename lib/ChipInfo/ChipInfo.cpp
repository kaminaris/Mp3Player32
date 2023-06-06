#include "ChipInfo.h"

void ChipInfo::printChipInfo() {
	uint64_t mac = ESP.getEfuseMac();

	String info = "";

	info += "\nChip model: " + String(ESP.getChipModel());
	info += "\nChip cores: " + String(ESP.getChipCores());
	info += "\nChip frequency: " + String(ESP.getCpuFreqMHz()) + "Mhz";
	info += "\nChip version: " + String(ESP.getChipRevision());

	info += "\nRAM size: " + String((ESP.getHeapSize() / 1024.0), 0) + "kB";
	info += "\nPSRAM size: " + String((ESP.getPsramSize() / (1024.0 * 1024.0)), 0) + "MB";

	info += "\nFlash size: " + String((ESP.getFlashChipSize() / (1024.0 * 1024.0)), 0) + "MB";
	info += "\nFlash speed: " + String((ESP.getFlashChipSpeed() / 1000000.0), 0) + "Mhz";

	info += "\nSDK version: " + String(ESP.getSdkVersion());
	info += "\nFirmware size: " + String((ESP.getSketchSize() / (1024.0 * 1024.0)), 0) + "MB";
	info += "\nMAC address: ";

	for (int i = 0; i < 6; i++) {
		if (i > 0) {
			info += "-";
		}
		info += String(byte(mac >> (i * 8) & 0xFF), HEX);
	}

	Serial.println(info);
}
