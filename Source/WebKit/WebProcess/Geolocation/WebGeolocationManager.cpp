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
#include "WebGeolocationManager.h"

#include "WebGeolocationManagerMessages.h"
#include "WebGeolocationManagerProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>
#include <WebCore/Geolocation.h>
#include <WebCore/GeolocationController.h>
#include <WebCore/GeolocationError.h>
#include <WebCore/GeolocationPositionData.h>
#include <WebCore/Page.h>

namespace WebKit {
using namespace WebCore;

static RegistrableDomain registrableDomainForPage(WebPage& page)
{
    auto* document = page.corePage() ? page.corePage()->mainFrame().document() : nullptr;
    if (!document)
        return { };
    return RegistrableDomain { document->url() };
}

const char* WebGeolocationManager::supplementName()
{
    return "WebGeolocationManager";
}

WebGeolocationManager::WebGeolocationManager(WebProcess& process)
    : m_process(process)
{
    m_process.addMessageReceiver(Messages::WebGeolocationManager::messageReceiverName(), *this);
}

WebGeolocationManager::~WebGeolocationManager() = default;

void WebGeolocationManager::registerWebPage(WebPage& page, const String& authorizationToken, bool needsHighAccuracy)
{
    auto registrableDomain = registrableDomainForPage(page);
    if (registrableDomain.string().isEmpty())
        return;

    auto& pageSets = m_pageSets.add(registrableDomain, PageSets()).iterator->value;
    bool wasUpdating = isUpdating(pageSets);
    bool highAccuracyWasEnabled = isHighAccuracyEnabled(pageSets);

    pageSets.pageSet.add(page);
    if (needsHighAccuracy)
        pageSets.highAccuracyPageSet.add(page);
    m_pageToRegistrableDomain.add(page, registrableDomain);

    if (!wasUpdating) {
        m_process.parentProcessConnection()->send(Messages::WebGeolocationManagerProxy::StartUpdating(registrableDomain, page.webPageProxyIdentifier(), authorizationToken, needsHighAccuracy), 0);
        return;
    }

    bool highAccuracyShouldBeEnabled = isHighAccuracyEnabled(pageSets);
    if (highAccuracyWasEnabled != highAccuracyShouldBeEnabled)
        m_process.parentProcessConnection()->send(Messages::WebGeolocationManagerProxy::SetEnableHighAccuracy(registrableDomain, highAccuracyShouldBeEnabled), 0);
}

void WebGeolocationManager::unregisterWebPage(WebPage& page)
{
    auto registrableDomain = m_pageToRegistrableDomain.take(page);
    if (registrableDomain.string().isEmpty())
        return;

    auto it = m_pageSets.find(registrableDomain);
    if (it == m_pageSets.end())
        return;

    auto& pageSets = it->value;
    bool highAccuracyWasEnabled = isHighAccuracyEnabled(pageSets);

    pageSets.pageSet.remove(page);
    pageSets.highAccuracyPageSet.remove(page);

    if (!isUpdating(pageSets))
        m_process.parentProcessConnection()->send(Messages::WebGeolocationManagerProxy::StopUpdating(registrableDomain), 0);
    else {
        bool highAccuracyShouldBeEnabled = isHighAccuracyEnabled(pageSets);
        if (highAccuracyWasEnabled != highAccuracyShouldBeEnabled)
            m_process.parentProcessConnection()->send(Messages::WebGeolocationManagerProxy::SetEnableHighAccuracy(registrableDomain, highAccuracyShouldBeEnabled), 0);
    }

    if (pageSets.pageSet.isEmptyIgnoringNullReferences() && pageSets.highAccuracyPageSet.isEmptyIgnoringNullReferences())
        m_pageSets.remove(it);
}

void WebGeolocationManager::setEnableHighAccuracyForPage(WebPage& page, bool enabled)
{
    auto registrableDomain = m_pageToRegistrableDomain.get(page);
    if (registrableDomain.string().isEmpty())
        return;

    auto it = m_pageSets.find(registrableDomain);
    ASSERT(it != m_pageSets.end());
    if (it == m_pageSets.end())
        return;

    auto& pageSets = it->value;
    bool highAccuracyWasEnabled = isHighAccuracyEnabled(pageSets);

    if (enabled)
        pageSets.highAccuracyPageSet.add(page);
    else
        pageSets.highAccuracyPageSet.remove(page);

    bool highAccuracyShouldBeEnabled = isHighAccuracyEnabled(pageSets);
    if (highAccuracyWasEnabled != isHighAccuracyEnabled(pageSets))
        m_process.parentProcessConnection()->send(Messages::WebGeolocationManagerProxy::SetEnableHighAccuracy(registrableDomain, highAccuracyShouldBeEnabled), 0);
}

void WebGeolocationManager::didChangePosition(const WebCore::RegistrableDomain& registrableDomain, const GeolocationPositionData& position)
{
#if ENABLE(GEOLOCATION)
    if (auto it = m_pageSets.find(registrableDomain); it != m_pageSets.end()) {
        for (auto& page : copyToVector(it->value.pageSet)) {
            if (page->corePage())
                GeolocationController::from(page->corePage())->positionChanged(position);
        }
    }
#else
    UNUSED_PARAM(registrableDomain);
    UNUSED_PARAM(position);
#endif // ENABLE(GEOLOCATION)
}

void WebGeolocationManager::didFailToDeterminePosition(const WebCore::RegistrableDomain& registrableDomain, const String& errorMessage)
{
#if ENABLE(GEOLOCATION)
    if (auto it = m_pageSets.find(registrableDomain); it != m_pageSets.end()) {
        // FIXME: Add localized error string.
        auto error = GeolocationError::create(GeolocationError::PositionUnavailable, errorMessage);

        for (auto& page : copyToVector(it->value.pageSet)) {
            if (page->corePage())
                GeolocationController::from(page->corePage())->errorOccurred(error.get());
        }
    }
#else
    UNUSED_PARAM(registrableDomain);
    UNUSED_PARAM(errorMessage);
#endif // ENABLE(GEOLOCATION)
}

bool WebGeolocationManager::isUpdating(const PageSets& pageSets) const
{
    return !pageSets.pageSet.isEmptyIgnoringNullReferences();
}

bool WebGeolocationManager::isHighAccuracyEnabled(const PageSets& pageSets) const
{
    return !pageSets.highAccuracyPageSet.isEmptyIgnoringNullReferences();
}

#if PLATFORM(IOS_FAMILY)
void WebGeolocationManager::resetPermissions(const WebCore::RegistrableDomain& registrableDomain)
{
    auto it = m_pageSets.find(registrableDomain);
    if (it != m_pageSets.end())
        return;

    for (auto& page : copyToVector(it->value.pageSet)) {
        if (auto* mainFrame = page->corePage() ? &page->corePage()->mainFrame() : nullptr)
            mainFrame->resetAllGeolocationPermission();
    }
}
#endif // PLATFORM(IOS_FAMILY)

} // namespace WebKit
