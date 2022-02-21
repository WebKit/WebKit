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
#include "WKNotificationManager.h"

#include "APIArray.h"
#include "APIData.h"
#include "WKAPICast.h"
#include "WebNotification.h"
#include "WebNotificationManagerProxy.h"
#include "WebNotificationProvider.h"

using namespace WebKit;

WKTypeID WKNotificationManagerGetTypeID()
{
    return toAPI(WebNotificationManagerProxy::APIType);
}

void WKNotificationManagerSetProvider(WKNotificationManagerRef managerRef, const WKNotificationProviderBase* wkProvider)
{
    toImpl(managerRef)->setProvider(makeUnique<WebNotificationProvider>(wkProvider));
}

void WKNotificationManagerProviderDidShowNotification(WKNotificationManagerRef managerRef, uint64_t notificationID)
{
    toImpl(managerRef)->providerDidShowNotification(notificationID);
}

void WKNotificationManagerProviderDidClickNotification(WKNotificationManagerRef managerRef, uint64_t notificationID)
{
    toImpl(managerRef)->providerDidClickNotification(notificationID);
}

void WKNotificationManagerProviderDidClickNotification_b(WKNotificationManagerRef managerRef, WKDataRef identifier)
{
    auto span = toImpl(identifier)->dataReference();
    if (span.size() != 16)
        return;

    toImpl(managerRef)->providerDidClickNotification(UUID { Span<const uint8_t, 16> { span.data(), 16 } });
}

void WKNotificationManagerProviderDidCloseNotifications(WKNotificationManagerRef managerRef, WKArrayRef notificationIDs)
{
    toImpl(managerRef)->providerDidCloseNotifications(toImpl(notificationIDs));
}

void WKNotificationManagerProviderDidUpdateNotificationPolicy(WKNotificationManagerRef managerRef, WKSecurityOriginRef origin, bool allowed)
{
    toImpl(managerRef)->providerDidUpdateNotificationPolicy(toImpl(origin), allowed);
}

void WKNotificationManagerProviderDidRemoveNotificationPolicies(WKNotificationManagerRef managerRef, WKArrayRef origins)
{
    toImpl(managerRef)->providerDidRemoveNotificationPolicies(toImpl(origins));
}

WKNotificationManagerRef WKNotificationManagerGetSharedServiceWorkerNotificationManager()
{
    return toAPI(&WebNotificationManagerProxy::sharedServiceWorkerManager());
}
