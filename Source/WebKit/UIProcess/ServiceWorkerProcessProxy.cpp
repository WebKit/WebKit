/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ServiceWorkerProcessProxy.h"

#if ENABLE(SERVICE_WORKER)

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "WebCredential.h"
#include "WebPageGroup.h"
#include "WebPreferencesStore.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebSWContextManagerConnectionMessages.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/RegistrationDatabase.h>

namespace WebKit {
using namespace WebCore;

Ref<ServiceWorkerProcessProxy> ServiceWorkerProcessProxy::create(WebProcessPool& pool, const SecurityOriginData& securityOrigin, WebsiteDataStore& store)
{
    auto proxy = adoptRef(*new ServiceWorkerProcessProxy { pool, securityOrigin, store });
    proxy->connect();
    return proxy;
}

ServiceWorkerProcessProxy::ServiceWorkerProcessProxy(WebProcessPool& pool, const SecurityOriginData& securityOrigin, WebsiteDataStore& store)
    : WebProcessProxy { pool, store, IsPrewarmed::No }
    , m_securityOrigin(securityOrigin)
    , m_serviceWorkerPageID(generatePageID())
{
}

ServiceWorkerProcessProxy::~ServiceWorkerProcessProxy()
{
}

bool ServiceWorkerProcessProxy::hasRegisteredServiceWorkers(const String& serviceWorkerDirectory)
{
    String registrationFile = WebCore::serviceWorkerRegistrationDatabaseFilename(serviceWorkerDirectory);
    return WebCore::FileSystem::fileExists(registrationFile);
}

void ServiceWorkerProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    WebProcessProxy::getLaunchOptions(launchOptions);

    launchOptions.extraInitializationData.add("service-worker-process"_s, "1"_s);
    launchOptions.extraInitializationData.add("security-origin"_s, securityOrigin().toString());
}

void ServiceWorkerProcessProxy::start(const WebPreferencesStore& store, Optional<PAL::SessionID> initialSessionID)
{
    send(Messages::WebProcess::EstablishWorkerContextConnectionToNetworkProcess { processPool().defaultPageGroup().pageGroupID(), m_serviceWorkerPageID, store, initialSessionID.value_or(PAL::SessionID::defaultSessionID()) }, 0);
}

void ServiceWorkerProcessProxy::setUserAgent(const String& userAgent)
{
    send(Messages::WebSWContextManagerConnection::SetUserAgent { userAgent }, 0);
}

void ServiceWorkerProcessProxy::updatePreferencesStore(const WebPreferencesStore& store)
{
    send(Messages::WebSWContextManagerConnection::UpdatePreferencesStore { store }, 0);
}

void ServiceWorkerProcessProxy::didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, Ref<AuthenticationChallengeProxy>&& challenge)
{
    UNUSED_PARAM(pageID);
    UNUSED_PARAM(frameID);

    // FIXME: Expose an API to delegate the actual decision to the application layer.
    auto& protectionSpace = challenge->core().protectionSpace();
    if (protectionSpace.authenticationScheme() == WebCore::ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested && processPool().allowsAnySSLCertificateForServiceWorker()) {
        auto credential = WebCore::Credential("accept server trust"_s, emptyString(), WebCore::CredentialPersistenceNone);
        challenge->listener().completeChallenge(AuthenticationChallengeDisposition::UseCredential, credential);
        return;
    }
    notImplemented();
    challenge->listener().completeChallenge(AuthenticationChallengeDisposition::PerformDefaultHandling);
}

void ServiceWorkerProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    WebProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    // Prevent App Nap for Service Worker processes.
    // FIXME: Ideally, the Service Worker process would app nap when all its clients app nap (http://webkit.org/b/185626).
    setProcessSuppressionEnabled(false);
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
