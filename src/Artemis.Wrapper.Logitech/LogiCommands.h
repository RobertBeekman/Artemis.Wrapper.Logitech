#pragma once

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