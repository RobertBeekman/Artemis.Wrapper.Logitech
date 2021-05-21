#pragma once
#include "pch.h"
#include "OriginalDllWrapper.h"
#include "Constants.h"
#include "Logger.h"
#include "Utils.h"

void OriginalDllWrapper::LoadDll() {
	if (IsDllLoaded()) {
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

	//Why do i need to call this before initing a DllHelper?
	//if i remove this line, the game loading the dll crashes.
	//investigate
	LoadLibraryW(buffer);

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