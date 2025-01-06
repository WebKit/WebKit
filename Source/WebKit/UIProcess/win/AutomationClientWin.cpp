/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "AutomationClientWin.h"

#if ENABLE(REMOTE_INSPECTOR)
#include "APIPageConfiguration.h"
#include "WKAPICast.h"
#include "WebAutomationSession.h"
#include "WebPageProxy.h"
#include <WebKit/WKAuthenticationChallenge.h>
#include <WebKit/WKAuthenticationDecisionListener.h>
#include <WebKit/WKCredential.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKString.h>
#include <wtf/RunLoop.h>
#endif

namespace WebKit {

#if ENABLE(REMOTE_INSPECTOR)

// AutomationSessionClient
AutomationSessionClient::AutomationSessionClient(const String& sessionIdentifier, const Inspector::RemoteInspector::Client::SessionCapabilities& capabilities)
    : m_sessionIdentifier(sessionIdentifier)
    , m_capabilities(capabilities)
{
}

void AutomationSessionClient::close(WKPageRef pageRef, const void* clientInfo)
{
    auto page = WebKit::toImpl(pageRef);
    page->setControlledByAutomation(false);

    auto sessionClient = static_cast<AutomationSessionClient*>(const_cast<void*>(clientInfo));
    sessionClient->releaseWebView(page);
}

void AutomationSessionClient::didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef authenticationChallenge, const void *clientInfo)
{
    static_cast<AutomationSessionClient*>(const_cast<void*>(clientInfo))->didReceiveAuthenticationChallenge(page, authenticationChallenge);
}

void AutomationSessionClient::didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef authenticationChallenge)
{
    auto decisionListener = WKAuthenticationChallengeGetDecisionListener(authenticationChallenge);
    if (m_capabilities.acceptInsecureCertificates) {
        auto username = adoptWK(WKStringCreateWithUTF8CString("accept server trust"));
        auto password = adoptWK(WKStringCreateWithUTF8CString(""));
        auto credential = adoptWK(WKCredentialCreate(username.get(), password.get(), kWKCredentialPersistenceNone));
        WKAuthenticationDecisionListenerUseCredential(decisionListener, credential.get());
    } else
        WKAuthenticationDecisionListenerRejectProtectionSpaceAndContinue(decisionListener);
}

void AutomationSessionClient::requestNewPageWithOptions(WebKit::WebAutomationSession& session, API::AutomationSessionBrowsingContextOptions options, CompletionHandler<void(WebKit::WebPageProxy*)>&& completionHandler)
{
    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(session.protectedProcessPool());

    RECT r { };
    Ref newWindow = WebView::create(r, pageConfiguration, 0);

    auto newPage = newWindow->page();
    newPage->setControlledByAutomation(true);

    WKPageUIClientV0 uiClient = { };
    uiClient.base.version = 0;
    uiClient.base.clientInfo = this;
    uiClient.close = close;
    WKPageSetPageUIClient(toAPI(newPage), &uiClient.base);

    WKPageNavigationClientV0 navigationClient = { };
    navigationClient.base.version = 0;
    navigationClient.base.clientInfo = this;
    navigationClient.didReceiveAuthenticationChallenge = didReceiveAuthenticationChallenge;
    WKPageSetPageNavigationClient(toAPI(newPage), &navigationClient.base);

    retainWebView(WTFMove(newWindow));

    completionHandler(newPage);
}

void AutomationSessionClient::didDisconnectFromRemote(WebKit::WebAutomationSession& session)
{
    session.setClient(nullptr);

    RunLoop::main().dispatch([&session] {
        auto processPool = session.protectedProcessPool();
        if (processPool) {
            processPool->setAutomationSession(nullptr);
            processPool->setPagesControlledByAutomation(false);
        }
    });
}

void AutomationSessionClient::retainWebView(Ref<WebView>&& webView)
{
    m_webViews.add(WTFMove(webView));
}

void AutomationSessionClient::releaseWebView(WebPageProxy* page)
{
    m_webViews.removeIf([&](auto& view) {
        if (view->page() == page) {
            view->close();
            return true;
        }
        return false;
    });
}

// AutomationClient
AutomationClient::AutomationClient(WebProcessPool& processPool)
    : m_processPool(processPool)
{
    Inspector::RemoteInspector::singleton().setClient(this);
}

AutomationClient::~AutomationClient()
{
    Inspector::RemoteInspector::singleton().setClient(nullptr);
}

RefPtr<WebProcessPool> AutomationClient::protectedProcessPool() const
{
    if (RefPtr processPool = m_processPool.get())
        return processPool;

    return nullptr;
}

void AutomationClient::requestAutomationSession(const String& sessionIdentifier, const Inspector::RemoteInspector::Client::SessionCapabilities& capabilities)
{
    ASSERT(isMainRunLoop());

    auto session = adoptRef(new WebAutomationSession());
    session->setSessionIdentifier(sessionIdentifier);
    session->setClient(WTF::makeUnique<AutomationSessionClient>(sessionIdentifier, capabilities));
    m_processPool->setAutomationSession(WTFMove(session));
}

void AutomationClient::closeAutomationSession()
{
    RunLoop::main().dispatch([this] {
        auto processPool = protectedProcessPool();
        if (!processPool || !processPool->automationSession())
            return;

        processPool->automationSession()->setClient(nullptr);
        processPool->setAutomationSession(nullptr);
    });
}

#endif

} // namespace WebKit
