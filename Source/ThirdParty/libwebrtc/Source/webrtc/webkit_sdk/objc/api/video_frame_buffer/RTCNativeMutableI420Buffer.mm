/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCNativeMutableI420Buffer.h"

#import "RTCNativeI420Buffer+Private.h"

#include "api/video/i420_buffer.h"

@implementation RTCMutableI420Buffer

- (uint8_t *)mutableDataY {
  return static_cast<webrtc::I420Buffer *>(_i420Buffer.get())->MutableDataY();
}

- (uint8_t *)mutableDataU {
  return static_cast<webrtc::I420Buffer *>(_i420Buffer.get())->MutableDataU();
}

- (uint8_t *)mutableDataV {
  return static_cast<webrtc::I420Buffer *>(_i420Buffer.get())->MutableDataV();
}

@end
