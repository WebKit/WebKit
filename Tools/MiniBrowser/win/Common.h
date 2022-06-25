/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#pragma once

#include "stdafx.h"
#include "MainWindow.h"

enum class BrowserWindowType {
    WebKit,
    WebKitLegacy
};

struct CommandLineOptions {
    bool usesLayeredWebView { };
    bool useFullDesktop { };
    BrowserWindowType windowType;
    _bstr_t requestedURL;

    CommandLineOptions()
#if ENABLE(WEBKIT)
        : windowType(BrowserWindowType::WebKit)
#else
        : windowType(BrowserWindowType::WebKitLegacy)
#endif
    {
    }
};

struct Credential {
    std::wstring username;
    std::wstring password;
};

struct ProxySettings {
    bool enable { true };
    bool custom { false };
    std::wstring url;
    std::wstring excludeHosts;
};

void computeFullDesktopFrame();
bool getAppDataFolder(_bstr_t& directory);
CommandLineOptions parseCommandLine();
void createCrashReport(EXCEPTION_POINTERS*);
std::optional<Credential> askCredential(HWND, const std::wstring& realm);
bool askProxySettings(HWND, ProxySettings&);

bool askServerTrustEvaluation(HWND, const std::wstring& text);
std::wstring replaceString(std::wstring src, const std::wstring& oldValue, const std::wstring& newValue);

extern HINSTANCE hInst;
extern POINT s_windowPosition;
extern SIZE s_windowSize;
