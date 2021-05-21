#pragma once
#include "LogitechLEDLib.h"
#include "DllHelper.h"
#include <string>

class OriginalDllWrapper {
private:
	DllHelper* _dll = NULL;
	void LoadFunctions();
public:
	void LoadDll();
	bool IsDllLoaded();

	decltype(LogiLedInit)* LogiLedInit;
	decltype(LogiLedInitWithName)* LogiLedInitWithName;

	decltype(LogiLedGetSdkVersion)* LogiLedGetSdkVersion;
	decltype(LogiLedGetConfigOptionNumber)* LogiLedGetConfigOptionNumber;
	decltype(LogiLedGetConfigOptionBool)* LogiLedGetConfigOptionBool;
	decltype(LogiLedGetConfigOptionColor)* LogiLedGetConfigOptionColor;
	decltype(LogiLedGetConfigOptionRect)* LogiLedGetConfigOptionRect;
	decltype(LogiLedGetConfigOptionString)* LogiLedGetConfigOptionString;
	decltype(LogiLedGetConfigOptionKeyInput)* LogiLedGetConfigOptionKeyInput;
	decltype(LogiLedGetConfigOptionSelect)* LogiLedGetConfigOptionSelect;
	decltype(LogiLedGetConfigOptionRange)* LogiLedGetConfigOptionRange;
	decltype(LogiLedSetConfigOptionLabel)* LogiLedSetConfigOptionLabel;

	decltype(LogiLedSetTargetDevice)* LogiLedSetTargetDevice;
	decltype(LogiLedSaveCurrentLighting)* LogiLedSaveCurrentLighting;
	decltype(LogiLedSetLighting)* LogiLedSetLighting;
	decltype(LogiLedRestoreLighting)* LogiLedRestoreLighting;
	decltype(LogiLedFlashLighting)* LogiLedFlashLighting;
	decltype(LogiLedPulseLighting)* LogiLedPulseLighting;
	decltype(LogiLedStopEffects)* LogiLedStopEffects;

	decltype(LogiLedSetLightingFromBitmap)* LogiLedSetLightingFromBitmap;
	decltype(LogiLedSetLightingForKeyWithScanCode)* LogiLedSetLightingForKeyWithScanCode;
	decltype(LogiLedSetLightingForKeyWithHidCode)* LogiLedSetLightingForKeyWithHidCode;
	decltype(LogiLedSetLightingForKeyWithQuartzCode)* LogiLedSetLightingForKeyWithQuartzCode;
	decltype(LogiLedSetLightingForKeyWithKeyName)* LogiLedSetLightingForKeyWithKeyName;
	decltype(LogiLedSaveLightingForKey)* LogiLedSaveLightingForKey;
	decltype(LogiLedRestoreLightingForKey)* LogiLedRestoreLightingForKey;
	decltype(LogiLedExcludeKeysFromBitmap)* LogiLedExcludeKeysFromBitmap;

	decltype(LogiLedFlashSingleKey)* LogiLedFlashSingleKey;
	decltype(LogiLedPulseSingleKey)* LogiLedPulseSingleKey;
	decltype(LogiLedStopEffectsOnKey)* LogiLedStopEffectsOnKey;

	decltype(LogiLedSetLightingForTargetZone)* LogiLedSetLightingForTargetZone;

	decltype(LogiLedShutdown)* LogiLedShutdown;
};