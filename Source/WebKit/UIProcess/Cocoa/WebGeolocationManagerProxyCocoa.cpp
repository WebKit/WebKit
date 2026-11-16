/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "WebGeolocationManagerMessages.h"
#include "WebProcessProxy.h"

namespace WebKit {

// FIXME: This should be used by all Cocoa ports
#if PLATFORM(IOS_FAMILY)

void WebGeolocationManagerProxy::positionChanged(const String& websiteIdentifier, WebCore::GeolocationPositionData&& position)
{
    auto registrableDomain = WebCore::RegistrableDomain::uncheckedCreateFromRegistrableDomainString(websiteIdentifier);
    auto it = m_perDomainData.find(registrableDomain);
    if (it == m_perDomainData.end())
        return;

    auto& perDomainData = *it->value;
    perDomainData.lastPosition = WTF::move(position);
    for (Ref webProcessProxy : perDomainData.watchers)
        webProcessProxy->send(Messages::WebGeolocationManager::DidChangePosition(registrableDomain, perDomainData.lastPosition.value()), 0);
}

void WebGeolocationManagerProxy::errorOccurred(const String& websiteIdentifier, const String& errorMessage)
{
    auto registrableDomain = WebCore::RegistrableDomain::uncheckedCreateFromRegistrableDomainString(websiteIdentifier);
    auto it = m_perDomainData.find(registrableDomain);
    if (it == m_perDomainData.end())
        return;

    auto& perDomainData = *it->value;
    for (Ref webProcessProxy : perDomainData.watchers)
        webProcessProxy->send(Messages::WebGeolocationManager::DidFailToDeterminePosition(registrableDomain, errorMessage), 0);
}

void WebGeolocationManagerProxy::resetGeolocation(const String& websiteIdentifier)
{
    auto registrableDomain = WebCore::RegistrableDomain::uncheckedCreateFromRegistrableDomainString(websiteIdentifier);
    auto it = m_perDomainData.find(registrableDomain);
    if (it == m_perDomainData.end())
        return;

    auto& perDomainData = *it->value;
    for (Ref webProcessProxy : perDomainData.watchers)
        webProcessProxy->send(Messages::WebGeolocationManager::ResetPermissions(registrableDomain), 0);
}

#endif

} // namespace WebKit
