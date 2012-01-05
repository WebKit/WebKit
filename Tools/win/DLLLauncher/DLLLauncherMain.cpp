/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file contains code for a launcher executable for WebKit apps. When compiled into foo.exe, it
// will set PATH so that Apple Application Support DLLs can be found, then will load foo.dll and
// call its dllLauncherEntryPoint function, which should be declared like so:
//     extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpstCmdLine, int nCmdShow);

#include <shlwapi.h>
#include <string>
#include <vector>
#include <windows.h>

using namespace std;

static void enableTerminationOnHeapCorruption()
{
    // Enable termination on heap corruption on OSes that support it (Vista and XPSP3).
    // http://msdn.microsoft.com/en-us/library/aa366705(VS.85).aspx

    HEAP_INFORMATION_CLASS heapEnableTerminationOnCorruption = static_cast<HEAP_INFORMATION_CLASS>(1);

    HMODULE module = ::GetModuleHandleW(L"kernel32.dll");
    if (!module)
        return;

    typedef BOOL (WINAPI*HSI)(HANDLE, HEAP_INFORMATION_CLASS, PVOID, SIZE_T);
    HSI heapSetInformation = reinterpret_cast<HSI>(::GetProcAddress(module, "HeapSetInformation"));
    if (!heapSetInformation)
        return;

    heapSetInformation(0, heapEnableTerminationOnCorruption, 0, 0);
}

static wstring getStringValue(HKEY key, const wstring& valueName)
{
    DWORD type = 0;
    DWORD bufferSize = 0;
    if (::RegQueryValueExW(key, valueName.c_str(), 0, &type, 0, &bufferSize) != ERROR_SUCCESS || type != REG_SZ)
        return wstring();

    vector<wchar_t> buffer(bufferSize / sizeof(wchar_t));
    if (::RegQueryValueExW(key, valueName.c_str(), 0, &type, reinterpret_cast<LPBYTE>(&buffer[0]), &bufferSize) != ERROR_SUCCESS)
        return wstring();

    return &buffer[0];
}

static wstring applePathFromRegistry(const wstring& key, const wstring& value)
{
    HKEY applePathKey = 0;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_READ, &applePathKey) != ERROR_SUCCESS)
        return wstring();
    wstring path = getStringValue(applePathKey, value);
    ::RegCloseKey(applePathKey);
    return path;
}

static wstring appleApplicationSupportDirectory()
{
    return applePathFromRegistry(L"SOFTWARE\\Apple Inc.\\Apple Application Support", L"InstallDir");
}

static wstring copyEnvironmentVariable(const wstring& variable)
{
    DWORD length = ::GetEnvironmentVariableW(variable.c_str(), 0, 0);
    if (!length)
        return wstring();
    vector<wchar_t> buffer(length);
    if (!GetEnvironmentVariable(variable.c_str(), &buffer[0], buffer.size()) || !buffer[0])
        return wstring();
    return &buffer[0];
}

static bool prependPath(const wstring& directoryToPrepend)
{
    wstring pathVariable = L"PATH";
    wstring oldPath = copyEnvironmentVariable(pathVariable);
    wstring newPath = directoryToPrepend + L';' + oldPath;
    return ::SetEnvironmentVariableW(pathVariable.c_str(), newPath.c_str());
}

static int fatalError(const wstring& programName, const wstring& message)
{
    wstring caption = programName + L" can't open.";
    ::MessageBoxW(0, message.c_str(), caption.c_str(), MB_ICONERROR);
    return 1;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpstrCmdLine, int nCmdShow)
{
    enableTerminationOnHeapCorruption();

    // Get the path of our executable.
    wchar_t exePath[MAX_PATH];
    if (!::GetModuleFileNameW(0, exePath, _countof(exePath)))
        return fatalError(L"Unknown Program", L"Failed to determine name of executable.");

    ::PathRemoveExtensionW(exePath);

    wstring programName = ::PathFindFileNameW(exePath);

    wstring aasDirectory = appleApplicationSupportDirectory();
    if (aasDirectory.empty())
        return fatalError(programName, L"Failed to determine path to Apple Application Support directory.");
    if (!prependPath(aasDirectory))
        return fatalError(programName, L"Failed to modify PATH environment variable.");

    // Load our corresponding DLL.
    wstring dllName = programName + L".dll";
    if (!::PathRemoveFileSpecW(exePath))
        return fatalError(programName, L"::PathRemoveFileSpecW failed.");
    if (!::PathAppendW(exePath, dllName.c_str()))
        return fatalError(programName, L"::PathAppendW failed.");
    HMODULE module = ::LoadLibraryW(exePath);
    if (!module)
        return fatalError(programName, L"::LoadLibraryW failed.");

    typedef int (WINAPI*EntryPoint)(HINSTANCE, HINSTANCE, LPWSTR, int);
    EntryPoint entryPoint = reinterpret_cast<EntryPoint>(::GetProcAddress(module, "_dllLauncherEntryPoint@16"));
    if (!entryPoint)
        return fatalError(programName, L"Failed to find dllLauncherEntryPoint function.");

    return entryPoint(hInstance, hPrevInstance, lpstrCmdLine, nCmdShow);
}
