/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>

#import "RTCI420Buffer.h"
#import "RTCMutableYUVPlanarBuffer.h"

NS_ASSUME_NONNULL_BEGIN

/** Extension of the I420 buffer with mutable data access */
RTC_OBJC_EXPORT
@protocol RTCMutableI420Buffer <RTCI420Buffer, RTCMutableYUVPlanarBuffer>
@end

NS_ASSUME_NONNULL_END
