/*
 * Copyright (C) 2006, 2008, 2013-2015 Apple Inc.  All rights reserved.
 * Copyright (C) 2009, 2011 Brent Fulgham.  All rights reserved.
 * Copyright (C) 2009, 2010, 2011 Appcelerator, Inc. All rights reserved.
 * Copyright (C) 2013 Alex Christensen. All rights reserved.
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

#include "stdafx.h"
#include "Common.h"

#include "MiniBrowserLibResource.h"
#include "MiniBrowserReplace.h"
#include <dbghelp.h>
#include <shlobj.h>
#include <wtf/StdLibExtras.h>

// Global Variables:
HINSTANCE hInst;

// Support moving the transparent window
POINT s_windowPosition = { 100, 100 };
SIZE s_windowSize = { 500, 200 };

namespace WebCore {
float deviceScaleFactorForWindow(HWND);
}

void computeFullDesktopFrame()
{
    RECT desktop;
    if (!::SystemParametersInfo(SPI_GETWORKAREA, 0, static_cast<void*>(&desktop), 0))
        return;

    float scaleFactor = WebCore::deviceScaleFactorForWindow(nullptr);

    s_windowPosition.x = 0;
    s_windowPosition.y = 0;
    s_windowSize.cx = scaleFactor * (desktop.right - desktop.left);
    s_windowSize.cy = scaleFactor * (desktop.bottom - desktop.top);
}

BOOL WINAPI DllMain(HINSTANCE dllInstance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        hInst = dllInstance;

    return TRUE;
}

bool getAppDataFolder(_bstr_t& directory)
{
    wchar_t appDataDirectory[MAX_PATH];
    if (FAILED(SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, 0, appDataDirectory)))
        return false;

    wchar_t executablePath[MAX_PATH];
    if (!::GetModuleFileNameW(0, executablePath, MAX_PATH))
        return false;

    ::PathRemoveExtensionW(executablePath);

    directory = _bstr_t(appDataDirectory) + L"\\" + ::PathFindFileNameW(executablePath);

    return true;
}

void createCrashReport(EXCEPTION_POINTERS* exceptionPointers)
{
    _bstr_t directory;

    if (!getAppDataFolder(directory))
        return;

    if (::SHCreateDirectoryEx(0, directory, 0) != ERROR_SUCCESS
        && ::GetLastError() != ERROR_FILE_EXISTS
        && ::GetLastError() != ERROR_ALREADY_EXISTS)
        return;

    std::wstring fileName = std::wstring(static_cast<const wchar_t*>(directory)) + L"\\CrashReport.dmp";
    HANDLE miniDumpFile = ::CreateFile(fileName.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (miniDumpFile && miniDumpFile != INVALID_HANDLE_VALUE) {

        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = ::GetCurrentThreadId();
        mdei.ExceptionPointers  = exceptionPointers;
        mdei.ClientPointers = 0;

#ifdef _DEBUG
        MINIDUMP_TYPE dumpType = MiniDumpWithFullMemory;
#else
        MINIDUMP_TYPE dumpType = MiniDumpNormal;
#endif

        ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), miniDumpFile, dumpType, &mdei, 0, 0);
        ::CloseHandle(miniDumpFile);
        processCrashReport(fileName.c_str());
    }
}

struct AuthDialogData {
    std::wstring& username;
    std::wstring& password;
};

static INT_PTR CALLBACK authDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    AuthDialogData& data = *reinterpret_cast<AuthDialogData*>(GetWindowLongPtr(hDlg, DWLP_USER));
    switch (message) {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, DWLP_USER, lParam);
        return TRUE;

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDOK: {
            TCHAR str[256];
            int strLen = GetWindowText(GetDlgItem(hDlg, IDC_AUTH_USER), str, WTF_ARRAY_LENGTH(str)-1);
            str[strLen] = 0;
            data.username = str;

            strLen = GetWindowText(GetDlgItem(hDlg, IDC_AUTH_PASSWORD), str, WTF_ARRAY_LENGTH(str)-1);
            str[strLen] = 0;
            data.password = str;

            EndDialog(hDlg, true);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, false);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

HRESULT displayAuthDialog(HWND hwnd, std::wstring& username, std::wstring& password)
{
    AuthDialogData data { username, password };
    auto result = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_AUTH), hwnd, authDialogProc, reinterpret_cast<LPARAM>(&data));
    return result > 0 ? S_OK : E_FAIL;
}

CommandLineOptions parseCommandLine()
{
    CommandLineOptions options;

    int argc = 0;
    WCHAR** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        if (!wcsicmp(argv[i], L"--transparent"))
            options.usesLayeredWebView = true;
        else if (!wcsicmp(argv[i], L"--desktop"))
            options.useFullDesktop = true;
        else if (!wcsicmp(argv[i], L"--performance"))
            options.pageLoadTesting = true;
        else if (!wcsicmp(argv[i], L"--wk1") || !wcsicmp(argv[i], L"--legacy"))
            options.windowType = MainWindow::BrowserWindowType::WebKitLegacy;
#if ENABLE(WEBKIT)
        else if (!wcsicmp(argv[i], L"--wk2") || !wcsicmp(argv[i], L"--webkit"))
            options.windowType = MainWindow::BrowserWindowType::WebKit;
#endif
        else if (!options.requestedURL)
            options.requestedURL = argv[i];
    }

    return options;
}
