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

#if !TARGET_OS_IPHONE

#import <AppKit/NSOpenGLView.h>

#import <WebRTC/RTCVideoRenderer.h>

NS_ASSUME_NONNULL_BEGIN

@class RTCNSGLVideoView;
@protocol RTCNSGLVideoViewDelegate

- (void)videoView:(RTCNSGLVideoView *)videoView didChangeVideoSize:(CGSize)size;

@end

@interface RTCNSGLVideoView : NSOpenGLView <RTCVideoRenderer>

@property(nonatomic, weak) id<RTCNSGLVideoViewDelegate> delegate;

@end

NS_ASSUME_NONNULL_END

#endif
