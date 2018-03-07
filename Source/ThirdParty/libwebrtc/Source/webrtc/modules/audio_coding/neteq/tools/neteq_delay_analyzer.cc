/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_delay_analyzer.h"

#include <algorithm>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {
namespace test {
namespace {
// Helper function for NetEqDelayAnalyzer::CreateGraphs. Returns the
// interpolated value of a function at the point x. Vector x_vec contains the
// sample points, and y_vec contains the function values at these points. The
// return value is a linear interpolation between y_vec values.
double LinearInterpolate(double x,
                         const std::vector<int64_t>& x_vec,
                         const std::vector<int64_t>& y_vec) {
  // Find first element which is larger than x.
  auto it = std::upper_bound(x_vec.begin(), x_vec.end(), x);
  if (it == x_vec.end()) {
    --it;
  }
  const size_t upper_ix = it - x_vec.begin();

  size_t lower_ix;
  if (upper_ix == 0 || x_vec[upper_ix] <= x) {
    lower_ix = upper_ix;
  } else {
    lower_ix = upper_ix - 1;
  }
  double y;
  if (lower_ix == upper_ix) {
    y = y_vec[lower_ix];
  } else {
    RTC_DCHECK_NE(x_vec[lower_ix], x_vec[upper_ix]);
    y = (x - x_vec[lower_ix]) * (y_vec[upper_ix] - y_vec[lower_ix]) /
            (x_vec[upper_ix] - x_vec[lower_ix]) +
        y_vec[lower_ix];
  }
  return y;
}
}  // namespace

void NetEqDelayAnalyzer::AfterInsertPacket(
    const test::NetEqInput::PacketData& packet,
    NetEq* neteq) {
  data_.insert(
      std::make_pair(packet.header.timestamp, TimingData(packet.time_ms)));
  ssrcs_.insert(packet.header.ssrc);
  payload_types_.insert(packet.header.payloadType);
}

void NetEqDelayAnalyzer::BeforeGetAudio(NetEq* neteq) {
  last_sync_buffer_ms_ = neteq->SyncBufferSizeMs();
}

void NetEqDelayAnalyzer::AfterGetAudio(int64_t time_now_ms,
                                       const AudioFrame& audio_frame,
                                       bool /*muted*/,
                                       NetEq* neteq) {
  get_audio_time_ms_.push_back(time_now_ms);
  // Check what timestamps were decoded in the last GetAudio call.
  std::vector<uint32_t> dec_ts = neteq->LastDecodedTimestamps();
  // Find those timestamps in data_, insert their decoding time and sync
  // delay.
  for (uint32_t ts : dec_ts) {
    auto it = data_.find(ts);
    if (it == data_.end()) {
      // This is a packet that was split out from another packet. Skip it.
      continue;
    }
    auto& it_timing = it->second;
    RTC_CHECK(!it_timing.decode_get_audio_count)
        << "Decode time already written";
    it_timing.decode_get_audio_count = get_audio_count_;
    RTC_CHECK(!it_timing.sync_delay_ms) << "Decode time already written";
    it_timing.sync_delay_ms = last_sync_buffer_ms_;
    it_timing.target_delay_ms = neteq->TargetDelayMs();
    it_timing.current_delay_ms = neteq->FilteredCurrentDelayMs();
  }
  last_sample_rate_hz_ = audio_frame.sample_rate_hz_;
  ++get_audio_count_;
}

void NetEqDelayAnalyzer::CreateGraphs(
    std::vector<float>* send_time_s,
    std::vector<float>* arrival_delay_ms,
    std::vector<float>* corrected_arrival_delay_ms,
    std::vector<rtc::Optional<float>>* playout_delay_ms,
    std::vector<rtc::Optional<float>>* target_delay_ms) const {
  if (get_audio_time_ms_.empty()) {
    return;
  }
  // Create nominal_get_audio_time_ms, a vector starting at
  // get_audio_time_ms_[0] and increasing by 10 for each element.
  std::vector<int64_t> nominal_get_audio_time_ms(get_audio_time_ms_.size());
  nominal_get_audio_time_ms[0] = get_audio_time_ms_[0];
  std::transform(
      nominal_get_audio_time_ms.begin(), nominal_get_audio_time_ms.end() - 1,
      nominal_get_audio_time_ms.begin() + 1, [](int64_t& x) { return x + 10; });
  RTC_DCHECK(
      std::is_sorted(get_audio_time_ms_.begin(), get_audio_time_ms_.end()));

  std::vector<double> rtp_timestamps_ms;
  double offset = std::numeric_limits<double>::max();
  TimestampUnwrapper unwrapper;
  // This loop traverses data_ and populates rtp_timestamps_ms as well as
  // calculates the base offset.
  for (auto& d : data_) {
    rtp_timestamps_ms.push_back(
        unwrapper.Unwrap(d.first) /
        rtc::CheckedDivExact(last_sample_rate_hz_, 1000));
    offset =
        std::min(offset, d.second.arrival_time_ms - rtp_timestamps_ms.back());
  }

  // Calculate send times in seconds for each packet. This is the (unwrapped)
  // RTP timestamp in ms divided by 1000.
  send_time_s->resize(rtp_timestamps_ms.size());
  std::transform(rtp_timestamps_ms.begin(), rtp_timestamps_ms.end(),
                 send_time_s->begin(), [rtp_timestamps_ms](double x) {
                   return (x - rtp_timestamps_ms[0]) / 1000.f;
                 });
  RTC_DCHECK_EQ(send_time_s->size(), rtp_timestamps_ms.size());

  // This loop traverses the data again and populates the graph vectors. The
  // reason to have two loops and traverse twice is that the offset cannot be
  // known until the first traversal is done. Meanwhile, the final offset must
  // be known already at the start of this second loop.
  auto data_it = data_.cbegin();
  for (size_t i = 0; i < send_time_s->size(); ++i, ++data_it) {
    RTC_DCHECK(data_it != data_.end());
    const double offset_send_time_ms = rtp_timestamps_ms[i] + offset;
    const auto& timing = data_it->second;
    corrected_arrival_delay_ms->push_back(
        LinearInterpolate(timing.arrival_time_ms, get_audio_time_ms_,
                          nominal_get_audio_time_ms) -
        offset_send_time_ms);
    arrival_delay_ms->push_back(timing.arrival_time_ms - offset_send_time_ms);

    if (timing.decode_get_audio_count) {
      // This packet was decoded.
      RTC_DCHECK(timing.sync_delay_ms);
      const float playout_ms = *timing.decode_get_audio_count * 10 +
                               get_audio_time_ms_[0] + *timing.sync_delay_ms -
                               offset_send_time_ms;
      playout_delay_ms->push_back(playout_ms);
      RTC_DCHECK(timing.target_delay_ms);
      RTC_DCHECK(timing.current_delay_ms);
      const float target =
          playout_ms - *timing.current_delay_ms + *timing.target_delay_ms;
      target_delay_ms->push_back(target);
    } else {
      // This packet was never decoded. Mark target and playout delays as empty.
      playout_delay_ms->push_back(rtc::nullopt);
      target_delay_ms->push_back(rtc::nullopt);
    }
  }
  RTC_DCHECK(data_it == data_.end());
  RTC_DCHECK_EQ(send_time_s->size(), corrected_arrival_delay_ms->size());
  RTC_DCHECK_EQ(send_time_s->size(), playout_delay_ms->size());
  RTC_DCHECK_EQ(send_time_s->size(), target_delay_ms->size());
}

void NetEqDelayAnalyzer::CreateMatlabScript(
    const std::string& script_name) const {
  std::vector<float> send_time_s;
  std::vector<float> arrival_delay_ms;
  std::vector<float> corrected_arrival_delay_ms;
  std::vector<rtc::Optional<float>> playout_delay_ms;
  std::vector<rtc::Optional<float>> target_delay_ms;
  CreateGraphs(&send_time_s, &arrival_delay_ms, &corrected_arrival_delay_ms,
               &playout_delay_ms, &target_delay_ms);

  // Create an output file stream to Matlab script file.
  std::ofstream output(script_name);
  // The iterator is used to batch-output comma-separated values from vectors.
  std::ostream_iterator<float> output_iterator(output, ",");

  output << "send_time_s = [ ";
  std::copy(send_time_s.begin(), send_time_s.end(), output_iterator);
  output << "];" << std::endl;

  output << "arrival_delay_ms = [ ";
  std::copy(arrival_delay_ms.begin(), arrival_delay_ms.end(), output_iterator);
  output << "];" << std::endl;

  output << "corrected_arrival_delay_ms = [ ";
  std::copy(corrected_arrival_delay_ms.begin(),
            corrected_arrival_delay_ms.end(), output_iterator);
  output << "];" << std::endl;

  output << "playout_delay_ms = [ ";
  for (const auto& v : playout_delay_ms) {
    if (!v) {
      output << "nan, ";
    } else {
      output << *v << ", ";
    }
  }
  output << "];" << std::endl;

  output << "target_delay_ms = [ ";
  for (const auto& v : target_delay_ms) {
    if (!v) {
      output << "nan, ";
    } else {
      output << *v << ", ";
    }
  }
  output << "];" << std::endl;

  output << "h=plot(send_time_s, arrival_delay_ms, "
         << "send_time_s, target_delay_ms, 'g.', "
         << "send_time_s, playout_delay_ms);" << std::endl;
  output << "set(h(1),'color',0.75*[1 1 1]);" << std::endl;
  output << "set(h(2),'markersize',6);" << std::endl;
  output << "set(h(3),'linew',1.5);" << std::endl;
  output << "ax1=axis;" << std::endl;
  output << "axis tight" << std::endl;
  output << "ax2=axis;" << std::endl;
  output << "axis([ax2(1:3) ax1(4)])" << std::endl;
  output << "xlabel('send time [s]');" << std::endl;
  output << "ylabel('relative delay [ms]');" << std::endl;
  if (!ssrcs_.empty()) {
    auto ssrc_it = ssrcs_.cbegin();
    output << "title('SSRC: 0x" << std::hex << static_cast<int64_t>(*ssrc_it++);
    while (ssrc_it != ssrcs_.end()) {
      output << ", 0x" << std::hex << static_cast<int64_t>(*ssrc_it++);
    }
    output << std::dec;
    auto pt_it = payload_types_.cbegin();
    output << "; Payload Types: " << *pt_it++;
    while (pt_it != payload_types_.end()) {
      output << ", " << *pt_it++;
    }
    output << "');" << std::endl;
  }
}

}  // namespace test
}  // namespace webrtc
