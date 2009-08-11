/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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
    if (RegQueryValueEx(key, valueName, 0, &type, reinterpret_cast<LPBYTE>(buffer), &bufferSize) != ERROR_SUCCESS) {
        free(buffer);
        return 0;
    }

    return buffer;
}

static TCHAR* getInstalledWebKitDirectory()
{
    LPCTSTR installPathKeyString = TEXT("SOFTWARE\\Apple Computer, Inc.\\Safari");
    LPCTSTR installPathWin64KeyString = TEXT("SOFTWARE\\Wow6432Node\\Apple Computer, Inc.\\Safari");
    HKEY installPathKey = 0;
    LONG error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, installPathKeyString, 0, KEY_READ, &installPathKey);
    if (error != ERROR_SUCCESS)
        error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, installPathWin64KeyString, 0, KEY_READ, &installPathKey);
    if (error != ERROR_SUCCESS) {
        LOG_WARNING(TEXT("Failed to open registry key %s\n"), installPathKeyString);
        return 0;
    }
    LPTSTR webKitPath = getStringValue(installPathKey, TEXT("InstallDir"));
    RegCloseKey(installPathKey);
    if (!webKitPath) {
        LOG_WARNING(TEXT("Couldn't retrieve value for registry key %s\n"), installPathKeyString);
        return 0;
    }
    return webKitPath;
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
        TEXT("mkdir 2>NUL \"%%TMP%%\\WebKitNightly\\JavaScriptCore.resources\""),
        TEXT("xcopy /y /i /d \"%sSafari.exe\" \"%%TMP%%\\WebKitNightly\""),
        TEXT("if exist \"%sSafari.dll\" xcopy /y /i /d \"%sSafari.dll\" \"%%TMP%%\\WebKitNightly\""),
        TEXT("xcopy /y /i /d /e \"%sSafari.resources\" \"%%TMP%%\\WebKitNightly\\Safari.resources\""),
        TEXT("xcopy /y /i /d /e \"%splugins\" \"%%TMP%%\\WebKitNightly\\plugins\""),
        TEXT("xcopy /y /i /d WebKit.dll \"%%TMP%%\\WebKitNightly\""),
        TEXT("xcopy /y /i /d WebKit.pdb \"%%TMP%%\\WebKitNightly\""),
        TEXT("xcopy /y /i /d /e WebKit.resources \"%%TMP%%\\WebKitNightly\\WebKit.resources\""),
        TEXT("xcopy /y /i /d JavaScriptCore.dll \"%%TMP%%\\WebKitNightly\""),
        TEXT("xcopy /y /i /d JavaScriptCore.pdb \"%%TMP%%\\WebKitNightly\""),
        TEXT("xcopy /y /i /d /e JavaScriptCore.resources \"%%TMP%%\\WebKitNightly\\JavaScriptCore.resources\""),
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
        _tprintf(lines[i], path, path);
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
