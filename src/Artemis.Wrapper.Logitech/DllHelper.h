#pragma once
#include "pch.h"
#include <type_traits>

//adapted from: https://blog.benoitblanchon.fr/getprocaddress-like-a-boss/
class ProcPtr {
public:
    explicit ProcPtr(FARPROC ptr) : _ptr(ptr) {}

    template <typename T, typename = std::enable_if_t<std::is_function_v<T>>>
    operator T* () const {
        return reinterpret_cast<T*>(_ptr);
    }

private:
    FARPROC _ptr;
};

class DllHelper {
public:
    explicit DllHelper(LPCTSTR filename) : _module(LoadLibraryW(filename)) {}

    ~DllHelper() { FreeLibrary(_module); }

    ProcPtr operator[](LPCSTR proc_name) const {
        return ProcPtr(GetProcAddress(_module, proc_name));
    }

    bool IsLoaded() {
        return _module != NULL;
    }
private:
    HMODULE _module;
};