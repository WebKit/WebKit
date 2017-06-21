/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_DELAY_ANALYZER_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_DELAY_ANALYZER_H_

#include <map>
#include <vector>

#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_coding/neteq/tools/neteq_input.h"
#include "webrtc/modules/audio_coding/neteq/tools/neteq_test.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

class NetEqDelayAnalyzer : public test::NetEqPostInsertPacket,
                           public test::NetEqGetAudioCallback {
 public:
  void AfterInsertPacket(const test::NetEqInput::PacketData& packet,
                         NetEq* neteq) override;

  void BeforeGetAudio(NetEq* neteq) override;

  void AfterGetAudio(int64_t time_now_ms,
                     const AudioFrame& audio_frame,
                     bool muted,
                     NetEq* neteq) override;

  void CreateGraphs(std::vector<float>* send_times_s,
                    std::vector<float>* arrival_delay_ms,
                    std::vector<float>* corrected_arrival_delay_ms,
                    std::vector<rtc::Optional<float>>* playout_delay_ms,
                    std::vector<rtc::Optional<float>>* target_delay_ms) const;

 private:
  struct TimingData {
    explicit TimingData(double at) : arrival_time_ms(at) {}
    double arrival_time_ms;
    rtc::Optional<int64_t> decode_get_audio_count;
    rtc::Optional<int64_t> sync_delay_ms;
    rtc::Optional<int> target_delay_ms;
    rtc::Optional<int> current_delay_ms;
  };
  std::map<uint32_t, TimingData> data_;
  std::vector<int64_t> get_audio_time_ms_;
  size_t get_audio_count_ = 0;
  size_t last_sync_buffer_ms_ = 0;
  int last_sample_rate_hz_ = 0;
};

}  // namespace test
}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_DELAY_ANALYZER_H_
