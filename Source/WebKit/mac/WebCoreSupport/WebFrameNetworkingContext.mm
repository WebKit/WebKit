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

#import "WebFrameInternal.h"
#import "WebViewPrivate.h"
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderClient.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/Page.h>
#import <WebCore/ResourceError.h>
#import <WebCore/Settings.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

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

RetainPtr<CFDataRef> WebFrameNetworkingContext::sourceApplicationAuditData() const
{
    if (!frame())
        return nil;

    if (!frame()->page())
        return nil;

    WebView *webView = kit(frame()->page());
    if (!webView)
        return nil;

    return (CFDataRef)webView._sourceApplicationAuditData;
}

ResourceError WebFrameNetworkingContext::blockedError(const ResourceRequest& request) const
{
    return frame()->loader()->client()->blockedError(request);
}

void WebFrameNetworkingContext::ensurePrivateBrowsingSession()
{
    ASSERT(isMainThread());
    if (privateBrowsingStorageSession())
        return;

    privateBrowsingStorageSession() = NetworkStorageSession::createPrivateBrowsingSession([[NSBundle mainBundle] bundleIdentifier]);
}

void WebFrameNetworkingContext::destroyPrivateBrowsingSession()
{
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

