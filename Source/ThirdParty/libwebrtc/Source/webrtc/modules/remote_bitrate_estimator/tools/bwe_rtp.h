/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TOOLS_BWE_RTP_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TOOLS_BWE_RTP_H_

#include <string>

namespace webrtc {
class Clock;
class RemoteBitrateEstimator;
class RemoteBitrateObserver;
class RtpHeaderParser;
namespace test {
class RtpFileReader;
}
}

bool ParseArgsAndSetupEstimator(
    int argc,
    char** argv,
    webrtc::Clock* clock,
    webrtc::RemoteBitrateObserver* observer,
    webrtc::test::RtpFileReader** rtp_reader,
    webrtc::RtpHeaderParser** parser,
    webrtc::RemoteBitrateEstimator** estimator,
    std::string* estimator_used);

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TOOLS_BWE_RTP_H_
