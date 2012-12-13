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

#include "FrameLoaderClient.h"
#if USE(CFNETWORK)
#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <WebCore/CookieStorageCFNet.h>
#endif
#include <WebCore/Settings.h>
#if USE(CFNETWORK)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

using namespace WebCore;

#if USE(CFNETWORK)
static CFURLStorageSessionRef defaultCFStorageSession;
static CFURLStorageSessionRef privateBrowsingStorageSession;
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

void WebFrameNetworkingContext::switchToNewTestingSession()
{
#if USE(CFNETWORK)
    // Set a private session for testing to avoid interfering with global cookies. This should be different from private browsing session.
    defaultCFStorageSession = wkCreatePrivateStorageSession(CFSTR("Private WebKit Session"), defaultCFStorageSession);
#endif
}

void WebFrameNetworkingContext::ensurePrivateBrowsingSession()
{
    ASSERT(isMainThread());
#if USE(CFNETWORK)
    if (privateBrowsingStorageSession)
        return;

    String base = privateBrowsingStorageSessionIdentifierBase().isNull() ? String(reinterpret_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(CFBundleGetMainBundle(), kCFBundleIdentifierKey))) : privateBrowsingStorageSessionIdentifierBase();
    RetainPtr<CFStringRef> cfIdentifier = String(base + ".PrivateBrowsing").createCFString();

    privateBrowsingStorageSession = wkCreatePrivateStorageSession(cfIdentifier.get(), defaultCFStorageSession);
#endif
}

void WebFrameNetworkingContext::destroyPrivateBrowsingSession()
{
#if USE(CFNETWORK)
    if (!privateBrowsingStorageSession)
        return;

    CFRelease(privateBrowsingStorageSession);
    privateBrowsingStorageSession = 0;
#endif
}

#if USE(CFNETWORK)
CFURLStorageSessionRef WebFrameNetworkingContext::defaultStorageSession()
{
    return defaultCFStorageSession;
}

bool WebFrameNetworkingContext::inPrivateBrowsingMode() const
{
    return frame() && frame()->settings() && frame()->settings()->privateBrowsingEnabled();
}

CFURLStorageSessionRef WebFrameNetworkingContext::storageSession() const
{
    bool privateBrowsingEnabled = inPrivateBrowsingMode();
    if (privateBrowsingEnabled) {
        ASSERT(privateBrowsingStorageSession);
        return privateBrowsingStorageSession;
    }
    return defaultCFStorageSession;
}

void WebFrameNetworkingContext::setCookieAcceptPolicyForTestingContext(CFHTTPCookieStorageAcceptPolicy policy)
{
    ASSERT(defaultCFStorageSession);
    RetainPtr<CFHTTPCookieStorageRef> defaultCookieStorage = adoptCF(wkCopyHTTPCookieStorage(defaultCFStorageSession));
    CFHTTPCookieStorageSetCookieAcceptPolicy(defaultCookieStorage.get(), policy);
}

void WebFrameNetworkingContext::setCookieAcceptPolicyForAllContexts(CFHTTPCookieStorageAcceptPolicy policy)
{
    if (RetainPtr<CFHTTPCookieStorageRef> cookieStorage = defaultCFHTTPCookieStorage())
        CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), policy);

    if (privateBrowsingStorageSession) {
        RetainPtr<CFHTTPCookieStorageRef> privateBrowsingCookieStorage = adoptCF(wkCopyHTTPCookieStorage(privateBrowsingStorageSession));
        CFHTTPCookieStorageSetCookieAcceptPolicy(privateBrowsingCookieStorage.get(), policy);
    }
}
#endif
