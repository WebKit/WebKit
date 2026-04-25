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
#import "WKScreenTimeConfigurationObserver.h"

#if ENABLE(SCREEN_TIME)

#import "WKWebViewInternal.h"
#import <wtf/NeverDestroyed.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/darwin/DispatchExtras.h>

#import <pal/cocoa/ScreenTimeSoftLink.h>

static void *screenTimeConfigurationObserverKVOContext = &screenTimeConfigurationObserverKVOContext;

static dispatch_queue_t screenTimeUpdateQueueSingleton()
{
    static NeverDestroyed<OSObjectPtr<dispatch_queue_t>> queue = adoptOSObject(dispatch_queue_create("com.apple.WebKit.ScreenTimeUpdateQueue", DISPATCH_QUEUE_SERIAL));
    return queue.get().get();
}

@implementation WKScreenTimeConfigurationObserver {
    RetainPtr<STScreenTimeConfigurationObserver> _screenTimeConfigurationObserver;
    WeakObjCPtr<WKWebView> _webView;
}

- (instancetype)initWithView:(WKWebView *)view
{
    if (!(self = [super init]))
        return nil;

    _webView = view;

    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    if (context == &screenTimeConfigurationObserverKVOContext) {
        ensureOnMainRunLoop([webView = _webView] {
            [webView _updateScreenTimeBasedOnWindowVisibility];
        });
        return;
    }

    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

- (void)startObserving
{
    if (_screenTimeConfigurationObserver)
        return;

    _screenTimeConfigurationObserver = adoptNS([PAL::allocSTScreenTimeConfigurationObserverInstance() initWithUpdateQueue:screenTimeUpdateQueueSingleton()]);
    [_screenTimeConfigurationObserver addObserver:self forKeyPath:@"configuration.enforcesChildRestrictions" options:0 context:&screenTimeConfigurationObserverKVOContext];
    [_screenTimeConfigurationObserver startObserving];
}

- (void)stopObserving
{
    [_screenTimeConfigurationObserver stopObserving];
    [_screenTimeConfigurationObserver removeObserver:self forKeyPath:@"configuration.enforcesChildRestrictions" context:&screenTimeConfigurationObserverKVOContext];
    _screenTimeConfigurationObserver = nil;
}

- (BOOL)enforcesChildRestrictions
{
    return [_screenTimeConfigurationObserver configuration].enforcesChildRestrictions;
}

@end

#endif
