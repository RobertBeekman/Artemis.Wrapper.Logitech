// dllmain.cpp : Defines the entry point for the DLL application.
#pragma warning( disable : 6387 )
#include "pch.h"
#include "LogitechLEDLib.h"
#include "LogiCommands.h"
#include "Constants.h"
#include "Logger.h"
#include "OriginalDllWrapper.h"
#include <string>
#include <vector>

#pragma region Static variables
static OriginalDllWrapper originalDllWrapper;
static bool isInitialized = false;
static bool isPipeConnected = false;
static HANDLE artemisPipe = NULL;
static std::string program_name = "";
#pragma endregion

#pragma region Helper methods
//https://stackoverflow.com/questions/215963
std::string utf8_encode(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

std::wstring utf8_decode(const std::string& str)
{
	if (str.empty()) return std::wstring();
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

std::string trim(std::string str)
{
	size_t firstChar = str.find_first_not_of('\0');
	size_t lastChar = str.find_last_not_of('\0');
	size_t length = lastChar - firstChar + 1;
	return str.substr(firstChar, length);
}

std::string GetCallerPath()
{
	WCHAR filenameBuffer[MAX_PATH];
	int bytes = GetModuleFileNameW(NULL, filenameBuffer, MAX_PATH);

	return trim(utf8_encode(filenameBuffer));
}
#pragma endregion

#pragma region Pipe connection
void ConnectToPipe() {
	LOG("Connecting to pipe...");

	artemisPipe = CreateFile(
		PIPE_NAME,
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	isPipeConnected = artemisPipe != NULL && artemisPipe != INVALID_HANDLE_VALUE;

	LOG(isPipeConnected ? "Connected to pipe successfully" : "Could not connect to pipe");
}

void ClosePipe() {
	LOG("Closing pipe...");
	CloseHandle(artemisPipe);
	isPipeConnected = false;
	LOG("Closed pipe");
}

void WriteToPipe(LPCVOID data, DWORD dataLength) {
	if (originalDllWrapper.IsDllLoaded())
		return;

	if (!isPipeConnected) {
		LOG("Pipe disconnected when writing, trying to reconnect...");
		ConnectToPipe();
		if (!isPipeConnected) {
			return;
		}
		LOG("Pipe connection reestablished");
	}

	DWORD writtenLength;
	BOOL result = WriteFile(
		artemisPipe,
		data,
		dataLength,
		&writtenLength,
		NULL);

	if ((!result) || (writtenLength < dataLength)) {
		LOG(fmt::format("Error writing to pipe: \'{error}\'. Wrote {bytes} bytes out of {total}", result, writtenLength, dataLength));
		ClosePipe();
	}
}

void WriteStringToPipe(std::string data) {
	LOG(fmt::format("Writing to pipe: \'{}\'", data));

	const char* cData = data.c_str();
	unsigned int cDataLength = (int)strlen(cData) + 1;

	unsigned int logCommand = LogiCommands::LogLine;
	unsigned int arraySize =
		sizeof(unsigned int) + //length
		sizeof(unsigned int) + //command
		cDataLength;

	std::vector<unsigned char> buff(arraySize, 0);
	unsigned int buffPtr = 0;

	memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
	buffPtr += sizeof(arraySize);

	memcpy(&buff[buffPtr], &logCommand, sizeof(logCommand));
	buffPtr += sizeof(logCommand);

	memcpy(&buff[buffPtr], cData, cDataLength);
	buffPtr += cDataLength;

	WriteToPipe(buff.data(), arraySize);
}
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

#pragma region Exports
bool LogiLedInit()
{
	if (isInitialized) {
		LOG("Program tried to initialize twice, returning true");
		return true;
	}

	LOG("LogiLedInit Called");
	if (program_name != ARTEMIS_EXE_NAME) {
		ConnectToPipe();

		if (isPipeConnected) {
			auto c_str = program_name.c_str();
			unsigned int strLength = (int)strlen(c_str) + 1;
			const unsigned int command = LogiCommands::Init;
			const unsigned int arraySize =
				sizeof(unsigned int) +  //length
				sizeof(unsigned int) +  //command
				strLength;              //str

			std::vector<unsigned char> buff(arraySize, 0);
			unsigned int buffPtr = 0;

			memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
			buffPtr += sizeof(arraySize);

			memcpy(&buff[buffPtr], &command, sizeof(command));
			buffPtr += sizeof(command);

			memcpy(&buff[buffPtr], c_str, strLength);
			buffPtr += strLength;

			WriteToPipe(buff.data(), arraySize);

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

bool LogiLedInitWithName(const char name[])
{
	if (isPipeConnected) {
		const char* c_str = name;
		unsigned int strLength = (int)strlen(c_str) + 1;
		const unsigned int command = LogiCommands::Init;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
			strLength;              //str

		std::vector<unsigned char> buff(arraySize,  0);
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], c_str, strLength);
		buffPtr += strLength;

		WriteToPipe(buff.data(), arraySize);

		return true;
	}

	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedInitWithName(name);
	}

	return false;
}

bool LogiLedSetTargetDevice(int targetDevice)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::SetTargetDevice;
		const unsigned int arraySize =
			sizeof(unsigned int) + //length
			sizeof(unsigned int) + //command
			sizeof(int);           //argument
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &targetDevice, sizeof(targetDevice));
		buffPtr += sizeof(targetDevice);

		WriteToPipe(buff, arraySize);
		return true;
	}

	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetTargetDevice(targetDevice);
	}

	return false;
}

bool LogiLedSaveCurrentLighting()
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::SaveCurrentLighting;
		const unsigned int arraySize =
			sizeof(unsigned int) + //length
			sizeof(unsigned int);  //command
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		WriteToPipe(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSaveCurrentLighting();
	}
	return false;
}

bool LogiLedSetLighting(int redPercentage, int greenPercentage, int bluePercentage)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::SetLighting;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
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

		WriteToPipe(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLighting(redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedRestoreLighting()
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::RestoreLighting;
		const unsigned int arraySize =
			sizeof(unsigned int) + //length
			sizeof(unsigned int);  //command
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		WriteToPipe(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedRestoreLighting();
	}
	return false;
}

bool LogiLedFlashLighting(int redPercentage, int greenPercentage, int bluePercentage, int milliSecondsDuration, int milliSecondsInterval)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::FlashLighting;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char) + //b
			sizeof(int) + //duration
			sizeof(int); //interval

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

		WriteToPipe(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedFlashLighting(redPercentage, greenPercentage, bluePercentage, milliSecondsDuration, milliSecondsInterval);
	}
	return false;
}

bool LogiLedPulseLighting(int redPercentage, int greenPercentage, int bluePercentage, int milliSecondsDuration, int milliSecondsInterval)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::PulseLighting;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char) + //b
			sizeof(int) + //duration
			sizeof(int); //interval

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

		WriteToPipe(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedPulseLighting(redPercentage, greenPercentage, bluePercentage, milliSecondsDuration, milliSecondsInterval);
	}
	return false;
}

bool LogiLedStopEffects()
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::StopEffects;
		const unsigned int arraySize =
			sizeof(unsigned int) + //length
			sizeof(unsigned int);  //command
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		WriteToPipe(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedStopEffects();
	}
	return false;
}

bool LogiLedSetLightingFromBitmap(unsigned char bitmap[])
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::SetLightingFromBitmap;
		const unsigned int arraySize =
			sizeof(unsigned int) + //length
			sizeof(unsigned int) + //command
			LOGI_LED_BITMAP_SIZE;

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &bitmap[0], LOGI_LED_BITMAP_SIZE);
		buffPtr += LOGI_LED_BITMAP_SIZE;

		WriteToPipe(buff, arraySize);
		return true;
	}

	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingFromBitmap(bitmap);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithScanCode(int keyCode, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::SetLightingForKeyWithScanCode;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char) + //b
			sizeof(int); //key

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		memcpy(&buff[buffPtr], &keyCode, sizeof(keyCode));
		buffPtr += sizeof(keyCode);

		WriteToPipe(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingForKeyWithScanCode(keyCode, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithHidCode(int keyCode, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::SetLightingForKeyWithHidCode;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char) + //b
			sizeof(int); //key

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		memcpy(&buff[buffPtr], &keyCode, sizeof(keyCode));
		buffPtr += sizeof(keyCode);

		WriteToPipe(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingForKeyWithHidCode(keyCode, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithQuartzCode(int keyCode, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::SetLightingForKeyWithQuartzCode;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char) + //b
			sizeof(int); //key

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		memcpy(&buff[buffPtr], &keyCode, sizeof(keyCode));
		buffPtr += sizeof(keyCode);

		WriteToPipe(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingForKeyWithQuartzCode(keyCode, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithKeyName(LogiLed::KeyName keyName, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::SetLightingForKeyWithKeyName;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char) + //b
			sizeof(int); //keyName

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		WriteToPipe(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSetLightingForKeyWithKeyName(keyName, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSaveLightingForKey(LogiLed::KeyName keyName)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::SaveLightingForKey;
		const unsigned int arraySize =
			sizeof(unsigned int) + //length
			sizeof(unsigned int) + //command
			sizeof(int);           //keyName
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		WriteToPipe(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedSaveLightingForKey(keyName);
	}
	return false;
}

bool LogiLedRestoreLightingForKey(LogiLed::KeyName keyName)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::RestoreLightingForKey;
		const unsigned int arraySize =
			sizeof(unsigned int) + //length
			sizeof(unsigned int) + //command
			sizeof(int);           //keyName
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		WriteToPipe(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedRestoreLightingForKey(keyName);
	}
	return false;
}

bool LogiLedFlashSingleKey(LogiLed::KeyName keyName, int redPercentage, int greenPercentage, int bluePercentage, int msDuration, int msInterval)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::FlashSingleKey;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
			sizeof(unsigned char) + //r
			sizeof(unsigned char) + //g
			sizeof(unsigned char) + //b
			sizeof(int) + //duration
			sizeof(int) + //interval
			sizeof(int); //keyName

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		buff[buffPtr++] = (unsigned char)((double)redPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)greenPercentage / 100.0 * 255.0);
		buff[buffPtr++] = (unsigned char)((double)bluePercentage / 100.0 * 255.0);

		memcpy(&buff[buffPtr], &msDuration, sizeof(msDuration));
		buffPtr += sizeof(msDuration);

		memcpy(&buff[buffPtr], &msInterval, sizeof(msInterval));
		buffPtr += sizeof(msInterval);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		WriteToPipe(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedFlashSingleKey(keyName, redPercentage, greenPercentage, bluePercentage, msDuration, msInterval);
	}
	return false;
}

bool LogiLedPulseSingleKey(LogiLed::KeyName keyName, int startRedPercentage, int startGreenPercentage, int startBluePercentage, int finishRedPercentage, int finishGreenPercentage, int finishBluePercentage, int msDuration, bool isInfinite)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::PulseSingleKey;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
			sizeof(unsigned char) + //r start
			sizeof(unsigned char) + //g start
			sizeof(unsigned char) + //b start
			sizeof(unsigned char) + //r end
			sizeof(unsigned char) + //g end
			sizeof(unsigned char) + //b end
			sizeof(int) + //duration
			sizeof(bool) + //infinite
			sizeof(int); //keyName

		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

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

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		WriteToPipe(buff, arraySize);

		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedPulseSingleKey(keyName, startRedPercentage, startGreenPercentage, startBluePercentage, finishRedPercentage, finishGreenPercentage, finishBluePercentage, msDuration, isInfinite);
	}
	return false;
}

bool LogiLedStopEffectsOnKey(LogiLed::KeyName keyName)
{
	if (isPipeConnected) {
		const unsigned int command = LogiCommands::StopEffectsOnKey;
		const unsigned int arraySize =
			sizeof(unsigned int) + //length
			sizeof(unsigned int) + //command
			sizeof(int);           //keyName
		unsigned char buff[arraySize] = { 0 };
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], &keyName, sizeof(keyName));
		buffPtr += sizeof(keyName);

		WriteToPipe(buff, arraySize);
		return true;
	}
	if (originalDllWrapper.IsDllLoaded()) {
		return originalDllWrapper.LogiLedStopEffectsOnKey(keyName);
	}
	return false;
}

void LogiLedShutdown()
{
	if (!isInitialized)
		return;

	LOG("LogiLedShutdown called");

	if (isPipeConnected) {
		LOG("Informing artemis and closing pipe...");

		const char* c_str = program_name.c_str();
		unsigned int strLength = (int)strlen(c_str) + 1;
		const unsigned int command = LogiCommands::Shutdown;
		const unsigned int arraySize =
			sizeof(unsigned int) +  //length
			sizeof(unsigned int) +  //command
			strLength;              //str

		std::vector<unsigned char> buff(arraySize, 0);
		unsigned int buffPtr = 0;

		memcpy(&buff[buffPtr], &arraySize, sizeof(arraySize));
		buffPtr += sizeof(arraySize);

		memcpy(&buff[buffPtr], &command, sizeof(command));
		buffPtr += sizeof(command);

		memcpy(&buff[buffPtr], c_str, strLength);
		buffPtr += strLength;

		WriteToPipe(buff.data(), arraySize);

		ClosePipe();
	}

	isInitialized = false;
	return;
}
#pragma endregion