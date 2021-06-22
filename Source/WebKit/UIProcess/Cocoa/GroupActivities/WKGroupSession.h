/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, WKGroupSessionState) {
    WKGroupSessionStateWaiting = 0,
    WKGroupSessionStateJoined = 1,
    WKGroupSessionStateInvalidated = 2,
};

@class AVPlaybackCoordinator;
@class NSURL;
@class NSUUID;
@class WKGroupSession;
@class WKURLActivity;

NS_ASSUME_NONNULL_BEGIN

@interface WKGroupSessionObserver : NSObject
@property (nonatomic, copy) void (^ _Nullable newSessionCallback)(WKGroupSession *);
- (nonnull instancetype)init;
@end

@interface WKGroupSession : NSObject
@property (nonatomic, readonly, strong) WKURLActivity *activity;
@property (nonatomic, readonly, copy) NSUUID *uuid;
@property (nonatomic, readonly) WKGroupSessionState state;
@property (nonatomic, copy) void (^ _Nullable newActivityCallback)(WKURLActivity *);
@property (nonatomic, copy) void (^ _Nullable stateChangedCallback)(WKGroupSessionState);
- (void)join;
- (void)leave;
- (void)coordinateWithCoordinator:(AVPlaybackCoordinator *)playbackCoordinator;
@end

@interface WKURLActivity : NSObject
@property (nonatomic, copy) NSURL * _Nullable fallbackURL;
@end

NS_ASSUME_NONNULL_END
