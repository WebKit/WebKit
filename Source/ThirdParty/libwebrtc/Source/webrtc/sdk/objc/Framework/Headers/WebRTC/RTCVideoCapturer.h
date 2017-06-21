/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <WebRTC/RTCVideoFrame.h>

NS_ASSUME_NONNULL_BEGIN

@class RTCVideoCapturer;

RTC_EXPORT
@protocol RTCVideoCapturerDelegate <NSObject>
- (void)capturer:(RTCVideoCapturer *)capturer didCaptureVideoFrame:(RTCVideoFrame *)frame;
@end

RTC_EXPORT
@interface RTCVideoCapturer : NSObject

@property(nonatomic, readonly, weak) id<RTCVideoCapturerDelegate> delegate;

- (instancetype)initWithDelegate:(id<RTCVideoCapturerDelegate>)delegate;

@end

NS_ASSUME_NONNULL_END
