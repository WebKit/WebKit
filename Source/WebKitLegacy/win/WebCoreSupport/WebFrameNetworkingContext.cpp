/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "WebFrameNetworkingContext.h"

#include "NetworkStorageSessionMap.h"
#include "WebView.h"
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Page.h>
#include <WebCore/ResourceError.h>
#include <WebCore/Settings.h>
#include <wtf/NeverDestroyed.h>

#if USE(CFURLCONNECTION)
#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <pal/spi/cf/CFNetworkSPI.h>
#endif

using namespace WebCore;

static String& identifierBase()
{
    static NeverDestroyed<String> base;
    return base;
}

#if USE(CFURLCONNECTION)
void WebFrameNetworkingContext::setCookieAcceptPolicyForAllContexts(WebKitCookieStorageAcceptPolicy policy)
{
    if (RetainPtr<CFHTTPCookieStorageRef> cookieStorage = NetworkStorageSessionMap::defaultStorageSession().cookieStorage())
        CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), policy);

    if (auto privateSession = NetworkStorageSessionMap::storageSession(PAL::SessionID::legacyPrivateSessionID()))
        CFHTTPCookieStorageSetCookieAcceptPolicy(privateSession->cookieStorage().get(), policy);
}
#endif

void WebFrameNetworkingContext::setPrivateBrowsingStorageSessionIdentifierBase(const String& base)
{
    ASSERT(isMainThread());

    identifierBase() = base;
}

NetworkStorageSession& WebFrameNetworkingContext::ensurePrivateBrowsingSession()
{
#if USE(CFURLCONNECTION)
    ASSERT(isMainThread());

    if (auto privateSession = NetworkStorageSessionMap::storageSession(PAL::SessionID::legacyPrivateSessionID()))
        return *privateSession;

    String base;
    if (identifierBase().isNull()) {
        if (CFTypeRef bundleValue = CFBundleGetValueForInfoDictionaryKey(CFBundleGetMainBundle(), kCFBundleIdentifierKey))
            if (CFGetTypeID(bundleValue) == CFStringGetTypeID())
                base = reinterpret_cast<CFStringRef>(bundleValue);
    } else
        base = identifierBase();

    NetworkStorageSessionMap::ensureSession(PAL::SessionID::legacyPrivateSessionID(), base);

#elif USE(CURL)
    ASSERT(isMainThread());

    NetworkStorageSessionMap::ensureSession(PAL::SessionID::legacyPrivateSessionID());

#endif
    return *NetworkStorageSessionMap::storageSession(PAL::SessionID::legacyPrivateSessionID());
}

void WebFrameNetworkingContext::destroyPrivateBrowsingSession()
{
    ASSERT(isMainThread());

    NetworkStorageSessionMap::destroySession(PAL::SessionID::legacyPrivateSessionID());
}

ResourceError WebFrameNetworkingContext::blockedError(const ResourceRequest& request) const
{
    return frame()->loader().client().blockedError(request);
}

NetworkStorageSession* WebFrameNetworkingContext::storageSession() const
{
    ASSERT(isMainThread());

    if (frame() && frame()->page()->usesEphemeralSession())
        return NetworkStorageSessionMap::storageSession(PAL::SessionID::legacyPrivateSessionID());

    return &NetworkStorageSessionMap::defaultStorageSession();
}
