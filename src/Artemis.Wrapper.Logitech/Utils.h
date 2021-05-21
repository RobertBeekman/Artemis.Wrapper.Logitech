#pragma once
#include "pch.h"
#include<string>

//https://stackoverflow.com/questions/215963
inline std::string utf8_encode(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

inline std::wstring utf8_decode(const std::string& str)
{
	if (str.empty()) return std::wstring();
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

inline std::string trim(std::string str)
{
	size_t firstChar = str.find_first_not_of('\0');
	size_t lastChar = str.find_last_not_of('\0');
	size_t length = lastChar - firstChar + 1;
	return str.substr(firstChar, length);
}

inline std::string GetCallerPath()
{
	WCHAR filenameBuffer[MAX_PATH];
	int bytes = GetModuleFileNameW(NULL, filenameBuffer, MAX_PATH);

	return trim(utf8_encode(filenameBuffer));
}