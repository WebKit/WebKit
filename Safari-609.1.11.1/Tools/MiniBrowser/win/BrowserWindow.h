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

#include <windows.h>
#include <wtf/RefCounted.h>

class BrowserWindowClient {
public:
    virtual void progressChanged(double) = 0;
    virtual void progressFinished() = 0;
    virtual void activeURLChanged(std::wstring) = 0;
};

class BrowserWindow : public RefCounted<BrowserWindow> {
public:
    virtual ~BrowserWindow() { };

    virtual HRESULT init() = 0;
    virtual HWND hwnd() = 0;

    virtual HRESULT loadURL(const BSTR& passedURL) = 0;
    virtual void reload() = 0;
    virtual void navigateForwardOrBackward(UINT menuID) = 0;
    virtual void navigateToHistory(UINT menuID) = 0;
    virtual void setPreference(UINT menuID, bool enable) = 0;
    virtual bool usesLayeredWebView() const { return false; }

    virtual void print() = 0;
    virtual void launchInspector() = 0;
    virtual void openProxySettings() = 0;

    virtual _bstr_t userAgent() = 0;
    void setUserAgent(UINT menuID);
    virtual void setUserAgent(_bstr_t& customUAString) = 0;

    virtual void showLayerTree() = 0;
    virtual void updateStatistics(HWND dialog) = 0;

    virtual void resetZoom() = 0;
    virtual void zoomIn() = 0;
    virtual void zoomOut() = 0;
};
