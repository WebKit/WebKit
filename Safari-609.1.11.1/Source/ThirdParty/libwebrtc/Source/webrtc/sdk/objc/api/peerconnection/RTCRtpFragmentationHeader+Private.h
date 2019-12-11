/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "base/RTCRtpFragmentationHeader.h"

#include "modules/include/module_common_types.h"

NS_ASSUME_NONNULL_BEGIN

/* Interfaces for converting to/from internal C++ formats. */
@interface RTCRtpFragmentationHeader (Private)

- (instancetype)initWithNativeFragmentationHeader:
        (const webrtc::RTPFragmentationHeader *__nullable)fragmentationHeader;
- (std::unique_ptr<webrtc::RTPFragmentationHeader>)createNativeFragmentationHeader;

@end

NS_ASSUME_NONNULL_END
