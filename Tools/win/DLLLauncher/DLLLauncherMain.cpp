/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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
// If USE_CONSOLE_ENTRY_POINT is defined, this function will be called instead:
//     extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[]);

#include <shlwapi.h>
#include <string>
#include <vector>
#include <windows.h>

using namespace std;

static void enableTerminationOnHeapCorruption()
{
    HEAP_INFORMATION_CLASS heapEnableTerminationOnCorruption = static_cast<HEAP_INFORMATION_CLASS>(1);
    HeapSetInformation(0, heapEnableTerminationOnCorruption, 0, 0);
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

#if !defined(WIN_CAIRO)
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

static wstring iTunesDirectory()
{
    return applePathFromRegistry(L"SOFTWARE\\Apple Computer, Inc.\\iTunes\\", L"InstallDir");
}

static bool prependPath(const wstring& directoryToPrepend)
{
    wstring pathVariable = L"PATH";
    wstring oldPath = copyEnvironmentVariable(pathVariable);
    wstring newPath = directoryToPrepend + L';' + oldPath;
    return ::SetEnvironmentVariableW(pathVariable.c_str(), newPath.c_str());
}
#endif

static int fatalError(const wstring& programName, const wstring& message)
{
    wstring caption = programName + L" can't open.";
#if USE_CONSOLE_ENTRY_POINT
    fwprintf(stderr, L"%s\n%s\n", caption.c_str(), message.c_str());
#else
    ::MessageBoxW(0, message.c_str(), caption.c_str(), MB_ICONERROR);
#endif
    return 1;
}

static bool directoryExists(const wstring& path)
{
    DWORD attrib = ::GetFileAttributes(path.c_str());

    return ((attrib != INVALID_FILE_ATTRIBUTES) && (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool modifyPath(const wstring& programName)
{
#ifdef WIN_CAIRO

    wstring pathWinCairo = copyEnvironmentVariable(L"WEBKIT_LIBRARIES");
    if (!directoryExists(pathWinCairo))
        return true;
#if defined(_M_X64)
    pathWinCairo += L"\\bin64";
#else
    pathWinCairo += L"\\bin32";
#endif
    if (!SetDllDirectory(pathWinCairo.c_str())) {
        fatalError(programName, L"Failed to SetDllDirectory");
        return false;
    }
    return true;

#else
    auto modifyPathWith = [&] (const wstring& pathPrefix) {
        if (!prependPath(pathPrefix)) {
            fatalError(programName, L"Failed to modify PATH environment variable.");
            return false;
        }
        return true;
    };

    const wstring& applicationSupportPathPrefix = appleApplicationSupportDirectory();
    if (directoryExists(applicationSupportPathPrefix))
        return modifyPathWith(applicationSupportPathPrefix);

    const wstring& iTunesPathPrefix = iTunesDirectory();
    if (directoryExists(iTunesPathPrefix))
        return modifyPathWith(iTunesPathPrefix);

    fatalError(programName, L"Couldn't find path to Apple Application Support (AAS) or iTunes via the registry.  Do you have iTunes installed?");
    return false;
#endif
}

static wstring getLastErrorString(HRESULT hr)
{
    static const DWORD kFlags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    static const size_t bufSize = 4096;

    wchar_t errorMessage[bufSize];
    DWORD len = ::FormatMessageW(kFlags, 0, hr, 0, errorMessage, bufSize, 0);
    if (len >= bufSize)
        len = bufSize - 1;

    errorMessage[len] = 0;

    return errorMessage;
}

#if USE_CONSOLE_ENTRY_POINT
int main(int argc, const char* argv[])
#else
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpstrCmdLine, _In_ int nCmdShow)
#endif
{
#ifdef _CRTDBG_MAP_ALLOC
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#endif

    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);

    enableTerminationOnHeapCorruption();

    // Get the path of our executable.
    wchar_t exePath[MAX_PATH];
    if (!::GetModuleFileNameW(0, exePath, _countof(exePath)))
        return fatalError(L"Unknown Program", L"Failed to determine name of executable: " + getLastErrorString(::GetLastError()));

    ::PathRemoveExtensionW(exePath);

    wstring programName = ::PathFindFileNameW(exePath);

    if (!modifyPath(programName))
        return 1;

    // Load our corresponding DLL.
    wstring dllName = programName + L"Lib.dll";
    if (!::PathRemoveFileSpecW(exePath))
        return fatalError(programName, L"::PathRemoveFileSpecW failed: " + getLastErrorString(::GetLastError()));
    if (!::PathAppendW(exePath, dllName.c_str()))
        return fatalError(programName, L"::PathAppendW failed: " + getLastErrorString(::GetLastError()));
    HMODULE module = ::LoadLibraryW(exePath);
    if (!module)
        return fatalError(programName, L"::LoadLibraryW failed: \npath=" + wstring(exePath) + L"\n" + getLastErrorString(::GetLastError()));

#if USE_CONSOLE_ENTRY_POINT
    typedef int (WINAPI*EntryPoint)(int, const char*[]);
#if defined _M_AMD64 || defined _WIN64
    const char* entryPointName = "dllLauncherEntryPoint";
#else
    const char* entryPointName = "_dllLauncherEntryPoint@8";
#endif
#else
    typedef int (WINAPI*EntryPoint)(HINSTANCE, HINSTANCE, LPWSTR, int);
#if defined _M_AMD64 || defined _WIN64
    const char* entryPointName = "dllLauncherEntryPoint";
#else
    const char* entryPointName = "_dllLauncherEntryPoint@16";
#endif
#endif

    EntryPoint entryPoint = reinterpret_cast<EntryPoint>(::GetProcAddress(module, entryPointName));
    if (!entryPoint)
        return fatalError(programName, L"Failed to find dllLauncherEntryPoint function: " + getLastErrorString(::GetLastError()));

#if USE_CONSOLE_ENTRY_POINT
    return entryPoint(argc, argv);
#else
    return entryPoint(hInstance, hPrevInstance, lpstrCmdLine, nCmdShow);
#endif
}
