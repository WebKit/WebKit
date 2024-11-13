/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if HAVE(APPLE_LOW_POWER_MODE_SUPPORT)
#import "LowPowerModeNotifier.h"

#import "Logging.h"
#import <Foundation/NSProcessInfo.h>
#import <wtf/MainThread.h>

@interface WebLowPowerModeObserver : NSObject
@property (nonatomic) CheckedPtr<WebCore::LowPowerModeNotifier> notifier;
@property (nonatomic, readonly) BOOL isLowPowerModeEnabled;
@end

@implementation WebLowPowerModeObserver {
}

- (WebLowPowerModeObserver *)initWithNotifier:(WebCore::LowPowerModeNotifier&)notifier
{
    self = [super init];
    if (!self)
        return nil;

    _notifier = &notifier;
    _isLowPowerModeEnabled = [NSProcessInfo processInfo].lowPowerModeEnabled;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_didReceiveLowPowerModeChange) name:NSProcessInfoPowerStateDidChangeNotification object:nil];
    return self;
}

- (void)dealloc
{
    if (_notifier)
        [self detach];
    [super dealloc];
}

- (void)detach
{
    ASSERT(isMainThread());
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSProcessInfoPowerStateDidChangeNotification object:nil];
    _notifier = nullptr;
}

- (void)_didReceiveLowPowerModeChange
{
    // We need to make sure we notify the client on the main thread.
    ensureOnMainRunLoop([self, protectedSelf = RetainPtr<WebLowPowerModeObserver>(self), lowPowerModeEnabled = [NSProcessInfo processInfo].lowPowerModeEnabled] {
        if (!_notifier)
            return;
        _isLowPowerModeEnabled = lowPowerModeEnabled;
        notifyLowPowerModeChanged(*_notifier, _isLowPowerModeEnabled);
    });
}

@end

namespace WebCore {

LowPowerModeNotifier::LowPowerModeNotifier(LowPowerModeChangeCallback&& callback)
    : m_observer(adoptNS([[WebLowPowerModeObserver alloc] initWithNotifier:*this]))
    , m_callback(WTFMove(callback))
{
    ASSERT(isMainThread());
}

LowPowerModeNotifier::~LowPowerModeNotifier()
{
    ASSERT(isMainThread());
    [m_observer detach];
}

bool LowPowerModeNotifier::isLowPowerModeEnabled() const
{
    return m_observer.get().isLowPowerModeEnabled;
}

void LowPowerModeNotifier::notifyLowPowerModeChanged(bool enabled)
{
    m_callback(enabled);
}

void notifyLowPowerModeChanged(LowPowerModeNotifier& notifier, bool enabled)
{
    RELEASE_LOG(PerformanceLogging, "Low power mode state has changed to %d", enabled);
    notifier.notifyLowPowerModeChanged(enabled);
}

} // namespace WebCore

#endif // HAVE(APPLE_LOW_POWER_MODE_SUPPORT)
