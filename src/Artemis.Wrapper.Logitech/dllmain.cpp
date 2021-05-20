// dllmain.cpp : Defines the entry point for the DLL application.
#pragma warning( disable : 6387 )
#include "pch.h"
#include "LogitechLEDLib.h"
#include <string>
#include <vector>

#define PIPE_NAME L"\\\\.\\pipe\\Artemis\\Logitech"
#define ARTEMIS_REG_NAME L"Artemis"
#define ARTEMIS_EXE_NAME "Artemis.UI.exe"

#ifdef _WIN64
#define REGISTRY_PATH L"SOFTWARE\\Classes\\CLSID\\{a6519e67-7632-4375-afdf-caa889744403}\\ServerBinary" 
#define _BITS "64"
#else
#define REGISTRY_PATH L"SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{a6519e67-7632-4375-afdf-caa889744403}\\ServerBinary"
#define _BITS "32"
#endif

#ifdef _DEBUG
#include <fstream>
#include "fmt/format.h"
#include "fmt/chrono.h"
void log_to_file(std::string data) {
	std::ofstream logFile;
	logFile.open("ArtemisWrapper.log", std::ios::out | std::ios::app);

	std::time_t t = std::time(nullptr);
	struct tm newTime;
	localtime_s(&newTime, &t);
	std::string timeHeader = fmt::format("[{:%Y-%m-%d %H:%M:%S}] ", newTime);

	logFile << timeHeader << data << '\n';
	logFile.close();
}
#define LOG(x) log_to_file(x)
#else
#define LOG(x) ((void)0)
#endif 

enum LogiCommands : unsigned int {
	LogLine = 0,
	Init,
	InitWithName,
	GetSdkVersion,
	GetConfigOptionNumber,
	GetConfigOptionBool,
	GetConfigOptionColor,
	GetConfigOptionRect,
	GetConfigOptionString,
	GetConfigOptionKeyInput,
	GetConfigOptionSelect,
	GetConfigOptionRange,
	SetConfigOptionLabel,
	SetTargetDevice,
	SaveCurrentLighting,
	SetLighting,
	RestoreLighting,
	FlashLighting,
	PulseLighting,
	StopEffects,
	SetLightingFromBitmap,
	SetLightingForKeyWithScanCode,
	SetLightingForKeyWithHidCode,
	SetLightingForKeyWithQuartzCode,
	SetLightingForKeyWithKeyName,
	SaveLightingForKey,
	RestoreLightingForKey,
	ExcludeKeysFromBitmap,
	FlashSingleKey,
	PulseSingleKey,
	StopEffectsOnKey,
	SetLightingForTargetZone,
	Shutdown,
};

#pragma region Static variables
static bool isInitialized = false;
static bool isPipeConnected = false;
static bool isOriginalDllLoaded = false;
static HANDLE artemisPipe = NULL;
static HMODULE originalDll = NULL;
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

#pragma region Dll Management
#pragma region FuncPointer typedefs
typedef void (*FuncVoid)();
typedef bool (*FuncBool)();
typedef bool (*FuncBoolName)(const char name[]);
typedef bool (*FuncBoolBitmap)(unsigned char bitmap[]);
typedef bool (*FuncBoolDeviceType)(int targetDevice);
typedef bool (*FuncBoolKeyName)(LogiLed::KeyName keyName);
typedef bool (*FuncBoolKeyNames)(LogiLed::KeyName* keyList, int listCount);
typedef bool (*FuncBoolColors)(int redPercentage, int greenPercentage, int bluePercentage);
typedef bool (*FuncBoolKeyCodeColor)(int keyCode, int redPercentage, int greenPercentage, int bluePercentage);
typedef bool (*FuncBoolKeyNameColor)(LogiLed::KeyName keyName, int redPercentage, int greenPercentage, int bluePercentage);
typedef bool (*FuncBoolColorInterval)(int redPercentage, int greenPercentage, int bluePercentage, int milliSecondsDuration, int milliSecondsInterval);
typedef bool (*FuncBoolFlashSingleKey)(LogiLed::KeyName keyName, int redPercentage, int greenPercentage, int bluePercentage, int milliSecondsDuration, int milliSecondsInterval);
typedef bool (*FuncBoolPulseSingleKey)(LogiLed::KeyName keyName, int startRedPercentage, int startGreenPercentage, int startBluePercentage, int finishRedPercentage, int finishGreenPercentage, int finishBluePercentage, int msDuration, bool isInfinite);
#pragma endregion //FuncPointer typedefs

#pragma region Original Dll Functions
FuncBool LogiLedInitOriginal;
FuncBoolName LogiLedInitWithNameOriginal;

FuncBoolDeviceType LogiLedSetTargetDeviceOriginal;
FuncBool LogiLedSaveCurrentLightingOriginal;
FuncBoolColors LogiLedSetLightingOriginal;
FuncBool LogiLedRestoreLightingOriginal;
FuncBoolColorInterval LogiLedFlashLightingOriginal;
FuncBoolColorInterval LogiLedPulseLightingOriginal;
FuncBool LogiLedStopEffectsOriginal;

FuncBoolBitmap LogiLedSetLightingFromBitmapOriginal;
FuncBoolKeyCodeColor LogiLedSetLightingForKeyWithScanCodeOriginal;
FuncBoolKeyCodeColor LogiLedSetLightingForKeyWithHidCodeOriginal;
FuncBoolKeyCodeColor LogiLedSetLightingForKeyWithQuartzCodeOriginal;
FuncBoolKeyNameColor LogiLedSetLightingForKeyWithKeyNameOriginal;
FuncBoolKeyName LogiLedSaveLightingForKeyOriginal;
FuncBoolKeyName LogiLedRestoreLightingForKeyOriginal;
FuncBoolKeyNames LogiLedExcludeKeysFromBitmapOriginal;

FuncBoolFlashSingleKey LogiLedFlashSingleKeyOriginal;
FuncBoolPulseSingleKey LogiLedPulseSingleKeyOriginal;
FuncBoolKeyName LogiLedStopEffectsOnKeyOriginal;

FuncVoid LogiLedShutdownOriginal;
#pragma endregion //Original Dll Methods

#pragma region Original dll loading & unloading
void LoadOriginalDll()
{
	if (isOriginalDllLoaded) {
		LOG("Tried to load original dll again, returning true");
		return;
	}

	HKEY registryKey;
	LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, REGISTRY_PATH, 0, KEY_QUERY_VALUE, &registryKey);
	if (result != ERROR_SUCCESS) {
		LOG(fmt::format("Failed to open registry key \'{}\'. Error: {}}", utf8_encode(REGISTRY_PATH), result));
		return;
	}

	LOG(fmt::format("Opened registry key \'{}\'", utf8_encode(REGISTRY_PATH)));
	WCHAR buffer[255] = { 0 };
	DWORD bufferSize = sizeof(buffer);
	LSTATUS resultB = RegQueryValueExW(registryKey, ARTEMIS_REG_NAME, 0, NULL, (LPBYTE)buffer, &bufferSize);
	if (resultB != ERROR_SUCCESS) {
		LOG(fmt::format("Failed to query registry name \'{}\'. Error: {}", utf8_encode(ARTEMIS_REG_NAME), resultB));
		return;
	}

	LOG(fmt::format("Queried registry name \'{}\' and got value \'{}\'", utf8_encode(ARTEMIS_REG_NAME), utf8_encode(buffer)));
	if (GetFileAttributesW(buffer) == INVALID_FILE_ATTRIBUTES) {
		LOG(fmt::format("Dll file \'{path}\' does not exist. Failed to load original dll.", utf8_encode(buffer)));
		return;
	}

	originalDll = LoadLibraryW(buffer);
	isOriginalDllLoaded = originalDll != NULL;
	if (!isOriginalDllLoaded) {
		LOG("Failed to load original dll");
		return;
	}
	LOG("Loaded original dll successfully");
	LogiLedInitOriginal = (FuncBool)GetProcAddress(originalDll, "LogiLedInit");
	LogiLedInitWithNameOriginal = (FuncBoolName)GetProcAddress(originalDll, "LogiLedInitWithName");
	LogiLedSetTargetDeviceOriginal = (FuncBoolDeviceType)GetProcAddress(originalDll, "LogiLedSetTargetDevice");
	LogiLedSaveCurrentLightingOriginal = (FuncBool)GetProcAddress(originalDll, "LogiLedSaveCurrentLighting");
	LogiLedSetLightingOriginal = (FuncBoolColors)GetProcAddress(originalDll, "LogiLedSetLighting");
	LogiLedRestoreLightingOriginal = (FuncBool)GetProcAddress(originalDll, "LogiLedRestoreLighting");
	LogiLedFlashLightingOriginal = (FuncBoolColorInterval)GetProcAddress(originalDll, "LogiLedFlashLighting");
	LogiLedPulseLightingOriginal = (FuncBoolColorInterval)GetProcAddress(originalDll, "LogiLedPulseLighting");
	LogiLedStopEffectsOriginal = (FuncBool)GetProcAddress(originalDll, "LogiLedStopEffects");
	LogiLedSetLightingFromBitmapOriginal = (FuncBoolBitmap)GetProcAddress(originalDll, "LogiLedSetLightingFromBitmap");
	LogiLedSetLightingForKeyWithScanCodeOriginal = (FuncBoolKeyCodeColor)GetProcAddress(originalDll, "LogiLedSetLightingForKeyWithScanCode");
	LogiLedSetLightingForKeyWithHidCodeOriginal = (FuncBoolKeyCodeColor)GetProcAddress(originalDll, "LogiLedSetLightingForKeyWithHidCode");
	LogiLedSetLightingForKeyWithQuartzCodeOriginal = (FuncBoolKeyCodeColor)GetProcAddress(originalDll, "LogiLedSetLightingForKeyWithQuartzCode");
	LogiLedSetLightingForKeyWithKeyNameOriginal = (FuncBoolKeyNameColor)GetProcAddress(originalDll, "LogiLedSetLightingForKeyWithKeyName");
	LogiLedSaveLightingForKeyOriginal = (FuncBoolKeyName)GetProcAddress(originalDll, "LogiLedSaveLightingForKey");
	LogiLedRestoreLightingForKeyOriginal = (FuncBoolKeyName)GetProcAddress(originalDll, "LogiLedRestoreLightingForKey");
	LogiLedExcludeKeysFromBitmapOriginal = (FuncBoolKeyNames)GetProcAddress(originalDll, "LogiLedExcludeKeysFromBitmap");
	LogiLedFlashSingleKeyOriginal = (FuncBoolFlashSingleKey)GetProcAddress(originalDll, "LogiLedFlashSingleKey");
	LogiLedPulseSingleKeyOriginal = (FuncBoolPulseSingleKey)GetProcAddress(originalDll, "LogiLedPulseSingleKey");
	LogiLedStopEffectsOnKeyOriginal = (FuncBoolKeyName)GetProcAddress(originalDll, "LogiLedStopEffectsOnKey");
	LogiLedShutdownOriginal = (FuncVoid)GetProcAddress(originalDll, "LogiLedShutdown");
	LOG("Got original function addresses successfully");
	return;
}

void FreeOriginalDll() {
	if (!isOriginalDllLoaded)
		return;

	LogiLedInitOriginal = NULL;
	LogiLedInitWithNameOriginal = NULL;
	LogiLedSetTargetDeviceOriginal = NULL;
	LogiLedSaveCurrentLightingOriginal = NULL;
	LogiLedSetLightingOriginal = NULL;
	LogiLedRestoreLightingOriginal = NULL;
	LogiLedFlashLightingOriginal = NULL;
	LogiLedPulseLightingOriginal = NULL;
	LogiLedStopEffectsOriginal = NULL;
	LogiLedSetLightingFromBitmapOriginal = NULL;
	LogiLedSetLightingForKeyWithScanCodeOriginal = NULL;
	LogiLedSetLightingForKeyWithHidCodeOriginal = NULL;
	LogiLedSetLightingForKeyWithQuartzCodeOriginal = NULL;
	LogiLedSetLightingForKeyWithKeyNameOriginal = NULL;
	LogiLedSaveLightingForKeyOriginal = NULL;
	LogiLedRestoreLightingForKeyOriginal = NULL;
	LogiLedExcludeKeysFromBitmapOriginal = NULL;
	LogiLedFlashSingleKeyOriginal = NULL;
	LogiLedPulseSingleKeyOriginal = NULL;
	LogiLedStopEffectsOnKeyOriginal = NULL;
	LogiLedShutdownOriginal = NULL;
	FreeLibrary(originalDll);
	originalDll = NULL;
	isOriginalDllLoaded = false;
	LOG("Freed original dll");
}
#pragma endregion //Original dll loading & unloading
#pragma endregion //Dll Management

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
	if (isOriginalDllLoaded)
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

	LoadOriginalDll();

	if (isOriginalDllLoaded) {
		isInitialized = true;
		return LogiLedInitOriginal();
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

	if (isOriginalDllLoaded) {
		return LogiLedInitWithNameOriginal(name);
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

	if (isOriginalDllLoaded) {
		return LogiLedSetTargetDeviceOriginal(targetDevice);
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
	if (isOriginalDllLoaded) {
		return LogiLedSaveCurrentLightingOriginal();
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
	if (isOriginalDllLoaded) {
		return LogiLedSetLightingOriginal(redPercentage, greenPercentage, bluePercentage);
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
	if (isOriginalDllLoaded) {
		return LogiLedRestoreLightingOriginal();
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
	if (isOriginalDllLoaded) {
		return LogiLedFlashLightingOriginal(redPercentage, greenPercentage, bluePercentage, milliSecondsDuration, milliSecondsInterval);
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
	if (isOriginalDllLoaded) {
		return LogiLedPulseLightingOriginal(redPercentage, greenPercentage, bluePercentage, milliSecondsDuration, milliSecondsInterval);
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
	if (isOriginalDllLoaded) {
		return LogiLedStopEffectsOriginal();
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

	if (isOriginalDllLoaded) {
		return LogiLedSetLightingFromBitmapOriginal(bitmap);
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
	if (isOriginalDllLoaded) {
		return LogiLedSetLightingForKeyWithScanCodeOriginal(keyCode, redPercentage, greenPercentage, bluePercentage);
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
	if (isOriginalDllLoaded) {
		return LogiLedSetLightingForKeyWithHidCodeOriginal(keyCode, redPercentage, greenPercentage, bluePercentage);
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
	if (isOriginalDllLoaded) {
		return LogiLedSetLightingForKeyWithQuartzCodeOriginal(keyCode, redPercentage, greenPercentage, bluePercentage);
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
	if (isOriginalDllLoaded) {
		return LogiLedSetLightingForKeyWithKeyNameOriginal(keyName, redPercentage, greenPercentage, bluePercentage);
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
	if (isOriginalDllLoaded) {
		return LogiLedSaveLightingForKeyOriginal(keyName);
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
	if (isOriginalDllLoaded) {
		return LogiLedRestoreLightingForKeyOriginal(keyName);
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
	if (isOriginalDllLoaded) {
		return LogiLedFlashSingleKeyOriginal(keyName, redPercentage, greenPercentage, bluePercentage, msDuration, msInterval);
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
	if (isOriginalDllLoaded) {
		return LogiLedPulseSingleKeyOriginal(keyName, startRedPercentage, startGreenPercentage, startBluePercentage, finishRedPercentage, finishGreenPercentage, finishBluePercentage, msDuration, isInfinite);
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
	if (isOriginalDllLoaded) {
		return LogiLedStopEffectsOnKeyOriginal(keyName);
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

	if (isOriginalDllLoaded) {
		LOG("Calling original shutdown and freeing original dll...");
		LogiLedShutdownOriginal();
		FreeOriginalDll();
	}

	if (!isPipeConnected && !isOriginalDllLoaded)
		LOG("Exiting without doing anything. Fix?");

	isInitialized = false;
	return;
}
#pragma endregion
