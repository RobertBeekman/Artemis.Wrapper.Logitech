#pragma once

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