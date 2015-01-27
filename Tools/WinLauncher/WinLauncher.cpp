/*
 * Copyright (C) 2006, 2008, 2013, 2014 Apple Inc.  All rights reserved.
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
#include "WinLauncher.h"

#include "DOMDefaultImpl.h"
#include "WinLauncherLibResource.h"
#include "WinLauncherReplace.h"
#include <WebKit/WebKitCOMAPI.h>
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

static const int maxHistorySize = 10;

typedef _com_ptr_t<_com_IIID<IWebMutableURLRequest, &__uuidof(IWebMutableURLRequest)>> IWebMutableURLRequestPtr;

WinLauncher::WinLauncher(HWND mainWnd, HWND urlBarWnd, bool useLayeredWebView, bool pageLoadTesting)
    : m_hMainWnd(mainWnd)
    , m_hURLBarWnd(urlBarWnd)
    , m_useLayeredWebView(useLayeredWebView)
    , m_pageLoadTestClient(std::make_unique<PageLoadTestClient>(this, pageLoadTesting))
{
}

HRESULT WinLauncher::init()
{
    HRESULT hr = WebKitCreateInstance(CLSID_WebView, 0, IID_IWebView, reinterpret_cast<void**>(&m_webView.GetInterfacePtr()));
    if (FAILED(hr))
        return hr;

    hr = m_webView->QueryInterface(IID_IWebViewPrivate, reinterpret_cast<void**>(&m_webViewPrivate.GetInterfacePtr()));
    if (FAILED(hr))
        return hr;

    hr = WebKitCreateInstance(CLSID_WebHistory, 0, __uuidof(m_webHistory), reinterpret_cast<void**>(&m_webHistory.GetInterfacePtr()));
    if (FAILED(hr))
        return hr;

    hr = WebKitCreateInstance(CLSID_WebCoreStatistics, 0, __uuidof(m_statistics), reinterpret_cast<void**>(&m_statistics.GetInterfacePtr()));
    if (FAILED(hr))
        return hr;

    hr = WebKitCreateInstance(CLSID_WebCache, 0, __uuidof(m_webCache), reinterpret_cast<void**>(&m_webCache.GetInterfacePtr()));

    return hr;
}

HRESULT WinLauncher::prepareViews(HWND mainWnd, const RECT& clientRect, const BSTR& requestedURL, HWND& viewHwnd)
{
    if (!m_webView)
        return E_FAIL;

    HRESULT hr = m_webView->setHostWindow(mainWnd);
    if (FAILED(hr))
        return hr;

    hr = m_webView->initWithFrame(clientRect, 0, 0);
    if (FAILED(hr))
        return hr;

    if (!requestedURL) {
        IWebFramePtr frame;
        hr = m_webView->mainFrame(&frame.GetInterfacePtr());
        if (FAILED(hr))
            return hr;

        frame->loadHTMLString(_bstr_t(defaultHTML).GetBSTR(), 0);
    }

    hr = m_webViewPrivate->setTransparent(m_useLayeredWebView);
    if (FAILED(hr))
        return hr;

    hr = m_webViewPrivate->setUsesLayeredWindow(m_useLayeredWebView);
    if (FAILED(hr))
        return hr;

    hr = m_webViewPrivate->viewWindow(&viewHwnd);

    return hr;
}

HRESULT WinLauncher::setFrameLoadDelegate(IWebFrameLoadDelegate* frameLoadDelegate)
{
    m_frameLoadDelegate = frameLoadDelegate;
    return m_webView->setFrameLoadDelegate(frameLoadDelegate);
}

HRESULT WinLauncher::setFrameLoadDelegatePrivate(IWebFrameLoadDelegatePrivate* frameLoadDelegatePrivate)
{
    return m_webViewPrivate->setFrameLoadDelegatePrivate(frameLoadDelegatePrivate);
}

HRESULT WinLauncher::setUIDelegate(IWebUIDelegate* uiDelegate)
{
    m_uiDelegate = uiDelegate;
    return m_webView->setUIDelegate(uiDelegate);
}

HRESULT WinLauncher::setAccessibilityDelegate(IAccessibilityDelegate* accessibilityDelegate)
{
    m_accessibilityDelegate = accessibilityDelegate;
    return m_webView->setAccessibilityDelegate(accessibilityDelegate);
}


HRESULT WinLauncher::setResourceLoadDelegate(IWebResourceLoadDelegate* resourceLoadDelegate)
{
    m_resourceLoadDelegate = resourceLoadDelegate;
    return m_webView->setResourceLoadDelegate(resourceLoadDelegate);
}

IWebFramePtr WinLauncher::mainFrame()
{
    IWebFramePtr framePtr;
    m_webView->mainFrame(&framePtr.GetInterfacePtr());
    return framePtr;
}

bool WinLauncher::seedInitialDefaultPreferences()
{
    IWebPreferencesPtr tmpPreferences;
    if (FAILED(WebKitCreateInstance(CLSID_WebPreferences, 0, IID_IWebPreferences, reinterpret_cast<void**>(&tmpPreferences.GetInterfacePtr()))))
        return false;

    if (FAILED(tmpPreferences->standardPreferences(&m_standardPreferences.GetInterfacePtr())))
        return false;

    return true;
}

bool WinLauncher::setToDefaultPreferences()
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

void WinLauncher::showLastVisitedSites(IWebView& webView)
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

void WinLauncher::launchInspector()
{
    if (!m_webViewPrivate)
        return;

    if (!SUCCEEDED(m_webViewPrivate->inspector(&m_inspector.GetInterfacePtr())))
        return;

    m_inspector->show();
}

void WinLauncher::navigateForwardOrBackward(HWND hWnd, UINT menuID)
{
    if (!m_webView)
        return;

    BOOL wentBackOrForward = FALSE;
    if (IDM_HISTORY_FORWARD == menuID)
        m_webView->goForward(&wentBackOrForward);
    else
        m_webView->goBack(&wentBackOrForward);
}

void WinLauncher::navigateToHistory(HWND hWnd, UINT menuID)
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

bool WinLauncher::goBack()
{
    BOOL wentBack = FALSE;
    m_webView->goBack(&wentBack);
    return wentBack;
}

bool WinLauncher::goForward()
{
    BOOL wentForward = FALSE;
    m_webView->goForward(&wentForward);
    return wentForward;
}

HRESULT WinLauncher::loadURL(const BSTR& passedURL)
{
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

    if (!passedURL)
        return frame->loadHTMLString(_bstr_t(defaultHTML).GetBSTR(), 0);

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

void WinLauncher::exitProgram()
{
    ::PostMessage(m_hMainWnd, static_cast<UINT>(WM_COMMAND), MAKELPARAM(IDM_EXIT, 0), 0);
}

void WinLauncher::setUserAgent(UINT menuID)
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

void WinLauncher::setUserAgent(_bstr_t& customUserAgent)
{
    webView()->setCustomUserAgent(customUserAgent.GetBSTR());
}

_bstr_t WinLauncher::userAgent()
{
    _bstr_t userAgent;
    if (FAILED(webView()->customUserAgent(&userAgent.GetBSTR())))
        return _bstr_t(L"- Unknown -: Call failed.");

    return userAgent;
}

typedef _com_ptr_t<_com_IIID<IWebIBActions, &__uuidof(IWebIBActions)>> IWebIBActionsPtr;

void WinLauncher::resetZoom()
{
    IWebIBActionsPtr webActions;
    if (FAILED(m_webView->QueryInterface(IID_IWebIBActions, reinterpret_cast<void**>(&webActions.GetInterfacePtr()))))
        return;

    webActions->resetPageZoom(nullptr);
}

void WinLauncher::zoomIn()
{
    IWebIBActionsPtr webActions;
    if (FAILED(m_webView->QueryInterface(IID_IWebIBActions, reinterpret_cast<void**>(&webActions.GetInterfacePtr()))))
        return;

    webActions->zoomPageIn(nullptr);
}

void WinLauncher::zoomOut()
{
    IWebIBActionsPtr webActions;
    if (FAILED(m_webView->QueryInterface(IID_IWebIBActions, reinterpret_cast<void**>(&webActions.GetInterfacePtr()))))
        return;

    webActions->zoomPageOut(nullptr);
}
