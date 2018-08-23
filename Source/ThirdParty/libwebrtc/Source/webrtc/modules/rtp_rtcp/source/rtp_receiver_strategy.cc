/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_receiver_strategy.h"

#include <stdlib.h>

namespace webrtc {

RTPReceiverStrategy::RTPReceiverStrategy(RtpData* data_callback)
    : data_callback_(data_callback) {}

RTPReceiverStrategy::~RTPReceiverStrategy() = default;

}  // namespace webrtc
