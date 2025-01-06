/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoFrame.h"
#import "RTCVideoRenderer.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * RTCMTLVideoView is thin wrapper around MTKView.
 *
 * It has id<RTCVideoRenderer> property that renders video frames in the view's
 * bounds using Metal.
 * NOTE: always check if metal is available on the running device via
 * RTC_SUPPORTS_METAL macro before initializing this class.
 */
NS_CLASS_AVAILABLE_IOS(9)

RTC_OBJC_EXPORT
@interface RTCMTLVideoView : UIView<RTCVideoRenderer>

@property(nonatomic, weak) id<RTCVideoViewDelegate> delegate;

@property(nonatomic) UIViewContentMode videoContentMode;

/** @abstract Enables/disables rendering.
 */
@property(nonatomic, getter=isEnabled) BOOL enabled;

/** @abstract Wrapped RTCVideoRotation, or nil.
 */
@property(nonatomic, nullable) NSValue* rotationOverride;

@end

NS_ASSUME_NONNULL_END
