/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "base/RTCEncodedImage.h"

#include "common_video/include/video_frame.h"

NS_ASSUME_NONNULL_BEGIN

/* Interfaces for converting to/from internal C++ formats. */
@interface RTCEncodedImage (Private)

- (instancetype)initWithNativeEncodedImage:(webrtc::EncodedImage)encodedImage;
- (webrtc::EncodedImage)nativeEncodedImage;

@end

NS_ASSUME_NONNULL_END
