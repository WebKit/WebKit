/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2012 Apple Inc. All rights reserved.

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

#import "config.h"
#import "WebFrameNetworkingContext.h"

#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderClient.h>
#import <WebCore/Page.h>
#import <WebCore/ResourceError.h>
#import <WebCore/Settings.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

namespace WebKit {

static OwnPtr<NetworkStorageSession>& privateBrowsingStorageSession()
{
    DEFINE_STATIC_LOCAL(OwnPtr<NetworkStorageSession>, session, ());
    return session;
}

bool WebFrameNetworkingContext::needsSiteSpecificQuirks() const
{
    return frame() && frame()->settings() && frame()->settings()->needsSiteSpecificQuirks();
}

bool WebFrameNetworkingContext::localFileContentSniffingEnabled() const
{
    return frame() && frame()->settings() && frame()->settings()->localFileContentSniffingEnabled();
}

SchedulePairHashSet* WebFrameNetworkingContext::scheduledRunLoopPairs() const
{
    return frame() && frame()->page() ? frame()->page()->scheduledRunLoopPairs() : 0;
}

ResourceError WebFrameNetworkingContext::blockedError(const ResourceRequest& request) const
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
    // FIXME (NetworkProcess): Don't create an unnecessary session when using network process.

    ASSERT(isMainThread());
    if (privateBrowsingStorageSession())
        return;

    String identifierBase = privateBrowsingStorageSessionIdentifierBase().isNull() ? String([[NSBundle mainBundle] bundleIdentifier]) : privateBrowsingStorageSessionIdentifierBase();

    privateBrowsingStorageSession() = NetworkStorageSession::createPrivateBrowsingSession(identifierBase);
}

void WebFrameNetworkingContext::destroyPrivateBrowsingSession()
{
    if (!privateBrowsingStorageSession)
        return;

    privateBrowsingStorageSession() = nullptr;
}

NetworkStorageSession& WebFrameNetworkingContext::storageSession() const
{
    bool inPrivateBrowsingMode = frame() && frame()->settings() && frame()->settings()->privateBrowsingEnabled();
    if (inPrivateBrowsingMode) {
        ASSERT(privateBrowsingStorageSession());
        return *privateBrowsingStorageSession();
    }
    return NetworkStorageSession::defaultStorageSession();
}

void WebFrameNetworkingContext::setCookieAcceptPolicyForAllContexts(HTTPCookieAcceptPolicy policy)
{
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookieAcceptPolicy:static_cast<NSHTTPCookieAcceptPolicy>(policy)];

    if (RetainPtr<CFHTTPCookieStorageRef> cookieStorage = NetworkStorageSession::defaultStorageSession().cookieStorage())
        WKSetHTTPCookieAcceptPolicy(cookieStorage.get(), policy);

    if (privateBrowsingStorageSession()) {
        WKSetHTTPCookieAcceptPolicy(privateBrowsingStorageSession()->cookieStorage().get(), policy);
    }
}

}
