/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "RTCMacros.h"
#import "RTCVideoRenderer.h"
#import "RTCVideoViewShading.h"

NS_ASSUME_NONNULL_BEGIN

@class RTCEAGLVideoView;

/**
 * RTCEAGLVideoView is an RTCVideoRenderer which renders video frames in its
 * bounds using OpenGLES 2.0 or OpenGLES 3.0.
 */
RTC_OBJC_EXPORT
NS_EXTENSION_UNAVAILABLE_IOS("Rendering not available in app extensions.")
@interface RTCEAGLVideoView : UIView <RTCVideoRenderer>

@property(nonatomic, weak) id<RTCVideoViewDelegate> delegate;

- (instancetype)initWithFrame:(CGRect)frame
                       shader:(id<RTCVideoViewShading>)shader NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder *)aDecoder
                       shader:(id<RTCVideoViewShading>)shader NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
