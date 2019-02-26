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
#include "stdafx.h"
#include "WebKitBrowserWindow.h"

#include "MiniBrowserLibResource.h"
#include "common.h"
#include <WebKit/WKAuthenticationChallenge.h>
#include <WebKit/WKAuthenticationDecisionListener.h>
#include <WebKit/WKCredential.h>
#include <WebKit/WKInspector.h>
#include <WebKit/WKProtectionSpace.h>
#include <WebKit/WKWebsiteDataStoreRefCurl.h>
#include <vector>

std::wstring createString(WKStringRef wkString)
{
    size_t maxSize = WKStringGetLength(wkString);

    std::vector<WKChar> wkCharBuffer(maxSize);
    size_t actualLength = WKStringGetCharacters(wkString, wkCharBuffer.data(), maxSize);
    return std::wstring(wkCharBuffer.data(), actualLength);
}

std::wstring createString(WKURLRef wkURL)
{
    WKRetainPtr<WKStringRef> url = adoptWK(WKURLCopyString(wkURL));
    return createString(url.get());
}

std::string createUTF8String(const wchar_t* src, size_t srcLength)
{
    int length = WideCharToMultiByte(CP_UTF8, 0, src, srcLength, 0, 0, nullptr, nullptr);
    std::vector<char> buffer(length);
    size_t actualLength = WideCharToMultiByte(CP_UTF8, 0, src, srcLength, buffer.data(), length, nullptr, nullptr);
    return { buffer.data(), actualLength };
}

WKRetainPtr<WKStringRef> createWKString(_bstr_t str)
{
    auto utf8 = createUTF8String(str, str.length());
    return adoptWK(WKStringCreateWithUTF8CString(utf8.data()));
}

WKRetainPtr<WKStringRef> createWKString(const std::wstring& str)
{
    auto utf8 = createUTF8String(str.c_str(), str.length());
    return adoptWK(WKStringCreateWithUTF8CString(utf8.data()));
}

WKRetainPtr<WKURLRef> createWKURL(_bstr_t str)
{
    auto utf8 = createUTF8String(str, str.length());
    return adoptWK(WKURLCreateWithUTF8CString(utf8.data()));
}

WKRetainPtr<WKURLRef> createWKURL(const std::wstring& str)
{
    auto utf8 = createUTF8String(str.c_str(), str.length());
    return adoptWK(WKURLCreateWithUTF8CString(utf8.data()));
}

Ref<BrowserWindow> WebKitBrowserWindow::create(HWND mainWnd, HWND urlBarWnd, bool, bool)
{
    return adoptRef(*new WebKitBrowserWindow(mainWnd, urlBarWnd));
}

WebKitBrowserWindow::WebKitBrowserWindow(HWND mainWnd, HWND urlBarWnd)
    : m_hMainWnd(mainWnd)
    , m_urlBarWnd(urlBarWnd)
{
    RECT rect = { };
    auto conf = adoptWK(WKPageConfigurationCreate());

    auto prefs = WKPreferencesCreate();
    WKPreferencesSetDeveloperExtrasEnabled(prefs, true);
    WKPageConfigurationSetPreferences(conf.get(), prefs);

    m_context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    WKPageConfigurationSetContext(conf.get(), m_context.get());

    m_view = adoptWK(WKViewCreate(rect, conf.get(), mainWnd));
    auto page = WKViewGetPage(m_view.get());

    WKPageNavigationClientV0 navigationClient = { };
    navigationClient.base.version = 0;
    navigationClient.base.clientInfo = this;
    navigationClient.didFinishNavigation = didFinishNavigation;
    navigationClient.didCommitNavigation = didCommitNavigation;
    navigationClient.didReceiveAuthenticationChallenge = didReceiveAuthenticationChallenge;
    WKPageSetPageNavigationClient(page, &navigationClient.base);

    updateProxySettings();
}

void WebKitBrowserWindow::updateProxySettings()
{
    auto store = WKContextGetWebsiteDataStore(m_context.get());

    if (!m_proxy.enable) {
        WKWebsiteDataStoreDisableNetworkProxySettings(store);
        return;
    }

    if (!m_proxy.custom) {
        WKWebsiteDataStoreEnableDefaultNetworkProxySettings(store);
        return;
    }

    auto url = createWKURL(m_proxy.url);
    auto excludeHosts = createWKString(m_proxy.excludeHosts);
    WKWebsiteDataStoreEnableCustomNetworkProxySettings(store, url.get(), excludeHosts.get());
}

HRESULT WebKitBrowserWindow::init()
{
    return S_OK;
}

HWND WebKitBrowserWindow::hwnd()
{
    return WKViewGetWindow(m_view.get());
}

HRESULT WebKitBrowserWindow::loadURL(const BSTR& url)
{
    auto page = WKViewGetPage(m_view.get());
    WKPageLoadURL(page, createWKURL(_bstr_t(url)).get());
    return true;
}

HRESULT WebKitBrowserWindow::loadHTMLString(const BSTR& str)
{
    auto page = WKViewGetPage(m_view.get());
    auto url = createWKURL(_bstr_t(L"about:"));
    WKPageLoadHTMLString(page, createWKString(_bstr_t(str)).get(), url.get());
    return true;
}

void WebKitBrowserWindow::navigateForwardOrBackward(UINT menuID)
{
    auto page = WKViewGetPage(m_view.get());
    if (menuID == IDM_HISTORY_FORWARD)
        WKPageGoForward(page);
    else
        WKPageGoBack(page);
}

void WebKitBrowserWindow::navigateToHistory(UINT menuID)
{
    // Not implemented
}

void WebKitBrowserWindow::setPreference(UINT menuID, bool enable)
{
    auto page = WKViewGetPage(m_view.get());
    auto pgroup = WKPageGetPageGroup(page);
    auto pref = WKPageGroupGetPreferences(pgroup);
    switch (menuID) {
    case IDM_DISABLE_IMAGES:
        WKPreferencesSetLoadsImagesAutomatically(pref, !enable);
        break;
    case IDM_DISABLE_JAVASCRIPT:
        WKPreferencesSetJavaScriptEnabled(pref, !enable);
        break;
    }
}

void WebKitBrowserWindow::print()
{
    // Not implemented
}

void WebKitBrowserWindow::launchInspector()
{
    auto page = WKViewGetPage(m_view.get());
    auto inspector = WKPageGetInspector(page);
    WKInspectorShow(inspector);
}

void WebKitBrowserWindow::openProxySettings()
{
    if (askProxySettings(m_hMainWnd, m_proxy))
        updateProxySettings();

}

void WebKitBrowserWindow::setUserAgent(_bstr_t& customUAString)
{
    auto page = WKViewGetPage(m_view.get());
    auto ua = createWKString(customUAString);
    WKPageSetCustomUserAgent(page, ua.get());
}

_bstr_t WebKitBrowserWindow::userAgent()
{
    auto page = WKViewGetPage(m_view.get());
    auto ua = adoptWK(WKPageCopyUserAgent(page));
    return createString(ua.get()).c_str();
}

void WebKitBrowserWindow::showLayerTree()
{
    // Not implemented
}

void WebKitBrowserWindow::updateStatistics(HWND hDlg)
{
    // Not implemented
}


void WebKitBrowserWindow::resetZoom()
{
    auto page = WKViewGetPage(m_view.get());
    WKPageSetPageZoomFactor(page, 1);
}

void WebKitBrowserWindow::zoomIn()
{
    auto page = WKViewGetPage(m_view.get());
    double s = WKPageGetPageZoomFactor(page);
    WKPageSetPageZoomFactor(page, s * 1.25);
}

void WebKitBrowserWindow::zoomOut()
{
    auto page = WKViewGetPage(m_view.get());
    double s = WKPageGetPageZoomFactor(page);
    WKPageSetPageZoomFactor(page, s * 0.8);
}

static WebKitBrowserWindow& toWebKitBrowserWindow(const void *clientInfo)
{
    return *const_cast<WebKitBrowserWindow*>(static_cast<const WebKitBrowserWindow*>(clientInfo));
}

void WebKitBrowserWindow::didFinishNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef userData, const void* clientInfo)
{
    WKRetainPtr<WKStringRef> title = adoptWK(WKPageCopyTitle(page));
    std::wstring titleString = createString(title.get()) + L" [WebKit]";
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    SetWindowText(thisWindow.m_hMainWnd, titleString.c_str());
}

void WebKitBrowserWindow::didCommitNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef userData, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);

    WKRetainPtr<WKURLRef> wkurl = adoptWK(WKPageCopyCommittedURL(page));
    std::wstring urlString = createString(wkurl.get());
    SetWindowText(thisWindow.m_urlBarWnd, urlString.c_str());
}

void WebKitBrowserWindow::didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef challenge, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    auto protectionSpace = WKAuthenticationChallengeGetProtectionSpace(challenge);
    auto decisionListener = WKAuthenticationChallengeGetDecisionListener(challenge);

    WKRetainPtr<WKStringRef> realm(WKProtectionSpaceCopyRealm(protectionSpace));
    if (auto credential = askCredential(thisWindow.hwnd(), createString(realm.get()))) {
        WKRetainPtr<WKStringRef> username = createWKString(credential->username);
        WKRetainPtr<WKStringRef> password = createWKString(credential->password);
        WKRetainPtr<WKCredentialRef> wkCredential(AdoptWK, WKCredentialCreate(username.get(), password.get(), kWKCredentialPersistenceForSession));
        WKAuthenticationDecisionListenerUseCredential(decisionListener, wkCredential.get());
        return;
    }

    WKAuthenticationDecisionListenerUseCredential(decisionListener, nullptr);
}
