/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "MediaSessionInterruptionProviderMac.h"

#if ENABLE(MEDIA_SESSION) && PLATFORM(MAC)

#include <CoreFoundation/CoreFoundation.h>

namespace WebCore {

static const CFStringRef callDidBeginRingingNotification = CFSTR("CallDidBeginRinging");
static const CFStringRef callDidEndRingingNotification = CFSTR("CallDidEndRinging");
static const CFStringRef callDidConnectNotification = CFSTR("CallDidConnect");

static void callDidBeginRinging(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    ASSERT_ARG(observer, observer);
    MediaSessionInterruptionProvider* provider = (MediaSessionInterruptionProvider*)observer;
    provider->client().didReceiveStartOfInterruptionNotification(MediaSessionInterruptingCategory::Transient);
}

static void callDidEndRinging(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    ASSERT_ARG(observer, observer);
    MediaSessionInterruptionProviderMac* provider = (MediaSessionInterruptionProviderMac*)observer;
    provider->client().didReceiveEndOfInterruptionNotification(MediaSessionInterruptingCategory::Transient);
}

static void callDidConnect(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    ASSERT_ARG(observer, observer);
    MediaSessionInterruptionProviderMac* provider = (MediaSessionInterruptionProviderMac*)observer;
    MediaSessionInterruptionProviderClient& client = provider->client();
    client.didReceiveEndOfInterruptionNotification(MediaSessionInterruptingCategory::Transient);
    client.didReceiveStartOfInterruptionNotification(MediaSessionInterruptingCategory::Content);
}

void MediaSessionInterruptionProviderMac::beginListeningForInterruptions()
{
    CFNotificationCenterRef notificationCenter = CFNotificationCenterGetDistributedCenter();
    CFNotificationCenterAddObserver(notificationCenter, this, callDidBeginRinging, callDidBeginRingingNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(notificationCenter, this, callDidEndRinging, callDidEndRingingNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(notificationCenter, this, callDidConnect, callDidConnectNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
}

void MediaSessionInterruptionProviderMac::stopListeningForInterruptions()
{
    CFNotificationCenterRemoveEveryObserver(CFNotificationCenterGetDistributedCenter(), this);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SESSION) && PLATFORM(MAC)
