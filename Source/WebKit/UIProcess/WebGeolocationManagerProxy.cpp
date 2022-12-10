/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

#define MESSAGE_CHECK(connection, assertion) MESSAGE_CHECK_BASE(assertion, (connection))

namespace WebKit {

static inline WebProcessProxy& connectionToWebProcessProxy(const IPC::Connection& connection)
{
    return static_cast<WebProcessProxy&>(*connection.client());
}

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
{
    WebContextSupplement::processPool()->addMessageReceiver(Messages::WebGeolocationManagerProxy::messageReceiverName(), *this);
}

WebGeolocationManagerProxy::~WebGeolocationManagerProxy() = default;

void WebGeolocationManagerProxy::setProvider(std::unique_ptr<API::GeolocationProvider>&& provider)
{
    m_clientProvider = WTFMove(provider);
}

// WebContextSupplement

void WebGeolocationManagerProxy::processPoolDestroyed()
{
    if (m_perDomainData.isEmpty())
        return;

    m_perDomainData.clear();
    if (m_clientProvider)
        m_clientProvider->stopUpdating(*this);
}

void WebGeolocationManagerProxy::webProcessIsGoingAway(WebProcessProxy& proxy)
{
    Vector<WebCore::RegistrableDomain> affectedDomains;
    for (auto& [registrableDomain, perDomainData] : m_perDomainData) {
        if (perDomainData->watchers.contains(proxy))
            affectedDomains.append(registrableDomain);
    }
    for (auto& registrableDomain : affectedDomains)
        stopUpdatingWithProxy(proxy, registrableDomain);
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
    for (auto& [registrableDomain, perDomainData] : m_perDomainData) {
        perDomainData->lastPosition = position->corePosition();
        for (auto& process : perDomainData->watchers)
            process.send(Messages::WebGeolocationManager::DidChangePosition(registrableDomain, perDomainData->lastPosition.value()), 0);
    }
}

void WebGeolocationManagerProxy::providerDidFailToDeterminePosition(const String& errorMessage)
{
    for (auto& [registrableDomain, perDomainData] : m_perDomainData) {
        for (auto& proxy : perDomainData->watchers)
            proxy.send(Messages::WebGeolocationManager::DidFailToDeterminePosition(registrableDomain, errorMessage), 0);
    }
}

#if PLATFORM(IOS_FAMILY)
void WebGeolocationManagerProxy::resetPermissions()
{
    ASSERT(m_clientProvider);
    for (auto& [registrableDomain, perDomainData] : m_perDomainData) {
        for (auto& proxy : perDomainData->watchers)
            proxy.send(Messages::WebGeolocationManager::ResetPermissions(registrableDomain), 0);
    }
}
#endif

void WebGeolocationManagerProxy::startUpdating(IPC::Connection& connection, const WebCore::RegistrableDomain& registrableDomain, WebPageProxyIdentifier pageProxyID, const String& authorizationToken, bool enableHighAccuracy)
{
    startUpdatingWithProxy(connectionToWebProcessProxy(connection), registrableDomain, pageProxyID, authorizationToken, enableHighAccuracy);
}

void WebGeolocationManagerProxy::startUpdatingWithProxy(WebProcessProxy& proxy, const WebCore::RegistrableDomain& registrableDomain, WebPageProxyIdentifier pageProxyID, const String& authorizationToken, bool enableHighAccuracy)
{
    auto page = WebProcessProxy::webPage(pageProxyID);
    MESSAGE_CHECK(proxy.connection(), !!page);

    auto isValidAuthorizationToken = page->geolocationPermissionRequestManager().isValidAuthorizationToken(authorizationToken);
    MESSAGE_CHECK(proxy.connection(), isValidAuthorizationToken);

    auto& perDomainData = *m_perDomainData.ensure(registrableDomain, [] {
        return makeUnique<PerDomainData>();
    }).iterator->value;

    bool wasUpdating = isUpdating(perDomainData);
    bool highAccuracyWasEnabled = isHighAccuracyEnabled(perDomainData);

    perDomainData.watchers.add(proxy);
    if (enableHighAccuracy)
        perDomainData.watchersNeedingHighAccuracy.add(proxy);

    if (!wasUpdating) {
        providerStartUpdating(perDomainData, registrableDomain);
        return;
    }
    if (!highAccuracyWasEnabled && enableHighAccuracy)
        providerSetEnabledHighAccuracy(perDomainData, enableHighAccuracy);

    if (perDomainData.lastPosition)
        proxy.send(Messages::WebGeolocationManager::DidChangePosition(registrableDomain, perDomainData.lastPosition.value()), 0);
}

void WebGeolocationManagerProxy::stopUpdating(IPC::Connection& connection, const WebCore::RegistrableDomain& registrableDomain)
{
    stopUpdatingWithProxy(connectionToWebProcessProxy(connection), registrableDomain);
}

void WebGeolocationManagerProxy::stopUpdatingWithProxy(WebProcessProxy& proxy, const WebCore::RegistrableDomain& registrableDomain)
{
    auto it = m_perDomainData.find(registrableDomain);
    if (it == m_perDomainData.end())
        return;

    auto& perDomainData = *it->value;
    bool wasUpdating = isUpdating(perDomainData);
    bool highAccuracyWasEnabled = isHighAccuracyEnabled(perDomainData);

    perDomainData.watchers.remove(proxy);
    perDomainData.watchersNeedingHighAccuracy.remove(proxy);

    if (wasUpdating && !isUpdating(perDomainData))
        providerStopUpdating(perDomainData);
    else {
        bool highAccuracyShouldBeEnabled = isHighAccuracyEnabled(perDomainData);
        if (highAccuracyShouldBeEnabled != highAccuracyWasEnabled)
            providerSetEnabledHighAccuracy(perDomainData, highAccuracyShouldBeEnabled);
    }

    if (perDomainData.watchers.computesEmpty() && perDomainData.watchersNeedingHighAccuracy.computesEmpty())
        m_perDomainData.remove(it);
}

void WebGeolocationManagerProxy::setEnableHighAccuracy(IPC::Connection& connection, const WebCore::RegistrableDomain& registrableDomain, bool enabled)
{
    setEnableHighAccuracyWithProxy(connectionToWebProcessProxy(connection), registrableDomain, enabled);
}

void WebGeolocationManagerProxy::setEnableHighAccuracyWithProxy(WebProcessProxy& proxy, const WebCore::RegistrableDomain& registrableDomain, bool enabled)
{
    auto it = m_perDomainData.find(registrableDomain);
    ASSERT(it != m_perDomainData.end());
    if (it == m_perDomainData.end())
        return;

    auto& perDomainData = *it->value;
    bool highAccuracyWasEnabled = isHighAccuracyEnabled(perDomainData);

    if (enabled)
        perDomainData.watchersNeedingHighAccuracy.add(proxy);
    else
        perDomainData.watchersNeedingHighAccuracy.remove(proxy);

    if (isUpdating(perDomainData) && highAccuracyWasEnabled != enabled)
        providerSetEnabledHighAccuracy(perDomainData, enabled);
}

bool WebGeolocationManagerProxy::isUpdating(const PerDomainData& perDomainData) const
{
#if PLATFORM(IOS_FAMILY)
    if (!m_clientProvider)
        return !perDomainData.watchers.computesEmpty();
#else
    UNUSED_PARAM(perDomainData);
#endif

    for (auto& perDomainData : m_perDomainData.values()) {
        if (!perDomainData->watchers.computesEmpty())
            return true;
    }
    return false;
}


bool WebGeolocationManagerProxy::isHighAccuracyEnabled(const PerDomainData& perDomainData) const
{
#if PLATFORM(IOS_FAMILY)
    if (!m_clientProvider)
        return !perDomainData.watchersNeedingHighAccuracy.computesEmpty();
#else
    UNUSED_PARAM(perDomainData);
#endif

    for (auto& data : m_perDomainData.values()) {
        if (!data->watchersNeedingHighAccuracy.computesEmpty())
            return true;
    }
    return false;
}

void WebGeolocationManagerProxy::providerStartUpdating(PerDomainData& perDomainData, const WebCore::RegistrableDomain& registrableDomain)
{
#if PLATFORM(IOS_FAMILY)
    if (!m_clientProvider) {
        ASSERT(!perDomainData.provider);
        perDomainData.provider = makeUnique<WebCore::CoreLocationGeolocationProvider>(registrableDomain, *this);
        perDomainData.provider->setEnableHighAccuracy(!perDomainData.watchersNeedingHighAccuracy.computesEmpty());
        return;
    }
#else
    UNUSED_PARAM(registrableDomain);
    if (!m_clientProvider)
        return;
#endif

    m_clientProvider->setEnableHighAccuracy(*this, isHighAccuracyEnabled(perDomainData));
    m_clientProvider->startUpdating(*this);
}

void WebGeolocationManagerProxy::providerStopUpdating(PerDomainData& perDomainData)
{
#if PLATFORM(IOS_FAMILY)
    if (!m_clientProvider) {
        perDomainData.provider = nullptr;
        return;
    }
#else
    UNUSED_PARAM(perDomainData);
    if (!m_clientProvider)
        return;
#endif

    m_clientProvider->stopUpdating(*this);
}

void WebGeolocationManagerProxy::providerSetEnabledHighAccuracy(PerDomainData& perDomainData, bool enabled)
{
#if PLATFORM(IOS_FAMILY)
    if (!m_clientProvider) {
        perDomainData.provider->setEnableHighAccuracy(enabled);
        return;
    }
#else
    UNUSED_PARAM(perDomainData);
    if (!m_clientProvider)
        return;
#endif

    m_clientProvider->setEnableHighAccuracy(*this, enabled);
}

} // namespace WebKit

#undef MESSAGE_CHECK
