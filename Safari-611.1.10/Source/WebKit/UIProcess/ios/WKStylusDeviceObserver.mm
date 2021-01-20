/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WKStylusDeviceObserver.h"

#if HAVE(PENCILKIT_TEXT_INPUT)

#import "WebProcessProxy.h"
#import <UIKit/UIScribbleInteraction.h>
#import <WebCore/VersionChecks.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>

static Seconds changeTimeInterval { 10_min };

@implementation WKStylusDeviceObserver {
    BOOL _hasStylusDevice;
    size_t _startCount;
    RetainPtr<NSTimer> _changeTimer;
}

+ (WKStylusDeviceObserver *)sharedInstance
{
    static WKStylusDeviceObserver *instance;
    if (!instance)
        instance = [[WKStylusDeviceObserver alloc] init];
    return instance;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _hasStylusDevice = UIScribbleInteraction.isPencilInputExpected;

    if (NSNumber *changeTimeIntervalOverride = [NSUserDefaults.standardUserDefaults objectForKey:@"WKStylusDeviceObserverChangeTimeInterval"]) {
        changeTimeInterval = Seconds([changeTimeIntervalOverride doubleValue]);
        WTFLogAlways("Warning: WKStylusDeviceObserver changeTimeInterval was overriden via user defaults and is now %g seconds", changeTimeInterval.seconds());
    }

    return self;
}

#pragma mark - State

- (void)setHasStylusDevice:(BOOL)hasStylusDevice
{
    if (hasStylusDevice != _hasStylusDevice) {
        _hasStylusDevice = hasStylusDevice;

        WebKit::WebProcessProxy::notifyHasStylusDeviceChanged(_hasStylusDevice);
    }

    [_changeTimer invalidate];
    _changeTimer = nil;
}

- (void)start
{
    if (++_startCount > 1)
        return;

    if (WebCore::linkedOnOrAfter(WebCore::SDKVersion::FirstThatObservesClassProperty))
        [[UIScribbleInteraction class] addObserver:self forKeyPath:@"isPencilInputExpected" options:NSKeyValueObservingOptionInitial context:nil];
}

- (void)stop
{
    ASSERT(_startCount);
    if (!_startCount || --_startCount)
        return;

    if (WebCore::linkedOnOrAfter(WebCore::SDKVersion::FirstThatObservesClassProperty))
        [[UIScribbleInteraction class] removeObserver:self forKeyPath:@"isPencilInputExpected"];
}

#pragma mark - isPencilInputExpected KVO

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    ASSERT([keyPath isEqualToString:@"isPencilInputExpected"]);
    ASSERT(object == [UIScribbleInteraction class]);

    if (UIScribbleInteraction.isPencilInputExpected)
        self.hasStylusDevice = YES;
    else
        [self startChangeTimer:changeTimeInterval.seconds()];
}

#pragma mark - isPencilInputExpected Timer

- (void)startChangeTimer:(NSTimeInterval)timeInterval
{
    [_changeTimer invalidate];
    _changeTimer = [NSTimer scheduledTimerWithTimeInterval:timeInterval target:self selector:@selector(changeTimerFired:) userInfo:nil repeats:NO];
}

- (void)changeTimerFired:(NSTimer *)timer
{
    self.hasStylusDevice = NO;
}

@end

#endif // HAVE(PENCILKIT_TEXT_INPUT)
