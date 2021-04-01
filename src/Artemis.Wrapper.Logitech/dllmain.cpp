// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "LogitechLEDLib.h"
#include <string>
#include <fstream>
#include <time.h>

#define PIPE_NAME L"\\\\.\\pipe\\Artemis\\Logitech"
#define ARTEMIS_REG_NAME L"Artemis"

#ifdef _WIN64
#define REGISTRY_PATH L"SOFTWARE\\Classes\\CLSID\\{a6519e67-7632-4375-afdf-caa889744403}\\ServerBinary" 
#define _BITS "64"
#else
#define REGISTRY_PATH L"SOFTWARE\\Classes\\WOW6432Node\\CLSID\{a6519e67-7632-4375-afdf-caa889744403}\\ServerBinary"
#define _BITS "32"
#endif

#ifdef _DEBUG
#include "fmt/format.h"
#include "fmt/chrono.h"
void log_to_file(std::string data) {
	std::ofstream logFile;
	logFile.open("log.txt", std::ios::out | std::ios::app);

	std::time_t t = std::time(nullptr);
	struct tm newTime;
	localtime_s(&newTime, &t);
	auto timeHeader = fmt::format("[{:%Y-%m-%d %H:%M:%S}] ", newTime);

	logFile << timeHeader << data << '\n';
	logFile.close();
}
#define LOG(x) log_to_file(x)
#else
#define LOG(x) ((void)0)
#endif 

#pragma region Static variables
static HANDLE artemisPipe = NULL;
static HMODULE originalDll = NULL;
static bool originalDllLoaded = false;
static bool artemisConnected = false;
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
#pragma endregion

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
#pragma endregion

#pragma region Original Dll Methods
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
#pragma endregion

std::string GetCallerPath()
{
	WCHAR filenameBuffer[MAX_PATH];
	int bytes = GetModuleFileNameW(NULL, filenameBuffer, MAX_PATH);

	return trim(utf8_encode(filenameBuffer));
}

bool TryConnectToPipe() {
	artemisPipe = CreateFile(
		PIPE_NAME,
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	return !(artemisPipe == NULL || artemisPipe == INVALID_HANDLE_VALUE);
}

void WriteToPipe(std::string data) {
	if (originalDllLoaded)
		return;

	if (artemisPipe == INVALID_HANDLE_VALUE || artemisPipe == NULL) {
		if (!TryConnectToPipe()) {
			return;
		}
	}

	LOG("Writing to pipe: " + data);
	DWORD cbBytes;
	data.push_back('\n');
	const char* c_contents = data.c_str();
	DWORD c_cotents_len = (DWORD)strlen(c_contents);

	BOOL bResult = WriteFile(
		artemisPipe,
		c_contents,
		c_cotents_len,
		&cbBytes,
		NULL);
}

bool TryLoadOriginalDll()
{
	if (originalDllLoaded || originalDll != NULL)
		return true;

	HKEY registryKey;
	LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, REGISTRY_PATH, 0, KEY_QUERY_VALUE, &registryKey);
	if (result != ERROR_SUCCESS) {
		LOG(fmt::format("Failed to open registry key \'{}\'. Error: {}}", utf8_encode(REGISTRY_PATH), result));
		return false;
	}

	LOG(fmt::format("Opened registry key \'{}\'", utf8_encode(REGISTRY_PATH)));
	WCHAR buffer[255] = { 0 };
	DWORD bufferSize = sizeof(buffer);
	LSTATUS resultB = RegQueryValueExW(registryKey, ARTEMIS_REG_NAME, 0, NULL, (LPBYTE)buffer, &bufferSize);
	if (resultB != ERROR_SUCCESS) {
		LOG(fmt::format("Failed to query registry name \'{}\'. Error: {}", utf8_encode(ARTEMIS_REG_NAME), resultB));
		return false;
	}

	LOG(fmt::format("Queried registry name \'{}\' and got value \'{}\'", utf8_encode(ARTEMIS_REG_NAME), utf8_encode(buffer)));

	originalDll = LoadLibraryW(buffer);
	if (originalDll == NULL) {
		LOG("Failed to load original dll");
		return false;
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
	return true;
}

bool IsProgramNameBlacklisted(std::string name) {
	return name == "Artemis.UI.exe";
}

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
	LOG("LogiLedInit Called");
	bool blacklisted = IsProgramNameBlacklisted(program_name);
	if (!blacklisted) {
		LOG("Trying to connect to pipe...");
		artemisConnected = TryConnectToPipe();

		if (artemisConnected) {
			LOG("Connected to pipe.");
			WriteToPipe("LogiLedInit: " + program_name);
			return true;
		}
		else {
			LOG("Failed to connect to pipe.");

		}
	}
	else {
		LOG(fmt::format("Program name {} blacklisted.", program_name));
	}

	LOG("Trying to load original dll...");

	originalDllLoaded = TryLoadOriginalDll();

	if (originalDllLoaded) {
		return LogiLedInitOriginal();
	}

	return false;
}

bool LogiLedInitWithName(const char name[])
{
	if (artemisConnected) {
		WriteToPipe("LogiLedInitWithName: " + std::string(name));
		return true;
	}

	if (originalDllLoaded) {
		return LogiLedInitWithNameOriginal(name);
	}

	return false;
}

bool LogiLedSetTargetDevice(int targetDevice)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedSetTargetDevice");
		return true;
	}

	if (originalDllLoaded) {
		return LogiLedSetTargetDeviceOriginal(targetDevice);
	}

	return false;
}

bool LogiLedSaveCurrentLighting()
{
	if (artemisConnected) {
		WriteToPipe("LogiLedSaveCurrentLighting");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedSaveCurrentLightingOriginal();
	}
	return false;
}

bool LogiLedSetLighting(int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedSetLighting");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedSetLightingOriginal(redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedRestoreLighting()
{
	if (artemisConnected) {
		WriteToPipe("LogiLedRestoreLighting");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedRestoreLightingOriginal();
	}
	return false;
}

bool LogiLedFlashLighting(int redPercentage, int greenPercentage, int bluePercentage, int milliSecondsDuration, int milliSecondsInterval)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedFlashLighting");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedFlashLightingOriginal(redPercentage, greenPercentage, bluePercentage, milliSecondsDuration, milliSecondsInterval);
	}
	return false;
}

bool LogiLedPulseLighting(int redPercentage, int greenPercentage, int bluePercentage, int milliSecondsDuration, int milliSecondsInterval)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedPulseLighting");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedPulseLightingOriginal(redPercentage, greenPercentage, bluePercentage, milliSecondsDuration, milliSecondsInterval);
	}
	return false;
}

bool LogiLedStopEffects()
{
	if (artemisConnected) {
		WriteToPipe("LogiLedStopEffects");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedStopEffectsOriginal();
	}
	return false;
}

bool LogiLedSetLightingFromBitmap(unsigned char bitmap[])
{
	if (artemisConnected) {
		WriteToPipe("LogiLedSetLightingFromBitmap");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedSetLightingFromBitmapOriginal(bitmap);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithScanCode(int keyCode, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedSetLightingForKeyWithScanCode");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedSetLightingForKeyWithScanCodeOriginal(keyCode, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithHidCode(int keyCode, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedSetLightingForKeyWithHidCode");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedSetLightingForKeyWithHidCodeOriginal(keyCode, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithQuartzCode(int keyCode, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedSetLightingForKeyWithQuartzCode");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedSetLightingForKeyWithQuartzCodeOriginal(keyCode, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSetLightingForKeyWithKeyName(LogiLed::KeyName keyName, int redPercentage, int greenPercentage, int bluePercentage)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedSetLightingForKeyWithKeyName");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedSetLightingForKeyWithKeyNameOriginal(keyName, redPercentage, greenPercentage, bluePercentage);
	}
	return false;
}

bool LogiLedSaveLightingForKey(LogiLed::KeyName keyName)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedSaveLightingForKey");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedSaveLightingForKeyOriginal(keyName);
	}
	return false;
}

bool LogiLedRestoreLightingForKey(LogiLed::KeyName keyName)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedRestoreLightingForKey");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedRestoreLightingForKeyOriginal(keyName);
	}
	return false;
}

bool LogiLedFlashSingleKey(LogiLed::KeyName keyName, int redPercentage, int greenPercentage, int bluePercentage, int msDuration, int msInterval)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedFlashSingleKey");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedFlashSingleKeyOriginal(keyName, redPercentage, greenPercentage, bluePercentage, msDuration, msInterval);
	}
	return false;
}

bool LogiLedPulseSingleKey(LogiLed::KeyName keyName, int startRedPercentage, int startGreenPercentage, int startBluePercentage, int finishRedPercentage, int finishGreenPercentage, int finishBluePercentage, int msDuration, bool isInfinite)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedPulseSingleKey");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedPulseSingleKeyOriginal(keyName, startRedPercentage, startGreenPercentage, startBluePercentage, finishRedPercentage, finishGreenPercentage, finishBluePercentage, msDuration, isInfinite);
	}
	return false;
}

bool LogiLedStopEffectsOnKey(LogiLed::KeyName keyName)
{
	if (artemisConnected) {
		WriteToPipe("LogiLedStopEffectsOnKey");
		return true;
	}
	if (originalDllLoaded) {
		return LogiLedStopEffectsOnKeyOriginal(keyName);
	}
	return false;
}

void LogiLedShutdown()
{
	if (artemisConnected) {
		WriteToPipe("LogiLedShutdown");
		CloseHandle(artemisPipe);
		artemisConnected = false;
		return;
	}
	if (originalDllLoaded) {
		LogiLedShutdownOriginal();

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
		originalDllLoaded = false;
	}
	return;
}
#pragma endregion
