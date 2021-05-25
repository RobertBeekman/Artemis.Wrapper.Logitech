// dllmain.cpp : Defines the entry point for the DLL application.
#pragma warning( disable : 6387 )
#pragma warning( disable : 26812 )
#include "pch.h"
#include "LogitechLEDLib.h"
#include "LogiCommands.h"
#include "Constants.h"
#include "Logger.h"
#include "Utils.h"
#include "OriginalDllWrapper.h"
#include "ArtemisPipeClient.h"
#include <string>
#include <vector>

#pragma region Static variables
static OriginalDllWrapper originalDllWrapper;
static ArtemisPipeClient artemisPipeClient;
static bool isInitialized = false;
static std::string program_name = "";
#pragma endregion

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		std::string program_path = GetCallerPath();
		size_t lastBackslashIndex = program_path.find_last_of('\\') + 1;
		program_name = program_path.substr(lastBackslashIndex, program_path.length() - lastBackslashIndex);

		LOG(fmt::format("Main called, DLL loaded into {} ({} bits)", program_name, _BITS));
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

bool LogiLedInit()
{
	return LogiLedInitWithName(program_name.c_str());
}

bool LogiLedInitWithName(const char name[])
{
	if (isInitialized) {
		LOG("Program tried to initialize twice, returning true");
		return true;
	}

	LOG("LogiLedInit Called");
	if (program_name != ARTEMIS_EXE_NAME) {
		artemisPipeClient.Connect();

		if (artemisPipeClient.IsConnected()) {
			unsigned int nameLength = (int)strlen(name) + 1;
			const unsigned int command = LogiCommands::Init;
			const unsigned int arraySize =
				sizeof(arraySize) +
				sizeof(command) +
				nameLength;

			std::vector<unsigned char> buff(arraySize, 0);
			unsigned int buffPtr = 0;

			memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
			buffPtr += sizeof(arraySize);

			memcpy(&buff[buffPtr], &command, sizeof(command));
			buffPtr += sizeof(command);

			memcpy(&buff[buffPtr], name, nameLength);
			buffPtr += nameLength;

			artemisPipeClient.Write(buff.data(), arraySize);

			isInitialized = true;
			return true;
		}
	}
	else {
		LOG(fmt::format("Program name {} blacklisted.", program_name));
	}

	LOG("Trying to load original dll...");

	originalDllWrapper.LoadDll();

	if (originalDllWrapper.IsDllLoaded()) {
		isInitialized = true;
		return originalDllWrapper.LogiLedInit();
	}
	isInitialized = false;
	return false;
}


bool LogiLedSetTargetDevice(int targetDevice)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::SetTargetDevice;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(targetDevice);
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &targetDevice, sizeof(targetDevice));
		buffPtr += sizeof(targetDevice);

		artemisPipeClient.Write(buff, arraySize);
		return true;
	}

	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetTargetDevice(targetDevice);
	}

	return false;
}

bool LogiLedSaveCurrentLighting()
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::SaveCurrentLighting;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command);
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		artemisPipeClient.Write(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSaveCurrentLighting();
	}
	return false;
}

bool LogiLedSetLighting(int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::SetLighting;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char);  //b

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		artemisPipeClient.Write(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLighting(redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedRestoreLighting()
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::RestoreLighting;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command);
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		artemisPipeClient.Write(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedRestoreLighting();
	}
	return false;
}

bool LogiLedFlashLighting(int redPercentage, int greenPercentage, int bluePercentage, int milliSecondsDuration, int milliSecondsInterval)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::FlashLighting;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char) + //b
			sizeof(milliSecondsDuration) +
			sizeof(milliSecondsInterval);

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		memcpy(&buff[buffPtr], &milliSecondsDuration, sizeof(milliSecondsDuration));
		buffPtr += sizeof(milliSecondsDuration);

		memcpy(&buff[buffPtr], &milliSecondsInterval, sizeof(milliSecondsInterval));
		buffPtr += sizeof(milliSecondsInterval);

		artemisPipeClient.Write(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedFlashLighting(redPercentage, greenPercentage, bluePercentage, milliSecondsDuration, milliSecondsInterval);
	}
	return false;
}

bool LogiLedPulseLighting(int redPercentage, int greenPercentage, int bluePercentage, int milliSecondsDuration, int milliSecondsInterval)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::PulseLighting;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char) + //b
			sizeof(milliSecondsDuration) +
			sizeof(milliSecondsInterval);

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		memcpy(&buff[buffPtr], &milliSecondsDuration, sizeof(milliSecondsDuration));
		buffPtr += sizeof(milliSecondsDuration);

		memcpy(&buff[buffPtr], &milliSecondsInterval, sizeof(milliSecondsInterval));
		buffPtr += sizeof(milliSecondsInterval);

		artemisPipeClient.Write(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedPulseLighting(redPercentage, greenPercentage, bluePercentage, milliSecondsDuration, milliSecondsInterval);
	}
	return false;
}

bool LogiLedStopEffects()
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::StopEffects;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command);
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		artemisPipeClient.Write(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedStopEffects();
	}
	return false;
}


bool LogiLedSetLightingFromBitmap(unsigned char bitmap[])
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::SetLightingFromBitmap;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			LOGI_LED_BITMAP_SIZE;

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], bitmap, LOGI_LED_BITMAP_SIZE);
		buffPtr += LOGI_LED_BITMAP_SIZE;

		artemisPipeClient.Write(buff, arraySize);
		return true;
	}

	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingFromBitmap(bitmap);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithScanCode(int keyCode, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::SetLightingForKeyWithScanCode;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(keyCode) +
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char);  //b

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyCode, sizeof(keyCode));
		buffPtr += sizeof(keyCode);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		artemisPipeClient.Write(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingForKeyWithScanCode(keyCode, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithHidCode(int keyCode, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::SetLightingForKeyWithHidCode;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(keyCode) +
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char);  //b

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyCode, sizeof(keyCode));
		buffPtr += sizeof(keyCode);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		artemisPipeClient.Write(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingForKeyWithHidCode(keyCode, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithQuartzCode(int keyCode, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::SetLightingForKeyWithQuartzCode;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(keyCode) +
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char);  //b

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyCode, sizeof(keyCode));
		buffPtr += sizeof(keyCode);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		artemisPipeClient.Write(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingForKeyWithQuartzCode(keyCode, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithKeyName(LogiLed::KeyName keyName, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::SetLightingForKeyWithKeyName;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(LogiLed::KeyName) +
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char);  //b

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		artemisPipeClient.Write(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingForKeyWithKeyName(keyName, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSaveLightingForKey(LogiLed::KeyName keyName)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::SaveLightingForKey;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(LogiLed::KeyName);
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		artemisPipeClient.Write(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSaveLightingForKey(keyName);
	}
	return false;
}

bool LogiLedRestoreLightingForKey(LogiLed::KeyName keyName)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::RestoreLightingForKey;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(LogiLed::KeyName);
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		artemisPipeClient.Write(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedRestoreLightingForKey(keyName);
	}
	return false;
}

bool LogiLedExcludeKeysFromBitmap(LogiLed::KeyName* keyList, int listCount)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::ExcludeKeysFromBitmap;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(LogiLed::KeyName) * listCount;

		std::vector<unsigned char> buff(arraySize, 0);
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], keyList, sizeof(int) * listCount);
		buffPtr += sizeof(int) * listCount;

		artemisPipeClient.Write(buff.data(), arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedExcludeKeysFromBitmap(keyList, listCount);
	}
	return false;
}


bool LogiLedFlashSingleKey(LogiLed::KeyName keyName, int redPercentage, int greenPercentage, int bluePercentage, int msDuration, int msInterval)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::FlashSingleKey;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(LogiLed::KeyName) +
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char) + //b
			sizeof(msDuration) +
			sizeof(msInterval);

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		memcpy(&buff[buffPtr], &msDuration, sizeof(msDuration));
		buffPtr += sizeof(msDuration);

		memcpy(&buff[buffPtr], &msInterval, sizeof(msInterval));
		buffPtr += sizeof(msInterval);

		artemisPipeClient.Write(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedFlashSingleKey(keyName, redPercentage, greenPercentage, bluePercentage, msDuration, msInterval);
	}
	return false;
}

bool LogiLedPulseSingleKey(LogiLed::KeyName keyName, int startRedPercentage, int startGreenPercentage, int startBluePercentage, int finishRedPercentage, int finishGreenPercentage, int finishBluePercentage, int msDuration, bool isInfinite)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::PulseSingleKey;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(LogiLed::KeyName) +
			sizeof(unsigned char) + //r start
			sizeof(unsigned char) + //g start
			sizeof(unsigned char) + //b start
			sizeof(unsigned char) + //r end
			sizeof(unsigned char) + //g end
			sizeof(unsigned char) + //b end
			sizeof(msDuration) +
			sizeof(isInfinite);

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		buff[buffPtr++] = (unsigned char)((double)startRedPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)startGreenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)startBluePercentage / 100.0 * 255.0);

		buff[buffPtr++] = (unsigned char)((double)finishRedPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)finishGreenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)finishBluePercentage / 100.0 * 255.0);

		memcpy(&buff[buffPtr], &msDuration, sizeof(msDuration));
		buffPtr += sizeof(msDuration);

		memcpy(&buff[buffPtr], &isInfinite, sizeof(isInfinite));
		buffPtr += sizeof(isInfinite);

		artemisPipeClient.Write(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedPulseSingleKey(keyName, startRedPercentage, startGreenPercentage, startBluePercentage, finishRedPercentage, finishGreenPercentage, finishBluePercentage, msDuration, isInfinite);
	}
	return false;
}

bool LogiLedStopEffectsOnKey(LogiLed::KeyName keyName)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::StopEffectsOnKey;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(LogiLed::KeyName);
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		artemisPipeClient.Write(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedStopEffectsOnKey(keyName);
	}
	return false;
}


bool LogiLedSetLightingForTargetZone(LogiLed::DeviceType deviceType, int zone, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisPipeClient.IsConnected()) {
		const unsigned int command = LogiCommands::FlashSingleKey;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			sizeof(LogiLed::DeviceType) +
			sizeof(zone) +
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char);  //b

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &deviceType, sizeof(deviceType));
		buffPtr += sizeof(deviceType);

		memcpy(&buff[buffPtr], &zone, sizeof(zone));
		buffPtr += sizeof(zone);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		artemisPipeClient.Write(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingForTargetZone(deviceType, zone, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}


void LogiLedShutdown()
{
	if (!isInitialized)
		return;

	LOG("LogiLedShutdown called");

	if (artemisPipeClient.IsConnected()) {
		LOG("Informing artemis and closing pipe...");

		const char* c_str = program_name.c_str();
		unsigned int strLength = (int)strlen(c_str) + 1;
		const unsigned int command = LogiCommands::Shutdown;
		const unsigned int arraySize =
			sizeof(arraySize) +
			sizeof(command) +
			strLength;

		std::vector<unsigned char> buff(arraySize, 0);
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], c_str, strLength);
		buffPtr += strLength;

		artemisPipeClient.Write(buff.data(), arraySize);

		artemisPipeClient.Disconnect();
	}

	isInitialized = false;
	return;
}

#pragma region Useless methods
bool LogiGetConfigOptionNumber(const wchar_t* configPath, double* defaultValue) 
{ 
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedGetConfigOptionNumber(configPath, defaultValue);
	}
	return false; 
}
bool LogiGetConfigOptionBool(const wchar_t* configPath, bool* defaultValue) 
{
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedGetConfigOptionBool(configPath, defaultValue);
	}
	return false;
}
bool LogiGetConfigOptionColor(const wchar_t* configPath, int* defaultRed, int* defaultGreen, int* defaultBlue)
{
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedGetConfigOptionColor(configPath, defaultRed, defaultGreen, defaultBlue);
	}
	return false;
}
bool LogiGetConfigOptionRect(const wchar_t* configPath, int* defaultX, int* defaultY, int* defaultWidth, int* defaultHeight) 
{
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedGetConfigOptionRect(configPath, defaultX, defaultY, defaultWidth, defaultHeight);
	}
	return false;
}
bool LogiGetConfigOptionRange(const wchar_t* configPath, int* defaultValue, int min, int max)
{
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedGetConfigOptionRange(configPath, defaultValue, min, max);
	}
	return false;
}
bool LogiGetConfigOptionSelect(const wchar_t* configPath, wchar_t* defaultValue, int* valueSize, const wchar_t* values, int bufferSize)
{
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedGetConfigOptionSelect(configPath, defaultValue, valueSize, values, bufferSize);
	}
	return false;
}
bool LogiGetConfigOptionKeyInput(const wchar_t* configPath, wchar_t* defaultValue, int bufferSize)
{
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedGetConfigOptionKeyInput(configPath, defaultValue, bufferSize);
	}
	return false;
}
bool LogiSetConfigOptionLabel(const wchar_t* configPath, wchar_t* label) 
{
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetConfigOptionLabel(configPath, label);
	}
	return false;
}
#pragma endregion
