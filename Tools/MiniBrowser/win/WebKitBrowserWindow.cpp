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

#include "Common.h"
#include "Common2.h"
#include "MiniBrowserLibResource.h"
#include <WebCore/GDIUtilities.h>
#include <WebKit/WKAuthenticationChallenge.h>
#include <WebKit/WKAuthenticationDecisionListener.h>
#include <WebKit/WKCertificateInfoCurl.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKCredential.h>
#include <WebKit/WKInspector.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKProtectionSpace.h>
#include <WebKit/WKProtectionSpaceCurl.h>
#include <WebKit/WKSecurityOriginRef.h>
#include <WebKit/WKWebsiteDataStoreRef.h>
#include <WebKit/WKWebsiteDataStoreRefCurl.h>
#include <filesystem>
#include <sstream>
#include <vector>

std::wstring createString(WKURLRef wkURL)
{
    if (!wkURL)
        return { };
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

std::wstring createPEMString(WKProtectionSpaceRef protectionSpace)
{
    auto chain = adoptWK(WKProtectionSpaceCopyCertificateChain(protectionSpace));

    std::wstring pems;

    for (auto i = 0; i < WKArrayGetSize(chain.get()); i++) {
        auto item = WKArrayGetItemAtIndex(chain.get(), i);
        assert(WKGetTypeID(item) == WKDataGetTypeID());
        auto certificate = static_cast<WKDataRef>(item);
        auto size = WKDataGetSize(certificate);
        auto data = WKDataGetBytes(certificate);

        for (size_t i = 0; i < size; i++)
            pems.push_back(data[i]);
    }

    return replaceString(pems, L"\n", L"\r\n");
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

WKRetainPtr<WKStringRef> createWKString(const wchar_t* str)
{
    return createWKString(std::wstring(str));
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

WKRetainPtr<WKStringRef> injectedBundlePath()
{
    auto module = GetModuleHandle(nullptr);
    std::wstring path;
    for (;;) {
        path.resize(path.size() + MAX_PATH);
        auto copied = GetModuleFileName(module, path.data(), path.size());
        if (copied < path.size()) {
            path.resize(copied);
            break;
        }
    }
    path = std::filesystem::path(path).replace_filename("MiniBrowserInjectedBundle.dll");
    return createWKString(path);
}

Ref<BrowserWindow> WebKitBrowserWindow::create(BrowserWindowClient& client, HWND mainWnd, bool)
{
    auto conf = adoptWK(WKPageConfigurationCreate());

    auto prefs = adoptWK(WKPreferencesCreate());

    auto pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(createWKString("WinMiniBrowser").get()));
    WKPageConfigurationSetPageGroup(conf.get(), pageGroup.get());
    WKPageGroupSetPreferences(pageGroup.get(), prefs.get());

    WKPreferencesSetMediaCapabilitiesEnabled(prefs.get(), false);
    WKPreferencesSetDeveloperExtrasEnabled(prefs.get(), true);
    WKPageConfigurationSetPreferences(conf.get(), prefs.get());

    auto contextConf = adoptWK(WKContextConfigurationCreate());
    WKContextConfigurationSetInjectedBundlePath(contextConf.get(), injectedBundlePath().get());

    auto context = adoptWK(WKContextCreateWithConfiguration(contextConf.get()));
    WKPageConfigurationSetContext(conf.get(), context.get());

    return adoptRef(*new WebKitBrowserWindow(client, conf.get(), mainWnd));
}

WebKitBrowserWindow::WebKitBrowserWindow(BrowserWindowClient& client, WKPageConfigurationRef conf, HWND mainWnd)
    : m_client(client)
    , m_hMainWnd(mainWnd)
{
    RECT rect = { };
    m_view = adoptWK(WKViewCreate(rect, conf, mainWnd));
    WKViewSetIsInWindow(m_view.get(), true);

    auto page = WKViewGetPage(m_view.get());

    WKPageNavigationClientV0 navigationClient = { };
    navigationClient.base.version = 0;
    navigationClient.base.clientInfo = this;
    navigationClient.didFailProvisionalNavigation = didFailProvisionalNavigation;
    navigationClient.didReceiveAuthenticationChallenge = didReceiveAuthenticationChallenge;
    WKPageSetPageNavigationClient(page, &navigationClient.base);

    WKPageUIClientV13 uiClient = { };
    uiClient.base.version = 13;
    uiClient.base.clientInfo = this;
    uiClient.createNewPage = createNewPage;
    uiClient.didNotHandleKeyEvent = didNotHandleKeyEvent;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;
    uiClient.runJavaScriptConfirm = runJavaScriptConfirm;
    uiClient.runJavaScriptPrompt = runJavaScriptPrompt;
    WKPageSetPageUIClient(page, &uiClient.base);

    WKPageStateClientV0 stateClient = { };
    stateClient.base.version = 0;
    stateClient.base.clientInfo = this;
    stateClient.didChangeTitle = didChangeTitle;
    stateClient.didChangeIsLoading = didChangeIsLoading;
    stateClient.didChangeEstimatedProgress = didChangeEstimatedProgress;
    stateClient.didChangeActiveURL = didChangeActiveURL;
    WKPageSetPageStateClient(page, &stateClient.base);

    updateProxySettings();
    resetZoom();
}

void WebKitBrowserWindow::updateProxySettings()
{
    auto context = WKPageGetContext(WKViewGetPage(m_view.get()));
    auto store = WKWebsiteDataStoreGetDefaultDataStore();

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

void WebKitBrowserWindow::resetFeatureMenu(FeatureType featureType, HMENU menu, bool resetsSettingsToDefaults)
{
    auto page = WKViewGetPage(m_view.get());
    auto pgroup = WKPageGetPageGroup(page);
    auto pref = WKPageGroupGetPreferences(pgroup);
    switch (featureType) {
    case FeatureType::Experimental: {
        auto features = adoptWK(WKPreferencesCopyExperimentalFeatures(pref));
        auto size = WKArrayGetSize(features.get());
        for (size_t i = 0; i < size; i++) {
            auto item = WKArrayGetItemAtIndex(features.get(), i);
            assert(WKGetTypeID(item) == WKExperimentalFeatureGetTypeID());
            auto feature = static_cast<WKExperimentalFeatureRef>(item);
            auto name = createString(adoptWK(WKExperimentalFeatureCopyName(feature)).get());
            auto key = adoptWK(WKExperimentalFeatureCopyKey(feature));
            bool defaultValue = WKExperimentalFeatureDefaultValue(feature);
            if (resetsSettingsToDefaults) {
                auto flag = MF_BYCOMMAND | (defaultValue ? MF_CHECKED : MF_UNCHECKED);
                CheckMenuItem(menu, IDM_EXPERIMENTAL_FEATURES_BEGIN + i, flag);
                WKPreferencesSetExperimentalFeatureForKey(pref, defaultValue, key.get());
            } else {
                auto flag = MF_STRING | (defaultValue ? MF_CHECKED : MF_UNCHECKED);
                AppendMenu(menu, flag, IDM_EXPERIMENTAL_FEATURES_BEGIN + i, name.c_str());
                m_experimentalFeatureKeys.push_back(std::move(key));
            }
        }
        break;
    }
    case FeatureType::InternalDebug: {
        auto features = adoptWK(WKPreferencesCopyInternalDebugFeatures(pref));
        auto size = WKArrayGetSize(features.get());
        for (size_t i = 0; i < size; i++) {
            auto item = WKArrayGetItemAtIndex(features.get(), i);
            assert(WKGetTypeID(item) == WKInternalDebugFeatureGetTypeID());
            auto feature = static_cast<WKInternalDebugFeatureRef>(item);
            auto name = createString(adoptWK(WKInternalDebugFeatureCopyName(feature)).get());
            auto key = adoptWK(WKInternalDebugFeatureCopyKey(feature));
            bool defaultValue = WKInternalDebugFeatureDefaultValue(feature);
            if (resetsSettingsToDefaults) {
                auto flag = MF_BYCOMMAND | (defaultValue ? MF_CHECKED : MF_UNCHECKED);
                CheckMenuItem(menu, IDM_INTERNAL_DEBUG_FEATURES_BEGIN + i, flag);
                WKPreferencesSetInternalDebugFeatureForKey(pref, defaultValue, key.get());
            } else {
                auto flag = MF_STRING | (defaultValue ? MF_CHECKED : MF_UNCHECKED);
                AppendMenu(menu, flag, IDM_INTERNAL_DEBUG_FEATURES_BEGIN + i, name.c_str());
                m_internalDebugFeatureKeys.push_back(std::move(key));
            }
        }
        break;
    }
    }
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

void WebKitBrowserWindow::reload()
{
    auto page = WKViewGetPage(m_view.get());
    WKPageReload(page);
}

void WebKitBrowserWindow::navigateForwardOrBackward(bool forward)
{
    auto page = WKViewGetPage(m_view.get());
    if (forward)
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
    if (IDM_EXPERIMENTAL_FEATURES_BEGIN <= menuID && menuID <= IDM_EXPERIMENTAL_FEATURES_END) {
        int index = menuID - IDM_EXPERIMENTAL_FEATURES_BEGIN;
        WKPreferencesSetExperimentalFeatureForKey(pref, enable, m_experimentalFeatureKeys[index].get());
        return;
    }
    if (IDM_INTERNAL_DEBUG_FEATURES_BEGIN <= menuID && menuID <= IDM_INTERNAL_DEBUG_FEATURES_END) {
        int index = menuID - IDM_INTERNAL_DEBUG_FEATURES_BEGIN;
        WKPreferencesSetInternalDebugFeatureForKey(pref, enable, m_internalDebugFeatureKeys[index].get());
        return;
    }
    switch (menuID) {
    case IDM_ACC_COMPOSITING:
        WKPreferencesSetAcceleratedCompositingEnabled(pref, enable);
        break;
    case IDM_COMPOSITING_BORDERS:
        WKPreferencesSetCompositingBordersVisible(pref, enable);
        WKPreferencesSetCompositingRepaintCountersVisible(pref, enable);
        break;
    case IDM_DEBUG_INFO_LAYER:
        WKPreferencesSetTiledScrollingIndicatorVisible(pref, enable);
        break;
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
    auto page = WKViewGetPage(m_view.get());
    auto name = createWKString("DumpLayerTree");
    WKPagePostMessageToInjectedBundle(page, name.get(), nullptr);
}

void WebKitBrowserWindow::updateStatistics(HWND hDlg)
{
    // Not implemented
}


void WebKitBrowserWindow::resetZoom()
{
    auto page = WKViewGetPage(m_view.get());
    WKPageSetPageZoomFactor(page, WebCore::deviceScaleFactorForWindow(hwnd()));
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

void WebKitBrowserWindow::didChangeTitle(const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    auto page = WKViewGetPage(thisWindow.m_view.get());
    WKRetainPtr<WKStringRef> title = adoptWK(WKPageCopyTitle(page));
    std::wstring titleString = createString(title.get()) + L" [WebKit]";
    SetWindowText(thisWindow.m_hMainWnd, titleString.c_str());
}

void WebKitBrowserWindow::didChangeIsLoading(const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    thisWindow.m_client.progressFinished();
}

void WebKitBrowserWindow::didChangeEstimatedProgress(const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    auto page = WKViewGetPage(thisWindow.m_view.get());
    thisWindow.m_client.progressChanged(WKPageGetEstimatedProgress(page));
}

void WebKitBrowserWindow::didChangeActiveURL(const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    auto page = WKViewGetPage(thisWindow.m_view.get());
    WKRetainPtr<WKURLRef> url = adoptWK(WKPageCopyActiveURL(page));
    thisWindow.m_client.activeURLChanged(createString(url.get()));
}

void WebKitBrowserWindow::didFailProvisionalNavigation(WKPageRef page, WKNavigationRef navigation, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    std::wstringstream text;
    text << createString(adoptWK(WKErrorCopyLocalizedDescription(error)).get()) << std::endl;
    text << L"Error Code: " << WKErrorGetErrorCode(error) << std::endl;
    text << L"Domain: " << createString(adoptWK(WKErrorCopyDomain(error)).get()) << std::endl;
    text << L"Failing URL: " << createString(adoptWK(WKErrorCopyFailingURL(error)).get());
    MessageBox(thisWindow.m_hMainWnd, text.str().c_str(), L"Provisional Navigation Failure", MB_OK | MB_ICONWARNING);
}

void WebKitBrowserWindow::didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef challenge, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    auto protectionSpace = WKAuthenticationChallengeGetProtectionSpace(challenge);
    auto decisionListener = WKAuthenticationChallengeGetDecisionListener(challenge);
    auto authenticationScheme = WKProtectionSpaceGetAuthenticationScheme(protectionSpace);

    if (authenticationScheme == kWKProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested) {
        if (thisWindow.canTrustServerCertificate(protectionSpace)) {
            WKRetainPtr<WKStringRef> username = createWKString("accept server trust");
            WKRetainPtr<WKStringRef> password = createWKString("");
            WKRetainPtr<WKCredentialRef> wkCredential = adoptWK(WKCredentialCreate(username.get(), password.get(), kWKCredentialPersistenceForSession));
            WKAuthenticationDecisionListenerUseCredential(decisionListener, wkCredential.get());
            return;
        }
    } else {
        WKRetainPtr<WKStringRef> realm(WKProtectionSpaceCopyRealm(protectionSpace));

        if (auto credential = askCredential(thisWindow.hwnd(), createString(realm.get()))) {
            WKRetainPtr<WKStringRef> username = createWKString(credential->username);
            WKRetainPtr<WKStringRef> password = createWKString(credential->password);
            WKRetainPtr<WKCredentialRef> wkCredential = adoptWK(WKCredentialCreate(username.get(), password.get(), kWKCredentialPersistenceForSession));
            WKAuthenticationDecisionListenerUseCredential(decisionListener, wkCredential.get());
            return;
        }
    }

    WKAuthenticationDecisionListenerUseCredential(decisionListener, nullptr);
}

bool WebKitBrowserWindow::canTrustServerCertificate(WKProtectionSpaceRef protectionSpace)
{
    auto host = createString(adoptWK(WKProtectionSpaceCopyHost(protectionSpace)).get());
    auto verificationError = WKProtectionSpaceGetCertificateVerificationError(protectionSpace);
    auto description = createString(adoptWK(WKProtectionSpaceCopyCertificateVerificationErrorDescription(protectionSpace)).get());
    auto pem = createPEMString(protectionSpace);

    auto it = m_acceptedServerTrustCerts.find(host);
    if (it != m_acceptedServerTrustCerts.end() && it->second == pem)
        return true;

    std::wstring textString = L"[HOST] " + host + L"\r\n";
    textString.append(L"[ERROR] " + std::to_wstring(verificationError) + L"\r\n");
    textString.append(L"[DESCRIPTION] " + description + L"\r\n");
    textString.append(pem);

    if (askServerTrustEvaluation(hwnd(), textString)) {
        m_acceptedServerTrustCerts.emplace(host, pem);
        return true;
    }

    return false;
}

WKPageRef WebKitBrowserWindow::createNewPage(WKPageRef page, WKPageConfigurationRef configuration, WKNavigationActionRef navigationAction, WKWindowFeaturesRef windowFeatures, const void *clientInfo)
{
    auto& newWindow = MainWindow::create().leakRef();
    auto factory = [configuration](BrowserWindowClient& client, HWND mainWnd, bool) -> auto {
        return adoptRef(*new WebKitBrowserWindow(client, configuration, mainWnd));
    };
    bool ok = newWindow.init(factory, hInst);
    if (!ok)
        return nullptr;
    ShowWindow(newWindow.hwnd(), SW_SHOW);
    auto& newBrowserWindow = *static_cast<WebKitBrowserWindow*>(newWindow.browserWindow());
    WKRetainPtr<WKPageRef> newPage = WKViewGetPage(newBrowserWindow.m_view.get());
    return newPage.leakRef();
}

void WebKitBrowserWindow::didNotHandleKeyEvent(WKPageRef, WKNativeEventPtr event, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    PostMessage(thisWindow.m_hMainWnd, event->message, event->wParam, event->lParam);
}

void WebKitBrowserWindow::runJavaScriptAlert(WKPageRef page, WKStringRef alertText, WKFrameRef frame, WKSecurityOriginRef securityOrigin, WKPageRunJavaScriptAlertResultListenerRef listener, const void *clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    std::wstring title = L"Alert: ";
    title += createString(adoptWK(WKSecurityOriginCopyToString(securityOrigin)).get());
    auto text = createString(alertText);
    MessageBox(thisWindow.m_hMainWnd, text.c_str(), title.c_str(), MB_OK);
    WKPageRunJavaScriptAlertResultListenerCall(listener);
}

void WebKitBrowserWindow::runJavaScriptConfirm(WKPageRef page, WKStringRef message, WKFrameRef frame, WKSecurityOriginRef securityOrigin, WKPageRunJavaScriptConfirmResultListenerRef listener, const void *clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    std::wstring title = L"Confirm: ";
    title += createString(adoptWK(WKSecurityOriginCopyToString(securityOrigin)).get());
    auto text = createString(message);
    bool result = MessageBox(thisWindow.m_hMainWnd, text.c_str(), title.c_str(), MB_OKCANCEL) == IDOK;
    WKPageRunJavaScriptConfirmResultListenerCall(listener, result);
}

void WebKitBrowserWindow::runJavaScriptPrompt(WKPageRef page, WKStringRef message, WKStringRef defaultValue, WKFrameRef frame, WKSecurityOriginRef securityOrigin, WKPageRunJavaScriptPromptResultListenerRef listener, const void *clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    std::wstring title = L"Prompt: ";
    title += createString(adoptWK(WKSecurityOriginCopyToString(securityOrigin)).get());
    auto text = createString(message);
    text += L"\nDefault Value: " + createString(defaultValue);
    MessageBox(thisWindow.m_hMainWnd, text.c_str(), title.c_str(), MB_OK);
    WKPageRunJavaScriptPromptResultListenerCall(listener, defaultValue);
}

