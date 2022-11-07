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
#include <WebKitLegacy/WebKit.h>
#include <comip.h>
#include <memory>
#include <vector>
#include <wtf/Ref.h>

_COM_SMARTPTR_TYPEDEF(IAccessibilityDelegate, IID_IAccessibilityDelegate);
_COM_SMARTPTR_TYPEDEF(IWebBackForwardList, IID_IWebBackForwardList);
_COM_SMARTPTR_TYPEDEF(IWebCache, IID_IWebCache);
_COM_SMARTPTR_TYPEDEF(IWebCoreStatistics, IID_IWebCoreStatistics);
_COM_SMARTPTR_TYPEDEF(IWebDataSource, IID_IWebDataSource);
_COM_SMARTPTR_TYPEDEF(IWebDownloadDelegate, IID_IWebDownloadDelegate);
_COM_SMARTPTR_TYPEDEF(IWebFrame, IID_IWebFrame);
_COM_SMARTPTR_TYPEDEF(IWebFrame2, IID_IWebFrame2);
_COM_SMARTPTR_TYPEDEF(IWebFrameLoadDelegate, IID_IWebFrameLoadDelegate);
_COM_SMARTPTR_TYPEDEF(IWebFramePrivate, IID_IWebFramePrivate);
_COM_SMARTPTR_TYPEDEF(IWebHistory, IID_IWebHistory);
_COM_SMARTPTR_TYPEDEF(IWebHistoryItem, IID_IWebHistoryItem);
_COM_SMARTPTR_TYPEDEF(IWebHistoryPrivate, IID_IWebHistoryPrivate);
_COM_SMARTPTR_TYPEDEF(IWebIBActions, IID_IWebIBActions);
_COM_SMARTPTR_TYPEDEF(IWebInspector, IID_IWebInspector);
_COM_SMARTPTR_TYPEDEF(IWebKitMessageLoop, IID_IWebKitMessageLoop);
_COM_SMARTPTR_TYPEDEF(IWebMutableURLRequest, IID_IWebMutableURLRequest);
_COM_SMARTPTR_TYPEDEF(IWebNotificationCenter, IID_IWebNotificationCenter);
_COM_SMARTPTR_TYPEDEF(IWebNotificationObserver, IID_IWebNotificationObserver);
_COM_SMARTPTR_TYPEDEF(IWebPreferences, IID_IWebPreferences);
_COM_SMARTPTR_TYPEDEF(IWebPreferencesPrivate3, IID_IWebPreferencesPrivate3);
_COM_SMARTPTR_TYPEDEF(IWebResourceLoadDelegate, IID_IWebResourceLoadDelegate);
_COM_SMARTPTR_TYPEDEF(IWebUIDelegate, IID_IWebUIDelegate);
_COM_SMARTPTR_TYPEDEF(IWebView, IID_IWebView);
_COM_SMARTPTR_TYPEDEF(IWebViewPrivate2, IID_IWebViewPrivate2);
_COM_SMARTPTR_TYPEDEF(IWebViewPrivate3, IID_IWebViewPrivate3);

class WebKitLegacyBrowserWindow : public BrowserWindow {
public:
    static Ref<BrowserWindow> create(BrowserWindowClient&, HWND mainWnd, bool useLayeredWebView = false);

private:
    friend class AccessibilityDelegate;
    friend class MiniBrowserWebHost;
    friend class PrintWebUIDelegate;
    friend class WebDownloadDelegate;
    friend class ResourceLoadDelegate;

    HRESULT init();
    HRESULT prepareViews(HWND mainWnd, const RECT& clientRect);

    void resetFeatureMenu(FeatureType, HMENU, bool resetsSettingsToDefaults) override;

    HRESULT loadURL(const BSTR& passedURL);
    void reload();

    void showLastVisitedSites(IWebView&);
    void launchInspector();
    void openProxySettings();
    void navigateForwardOrBackward(bool forward);
    void navigateToHistory(UINT menuID);
    bool seedInitialDefaultPreferences();
    bool setToDefaultPreferences();

    HRESULT setUIDelegate(IWebUIDelegate*);
    HRESULT setAccessibilityDelegate(IAccessibilityDelegate*);
    HRESULT setResourceLoadDelegate(IWebResourceLoadDelegate*);
    HRESULT setDownloadDelegate(IWebDownloadDelegatePtr);

    IWebPreferencesPtr standardPreferences() { return m_standardPreferences;  }
    IWebPreferencesPrivate3Ptr privatePreferences() { return m_prefsPrivate; }
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

    void resetZoom();
    void zoomIn();
    void zoomOut();

    void clearCookies();
    void clearWebsiteData();

    void showLayerTree();

    HWND hwnd() { return m_viewWnd; }

    void print();
    void updateStatistics(HWND dialog);
    void setPreference(UINT menuID, bool enable);

    WebKitLegacyBrowserWindow(BrowserWindowClient&, HWND mainWnd, bool useLayeredWebView);
    ~WebKitLegacyBrowserWindow();
    void subclassForLayeredWindow();
    bool setCacheFolder();

    BrowserWindowClient& m_client;
    std::vector<IWebHistoryItemPtr> m_historyItems;

    IWebViewPtr m_webView;
    IWebViewPrivate2Ptr m_webViewPrivate;

    IWebHistoryPtr m_webHistory;
    IWebInspectorPtr m_inspector;
    IWebPreferencesPtr m_standardPreferences;
    IWebPreferencesPrivate3Ptr m_prefsPrivate;
    IWebNotificationCenterPtr m_defaultNotificationCenter;
    IWebNotificationObserverPtr m_notificationObserver;

    IWebCoreStatisticsPtr m_statistics;
    IWebCachePtr m_webCache;

    HWND m_hMainWnd { nullptr };
    HWND m_viewWnd { nullptr };

    bool m_useLayeredWebView;
};
