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
#include "MiniBrowser.h"

#include "AccessibilityDelegate.h"
#include "Common.h"
#include "DOMDefaultImpl.h"
#include "MiniBrowserLibResource.h"
#include "MiniBrowserReplace.h"
#include "MiniBrowserWebHost.h"
#include "PrintWebUIDelegate.h"
#include "ResourceLoadDelegate.h"
#include "WebDownloadDelegate.h"
#include <WebKitLegacy/WebKitCOMAPI.h>
#include <wtf/ExportMacros.h>
#include <wtf/Platform.h>

#if USE(CF)
#include <CoreFoundation/CFRunLoop.h>
#endif

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace WebCore {
float deviceScaleFactorForWindow(HWND);
}

static const int maxHistorySize = 10;

typedef _com_ptr_t<_com_IIID<IWebMutableURLRequest, &__uuidof(IWebMutableURLRequest)>> IWebMutableURLRequestPtr;

MiniBrowser::MiniBrowser(HWND mainWnd, HWND urlBarWnd, bool useLayeredWebView, bool pageLoadTesting)
    : m_hMainWnd(mainWnd)
    , m_hURLBarWnd(urlBarWnd)
    , m_useLayeredWebView(useLayeredWebView)
    , m_pageLoadTestClient(std::make_unique<PageLoadTestClient>(this, pageLoadTesting))
{
}

HRESULT MiniBrowser::init()
{
    updateDeviceScaleFactor();

    HRESULT hr = WebKitCreateInstance(CLSID_WebView, 0, IID_IWebView, reinterpret_cast<void**>(&m_webView.GetInterfacePtr()));
    if (FAILED(hr))
        return hr;

    hr = m_webView->QueryInterface(IID_IWebViewPrivate2, reinterpret_cast<void**>(&m_webViewPrivate.GetInterfacePtr()));
    if (FAILED(hr))
        return hr;

    hr = WebKitCreateInstance(CLSID_WebHistory, 0, __uuidof(m_webHistory), reinterpret_cast<void**>(&m_webHistory.GetInterfacePtr()));
    if (FAILED(hr))
        return hr;

    hr = WebKitCreateInstance(CLSID_WebCoreStatistics, 0, __uuidof(m_statistics), reinterpret_cast<void**>(&m_statistics.GetInterfacePtr()));
    if (FAILED(hr))
        return hr;

    hr = WebKitCreateInstance(CLSID_WebCache, 0, __uuidof(m_webCache), reinterpret_cast<void**>(&m_webCache.GetInterfacePtr()));
    if (FAILED(hr))
        return hr;

    if (!seedInitialDefaultPreferences())
        return E_FAIL;

    if (!setToDefaultPreferences())
        return E_FAIL;

    if (!setCacheFolder())
        return E_FAIL;

    auto webHost = new MiniBrowserWebHost(this, m_hURLBarWnd);

    hr = setFrameLoadDelegate(webHost);
    if (FAILED(hr))
        return hr;

    hr = setFrameLoadDelegatePrivate(webHost);
    if (FAILED(hr))
        return hr;

    hr = setUIDelegate(new PrintWebUIDelegate());
    if (FAILED (hr))
        return hr;

    hr = setAccessibilityDelegate(new AccessibilityDelegate());
    if (FAILED (hr))
        return hr;

    hr = setResourceLoadDelegate(new ResourceLoadDelegate(this));
    if (FAILED(hr))
        return hr;

    IWebDownloadDelegatePtr downloadDelegate;
    downloadDelegate.Attach(new WebDownloadDelegate());
    hr = setDownloadDelegate(downloadDelegate);
    if (FAILED(hr))
        return hr;

    RECT clientRect;
    ::GetClientRect(m_hMainWnd, &clientRect);
    if (usesLayeredWebView())
        clientRect = { s_windowPosition.x, s_windowPosition.y, s_windowPosition.x + s_windowSize.cx, s_windowPosition.y + s_windowSize.cy };

    hr = prepareViews(m_hMainWnd, clientRect);
    if (FAILED(hr))
        return hr;

    if (usesLayeredWebView())
        subclassForLayeredWindow();

    return hr;
}

bool MiniBrowser::setCacheFolder()
{
    _bstr_t appDataFolder;
    if (!getAppDataFolder(appDataFolder))
        return false;

    appDataFolder += L"\\cache";
    webCache()->setCacheFolder(appDataFolder);

    return true;
}

HRESULT MiniBrowser::prepareViews(HWND mainWnd, const RECT& clientRect)
{
    if (!m_webView)
        return E_FAIL;

    HRESULT hr = m_webView->setHostWindow(mainWnd);
    if (FAILED(hr))
        return hr;

    hr = m_webView->initWithFrame(clientRect, 0, 0);
    if (FAILED(hr))
        return hr;

    hr = m_webViewPrivate->setTransparent(m_useLayeredWebView);
    if (FAILED(hr))
        return hr;

    hr = m_webViewPrivate->setUsesLayeredWindow(m_useLayeredWebView);
    if (FAILED(hr))
        return hr;

    hr = m_webViewPrivate->viewWindow(&m_viewWnd);

    return hr;
}

HRESULT MiniBrowser::loadHTMLString(const BSTR& str)
{
    IWebFramePtr frame;
    HRESULT hr = m_webView->mainFrame(&frame.GetInterfacePtr());
    if (FAILED(hr))
        return hr;

    frame->loadHTMLString(str, 0);
    return hr;
}

static WNDPROC gDefWebKitProc;

static LRESULT CALLBACK viewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_NCHITTEST:
        constexpr int dragBarHeight = 30;
        RECT window;
        ::GetWindowRect(hWnd, &window);
        // For testing our transparent window, we need a region to use as a handle for
        // dragging. The right way to do this would be to query the web view to see what's
        // under the mouse. However, for testing purposes we just use an arbitrary
        // 30 logical pixel band at the top of the view as an arbitrary gripping location.
        //
        // When we are within this bad, return HT_CAPTION to tell Windows we want to
        // treat this region as if it were the title bar on a normal window.
        int y = HIWORD(lParam);
        float scaledDragBarHeightFactor = dragBarHeight * WebCore::deviceScaleFactorForWindow(hWnd);
        if ((y > window.top) && (y < window.top + scaledDragBarHeightFactor))
            return HTCAPTION;
    }
    return CallWindowProc(gDefWebKitProc, hWnd, message, wParam, lParam);
}

void MiniBrowser::subclassForLayeredWindow()
{
#if defined _M_AMD64 || defined _WIN64
    gDefWebKitProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(m_viewWnd, GWLP_WNDPROC));
    ::SetWindowLongPtr(m_viewWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(viewWndProc));
#else
    gDefWebKitProc = reinterpret_cast<WNDPROC>(::GetWindowLong(m_viewWnd, GWL_WNDPROC));
    ::SetWindowLong(m_viewWnd, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(viewWndProc));
#endif
}

HRESULT MiniBrowser::setFrameLoadDelegate(IWebFrameLoadDelegate* frameLoadDelegate)
{
    m_frameLoadDelegate = frameLoadDelegate;
    return m_webView->setFrameLoadDelegate(frameLoadDelegate);
}

HRESULT MiniBrowser::setFrameLoadDelegatePrivate(IWebFrameLoadDelegatePrivate* frameLoadDelegatePrivate)
{
    return m_webViewPrivate->setFrameLoadDelegatePrivate(frameLoadDelegatePrivate);
}

HRESULT MiniBrowser::setUIDelegate(IWebUIDelegate* uiDelegate)
{
    m_uiDelegate = uiDelegate;
    return m_webView->setUIDelegate(uiDelegate);
}

HRESULT MiniBrowser::setAccessibilityDelegate(IAccessibilityDelegate* accessibilityDelegate)
{
    m_accessibilityDelegate = accessibilityDelegate;
    return m_webView->setAccessibilityDelegate(accessibilityDelegate);
}

HRESULT MiniBrowser::setResourceLoadDelegate(IWebResourceLoadDelegate* resourceLoadDelegate)
{
    m_resourceLoadDelegate = resourceLoadDelegate;
    return m_webView->setResourceLoadDelegate(resourceLoadDelegate);
}

HRESULT MiniBrowser::setDownloadDelegate(IWebDownloadDelegatePtr downloadDelegate)
{
    m_downloadDelegate = downloadDelegate;
    return m_webView->setDownloadDelegate(downloadDelegate);
}

IWebFramePtr MiniBrowser::mainFrame()
{
    IWebFramePtr framePtr;
    m_webView->mainFrame(&framePtr.GetInterfacePtr());
    return framePtr;
}

bool MiniBrowser::seedInitialDefaultPreferences()
{
    IWebPreferencesPtr tmpPreferences;
    if (FAILED(WebKitCreateInstance(CLSID_WebPreferences, 0, IID_IWebPreferences, reinterpret_cast<void**>(&tmpPreferences.GetInterfacePtr()))))
        return false;

    if (FAILED(tmpPreferences->standardPreferences(&m_standardPreferences.GetInterfacePtr())))
        return false;

    return true;
}

bool MiniBrowser::setToDefaultPreferences()
{
    HRESULT hr = m_standardPreferences->QueryInterface(IID_IWebPreferencesPrivate, reinterpret_cast<void**>(&m_prefsPrivate.GetInterfacePtr()));
    if (!SUCCEEDED(hr))
        return false;

#if USE(CG)
    m_standardPreferences->setAVFoundationEnabled(TRUE);
    m_prefsPrivate->setAcceleratedCompositingEnabled(TRUE);
#endif

    m_prefsPrivate->setFullScreenEnabled(TRUE);
    m_prefsPrivate->setShowDebugBorders(FALSE);
    m_prefsPrivate->setShowRepaintCounter(FALSE);
    m_prefsPrivate->setShouldInvertColors(FALSE);

    m_standardPreferences->setLoadsImagesAutomatically(TRUE);
    m_prefsPrivate->setAuthorAndUserStylesEnabled(TRUE);
    m_standardPreferences->setJavaScriptEnabled(TRUE);
    m_prefsPrivate->setAllowUniversalAccessFromFileURLs(FALSE);
    m_prefsPrivate->setAllowFileAccessFromFileURLs(TRUE);

    m_prefsPrivate->setDeveloperExtrasEnabled(TRUE);

    return true;
}

static void updateMenuItemForHistoryItem(HMENU menu, IWebHistoryItem& historyItem, int currentHistoryItem)
{
    UINT menuID = IDM_HISTORY_LINK0 + currentHistoryItem;

    MENUITEMINFO menuItemInfo = { 0 };
    menuItemInfo.cbSize = sizeof(MENUITEMINFO);
    menuItemInfo.fMask = MIIM_TYPE;
    menuItemInfo.fType = MFT_STRING;

    _bstr_t title;
    historyItem.title(title.GetAddress());
    menuItemInfo.dwTypeData = static_cast<LPWSTR>(title);

    ::SetMenuItemInfo(menu, menuID, FALSE, &menuItemInfo);
    ::EnableMenuItem(menu, menuID, MF_BYCOMMAND | MF_ENABLED);
}

void MiniBrowser::showLastVisitedSites(IWebView& webView)
{
    HMENU menu = ::GetMenu(m_hMainWnd);

    _com_ptr_t<_com_IIID<IWebBackForwardList, &__uuidof(IWebBackForwardList)>> backForwardList;
    HRESULT hr = webView.backForwardList(&backForwardList.GetInterfacePtr());
    if (FAILED(hr))
        return;

    int capacity = 0;
    hr = backForwardList->capacity(&capacity);
    if (FAILED(hr))
        return;

    int backCount = 0;
    hr = backForwardList->backListCount(&backCount);
    if (FAILED(hr))
        return;

    UINT backSetting = MF_BYCOMMAND | ((backCount) ? MF_ENABLED : MF_DISABLED);
    ::EnableMenuItem(menu, IDM_HISTORY_BACKWARD, backSetting);

    int forwardCount = 0;
    hr = backForwardList->forwardListCount(&forwardCount);
    if (FAILED(hr))
        return;

    UINT forwardSetting = MF_BYCOMMAND | ((forwardCount) ? MF_ENABLED : MF_DISABLED);
    ::EnableMenuItem(menu, IDM_HISTORY_FORWARD, forwardSetting);

    IWebHistoryItemPtr currentItem;
    hr = backForwardList->currentItem(&currentItem.GetInterfacePtr());
    if (FAILED(hr))
        return;

    hr = m_webHistory->addItems(1, &currentItem.GetInterfacePtr());
    if (FAILED(hr))
        return;

    _com_ptr_t<_com_IIID<IWebHistoryPrivate, &__uuidof(IWebHistoryPrivate)>> webHistory;
    hr = m_webHistory->QueryInterface(IID_IWebHistoryPrivate, reinterpret_cast<void**>(&webHistory.GetInterfacePtr()));
    if (FAILED(hr))
        return;

    int totalListCount = 0;
    hr = webHistory->allItems(&totalListCount, 0);
    if (FAILED(hr))
        return;

    m_historyItems.resize(totalListCount);

    std::vector<IWebHistoryItem*> historyToLoad(totalListCount);
    hr = webHistory->allItems(&totalListCount, historyToLoad.data());
    if (FAILED(hr))
        return;

    size_t i = 0;
    for (auto& cur : historyToLoad) {
        m_historyItems[i].Attach(cur);
        ++i;
    }

    int allItemsOffset = 0;
    if (totalListCount > maxHistorySize)
        allItemsOffset = totalListCount - maxHistorySize;

    int currentHistoryItem = 0;
    for (int i = 0; i < m_historyItems.size() && (allItemsOffset + currentHistoryItem) < m_historyItems.size(); ++i) {
        updateMenuItemForHistoryItem(menu, *(m_historyItems[allItemsOffset + currentHistoryItem]), currentHistoryItem);
        ++currentHistoryItem;
    }

    // Hide any history we aren't using yet.
    for (int i = currentHistoryItem; i < maxHistorySize; ++i)
        ::EnableMenuItem(menu, IDM_HISTORY_LINK0 + i, MF_BYCOMMAND | MF_DISABLED);
}

void MiniBrowser::launchInspector()
{
    if (!m_webViewPrivate)
        return;

    if (!SUCCEEDED(m_webViewPrivate->inspector(&m_inspector.GetInterfacePtr())))
        return;

    m_inspector->show();
}

void MiniBrowser::navigateForwardOrBackward(HWND hWnd, UINT menuID)
{
    if (!m_webView)
        return;

    BOOL wentBackOrForward = FALSE;
    if (IDM_HISTORY_FORWARD == menuID)
        m_webView->goForward(&wentBackOrForward);
    else
        m_webView->goBack(&wentBackOrForward);
}

void MiniBrowser::navigateToHistory(HWND hWnd, UINT menuID)
{
    if (!m_webView)
        return;

    int historyEntry = menuID - IDM_HISTORY_LINK0;
    if (historyEntry > m_historyItems.size())
        return;

    IWebHistoryItemPtr desiredHistoryItem = m_historyItems[historyEntry];
    if (!desiredHistoryItem)
        return;

    BOOL succeeded = FALSE;
    m_webView->goToBackForwardItem(desiredHistoryItem, &succeeded);

    _bstr_t frameURL;
    desiredHistoryItem->URLString(frameURL.GetAddress());

    ::SendMessage(m_hURLBarWnd, (UINT)WM_SETTEXT, 0, (LPARAM)frameURL.GetBSTR());
}

bool MiniBrowser::goBack()
{
    BOOL wentBack = FALSE;
    m_webView->goBack(&wentBack);
    return wentBack;
}

bool MiniBrowser::goForward()
{
    BOOL wentForward = FALSE;
    m_webView->goForward(&wentForward);
    return wentForward;
}

HRESULT MiniBrowser::loadURL(const BSTR& passedURL)
{
    if (!passedURL)
        return E_INVALIDARG;

    _bstr_t urlBStr(passedURL);
    if (!!urlBStr && (::PathFileExists(urlBStr) || ::PathIsUNC(urlBStr))) {
        TCHAR fileURL[INTERNET_MAX_URL_LENGTH];
        DWORD fileURLLength = sizeof(fileURL) / sizeof(fileURL[0]);

        if (SUCCEEDED(::UrlCreateFromPath(urlBStr, fileURL, &fileURLLength, 0)))
            urlBStr = fileURL;
    }

    IWebFramePtr frame;
    HRESULT hr = m_webView->mainFrame(&frame.GetInterfacePtr());
    if (FAILED(hr))
        return hr;

    IWebMutableURLRequestPtr request;
    hr = WebKitCreateInstance(CLSID_WebMutableURLRequest, 0, IID_IWebMutableURLRequest, (void**)&request);
    if (FAILED(hr))
        return hr;

    hr = request->initWithURL(wcsstr(static_cast<wchar_t*>(urlBStr), L"://") ? urlBStr : _bstr_t(L"http://") + urlBStr, WebURLRequestUseProtocolCachePolicy, 60);
    if (FAILED(hr))
        return hr;

    _bstr_t methodBStr(L"GET");
    hr = request->setHTTPMethod(methodBStr);
    if (FAILED(hr))
        return hr;

    hr = frame->loadRequest(request);

    return hr;
}

void MiniBrowser::exitProgram()
{
    ::PostMessage(m_hMainWnd, static_cast<UINT>(WM_COMMAND), MAKELPARAM(IDM_EXIT, 0), 0);
}

void MiniBrowser::setUserAgent(UINT menuID)
{
    if (!webView())
        return;

    _bstr_t customUserAgent;
    switch (menuID) {
    case IDM_UA_DEFAULT:
        // Set to null user agent
        break;
    case IDM_UA_SAFARI_8_0:
        customUserAgent = L"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_10) AppleWebKit/600.1.25 (KHTML, like Gecko) Version/8.0 Safari/600.1.25";
        break;
    case IDM_UA_SAFARI_IOS_8_IPHONE:
        customUserAgent = L"Mozilla/5.0 (iPhone; CPU OS 8_1 like Mac OS X) AppleWebKit/601.1.4 (KHTML, like Gecko) Version/8.0 Mobile/12B403 Safari/600.1.4";
        break;
    case IDM_UA_SAFARI_IOS_8_IPAD:
        customUserAgent = L"Mozilla/5.0 (iPad; CPU OS 8_1 like Mac OS X) AppleWebKit/600.1.4 (KHTML, like Gecko) Version/8.0 Mobile/12B403 Safari/600.1.4";
        break;
    case IDM_UA_IE_11:
        customUserAgent = L"Mozilla/5.0 (Windows NT 6.3; WOW64; Trident/7.0; rv:11.0) like Gecko";
        break;
    case IDM_UA_CHROME_MAC:
        customUserAgent = L"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_8_3) AppleWebKit/537.31 (KHTML, like Gecko) Chrome/26.0.1410.65 Safari/537.31";
        break;
    case IDM_UA_CHROME_WIN:
        customUserAgent = L"Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.31 (KHTML, like Gecko) Chrome/26.0.1410.64 Safari/537.31";
        break;
    case IDM_UA_FIREFOX_MAC:
        customUserAgent = L"Mozilla/5.0 (Macintosh; Intel Mac OS X 10.8; rv:20.0) Gecko/20100101 Firefox/20.0";
        break;
    case IDM_UA_FIREFOX_WIN:
        customUserAgent = L"Mozilla/5.0 (Windows NT 6.2; WOW64; rv:20.0) Gecko/20100101 Firefox/20.0";
        break;
    case IDM_UA_OTHER:
    default:
        ASSERT(0); // We should never hit this case
        return;
    }

    setUserAgent(customUserAgent);
}

void MiniBrowser::setUserAgent(_bstr_t& customUserAgent)
{
    webView()->setCustomUserAgent(customUserAgent.GetBSTR());
}

_bstr_t MiniBrowser::userAgent()
{
    _bstr_t userAgent;
    if (FAILED(webView()->customUserAgent(&userAgent.GetBSTR())))
        return _bstr_t(L"- Unknown -: Call failed.");

    return userAgent;
}

typedef _com_ptr_t<_com_IIID<IWebIBActions, &__uuidof(IWebIBActions)>> IWebIBActionsPtr;

void MiniBrowser::resetZoom()
{
    IWebIBActionsPtr webActions;
    if (FAILED(m_webView->QueryInterface(IID_IWebIBActions, reinterpret_cast<void**>(&webActions.GetInterfacePtr()))))
        return;

    webActions->resetPageZoom(nullptr);
}

void MiniBrowser::zoomIn()
{
    IWebIBActionsPtr webActions;
    if (FAILED(m_webView->QueryInterface(IID_IWebIBActions, reinterpret_cast<void**>(&webActions.GetInterfacePtr()))))
        return;

    webActions->zoomPageIn(nullptr);
}

void MiniBrowser::zoomOut()
{
    IWebIBActionsPtr webActions;
    if (FAILED(m_webView->QueryInterface(IID_IWebIBActions, reinterpret_cast<void**>(&webActions.GetInterfacePtr()))))
        return;

    webActions->zoomPageOut(nullptr);
}

typedef _com_ptr_t<_com_IIID<IWebViewPrivate3, &__uuidof(IWebViewPrivate3)>> IWebViewPrivate3Ptr;

void MiniBrowser::showLayerTree()
{
    IWebViewPrivate3Ptr webViewPrivate;
    if (FAILED(m_webView->QueryInterface(IID_IWebViewPrivate3, reinterpret_cast<void**>(&webViewPrivate.GetInterfacePtr()))))
        return;

    OutputDebugString(L"CURRENT TREE:\n");

    _bstr_t layerTreeBstr;
    if (FAILED(webViewPrivate->layerTreeAsString(layerTreeBstr.GetAddress())))
        OutputDebugString(L"    Failed to retrieve the layer tree.\n");
    else
        OutputDebugString(layerTreeBstr);
    OutputDebugString(L"\n\n");
}

void MiniBrowser::generateFontForScaleFactor(float scaleFactor)
{
    if (m_hURLBarFont)
        ::DeleteObject(m_hURLBarFont);

    m_hURLBarFont = ::CreateFont(scaleFactor * 18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, L"Times New Roman");
}


void MiniBrowser::updateDeviceScaleFactor()
{
    m_deviceScaleFactor = WebCore::deviceScaleFactorForWindow(m_hMainWnd);
    generateFontForScaleFactor(m_deviceScaleFactor);
}
