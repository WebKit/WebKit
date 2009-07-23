/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "resource.h"
#include <shlwapi.h>
#include <tchar.h>
#include <windows.h>

static LPTSTR getStringValue(HKEY key, LPCTSTR valueName)
{
    DWORD type = 0;
    DWORD bufferSize = 0;
    if (RegQueryValueEx(key, valueName, 0, &type, 0, &bufferSize) != ERROR_SUCCESS || type != REG_SZ)
        return 0;

    LPTSTR buffer = static_cast<LPTSTR>(malloc(bufferSize));
    if (RegQueryValueEx(key, valueName, 0, &type, reinterpret_cast<LPBYTE>(buffer), &bufferSize) != ERROR_SUCCESS) {
        free(buffer);
        return 0;
    }

    return buffer;
}

static LPTSTR applePathFromRegistry(LPCTSTR key, LPCTSTR value)
{
    HKEY applePathKey = 0;
    LONG error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &applePathKey);
    if (error != ERROR_SUCCESS)
        return 0;
    LPTSTR path = getStringValue(applePathKey, value);
    RegCloseKey(applePathKey);
    return path;
}

static LPTSTR safariInstallDir()
{
    return applePathFromRegistry(TEXT("SOFTWARE\\Apple Computer, Inc.\\Safari"), TEXT("InstallDir"));
}

static LPTSTR safariBrowserExe()
{
    return applePathFromRegistry(TEXT("SOFTWARE\\Apple Computer, Inc.\\Safari"), TEXT("BrowserExe"));
}

int APIENTRY _tWinMain(HINSTANCE instance, HINSTANCE, LPTSTR commandLine, int)
{
    LPTSTR path = safariInstallDir();
    LPTSTR browserExe = safariBrowserExe();
    if (!path || !browserExe) {
        MessageBox(0, TEXT("Safari must be installed to run a WebKit nightly. You can download Safari from http://www.apple.com/safari/download"), TEXT("Safari not found"), MB_ICONSTOP);
        return 1;
    }

    // Set WEBKITNIGHTLY environment variable to point to the nightly bits
    TCHAR exePath[MAX_PATH];
    if (!GetModuleFileName(0, exePath, ARRAYSIZE(exePath)))
        return 1;
    if (!PathRemoveFileSpec(exePath))
        return 1;
    SetEnvironmentVariable(TEXT("WEBKITNIGHTLY"), exePath);

    // Launch Safari as a child process
    STARTUPINFO startupInfo = {0};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {0};
    if (!CreateProcess(browserExe, commandLine, 0, 0, FALSE, NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT, 0, path, &startupInfo, &processInfo))
        MessageBox(0, TEXT("Safari could not be launched. Please make sure you have the latest version of Safari installed and try again. You can download Safari from http://www.apple.com/safari/download"), TEXT("Safari launch failed"), MB_ICONSTOP);

    free(browserExe);
    free(path);
    return 0;
}
