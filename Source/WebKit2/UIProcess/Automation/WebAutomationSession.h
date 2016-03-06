/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebAutomationSession_h
#define WebAutomationSession_h

#include "APIObject.h"
#include "InspectorBackendDispatchers.h"
#include <wtf/Forward.h>

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteAutomationTarget.h>
#endif

namespace API {
class AutomationSessionClient;
}

namespace Inspector {
class BackendDispatcher;
class FrontendRouter;
}

namespace WebKit {

class WebAutomationSessionClient;
class WebPageProxy;
class WebProcessPool;

class WebAutomationSession final : public API::ObjectImpl<API::Object::Type::AutomationSession>
#if ENABLE(REMOTE_INSPECTOR)
    , public Inspector::RemoteAutomationTarget
#endif
    , public Inspector::AutomationBackendDispatcherHandler
{
public:
    typedef HashMap<uint64_t, String> WebPageHandleMap;
    typedef HashMap<String, uint64_t> HandleWebPageMap;

    WebAutomationSession();
    ~WebAutomationSession();

    void setClient(std::unique_ptr<API::AutomationSessionClient>);

    void setSessionIdentifier(const String& sessionIdentifier) { m_sessionIdentifier = sessionIdentifier; }
    String sessionIdentifier() const { return m_sessionIdentifier; }

    WebKit::WebProcessPool* processPool() const { return m_processPool; }
    void setProcessPool(WebKit::WebProcessPool* processPool) { m_processPool = processPool; }

#if ENABLE(REMOTE_INSPECTOR)
    // Inspector::RemoteAutomationTarget API
    String name() const override { return m_sessionIdentifier; }
    void dispatchMessageFromRemote(const String& message) override;
    void connect(Inspector::FrontendChannel*, bool isAutomaticConnection = false) override;
    void disconnect(Inspector::FrontendChannel*) override;
#endif

    // Inspector::AutomationBackendDispatcherHandler API
    void getBrowsingContexts(Inspector::ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Automation::BrowsingContext>>&) override;
    void createBrowsingContext(Inspector::ErrorString&, String*) override;
    void closeBrowsingContext(Inspector::ErrorString&, const String&) override;
    void switchToBrowsingContext(Inspector::ErrorString&, const String&) override;

private:
    WebKit::WebPageProxy* webPageProxyForHandle(const String&);
    String handleForWebPageProxy(WebKit::WebPageProxy*);

    WebKit::WebProcessPool* m_processPool { nullptr };
    std::unique_ptr<API::AutomationSessionClient> m_client;
    String m_sessionIdentifier { ASCIILiteral("Untitled Session") };
    Ref<Inspector::FrontendRouter> m_frontendRouter;
    Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    Ref<Inspector::AutomationBackendDispatcher> m_domainDispatcher;

    WebPageHandleMap m_webPageHandleMap;
    HandleWebPageMap m_handleWebPageMap;
    String m_activeBrowsingContextHandle;

#if ENABLE(REMOTE_INSPECTOR)
    Inspector::FrontendChannel* m_remoteChannel { nullptr };
#endif
};

} // namespace WebKit

#endif // WebAutomationSession_h
