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
#include <WebKit/WKDownloadClient.h>
#include <WebKit/WKDownloadRef.h>
#include <WebKit/WKFrameInfoRef.h>
#include <WebKit/WKHTTPCookieStoreRef.h>
#include <WebKit/WKInspector.h>
#include <WebKit/WKNavigationResponseRef.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKProtectionSpace.h>
#include <WebKit/WKProtectionSpaceCurl.h>
#include <WebKit/WKSecurityOriginRef.h>
#include <WebKit/WKURLResponse.h>
#include <WebKit/WKWebsiteDataStoreConfigurationRef.h>
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

    for (size_t i = 0; i < WKArrayGetSize(chain.get()); i++) {
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
    auto websiteDataStoreConf = adoptWK(WKWebsiteDataStoreConfigurationCreate());

    auto websiteDataStore = adoptWK(WKWebsiteDataStoreCreateWithConfiguration(websiteDataStoreConf.get()));

    auto contextConf = adoptWK(WKContextConfigurationCreate());
    WKContextConfigurationSetInjectedBundlePath(contextConf.get(), injectedBundlePath().get());

    auto context = adoptWK(WKContextCreateWithConfiguration(contextConf.get()));

    auto preferences = adoptWK(WKPreferencesCreate());
    WKPreferencesSetMediaCapabilitiesEnabled(preferences.get(), false);
    WKPreferencesSetDeveloperExtrasEnabled(preferences.get(), true);

    auto pageConf = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetWebsiteDataStore(pageConf.get(), websiteDataStore.get());
    WKPageConfigurationSetContext(pageConf.get(), context.get());
    WKPageConfigurationSetPreferences(pageConf.get(), preferences.get());

    return adoptRef(*new WebKitBrowserWindow(client, pageConf.get(), mainWnd));
}

WebKitBrowserWindow::WebKitBrowserWindow(BrowserWindowClient& client, WKPageConfigurationRef pageConf, HWND mainWnd)
    : m_client(client)
    , m_hMainWnd(mainWnd)
{
    RECT rect = { };
    m_view = adoptWK(WKViewCreate(rect, pageConf, mainWnd));
    WKViewSetIsInWindow(m_view.get(), true);

    auto page = WKViewGetPage(m_view.get());

    WKPageNavigationClientV3 navigationClient = { };
    navigationClient.base.version = 3;
    navigationClient.base.clientInfo = this;
    navigationClient.decidePolicyForNavigationResponse = decidePolicyForNavigationResponse;
    navigationClient.didFailProvisionalNavigation = didFailProvisionalNavigation;
    navigationClient.didReceiveAuthenticationChallenge = didReceiveAuthenticationChallenge;
    navigationClient.navigationActionDidBecomeDownload = navigationActionDidBecomeDownload;
    navigationClient.navigationResponseDidBecomeDownload = navigationResponseDidBecomeDownload;
    navigationClient.contextMenuDidCreateDownload = contextMenuDidCreateDownload;
    WKPageSetPageNavigationClient(page, &navigationClient.base);

    WKPageUIClientV13 uiClient = { };
    uiClient.base.version = 13;
    uiClient.base.clientInfo = this;
    uiClient.createNewPage = createNewPage;
    uiClient.close = close;
    uiClient.didNotHandleKeyEvent = didNotHandleKeyEvent;
    uiClient.getWindowFrame = getWindowFrame;
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

    adjustScaleFactors();
}

void WebKitBrowserWindow::updateProxySettings()
{
    auto page = WKViewGetPage(m_view.get());
    auto websiteDataStore = WKPageGetWebsiteDataStore(page);

    if (!m_proxy.enable) {
        WKWebsiteDataStoreDisableNetworkProxySettings(websiteDataStore);
        return;
    }

    if (!m_proxy.custom) {
        WKWebsiteDataStoreEnableDefaultNetworkProxySettings(websiteDataStore);
        return;
    }

    auto url = createWKURL(m_proxy.url);
    auto excludeHosts = createWKString(m_proxy.excludeHosts);
    WKWebsiteDataStoreEnableCustomNetworkProxySettings(websiteDataStore, url.get(), excludeHosts.get());
}

// FIXME: The current design of WebKit produces too many noises on fractional device scale factor.
// This rounds device scale factor and tweaks zoom factor for tantative workarond.
void WebKitBrowserWindow::adjustScaleFactors()
{
    WKPageRef page = WKViewGetPage(m_view.get());

    float customZoomRatio = WKPageGetPageZoomFactor(page) / m_defaultResetPageZoomFactor;
    float deviceScaleFactor = WebCore::deviceScaleFactorForWindow(hwnd());
    int roundedDeviceScaleFactor = std::round(deviceScaleFactor);
    m_defaultResetPageZoomFactor = deviceScaleFactor / roundedDeviceScaleFactor;

    WKPageSetCustomBackingScaleFactor(page, roundedDeviceScaleFactor);
    WKPageSetPageZoomFactor(page, m_defaultResetPageZoomFactor * customZoomRatio);
}

HRESULT WebKitBrowserWindow::init()
{
    return S_OK;
}

void WebKitBrowserWindow::resetFeatureMenu(FeatureType featureType, HMENU menu, bool resetsSettingsToDefaults)
{
    auto page = WKViewGetPage(m_view.get());
    auto configuration = adoptWK(WKPageCopyPageConfiguration(page));
    auto pref = WKPageConfigurationGetPreferences(configuration.get());
    switch (featureType) {
    case FeatureType::Experimental: {
        auto features = adoptWK(WKPreferencesCopyExperimentalFeatures(pref));
        auto size = WKArrayGetSize(features.get());
        for (size_t i = 0; i < size; i++) {
            auto item = WKArrayGetItemAtIndex(features.get(), i);
            assert(WKGetTypeID(item) == WKFeatureGetTypeID());
            auto feature = static_cast<WKFeatureRef>(item);
            auto name = createString(adoptWK(WKFeatureCopyName(feature)).get());
            auto key = adoptWK(WKFeatureCopyKey(feature));
            bool defaultValue = WKFeatureDefaultValue(feature);
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
            assert(WKGetTypeID(item) == WKFeatureGetTypeID());
            auto feature = static_cast<WKFeatureRef>(item);
            auto name = createString(adoptWK(WKFeatureCopyName(feature)).get());
            auto key = adoptWK(WKFeatureCopyKey(feature));
            bool defaultValue = WKFeatureDefaultValue(feature);
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

void WebKitBrowserWindow::navigateToHistory(UINT)
{
    // Not implemented
}

void WebKitBrowserWindow::setPreference(UINT menuID, bool enable)
{
    auto page = WKViewGetPage(m_view.get());
    auto configuration = adoptWK(WKPageCopyPageConfiguration(page));
    auto pref = WKPageConfigurationGetPreferences(configuration.get());
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

void WebKitBrowserWindow::updateStatistics(HWND)
{
    // Not implemented
}


void WebKitBrowserWindow::resetZoom()
{
    auto page = WKViewGetPage(m_view.get());
    WKPageSetPageZoomFactor(page, m_defaultResetPageZoomFactor);
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

static void deleteAllCookiesCallback(void*)
{

}

void WebKitBrowserWindow::clearCookies()
{
    auto page = WKViewGetPage(m_view.get());
    auto websiteDataStore = WKPageGetWebsiteDataStore(page);

    WKHTTPCookieStoreRef cookieStore = WKWebsiteDataStoreGetHTTPCookieStore(websiteDataStore);
    if (cookieStore)
        WKHTTPCookieStoreDeleteAllCookies(cookieStore, this, deleteAllCookiesCallback);
}

static void removeAllFetchCachesCallback(void*)
{
}

static void removeNetworkCacheCallback(void*)
{
}

static void removeMemoryCachesCallback(void*)
{
}

static void removeAllServiceWorkerRegistrationsCallback(void*)
{
}

static void removeAllIndexedDatabasesCallback(void*)
{
}

static void removeLocalStorageCallback(void*)
{
}
    
void WebKitBrowserWindow::clearWebsiteData()
{
    auto page = WKViewGetPage(m_view.get());
    auto websiteDataStore = WKPageGetWebsiteDataStore(page);

    WKWebsiteDataStoreRemoveAllFetchCaches(websiteDataStore, this, removeAllFetchCachesCallback);
    WKWebsiteDataStoreRemoveNetworkCache(websiteDataStore, this, removeNetworkCacheCallback);
    WKWebsiteDataStoreRemoveMemoryCaches(websiteDataStore, this, removeMemoryCachesCallback);
    WKWebsiteDataStoreRemoveAllServiceWorkerRegistrations(websiteDataStore, this, removeAllServiceWorkerRegistrationsCallback);
    WKWebsiteDataStoreRemoveAllIndexedDatabases(websiteDataStore, this, removeAllIndexedDatabasesCallback);
    WKWebsiteDataStoreRemoveLocalStorage(websiteDataStore, this, removeLocalStorageCallback);
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

void WebKitBrowserWindow::decidePolicyForNavigationResponse(WKPageRef, WKNavigationResponseRef navigationResponse, WKFramePolicyListenerRef listener, WKTypeRef, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    auto response = adoptWK(WKNavigationResponseCopyResponse(navigationResponse));
    auto code = WKURLResponseHTTPStatusCode(response.get());
    auto expectedContentLength = WKURLResponseGetExpectedContentLength(response.get());
    if (code > 300 && !expectedContentLength) {
        std::wstringstream text;
        text << L"Error Code: " << code << std::endl;
        text << L"MIME type: " << createString(adoptWK(WKURLResponseCopyMIMEType(response.get())).get()) << std::endl;
        text << L"URL: " << createString(adoptWK(WKURLResponseCopyURL(response.get())).get());
        MessageBox(thisWindow.m_hMainWnd, text.str().c_str(), L"No Content", MB_OK | MB_ICONWARNING);
    }

    auto isMainFrame = WKFrameInfoGetIsMainFrame(adoptWK(WKNavigationResponseCopyFrameInfo(navigationResponse)).get());
    if (WKNavigationResponseCanShowMIMEType(navigationResponse) || !isMainFrame)
        WKFramePolicyListenerUse(listener);
    else {
        std::wstringstream text;
        text << L"Do you want to save this file?" << std::endl;
        text << std::endl;
        text << L"MIME type: " << createString(adoptWK(WKURLResponseCopyMIMEType(response.get())).get()) << std::endl;
        text << L"URL: " << createString(adoptWK(WKURLResponseCopyURL(response.get())).get());

        if (MessageBox(thisWindow.hwnd(), text.str().c_str(), L"Unsupported MIME type", MB_OKCANCEL | MB_ICONWARNING) == IDOK)
            WKFramePolicyListenerDownload(listener);
        else
            WKFramePolicyListenerIgnore(listener);
    }
}

void WebKitBrowserWindow::didFailProvisionalNavigation(WKPageRef, WKNavigationRef, WKErrorRef error, WKTypeRef, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    std::wstringstream text;
    text << createString(adoptWK(WKErrorCopyLocalizedDescription(error)).get()) << std::endl;
    text << L"Error Code: " << WKErrorGetErrorCode(error) << std::endl;
    text << L"Domain: " << createString(adoptWK(WKErrorCopyDomain(error)).get()) << std::endl;
    text << L"Failing URL: " << createString(adoptWK(WKErrorCopyFailingURL(error)).get());
    MessageBox(thisWindow.m_hMainWnd, text.str().c_str(), L"Provisional Navigation Failure", MB_OK | MB_ICONWARNING);
}

void WebKitBrowserWindow::didReceiveAuthenticationChallenge(WKPageRef, WKAuthenticationChallengeRef challenge, const void* clientInfo)
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

void WebKitBrowserWindow::navigationActionDidBecomeDownload(WKPageRef, WKNavigationActionRef, WKDownloadRef download, const void* clientInfo)
{
    setDownloadClient(download, clientInfo);
}

void WebKitBrowserWindow::navigationResponseDidBecomeDownload(WKPageRef, WKNavigationResponseRef, WKDownloadRef download, const void* clientInfo)
{
    setDownloadClient(download, clientInfo);
}

void WebKitBrowserWindow::contextMenuDidCreateDownload(WKPageRef, WKDownloadRef download, const void* clientInfo)
{
    setDownloadClient(download, clientInfo);
}

void WebKitBrowserWindow::setDownloadClient(WKDownloadRef download, const void* clientInfo)
{
    WKDownloadClientV0 client = { };
    client.base = { 0, clientInfo };
    client.decideDestinationWithResponse = downloadDecideDestinationWithResponse;
    client.didFinish = downloadDidFinish;
    client.didFailWithError = downloadDidFailWithError;

    WKDownloadSetClient(download, &client.base);
}

WKStringRef WebKitBrowserWindow::downloadDecideDestinationWithResponse(WKDownloadRef, WKURLResponseRef response, WKStringRef suggestedFilename, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);

    auto filename = createString(suggestedFilename);
    if (filename.empty()) {
        auto urlRef = adoptWK(WKURLResponseCopyURL(response));
        auto hostRef = adoptWK(WKURLCopyHostName(urlRef.get()));
        filename = createString(hostRef.get());
    }

    std::wstring folderPath;
    if (!getKnownFolderPath(FOLDERID_Downloads, folderPath)) {
        MessageBox(thisWindow.hwnd(), L"Could not get Downloads folder path.", L"Download Failure", MB_OK | MB_ICONWARNING);
        return nullptr;
    }

    return createWKString(folderPath + L"\\" + filename).leakRef();
}

void WebKitBrowserWindow::downloadDidFinish(WKDownloadRef, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    MessageBox(thisWindow.hwnd(), L"File has been downloaded successfully.", L"Download", MB_OK);
}

void WebKitBrowserWindow::downloadDidFailWithError(WKDownloadRef, WKErrorRef error, WKDataRef, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    std::wstringstream text;
    text << createString(adoptWK(WKErrorCopyLocalizedDescription(error)).get()) << std::endl;
    text << L"Error Code: " << WKErrorGetErrorCode(error) << std::endl;
    text << L"Domain: " << createString(adoptWK(WKErrorCopyDomain(error)).get()) << std::endl;
    text << L"Failing URL: " << createString(adoptWK(WKErrorCopyFailingURL(error)).get());
    MessageBox(thisWindow.hwnd(), text.str().c_str(), L"Download Failure", MB_OK | MB_ICONWARNING);
}

void WebKitBrowserWindow::close(WKPageRef, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    PostMessage(thisWindow.hwnd(), WM_CLOSE, 0, 0);
}

WKPageRef WebKitBrowserWindow::createNewPage(WKPageRef, WKPageConfigurationRef pageConf, WKNavigationActionRef, WKWindowFeaturesRef, const void*)
{
    auto& newWindow = MainWindow::create().leakRef();
    auto factory = [pageConf](BrowserWindowClient& client, HWND mainWnd, bool) -> auto {
        return adoptRef(*new WebKitBrowserWindow(client, pageConf, mainWnd));
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

WKRect WebKitBrowserWindow::getWindowFrame(WKPageRef, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    RECT rect;
    GetWindowRect(thisWindow.hwnd(), &rect);
    return WKRectMake(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

void WebKitBrowserWindow::runJavaScriptAlert(WKPageRef, WKStringRef alertText, WKFrameRef, WKSecurityOriginRef securityOrigin, WKPageRunJavaScriptAlertResultListenerRef listener, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    std::wstring title = L"Alert: ";
    title += createString(adoptWK(WKSecurityOriginCopyToString(securityOrigin)).get());
    auto text = createString(alertText);
    MessageBox(thisWindow.m_hMainWnd, text.c_str(), title.c_str(), MB_OK);
    WKPageRunJavaScriptAlertResultListenerCall(listener);
}

void WebKitBrowserWindow::runJavaScriptConfirm(WKPageRef, WKStringRef message, WKFrameRef, WKSecurityOriginRef securityOrigin, WKPageRunJavaScriptConfirmResultListenerRef listener, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    std::wstring title = L"Confirm: ";
    title += createString(adoptWK(WKSecurityOriginCopyToString(securityOrigin)).get());
    auto text = createString(message);
    bool result = MessageBox(thisWindow.m_hMainWnd, text.c_str(), title.c_str(), MB_OKCANCEL) == IDOK;
    WKPageRunJavaScriptConfirmResultListenerCall(listener, result);
}

void WebKitBrowserWindow::runJavaScriptPrompt(WKPageRef, WKStringRef message, WKStringRef defaultValue, WKFrameRef, WKSecurityOriginRef securityOrigin, WKPageRunJavaScriptPromptResultListenerRef listener, const void* clientInfo)
{
    auto& thisWindow = toWebKitBrowserWindow(clientInfo);
    std::wstring title = L"Prompt: ";
    title += createString(adoptWK(WKSecurityOriginCopyToString(securityOrigin)).get());
    auto text = createString(message);
    text += L"\nDefault Value: " + createString(defaultValue);
    MessageBox(thisWindow.m_hMainWnd, text.c_str(), title.c_str(), MB_OK);
    WKPageRunJavaScriptPromptResultListenerCall(listener, defaultValue);
}

