/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "EffectiveRateChangedListener.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CMTime.h>
#import <pal/spi/cf/CFNotificationCenterSPI.h>
#import <wtf/Function.h>
#import <wtf/cf/NotificationCenterCF.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/NSObjCRuntimeSPI.h>

#import <pal/cf/CoreMediaSoftLink.h>

@interface WebEffectiveRateChangedListenerObjCAdapter : NSObject
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithEffectiveRateChangedListener:(const WebCore::EffectiveRateChangedListener&)listener;
- (RefPtr<WebCore::EffectiveRateChangedListener>)listener;
@end

NS_DIRECT_MEMBERS
@implementation WebEffectiveRateChangedListenerObjCAdapter {
    ThreadSafeWeakPtr<WebCore::EffectiveRateChangedListener> _listener;
}

- (instancetype)initWithEffectiveRateChangedListener:(const WebCore::EffectiveRateChangedListener&)listener
{
    if ((self = [super init]))
        _listener = listener;
    return self;
}

- (RefPtr<WebCore::EffectiveRateChangedListener>)listener
{
    return _listener.get();
}
@end

namespace WebCore {

static void timebaseEffectiveRateChangedCallback(CFNotificationCenterRef, void* observer, CFNotificationName, const void*, CFDictionaryRef)
{
    RetainPtr adapter { dynamic_objc_cast<WebEffectiveRateChangedListenerObjCAdapter>(reinterpret_cast<id>(observer)) };
    if (RefPtr listener = [adapter listener])
        listener->effectiveRateChanged();
}

EffectiveRateChangedListener::EffectiveRateChangedListener(Function<void(double)>&& callback, CMTimebaseRef timebase)
    : m_callback(WTF::move(callback))
    , m_objcAdapter(adoptNS([[WebEffectiveRateChangedListenerObjCAdapter alloc] initWithEffectiveRateChangedListener:*this]))
    , m_timebase(timebase)
{
    ASSERT(timebase);
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenterSingleton(), m_objcAdapter.get(), timebaseEffectiveRateChangedCallback, kCMTimebaseNotification_EffectiveRateChanged, timebase, static_cast<CFNotificationSuspensionBehavior>(_CFNotificationObserverIsObjC));
}

EffectiveRateChangedListener::~EffectiveRateChangedListener()
{
    stop();
}

void EffectiveRateChangedListener::effectiveRateChanged()
{
    m_callback(PAL::CMTimebaseGetRate(m_timebase.get()));
}

void EffectiveRateChangedListener::stop()
{
    if (m_stopped.exchange(true))
        return;
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenterSingleton(), m_objcAdapter.get(), kCMTimebaseNotification_EffectiveRateChanged, m_timebase.get());
}

} // namespace WebCore
