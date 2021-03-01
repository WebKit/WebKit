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
#import "WKMouseDeviceObserver.h"

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT) && PLATFORM(IOS)

#import "WebProcessProxy.h"
#import <wtf/BlockPtr.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>

@implementation WKMouseDeviceObserver {
    BOOL _hasMouseDevice;
    size_t _startCount;
    RetainPtr<id<BSInvalidatable>> _token;
    OSObjectPtr<dispatch_queue_t> _deviceObserverTokenQueue;
}

+ (WKMouseDeviceObserver *)sharedInstance
{
    static WKMouseDeviceObserver *instance;
    if (!instance)
        instance = [[WKMouseDeviceObserver alloc] init];
    return instance;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _deviceObserverTokenQueue = adoptOSObject(dispatch_queue_create("WKMouseDeviceObserver _deviceObserverTokenQueue", DISPATCH_QUEUE_SERIAL));

    return self;
}

#pragma mark - BKSMousePointerDeviceObserver state

- (void)start
{
    [self startWithCompletionHandler:^{ }];
}

- (void)startWithCompletionHandler:(void (^)(void))completionHandler
{
    if (++_startCount > 1)
        return;

    dispatch_async(_deviceObserverTokenQueue.get(), [strongSelf = retainPtr(self), completionHandler = makeBlockPtr(completionHandler)] {
        ASSERT(!strongSelf->_token);
        strongSelf->_token = [[BKSMousePointerService sharedInstance] addPointerDeviceObserver:strongSelf.get()];

        completionHandler();
    });
}

- (void)stop
{
    [self stopWithCompletionHandler:^{ }];
}

- (void)stopWithCompletionHandler:(void (^)(void))completionHandler
{
    ASSERT(_startCount);
    if (!_startCount || --_startCount)
        return;

    dispatch_async(_deviceObserverTokenQueue.get(), [strongSelf = retainPtr(self), completionHandler = makeBlockPtr(completionHandler)] {
        ASSERT(strongSelf->_token);
        [strongSelf->_token invalidate];
        strongSelf->_token = nil;

        completionHandler();
    });
}

#pragma mark - BKSMousePointerDeviceObserver handlers

- (void)mousePointerDevicesDidChange:(NSSet<BKSMousePointerDevice *> *)mousePointerDevices
{
    BOOL hasMouseDevice = mousePointerDevices.count > 0;
    if (hasMouseDevice == _hasMouseDevice)
        return;

    _hasMouseDevice = hasMouseDevice;

    WebKit::WebProcessProxy::notifyHasMouseDeviceChanged();
}

#pragma mark - Testing

- (void)_setHasMouseDeviceForTesting:(BOOL)hasMouseDevice
{
    _hasMouseDevice = hasMouseDevice;

    WebKit::WebProcessProxy::notifyHasMouseDeviceChanged();
}

@end

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT) && PLATFORM(IOS)
