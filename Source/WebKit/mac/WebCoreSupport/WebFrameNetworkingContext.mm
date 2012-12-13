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

#import "WebFrameNetworkingContext.h"

#import <WebCore/FrameLoaderClient.h>
#import <WebCore/Page.h>
#import <WebCore/ResourceError.h>
#import <WebCore/Settings.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

static CFURLStorageSessionRef defaultCFStorageSession;
static CFURLStorageSessionRef privateBrowsingStorageSession;

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

void WebFrameNetworkingContext::switchToNewTestingSession()
{
    // Set a private session for testing to avoid interfering with global cookies. This should be different from private browsing session.
    if (defaultCFStorageSession)
        CFRelease(defaultCFStorageSession);
    defaultCFStorageSession = WKCreatePrivateStorageSession(CFSTR("Private WebKit Session"));
}

void WebFrameNetworkingContext::ensurePrivateBrowsingSession()
{
    ASSERT(isMainThread());
    if (privateBrowsingStorageSession)
        return;

    String base = privateBrowsingStorageSessionIdentifierBase().isNull() ? String([[NSBundle mainBundle] bundleIdentifier]) : privateBrowsingStorageSessionIdentifierBase();
    RetainPtr<CFStringRef> cfIdentifier = String(base + ".PrivateBrowsing").createCFString();

    privateBrowsingStorageSession = WKCreatePrivateStorageSession(cfIdentifier.get());
}

void WebFrameNetworkingContext::destroyPrivateBrowsingSession()
{
    if (!privateBrowsingStorageSession)
        return;

    CFRelease(privateBrowsingStorageSession);
    privateBrowsingStorageSession = 0;
}

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
    if (inPrivateBrowsingMode()) {
        ASSERT(privateBrowsingStorageSession);
        return privateBrowsingStorageSession;
    }
    return defaultCFStorageSession;
}

void WebFrameNetworkingContext::setCookieAcceptPolicyForTestingContext(NSHTTPCookieAcceptPolicy policy)
{
    ASSERT(defaultCFStorageSession);
    RetainPtr<CFHTTPCookieStorageRef> defaultCookieStorage = adoptCF(WKCopyHTTPCookieStorage(defaultCFStorageSession));
    WKSetHTTPCookieAcceptPolicy(defaultCookieStorage.get(), policy);
}
