/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCMetricsSampleInfo.h"

// Adding 'nogncheck' to disable the gn include headers check.
// We don't want to depend on 'system_wrappers:metrics_default' because
// clients should be able to provide their own implementation.
#include "webrtc/system_wrappers/include/metrics_default.h"  // nogncheck

NS_ASSUME_NONNULL_BEGIN

@interface RTCMetricsSampleInfo ()

/** Initialize an RTCMetricsSampleInfo object from native SampleInfo. */
- (instancetype)initWithNativeSampleInfo:
    (const webrtc::metrics::SampleInfo &)info;

@end

NS_ASSUME_NONNULL_END
