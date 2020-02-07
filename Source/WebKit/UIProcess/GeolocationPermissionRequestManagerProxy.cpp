/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "GeolocationPermissionRequestManagerProxy.h"

#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <wtf/UUID.h>

namespace WebKit {

GeolocationPermissionRequestManagerProxy::GeolocationPermissionRequestManagerProxy(WebPageProxy& page)
    : m_page(page)
{
}

void GeolocationPermissionRequestManagerProxy::invalidateRequests()
{
    for (auto& request : m_pendingRequests.values())
        request->invalidate();

    m_pendingRequests.clear();
}

Ref<GeolocationPermissionRequestProxy> GeolocationPermissionRequestManagerProxy::createRequest(uint64_t geolocationID)
{
    auto request = GeolocationPermissionRequestProxy::create(this, geolocationID);
    m_pendingRequests.add(geolocationID, request.ptr());
    return request;
}

void GeolocationPermissionRequestManagerProxy::didReceiveGeolocationPermissionDecision(uint64_t geolocationID, bool allowed)
{
    if (!m_page.hasRunningProcess())
        return;

    auto it = m_pendingRequests.find(geolocationID);
    if (it == m_pendingRequests.end())
        return;

#if ENABLE(GEOLOCATION)
    String authorizationToken = allowed ? createCanonicalUUIDString() : String();
    if (!authorizationToken.isNull())
        m_validAuthorizationTokens.add(authorizationToken);
    m_page.process().send(Messages::WebPage::DidReceiveGeolocationPermissionDecision(geolocationID, authorizationToken), m_page.webPageID());
#else
    UNUSED_PARAM(allowed);
#endif

    m_pendingRequests.remove(it);
}

bool GeolocationPermissionRequestManagerProxy::isValidAuthorizationToken(const String& authorizationToken) const
{
    return !authorizationToken.isNull() && m_validAuthorizationTokens.contains(authorizationToken);
}

void GeolocationPermissionRequestManagerProxy::revokeAuthorizationToken(const String& authorizationToken)
{
    ASSERT(isValidAuthorizationToken(authorizationToken));
    if (!isValidAuthorizationToken(authorizationToken))
        return;
    m_validAuthorizationTokens.remove(authorizationToken);
}

} // namespace WebKit
