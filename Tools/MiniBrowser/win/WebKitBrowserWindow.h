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

#include "BrowserWindow.h"
#include "Common.h"
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WebKit2_C.h>

class WebKitBrowserWindow : public BrowserWindow {
public:
    static Ref<BrowserWindow> create(HWND mainWnd, HWND urlBarWnd, bool useLayeredWebView = false, bool pageLoadTesting = false);

private:
    WebKitBrowserWindow(HWND mainWnd, HWND urlBarWnd);

    HRESULT init() override;
    HWND hwnd() override;

    HRESULT loadHTMLString(const BSTR&) override;
    HRESULT loadURL(const BSTR& url) override;
    void navigateForwardOrBackward(UINT menuID) override;
    void navigateToHistory(UINT menuID) override;
    void setPreference(UINT menuID, bool enable) override;

    void print() override;
    void launchInspector() override;
    void openProxySettings() override;

    _bstr_t userAgent() override;
    void setUserAgent(_bstr_t&) override;

    void showLayerTree() override;
    void updateStatistics(HWND dialog) override;

    void resetZoom() override;
    void zoomIn() override;
    void zoomOut() override;

    void updateProxySettings();

    static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*);
    static void didCommitNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*);
    static void didReceiveAuthenticationChallenge(WKPageRef, WKAuthenticationChallengeRef, const void*);

    WKRetainPtr<WKContextRef> m_context;
    WKRetainPtr<WKViewRef> m_view;
    HWND m_hMainWnd { nullptr };
    HWND m_urlBarWnd { nullptr };
    ProxySettings m_proxy { };
};
