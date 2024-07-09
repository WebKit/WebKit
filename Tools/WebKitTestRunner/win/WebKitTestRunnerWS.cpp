/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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

#include <iostream>
#include <string>
#include <windows.h>

#pragma comment(lib, "user32")

#define DESKTOP_NAME L"Default"

bool replace(std::wstring& str, const std::wstring& from, const std::wstring& to)
{
    const auto pos = str.find(from);
    const int len = from.length();
    if (pos == std::wstring::npos)
        return false;
    str.replace(pos, len, to);
    return true;
}

bool containsShowWindowOption(std::wstring& cmd)
{
    return cmd.find(L"--show-window") != std::wstring::npos;
}

int wmain()
{
    std::wstring cmd = GetCommandLine();
    if (!replace(cmd, L"WebKitTestRunnerWS", L"WebKitTestRunner")) {
        std::wcout << L"WebKitTestRunnerWS not found: " << cmd << std::endl;
        return EXIT_FAILURE;
    }

    std::wstring winStaName = L"WebKit_" + std::to_wstring(GetCurrentProcessId());
    HDESK desktop = nullptr;
    if (!containsShowWindowOption(cmd)) {
        HWINSTA windowStation = CreateWindowStation(winStaName.data(), CWF_CREATE_ONLY, GENERIC_ALL, nullptr);
        if (windowStation) {
            if (SetProcessWindowStation(windowStation)) {
                desktop = CreateDesktop(DESKTOP_NAME, nullptr, nullptr, 0, GENERIC_ALL, nullptr);
                if (!desktop)
                    std::wcout << L"CreateDesktop failed with error: " << GetLastError() << std::endl;
            } else
                std::wcout << L"SetProcessWindowStation failed with error: " << GetLastError() << std::endl;
        }
    }

    std::wstring desktopName = winStaName + L'\\' + DESKTOP_NAME;
    PROCESS_INFORMATION processInformation { };
    STARTUPINFO startupInfo { };
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.lpDesktop = desktop ? desktopName.data() : nullptr;
    if (!CreateProcess(0, cmd.data(), nullptr, nullptr, true, 0, 0, 0, &startupInfo, &processInformation)) {
        std::wcout << L"CreateProcess failed: " << GetLastError() << std::endl;
        return EXIT_FAILURE;
    }
    WaitForSingleObject(processInformation.hProcess, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(processInformation.hProcess, &exitCode);
    return exitCode;
}
