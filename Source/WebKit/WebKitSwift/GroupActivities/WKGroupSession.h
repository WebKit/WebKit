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

#pragma once

#import <wtf/Platform.h>

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, WKGroupSessionState) {
    WKGroupSessionStateWaiting = 0,
    WKGroupSessionStateJoined = 1,
    WKGroupSessionStateInvalidated = 2,
};

@class AVPlaybackCoordinator;

@interface WKURLActivity : NSObject
@property (nonatomic, copy, readonly, nullable) NSURL *fallbackURL;
@end

NS_SWIFT_UI_ACTOR
@interface WKGroupSession : NSObject
@property (nonatomic, readonly) WKURLActivity *activity;
@property (nonatomic, readonly, copy) NSUUID *uuid;
@property (nonatomic, readonly) WKGroupSessionState state;
@property (nonatomic, copy, nullable) void (^newActivityCallback)(WKURLActivity *);
@property (nonatomic, copy, nullable) void (^stateChangedCallback)(WKGroupSessionState);
- (void)join;
- (void)leave;
- (void)coordinateWithCoordinator:(AVPlaybackCoordinator *)playbackCoordinator;
@end

NS_SWIFT_UI_ACTOR
@interface WKGroupSessionObserver : NSObject
@property (nonatomic, copy, nullable) void (^newSessionCallback)(WKGroupSession *);
@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
