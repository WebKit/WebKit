/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/media_transport_config.h"

#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

MediaTransportConfig::MediaTransportConfig(
    MediaTransportInterface* media_transport)
    : media_transport(media_transport) {
  RTC_DCHECK(media_transport != nullptr);
}

MediaTransportConfig::MediaTransportConfig(size_t rtp_max_packet_size)
    : rtp_max_packet_size(rtp_max_packet_size) {
  RTC_DCHECK_GT(rtp_max_packet_size, 0);
}

std::string MediaTransportConfig::DebugString()
    const {  // TODO(sukhanov): Add rtp_max_packet_size (requires fixing
             // audio_send/receive_stream_unittest.cc).
  rtc::StringBuilder result;
  result << "{media_transport: "
         << (media_transport != nullptr ? "(Transport)" : "null") << "}";
  return result.Release();
}

}  // namespace webrtc
