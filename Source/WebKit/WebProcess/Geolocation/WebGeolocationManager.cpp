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
#include <WebCore/Geolocation.h>
#include <WebCore/GeolocationController.h>
#include <WebCore/GeolocationError.h>
#include <WebCore/GeolocationPositionData.h>
#include <WebCore/Page.h>

namespace WebKit {
using namespace WebCore;

const char* WebGeolocationManager::supplementName()
{
    return "WebGeolocationManager";
}

WebGeolocationManager::WebGeolocationManager(WebProcess& process)
    : m_process(process)
{
    m_process.addMessageReceiver(Messages::WebGeolocationManager::messageReceiverName(), *this);
}

void WebGeolocationManager::registerWebPage(WebPage& page, const String& authorizationToken)
{
    bool wasUpdating = isUpdating();

    m_pageSet.add(&page);

    if (!wasUpdating)
        m_process.parentProcessConnection()->send(Messages::WebGeolocationManagerProxy::StartUpdating(page.webPageProxyIdentifier(), authorizationToken), 0);
}

void WebGeolocationManager::unregisterWebPage(WebPage& page)
{
    bool highAccuracyWasEnabled = isHighAccuracyEnabled();

    m_pageSet.remove(&page);
    m_highAccuracyPageSet.remove(&page);

    if (!isUpdating())
        m_process.parentProcessConnection()->send(Messages::WebGeolocationManagerProxy::StopUpdating(), 0);
    else {
        bool highAccuracyShouldBeEnabled = isHighAccuracyEnabled();
        if (highAccuracyWasEnabled != highAccuracyShouldBeEnabled)
            m_process.parentProcessConnection()->send(Messages::WebGeolocationManagerProxy::SetEnableHighAccuracy(highAccuracyShouldBeEnabled), 0);
    }
}

void WebGeolocationManager::setEnableHighAccuracyForPage(WebPage& page, bool enabled)
{
    bool highAccuracyWasEnabled = isHighAccuracyEnabled();

    if (enabled)
        m_highAccuracyPageSet.add(&page);
    else
        m_highAccuracyPageSet.remove(&page);

    bool highAccuracyShouldBeEnabled = isHighAccuracyEnabled();
    if (highAccuracyWasEnabled != highAccuracyShouldBeEnabled)
        m_process.parentProcessConnection()->send(Messages::WebGeolocationManagerProxy::SetEnableHighAccuracy(highAccuracyShouldBeEnabled), 0);
}

void WebGeolocationManager::didChangePosition(const GeolocationPositionData& position)
{
#if ENABLE(GEOLOCATION)
    for (auto& page : copyToVector(m_pageSet)) {
        if (page->corePage())
            GeolocationController::from(page->corePage())->positionChanged(position);
    }
#else
    UNUSED_PARAM(position);
#endif // ENABLE(GEOLOCATION)
}

void WebGeolocationManager::didFailToDeterminePosition(const String& errorMessage)
{
#if ENABLE(GEOLOCATION)
    // FIXME: Add localized error string.
    auto error = GeolocationError::create(GeolocationError::PositionUnavailable, errorMessage);

    for (auto& page : copyToVector(m_pageSet)) {
        if (page->corePage())
            GeolocationController::from(page->corePage())->errorOccurred(error.get());
    }
#else
    UNUSED_PARAM(errorMessage);
#endif // ENABLE(GEOLOCATION)
}

#if PLATFORM(IOS_FAMILY)
void WebGeolocationManager::resetPermissions()
{
    m_process.resetAllGeolocationPermissions();
}
#endif // PLATFORM(IOS_FAMILY)

} // namespace WebKit
