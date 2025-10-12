/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
#include "WebGeolocationClient.h"

#if ENABLE(GEOLOCATION)

#include "GeolocationPermissionRequestManager.h"
#include "WebGeolocationManager.h"
#include "WebProcess.h"
#include <WebCore/Geolocation.h>
#include <WebCore/GeolocationPositionData.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebGeolocationClient);

WebGeolocationClient::WebGeolocationClient(WebPage& page)
    : m_page(page)
{
}

WebGeolocationClient::~WebGeolocationClient() = default;

void WebGeolocationClient::geolocationDestroyed()
{
    if (RefPtr page = m_page.get())
        WebProcess::singleton().protectedSupplement<WebGeolocationManager>()->unregisterWebPage(*page);
}

void WebGeolocationClient::startUpdating(const String& authorizationToken, bool needsHighAccuracy)
{
    if (RefPtr page = m_page.get())
        WebProcess::singleton().protectedSupplement<WebGeolocationManager>()->registerWebPage(*page, authorizationToken, needsHighAccuracy);
}

void WebGeolocationClient::stopUpdating()
{
    if (RefPtr page = m_page.get())
        WebProcess::singleton().protectedSupplement<WebGeolocationManager>()->unregisterWebPage(*page);
}

void WebGeolocationClient::setEnableHighAccuracy(bool enabled)
{
    if (RefPtr page = m_page.get())
        WebProcess::singleton().protectedSupplement<WebGeolocationManager>()->setEnableHighAccuracyForPage(*page, enabled);
}

std::optional<GeolocationPositionData> WebGeolocationClient::lastPosition()
{
    return std::nullopt;
}

void WebGeolocationClient::requestPermission(Geolocation& geolocation)
{
    if (m_page)
        m_page->geolocationPermissionRequestManager().startRequestForGeolocation(geolocation);
}

void WebGeolocationClient::revokeAuthorizationToken(const String& authorizationToken)
{
    if (m_page)
        m_page->geolocationPermissionRequestManager().revokeAuthorizationToken(authorizationToken);
}

void WebGeolocationClient::cancelPermissionRequest(Geolocation& geolocation)
{
    if (m_page)
        m_page->geolocationPermissionRequestManager().cancelRequestForGeolocation(geolocation);
}

} // namespace WebKit

#endif // ENABLE(GEOLOCATION)
