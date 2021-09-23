/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(REMOTE_INSPECTOR)

#include "InspectorPlaywrightAgentClient.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include "WebPageInspectorController.h"
#include "WebProcessPool.h"
#include "DownloadProxy.h"
#include <wtf/HashMap.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace Inspector {
class BackendDispatcher;
class FrontendChannel;
class FrontendRouter;
class PlaywrightFrontendDispatcher;
}

namespace PAL {
class SessionID;
}

namespace WebKit {

class WebFrameProxy;

class InspectorPlaywrightAgent final
    : public WebPageInspectorControllerObserver
    , public Inspector::PlaywrightBackendDispatcherHandler
    , public DownloadInstrumentation {
    WTF_MAKE_NONCOPYABLE(InspectorPlaywrightAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit InspectorPlaywrightAgent(std::unique_ptr<InspectorPlaywrightAgentClient> client);
    ~InspectorPlaywrightAgent() override;

    // Transport
    void connectFrontend(Inspector::FrontendChannel&);
    void disconnectFrontend();
    void dispatchMessageFromFrontend(const String& message);

private:
    class BrowserContextDeletion;
    class PageProxyChannel;
    class TargetHandler;

    // WebPageInspectorControllerObserver
    void didCreateInspectorController(WebPageProxy&) override;
    void willDestroyInspectorController(WebPageProxy&) override;
    void didFailProvisionalLoad(WebPageProxy&, uint64_t navigationID, const String& error) override;
    void willCreateNewPage(WebPageProxy&, const WebCore::WindowFeatures&, const URL&) override;
    void didFinishScreencast(const PAL::SessionID& sessionID, const String& screencastID) override;

    // PlaywrightDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable() override;
    Inspector::Protocol::ErrorStringOr<void> disable() override;
    void close(Ref<CloseCallback>&&) override;
    Inspector::Protocol::ErrorStringOr<String /* browserContextID */> createContext(const String& proxyServer, const String& proxyBypassList) override;
    void deleteContext(const String& browserContextID, Ref<DeleteContextCallback>&& callback) override;
    Inspector::Protocol::ErrorStringOr<String /* pageProxyID */> createPage(const String& browserContextID) override;
    void navigate(const String& url, const String& pageProxyID, const String& frameId, const String& referrer, Ref<NavigateCallback>&&) override;
    Inspector::Protocol::ErrorStringOr<void> setIgnoreCertificateErrors(const String& browserContextID, bool ignore) override;

    void getAllCookies(const String& browserContextID, Ref<GetAllCookiesCallback>&&) override;
    void setCookies(const String& browserContextID, Ref<JSON::Array>&& in_cookies, Ref<SetCookiesCallback>&&) override;
    void deleteAllCookies(const String& browserContextID, Ref<DeleteAllCookiesCallback>&&) override;

    void getLocalStorageData(const String& browserContextID, Ref<GetLocalStorageDataCallback>&&) override;
    void setLocalStorageData(const String& browserContextID, Ref<JSON::Array>&& origins, Ref<SetLocalStorageDataCallback>&&) override;

    Inspector::Protocol::ErrorStringOr<void> setGeolocationOverride(const String& browserContextID, RefPtr<JSON::Object>&& geolocation) override;
    Inspector::Protocol::ErrorStringOr<void> setLanguages(Ref<JSON::Array>&& languages, const String& browserContextID) override;
    Inspector::Protocol::ErrorStringOr<void> setDownloadBehavior(const String& behavior, const String& downloadPath, const String& browserContextID) override;
    Inspector::Protocol::ErrorStringOr<void> cancelDownload(const String& uuid) override;

    // DownloadInstrumentation
    void downloadCreated(const String& uuid, const WebCore::ResourceRequest&, const FrameInfoData& frameInfoData, WebPageProxy* page, RefPtr<DownloadProxy> download) override;
    void downloadFilenameSuggested(const String& uuid, const String& suggestedFilename) override;
    void downloadFinished(const String& uuid, const String& error) override;

    BrowserContext* getExistingBrowserContext(const String& browserContextID);
    BrowserContext* lookupBrowserContext(Inspector::ErrorString&, const String& browserContextID);
    WebFrameProxy* frameForID(const String& frameID, String& error);
    void closeImpl(Function<void(String)>&&);

    Inspector::FrontendChannel* m_frontendChannel { nullptr };
    Ref<Inspector::FrontendRouter> m_frontendRouter;
    Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    std::unique_ptr<InspectorPlaywrightAgentClient> m_client;
    std::unique_ptr<Inspector::PlaywrightFrontendDispatcher> m_frontendDispatcher;
    Ref<Inspector::PlaywrightBackendDispatcher> m_playwrightDispatcher;
    HashMap<String, std::unique_ptr<PageProxyChannel>> m_pageProxyChannels;
    BrowserContext* m_defaultContext;
    HashMap<String, RefPtr<DownloadProxy>> m_downloads;
    HashMap<String, std::unique_ptr<BrowserContext>> m_browserContexts;
    HashMap<String, std::unique_ptr<BrowserContextDeletion>> m_browserContextDeletions;
    bool m_isEnabled { false };
};

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
