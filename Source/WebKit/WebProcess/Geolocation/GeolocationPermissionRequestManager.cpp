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
#include "GeolocationPermissionRequestManager.h"

#if ENABLE(GEOLOCATION)

#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/Geolocation.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>

namespace WebKit {
using namespace WebCore;

static uint64_t generateGeolocationID()
{
    static uint64_t uniqueGeolocationID = 1;
    return uniqueGeolocationID++;
}

GeolocationPermissionRequestManager::GeolocationPermissionRequestManager(WebPage& page)
    : m_page(page)
{
}

void GeolocationPermissionRequestManager::startRequestForGeolocation(Geolocation& geolocation)
{
    Frame* frame = geolocation.frame();

    ASSERT_WITH_MESSAGE(frame, "It is not well understood in which cases the Geolocation is alive after its frame goes away. If you hit this assertion, please add a test covering this case.");
    if (!frame) {
        geolocation.setIsAllowed(false, { });
        return;
    }

    uint64_t geolocationID = generateGeolocationID();

    m_geolocationToIDMap.set(&geolocation, geolocationID);
    m_idToGeolocationMap.set(geolocationID, &geolocation);

    WebFrame* webFrame = WebFrame::fromCoreFrame(*frame);
    ASSERT(webFrame);

    SecurityOrigin& origin = frame->document()->securityOrigin();

    m_page.send(Messages::WebPageProxy::RequestGeolocationPermissionForFrame(geolocationID, webFrame->frameID(), origin.data().databaseIdentifier()));
}

void GeolocationPermissionRequestManager::revokeAuthorizationToken(const String& authorizationToken)
{
    m_page.send(Messages::WebPageProxy::RevokeGeolocationAuthorizationToken(authorizationToken));
}

void GeolocationPermissionRequestManager::cancelRequestForGeolocation(Geolocation& geolocation)
{
    uint64_t geolocationID = m_geolocationToIDMap.take(&geolocation);
    if (!geolocationID)
        return;
    m_idToGeolocationMap.remove(geolocationID);
}

void GeolocationPermissionRequestManager::didReceiveGeolocationPermissionDecision(uint64_t geolocationID, const String& authorizationToken)
{
    Geolocation* geolocation = m_idToGeolocationMap.take(geolocationID);
    if (!geolocation)
        return;
    m_geolocationToIDMap.remove(geolocation);

    geolocation->setIsAllowed(!authorizationToken.isNull(), authorizationToken);
}

} // namespace WebKit

#endif // ENABLE(GEOLOCATION)
