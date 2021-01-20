/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "WebGeolocationManagerProxy.h"

#include "APIGeolocationProvider.h"
#include "Logging.h"
#include "WebGeolocationManagerMessages.h"
#include "WebGeolocationManagerProxyMessages.h"
#include "WebGeolocationPosition.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, (&connection))

namespace WebKit {

const char* WebGeolocationManagerProxy::supplementName()
{
    return "WebGeolocationManagerProxy";
}

Ref<WebGeolocationManagerProxy> WebGeolocationManagerProxy::create(WebProcessPool* processPool)
{
    return adoptRef(*new WebGeolocationManagerProxy(processPool));
}

WebGeolocationManagerProxy::WebGeolocationManagerProxy(WebProcessPool* processPool)
    : WebContextSupplement(processPool)
    , m_provider(makeUnique<API::GeolocationProvider>())
{
    WebContextSupplement::processPool()->addMessageReceiver(Messages::WebGeolocationManagerProxy::messageReceiverName(), *this);
}

void WebGeolocationManagerProxy::setProvider(std::unique_ptr<API::GeolocationProvider>&& provider)
{
    if (!provider)
        m_provider = makeUnique<API::GeolocationProvider>();
    else
        m_provider = WTFMove(provider);
}

// WebContextSupplement

void WebGeolocationManagerProxy::processPoolDestroyed()
{
    bool wasUpdating = isUpdating();
    m_updateRequesters.clear();

    ASSERT(!isUpdating());
    if (wasUpdating)
        m_provider->stopUpdating(*this);
}

void WebGeolocationManagerProxy::processDidClose(WebProcessProxy* webProcessProxy)
{
    removeRequester(webProcessProxy);
}

void WebGeolocationManagerProxy::refWebContextSupplement()
{
    API::Object::ref();
}

void WebGeolocationManagerProxy::derefWebContextSupplement()
{
    API::Object::deref();
}

void WebGeolocationManagerProxy::providerDidChangePosition(WebGeolocationPosition* position)
{
    m_lastPosition = position->corePosition();

    if (!processPool())
        return;

    processPool()->sendToAllProcesses(Messages::WebGeolocationManager::DidChangePosition(m_lastPosition.value()));
}

void WebGeolocationManagerProxy::providerDidFailToDeterminePosition(const String& errorMessage)
{
    if (!processPool())
        return;

    processPool()->sendToAllProcesses(Messages::WebGeolocationManager::DidFailToDeterminePosition(errorMessage));
}

#if PLATFORM(IOS_FAMILY)
void WebGeolocationManagerProxy::resetPermissions()
{
    processPool()->sendToAllProcesses(Messages::WebGeolocationManager::ResetPermissions());
}
#endif

void WebGeolocationManagerProxy::startUpdating(IPC::Connection& connection, WebPageProxyIdentifier pageProxyID, const String& authorizationToken)
{
    auto* page = WebProcessProxy::webPage(pageProxyID);
    MESSAGE_CHECK(page);

    auto isValidAuthorizationToken = page->geolocationPermissionRequestManager().isValidAuthorizationToken(authorizationToken);
    MESSAGE_CHECK(isValidAuthorizationToken);

    bool wasUpdating = isUpdating();
    m_updateRequesters.add(&connection.client());
    if (!wasUpdating) {
        m_provider->setEnableHighAccuracy(*this, isHighAccuracyEnabled());
        m_provider->startUpdating(*this);
    } else if (m_lastPosition)
        connection.send(Messages::WebGeolocationManager::DidChangePosition(m_lastPosition.value()), 0);
}

void WebGeolocationManagerProxy::stopUpdating(IPC::Connection& connection)
{
    removeRequester(&connection.client());
}

void WebGeolocationManagerProxy::removeRequester(const IPC::Connection::Client* client)
{
    bool wasUpdating = isUpdating();
    bool highAccuracyWasEnabled = isHighAccuracyEnabled();

    m_highAccuracyRequesters.remove(client);
    m_updateRequesters.remove(client);

    if (wasUpdating && !isUpdating())
        m_provider->stopUpdating(*this);
    else {
        bool highAccuracyShouldBeEnabled = isHighAccuracyEnabled();
        if (highAccuracyShouldBeEnabled != highAccuracyWasEnabled)
            m_provider->setEnableHighAccuracy(*this, highAccuracyShouldBeEnabled);
    }
}

void WebGeolocationManagerProxy::setEnableHighAccuracy(IPC::Connection& connection, bool enabled)
{
    bool highAccuracyWasEnabled = isHighAccuracyEnabled();

    if (enabled)
        m_highAccuracyRequesters.add(&connection.client());
    else
        m_highAccuracyRequesters.remove(&connection.client());

    bool highAccuracyShouldBeEnabled = isHighAccuracyEnabled();
    if (isUpdating() && highAccuracyWasEnabled != highAccuracyShouldBeEnabled)
        m_provider->setEnableHighAccuracy(*this, highAccuracyShouldBeEnabled);
}

} // namespace WebKit

#undef MESSAGE_CHECK
