/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TEST_AUTO_TEST_FAKES_LOUDEST_FILTER_H_
#define WEBRTC_VOICE_ENGINE_TEST_AUTO_TEST_FAKES_LOUDEST_FILTER_H_

#include <map>
#include "webrtc/base/timeutils.h"
#include "webrtc/common_types.h"

namespace voetest {

class LoudestFilter {
 public:
  /* ForwardThisPacket()
   * Decide whether to forward a RTP packet, given its header.
   *
   * Input:
   *   rtp_header : Header of the RTP packet of interest.
   */
  bool ForwardThisPacket(const webrtc::RTPHeader& rtp_header);

 private:
  struct Status {
    void Set(int audio_level, int64_t last_time_ms) {
      this->audio_level = audio_level;
      this->last_time_ms = last_time_ms;
    }
    int audio_level;
    int64_t last_time_ms;
  };

  void RemoveTimeoutStreams(int64_t time_ms);
  unsigned int FindQuietestStream();

  // Keeps the streams being forwarded in pair<SSRC, Status>.
  std::map<unsigned int, Status> stream_levels_;

  const int32_t kStreamTimeOutMs = 5000;
  const size_t kMaxMixSize = 3;
  const int kInvalidAudioLevel = 128;
};


}  // namespace voetest

#endif  // WEBRTC_VOICE_ENGINE_TEST_AUTO_TEST_FAKES_LOUDEST_FILTER_H_
