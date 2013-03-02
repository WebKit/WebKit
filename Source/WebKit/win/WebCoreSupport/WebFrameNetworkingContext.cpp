/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "WebFrameNetworkingContext.h"

#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#if USE(CFNETWORK)
#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <WebCore/ResourceHandle.h>
#endif
#include <WebCore/Settings.h>
#if USE(CFNETWORK)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

using namespace WebCore;

#if USE(CFNETWORK)
static OwnPtr<NetworkStorageSession>& privateBrowsingStorageSession()
{
    DEFINE_STATIC_LOCAL(OwnPtr<NetworkStorageSession>, session, ());
    return session;
}
#endif

PassRefPtr<WebFrameNetworkingContext> WebFrameNetworkingContext::create(Frame* frame, const String& userAgent)
{
    return adoptRef(new WebFrameNetworkingContext(frame, userAgent));
}

String WebFrameNetworkingContext::userAgent() const
{
    return m_userAgent;
}

String WebFrameNetworkingContext::referrer() const
{
    return frame()->loader()->referrer();
}

WebCore::ResourceError WebFrameNetworkingContext::blockedError(const WebCore::ResourceRequest& request) const
{
    return frame()->loader()->client()->blockedError(request);
}

static String& privateBrowsingStorageSessionIdentifierBase()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(String, base, ());
    return base;
}

void WebFrameNetworkingContext::setPrivateBrowsingStorageSessionIdentifierBase(const String& identifier)
{
    privateBrowsingStorageSessionIdentifierBase() = identifier;
}

void WebFrameNetworkingContext::ensurePrivateBrowsingSession()
{
    ASSERT(isMainThread());
#if USE(CFNETWORK)
    if (privateBrowsingStorageSession())
        return;

    String identifierBase = privateBrowsingStorageSessionIdentifierBase().isNull() ? String(reinterpret_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(CFBundleGetMainBundle(), kCFBundleIdentifierKey))) : privateBrowsingStorageSessionIdentifierBase();

    privateBrowsingStorageSession() = NetworkStorageSession::createPrivateBrowsingSession(identifierBase);
#endif
}

void WebFrameNetworkingContext::destroyPrivateBrowsingSession()
{
#if USE(CFNETWORK)
    privateBrowsingStorageSession() = nullptr;
#endif
}

#if USE(CFNETWORK)
NetworkStorageSession& WebFrameNetworkingContext::storageSession() const
{
    bool inPrivateBrowsingMode = frame() && frame()->settings() && frame()->settings()->privateBrowsingEnabled();
    if (inPrivateBrowsingMode) {
        ASSERT(privateBrowsingStorageSession());
        return *privateBrowsingStorageSession();
    }
    return NetworkStorageSession::defaultStorageSession();
}

void WebFrameNetworkingContext::setCookieAcceptPolicyForAllContexts(CFHTTPCookieStorageAcceptPolicy policy)
{
    if (RetainPtr<CFHTTPCookieStorageRef> cookieStorage = NetworkStorageSession::defaultStorageSession().cookieStorage())
        CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), policy);

    if (privateBrowsingStorageSession())
        CFHTTPCookieStorageSetCookieAcceptPolicy(privateBrowsingStorageSession()->cookieStorage().get(), policy);
}
#endif
