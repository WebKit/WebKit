/*
 * Copyright (C) 2014-2015 Apple Inc.  All rights reserved.
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
#include "PageLoadTestClient.h"
#include <WebKitLegacy/WebKit.h>
#include <comip.h>
#include <memory>
#include <vector>

typedef _com_ptr_t<_com_IIID<IWebFrame, &__uuidof(IWebFrame)>> IWebFramePtr;
typedef _com_ptr_t<_com_IIID<IWebView, &__uuidof(IWebView)>> IWebViewPtr;
typedef _com_ptr_t<_com_IIID<IWebViewPrivate2, &__uuidof(IWebViewPrivate2)>> IWebViewPrivatePtr;
typedef _com_ptr_t<_com_IIID<IWebFrameLoadDelegate, &__uuidof(IWebFrameLoadDelegate)>> IWebFrameLoadDelegatePtr;
typedef _com_ptr_t<_com_IIID<IWebHistory, &__uuidof(IWebHistory)>> IWebHistoryPtr;
typedef _com_ptr_t<_com_IIID<IWebHistoryItem, &__uuidof(IWebHistoryItem)>> IWebHistoryItemPtr;
typedef _com_ptr_t<_com_IIID<IWebPreferences, &__uuidof(IWebPreferences)>> IWebPreferencesPtr;
typedef _com_ptr_t<_com_IIID<IWebPreferencesPrivate3, &__uuidof(IWebPreferencesPrivate3)>> IWebPreferencesPrivatePtr;
typedef _com_ptr_t<_com_IIID<IWebUIDelegate, &__uuidof(IWebUIDelegate)>> IWebUIDelegatePtr;
typedef _com_ptr_t<_com_IIID<IAccessibilityDelegate, &__uuidof(IAccessibilityDelegate)>> IAccessibilityDelegatePtr;
typedef _com_ptr_t<_com_IIID<IWebInspector, &__uuidof(IWebInspector)>> IWebInspectorPtr;
typedef _com_ptr_t<_com_IIID<IWebCoreStatistics, &__uuidof(IWebCoreStatistics)>> IWebCoreStatisticsPtr;
typedef _com_ptr_t<_com_IIID<IWebCache, &__uuidof(IWebCache)>> IWebCachePtr;
typedef _com_ptr_t<_com_IIID<IWebResourceLoadDelegate, &__uuidof(IWebResourceLoadDelegate)>> IWebResourceLoadDelegatePtr;
typedef _com_ptr_t<_com_IIID<IWebDownloadDelegate, &__uuidof(IWebDownloadDelegate)>> IWebDownloadDelegatePtr;
typedef _com_ptr_t<_com_IIID<IWebFramePrivate, &__uuidof(IWebFramePrivate)>> IWebFramePrivatePtr;

class WebKitLegacyBrowserWindow : public BrowserWindow {
public:
    static Ref<BrowserWindow> create(HWND mainWnd, HWND urlBarWnd, bool useLayeredWebView = false, bool pageLoadTesting = false);

private:
    friend class AccessibilityDelegate;
    friend class MiniBrowserWebHost;
    friend class PrintWebUIDelegate;
    friend class WebDownloadDelegate;
    friend class ResourceLoadDelegate;
    friend class PageLoadTestClient;

    ULONG AddRef();
    ULONG Release();

    HRESULT init();
    HRESULT prepareViews(HWND mainWnd, const RECT& clientRect);

    HRESULT loadHTMLString(const BSTR&);
    HRESULT loadURL(const BSTR& passedURL);

    void showLastVisitedSites(IWebView&);
    void launchInspector();
    void openProxySettings();
    void navigateForwardOrBackward(UINT menuID);
    void navigateToHistory(UINT menuID);
    void exitProgram();
    bool seedInitialDefaultPreferences();
    bool setToDefaultPreferences();

    HRESULT setFrameLoadDelegate(IWebFrameLoadDelegate*);
    HRESULT setFrameLoadDelegatePrivate(IWebFrameLoadDelegatePrivate*);
    HRESULT setUIDelegate(IWebUIDelegate*);
    HRESULT setAccessibilityDelegate(IAccessibilityDelegate*);
    HRESULT setResourceLoadDelegate(IWebResourceLoadDelegate*);
    HRESULT setDownloadDelegate(IWebDownloadDelegatePtr);

    IWebPreferencesPtr standardPreferences() { return m_standardPreferences;  }
    IWebPreferencesPrivatePtr privatePreferences() { return m_prefsPrivate; }
    IWebFramePtr mainFrame();
    IWebCoreStatisticsPtr statistics() { return m_statistics; }
    IWebCachePtr webCache() { return m_webCache;  }
    IWebViewPtr webView() { return m_webView; }

    bool hasWebView() const { return !!m_webView; }
    bool usesLayeredWebView() const { return m_useLayeredWebView; }
    bool goBack();
    bool goForward();

    void setUserAgent(_bstr_t& customUAString);
    _bstr_t userAgent();

    PageLoadTestClient& pageLoadTestClient() { return *m_pageLoadTestClient; }

    void resetZoom();
    void zoomIn();
    void zoomOut();

    void showLayerTree();

    HWND hwnd() { return m_viewWnd; }

    void print();
    void updateStatistics(HWND dialog);
    void setPreference(UINT menuID, bool enable);

    WebKitLegacyBrowserWindow(HWND mainWnd, HWND urlBarWnd, bool useLayeredWebView, bool pageLoadTesting);
    void subclassForLayeredWindow();
    bool setCacheFolder();

    std::vector<IWebHistoryItemPtr> m_historyItems;

    IWebViewPtr m_webView;
    IWebViewPrivatePtr m_webViewPrivate;

    IWebHistoryPtr m_webHistory;
    IWebInspectorPtr m_inspector;
    IWebPreferencesPtr m_standardPreferences;
    IWebPreferencesPrivatePtr m_prefsPrivate;

    IWebFrameLoadDelegatePtr m_frameLoadDelegate;
    IWebUIDelegatePtr m_uiDelegate;
    IAccessibilityDelegatePtr m_accessibilityDelegate;
    IWebResourceLoadDelegatePtr m_resourceLoadDelegate;
    IWebDownloadDelegatePtr m_downloadDelegate;

    IWebCoreStatisticsPtr m_statistics;
    IWebCachePtr m_webCache;

    HWND m_hMainWnd { nullptr };
    HWND m_hURLBarWnd { nullptr };
    HWND m_viewWnd { nullptr };

    bool m_useLayeredWebView;

    std::unique_ptr<PageLoadTestClient> m_pageLoadTestClient;
};
