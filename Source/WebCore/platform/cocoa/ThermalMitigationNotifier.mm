/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "ThermalMitigationNotifier.h"

#if HAVE(APPLE_THERMAL_MITIGATION_SUPPORT)

#import "Logging.h"
#import <Foundation/NSProcessInfo.h>
#import <wtf/MainThread.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {
static bool isThermalMitigationEnabled()
{
    return NSProcessInfo.processInfo.thermalState >= NSProcessInfoThermalStateSerious;
}
}

@interface WebThermalMitigationObserver : NSObject
@property (nonatomic) WeakPtr<WebCore::ThermalMitigationNotifier> notifier;
@property (nonatomic, readonly) BOOL thermalMitigationEnabled;
@end

@implementation WebThermalMitigationObserver

- (WebThermalMitigationObserver *)initWithNotifier:(WebCore::ThermalMitigationNotifier&)notifier
{
    self = [super init];
    if (!self)
        return nil;

    _notifier = notifier;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(thermalStateDidChange) name:NSProcessInfoThermalStateDidChangeNotification object:nil];
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSProcessInfoThermalStateDidChangeNotification object:nil];
    [super dealloc];
}

- (void)thermalStateDidChange
{
    callOnMainThread([self, protectedSelf = RetainPtr<WebThermalMitigationObserver>(self)] {
        if (CheckedPtr notifier = _notifier.get())
            notifyThermalMitigationChanged(*notifier, self.thermalMitigationEnabled);
    });
}

- (BOOL)thermalMitigationEnabled
{
    return WebCore::isThermalMitigationEnabled();
}

@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ThermalMitigationNotifier);

ThermalMitigationNotifier::ThermalMitigationNotifier(ThermalMitigationChangeCallback&& callback)
    : m_observer(adoptNS([[WebThermalMitigationObserver alloc] initWithNotifier:*this]))
    , m_callback(WTFMove(callback))
{
}

ThermalMitigationNotifier::~ThermalMitigationNotifier()
{
    [m_observer setNotifier:nil];
}

bool ThermalMitigationNotifier::thermalMitigationEnabled() const
{
    return [m_observer thermalMitigationEnabled];
}

bool ThermalMitigationNotifier::isThermalMitigationEnabled()
{
    return WebCore::isThermalMitigationEnabled();
}

void ThermalMitigationNotifier::notifyThermalMitigationChanged(bool enabled)
{
    m_callback(enabled);
}

void notifyThermalMitigationChanged(ThermalMitigationNotifier& notifier, bool enabled)
{
    RELEASE_LOG(PerformanceLogging, "Thermal mitigation is enabled: %d", enabled);
    notifier.notifyThermalMitigationChanged(enabled);
}

} // namespace WebCore

#endif // HAVE(APPLE_THERMAL_MITIGATION_SUPPORT)
