/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "resource.h"

#include <shlwapi.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>

#define LOG(header, ...) \
    do { \
        _ftprintf(stderr, header); \
        _ftprintf(stderr, __VA_ARGS__); \
    } while (0)
#define LOG_WARNING(...) LOG(TEXT("WARNING: "), __VA_ARGS__)
#define LOG_ERROR(...) LOG(TEXT("ERROR: "), __VA_ARGS__)

static TCHAR* getStringValue(HKEY key, LPCTSTR valueName)
{
    DWORD type = 0;
    DWORD bufferSize = 0;
    if (RegQueryValueEx(key, valueName, 0, &type, 0, &bufferSize) != ERROR_SUCCESS || type != REG_SZ)
        return 0;

    TCHAR* buffer = (TCHAR*)malloc(bufferSize);
    if (RegQueryValueEx(key, 0, 0, &type, reinterpret_cast<LPBYTE>(buffer), &bufferSize) != ERROR_SUCCESS) {
        free(buffer);
        return 0;
    }

    return buffer;
}

static LPOLESTR getWebViewCLSID()
{
    LPCTSTR webViewProgID = TEXT("WebKit.WebView");

    CLSID clsid = CLSID_NULL;
    HRESULT hr = CLSIDFromProgID(webViewProgID, &clsid);
    if (FAILED(hr)) {
        LOG_WARNING(TEXT("Failed to get CLSID for %s\n"), webViewProgID);
        return 0;
    }

    LPOLESTR clsidString = 0;
    if (FAILED(StringFromCLSID(clsid, &clsidString))) {
        LOG_WARNING(TEXT("Failed to get string representation of CLSID for WebView\n"));
        return 0;
    }

    return clsidString;
}

static TCHAR* getInstalledWebKitDirectory()
{
    LPCTSTR keyPrefix = TEXT("SOFTWARE\\Classes\\CLSID\\");
    LPCTSTR keySuffix = TEXT("\\InprocServer32");

    LPOLESTR clsid = getWebViewCLSID();
    if (!clsid)
        return 0;

    size_t keyBufferLength = _tcslen(keyPrefix) + _tcslen(clsid) + _tcslen(keySuffix) + 1;
    TCHAR* keyString = (TCHAR*)malloc(keyBufferLength * sizeof(TCHAR));

    int ret = _sntprintf_s(keyString, keyBufferLength, keyBufferLength - 1, TEXT("%s%s%s"), keyPrefix, clsid, keySuffix);
    CoTaskMemFree(clsid);
    if (ret == -1) {
        LOG_WARNING(TEXT("Failed to construct InprocServer32 key\n"));
        return 0;
    }

    HKEY serverKey = 0;
    LONG error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyString, 0, KEY_READ, &serverKey);
    free(keyString);
    if (error != ERROR_SUCCESS) {
        LOG_WARNING(TEXT("Failed to open registry key %s\n"), keyString);
        return 0;
    }

    TCHAR* webKitPath = getStringValue(serverKey, 0);
    RegCloseKey(serverKey);
    if (!webKitPath) {
        LOG_WARNING(TEXT("Couldn't retrieve value for registry key %s\n"), keyString);
        return 0;
    }

    TCHAR* startOfFileName = PathFindFileName(webKitPath);
    if (startOfFileName == webKitPath) {
        LOG_WARNING(TEXT("Couldn't find filename from path %s\n"), webKitPath);
        free(webKitPath);
        return 0;
    }

    *startOfFileName = '\0';
    return webKitPath;
}

static char* copyManifest(HMODULE module, LPCTSTR id)
{
    HRSRC resHandle = FindResource(module, id, MAKEINTRESOURCE(RT_MANIFEST));
    if (!resHandle)
        return 0;
    DWORD manifestSize = SizeofResource(module, resHandle);
    if (!manifestSize)
        return 0;
    HGLOBAL resData = LoadResource(module, resHandle);
    if (!resData)
        return 0;
    void* data = LockResource(resData);
    if (!data)
        return 0;

    char* dataCopy = static_cast<char*>(malloc(manifestSize + 1));
    if (!dataCopy)
        return 0;
    memcpy(dataCopy, data, manifestSize);
    dataCopy[manifestSize] = 0;
    return dataCopy;
}

static void replaceManifest()
{
    TCHAR safariPath[MAX_PATH];
    ::ExpandEnvironmentStrings(TEXT("%TMP%\\WebKitNightly\\Safari.exe"), safariPath, ARRAYSIZE(safariPath));
    
    // get the existing manifest out of Safari.exe
    HMODULE safariModule = LoadLibraryEx(safariPath, 0, LOAD_LIBRARY_AS_DATAFILE);
    if (!safariModule)
        return;
    char* safariManifest = copyManifest(safariModule, MAKEINTRESOURCE(1));
    FreeLibrary(safariModule);
    if (!safariManifest)
        return;

    // see if the existing Safari manifest contains registry free COM info
    // (we only need to update if it is not registry free COM-aware)
    bool needsUpdate = !strstr(safariManifest, "<comClass");
    free(safariManifest);
    if (!needsUpdate)
        return;

    // replace the manifest with the extra manifest stashed in FindSafar.exe (ID 100)
    char* replacementManifest = copyManifest(0, MAKEINTRESOURCE(IDR_SAFARI_MANIFEST));
    if (!replacementManifest)
        return;
    if (HANDLE h = BeginUpdateResource(safariPath, FALSE)) {
        UpdateResource(h, MAKEINTRESOURCE(RT_MANIFEST), MAKEINTRESOURCE(1), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), replacementManifest, strlen(replacementManifest));
        EndUpdateResource(h, FALSE);
    }

    free(replacementManifest);
}

int _tmain(int argc, TCHAR* argv[])
{
    TCHAR* path = getInstalledWebKitDirectory();
    if (!path) {
        LOG_ERROR(TEXT("Couldn't determine installed Safari path\n"));
        return 1;
    }

    bool printLauncher = false;
    bool printEnvironment = false;
    bool debugger = false;
    bool updateManifest = false;

    for (int i = 1; i < argc; ++i) {
        if (!_tcscmp(argv[i], TEXT("/printSafariLauncher"))) {
            printLauncher = true;
            continue;
        }
        if (!_tcscmp(argv[i], TEXT("/printSafariEnvironment"))) {
            printEnvironment = true;
            continue;
        }
        if (!_tcscmp(argv[i], TEXT("/debugger"))) {
            debugger = true;
            continue;
        }

        if (!_tcscmp(argv[i], TEXT("/updateManifest"))) {
            updateManifest = true;
            continue;
        }
    }

    if (updateManifest) {
        replaceManifest();
        return 0;
    }

    // printLauncher is inclusive of printEnvironment, so do not
    // leave both enabled:
    if (printLauncher && printEnvironment)
        printEnvironment = false;

    if (!printLauncher && !printEnvironment) {
        _tprintf(TEXT("%s\n"), path);
        free(path);
        return 0;
    }

    LPCTSTR lines[] = {
        TEXT("@echo off"),
        TEXT("del /s /q \"%%TMP%%\\WebKitNightly\""),
        TEXT("mkdir 2>NUL \"%%TMP%%\\WebKitNightly\\Safari.resources\""),
        TEXT("mkdir 2>NUL \"%%TMP%%\\WebKitNightly\\WebKit.resources\""),
        TEXT("xcopy /y /i /d \"%sSafari.exe\" \"%%TMP%%\\WebKitNightly\""),
        TEXT("xcopy /y /i /d /e \"%sSafari.resources\" \"%%TMP%%\\WebKitNightly\\Safari.resources\""),
        TEXT("xcopy /y /i /d /e \"%splugins\" \"%%TMP%%\\WebKitNightly\\plugins\""),
        TEXT("xcopy /y /i /d WebKit.dll \"%%TMP%%\\WebKitNightly\""),
        TEXT("xcopy /y /i /d WebKit.pdb \"%%TMP%%\\WebKitNightly\""),
        TEXT("xcopy /y /i /d /e WebKit.resources \"%%TMP%%\\WebKitNightly\\WebKit.resources\""),
        TEXT("FindSafari.exe /updateManifest"),
        TEXT("set PATH=%%CD%%;%s;%%PATH%%"),
    };

    LPCTSTR command = TEXT("\"%TMP%\\WebKitNightly\\Safari.exe\"");

    LPCTSTR launchLines[] = {
        TEXT("%s"),
    };

    LPCTSTR debuggerLines[] = {
        TEXT("if exist \"%%DevEnvDir%%\\VCExpress.exe\" ("),
        TEXT("\"%%DevEnvDir%%\\VCExpress.exe\" /debugExe %s"),
        TEXT(") else ("),
        TEXT("\"%%DevEnvDir%%\\devenv.exe\" /debugExe %s"),
        TEXT(")"),
    };

    for (int i = 0; i < ARRAYSIZE(lines); ++i) {
        _tprintf(lines[i], path);
        _tprintf(TEXT("\n"));
    }

    LPCTSTR* endLines = debugger ? debuggerLines : launchLines;

    // Don't print launch command if we just want the environment set up...
    if (!printEnvironment) {
       for (unsigned i = 0; i < (debugger ? ARRAYSIZE(debuggerLines) : ARRAYSIZE(launchLines)); ++i) {
           _tprintf(endLines[i], command);
           _tprintf(TEXT("\n"));
       }
    }
    
    free(path);
    return 0;
}
