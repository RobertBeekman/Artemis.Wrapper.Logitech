#pragma once

#ifdef _DEBUG
#include "fmt/format.h"
#include "fmt/chrono.h"
#include <fstream>
#include <string>
inline void log_to_file(std::string data) {
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