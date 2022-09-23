/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebNotification.h"

#import "WebNotificationInternal.h"

#if ENABLE(NOTIFICATIONS)
#import "WebSecurityOriginInternal.h"
#import <WebCore/Notification.h>
#import <WebCore/NotificationData.h>
#import <WebCore/ScriptExecutionContext.h>
#import <wtf/RefPtr.h>

using namespace WebCore;
#endif

@interface WebNotificationPrivate : NSObject
{
@public
#if ENABLE(NOTIFICATIONS)
    std::optional<NotificationData> _internal;
#endif
}
@end

@implementation WebNotificationPrivate
@end

#if ENABLE(NOTIFICATIONS)
@implementation WebNotification (WebNotificationInternal)

- (id)initWithCoreNotification:(NotificationData&&)coreNotification
{
    if (!(self = [super init]))
        return nil;
    _private = [[WebNotificationPrivate alloc] init];
    _private->_internal = WTFMove(coreNotification);
    return self;
}
@end
#endif

@implementation WebNotification
- (id)init
{
    return nil;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSString *)title
{
#if ENABLE(NOTIFICATIONS)
    return _private->_internal->title;
#else
    return nil;
#endif
}

- (NSString *)body
{
#if ENABLE(NOTIFICATIONS)
    return _private->_internal->body;
#else
    return nil;
#endif
}

- (NSString *)tag
{
#if ENABLE(NOTIFICATIONS)
    return _private->_internal->tag;
#else
    return nil;
#endif
}

- (NSString *)iconURL
{
#if ENABLE(NOTIFICATIONS)
    return _private->_internal->iconURL;
#else
    return nil;
#endif
}

- (NSString *)lang
{
#if ENABLE(NOTIFICATIONS)
    return _private->_internal->language;
#else
    return nil;
#endif
}

- (NSString *)dir
{
#if ENABLE(NOTIFICATIONS)
    switch (_private->_internal->direction) {
        case Notification::Direction::Auto:
            return @"auto";
        case Notification::Direction::Ltr:
            return @"ltr";
        case Notification::Direction::Rtl:
            return @"rtl";
    }
#else
    return nil;
#endif
}

- (WebSecurityOrigin *)origin
{
#if ENABLE(NOTIFICATIONS)
    return adoptNS([[WebSecurityOrigin alloc] _initWithString:_private->_internal->originString]).autorelease();
#else
    return nil;
#endif
}

- (NSString *)notificationID
{
#if ENABLE(NOTIFICATIONS)
    return _private->_internal->notificationID.toString();
#else
    return 0;
#endif
}

- (void)dispatchShowEvent
{
#if ENABLE(NOTIFICATIONS)
    Notification::ensureOnNotificationThread(*_private->_internal, [](auto* notification) {
        if (notification)
            notification->dispatchShowEvent();
    });
#endif
}

- (void)dispatchCloseEvent
{
#if ENABLE(NOTIFICATIONS)
    Notification::ensureOnNotificationThread(*_private->_internal, [](auto* notification) {
        if (notification)
            notification->dispatchCloseEvent();
    });
#endif
}

- (void)dispatchClickEvent
{
#if ENABLE(NOTIFICATIONS)
    Notification::ensureOnNotificationThread(*_private->_internal, [](auto* notification) {
        if (notification)
            notification->dispatchClickEvent();
    });
#endif
}

- (void)dispatchErrorEvent
{
#if ENABLE(NOTIFICATIONS)
    Notification::ensureOnNotificationThread(*_private->_internal, [](auto* notification) {
        if (notification)
            notification->dispatchErrorEvent();
    });
#endif
}

- (void)finalize
{
#if ENABLE(NOTIFICATIONS)
    Notification::ensureOnNotificationThread(*_private->_internal, [](auto* notification) {
        if (notification)
            notification->finalize();
    });
#endif
}

@end

