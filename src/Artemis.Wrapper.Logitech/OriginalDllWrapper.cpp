#pragma once
#include "pch.h"
#include "OriginalDllWrapper.h"
#include "Constants.h"
#include "Logger.h"

std::string remove_me(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

void OriginalDllWrapper::LoadDll() {
	if (IsDllLoaded()) {
		LOG("Tried to load original dll again, returning true");
		return;
	}

	HKEY registryKey;
	LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, REGISTRY_PATH, 0, KEY_QUERY_VALUE, &registryKey);
	if (result != ERROR_SUCCESS) {
		LOG(fmt::format("Failed to open registry key \'{}\'. Error: {}}", remove_me(REGISTRY_PATH), result));
		return;
	}

	LOG(fmt::format("Opened registry key \'{}\'", remove_me(REGISTRY_PATH)));
	WCHAR buffer[255] = { 0 };
	DWORD bufferSize = sizeof(buffer);
	LSTATUS resultB = RegQueryValueExW(registryKey, ARTEMIS_REG_NAME, 0, NULL, (LPBYTE)buffer, &bufferSize);
	if (resultB != ERROR_SUCCESS) {
		LOG(fmt::format("Failed to query registry name \'{}\'. Error: {}", remove_me(ARTEMIS_REG_NAME), resultB));
		return;
	}

	LOG(fmt::format("Queried registry name \'{}\' and got value \'{}\'", remove_me(ARTEMIS_REG_NAME), remove_me(buffer)));
	if (GetFileAttributesW(buffer) == INVALID_FILE_ATTRIBUTES) {
		LOG(fmt::format("Dll file \'{path}\' does not exist. Failed to load original dll.", remove_me(buffer)));
		return;
	}

	_dll = new DllHelper(buffer);

	if (!IsDllLoaded()) {
		LOG("Failed to load original dll");
		return;
	}
	LoadFunctions();
}

void OriginalDllWrapper::LoadFunctions() {
	auto dll = *_dll;

	LogiLedInit = dll["LogiLedInit"];
	LogiLedInitWithName = dll["LogiLedInitWithName"];

	LogiLedGetSdkVersion = dll["LogiLedGetSdkVersion"];
	LogiLedGetConfigOptionNumber = dll["LogiLedGetConfigOptionNumber"];
	LogiLedGetConfigOptionBool = dll["LogiLedGetConfigOptionBool"];
	LogiLedGetConfigOptionColor = dll["LogiLedGetConfigOptionColor"];
	LogiLedGetConfigOptionRect = dll["LogiLedGetConfigOptionRect"];
	LogiLedGetConfigOptionString = dll["LogiLedGetConfigOptionString"];
	LogiLedGetConfigOptionKeyInput = dll["LogiLedGetConfigOptionKeyInput"];
	LogiLedGetConfigOptionSelect = dll["LogiLedGetConfigOptionSelect"];
	LogiLedGetConfigOptionRange = dll["LogiLedGetConfigOptionRange"];
	LogiLedSetConfigOptionLabel = dll["LogiLedSetConfigOptionLabel"];

	LogiLedSetTargetDevice = dll["LogiLedSetTargetDevice"];
	LogiLedSaveCurrentLighting = dll["LogiLedSaveCurrentLighting"];
	LogiLedSetLighting = dll["LogiLedSetLighting"];
	LogiLedRestoreLighting = dll["LogiLedRestoreLighting"];
	LogiLedFlashLighting = dll["LogiLedFlashLighting"];
	LogiLedPulseLighting = dll["LogiLedPulseLighting"];
	LogiLedStopEffects = dll["LogiLedStopEffects"];

	LogiLedSetLightingFromBitmap = dll["LogiLedSetLightingFromBitmap"];
	LogiLedSetLightingForKeyWithScanCode = dll["LogiLedSetLightingForKeyWithScanCode"];
	LogiLedSetLightingForKeyWithHidCode = dll["LogiLedSetLightingForKeyWithHidCode"];
	LogiLedSetLightingForKeyWithQuartzCode = dll["LogiLedSetLightingForKeyWithQuartzCode"];
	LogiLedSetLightingForKeyWithKeyName = dll["LogiLedSetLightingForKeyWithKeyName"];
	LogiLedSaveLightingForKey = dll["LogiLedSaveLightingForKey"];
	LogiLedRestoreLightingForKey = dll["LogiLedRestoreLightingForKey"];
	LogiLedExcludeKeysFromBitmap = dll["LogiLedExcludeKeysFromBitmap"];

	LogiLedFlashSingleKey = dll["LogiLedFlashSingleKey"];
	LogiLedPulseSingleKey = dll["LogiLedPulseSingleKey"];
	LogiLedStopEffectsOnKey = dll["LogiLedStopEffectsOnKey"];

	LogiLedSetLightingForTargetZone = dll["LogiLedSetLightingForTargetZone"];

	LogiLedShutdown = dll["LogiLedShutdown"];
}

bool OriginalDllWrapper::IsDllLoaded(){
	if (_dll == NULL) {
		return false;
	}

	return _dll->IsLoaded();
}