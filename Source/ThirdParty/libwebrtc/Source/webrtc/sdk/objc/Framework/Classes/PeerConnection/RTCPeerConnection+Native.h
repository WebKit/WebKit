/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCPeerConnection.h"

#include <memory>

namespace rtc {
class BitrateAllocationStrategy;
}  // namespace rtc

NS_ASSUME_NONNULL_BEGIN

/**
 * This class extension exposes methods that work directly with injectable C++ components.
 */
@interface RTCPeerConnection ()

/** Sets current strategy. If not set default WebRTC allocator will be used. May be changed during
 *  an active session.
 */
- (void)setBitrateAllocationStrategy:
        (std::unique_ptr<rtc::BitrateAllocationStrategy>)bitrateAllocationStrategy;

@end

NS_ASSUME_NONNULL_END
