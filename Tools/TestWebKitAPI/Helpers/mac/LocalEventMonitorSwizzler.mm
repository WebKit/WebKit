/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "Helpers/mac/LocalEventMonitorSwizzler.h"

#if PLATFORM(MAC)

#import <AppKit/AppKit.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

static BlockPtr<NSEvent*(NSEvent*)> gEventMonitorHandler;

// Stands in for AppKit's local event observer, so that +[NSEvent removeMonitor:] accepts it.
@interface TestLocalEventObserver : NSObject {
    NSEventMask _mask;
    id _block;
    BOOL _isAdditive;
}
+ (void)initialize;
- (instancetype)initMatchingEvents:(NSEventMask)mask handler:(NSEvent *(^)(NSEvent *))block;
- (void)invalidate;
- (void)dealloc;
- (void)recomputeObserverMask;
@end

@implementation TestLocalEventObserver
+ (void)initialize
{
}

- (instancetype)initMatchingEvents:(NSEventMask)mask handler:(NSEvent *(^)(NSEvent *))block
{
    self = [super init];
    return self;
}

- (void)dealloc
{
    [super dealloc];
}

- (void)invalidate
{
}

- (void)recomputeObserverMask
{
}
@end

@interface TestEventMonitor : NSObject

+ (id)addLocalMonitorForEventsMatchingMask:(NSEventMask)mask handler:(NSEvent* (^)(NSEvent *event))block;

@end

@implementation TestEventMonitor

+ (id)addLocalMonitorForEventsMatchingMask:(NSEventMask)mask handler:(NSEvent* (^)(NSEvent *event))block
{
    gEventMonitorHandler = makeBlockPtr(block);
    return adoptNS([[TestLocalEventObserver alloc] initMatchingEvents:mask handler:block]).leakRef();
}

@end

namespace TestWebKitAPI {

LocalEventMonitorSwizzler::LocalEventMonitorSwizzler()
    : m_swizzler(NSEvent.class, @selector(addLocalMonitorForEventsMatchingMask:handler:), [TestEventMonitor methodForSelector:@selector(addLocalMonitorForEventsMatchingMask:handler:)])
{
}

NSEvent *LocalEventMonitorSwizzler::sendEventToMonitor(NSEvent *event)
{
    return gEventMonitorHandler ? gEventMonitorHandler(event) : event;
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
