/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/analyzer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "common_types.h"  // NOLINT(build/include)
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/audio_coding/neteq/tools/audio_sink.h"
#include "modules/audio_coding/neteq/tools/fake_decode_from_file.h"
#include "modules/audio_coding/neteq/tools/neteq_delay_analyzer.h"
#include "modules/audio_coding/neteq/tools/neteq_replacement_input.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/audio_coding/neteq/tools/resample_input_audio_file.h"
#include "modules/congestion_controller/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/bitrate_estimator.h"
#include "modules/congestion_controller/include/receive_side_congestion_controller.h"
#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "modules/include/module_common_types.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/rate_statistics.h"

#ifndef BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
#define BWE_TEST_LOGGING_COMPILE_TIME_ENABLE 0
#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE

namespace webrtc {
namespace plotting {

namespace {

void SortPacketFeedbackVector(std::vector<PacketFeedback>* vec) {
  auto pred = [](const PacketFeedback& packet_feedback) {
    return packet_feedback.arrival_time_ms == PacketFeedback::kNotReceived;
  };
  vec->erase(std::remove_if(vec->begin(), vec->end(), pred), vec->end());
  std::sort(vec->begin(), vec->end(), PacketFeedbackComparator());
}

std::string SsrcToString(uint32_t ssrc) {
  std::stringstream ss;
  ss << "SSRC " << ssrc;
  return ss.str();
}

// Checks whether an SSRC is contained in the list of desired SSRCs.
// Note that an empty SSRC list matches every SSRC.
bool MatchingSsrc(uint32_t ssrc, const std::vector<uint32_t>& desired_ssrc) {
  if (desired_ssrc.size() == 0)
    return true;
  return std::find(desired_ssrc.begin(), desired_ssrc.end(), ssrc) !=
         desired_ssrc.end();
}

double AbsSendTimeToMicroseconds(int64_t abs_send_time) {
  // The timestamp is a fixed point representation with 6 bits for seconds
  // and 18 bits for fractions of a second. Thus, we divide by 2^18 to get the
  // time in seconds and then multiply by 1000000 to convert to microseconds.
  static constexpr double kTimestampToMicroSec =
      1000000.0 / static_cast<double>(1ul << 18);
  return abs_send_time * kTimestampToMicroSec;
}

// Computes the difference |later| - |earlier| where |later| and |earlier|
// are counters that wrap at |modulus|. The difference is chosen to have the
// least absolute value. For example if |modulus| is 8, then the difference will
// be chosen in the range [-3, 4]. If |modulus| is 9, then the difference will
// be in [-4, 4].
int64_t WrappingDifference(uint32_t later, uint32_t earlier, int64_t modulus) {
  RTC_DCHECK_LE(1, modulus);
  RTC_DCHECK_LT(later, modulus);
  RTC_DCHECK_LT(earlier, modulus);
  int64_t difference =
      static_cast<int64_t>(later) - static_cast<int64_t>(earlier);
  int64_t max_difference = modulus / 2;
  int64_t min_difference = max_difference - modulus + 1;
  if (difference > max_difference) {
    difference -= modulus;
  }
  if (difference < min_difference) {
    difference += modulus;
  }
  if (difference > max_difference / 2 || difference < min_difference / 2) {
    RTC_LOG(LS_WARNING) << "Difference between" << later << " and " << earlier
                        << " expected to be in the range ("
                        << min_difference / 2 << "," << max_difference / 2
                        << ") but is " << difference
                        << ". Correct unwrapping is uncertain.";
  }
  return difference;
}

// Return default values for header extensions, to use on streams without stored
// mapping data. Currently this only applies to audio streams, since the mapping
// is not stored in the event log.
// TODO(ivoc): Remove this once this mapping is stored in the event log for
//             audio streams. Tracking bug: webrtc:6399
webrtc::RtpHeaderExtensionMap GetDefaultHeaderExtensionMap() {
  webrtc::RtpHeaderExtensionMap default_map;
  default_map.Register<AudioLevel>(webrtc::RtpExtension::kAudioLevelDefaultId);
  default_map.Register<TransmissionOffset>(
      webrtc::RtpExtension::kTimestampOffsetDefaultId);
  default_map.Register<AbsoluteSendTime>(
      webrtc::RtpExtension::kAbsSendTimeDefaultId);
  default_map.Register<VideoOrientation>(
      webrtc::RtpExtension::kVideoRotationDefaultId);
  default_map.Register<VideoContentTypeExtension>(
      webrtc::RtpExtension::kVideoContentTypeDefaultId);
  default_map.Register<VideoTimingExtension>(
      webrtc::RtpExtension::kVideoTimingDefaultId);
  default_map.Register<TransportSequenceNumber>(
      webrtc::RtpExtension::kTransportSequenceNumberDefaultId);
  default_map.Register<PlayoutDelayLimits>(
      webrtc::RtpExtension::kPlayoutDelayDefaultId);
  return default_map;
}

constexpr float kLeftMargin = 0.01f;
constexpr float kRightMargin = 0.02f;
constexpr float kBottomMargin = 0.02f;
constexpr float kTopMargin = 0.05f;

rtc::Optional<double> NetworkDelayDiff_AbsSendTime(
    const LoggedRtpPacket& old_packet,
    const LoggedRtpPacket& new_packet) {
  if (old_packet.header.extension.hasAbsoluteSendTime &&
      new_packet.header.extension.hasAbsoluteSendTime) {
    int64_t send_time_diff = WrappingDifference(
        new_packet.header.extension.absoluteSendTime,
        old_packet.header.extension.absoluteSendTime, 1ul << 24);
    int64_t recv_time_diff = new_packet.timestamp - old_packet.timestamp;
    double delay_change_us =
        recv_time_diff - AbsSendTimeToMicroseconds(send_time_diff);
    return delay_change_us / 1000;
  } else {
    return rtc::nullopt;
  }
}

rtc::Optional<double> NetworkDelayDiff_CaptureTime(
    const LoggedRtpPacket& old_packet,
    const LoggedRtpPacket& new_packet) {
  int64_t send_time_diff = WrappingDifference(
      new_packet.header.timestamp, old_packet.header.timestamp, 1ull << 32);
  int64_t recv_time_diff = new_packet.timestamp - old_packet.timestamp;

  const double kVideoSampleRate = 90000;
  // TODO(terelius): We treat all streams as video for now, even though
  // audio might be sampled at e.g. 16kHz, because it is really difficult to
  // figure out the true sampling rate of a stream. The effect is that the
  // delay will be scaled incorrectly for non-video streams.

  double delay_change =
      static_cast<double>(recv_time_diff) / 1000 -
      static_cast<double>(send_time_diff) / kVideoSampleRate * 1000;
  if (delay_change < -10000 || 10000 < delay_change) {
    RTC_LOG(LS_WARNING) << "Very large delay change. Timestamps correct?";
    RTC_LOG(LS_WARNING) << "Old capture time " << old_packet.header.timestamp
                        << ", received time " << old_packet.timestamp;
    RTC_LOG(LS_WARNING) << "New capture time " << new_packet.header.timestamp
                        << ", received time " << new_packet.timestamp;
    RTC_LOG(LS_WARNING) << "Receive time difference " << recv_time_diff << " = "
                        << static_cast<double>(recv_time_diff) / 1000000 << "s";
    RTC_LOG(LS_WARNING) << "Send time difference " << send_time_diff << " = "
                        << static_cast<double>(send_time_diff) /
                               kVideoSampleRate
                        << "s";
  }
  return delay_change;
}

// For each element in data, use |get_y()| to extract a y-coordinate and
// store the result in a TimeSeries.
template <typename DataType>
void ProcessPoints(
    rtc::FunctionView<rtc::Optional<float>(const DataType&)> get_y,
    const std::vector<DataType>& data,
    uint64_t begin_time,
    TimeSeries* result) {
  for (size_t i = 0; i < data.size(); i++) {
    float x = static_cast<float>(data[i].timestamp - begin_time) / 1000000;
    rtc::Optional<float> y = get_y(data[i]);
    if (y)
      result->points.emplace_back(x, *y);
  }
}

// For each pair of adjacent elements in |data|, use |get_y| to extract a
// y-coordinate and store the result in a TimeSeries. Note that the x-coordinate
// will be the time of the second element in the pair.
template <typename DataType, typename ResultType>
void ProcessPairs(
    rtc::FunctionView<rtc::Optional<ResultType>(const DataType&,
                                                const DataType&)> get_y,
    const std::vector<DataType>& data,
    uint64_t begin_time,
    TimeSeries* result) {
  for (size_t i = 1; i < data.size(); i++) {
    float x = static_cast<float>(data[i].timestamp - begin_time) / 1000000;
    rtc::Optional<ResultType> y = get_y(data[i - 1], data[i]);
    if (y)
      result->points.emplace_back(x, static_cast<float>(*y));
  }
}

// For each element in data, use |extract()| to extract a y-coordinate and
// store the result in a TimeSeries.
template <typename DataType, typename ResultType>
void AccumulatePoints(
    rtc::FunctionView<rtc::Optional<ResultType>(const DataType&)> extract,
    const std::vector<DataType>& data,
    uint64_t begin_time,
    TimeSeries* result) {
  ResultType sum = 0;
  for (size_t i = 0; i < data.size(); i++) {
    float x = static_cast<float>(data[i].timestamp - begin_time) / 1000000;
    rtc::Optional<ResultType> y = extract(data[i]);
    if (y) {
      sum += *y;
      result->points.emplace_back(x, static_cast<float>(sum));
    }
  }
}

// For each pair of adjacent elements in |data|, use |extract()| to extract a
// y-coordinate and store the result in a TimeSeries. Note that the x-coordinate
// will be the time of the second element in the pair.
template <typename DataType, typename ResultType>
void AccumulatePairs(
    rtc::FunctionView<rtc::Optional<ResultType>(const DataType&,
                                                const DataType&)> extract,
    const std::vector<DataType>& data,
    uint64_t begin_time,
    TimeSeries* result) {
  ResultType sum = 0;
  for (size_t i = 1; i < data.size(); i++) {
    float x = static_cast<float>(data[i].timestamp - begin_time) / 1000000;
    rtc::Optional<ResultType> y = extract(data[i - 1], data[i]);
    if (y)
      sum += *y;
    result->points.emplace_back(x, static_cast<float>(sum));
  }
}

// Calculates a moving average of |data| and stores the result in a TimeSeries.
// A data point is generated every |step| microseconds from |begin_time|
// to |end_time|. The value of each data point is the average of the data
// during the preceeding |window_duration_us| microseconds.
template <typename DataType, typename ResultType>
void MovingAverage(
    rtc::FunctionView<rtc::Optional<ResultType>(const DataType&)> extract,
    const std::vector<DataType>& data,
    uint64_t begin_time,
    uint64_t end_time,
    uint64_t window_duration_us,
    uint64_t step,
    webrtc::plotting::TimeSeries* result) {
  size_t window_index_begin = 0;
  size_t window_index_end = 0;
  ResultType sum_in_window = 0;

  for (uint64_t t = begin_time; t < end_time + step; t += step) {
    while (window_index_end < data.size() &&
           data[window_index_end].timestamp < t) {
      rtc::Optional<ResultType> value = extract(data[window_index_end]);
      if (value)
        sum_in_window += *value;
      ++window_index_end;
    }
    while (window_index_begin < data.size() &&
           data[window_index_begin].timestamp < t - window_duration_us) {
      rtc::Optional<ResultType> value = extract(data[window_index_begin]);
      if (value)
        sum_in_window -= *value;
      ++window_index_begin;
    }
    float window_duration_s = static_cast<float>(window_duration_us) / 1000000;
    float x = static_cast<float>(t - begin_time) / 1000000;
    float y = sum_in_window / window_duration_s;
    result->points.emplace_back(x, y);
  }
}

}  // namespace

EventLogAnalyzer::EventLogAnalyzer(const ParsedRtcEventLog& log)
    : parsed_log_(log), window_duration_(250000), step_(10000) {
  uint64_t first_timestamp = std::numeric_limits<uint64_t>::max();
  uint64_t last_timestamp = std::numeric_limits<uint64_t>::min();

  PacketDirection direction;
  uint8_t header[IP_PACKET_SIZE];
  size_t header_length;
  size_t total_length;

  uint8_t last_incoming_rtcp_packet[IP_PACKET_SIZE];
  uint8_t last_incoming_rtcp_packet_length = 0;

  // Make a default extension map for streams without configuration information.
  // TODO(ivoc): Once configuration of audio streams is stored in the event log,
  //             this can be removed. Tracking bug: webrtc:6399
  RtpHeaderExtensionMap default_extension_map = GetDefaultHeaderExtensionMap();

  rtc::Optional<uint64_t> last_log_start;

  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type != ParsedRtcEventLog::VIDEO_RECEIVER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::VIDEO_SENDER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::AUDIO_RECEIVER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::AUDIO_SENDER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::LOG_START &&
        event_type != ParsedRtcEventLog::LOG_END) {
      uint64_t timestamp = parsed_log_.GetTimestamp(i);
      first_timestamp = std::min(first_timestamp, timestamp);
      last_timestamp = std::max(last_timestamp, timestamp);
    }

    switch (parsed_log_.GetEventType(i)) {
      case ParsedRtcEventLog::VIDEO_RECEIVER_CONFIG_EVENT: {
        rtclog::StreamConfig config = parsed_log_.GetVideoReceiveConfig(i);
        StreamId stream(config.remote_ssrc, kIncomingPacket);
        video_ssrcs_.insert(stream);
        StreamId rtx_stream(config.rtx_ssrc, kIncomingPacket);
        video_ssrcs_.insert(rtx_stream);
        rtx_ssrcs_.insert(rtx_stream);
        break;
      }
      case ParsedRtcEventLog::VIDEO_SENDER_CONFIG_EVENT: {
        std::vector<rtclog::StreamConfig> configs =
            parsed_log_.GetVideoSendConfig(i);
        for (const auto& config : configs) {
          StreamId stream(config.local_ssrc, kOutgoingPacket);
          video_ssrcs_.insert(stream);
          StreamId rtx_stream(config.rtx_ssrc, kOutgoingPacket);
          video_ssrcs_.insert(rtx_stream);
          rtx_ssrcs_.insert(rtx_stream);
        }
        break;
      }
      case ParsedRtcEventLog::AUDIO_RECEIVER_CONFIG_EVENT: {
        rtclog::StreamConfig config = parsed_log_.GetAudioReceiveConfig(i);
        StreamId stream(config.remote_ssrc, kIncomingPacket);
        audio_ssrcs_.insert(stream);
        break;
      }
      case ParsedRtcEventLog::AUDIO_SENDER_CONFIG_EVENT: {
        rtclog::StreamConfig config = parsed_log_.GetAudioSendConfig(i);
        StreamId stream(config.local_ssrc, kOutgoingPacket);
        audio_ssrcs_.insert(stream);
        break;
      }
      case ParsedRtcEventLog::RTP_EVENT: {
        RtpHeaderExtensionMap* extension_map = parsed_log_.GetRtpHeader(
            i, &direction, header, &header_length, &total_length, nullptr);
        RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
        RTPHeader parsed_header;
        if (extension_map != nullptr) {
          rtp_parser.Parse(&parsed_header, extension_map);
        } else {
          // Use the default extension map.
          // TODO(ivoc): Once configuration of audio streams is stored in the
          //             event log, this can be removed.
          //             Tracking bug: webrtc:6399
          rtp_parser.Parse(&parsed_header, &default_extension_map);
        }
        uint64_t timestamp = parsed_log_.GetTimestamp(i);
        StreamId stream(parsed_header.ssrc, direction);
        rtp_packets_[stream].push_back(
            LoggedRtpPacket(timestamp, parsed_header, total_length));
        break;
      }
      case ParsedRtcEventLog::RTCP_EVENT: {
        uint8_t packet[IP_PACKET_SIZE];
        parsed_log_.GetRtcpPacket(i, &direction, packet, &total_length);
        // Currently incoming RTCP packets are logged twice, both for audio and
        // video. Only act on one of them. Compare against the previous parsed
        // incoming RTCP packet.
        if (direction == webrtc::kIncomingPacket) {
          RTC_CHECK_LE(total_length, IP_PACKET_SIZE);
          if (total_length == last_incoming_rtcp_packet_length &&
              memcmp(last_incoming_rtcp_packet, packet, total_length) == 0) {
            continue;
          } else {
            memcpy(last_incoming_rtcp_packet, packet, total_length);
            last_incoming_rtcp_packet_length = total_length;
          }
        }
        rtcp::CommonHeader header;
        const uint8_t* packet_end = packet + total_length;
        for (const uint8_t* block = packet; block < packet_end;
             block = header.NextPacket()) {
          RTC_CHECK(header.Parse(block, packet_end - block));
          if (header.type() == rtcp::TransportFeedback::kPacketType &&
              header.fmt() == rtcp::TransportFeedback::kFeedbackMessageType) {
            std::unique_ptr<rtcp::TransportFeedback> rtcp_packet(
                rtc::MakeUnique<rtcp::TransportFeedback>());
            if (rtcp_packet->Parse(header)) {
              uint32_t ssrc = rtcp_packet->sender_ssrc();
              StreamId stream(ssrc, direction);
              uint64_t timestamp = parsed_log_.GetTimestamp(i);
              rtcp_packets_[stream].push_back(LoggedRtcpPacket(
                  timestamp, kRtcpTransportFeedback, std::move(rtcp_packet)));
            }
          } else if (header.type() == rtcp::SenderReport::kPacketType) {
            std::unique_ptr<rtcp::SenderReport> rtcp_packet(
                rtc::MakeUnique<rtcp::SenderReport>());
            if (rtcp_packet->Parse(header)) {
              uint32_t ssrc = rtcp_packet->sender_ssrc();
              StreamId stream(ssrc, direction);
              uint64_t timestamp = parsed_log_.GetTimestamp(i);
              rtcp_packets_[stream].push_back(
                  LoggedRtcpPacket(timestamp, kRtcpSr, std::move(rtcp_packet)));
            }
          } else if (header.type() == rtcp::ReceiverReport::kPacketType) {
            std::unique_ptr<rtcp::ReceiverReport> rtcp_packet(
                rtc::MakeUnique<rtcp::ReceiverReport>());
            if (rtcp_packet->Parse(header)) {
              uint32_t ssrc = rtcp_packet->sender_ssrc();
              StreamId stream(ssrc, direction);
              uint64_t timestamp = parsed_log_.GetTimestamp(i);
              rtcp_packets_[stream].push_back(
                  LoggedRtcpPacket(timestamp, kRtcpRr, std::move(rtcp_packet)));
            }
          } else if (header.type() == rtcp::Remb::kPacketType &&
                     header.fmt() == rtcp::Remb::kFeedbackMessageType) {
            std::unique_ptr<rtcp::Remb> rtcp_packet(
                rtc::MakeUnique<rtcp::Remb>());
            if (rtcp_packet->Parse(header)) {
              uint32_t ssrc = rtcp_packet->sender_ssrc();
              StreamId stream(ssrc, direction);
              uint64_t timestamp = parsed_log_.GetTimestamp(i);
              rtcp_packets_[stream].push_back(LoggedRtcpPacket(
                  timestamp, kRtcpRemb, std::move(rtcp_packet)));
            }
          }
        }
        break;
      }
      case ParsedRtcEventLog::LOG_START: {
        if (last_log_start) {
          // A LOG_END event was missing. Use last_timestamp.
          RTC_DCHECK_GE(last_timestamp, *last_log_start);
          log_segments_.push_back(
            std::make_pair(*last_log_start, last_timestamp));
        }
        last_log_start = parsed_log_.GetTimestamp(i);
        break;
      }
      case ParsedRtcEventLog::LOG_END: {
        RTC_DCHECK(last_log_start);
        log_segments_.push_back(
            std::make_pair(*last_log_start, parsed_log_.GetTimestamp(i)));
        last_log_start.reset();
        break;
      }
      case ParsedRtcEventLog::AUDIO_PLAYOUT_EVENT: {
        uint32_t this_ssrc;
        parsed_log_.GetAudioPlayout(i, &this_ssrc);
        audio_playout_events_[this_ssrc].push_back(parsed_log_.GetTimestamp(i));
        break;
      }
      case ParsedRtcEventLog::LOSS_BASED_BWE_UPDATE: {
        LossBasedBweUpdate bwe_update;
        bwe_update.timestamp = parsed_log_.GetTimestamp(i);
        parsed_log_.GetLossBasedBweUpdate(i, &bwe_update.new_bitrate,
                                          &bwe_update.fraction_loss,
                                          &bwe_update.expected_packets);
        bwe_loss_updates_.push_back(bwe_update);
        break;
      }
      case ParsedRtcEventLog::DELAY_BASED_BWE_UPDATE: {
        bwe_delay_updates_.push_back(parsed_log_.GetDelayBasedBweUpdate(i));
        break;
      }
      case ParsedRtcEventLog::AUDIO_NETWORK_ADAPTATION_EVENT: {
        AudioNetworkAdaptationEvent ana_event;
        ana_event.timestamp = parsed_log_.GetTimestamp(i);
        parsed_log_.GetAudioNetworkAdaptation(i, &ana_event.config);
        audio_network_adaptation_events_.push_back(ana_event);
        break;
      }
      case ParsedRtcEventLog::BWE_PROBE_CLUSTER_CREATED_EVENT: {
        bwe_probe_cluster_created_events_.push_back(
            parsed_log_.GetBweProbeClusterCreated(i));
        break;
      }
      case ParsedRtcEventLog::BWE_PROBE_RESULT_EVENT: {
        bwe_probe_result_events_.push_back(parsed_log_.GetBweProbeResult(i));
        break;
      }
      case ParsedRtcEventLog::UNKNOWN_EVENT: {
        break;
      }
    }
  }

  if (last_timestamp < first_timestamp) {
    // No useful events in the log.
    first_timestamp = last_timestamp = 0;
  }
  begin_time_ = first_timestamp;
  end_time_ = last_timestamp;
  call_duration_s_ = static_cast<float>(end_time_ - begin_time_) / 1000000;
  if (last_log_start) {
    // The log was missing the last LOG_END event. Fake it.
    log_segments_.push_back(std::make_pair(*last_log_start, end_time_));
  }
}

class BitrateObserver : public SendSideCongestionController::Observer,
                        public RemoteBitrateObserver {
 public:
  BitrateObserver() : last_bitrate_bps_(0), bitrate_updated_(false) {}

  void OnNetworkChanged(uint32_t bitrate_bps,
                        uint8_t fraction_loss,
                        int64_t rtt_ms,
                        int64_t probing_interval_ms) override {
    last_bitrate_bps_ = bitrate_bps;
    bitrate_updated_ = true;
  }

  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                               uint32_t bitrate) override {}

  uint32_t last_bitrate_bps() const { return last_bitrate_bps_; }
  bool GetAndResetBitrateUpdated() {
    bool bitrate_updated = bitrate_updated_;
    bitrate_updated_ = false;
    return bitrate_updated;
  }

 private:
  uint32_t last_bitrate_bps_;
  bool bitrate_updated_;
};

bool EventLogAnalyzer::IsRtxSsrc(StreamId stream_id) const {
  return rtx_ssrcs_.count(stream_id) == 1;
}

bool EventLogAnalyzer::IsVideoSsrc(StreamId stream_id) const {
  return video_ssrcs_.count(stream_id) == 1;
}

bool EventLogAnalyzer::IsAudioSsrc(StreamId stream_id) const {
  return audio_ssrcs_.count(stream_id) == 1;
}

std::string EventLogAnalyzer::GetStreamName(StreamId stream_id) const {
  std::stringstream name;
  if (IsAudioSsrc(stream_id)) {
    name << "Audio ";
  } else if (IsVideoSsrc(stream_id)) {
    name << "Video ";
  } else {
    name << "Unknown ";
  }
  if (IsRtxSsrc(stream_id))
    name << "RTX ";
  if (stream_id.GetDirection() == kIncomingPacket) {
    name << "(In) ";
  } else {
    name << "(Out) ";
  }
  name << SsrcToString(stream_id.GetSsrc());
  return name.str();
}

// This is much more reliable for outgoing streams than for incoming streams.
rtc::Optional<uint32_t> EventLogAnalyzer::EstimateRtpClockFrequency(
    const std::vector<LoggedRtpPacket>& packets) const {
  RTC_CHECK(packets.size() >= 2);
  uint64_t end_time_us = log_segments_.empty()
                             ? std::numeric_limits<uint64_t>::max()
                             : log_segments_.front().second;
  SeqNumUnwrapper<uint32_t> unwrapper;
  uint64_t first_rtp_timestamp = unwrapper.Unwrap(packets[0].header.timestamp);
  uint64_t first_log_timestamp = packets[0].timestamp;
  uint64_t last_rtp_timestamp = first_rtp_timestamp;
  uint64_t last_log_timestamp = first_log_timestamp;
  for (size_t i = 1; i < packets.size(); i++) {
    if (packets[i].timestamp > end_time_us)
      break;
    last_rtp_timestamp = unwrapper.Unwrap(packets[i].header.timestamp);
    last_log_timestamp = packets[i].timestamp;
  }
  if (last_log_timestamp - first_log_timestamp < 1000000) {
    RTC_LOG(LS_WARNING)
        << "Failed to estimate RTP clock frequency: Stream too short. ("
        << packets.size() << " packets, "
        << last_log_timestamp - first_log_timestamp << " us)";
    return rtc::nullopt;
  }
  double duration =
      static_cast<double>(last_log_timestamp - first_log_timestamp) / 1000000;
  double estimated_frequency =
      (last_rtp_timestamp - first_rtp_timestamp) / duration;
  for (uint32_t f : {8000, 16000, 32000, 48000, 90000}) {
    if (std::fabs(estimated_frequency - f) < 0.05 * f) {
      return f;
    }
  }
  RTC_LOG(LS_WARNING) << "Failed to estimate RTP clock frequency: Estimate "
                      << estimated_frequency
                      << "not close to any stardard RTP frequency.";
  return rtc::nullopt;
}

void EventLogAnalyzer::CreatePacketGraph(PacketDirection desired_direction,
                                         Plot* plot) {
  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != desired_direction ||
        !MatchingSsrc(stream_id.GetSsrc(), desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName(stream_id), LineStyle::kBar);
    ProcessPoints<LoggedRtpPacket>(
        [](const LoggedRtpPacket& packet) {
          return rtc::Optional<float>(packet.total_length);
        },
        packet_stream, begin_time_, &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Packet size (bytes)", kBottomMargin,
                          kTopMargin);
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->SetTitle("Incoming RTP packets");
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->SetTitle("Outgoing RTP packets");
  }
}

template <typename T>
void EventLogAnalyzer::CreateAccumulatedPacketsTimeSeries(
    PacketDirection desired_direction,
    Plot* plot,
    const std::map<StreamId, std::vector<T>>& packets,
    const std::string& label_prefix) {
  for (auto& kv : packets) {
    StreamId stream_id = kv.first;
    const std::vector<T>& packet_stream = kv.second;
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != desired_direction ||
        !MatchingSsrc(stream_id.GetSsrc(), desired_ssrc_)) {
      continue;
    }

    std::string label = label_prefix + " " + GetStreamName(stream_id);
    TimeSeries time_series(label, LineStyle::kStep);
    for (size_t i = 0; i < packet_stream.size(); i++) {
      float x = static_cast<float>(packet_stream[i].timestamp - begin_time_) /
                1000000;
      time_series.points.emplace_back(x, i + 1);
    }

    plot->AppendTimeSeries(std::move(time_series));
  }
}

void EventLogAnalyzer::CreateAccumulatedPacketsGraph(
    PacketDirection desired_direction,
    Plot* plot) {
  CreateAccumulatedPacketsTimeSeries(desired_direction, plot, rtp_packets_,
                                     "RTP");
  CreateAccumulatedPacketsTimeSeries(desired_direction, plot, rtcp_packets_,
                                     "RTCP");

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Received Packets", kBottomMargin, kTopMargin);
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->SetTitle("Accumulated Incoming RTP/RTCP packets");
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->SetTitle("Accumulated Outgoing RTP/RTCP packets");
  }
}

// For each SSRC, plot the time between the consecutive playouts.
void EventLogAnalyzer::CreatePlayoutGraph(Plot* plot) {
  std::map<uint32_t, TimeSeries> time_series;
  std::map<uint32_t, uint64_t> last_playout;

  uint32_t ssrc;

  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::AUDIO_PLAYOUT_EVENT) {
      parsed_log_.GetAudioPlayout(i, &ssrc);
      uint64_t timestamp = parsed_log_.GetTimestamp(i);
      if (MatchingSsrc(ssrc, desired_ssrc_)) {
        float x = static_cast<float>(timestamp - begin_time_) / 1000000;
        float y = static_cast<float>(timestamp - last_playout[ssrc]) / 1000;
        if (time_series[ssrc].points.size() == 0) {
          // There were no previusly logged playout for this SSRC.
          // Generate a point, but place it on the x-axis.
          y = 0;
        }
        time_series[ssrc].points.push_back(TimeSeriesPoint(x, y));
        last_playout[ssrc] = timestamp;
      }
    }
  }

  // Set labels and put in graph.
  for (auto& kv : time_series) {
    kv.second.label = SsrcToString(kv.first);
    kv.second.line_style = LineStyle::kBar;
    plot->AppendTimeSeries(std::move(kv.second));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Time since last playout (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Audio playout");
}

// For audio SSRCs, plot the audio level.
void EventLogAnalyzer::CreateAudioLevelGraph(Plot* plot) {
  std::map<StreamId, TimeSeries> time_series;

  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    // TODO(ivoc): When audio send/receive configs are stored in the event
    //             log, a check should be added here to only process audio
    //             streams. Tracking bug: webrtc:6399
    for (auto& packet : packet_stream) {
      if (packet.header.extension.hasAudioLevel) {
        float x = static_cast<float>(packet.timestamp - begin_time_) / 1000000;
        // The audio level is stored in -dBov (so e.g. -10 dBov is stored as 10)
        // Here we convert it to dBov.
        float y = static_cast<float>(-packet.header.extension.audioLevel);
        time_series[stream_id].points.emplace_back(TimeSeriesPoint(x, y));
      }
    }
  }

  for (auto& series : time_series) {
    series.second.label = GetStreamName(series.first);
    series.second.line_style = LineStyle::kLine;
    plot->AppendTimeSeries(std::move(series.second));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetYAxis(-127, 0, "Audio level (dBov)", kBottomMargin,
                 kTopMargin);
  plot->SetTitle("Audio level");
}

// For each SSRC, plot the time between the consecutive playouts.
void EventLogAnalyzer::CreateSequenceNumberGraph(Plot* plot) {
  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != kIncomingPacket ||
        !MatchingSsrc(stream_id.GetSsrc(), desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName(stream_id), LineStyle::kBar);
    ProcessPairs<LoggedRtpPacket, float>(
        [](const LoggedRtpPacket& old_packet,
           const LoggedRtpPacket& new_packet) {
          int64_t diff =
              WrappingDifference(new_packet.header.sequenceNumber,
                                 old_packet.header.sequenceNumber, 1ul << 16);
          return diff;
        },
        packet_stream, begin_time_, &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Difference since last packet", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Sequence number");
}

void EventLogAnalyzer::CreateIncomingPacketLossGraph(Plot* plot) {
  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != kIncomingPacket ||
        !MatchingSsrc(stream_id.GetSsrc(), desired_ssrc_) ||
        packet_stream.size() == 0) {
      continue;
    }

    TimeSeries time_series(GetStreamName(stream_id), LineStyle::kLine,
                           PointStyle::kHighlight);
    const uint64_t kWindowUs = 1000000;
    const uint64_t kStep = 1000000;
    SequenceNumberUnwrapper unwrapper_;
    SequenceNumberUnwrapper prior_unwrapper_;
    size_t window_index_begin = 0;
    size_t window_index_end = 0;
    int64_t highest_seq_number =
        unwrapper_.Unwrap(packet_stream[0].header.sequenceNumber) - 1;
    int64_t highest_prior_seq_number =
        prior_unwrapper_.Unwrap(packet_stream[0].header.sequenceNumber) - 1;

    for (uint64_t t = begin_time_; t < end_time_ + kStep; t += kStep) {
      while (window_index_end < packet_stream.size() &&
             packet_stream[window_index_end].timestamp < t) {
        int64_t sequence_number = unwrapper_.Unwrap(
            packet_stream[window_index_end].header.sequenceNumber);
        highest_seq_number = std::max(highest_seq_number, sequence_number);
        ++window_index_end;
      }
      while (window_index_begin < packet_stream.size() &&
             packet_stream[window_index_begin].timestamp < t - kWindowUs) {
        int64_t sequence_number = prior_unwrapper_.Unwrap(
            packet_stream[window_index_begin].header.sequenceNumber);
        highest_prior_seq_number =
            std::max(highest_prior_seq_number, sequence_number);
        ++window_index_begin;
      }
      float x = static_cast<float>(t - begin_time_) / 1000000;
      int64_t expected_packets = highest_seq_number - highest_prior_seq_number;
      if (expected_packets > 0) {
        int64_t received_packets = window_index_end - window_index_begin;
        int64_t lost_packets = expected_packets - received_packets;
        float y = static_cast<float>(lost_packets) / expected_packets * 100;
        time_series.points.emplace_back(x, y);
      }
    }
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Estimated loss rate (%)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Estimated incoming loss rate");
}

void EventLogAnalyzer::CreateIncomingDelayDeltaGraph(Plot* plot) {
  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != kIncomingPacket ||
        !MatchingSsrc(stream_id.GetSsrc(), desired_ssrc_) ||
        IsAudioSsrc(stream_id) || !IsVideoSsrc(stream_id) ||
        IsRtxSsrc(stream_id)) {
      continue;
    }

    TimeSeries capture_time_data(GetStreamName(stream_id) + " capture-time",
                                 LineStyle::kBar);
    ProcessPairs<LoggedRtpPacket, double>(NetworkDelayDiff_CaptureTime,
                                          packet_stream, begin_time_,
                                          &capture_time_data);
    plot->AppendTimeSeries(std::move(capture_time_data));

    TimeSeries send_time_data(GetStreamName(stream_id) + " abs-send-time",
                              LineStyle::kBar);
    ProcessPairs<LoggedRtpPacket, double>(NetworkDelayDiff_AbsSendTime,
                                          packet_stream, begin_time_,
                                          &send_time_data);
    plot->AppendTimeSeries(std::move(send_time_data));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Latency change (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Network latency difference between consecutive packets");
}

void EventLogAnalyzer::CreateIncomingDelayGraph(Plot* plot) {
  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != kIncomingPacket ||
        !MatchingSsrc(stream_id.GetSsrc(), desired_ssrc_) ||
        IsAudioSsrc(stream_id) || !IsVideoSsrc(stream_id) ||
        IsRtxSsrc(stream_id)) {
      continue;
    }

    TimeSeries capture_time_data(GetStreamName(stream_id) + " capture-time",
                                 LineStyle::kLine);
    AccumulatePairs<LoggedRtpPacket, double>(NetworkDelayDiff_CaptureTime,
                                             packet_stream, begin_time_,
                                             &capture_time_data);
    plot->AppendTimeSeries(std::move(capture_time_data));

    TimeSeries send_time_data(GetStreamName(stream_id) + " abs-send-time",
                              LineStyle::kLine);
    AccumulatePairs<LoggedRtpPacket, double>(NetworkDelayDiff_AbsSendTime,
                                             packet_stream, begin_time_,
                                             &send_time_data);
    plot->AppendTimeSeries(std::move(send_time_data));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Latency change (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Network latency (relative to first packet)");
}

// Plot the fraction of packets lost (as perceived by the loss-based BWE).
void EventLogAnalyzer::CreateFractionLossGraph(Plot* plot) {
  TimeSeries time_series("Fraction lost", LineStyle::kLine,
                         PointStyle::kHighlight);
  for (auto& bwe_update : bwe_loss_updates_) {
    float x = static_cast<float>(bwe_update.timestamp - begin_time_) / 1000000;
    float y = static_cast<float>(bwe_update.fraction_loss) / 255 * 100;
    time_series.points.emplace_back(x, y);
  }

  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Percent lost packets", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Reported packet loss");
}

// Plot the total bandwidth used by all RTP streams.
void EventLogAnalyzer::CreateTotalBitrateGraph(
    PacketDirection desired_direction,
    Plot* plot,
    bool show_detector_state) {
  struct TimestampSize {
    TimestampSize(uint64_t t, size_t s) : timestamp(t), size(s) {}
    uint64_t timestamp;
    size_t size;
  };
  std::vector<TimestampSize> packets;

  PacketDirection direction;
  size_t total_length;

  // Extract timestamps and sizes for the relevant packets.
  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::RTP_EVENT) {
      parsed_log_.GetRtpHeader(i, &direction, nullptr, nullptr, &total_length,
                               nullptr);
      if (direction == desired_direction) {
        uint64_t timestamp = parsed_log_.GetTimestamp(i);
        packets.push_back(TimestampSize(timestamp, total_length));
      }
    }
  }

  size_t window_index_begin = 0;
  size_t window_index_end = 0;
  size_t bytes_in_window = 0;

  // Calculate a moving average of the bitrate and store in a TimeSeries.
  TimeSeries bitrate_series("Bitrate", LineStyle::kLine);
  for (uint64_t time = begin_time_; time < end_time_ + step_; time += step_) {
    while (window_index_end < packets.size() &&
           packets[window_index_end].timestamp < time) {
      bytes_in_window += packets[window_index_end].size;
      ++window_index_end;
    }
    while (window_index_begin < packets.size() &&
           packets[window_index_begin].timestamp < time - window_duration_) {
      RTC_DCHECK_LE(packets[window_index_begin].size, bytes_in_window);
      bytes_in_window -= packets[window_index_begin].size;
      ++window_index_begin;
    }
    float window_duration_in_seconds =
        static_cast<float>(window_duration_) / 1000000;
    float x = static_cast<float>(time - begin_time_) / 1000000;
    float y = bytes_in_window * 8 / window_duration_in_seconds / 1000;
    bitrate_series.points.emplace_back(x, y);
  }
  plot->AppendTimeSeries(std::move(bitrate_series));

  // Overlay the send-side bandwidth estimate over the outgoing bitrate.
  if (desired_direction == kOutgoingPacket) {
    TimeSeries loss_series("Loss-based estimate", LineStyle::kStep);
    for (auto& loss_update : bwe_loss_updates_) {
      float x =
          static_cast<float>(loss_update.timestamp - begin_time_) / 1000000;
      float y = static_cast<float>(loss_update.new_bitrate) / 1000;
      loss_series.points.emplace_back(x, y);
    }

    TimeSeries delay_series("Delay-based estimate", LineStyle::kStep);
    IntervalSeries overusing_series("Overusing", "#ff8e82",
                                    IntervalSeries::kHorizontal);
    IntervalSeries underusing_series("Underusing", "#5092fc",
                                     IntervalSeries::kHorizontal);
    IntervalSeries normal_series("Normal", "#c4ffc4",
                                 IntervalSeries::kHorizontal);
    IntervalSeries* last_series = &normal_series;
    double last_detector_switch = 0.0;

    BandwidthUsage last_detector_state = BandwidthUsage::kBwNormal;

    for (auto& delay_update : bwe_delay_updates_) {
      float x =
          static_cast<float>(delay_update.timestamp - begin_time_) / 1000000;
      float y = static_cast<float>(delay_update.bitrate_bps) / 1000;

      if (last_detector_state != delay_update.detector_state) {
        last_series->intervals.emplace_back(last_detector_switch, x);
        last_detector_state = delay_update.detector_state;
        last_detector_switch = x;

        switch (delay_update.detector_state) {
          case BandwidthUsage::kBwNormal:
            last_series = &normal_series;
            break;
          case BandwidthUsage::kBwUnderusing:
            last_series = &underusing_series;
            break;
          case BandwidthUsage::kBwOverusing:
            last_series = &overusing_series;
            break;
          case BandwidthUsage::kLast:
            RTC_NOTREACHED();
        }
      }

      delay_series.points.emplace_back(x, y);
    }

    RTC_CHECK(last_series);
    last_series->intervals.emplace_back(last_detector_switch, end_time_);

    TimeSeries created_series("Probe cluster created.", LineStyle::kNone,
                              PointStyle::kHighlight);
    for (auto& cluster : bwe_probe_cluster_created_events_) {
      float x = static_cast<float>(cluster.timestamp - begin_time_) / 1000000;
      float y = static_cast<float>(cluster.bitrate_bps) / 1000;
      created_series.points.emplace_back(x, y);
    }

    TimeSeries result_series("Probing results.", LineStyle::kNone,
                             PointStyle::kHighlight);
    for (auto& result : bwe_probe_result_events_) {
      if (result.bitrate_bps) {
        float x = static_cast<float>(result.timestamp - begin_time_) / 1000000;
        float y = static_cast<float>(*result.bitrate_bps) / 1000;
        result_series.points.emplace_back(x, y);
      }
    }

    if (show_detector_state) {
      plot->AppendIntervalSeries(std::move(overusing_series));
      plot->AppendIntervalSeries(std::move(underusing_series));
      plot->AppendIntervalSeries(std::move(normal_series));
    }

    plot->AppendTimeSeries(std::move(loss_series));
    plot->AppendTimeSeries(std::move(delay_series));
    plot->AppendTimeSeries(std::move(created_series));
    plot->AppendTimeSeries(std::move(result_series));
  }

  // Overlay the incoming REMB over the outgoing bitrate
  // and outgoing REMB over incoming bitrate.
  PacketDirection remb_direction =
      desired_direction == kOutgoingPacket ? kIncomingPacket : kOutgoingPacket;
  TimeSeries remb_series("Remb", LineStyle::kStep);
  std::multimap<uint64_t, const LoggedRtcpPacket*> remb_packets;
  for (const auto& kv : rtcp_packets_) {
    if (kv.first.GetDirection() == remb_direction) {
      for (const LoggedRtcpPacket& rtcp_packet : kv.second) {
        if (rtcp_packet.type == kRtcpRemb) {
          remb_packets.insert(
              std::make_pair(rtcp_packet.timestamp, &rtcp_packet));
        }
      }
    }
  }

  for (const auto& kv : remb_packets) {
    const LoggedRtcpPacket* const rtcp = kv.second;
    const rtcp::Remb* const remb = static_cast<rtcp::Remb*>(rtcp->packet.get());
    float x = static_cast<float>(rtcp->timestamp - begin_time_) / 1000000;
    float y = static_cast<float>(remb->bitrate_bps()) / 1000;
    remb_series.points.emplace_back(x, y);
  }
  plot->AppendTimeSeriesIfNotEmpty(std::move(remb_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->SetTitle("Incoming RTP bitrate");
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->SetTitle("Outgoing RTP bitrate");
  }
}

// For each SSRC, plot the bandwidth used by that stream.
void EventLogAnalyzer::CreateStreamBitrateGraph(
    PacketDirection desired_direction,
    Plot* plot) {
  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != desired_direction ||
        !MatchingSsrc(stream_id.GetSsrc(), desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName(stream_id), LineStyle::kLine);
    MovingAverage<LoggedRtpPacket, double>(
        [](const LoggedRtpPacket& packet) {
          return packet.total_length * 8.0 / 1000.0;
        },
        packet_stream, begin_time_, end_time_, window_duration_, step_,
        &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->SetTitle("Incoming bitrate per stream");
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->SetTitle("Outgoing bitrate per stream");
  }
}

void EventLogAnalyzer::CreateSendSideBweSimulationGraph(Plot* plot) {
  std::multimap<uint64_t, const LoggedRtpPacket*> outgoing_rtp;
  std::multimap<uint64_t, const LoggedRtcpPacket*> incoming_rtcp;

  for (const auto& kv : rtp_packets_) {
    if (kv.first.GetDirection() == PacketDirection::kOutgoingPacket) {
      for (const LoggedRtpPacket& rtp_packet : kv.second)
        outgoing_rtp.insert(std::make_pair(rtp_packet.timestamp, &rtp_packet));
    }
  }

  for (const auto& kv : rtcp_packets_) {
    if (kv.first.GetDirection() == PacketDirection::kIncomingPacket) {
      for (const LoggedRtcpPacket& rtcp_packet : kv.second)
        incoming_rtcp.insert(
            std::make_pair(rtcp_packet.timestamp, &rtcp_packet));
    }
  }

  SimulatedClock clock(0);
  BitrateObserver observer;
  RtcEventLogNullImpl null_event_log;
  PacketRouter packet_router;
  PacedSender pacer(&clock, &packet_router, &null_event_log);
  SendSideCongestionController cc(&clock, &observer, &null_event_log, &pacer);
  // TODO(holmer): Log the call config and use that here instead.
  static const uint32_t kDefaultStartBitrateBps = 300000;
  cc.SetBweBitrates(0, kDefaultStartBitrateBps, -1);

  TimeSeries time_series("Delay-based estimate", LineStyle::kStep,
                         PointStyle::kHighlight);
  TimeSeries acked_time_series("Acked bitrate", LineStyle::kLine,
                               PointStyle::kHighlight);
  TimeSeries acked_estimate_time_series(
      "Acked bitrate estimate", LineStyle::kLine, PointStyle::kHighlight);

  auto rtp_iterator = outgoing_rtp.begin();
  auto rtcp_iterator = incoming_rtcp.begin();

  auto NextRtpTime = [&]() {
    if (rtp_iterator != outgoing_rtp.end())
      return static_cast<int64_t>(rtp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  auto NextRtcpTime = [&]() {
    if (rtcp_iterator != incoming_rtcp.end())
      return static_cast<int64_t>(rtcp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  auto NextProcessTime = [&]() {
    if (rtcp_iterator != incoming_rtcp.end() ||
        rtp_iterator != outgoing_rtp.end()) {
      return clock.TimeInMicroseconds() +
             std::max<int64_t>(cc.TimeUntilNextProcess() * 1000, 0);
    }
    return std::numeric_limits<int64_t>::max();
  };

  RateStatistics acked_bitrate(250, 8000);
#if !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
  // The event_log_visualizer should normally not be compiled with
  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE since the normal plots won't work.
  // However, compiling with BWE_TEST_LOGGING, runnning with --plot_sendside_bwe
  // and piping the output to plot_dynamics.py can be used as a hack to get the
  // internal state of various BWE components. In this case, it is important
  // we don't instantiate the AcknowledgedBitrateEstimator both here and in
  // SendSideCongestionController since that would lead to duplicate outputs.
  AcknowledgedBitrateEstimator acknowledged_bitrate_estimator(
      rtc::MakeUnique<BitrateEstimator>());
#endif  // !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
  int64_t time_us = std::min(NextRtpTime(), NextRtcpTime());
  int64_t last_update_us = 0;
  while (time_us != std::numeric_limits<int64_t>::max()) {
    clock.AdvanceTimeMicroseconds(time_us - clock.TimeInMicroseconds());
    if (clock.TimeInMicroseconds() >= NextRtcpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtcpTime());
      const LoggedRtcpPacket& rtcp = *rtcp_iterator->second;
      if (rtcp.type == kRtcpTransportFeedback) {
        cc.OnTransportFeedback(
            *static_cast<rtcp::TransportFeedback*>(rtcp.packet.get()));
        std::vector<PacketFeedback> feedback = cc.GetTransportFeedbackVector();
        SortPacketFeedbackVector(&feedback);
        rtc::Optional<uint32_t> bitrate_bps;
        if (!feedback.empty()) {
#if !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
          acknowledged_bitrate_estimator.IncomingPacketFeedbackVector(feedback);
#endif  // !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
          for (const PacketFeedback& packet : feedback)
            acked_bitrate.Update(packet.payload_size, packet.arrival_time_ms);
          bitrate_bps = acked_bitrate.Rate(feedback.back().arrival_time_ms);
        }
        float x = static_cast<float>(clock.TimeInMicroseconds() - begin_time_) /
                  1000000;
        float y = bitrate_bps.value_or(0) / 1000;
        acked_time_series.points.emplace_back(x, y);
#if !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
        y = acknowledged_bitrate_estimator.bitrate_bps().value_or(0) / 1000;
        acked_estimate_time_series.points.emplace_back(x, y);
#endif  // !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
      }
      ++rtcp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextRtpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtpTime());
      const LoggedRtpPacket& rtp = *rtp_iterator->second;
      if (rtp.header.extension.hasTransportSequenceNumber) {
        RTC_DCHECK(rtp.header.extension.hasTransportSequenceNumber);
        cc.AddPacket(rtp.header.ssrc,
                     rtp.header.extension.transportSequenceNumber,
                     rtp.total_length, PacedPacketInfo());
        rtc::SentPacket sent_packet(
            rtp.header.extension.transportSequenceNumber, rtp.timestamp / 1000);
        cc.OnSentPacket(sent_packet);
      }
      ++rtp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextProcessTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextProcessTime());
      cc.Process();
    }
    if (observer.GetAndResetBitrateUpdated() ||
        time_us - last_update_us >= 1e6) {
      uint32_t y = observer.last_bitrate_bps() / 1000;
      float x = static_cast<float>(clock.TimeInMicroseconds() - begin_time_) /
                1000000;
      time_series.points.emplace_back(x, y);
      last_update_us = time_us;
    }
    time_us = std::min({NextRtpTime(), NextRtcpTime(), NextProcessTime()});
  }
  // Add the data set to the plot.
  plot->AppendTimeSeries(std::move(time_series));
  plot->AppendTimeSeries(std::move(acked_time_series));
  plot->AppendTimeSeriesIfNotEmpty(std::move(acked_estimate_time_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Simulated send-side BWE behavior");
}

void EventLogAnalyzer::CreateReceiveSideBweSimulationGraph(Plot* plot) {
  class RembInterceptingPacketRouter : public PacketRouter {
   public:
    void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                 uint32_t bitrate_bps) override {
      last_bitrate_bps_ = bitrate_bps;
      bitrate_updated_ = true;
      PacketRouter::OnReceiveBitrateChanged(ssrcs, bitrate_bps);
    }
    uint32_t last_bitrate_bps() const { return last_bitrate_bps_; }
    bool GetAndResetBitrateUpdated() {
      bool bitrate_updated = bitrate_updated_;
      bitrate_updated_ = false;
      return bitrate_updated;
    }

   private:
    uint32_t last_bitrate_bps_;
    bool bitrate_updated_;
  };

  std::multimap<uint64_t, const LoggedRtpPacket*> incoming_rtp;

  for (const auto& kv : rtp_packets_) {
    if (kv.first.GetDirection() == PacketDirection::kIncomingPacket &&
        IsVideoSsrc(kv.first)) {
      for (const LoggedRtpPacket& rtp_packet : kv.second)
        incoming_rtp.insert(std::make_pair(rtp_packet.timestamp, &rtp_packet));
    }
  }

  SimulatedClock clock(0);
  RembInterceptingPacketRouter packet_router;
  // TODO(terelius): The PacketRrouter is the used as the RemoteBitrateObserver.
  // Is this intentional?
  ReceiveSideCongestionController rscc(&clock, &packet_router);
  // TODO(holmer): Log the call config and use that here instead.
  // static const uint32_t kDefaultStartBitrateBps = 300000;
  // rscc.SetBweBitrates(0, kDefaultStartBitrateBps, -1);

  TimeSeries time_series("Receive side estimate", LineStyle::kLine,
                         PointStyle::kHighlight);
  TimeSeries acked_time_series("Received bitrate", LineStyle::kLine);

  RateStatistics acked_bitrate(250, 8000);
  int64_t last_update_us = 0;
  for (const auto& kv : incoming_rtp) {
    const LoggedRtpPacket& packet = *kv.second;
    int64_t arrival_time_ms = packet.timestamp / 1000;
    size_t payload = packet.total_length; /*Should subtract header?*/
    clock.AdvanceTimeMicroseconds(packet.timestamp -
                                  clock.TimeInMicroseconds());
    rscc.OnReceivedPacket(arrival_time_ms, payload, packet.header);
    acked_bitrate.Update(payload, arrival_time_ms);
    rtc::Optional<uint32_t> bitrate_bps = acked_bitrate.Rate(arrival_time_ms);
    if (bitrate_bps) {
      uint32_t y = *bitrate_bps / 1000;
      float x = static_cast<float>(clock.TimeInMicroseconds() - begin_time_) /
                1000000;
      acked_time_series.points.emplace_back(x, y);
    }
    if (packet_router.GetAndResetBitrateUpdated() ||
        clock.TimeInMicroseconds() - last_update_us >= 1e6) {
      uint32_t y = packet_router.last_bitrate_bps() / 1000;
      float x = static_cast<float>(clock.TimeInMicroseconds() - begin_time_) /
                1000000;
      time_series.points.emplace_back(x, y);
      last_update_us = clock.TimeInMicroseconds();
    }
  }
  // Add the data set to the plot.
  plot->AppendTimeSeries(std::move(time_series));
  plot->AppendTimeSeries(std::move(acked_time_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Simulated receive-side BWE behavior");
}

void EventLogAnalyzer::CreateNetworkDelayFeedbackGraph(Plot* plot) {
  std::multimap<uint64_t, const LoggedRtpPacket*> outgoing_rtp;
  std::multimap<uint64_t, const LoggedRtcpPacket*> incoming_rtcp;

  for (const auto& kv : rtp_packets_) {
    if (kv.first.GetDirection() == PacketDirection::kOutgoingPacket) {
      for (const LoggedRtpPacket& rtp_packet : kv.second)
        outgoing_rtp.insert(std::make_pair(rtp_packet.timestamp, &rtp_packet));
    }
  }

  for (const auto& kv : rtcp_packets_) {
    if (kv.first.GetDirection() == PacketDirection::kIncomingPacket) {
      for (const LoggedRtcpPacket& rtcp_packet : kv.second)
        incoming_rtcp.insert(
            std::make_pair(rtcp_packet.timestamp, &rtcp_packet));
    }
  }

  SimulatedClock clock(0);
  TransportFeedbackAdapter feedback_adapter(&clock);

  TimeSeries late_feedback_series("Late feedback results.", LineStyle::kNone,
                                  PointStyle::kHighlight);
  TimeSeries time_series("Network Delay Change", LineStyle::kLine,
                         PointStyle::kHighlight);
  int64_t estimated_base_delay_ms = std::numeric_limits<int64_t>::max();

  auto rtp_iterator = outgoing_rtp.begin();
  auto rtcp_iterator = incoming_rtcp.begin();

  auto NextRtpTime = [&]() {
    if (rtp_iterator != outgoing_rtp.end())
      return static_cast<int64_t>(rtp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  auto NextRtcpTime = [&]() {
    if (rtcp_iterator != incoming_rtcp.end())
      return static_cast<int64_t>(rtcp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  int64_t time_us = std::min(NextRtpTime(), NextRtcpTime());
  int64_t prev_y = 0;
  while (time_us != std::numeric_limits<int64_t>::max()) {
    clock.AdvanceTimeMicroseconds(time_us - clock.TimeInMicroseconds());
    if (clock.TimeInMicroseconds() >= NextRtcpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtcpTime());
      const LoggedRtcpPacket& rtcp = *rtcp_iterator->second;
      if (rtcp.type == kRtcpTransportFeedback) {
        feedback_adapter.OnTransportFeedback(
            *static_cast<rtcp::TransportFeedback*>(rtcp.packet.get()));
        std::vector<PacketFeedback> feedback =
            feedback_adapter.GetTransportFeedbackVector();
        SortPacketFeedbackVector(&feedback);
        for (const PacketFeedback& packet : feedback) {
          float x =
              static_cast<float>(clock.TimeInMicroseconds() - begin_time_) /
              1000000;
          if (packet.send_time_ms == -1) {
            late_feedback_series.points.emplace_back(x, prev_y);
            continue;
          }
          int64_t y = packet.arrival_time_ms - packet.send_time_ms;
          prev_y = y;
          estimated_base_delay_ms = std::min(y, estimated_base_delay_ms);
          time_series.points.emplace_back(x, y);
        }
      }
      ++rtcp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextRtpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtpTime());
      const LoggedRtpPacket& rtp = *rtp_iterator->second;
      if (rtp.header.extension.hasTransportSequenceNumber) {
        RTC_DCHECK(rtp.header.extension.hasTransportSequenceNumber);
        feedback_adapter.AddPacket(rtp.header.ssrc,
                                   rtp.header.extension.transportSequenceNumber,
                                   rtp.total_length, PacedPacketInfo());
        feedback_adapter.OnSentPacket(
            rtp.header.extension.transportSequenceNumber, rtp.timestamp / 1000);
      }
      ++rtp_iterator;
    }
    time_us = std::min(NextRtpTime(), NextRtcpTime());
  }
  // We assume that the base network delay (w/o queues) is the min delay
  // observed during the call.
  for (TimeSeriesPoint& point : time_series.points)
    point.y -= estimated_base_delay_ms;
  for (TimeSeriesPoint& point : late_feedback_series.points)
    point.y -= estimated_base_delay_ms;
  // Add the data set to the plot.
  plot->AppendTimeSeriesIfNotEmpty(std::move(time_series));
  plot->AppendTimeSeriesIfNotEmpty(std::move(late_feedback_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Delay (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle("Network Delay Change.");
}

std::vector<std::pair<int64_t, int64_t>> EventLogAnalyzer::GetFrameTimestamps()
    const {
  std::vector<std::pair<int64_t, int64_t>> timestamps;
  size_t largest_stream_size = 0;
  const std::vector<LoggedRtpPacket>* largest_video_stream = nullptr;
  // Find the incoming video stream with the most number of packets that is
  // not rtx.
  for (const auto& kv : rtp_packets_) {
    if (kv.first.GetDirection() == kIncomingPacket &&
        video_ssrcs_.find(kv.first) != video_ssrcs_.end() &&
        rtx_ssrcs_.find(kv.first) == rtx_ssrcs_.end() &&
        kv.second.size() > largest_stream_size) {
      largest_stream_size = kv.second.size();
      largest_video_stream = &kv.second;
    }
  }
  if (largest_video_stream == nullptr) {
    for (auto& packet : *largest_video_stream) {
      if (packet.header.markerBit) {
        int64_t capture_ms = packet.header.timestamp / 90.0;
        int64_t arrival_ms = packet.timestamp / 1000.0;
        timestamps.push_back(std::make_pair(capture_ms, arrival_ms));
      }
    }
  }
  return timestamps;
}

void EventLogAnalyzer::CreatePacerDelayGraph(Plot* plot) {
  for (const auto& kv : rtp_packets_) {
    const std::vector<LoggedRtpPacket>& packets = kv.second;
    StreamId stream_id = kv.first;
    if (stream_id.GetDirection() == kIncomingPacket)
      continue;

    if (packets.size() < 2) {
      RTC_LOG(LS_WARNING)
          << "Can't estimate a the RTP clock frequency or the "
             "pacer delay with less than 2 packets in the stream";
      continue;
    }
    rtc::Optional<uint32_t> estimated_frequency =
        EstimateRtpClockFrequency(packets);
    if (!estimated_frequency)
      continue;
    if (IsVideoSsrc(stream_id) && *estimated_frequency != 90000) {
      RTC_LOG(LS_WARNING)
          << "Video stream should use a 90 kHz clock but appears to use "
          << *estimated_frequency / 1000 << ". Discarding.";
      continue;
    }

    TimeSeries pacer_delay_series(
        GetStreamName(stream_id) + "(" +
            std::to_string(*estimated_frequency / 1000) + " kHz)",
        LineStyle::kLine, PointStyle::kHighlight);
    SeqNumUnwrapper<uint32_t> timestamp_unwrapper;
    uint64_t first_capture_timestamp =
        timestamp_unwrapper.Unwrap(packets.front().header.timestamp);
    uint64_t first_send_timestamp = packets.front().timestamp;
    for (LoggedRtpPacket packet : packets) {
      double capture_time_ms = (static_cast<double>(timestamp_unwrapper.Unwrap(
                                    packet.header.timestamp)) -
                                first_capture_timestamp) /
                               *estimated_frequency * 1000;
      double send_time_ms =
          static_cast<double>(packet.timestamp - first_send_timestamp) / 1000;
      float x = static_cast<float>(packet.timestamp - begin_time_) / 1000000;
      float y = send_time_ms - capture_time_ms;
      pacer_delay_series.points.emplace_back(x, y);
    }
    plot->AppendTimeSeries(std::move(pacer_delay_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Pacer delay (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle(
      "Delay from capture to send time. (First packet normalized to 0.)");
}

void EventLogAnalyzer::CreateTimestampGraph(Plot* plot) {
  for (const auto& kv : rtp_packets_) {
    const std::vector<LoggedRtpPacket>& rtp_packets = kv.second;
    StreamId stream_id = kv.first;

    {
      TimeSeries timestamp_data(GetStreamName(stream_id) + " capture-time",
                                LineStyle::kLine, PointStyle::kHighlight);
      for (LoggedRtpPacket packet : rtp_packets) {
        float x = static_cast<float>(packet.timestamp - begin_time_) / 1000000;
        float y = packet.header.timestamp;
        timestamp_data.points.emplace_back(x, y);
      }
      plot->AppendTimeSeries(std::move(timestamp_data));
    }

    {
      auto kv = rtcp_packets_.find(stream_id);
      if (kv != rtcp_packets_.end()) {
        const auto& packets = kv->second;
        TimeSeries timestamp_data(
            GetStreamName(stream_id) + " rtcp capture-time", LineStyle::kLine,
            PointStyle::kHighlight);
        for (const LoggedRtcpPacket& rtcp : packets) {
          if (rtcp.type != kRtcpSr)
            continue;
          rtcp::SenderReport* sr;
          sr = static_cast<rtcp::SenderReport*>(rtcp.packet.get());
          float x = static_cast<float>(rtcp.timestamp - begin_time_) / 1000000;
          float y = sr->rtp_timestamp();
          timestamp_data.points.emplace_back(x, y);
        }
        plot->AppendTimeSeries(std::move(timestamp_data));
      }
    }
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Timestamp (90khz)", kBottomMargin, kTopMargin);
  plot->SetTitle("Timestamps");
}

void EventLogAnalyzer::CreateAudioEncoderTargetBitrateGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder target bitrate", LineStyle::kLine,
                         PointStyle::kHighlight);
  ProcessPoints<AudioNetworkAdaptationEvent>(
      [](const AudioNetworkAdaptationEvent& ana_event) -> rtc::Optional<float> {
        if (ana_event.config.bitrate_bps)
          return static_cast<float>(*ana_event.config.bitrate_bps);
        return rtc::nullopt;
      },
      audio_network_adaptation_events_, begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (bps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder target bitrate");
}

void EventLogAnalyzer::CreateAudioEncoderFrameLengthGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder frame length", LineStyle::kLine,
                         PointStyle::kHighlight);
  ProcessPoints<AudioNetworkAdaptationEvent>(
      [](const AudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.frame_length_ms)
          return rtc::Optional<float>(
              static_cast<float>(*ana_event.config.frame_length_ms));
        return rtc::Optional<float>();
      },
      audio_network_adaptation_events_, begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Frame length (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder frame length");
}

void EventLogAnalyzer::CreateAudioEncoderPacketLossGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder uplink packet loss fraction",
                         LineStyle::kLine, PointStyle::kHighlight);
  ProcessPoints<AudioNetworkAdaptationEvent>(
      [](const AudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.uplink_packet_loss_fraction)
          return rtc::Optional<float>(static_cast<float>(
              *ana_event.config.uplink_packet_loss_fraction));
        return rtc::Optional<float>();
      },
      audio_network_adaptation_events_, begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Percent lost packets", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Reported audio encoder lost packets");
}

void EventLogAnalyzer::CreateAudioEncoderEnableFecGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder FEC", LineStyle::kLine,
                         PointStyle::kHighlight);
  ProcessPoints<AudioNetworkAdaptationEvent>(
      [](const AudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.enable_fec)
          return rtc::Optional<float>(
              static_cast<float>(*ana_event.config.enable_fec));
        return rtc::Optional<float>();
      },
      audio_network_adaptation_events_, begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "FEC (false/true)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder FEC");
}

void EventLogAnalyzer::CreateAudioEncoderEnableDtxGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder DTX", LineStyle::kLine,
                         PointStyle::kHighlight);
  ProcessPoints<AudioNetworkAdaptationEvent>(
      [](const AudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.enable_dtx)
          return rtc::Optional<float>(
              static_cast<float>(*ana_event.config.enable_dtx));
        return rtc::Optional<float>();
      },
      audio_network_adaptation_events_, begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "DTX (false/true)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder DTX");
}

void EventLogAnalyzer::CreateAudioEncoderNumChannelsGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder number of channels", LineStyle::kLine,
                         PointStyle::kHighlight);
  ProcessPoints<AudioNetworkAdaptationEvent>(
      [](const AudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.num_channels)
          return rtc::Optional<float>(
              static_cast<float>(*ana_event.config.num_channels));
        return rtc::Optional<float>();
      },
      audio_network_adaptation_events_, begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Number of channels (1 (mono)/2 (stereo))",
                          kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder number of channels");
}

class NetEqStreamInput : public test::NetEqInput {
 public:
  // Does not take any ownership, and all pointers must refer to valid objects
  // that outlive the one constructed.
  NetEqStreamInput(const std::vector<LoggedRtpPacket>* packet_stream,
                   const std::vector<uint64_t>* output_events_us,
                   rtc::Optional<uint64_t> end_time_us)
      : packet_stream_(*packet_stream),
        packet_stream_it_(packet_stream_.begin()),
        output_events_us_it_(output_events_us->begin()),
        output_events_us_end_(output_events_us->end()),
        end_time_us_(end_time_us) {
    RTC_DCHECK(packet_stream);
    RTC_DCHECK(output_events_us);
  }

  rtc::Optional<int64_t> NextPacketTime() const override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return rtc::nullopt;
    }
    if (end_time_us_ && packet_stream_it_->timestamp > *end_time_us_) {
      return rtc::nullopt;
    }
    // Convert from us to ms.
    return packet_stream_it_->timestamp / 1000;
  }

  rtc::Optional<int64_t> NextOutputEventTime() const override {
    if (output_events_us_it_ == output_events_us_end_) {
      return rtc::nullopt;
    }
    if (end_time_us_ && *output_events_us_it_ > *end_time_us_) {
      return rtc::nullopt;
    }
    // Convert from us to ms.
    return rtc::checked_cast<int64_t>(*output_events_us_it_ / 1000);
  }

  std::unique_ptr<PacketData> PopPacket() override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return std::unique_ptr<PacketData>();
    }
    std::unique_ptr<PacketData> packet_data(new PacketData());
    packet_data->header = packet_stream_it_->header;
    // Convert from us to ms.
    packet_data->time_ms = packet_stream_it_->timestamp / 1000.0;

    // This is a header-only "dummy" packet. Set the payload to all zeros, with
    // length according to the virtual length.
    packet_data->payload.SetSize(packet_stream_it_->total_length);
    std::fill_n(packet_data->payload.data(), packet_data->payload.size(), 0);

    ++packet_stream_it_;
    return packet_data;
  }

  void AdvanceOutputEvent() override {
    if (output_events_us_it_ != output_events_us_end_) {
      ++output_events_us_it_;
    }
  }

  bool ended() const override { return !NextEventTime(); }

  rtc::Optional<RTPHeader> NextHeader() const override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return rtc::nullopt;
    }
    return packet_stream_it_->header;
  }

 private:
  const std::vector<LoggedRtpPacket>& packet_stream_;
  std::vector<LoggedRtpPacket>::const_iterator packet_stream_it_;
  std::vector<uint64_t>::const_iterator output_events_us_it_;
  const std::vector<uint64_t>::const_iterator output_events_us_end_;
  const rtc::Optional<uint64_t> end_time_us_;
};

namespace {
// Creates a NetEq test object and all necessary input and output helpers. Runs
// the test and returns the NetEqDelayAnalyzer object that was used to
// instrument the test.
std::unique_ptr<test::NetEqDelayAnalyzer> CreateNetEqTestAndRun(
    const std::vector<LoggedRtpPacket>* packet_stream,
    const std::vector<uint64_t>* output_events_us,
    rtc::Optional<uint64_t> end_time_us,
    const std::string& replacement_file_name,
    int file_sample_rate_hz) {
  std::unique_ptr<test::NetEqInput> input(
      new NetEqStreamInput(packet_stream, output_events_us, end_time_us));

  constexpr int kReplacementPt = 127;
  std::set<uint8_t> cn_types;
  std::set<uint8_t> forbidden_types;
  input.reset(new test::NetEqReplacementInput(std::move(input), kReplacementPt,
                                              cn_types, forbidden_types));

  NetEq::Config config;
  config.max_packets_in_buffer = 200;
  config.enable_fast_accelerate = true;

  std::unique_ptr<test::VoidAudioSink> output(new test::VoidAudioSink());

  test::NetEqTest::DecoderMap codecs;

  // Create a "replacement decoder" that produces the decoded audio by reading
  // from a file rather than from the encoded payloads.
  std::unique_ptr<test::ResampleInputAudioFile> replacement_file(
      new test::ResampleInputAudioFile(replacement_file_name,
                                       file_sample_rate_hz));
  replacement_file->set_output_rate_hz(48000);
  std::unique_ptr<AudioDecoder> replacement_decoder(
      new test::FakeDecodeFromFile(std::move(replacement_file), 48000, false));
  test::NetEqTest::ExtDecoderMap ext_codecs;
  ext_codecs[kReplacementPt] = {replacement_decoder.get(),
                                NetEqDecoder::kDecoderArbitrary,
                                "replacement codec"};

  std::unique_ptr<test::NetEqDelayAnalyzer> delay_cb(
      new test::NetEqDelayAnalyzer);
  test::DefaultNetEqTestErrorCallback error_cb;
  test::NetEqTest::Callbacks callbacks;
  callbacks.error_callback = &error_cb;
  callbacks.post_insert_packet = delay_cb.get();
  callbacks.get_audio_callback = delay_cb.get();

  test::NetEqTest test(config, codecs, ext_codecs, std::move(input),
                       std::move(output), callbacks);
  test.Run();
  return delay_cb;
}
}  // namespace

// Plots the jitter buffer delay profile. This will plot only for the first
// incoming audio SSRC. If the stream contains more than one incoming audio
// SSRC, all but the first will be ignored.
void EventLogAnalyzer::CreateAudioJitterBufferGraph(
    const std::string& replacement_file_name,
    int file_sample_rate_hz,
    Plot* plot) {
  const auto& incoming_audio_kv = std::find_if(
      rtp_packets_.begin(), rtp_packets_.end(),
      [this](std::pair<StreamId, std::vector<LoggedRtpPacket>> kv) {
        return kv.first.GetDirection() == kIncomingPacket &&
               this->IsAudioSsrc(kv.first);
      });
  if (incoming_audio_kv == rtp_packets_.end()) {
    // No incoming audio stream found.
    return;
  }

  const uint32_t ssrc = incoming_audio_kv->first.GetSsrc();

  std::map<uint32_t, std::vector<uint64_t>>::const_iterator output_events_it =
      audio_playout_events_.find(ssrc);
  if (output_events_it == audio_playout_events_.end()) {
    // Could not find output events with SSRC matching the input audio stream.
    // Using the first available stream of output events.
    output_events_it = audio_playout_events_.cbegin();
  }

  rtc::Optional<uint64_t> end_time_us =
      log_segments_.empty()
          ? rtc::nullopt
          : rtc::Optional<uint64_t>(log_segments_.front().second);

  auto delay_cb = CreateNetEqTestAndRun(
      &incoming_audio_kv->second, &output_events_it->second, end_time_us,
      replacement_file_name, file_sample_rate_hz);

  std::vector<float> send_times_s;
  std::vector<float> arrival_delay_ms;
  std::vector<float> corrected_arrival_delay_ms;
  std::vector<rtc::Optional<float>> playout_delay_ms;
  std::vector<rtc::Optional<float>> target_delay_ms;
  delay_cb->CreateGraphs(&send_times_s, &arrival_delay_ms,
                         &corrected_arrival_delay_ms, &playout_delay_ms,
                         &target_delay_ms);
  RTC_DCHECK_EQ(send_times_s.size(), arrival_delay_ms.size());
  RTC_DCHECK_EQ(send_times_s.size(), corrected_arrival_delay_ms.size());
  RTC_DCHECK_EQ(send_times_s.size(), playout_delay_ms.size());
  RTC_DCHECK_EQ(send_times_s.size(), target_delay_ms.size());

  std::map<StreamId, TimeSeries> time_series_packet_arrival;
  std::map<StreamId, TimeSeries> time_series_relative_packet_arrival;
  std::map<StreamId, TimeSeries> time_series_play_time;
  std::map<StreamId, TimeSeries> time_series_target_time;
  float min_y_axis = 0.f;
  float max_y_axis = 0.f;
  const StreamId stream_id = incoming_audio_kv->first;
  for (size_t i = 0; i < send_times_s.size(); ++i) {
    time_series_packet_arrival[stream_id].points.emplace_back(
        TimeSeriesPoint(send_times_s[i], arrival_delay_ms[i]));
    time_series_relative_packet_arrival[stream_id].points.emplace_back(
        TimeSeriesPoint(send_times_s[i], corrected_arrival_delay_ms[i]));
    min_y_axis = std::min(min_y_axis, corrected_arrival_delay_ms[i]);
    max_y_axis = std::max(max_y_axis, corrected_arrival_delay_ms[i]);
    if (playout_delay_ms[i]) {
      time_series_play_time[stream_id].points.emplace_back(
          TimeSeriesPoint(send_times_s[i], *playout_delay_ms[i]));
      min_y_axis = std::min(min_y_axis, *playout_delay_ms[i]);
      max_y_axis = std::max(max_y_axis, *playout_delay_ms[i]);
    }
    if (target_delay_ms[i]) {
      time_series_target_time[stream_id].points.emplace_back(
          TimeSeriesPoint(send_times_s[i], *target_delay_ms[i]));
      min_y_axis = std::min(min_y_axis, *target_delay_ms[i]);
      max_y_axis = std::max(max_y_axis, *target_delay_ms[i]);
    }
  }

  // This code is adapted for a single stream. The creation of the streams above
  // guarantee that no more than one steam is included. If multiple streams are
  // to be plotted, they should likely be given distinct labels below.
  RTC_DCHECK_EQ(time_series_relative_packet_arrival.size(), 1);
  for (auto& series : time_series_relative_packet_arrival) {
    series.second.label = "Relative packet arrival delay";
    series.second.line_style = LineStyle::kLine;
    plot->AppendTimeSeries(std::move(series.second));
  }
  RTC_DCHECK_EQ(time_series_play_time.size(), 1);
  for (auto& series : time_series_play_time) {
    series.second.label = "Playout delay";
    series.second.line_style = LineStyle::kLine;
    plot->AppendTimeSeries(std::move(series.second));
  }
  RTC_DCHECK_EQ(time_series_target_time.size(), 1);
  for (auto& series : time_series_target_time) {
    series.second.label = "Target delay";
    series.second.line_style = LineStyle::kLine;
    series.second.point_style = PointStyle::kHighlight;
    plot->AppendTimeSeries(std::move(series.second));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetYAxis(min_y_axis, max_y_axis, "Relative delay (ms)", kBottomMargin,
                 kTopMargin);
  plot->SetTitle("NetEq timing");
}
}  // namespace plotting
}  // namespace webrtc
