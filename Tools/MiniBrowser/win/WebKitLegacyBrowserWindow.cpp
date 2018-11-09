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
#include "WebKitLegacyBrowserWindow.h"

#include "AccessibilityDelegate.h"
#include "Common.h"
#include "DOMDefaultImpl.h"
#include "MiniBrowserLibResource.h"
#include "MiniBrowserReplace.h"
#include "MiniBrowserWebHost.h"
#include "PrintWebUIDelegate.h"
#include "ResourceLoadDelegate.h"
#include "WebDownloadDelegate.h"
#include <WebCore/COMPtr.h>
#include <WebKitLegacy/WebKitCOMAPI.h>
#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <WebKitLegacy/CFDictionaryPropertyBag.h>
#endif

namespace WebCore {
float deviceScaleFactorForWindow(HWND);
}

static const int maxHistorySize = 10;

typedef _com_ptr_t<_com_IIID<IWebMutableURLRequest, &__uuidof(IWebMutableURLRequest)>> IWebMutableURLRequestPtr;

Ref<BrowserWindow> WebKitLegacyBrowserWindow::create(HWND mainWnd, HWND urlBarWnd, bool useLayeredWebView, bool pageLoadTesting)
{
    return adoptRef(*new WebKitLegacyBrowserWindow(mainWnd, urlBarWnd, useLayeredWebView, pageLoadTesting));
}

WebKitLegacyBrowserWindow::WebKitLegacyBrowserWindow(HWND mainWnd, HWND urlBarWnd, bool useLayeredWebView, bool pageLoadTesting)
    : m_hMainWnd(mainWnd)
    , m_hURLBarWnd(urlBarWnd)
    , m_useLayeredWebView(useLayeredWebView)
    , m_pageLoadTestClient(std::make_unique<PageLoadTestClient>(this, pageLoadTesting))
{
}

ULONG WebKitLegacyBrowserWindow::AddRef()
{
    ref();
    return refCount();
}

ULONG WebKitLegacyBrowserWindow::Release()
{
    auto count = refCount();
    deref();
    return --count;
}

HRESULT WebKitLegacyBrowserWindow::init()
{
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

    hr = setUIDelegate(new PrintWebUIDelegate(*this));
    if (FAILED (hr))
        return hr;

    hr = setAccessibilityDelegate(new AccessibilityDelegate(*this));
    if (FAILED (hr))
        return hr;

    hr = setResourceLoadDelegate(new ResourceLoadDelegate(this));
    if (FAILED(hr))
        return hr;

    IWebDownloadDelegatePtr downloadDelegate;
    downloadDelegate.Attach(new WebDownloadDelegate(*this));
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

bool WebKitLegacyBrowserWindow::setCacheFolder()
{
    _bstr_t appDataFolder;
    if (!getAppDataFolder(appDataFolder))
        return false;

    appDataFolder += L"\\cache";
    webCache()->setCacheFolder(appDataFolder);

    return true;
}

HRESULT WebKitLegacyBrowserWindow::prepareViews(HWND mainWnd, const RECT& clientRect)
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

HRESULT WebKitLegacyBrowserWindow::loadHTMLString(const BSTR& str)
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

void WebKitLegacyBrowserWindow::subclassForLayeredWindow()
{
#if defined _M_AMD64 || defined _WIN64
    gDefWebKitProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(m_viewWnd, GWLP_WNDPROC));
    ::SetWindowLongPtr(m_viewWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(viewWndProc));
#else
    gDefWebKitProc = reinterpret_cast<WNDPROC>(::GetWindowLong(m_viewWnd, GWL_WNDPROC));
    ::SetWindowLong(m_viewWnd, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(viewWndProc));
#endif
}

HRESULT WebKitLegacyBrowserWindow::setFrameLoadDelegate(IWebFrameLoadDelegate* frameLoadDelegate)
{
    m_frameLoadDelegate = frameLoadDelegate;
    return m_webView->setFrameLoadDelegate(frameLoadDelegate);
}

HRESULT WebKitLegacyBrowserWindow::setFrameLoadDelegatePrivate(IWebFrameLoadDelegatePrivate* frameLoadDelegatePrivate)
{
    return m_webViewPrivate->setFrameLoadDelegatePrivate(frameLoadDelegatePrivate);
}

HRESULT WebKitLegacyBrowserWindow::setUIDelegate(IWebUIDelegate* uiDelegate)
{
    m_uiDelegate = uiDelegate;
    return m_webView->setUIDelegate(uiDelegate);
}

HRESULT WebKitLegacyBrowserWindow::setAccessibilityDelegate(IAccessibilityDelegate* accessibilityDelegate)
{
    m_accessibilityDelegate = accessibilityDelegate;
    return m_webView->setAccessibilityDelegate(accessibilityDelegate);
}

HRESULT WebKitLegacyBrowserWindow::setResourceLoadDelegate(IWebResourceLoadDelegate* resourceLoadDelegate)
{
    m_resourceLoadDelegate = resourceLoadDelegate;
    return m_webView->setResourceLoadDelegate(resourceLoadDelegate);
}

HRESULT WebKitLegacyBrowserWindow::setDownloadDelegate(IWebDownloadDelegatePtr downloadDelegate)
{
    m_downloadDelegate = downloadDelegate;
    return m_webView->setDownloadDelegate(downloadDelegate);
}

IWebFramePtr WebKitLegacyBrowserWindow::mainFrame()
{
    IWebFramePtr framePtr;
    m_webView->mainFrame(&framePtr.GetInterfacePtr());
    return framePtr;
}

bool WebKitLegacyBrowserWindow::seedInitialDefaultPreferences()
{
    IWebPreferencesPtr tmpPreferences;
    if (FAILED(WebKitCreateInstance(CLSID_WebPreferences, 0, IID_IWebPreferences, reinterpret_cast<void**>(&tmpPreferences.GetInterfacePtr()))))
        return false;

    if (FAILED(tmpPreferences->standardPreferences(&m_standardPreferences.GetInterfacePtr())))
        return false;

    return true;
}

bool WebKitLegacyBrowserWindow::setToDefaultPreferences()
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

void WebKitLegacyBrowserWindow::showLastVisitedSites(IWebView& webView)
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

void WebKitLegacyBrowserWindow::launchInspector()
{
    if (!m_webViewPrivate)
        return;

    if (!SUCCEEDED(m_webViewPrivate->inspector(&m_inspector.GetInterfacePtr())))
        return;

    m_inspector->show();
}

void WebKitLegacyBrowserWindow::openProxySettings()
{
}

void WebKitLegacyBrowserWindow::navigateForwardOrBackward(UINT menuID)
{
    if (!m_webView)
        return;

    BOOL wentBackOrForward = FALSE;
    if (IDM_HISTORY_FORWARD == menuID)
        m_webView->goForward(&wentBackOrForward);
    else
        m_webView->goBack(&wentBackOrForward);
}

void WebKitLegacyBrowserWindow::navigateToHistory(UINT menuID)
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

bool WebKitLegacyBrowserWindow::goBack()
{
    BOOL wentBack = FALSE;
    m_webView->goBack(&wentBack);
    return wentBack;
}

bool WebKitLegacyBrowserWindow::goForward()
{
    BOOL wentForward = FALSE;
    m_webView->goForward(&wentForward);
    return wentForward;
}

HRESULT WebKitLegacyBrowserWindow::loadURL(const BSTR& passedURL)
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

void WebKitLegacyBrowserWindow::exitProgram()
{
    ::PostMessage(m_hMainWnd, static_cast<UINT>(WM_COMMAND), MAKELPARAM(IDM_EXIT, 0), 0);
}

void WebKitLegacyBrowserWindow::setUserAgent(_bstr_t& customUserAgent)
{
    if (!webView())
        return;

    webView()->setCustomUserAgent(customUserAgent.GetBSTR());
}

_bstr_t WebKitLegacyBrowserWindow::userAgent()
{
    _bstr_t userAgent;
    if (FAILED(webView()->customUserAgent(&userAgent.GetBSTR())))
        return _bstr_t(L"- Unknown -: Call failed.");

    return userAgent;
}

typedef _com_ptr_t<_com_IIID<IWebIBActions, &__uuidof(IWebIBActions)>> IWebIBActionsPtr;

void WebKitLegacyBrowserWindow::resetZoom()
{
    IWebIBActionsPtr webActions;
    if (FAILED(m_webView->QueryInterface(IID_IWebIBActions, reinterpret_cast<void**>(&webActions.GetInterfacePtr()))))
        return;

    webActions->resetPageZoom(nullptr);
}

void WebKitLegacyBrowserWindow::zoomIn()
{
    IWebIBActionsPtr webActions;
    if (FAILED(m_webView->QueryInterface(IID_IWebIBActions, reinterpret_cast<void**>(&webActions.GetInterfacePtr()))))
        return;

    webActions->zoomPageIn(nullptr);
}

void WebKitLegacyBrowserWindow::zoomOut()
{
    IWebIBActionsPtr webActions;
    if (FAILED(m_webView->QueryInterface(IID_IWebIBActions, reinterpret_cast<void**>(&webActions.GetInterfacePtr()))))
        return;

    webActions->zoomPageOut(nullptr);
}

typedef _com_ptr_t<_com_IIID<IWebViewPrivate3, &__uuidof(IWebViewPrivate3)>> IWebViewPrivate3Ptr;

void WebKitLegacyBrowserWindow::showLayerTree()
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

static BOOL CALLBACK AbortProc(HDC hDC, int Error)
{
    MSG msg;
    while (::PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    return TRUE;
}

static HDC getPrinterDC()
{
    PRINTDLG pdlg;
    memset(&pdlg, 0, sizeof(PRINTDLG));
    pdlg.lStructSize = sizeof(PRINTDLG);
    pdlg.Flags = PD_PRINTSETUP | PD_RETURNDC;

    ::PrintDlg(&pdlg);

    return pdlg.hDC;
}

static void initDocStruct(DOCINFO* di, TCHAR* docname)
{
    memset(di, 0, sizeof(DOCINFO));
    di->cbSize = sizeof(DOCINFO);
    di->lpszDocName = docname;
}

void WebKitLegacyBrowserWindow::print()
{
    HDC printDC = getPrinterDC();
    if (!printDC) {
        ::MessageBox(0, L"Error creating printing DC", L"Error", MB_APPLMODAL | MB_OK);
        return;
    }

    if (::SetAbortProc(printDC, AbortProc) == SP_ERROR) {
        ::MessageBox(0, L"Error setting up AbortProc", L"Error", MB_APPLMODAL | MB_OK);
        return;
    }

    IWebFramePtr frame = mainFrame();
    if (!frame)
        return;

    IWebFramePrivatePtr framePrivate;
    if (FAILED(frame->QueryInterface(&framePrivate.GetInterfacePtr())))
        return;

    framePrivate->setInPrintingMode(TRUE, printDC);

    UINT pageCount = 0;
    framePrivate->getPrintedPageCount(printDC, &pageCount);

    DOCINFO di;
    initDocStruct(&di, L"WebKit Doc");
    ::StartDoc(printDC, &di);

    // FIXME: Need CoreGraphics implementation
    void* graphicsContext = 0;
    for (size_t page = 1; page <= pageCount; ++page) {
        ::StartPage(printDC);
        framePrivate->spoolPages(printDC, page, page, graphicsContext);
        ::EndPage(printDC);
    }

    framePrivate->setInPrintingMode(FALSE, printDC);

    ::EndDoc(printDC);
    ::DeleteDC(printDC);
}

static void setWindowText(HWND dialog, UINT field, _bstr_t value)
{
    ::SetDlgItemText(dialog, field, value);
}

static void setWindowText(HWND dialog, UINT field, UINT value)
{
    String valueStr = WTF::String::number(value);

    setWindowText(dialog, field, _bstr_t(valueStr.utf8().data()));
}

typedef _com_ptr_t<_com_IIID<IPropertyBag, &__uuidof(IPropertyBag)>> IPropertyBagPtr;

static void setWindowText(HWND dialog, UINT field, IPropertyBagPtr statistics, const _bstr_t& key)
{
    _variant_t var;
    V_VT(&var) = VT_UI8;
    if (FAILED(statistics->Read(key, &var.GetVARIANT(), nullptr)))
        return;

    unsigned long long value = V_UI8(&var);
    String valueStr = WTF::String::number(value);

    setWindowText(dialog, field, _bstr_t(valueStr.utf8().data()));
}

static void setWindowText(HWND dialog, UINT field, CFDictionaryRef dictionary, CFStringRef key, UINT& total)
{
    CFNumberRef countNum = static_cast<CFNumberRef>(CFDictionaryGetValue(dictionary, key));
    if (!countNum)
        return;

    int count = 0;
    CFNumberGetValue(countNum, kCFNumberIntType, &count);

    setWindowText(dialog, field, static_cast<UINT>(count));
    total += count;
}

void WebKitLegacyBrowserWindow::updateStatistics(HWND dialog)
{
    IWebCoreStatisticsPtr webCoreStatistics = statistics();
    if (!webCoreStatistics)
        return;

    IPropertyBagPtr statistics;
    HRESULT hr = webCoreStatistics->memoryStatistics(&statistics.GetInterfacePtr());
    if (FAILED(hr))
        return;

    // FastMalloc.
    setWindowText(dialog, IDC_RESERVED_VM, statistics, "FastMallocReservedVMBytes");
    setWindowText(dialog, IDC_COMMITTED_VM, statistics, "FastMallocCommittedVMBytes");
    setWindowText(dialog, IDC_FREE_LIST_BYTES, statistics, "FastMallocFreeListBytes");

    // WebCore Cache.
#if USE(CF)
    IWebCachePtr webCache = this->webCache();

    int dictCount = 6;
    IPropertyBag* cacheDict[6] = { 0 };
    if (FAILED(webCache->statistics(&dictCount, cacheDict)))
        return;

    COMPtr<CFDictionaryPropertyBag> counts, sizes, liveSizes, decodedSizes, purgableSizes;
    counts.adoptRef(reinterpret_cast<CFDictionaryPropertyBag*>(cacheDict[0]));
    sizes.adoptRef(reinterpret_cast<CFDictionaryPropertyBag*>(cacheDict[1]));
    liveSizes.adoptRef(reinterpret_cast<CFDictionaryPropertyBag*>(cacheDict[2]));
    decodedSizes.adoptRef(reinterpret_cast<CFDictionaryPropertyBag*>(cacheDict[3]));
    purgableSizes.adoptRef(reinterpret_cast<CFDictionaryPropertyBag*>(cacheDict[4]));

    static CFStringRef imagesKey = CFSTR("images");
    static CFStringRef stylesheetsKey = CFSTR("style sheets");
    static CFStringRef xslKey = CFSTR("xsl");
    static CFStringRef scriptsKey = CFSTR("scripts");

    if (counts) {
        UINT totalObjects = 0;
        setWindowText(dialog, IDC_IMAGES_OBJECT_COUNT, counts->dictionary(), imagesKey, totalObjects);
        setWindowText(dialog, IDC_CSS_OBJECT_COUNT, counts->dictionary(), stylesheetsKey, totalObjects);
        setWindowText(dialog, IDC_XSL_OBJECT_COUNT, counts->dictionary(), xslKey, totalObjects);
        setWindowText(dialog, IDC_JSC_OBJECT_COUNT, counts->dictionary(), scriptsKey, totalObjects);
        setWindowText(dialog, IDC_TOTAL_OBJECT_COUNT, totalObjects);
    }

    if (sizes) {
        UINT totalBytes = 0;
        setWindowText(dialog, IDC_IMAGES_BYTES, sizes->dictionary(), imagesKey, totalBytes);
        setWindowText(dialog, IDC_CSS_BYTES, sizes->dictionary(), stylesheetsKey, totalBytes);
        setWindowText(dialog, IDC_XSL_BYTES, sizes->dictionary(), xslKey, totalBytes);
        setWindowText(dialog, IDC_JSC_BYTES, sizes->dictionary(), scriptsKey, totalBytes);
        setWindowText(dialog, IDC_TOTAL_BYTES, totalBytes);
    }

    if (liveSizes) {
        UINT totalLiveBytes = 0;
        setWindowText(dialog, IDC_IMAGES_LIVE_COUNT, liveSizes->dictionary(), imagesKey, totalLiveBytes);
        setWindowText(dialog, IDC_CSS_LIVE_COUNT, liveSizes->dictionary(), stylesheetsKey, totalLiveBytes);
        setWindowText(dialog, IDC_XSL_LIVE_COUNT, liveSizes->dictionary(), xslKey, totalLiveBytes);
        setWindowText(dialog, IDC_JSC_LIVE_COUNT, liveSizes->dictionary(), scriptsKey, totalLiveBytes);
        setWindowText(dialog, IDC_TOTAL_LIVE_COUNT, totalLiveBytes);
    }

    if (decodedSizes) {
        UINT totalDecoded = 0;
        setWindowText(dialog, IDC_IMAGES_DECODED_COUNT, decodedSizes->dictionary(), imagesKey, totalDecoded);
        setWindowText(dialog, IDC_CSS_DECODED_COUNT, decodedSizes->dictionary(), stylesheetsKey, totalDecoded);
        setWindowText(dialog, IDC_XSL_DECODED_COUNT, decodedSizes->dictionary(), xslKey, totalDecoded);
        setWindowText(dialog, IDC_JSC_DECODED_COUNT, decodedSizes->dictionary(), scriptsKey, totalDecoded);
        setWindowText(dialog, IDC_TOTAL_DECODED, totalDecoded);
    }

    if (purgableSizes) {
        UINT totalPurgable = 0;
        setWindowText(dialog, IDC_IMAGES_PURGEABLE_COUNT, purgableSizes->dictionary(), imagesKey, totalPurgable);
        setWindowText(dialog, IDC_CSS_PURGEABLE_COUNT, purgableSizes->dictionary(), stylesheetsKey, totalPurgable);
        setWindowText(dialog, IDC_XSL_PURGEABLE_COUNT, purgableSizes->dictionary(), xslKey, totalPurgable);
        setWindowText(dialog, IDC_JSC_PURGEABLE_COUNT, purgableSizes->dictionary(), scriptsKey, totalPurgable);
        setWindowText(dialog, IDC_TOTAL_PURGEABLE, totalPurgable);
    }
#endif

    // JavaScript Heap.
    setWindowText(dialog, IDC_JSC_HEAP_SIZE, statistics, "JavaScriptHeapSize");
    setWindowText(dialog, IDC_JSC_HEAP_FREE, statistics, "JavaScriptFreeSize");

    UINT count;
    if (SUCCEEDED(webCoreStatistics->javaScriptObjectsCount(&count)))
        setWindowText(dialog, IDC_TOTAL_JSC_HEAP_OBJECTS, count);
    if (SUCCEEDED(webCoreStatistics->javaScriptGlobalObjectsCount(&count)))
        setWindowText(dialog, IDC_GLOBAL_JSC_HEAP_OBJECTS, count);
    if (SUCCEEDED(webCoreStatistics->javaScriptProtectedObjectsCount(&count)))
        setWindowText(dialog, IDC_PROTECTED_JSC_HEAP_OBJECTS, count);

    // Font and Glyph Caches.
    if (SUCCEEDED(webCoreStatistics->cachedFontDataCount(&count)))
        setWindowText(dialog, IDC_TOTAL_FONT_OBJECTS, count);
    if (SUCCEEDED(webCoreStatistics->cachedFontDataInactiveCount(&count)))
        setWindowText(dialog, IDC_INACTIVE_FONT_OBJECTS, count);
    if (SUCCEEDED(webCoreStatistics->glyphPageCount(&count)))
        setWindowText(dialog, IDC_GLYPH_PAGES, count);

    // Site Icon Database.
    if (SUCCEEDED(webCoreStatistics->iconPageURLMappingCount(&count)))
        setWindowText(dialog, IDC_PAGE_URL_MAPPINGS, count);
    if (SUCCEEDED(webCoreStatistics->iconRetainedPageURLCount(&count)))
        setWindowText(dialog, IDC_RETAINED_PAGE_URLS, count);
    if (SUCCEEDED(webCoreStatistics->iconRecordCount(&count)))
        setWindowText(dialog, IDC_SITE_ICON_RECORDS, count);
    if (SUCCEEDED(webCoreStatistics->iconsWithDataCount(&count)))
        setWindowText(dialog, IDC_SITE_ICONS_WITH_DATA, count);
}

void WebKitLegacyBrowserWindow::setPreference(UINT menuID, bool enable)
{
    if (!standardPreferences() || !privatePreferences())
        return;

    switch (menuID) {
    case IDM_AVFOUNDATION:
        standardPreferences()->setAVFoundationEnabled(enable);
        break;
    case IDM_ACC_COMPOSITING:
        privatePreferences()->setAcceleratedCompositingEnabled(enable);
        break;
    case IDM_WK_FULLSCREEN:
        privatePreferences()->setFullScreenEnabled(enable);
        break;
    case IDM_COMPOSITING_BORDERS:
        privatePreferences()->setShowDebugBorders(enable);
        privatePreferences()->setShowRepaintCounter(enable);
        break;
    case IDM_DEBUG_INFO_LAYER:
        privatePreferences()->setShowTiledScrollingIndicator(enable);
        break;
    case IDM_INVERT_COLORS:
        privatePreferences()->setShouldInvertColors(enable);
        break;
    case IDM_DISABLE_IMAGES:
        standardPreferences()->setLoadsImagesAutomatically(!enable);
        break;
    case IDM_DISABLE_STYLES:
        privatePreferences()->setAuthorAndUserStylesEnabled(!enable);
        break;
    case IDM_DISABLE_JAVASCRIPT:
        standardPreferences()->setJavaScriptEnabled(!enable);
        break;
    case IDM_DISABLE_LOCAL_FILE_RESTRICTIONS:
        privatePreferences()->setAllowUniversalAccessFromFileURLs(enable);
        privatePreferences()->setAllowFileAccessFromFileURLs(enable);
        break;
    }
}
