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
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "common_types.h"  // NOLINT(build/include)
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/audio_coding/neteq/tools/audio_sink.h"
#include "modules/audio_coding/neteq/tools/fake_decode_from_file.h"
#include "modules/audio_coding/neteq/tools/neteq_delay_analyzer.h"
#include "modules/audio_coding/neteq/tools/neteq_replacement_input.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/audio_coding/neteq/tools/resample_input_audio_file.h"
#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/goog_cc/bitrate_estimator.h"
#include "modules/congestion_controller/goog_cc/delay_based_bwe.h"
#include "modules/congestion_controller/include/receive_side_congestion_controller.h"
#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/function_view.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/strings/string_builder.h"

#ifndef BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
#define BWE_TEST_LOGGING_COMPILE_TIME_ENABLE 0
#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE

namespace webrtc {

namespace {

const int kNumMicrosecsPerSec = 1000000;

void SortPacketFeedbackVector(std::vector<PacketFeedback>* vec) {
  auto pred = [](const PacketFeedback& packet_feedback) {
    return packet_feedback.arrival_time_ms == PacketFeedback::kNotReceived;
  };
  vec->erase(std::remove_if(vec->begin(), vec->end(), pred), vec->end());
  std::sort(vec->begin(), vec->end(), PacketFeedbackComparator());
}

std::string SsrcToString(uint32_t ssrc) {
  rtc::StringBuilder ss;
  ss << "SSRC " << ssrc;
  return ss.Release();
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
  // time in seconds and then multiply by kNumMicrosecsPerSec to convert to
  // microseconds.
  static constexpr double kTimestampToMicroSec =
      static_cast<double>(kNumMicrosecsPerSec) / static_cast<double>(1ul << 18);
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

// This is much more reliable for outgoing streams than for incoming streams.
template <typename RtpPacketContainer>
absl::optional<uint32_t> EstimateRtpClockFrequency(
    const RtpPacketContainer& packets,
    int64_t end_time_us) {
  RTC_CHECK(packets.size() >= 2);
  SeqNumUnwrapper<uint32_t> unwrapper;
  uint64_t first_rtp_timestamp =
      unwrapper.Unwrap(packets[0].rtp.header.timestamp);
  int64_t first_log_timestamp = packets[0].log_time_us();
  uint64_t last_rtp_timestamp = first_rtp_timestamp;
  int64_t last_log_timestamp = first_log_timestamp;
  for (size_t i = 1; i < packets.size(); i++) {
    if (packets[i].log_time_us() > end_time_us)
      break;
    last_rtp_timestamp = unwrapper.Unwrap(packets[i].rtp.header.timestamp);
    last_log_timestamp = packets[i].log_time_us();
  }
  if (last_log_timestamp - first_log_timestamp < kNumMicrosecsPerSec) {
    RTC_LOG(LS_WARNING)
        << "Failed to estimate RTP clock frequency: Stream too short. ("
        << packets.size() << " packets, "
        << last_log_timestamp - first_log_timestamp << " us)";
    return absl::nullopt;
  }
  double duration =
      static_cast<double>(last_log_timestamp - first_log_timestamp) /
      kNumMicrosecsPerSec;
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
  return absl::nullopt;
}

constexpr float kLeftMargin = 0.01f;
constexpr float kRightMargin = 0.02f;
constexpr float kBottomMargin = 0.02f;
constexpr float kTopMargin = 0.05f;

absl::optional<double> NetworkDelayDiff_AbsSendTime(
    const LoggedRtpPacket& old_packet,
    const LoggedRtpPacket& new_packet) {
  if (old_packet.header.extension.hasAbsoluteSendTime &&
      new_packet.header.extension.hasAbsoluteSendTime) {
    int64_t send_time_diff = WrappingDifference(
        new_packet.header.extension.absoluteSendTime,
        old_packet.header.extension.absoluteSendTime, 1ul << 24);
    int64_t recv_time_diff =
        new_packet.log_time_us() - old_packet.log_time_us();
    double delay_change_us =
        recv_time_diff - AbsSendTimeToMicroseconds(send_time_diff);
    return delay_change_us / 1000;
  } else {
    return absl::nullopt;
  }
}

absl::optional<double> NetworkDelayDiff_CaptureTime(
    const LoggedRtpPacket& old_packet,
    const LoggedRtpPacket& new_packet) {
  int64_t send_time_diff = WrappingDifference(
      new_packet.header.timestamp, old_packet.header.timestamp, 1ull << 32);
  int64_t recv_time_diff = new_packet.log_time_us() - old_packet.log_time_us();

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
                        << ", received time " << old_packet.log_time_us();
    RTC_LOG(LS_WARNING) << "New capture time " << new_packet.header.timestamp
                        << ", received time " << new_packet.log_time_us();
    RTC_LOG(LS_WARNING) << "Receive time difference " << recv_time_diff << " = "
                        << static_cast<double>(recv_time_diff) /
                               kNumMicrosecsPerSec
                        << "s";
    RTC_LOG(LS_WARNING) << "Send time difference " << send_time_diff << " = "
                        << static_cast<double>(send_time_diff) /
                               kVideoSampleRate
                        << "s";
  }
  return delay_change;
}

// For each element in data_view, use |f()| to extract a y-coordinate and
// store the result in a TimeSeries.
template <typename DataType, typename IterableType>
void ProcessPoints(rtc::FunctionView<float(const DataType&)> fx,
                   rtc::FunctionView<absl::optional<float>(const DataType&)> fy,
                   const IterableType& data_view,
                   TimeSeries* result) {
  for (size_t i = 0; i < data_view.size(); i++) {
    const DataType& elem = data_view[i];
    float x = fx(elem);
    absl::optional<float> y = fy(elem);
    if (y)
      result->points.emplace_back(x, *y);
  }
}

// For each pair of adjacent elements in |data|, use |f()| to extract a
// y-coordinate and store the result in a TimeSeries. Note that the x-coordinate
// will be the time of the second element in the pair.
template <typename DataType, typename ResultType, typename IterableType>
void ProcessPairs(
    rtc::FunctionView<float(const DataType&)> fx,
    rtc::FunctionView<absl::optional<ResultType>(const DataType&,
                                                 const DataType&)> fy,
    const IterableType& data,
    TimeSeries* result) {
  for (size_t i = 1; i < data.size(); i++) {
    float x = fx(data[i]);
    absl::optional<ResultType> y = fy(data[i - 1], data[i]);
    if (y)
      result->points.emplace_back(x, static_cast<float>(*y));
  }
}

// For each pair of adjacent elements in |data|, use |f()| to extract a
// y-coordinate and store the result in a TimeSeries. Note that the x-coordinate
// will be the time of the second element in the pair.
template <typename DataType, typename ResultType, typename IterableType>
void AccumulatePairs(
    rtc::FunctionView<float(const DataType&)> fx,
    rtc::FunctionView<absl::optional<ResultType>(const DataType&,
                                                 const DataType&)> fy,
    const IterableType& data,
    TimeSeries* result) {
  ResultType sum = 0;
  for (size_t i = 1; i < data.size(); i++) {
    float x = fx(data[i]);
    absl::optional<ResultType> y = fy(data[i - 1], data[i]);
    if (y)
      sum += *y;
    result->points.emplace_back(x, static_cast<float>(sum));
  }
}

// Calculates a moving average of |data| and stores the result in a TimeSeries.
// A data point is generated every |step| microseconds from |begin_time|
// to |end_time|. The value of each data point is the average of the data
// during the preceeding |window_duration_us| microseconds.
template <typename DataType, typename ResultType, typename IterableType>
void MovingAverage(
    rtc::FunctionView<float(int64_t)> fx,
    rtc::FunctionView<absl::optional<ResultType>(const DataType&)> fy,
    const IterableType& data_view,
    int64_t begin_time,
    int64_t end_time,
    int64_t window_duration_us,
    int64_t step,
    TimeSeries* result) {
  size_t window_index_begin = 0;
  size_t window_index_end = 0;
  ResultType sum_in_window = 0;

  for (int64_t t = begin_time; t < end_time + step; t += step) {
    while (window_index_end < data_view.size() &&
           data_view[window_index_end].log_time_us() < t) {
      absl::optional<ResultType> value = fy(data_view[window_index_end]);
      if (value)
        sum_in_window += *value;
      ++window_index_end;
    }
    while (window_index_begin < data_view.size() &&
           data_view[window_index_begin].log_time_us() <
               t - window_duration_us) {
      absl::optional<ResultType> value = fy(data_view[window_index_begin]);
      if (value)
        sum_in_window -= *value;
      ++window_index_begin;
    }
    float window_duration_s =
        static_cast<float>(window_duration_us) / kNumMicrosecsPerSec;
    float x = fx(t);
    float y = sum_in_window / window_duration_s;
    result->points.emplace_back(x, y);
  }
}

const char kUnknownEnumValue[] = "unknown";

const char kIceCandidateTypeLocal[] = "local";
const char kIceCandidateTypeStun[] = "stun";
const char kIceCandidateTypePrflx[] = "prflx";
const char kIceCandidateTypeRelay[] = "relay";

const char kProtocolUdp[] = "udp";
const char kProtocolTcp[] = "tcp";
const char kProtocolSsltcp[] = "ssltcp";
const char kProtocolTls[] = "tls";

const char kAddressFamilyIpv4[] = "ipv4";
const char kAddressFamilyIpv6[] = "ipv6";

const char kNetworkTypeEthernet[] = "ethernet";
const char kNetworkTypeLoopback[] = "loopback";
const char kNetworkTypeWifi[] = "wifi";
const char kNetworkTypeVpn[] = "vpn";
const char kNetworkTypeCellular[] = "cellular";

std::string GetIceCandidateTypeAsString(webrtc::IceCandidateType type) {
  switch (type) {
    case webrtc::IceCandidateType::kLocal:
      return kIceCandidateTypeLocal;
    case webrtc::IceCandidateType::kStun:
      return kIceCandidateTypeStun;
    case webrtc::IceCandidateType::kPrflx:
      return kIceCandidateTypePrflx;
    case webrtc::IceCandidateType::kRelay:
      return kIceCandidateTypeRelay;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetProtocolAsString(webrtc::IceCandidatePairProtocol protocol) {
  switch (protocol) {
    case webrtc::IceCandidatePairProtocol::kUdp:
      return kProtocolUdp;
    case webrtc::IceCandidatePairProtocol::kTcp:
      return kProtocolTcp;
    case webrtc::IceCandidatePairProtocol::kSsltcp:
      return kProtocolSsltcp;
    case webrtc::IceCandidatePairProtocol::kTls:
      return kProtocolTls;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetAddressFamilyAsString(
    webrtc::IceCandidatePairAddressFamily family) {
  switch (family) {
    case webrtc::IceCandidatePairAddressFamily::kIpv4:
      return kAddressFamilyIpv4;
    case webrtc::IceCandidatePairAddressFamily::kIpv6:
      return kAddressFamilyIpv6;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetNetworkTypeAsString(webrtc::IceCandidateNetworkType type) {
  switch (type) {
    case webrtc::IceCandidateNetworkType::kEthernet:
      return kNetworkTypeEthernet;
    case webrtc::IceCandidateNetworkType::kLoopback:
      return kNetworkTypeLoopback;
    case webrtc::IceCandidateNetworkType::kWifi:
      return kNetworkTypeWifi;
    case webrtc::IceCandidateNetworkType::kVpn:
      return kNetworkTypeVpn;
    case webrtc::IceCandidateNetworkType::kCellular:
      return kNetworkTypeCellular;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetCandidatePairLogDescriptionAsString(
    const LoggedIceCandidatePairConfig& config) {
  // Example: stun:wifi->relay(tcp):cellular@udp:ipv4
  // represents a pair of a local server-reflexive candidate on a WiFi network
  // and a remote relay candidate using TCP as the relay protocol on a cell
  // network, when the candidate pair communicates over UDP using IPv4.
  rtc::StringBuilder ss;
  std::string local_candidate_type =
      GetIceCandidateTypeAsString(config.local_candidate_type);
  std::string remote_candidate_type =
      GetIceCandidateTypeAsString(config.remote_candidate_type);
  if (config.local_candidate_type == webrtc::IceCandidateType::kRelay) {
    local_candidate_type +=
        "(" + GetProtocolAsString(config.local_relay_protocol) + ")";
  }
  ss << local_candidate_type << ":"
     << GetNetworkTypeAsString(config.local_network_type) << ":"
     << GetAddressFamilyAsString(config.local_address_family) << "->"
     << remote_candidate_type << ":"
     << GetAddressFamilyAsString(config.remote_address_family) << "@"
     << GetProtocolAsString(config.candidate_pair_protocol);
  return ss.Release();
}

std::string GetDirectionAsString(PacketDirection direction) {
  if (direction == kIncomingPacket) {
    return "Incoming";
  } else {
    return "Outgoing";
  }
}

std::string GetDirectionAsShortString(PacketDirection direction) {
  if (direction == kIncomingPacket) {
    return "In";
  } else {
    return "Out";
  }
}

}  // namespace

EventLogAnalyzer::EventLogAnalyzer(const ParsedRtcEventLogNew& log,
                                   bool normalize_time)
    : parsed_log_(log),
      window_duration_(250000),
      step_(10000),
      normalize_time_(normalize_time) {
  begin_time_ = parsed_log_.first_timestamp();
  end_time_ = parsed_log_.last_timestamp();
  if (end_time_ < begin_time_) {
    RTC_LOG(LS_WARNING) << "No useful events in the log.";
    begin_time_ = end_time_ = 0;
  }
  call_duration_s_ = ToCallTimeSec(end_time_);

  const auto& log_start_events = parsed_log_.start_log_events();
  const auto& log_end_events = parsed_log_.stop_log_events();
  auto start_iter = log_start_events.begin();
  auto end_iter = log_end_events.begin();
  while (start_iter != log_start_events.end()) {
    int64_t start = start_iter->log_time_us();
    ++start_iter;
    absl::optional<int64_t> next_start;
    if (start_iter != log_start_events.end())
      next_start.emplace(start_iter->log_time_us());
    if (end_iter != log_end_events.end() &&
        end_iter->log_time_us() <=
            next_start.value_or(std::numeric_limits<int64_t>::max())) {
      int64_t end = end_iter->log_time_us();
      RTC_DCHECK_LE(start, end);
      log_segments_.push_back(std::make_pair(start, end));
      ++end_iter;
    } else {
      // we're missing an end event. Assume that it occurred just before the
      // next start.
      log_segments_.push_back(
          std::make_pair(start, next_start.value_or(end_time_)));
    }
  }
  RTC_LOG(LS_INFO) << "Found " << log_segments_.size()
                   << " (LOG_START, LOG_END) segments in log.";
}

class BitrateObserver : public NetworkChangedObserver,
                        public RemoteBitrateObserver {
 public:
  BitrateObserver() : last_bitrate_bps_(0), bitrate_updated_(false) {}

  void OnNetworkChanged(uint32_t bitrate_bps,
                        uint8_t fraction_lost,
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

int64_t EventLogAnalyzer::ToCallTimeUs(int64_t timestamp) const {
  int64_t begin_time = 0;
  if (normalize_time_) {
    begin_time = begin_time_;
  }
  return timestamp - begin_time;
}

float EventLogAnalyzer::ToCallTimeSec(int64_t timestamp) const {
  return static_cast<float>(ToCallTimeUs(timestamp)) / kNumMicrosecsPerSec;
}

void EventLogAnalyzer::CreatePacketGraph(PacketDirection direction,
                                         Plot* plot) {
  for (const auto& stream : parsed_log_.rtp_packets_by_ssrc(direction)) {
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName(direction, stream.ssrc),
                           LineStyle::kBar);
    auto GetPacketSize = [](const LoggedRtpPacket& packet) {
      return absl::optional<float>(packet.total_length);
    };
    auto ToCallTime = [this](const LoggedRtpPacket& packet) {
      return this->ToCallTimeSec(packet.log_time_us());
    };
    ProcessPoints<LoggedRtpPacket>(ToCallTime, GetPacketSize,
                                   stream.packet_view, &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Packet size (bytes)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle(GetDirectionAsString(direction) + " RTP packets");
}

template <typename IterableType>
void EventLogAnalyzer::CreateAccumulatedPacketsTimeSeries(
    Plot* plot,
    const IterableType& packets,
    const std::string& label) {
  TimeSeries time_series(label, LineStyle::kStep);
  for (size_t i = 0; i < packets.size(); i++) {
    float x = ToCallTimeSec(packets[i].log_time_us());
    time_series.points.emplace_back(x, i + 1);
  }
  plot->AppendTimeSeries(std::move(time_series));
}

void EventLogAnalyzer::CreateAccumulatedPacketsGraph(PacketDirection direction,
                                                     Plot* plot) {
  for (const auto& stream : parsed_log_.rtp_packets_by_ssrc(direction)) {
    if (!MatchingSsrc(stream.ssrc, desired_ssrc_))
      continue;
    std::string label =
        std::string("RTP ") + GetStreamName(direction, stream.ssrc);
    CreateAccumulatedPacketsTimeSeries(plot, stream.packet_view, label);
  }
  std::string label =
      std::string("RTCP ") + "(" + GetDirectionAsShortString(direction) + ")";
  if (direction == kIncomingPacket) {
    CreateAccumulatedPacketsTimeSeries(
        plot, parsed_log_.incoming_rtcp_packets(), label);
  } else {
    CreateAccumulatedPacketsTimeSeries(
        plot, parsed_log_.outgoing_rtcp_packets(), label);
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Received Packets", kBottomMargin, kTopMargin);
  plot->SetTitle(std::string("Accumulated ") + GetDirectionAsString(direction) +
                 " RTP/RTCP packets");
}

// For each SSRC, plot the time between the consecutive playouts.
void EventLogAnalyzer::CreatePlayoutGraph(Plot* plot) {
  for (const auto& playout_stream : parsed_log_.audio_playout_events()) {
    uint32_t ssrc = playout_stream.first;
    if (!MatchingSsrc(ssrc, desired_ssrc_))
      continue;
    absl::optional<int64_t> last_playout_ms;
    TimeSeries time_series(SsrcToString(ssrc), LineStyle::kBar);
    for (const auto& playout_event : playout_stream.second) {
      float x = ToCallTimeSec(playout_event.log_time_us());
      int64_t playout_time_ms = playout_event.log_time_ms();
      // If there were no previous playouts, place the point on the x-axis.
      float y = playout_time_ms - last_playout_ms.value_or(playout_time_ms);
      time_series.points.push_back(TimeSeriesPoint(x, y));
      last_playout_ms.emplace(playout_time_ms);
    }
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Time since last playout (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Audio playout");
}

// For audio SSRCs, plot the audio level.
void EventLogAnalyzer::CreateAudioLevelGraph(PacketDirection direction,
                                             Plot* plot) {
  for (const auto& stream : parsed_log_.rtp_packets_by_ssrc(direction)) {
    if (!IsAudioSsrc(direction, stream.ssrc))
      continue;
    TimeSeries time_series(GetStreamName(direction, stream.ssrc),
                           LineStyle::kLine);
    for (auto& packet : stream.packet_view) {
      if (packet.header.extension.hasAudioLevel) {
        float x = ToCallTimeSec(packet.log_time_us());
        // The audio level is stored in -dBov (so e.g. -10 dBov is stored as 10)
        // Here we convert it to dBov.
        float y = static_cast<float>(-packet.header.extension.audioLevel);
        time_series.points.emplace_back(TimeSeriesPoint(x, y));
      }
    }
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetYAxis(-127, 0, "Audio level (dBov)", kBottomMargin, kTopMargin);
  plot->SetTitle(GetDirectionAsString(direction) + " audio level");
}

// For each SSRC, plot the time between the consecutive playouts.
void EventLogAnalyzer::CreateSequenceNumberGraph(Plot* plot) {
  for (const auto& stream : parsed_log_.incoming_rtp_packets_by_ssrc()) {
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName(kIncomingPacket, stream.ssrc),
                           LineStyle::kBar);
    auto GetSequenceNumberDiff = [](const LoggedRtpPacketIncoming& old_packet,
                                    const LoggedRtpPacketIncoming& new_packet) {
      int64_t diff =
          WrappingDifference(new_packet.rtp.header.sequenceNumber,
                             old_packet.rtp.header.sequenceNumber, 1ul << 16);
      return diff;
    };
    auto ToCallTime = [this](const LoggedRtpPacketIncoming& packet) {
      return this->ToCallTimeSec(packet.log_time_us());
    };
    ProcessPairs<LoggedRtpPacketIncoming, float>(
        ToCallTime, GetSequenceNumberDiff, stream.incoming_packets,
        &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Difference since last packet", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Sequence number");
}

void EventLogAnalyzer::CreateIncomingPacketLossGraph(Plot* plot) {
  for (const auto& stream : parsed_log_.incoming_rtp_packets_by_ssrc()) {
    const std::vector<LoggedRtpPacketIncoming>& packets =
        stream.incoming_packets;
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, desired_ssrc_) || packets.size() == 0) {
      continue;
    }

    TimeSeries time_series(GetStreamName(kIncomingPacket, stream.ssrc),
                           LineStyle::kLine, PointStyle::kHighlight);
    // TODO(terelius): Should the window and step size be read from the class
    // instead?
    const int64_t kWindowUs = 1000000;
    const int64_t kStep = 1000000;
    SeqNumUnwrapper<uint16_t> unwrapper_;
    SeqNumUnwrapper<uint16_t> prior_unwrapper_;
    size_t window_index_begin = 0;
    size_t window_index_end = 0;
    uint64_t highest_seq_number =
        unwrapper_.Unwrap(packets[0].rtp.header.sequenceNumber) - 1;
    uint64_t highest_prior_seq_number =
        prior_unwrapper_.Unwrap(packets[0].rtp.header.sequenceNumber) - 1;

    for (int64_t t = begin_time_; t < end_time_ + kStep; t += kStep) {
      while (window_index_end < packets.size() &&
             packets[window_index_end].rtp.log_time_us() < t) {
        uint64_t sequence_number = unwrapper_.Unwrap(
            packets[window_index_end].rtp.header.sequenceNumber);
        highest_seq_number = std::max(highest_seq_number, sequence_number);
        ++window_index_end;
      }
      while (window_index_begin < packets.size() &&
             packets[window_index_begin].rtp.log_time_us() < t - kWindowUs) {
        uint64_t sequence_number = prior_unwrapper_.Unwrap(
            packets[window_index_begin].rtp.header.sequenceNumber);
        highest_prior_seq_number =
            std::max(highest_prior_seq_number, sequence_number);
        ++window_index_begin;
      }
      float x = ToCallTimeSec(t);
      uint64_t expected_packets = highest_seq_number - highest_prior_seq_number;
      if (expected_packets > 0) {
        int64_t received_packets = window_index_end - window_index_begin;
        int64_t lost_packets = expected_packets - received_packets;
        float y = static_cast<float>(lost_packets) / expected_packets * 100;
        time_series.points.emplace_back(x, y);
      }
    }
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Estimated loss rate (%)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Estimated incoming loss rate");
}

void EventLogAnalyzer::CreateIncomingDelayDeltaGraph(Plot* plot) {
  for (const auto& stream : parsed_log_.rtp_packets_by_ssrc(kIncomingPacket)) {
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, desired_ssrc_) ||
        IsAudioSsrc(kIncomingPacket, stream.ssrc) ||
        !IsVideoSsrc(kIncomingPacket, stream.ssrc) ||
        IsRtxSsrc(kIncomingPacket, stream.ssrc)) {
      continue;
    }

    TimeSeries capture_time_data(
        GetStreamName(kIncomingPacket, stream.ssrc) + " capture-time",
        LineStyle::kBar);
    auto ToCallTime = [this](const LoggedRtpPacket& packet) {
      return this->ToCallTimeSec(packet.log_time_us());
    };
    ProcessPairs<LoggedRtpPacket, double>(
        ToCallTime, NetworkDelayDiff_CaptureTime, stream.packet_view,
        &capture_time_data);
    plot->AppendTimeSeries(std::move(capture_time_data));

    TimeSeries send_time_data(
        GetStreamName(kIncomingPacket, stream.ssrc) + " abs-send-time",
        LineStyle::kBar);
    ProcessPairs<LoggedRtpPacket, double>(ToCallTime,
                                          NetworkDelayDiff_AbsSendTime,
                                          stream.packet_view, &send_time_data);
    plot->AppendTimeSeries(std::move(send_time_data));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Latency change (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Network latency difference between consecutive packets");
}

void EventLogAnalyzer::CreateIncomingDelayGraph(Plot* plot) {
  for (const auto& stream : parsed_log_.rtp_packets_by_ssrc(kIncomingPacket)) {
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, desired_ssrc_) ||
        IsAudioSsrc(kIncomingPacket, stream.ssrc) ||
        !IsVideoSsrc(kIncomingPacket, stream.ssrc) ||
        IsRtxSsrc(kIncomingPacket, stream.ssrc)) {
      continue;
    }

    TimeSeries capture_time_data(
        GetStreamName(kIncomingPacket, stream.ssrc) + " capture-time",
        LineStyle::kLine);
    auto ToCallTime = [this](const LoggedRtpPacket& packet) {
      return this->ToCallTimeSec(packet.log_time_us());
    };
    AccumulatePairs<LoggedRtpPacket, double>(
        ToCallTime, NetworkDelayDiff_CaptureTime, stream.packet_view,
        &capture_time_data);
    plot->AppendTimeSeries(std::move(capture_time_data));

    TimeSeries send_time_data(
        GetStreamName(kIncomingPacket, stream.ssrc) + " abs-send-time",
        LineStyle::kLine);
    AccumulatePairs<LoggedRtpPacket, double>(
        ToCallTime, NetworkDelayDiff_AbsSendTime, stream.packet_view,
        &send_time_data);
    plot->AppendTimeSeries(std::move(send_time_data));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Latency change (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Network latency (relative to first packet)");
}

// Plot the fraction of packets lost (as perceived by the loss-based BWE).
void EventLogAnalyzer::CreateFractionLossGraph(Plot* plot) {
  TimeSeries time_series("Fraction lost", LineStyle::kLine,
                         PointStyle::kHighlight);
  for (auto& bwe_update : parsed_log_.bwe_loss_updates()) {
    float x = ToCallTimeSec(bwe_update.log_time_us());
    float y = static_cast<float>(bwe_update.fraction_lost) / 255 * 100;
    time_series.points.emplace_back(x, y);
  }

  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Percent lost packets", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Reported packet loss");
}

// Plot the total bandwidth used by all RTP streams.
void EventLogAnalyzer::CreateTotalIncomingBitrateGraph(Plot* plot) {
  // TODO(terelius): This could be provided by the parser.
  std::multimap<int64_t, size_t> packets_in_order;
  for (const auto& stream : parsed_log_.incoming_rtp_packets_by_ssrc()) {
    for (const LoggedRtpPacketIncoming& packet : stream.incoming_packets)
      packets_in_order.insert(
          std::make_pair(packet.rtp.log_time_us(), packet.rtp.total_length));
  }

  auto window_begin = packets_in_order.begin();
  auto window_end = packets_in_order.begin();
  size_t bytes_in_window = 0;

  // Calculate a moving average of the bitrate and store in a TimeSeries.
  TimeSeries bitrate_series("Bitrate", LineStyle::kLine);
  for (int64_t time = begin_time_; time < end_time_ + step_; time += step_) {
    while (window_end != packets_in_order.end() && window_end->first < time) {
      bytes_in_window += window_end->second;
      ++window_end;
    }
    while (window_begin != packets_in_order.end() &&
           window_begin->first < time - window_duration_) {
      RTC_DCHECK_LE(window_begin->second, bytes_in_window);
      bytes_in_window -= window_begin->second;
      ++window_begin;
    }
    float window_duration_in_seconds =
        static_cast<float>(window_duration_) / kNumMicrosecsPerSec;
    float x = ToCallTimeSec(time);
    float y = bytes_in_window * 8 / window_duration_in_seconds / 1000;
    bitrate_series.points.emplace_back(x, y);
  }
  plot->AppendTimeSeries(std::move(bitrate_series));

  // Overlay the outgoing REMB over incoming bitrate.
  TimeSeries remb_series("Remb", LineStyle::kStep);
  for (const auto& rtcp : parsed_log_.rembs(kOutgoingPacket)) {
    float x = ToCallTimeSec(rtcp.log_time_us());
    float y = static_cast<float>(rtcp.remb.bitrate_bps()) / 1000;
    remb_series.points.emplace_back(x, y);
  }
  plot->AppendTimeSeriesIfNotEmpty(std::move(remb_series));

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Incoming RTP bitrate");
}

// Plot the total bandwidth used by all RTP streams.
void EventLogAnalyzer::CreateTotalOutgoingBitrateGraph(Plot* plot,
                                                       bool show_detector_state,
                                                       bool show_alr_state) {
  // TODO(terelius): This could be provided by the parser.
  std::multimap<int64_t, size_t> packets_in_order;
  for (const auto& stream : parsed_log_.outgoing_rtp_packets_by_ssrc()) {
    for (const LoggedRtpPacketOutgoing& packet : stream.outgoing_packets)
      packets_in_order.insert(
          std::make_pair(packet.rtp.log_time_us(), packet.rtp.total_length));
  }

  auto window_begin = packets_in_order.begin();
  auto window_end = packets_in_order.begin();
  size_t bytes_in_window = 0;

  // Calculate a moving average of the bitrate and store in a TimeSeries.
  TimeSeries bitrate_series("Bitrate", LineStyle::kLine);
  for (int64_t time = begin_time_; time < end_time_ + step_; time += step_) {
    while (window_end != packets_in_order.end() && window_end->first < time) {
      bytes_in_window += window_end->second;
      ++window_end;
    }
    while (window_begin != packets_in_order.end() &&
           window_begin->first < time - window_duration_) {
      RTC_DCHECK_LE(window_begin->second, bytes_in_window);
      bytes_in_window -= window_begin->second;
      ++window_begin;
    }
    float window_duration_in_seconds =
        static_cast<float>(window_duration_) / kNumMicrosecsPerSec;
    float x = ToCallTimeSec(time);
    float y = bytes_in_window * 8 / window_duration_in_seconds / 1000;
    bitrate_series.points.emplace_back(x, y);
  }
  plot->AppendTimeSeries(std::move(bitrate_series));

  // Overlay the send-side bandwidth estimate over the outgoing bitrate.
  TimeSeries loss_series("Loss-based estimate", LineStyle::kStep);
  for (auto& loss_update : parsed_log_.bwe_loss_updates()) {
    float x = ToCallTimeSec(loss_update.log_time_us());
    float y = static_cast<float>(loss_update.bitrate_bps) / 1000;
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

  for (auto& delay_update : parsed_log_.bwe_delay_updates()) {
    float x = ToCallTimeSec(delay_update.log_time_us());
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
  for (auto& cluster : parsed_log_.bwe_probe_cluster_created_events()) {
    float x = ToCallTimeSec(cluster.log_time_us());
    float y = static_cast<float>(cluster.bitrate_bps) / 1000;
    created_series.points.emplace_back(x, y);
  }

  TimeSeries result_series("Probing results.", LineStyle::kNone,
                           PointStyle::kHighlight);
  for (auto& result : parsed_log_.bwe_probe_success_events()) {
    float x = ToCallTimeSec(result.log_time_us());
    float y = static_cast<float>(result.bitrate_bps) / 1000;
    result_series.points.emplace_back(x, y);
  }

  IntervalSeries alr_state("ALR", "#555555", IntervalSeries::kHorizontal);
  bool previously_in_alr = false;
  int64_t alr_start = 0;
  for (auto& alr : parsed_log_.alr_state_events()) {
    float y = ToCallTimeSec(alr.log_time_us());
    if (!previously_in_alr && alr.in_alr) {
      alr_start = alr.log_time_us();
      previously_in_alr = true;
    } else if (previously_in_alr && !alr.in_alr) {
      float x = ToCallTimeSec(alr_start);
      alr_state.intervals.emplace_back(x, y);
      previously_in_alr = false;
    }
  }

  if (previously_in_alr) {
    float x = ToCallTimeSec(alr_start);
    float y = ToCallTimeSec(end_time_);
    alr_state.intervals.emplace_back(x, y);
  }

  if (show_detector_state) {
    plot->AppendIntervalSeries(std::move(overusing_series));
    plot->AppendIntervalSeries(std::move(underusing_series));
    plot->AppendIntervalSeries(std::move(normal_series));
  }

  if (show_alr_state) {
    plot->AppendIntervalSeries(std::move(alr_state));
  }
  plot->AppendTimeSeries(std::move(loss_series));
  plot->AppendTimeSeries(std::move(delay_series));
  plot->AppendTimeSeries(std::move(created_series));
  plot->AppendTimeSeries(std::move(result_series));

  // Overlay the incoming REMB over the outgoing bitrate.
  TimeSeries remb_series("Remb", LineStyle::kStep);
  for (const auto& rtcp : parsed_log_.rembs(kIncomingPacket)) {
    float x = ToCallTimeSec(rtcp.log_time_us());
    float y = static_cast<float>(rtcp.remb.bitrate_bps()) / 1000;
    remb_series.points.emplace_back(x, y);
  }
  plot->AppendTimeSeriesIfNotEmpty(std::move(remb_series));

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Outgoing RTP bitrate");
}

// For each SSRC, plot the bandwidth used by that stream.
void EventLogAnalyzer::CreateStreamBitrateGraph(PacketDirection direction,
                                                Plot* plot) {
  for (const auto& stream : parsed_log_.rtp_packets_by_ssrc(direction)) {
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName(direction, stream.ssrc),
                           LineStyle::kLine);
    auto GetPacketSizeKilobits = [](const LoggedRtpPacket& packet) {
      return packet.total_length * 8.0 / 1000.0;
    };
    auto ToCallTime = [this](int64_t time) {
      return this->ToCallTimeSec(time);
    };
    MovingAverage<LoggedRtpPacket, double>(
        ToCallTime, GetPacketSizeKilobits, stream.packet_view, begin_time_,
        end_time_, window_duration_, step_, &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle(GetDirectionAsString(direction) + " bitrate per stream");
}

void EventLogAnalyzer::CreateSendSideBweSimulationGraph(Plot* plot) {
  using RtpPacketType = LoggedRtpPacketOutgoing;
  using TransportFeedbackType = LoggedRtcpPacketTransportFeedback;

  // TODO(terelius): This could be provided by the parser.
  std::multimap<int64_t, const RtpPacketType*> outgoing_rtp;
  for (const auto& stream : parsed_log_.outgoing_rtp_packets_by_ssrc()) {
    for (const RtpPacketType& rtp_packet : stream.outgoing_packets)
      outgoing_rtp.insert(
          std::make_pair(rtp_packet.rtp.log_time_us(), &rtp_packet));
  }

  const std::vector<TransportFeedbackType>& incoming_rtcp =
      parsed_log_.transport_feedbacks(kIncomingPacket);

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
      return static_cast<int64_t>(rtcp_iterator->log_time_us());
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
      absl::make_unique<BitrateEstimator>());
#endif  // !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
  int64_t time_us = std::min(NextRtpTime(), NextRtcpTime());
  int64_t last_update_us = 0;
  while (time_us != std::numeric_limits<int64_t>::max()) {
    clock.AdvanceTimeMicroseconds(time_us - clock.TimeInMicroseconds());
    if (clock.TimeInMicroseconds() >= NextRtcpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtcpTime());
      cc.OnTransportFeedback(rtcp_iterator->transport_feedback);
      std::vector<PacketFeedback> feedback = cc.GetTransportFeedbackVector();
      SortPacketFeedbackVector(&feedback);
      absl::optional<uint32_t> bitrate_bps;
      if (!feedback.empty()) {
#if !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
        acknowledged_bitrate_estimator.IncomingPacketFeedbackVector(feedback);
#endif  // !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
        for (const PacketFeedback& packet : feedback)
          acked_bitrate.Update(packet.payload_size, packet.arrival_time_ms);
        bitrate_bps = acked_bitrate.Rate(feedback.back().arrival_time_ms);
      }
      float x = ToCallTimeSec(clock.TimeInMicroseconds());
      float y = bitrate_bps.value_or(0) / 1000;
      acked_time_series.points.emplace_back(x, y);
#if !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
      y = acknowledged_bitrate_estimator.bitrate_bps().value_or(0) / 1000;
      acked_estimate_time_series.points.emplace_back(x, y);
#endif  // !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
      ++rtcp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextRtpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtpTime());
      const RtpPacketType& rtp_packet = *rtp_iterator->second;
      if (rtp_packet.rtp.header.extension.hasTransportSequenceNumber) {
        RTC_DCHECK(rtp_packet.rtp.header.extension.hasTransportSequenceNumber);
        cc.AddPacket(rtp_packet.rtp.header.ssrc,
                     rtp_packet.rtp.header.extension.transportSequenceNumber,
                     rtp_packet.rtp.total_length, PacedPacketInfo());
        rtc::SentPacket sent_packet(
            rtp_packet.rtp.header.extension.transportSequenceNumber,
            rtp_packet.rtp.log_time_us() / 1000);
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
      float x = ToCallTimeSec(clock.TimeInMicroseconds());
      time_series.points.emplace_back(x, y);
      last_update_us = time_us;
    }
    time_us = std::min({NextRtpTime(), NextRtcpTime(), NextProcessTime()});
  }
  // Add the data set to the plot.
  plot->AppendTimeSeries(std::move(time_series));
  plot->AppendTimeSeries(std::move(acked_time_series));
  plot->AppendTimeSeriesIfNotEmpty(std::move(acked_estimate_time_series));

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Simulated send-side BWE behavior");
}

void EventLogAnalyzer::CreateReceiveSideBweSimulationGraph(Plot* plot) {
  using RtpPacketType = LoggedRtpPacketIncoming;
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

  std::multimap<int64_t, const RtpPacketType*> incoming_rtp;

  for (const auto& stream : parsed_log_.incoming_rtp_packets_by_ssrc()) {
    if (IsVideoSsrc(kIncomingPacket, stream.ssrc)) {
      for (const auto& rtp_packet : stream.incoming_packets)
        incoming_rtp.insert(
            std::make_pair(rtp_packet.rtp.log_time_us(), &rtp_packet));
    }
  }

  SimulatedClock clock(0);
  RembInterceptingPacketRouter packet_router;
  // TODO(terelius): The PacketRouter is used as the RemoteBitrateObserver.
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
    const RtpPacketType& packet = *kv.second;
    int64_t arrival_time_ms = packet.rtp.log_time_us() / 1000;
    size_t payload = packet.rtp.total_length; /*Should subtract header?*/
    clock.AdvanceTimeMicroseconds(packet.rtp.log_time_us() -
                                  clock.TimeInMicroseconds());
    rscc.OnReceivedPacket(arrival_time_ms, payload, packet.rtp.header);
    acked_bitrate.Update(payload, arrival_time_ms);
    absl::optional<uint32_t> bitrate_bps = acked_bitrate.Rate(arrival_time_ms);
    if (bitrate_bps) {
      uint32_t y = *bitrate_bps / 1000;
      float x = ToCallTimeSec(clock.TimeInMicroseconds());
      acked_time_series.points.emplace_back(x, y);
    }
    if (packet_router.GetAndResetBitrateUpdated() ||
        clock.TimeInMicroseconds() - last_update_us >= 1e6) {
      uint32_t y = packet_router.last_bitrate_bps() / 1000;
      float x = ToCallTimeSec(clock.TimeInMicroseconds());
      time_series.points.emplace_back(x, y);
      last_update_us = clock.TimeInMicroseconds();
    }
  }
  // Add the data set to the plot.
  plot->AppendTimeSeries(std::move(time_series));
  plot->AppendTimeSeries(std::move(acked_time_series));

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Simulated receive-side BWE behavior");
}

void EventLogAnalyzer::CreateNetworkDelayFeedbackGraph(Plot* plot) {
  TimeSeries late_feedback_series("Late feedback results.", LineStyle::kNone,
                                  PointStyle::kHighlight);
  TimeSeries time_series("Network Delay Change", LineStyle::kLine,
                         PointStyle::kHighlight);
  int64_t estimated_base_delay_ms = std::numeric_limits<int64_t>::max();

  int64_t prev_y = 0;
  for (auto packet : GetNetworkTrace(parsed_log_)) {
    if (packet.arrival_time_ms == PacketFeedback::kNotReceived)
      continue;
    float x = ToCallTimeSec(1000 * packet.feedback_arrival_time_ms);
    if (packet.send_time_ms == PacketFeedback::kNoSendTime) {
      late_feedback_series.points.emplace_back(x, prev_y);
      continue;
    }
    int64_t y = packet.arrival_time_ms - packet.send_time_ms;
    prev_y = y;
    estimated_base_delay_ms = std::min(y, estimated_base_delay_ms);
    time_series.points.emplace_back(x, y);
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

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Delay (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle("Network Delay Change.");
}

void EventLogAnalyzer::CreatePacerDelayGraph(Plot* plot) {
  for (const auto& stream : parsed_log_.outgoing_rtp_packets_by_ssrc()) {
    const std::vector<LoggedRtpPacketOutgoing>& packets =
        stream.outgoing_packets;

    if (packets.size() < 2) {
      RTC_LOG(LS_WARNING)
          << "Can't estimate a the RTP clock frequency or the "
             "pacer delay with less than 2 packets in the stream";
      continue;
    }
    int64_t end_time_us = log_segments_.empty()
                              ? std::numeric_limits<int64_t>::max()
                              : log_segments_.front().second;
    absl::optional<uint32_t> estimated_frequency =
        EstimateRtpClockFrequency(packets, end_time_us);
    if (!estimated_frequency)
      continue;
    if (IsVideoSsrc(kOutgoingPacket, stream.ssrc) &&
        *estimated_frequency != 90000) {
      RTC_LOG(LS_WARNING)
          << "Video stream should use a 90 kHz clock but appears to use "
          << *estimated_frequency / 1000 << ". Discarding.";
      continue;
    }

    TimeSeries pacer_delay_series(
        GetStreamName(kOutgoingPacket, stream.ssrc) + "(" +
            std::to_string(*estimated_frequency / 1000) + " kHz)",
        LineStyle::kLine, PointStyle::kHighlight);
    SeqNumUnwrapper<uint32_t> timestamp_unwrapper;
    uint64_t first_capture_timestamp =
        timestamp_unwrapper.Unwrap(packets.front().rtp.header.timestamp);
    uint64_t first_send_timestamp = packets.front().rtp.log_time_us();
    for (const auto& packet : packets) {
      double capture_time_ms = (static_cast<double>(timestamp_unwrapper.Unwrap(
                                    packet.rtp.header.timestamp)) -
                                first_capture_timestamp) /
                               *estimated_frequency * 1000;
      double send_time_ms =
          static_cast<double>(packet.rtp.log_time_us() - first_send_timestamp) /
          1000;
      float x = ToCallTimeSec(packet.rtp.log_time_us());
      float y = send_time_ms - capture_time_ms;
      pacer_delay_series.points.emplace_back(x, y);
    }
    plot->AppendTimeSeries(std::move(pacer_delay_series));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Pacer delay (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle(
      "Delay from capture to send time. (First packet normalized to 0.)");
}

void EventLogAnalyzer::CreateTimestampGraph(PacketDirection direction,
                                            Plot* plot) {
  for (const auto& stream : parsed_log_.rtp_packets_by_ssrc(direction)) {
    TimeSeries rtp_timestamps(
        GetStreamName(direction, stream.ssrc) + " capture-time",
        LineStyle::kLine, PointStyle::kHighlight);
    for (const auto& packet : stream.packet_view) {
      float x = ToCallTimeSec(packet.log_time_us());
      float y = packet.header.timestamp;
      rtp_timestamps.points.emplace_back(x, y);
    }
    plot->AppendTimeSeries(std::move(rtp_timestamps));

    TimeSeries rtcp_timestamps(
        GetStreamName(direction, stream.ssrc) + " rtcp capture-time",
        LineStyle::kLine, PointStyle::kHighlight);
    // TODO(terelius): Why only sender reports?
    const auto& sender_reports = parsed_log_.sender_reports(direction);
    for (const auto& rtcp : sender_reports) {
      if (rtcp.sr.sender_ssrc() != stream.ssrc)
        continue;
      float x = ToCallTimeSec(rtcp.log_time_us());
      float y = rtcp.sr.rtp_timestamp();
      rtcp_timestamps.points.emplace_back(x, y);
    }
    plot->AppendTimeSeriesIfNotEmpty(std::move(rtcp_timestamps));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "RTP timestamp", kBottomMargin, kTopMargin);
  plot->SetTitle(GetDirectionAsString(direction) + " timestamps");
}

void EventLogAnalyzer::CreateSenderAndReceiverReportPlot(
    PacketDirection direction,
    rtc::FunctionView<float(const rtcp::ReportBlock&)> fy,
    std::string title,
    std::string yaxis_label,
    Plot* plot) {
  std::map<uint32_t, TimeSeries> sr_reports_by_ssrc;
  const auto& sender_reports = parsed_log_.sender_reports(direction);
  for (const auto& rtcp : sender_reports) {
    float x = ToCallTimeSec(rtcp.log_time_us());
    uint32_t ssrc = rtcp.sr.sender_ssrc();
    for (const auto& block : rtcp.sr.report_blocks()) {
      float y = fy(block);
      auto sr_report_it = sr_reports_by_ssrc.find(ssrc);
      bool inserted;
      if (sr_report_it == sr_reports_by_ssrc.end()) {
        std::tie(sr_report_it, inserted) = sr_reports_by_ssrc.emplace(
            ssrc, TimeSeries(GetStreamName(direction, ssrc) + " Sender Reports",
                             LineStyle::kLine, PointStyle::kHighlight));
      }
      sr_report_it->second.points.emplace_back(x, y);
    }
  }
  for (auto& kv : sr_reports_by_ssrc) {
    plot->AppendTimeSeries(std::move(kv.second));
  }

  std::map<uint32_t, TimeSeries> rr_reports_by_ssrc;
  const auto& receiver_reports = parsed_log_.receiver_reports(direction);
  for (const auto& rtcp : receiver_reports) {
    float x = ToCallTimeSec(rtcp.log_time_us());
    uint32_t ssrc = rtcp.rr.sender_ssrc();
    for (const auto& block : rtcp.rr.report_blocks()) {
      float y = fy(block);
      auto rr_report_it = rr_reports_by_ssrc.find(ssrc);
      bool inserted;
      if (rr_report_it == rr_reports_by_ssrc.end()) {
        std::tie(rr_report_it, inserted) = rr_reports_by_ssrc.emplace(
            ssrc,
            TimeSeries(GetStreamName(direction, ssrc) + " Receiver Reports",
                       LineStyle::kLine, PointStyle::kHighlight));
      }
      rr_report_it->second.points.emplace_back(x, y);
    }
  }
  for (auto& kv : rr_reports_by_ssrc) {
    plot->AppendTimeSeries(std::move(kv.second));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, yaxis_label, kBottomMargin, kTopMargin);
  plot->SetTitle(title);
}

void EventLogAnalyzer::CreateAudioEncoderTargetBitrateGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder target bitrate", LineStyle::kLine,
                         PointStyle::kHighlight);
  auto GetAnaBitrateBps = [](const LoggedAudioNetworkAdaptationEvent& ana_event)
      -> absl::optional<float> {
    if (ana_event.config.bitrate_bps)
      return absl::optional<float>(
          static_cast<float>(*ana_event.config.bitrate_bps));
    return absl::nullopt;
  };
  auto ToCallTime = [this](const LoggedAudioNetworkAdaptationEvent& packet) {
    return this->ToCallTimeSec(packet.log_time_us());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaBitrateBps,
      parsed_log_.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (bps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder target bitrate");
}

void EventLogAnalyzer::CreateAudioEncoderFrameLengthGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder frame length", LineStyle::kLine,
                         PointStyle::kHighlight);
  auto GetAnaFrameLengthMs =
      [](const LoggedAudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.frame_length_ms)
          return absl::optional<float>(
              static_cast<float>(*ana_event.config.frame_length_ms));
        return absl::optional<float>();
      };
  auto ToCallTime = [this](const LoggedAudioNetworkAdaptationEvent& packet) {
    return this->ToCallTimeSec(packet.log_time_us());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaFrameLengthMs,
      parsed_log_.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Frame length (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder frame length");
}

void EventLogAnalyzer::CreateAudioEncoderPacketLossGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder uplink packet loss fraction",
                         LineStyle::kLine, PointStyle::kHighlight);
  auto GetAnaPacketLoss =
      [](const LoggedAudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.uplink_packet_loss_fraction)
          return absl::optional<float>(static_cast<float>(
              *ana_event.config.uplink_packet_loss_fraction));
        return absl::optional<float>();
      };
  auto ToCallTime = [this](const LoggedAudioNetworkAdaptationEvent& packet) {
    return this->ToCallTimeSec(packet.log_time_us());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaPacketLoss,
      parsed_log_.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Percent lost packets", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Reported audio encoder lost packets");
}

void EventLogAnalyzer::CreateAudioEncoderEnableFecGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder FEC", LineStyle::kLine,
                         PointStyle::kHighlight);
  auto GetAnaFecEnabled =
      [](const LoggedAudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.enable_fec)
          return absl::optional<float>(
              static_cast<float>(*ana_event.config.enable_fec));
        return absl::optional<float>();
      };
  auto ToCallTime = [this](const LoggedAudioNetworkAdaptationEvent& packet) {
    return this->ToCallTimeSec(packet.log_time_us());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaFecEnabled,
      parsed_log_.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "FEC (false/true)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder FEC");
}

void EventLogAnalyzer::CreateAudioEncoderEnableDtxGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder DTX", LineStyle::kLine,
                         PointStyle::kHighlight);
  auto GetAnaDtxEnabled =
      [](const LoggedAudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.enable_dtx)
          return absl::optional<float>(
              static_cast<float>(*ana_event.config.enable_dtx));
        return absl::optional<float>();
      };
  auto ToCallTime = [this](const LoggedAudioNetworkAdaptationEvent& packet) {
    return this->ToCallTimeSec(packet.log_time_us());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaDtxEnabled,
      parsed_log_.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "DTX (false/true)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder DTX");
}

void EventLogAnalyzer::CreateAudioEncoderNumChannelsGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder number of channels", LineStyle::kLine,
                         PointStyle::kHighlight);
  auto GetAnaNumChannels =
      [](const LoggedAudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.num_channels)
          return absl::optional<float>(
              static_cast<float>(*ana_event.config.num_channels));
        return absl::optional<float>();
      };
  auto ToCallTime = [this](const LoggedAudioNetworkAdaptationEvent& packet) {
    return this->ToCallTimeSec(packet.log_time_us());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaNumChannels,
      parsed_log_.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Number of channels (1 (mono)/2 (stereo))",
                          kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder number of channels");
}

class NetEqStreamInput : public test::NetEqInput {
 public:
  // Does not take any ownership, and all pointers must refer to valid objects
  // that outlive the one constructed.
  NetEqStreamInput(const std::vector<LoggedRtpPacketIncoming>* packet_stream,
                   const std::vector<LoggedAudioPlayoutEvent>* output_events,
                   absl::optional<int64_t> end_time_ms)
      : packet_stream_(*packet_stream),
        packet_stream_it_(packet_stream_.begin()),
        output_events_it_(output_events->begin()),
        output_events_end_(output_events->end()),
        end_time_ms_(end_time_ms) {
    RTC_DCHECK(packet_stream);
    RTC_DCHECK(output_events);
  }

  absl::optional<int64_t> NextPacketTime() const override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return absl::nullopt;
    }
    if (end_time_ms_ && packet_stream_it_->rtp.log_time_ms() > *end_time_ms_) {
      return absl::nullopt;
    }
    return packet_stream_it_->rtp.log_time_ms();
  }

  absl::optional<int64_t> NextOutputEventTime() const override {
    if (output_events_it_ == output_events_end_) {
      return absl::nullopt;
    }
    if (end_time_ms_ && output_events_it_->log_time_ms() > *end_time_ms_) {
      return absl::nullopt;
    }
    return output_events_it_->log_time_ms();
  }

  std::unique_ptr<PacketData> PopPacket() override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return std::unique_ptr<PacketData>();
    }
    std::unique_ptr<PacketData> packet_data(new PacketData());
    packet_data->header = packet_stream_it_->rtp.header;
    packet_data->time_ms = packet_stream_it_->rtp.log_time_ms();

    // This is a header-only "dummy" packet. Set the payload to all zeros, with
    // length according to the virtual length.
    packet_data->payload.SetSize(packet_stream_it_->rtp.total_length -
                                 packet_stream_it_->rtp.header_length);
    std::fill_n(packet_data->payload.data(), packet_data->payload.size(), 0);

    ++packet_stream_it_;
    return packet_data;
  }

  void AdvanceOutputEvent() override {
    if (output_events_it_ != output_events_end_) {
      ++output_events_it_;
    }
  }

  bool ended() const override { return !NextEventTime(); }

  absl::optional<RTPHeader> NextHeader() const override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return absl::nullopt;
    }
    return packet_stream_it_->rtp.header;
  }

 private:
  const std::vector<LoggedRtpPacketIncoming>& packet_stream_;
  std::vector<LoggedRtpPacketIncoming>::const_iterator packet_stream_it_;
  std::vector<LoggedAudioPlayoutEvent>::const_iterator output_events_it_;
  const std::vector<LoggedAudioPlayoutEvent>::const_iterator output_events_end_;
  const absl::optional<int64_t> end_time_ms_;
};

namespace {
// Creates a NetEq test object and all necessary input and output helpers. Runs
// the test and returns the NetEqDelayAnalyzer object that was used to
// instrument the test.
std::unique_ptr<test::NetEqStatsGetter> CreateNetEqTestAndRun(
    const std::vector<LoggedRtpPacketIncoming>* packet_stream,
    const std::vector<LoggedAudioPlayoutEvent>* output_events,
    absl::optional<int64_t> end_time_ms,
    const std::string& replacement_file_name,
    int file_sample_rate_hz) {
  std::unique_ptr<test::NetEqInput> input(
      new NetEqStreamInput(packet_stream, output_events, end_time_ms));

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
  std::unique_ptr<test::NetEqStatsGetter> neteq_stats_getter(
      new test::NetEqStatsGetter(std::move(delay_cb)));
  test::DefaultNetEqTestErrorCallback error_cb;
  test::NetEqTest::Callbacks callbacks;
  callbacks.error_callback = &error_cb;
  callbacks.post_insert_packet = neteq_stats_getter->delay_analyzer();
  callbacks.get_audio_callback = neteq_stats_getter.get();

  test::NetEqTest test(config, codecs, ext_codecs, std::move(input),
                       std::move(output), callbacks);
  test.Run();
  return neteq_stats_getter;
}
}  // namespace

EventLogAnalyzer::NetEqStatsGetterMap EventLogAnalyzer::SimulateNetEq(
    const std::string& replacement_file_name,
    int file_sample_rate_hz) const {
  NetEqStatsGetterMap neteq_stats;

  for (const auto& stream : parsed_log_.incoming_rtp_packets_by_ssrc()) {
    const uint32_t ssrc = stream.ssrc;
    if (!IsAudioSsrc(kIncomingPacket, ssrc))
      continue;
    const std::vector<LoggedRtpPacketIncoming>* audio_packets =
        &stream.incoming_packets;
    if (audio_packets == nullptr) {
      // No incoming audio stream found.
      continue;
    }

    RTC_DCHECK(neteq_stats.find(ssrc) == neteq_stats.end());

    std::map<uint32_t, std::vector<LoggedAudioPlayoutEvent>>::const_iterator
        output_events_it = parsed_log_.audio_playout_events().find(ssrc);
    if (output_events_it == parsed_log_.audio_playout_events().end()) {
      // Could not find output events with SSRC matching the input audio stream.
      // Using the first available stream of output events.
      output_events_it = parsed_log_.audio_playout_events().cbegin();
    }

    absl::optional<int64_t> end_time_ms =
        log_segments_.empty()
            ? absl::nullopt
            : absl::optional<int64_t>(log_segments_.front().second / 1000);

    neteq_stats[ssrc] = CreateNetEqTestAndRun(
        audio_packets, &output_events_it->second, end_time_ms,
        replacement_file_name, file_sample_rate_hz);
  }

  return neteq_stats;
}

// Given a NetEqStatsGetter and the SSRC that the NetEqStatsGetter was created
// for, this method generates a plot for the jitter buffer delay profile.
void EventLogAnalyzer::CreateAudioJitterBufferGraph(
    uint32_t ssrc,
    const test::NetEqStatsGetter* stats_getter,
    Plot* plot) const {
  test::NetEqDelayAnalyzer::Delays arrival_delay_ms;
  test::NetEqDelayAnalyzer::Delays corrected_arrival_delay_ms;
  test::NetEqDelayAnalyzer::Delays playout_delay_ms;
  test::NetEqDelayAnalyzer::Delays target_delay_ms;

  stats_getter->delay_analyzer()->CreateGraphs(
      &arrival_delay_ms, &corrected_arrival_delay_ms, &playout_delay_ms,
      &target_delay_ms);

  TimeSeries time_series_packet_arrival("packet arrival delay",
                                        LineStyle::kLine);
  TimeSeries time_series_relative_packet_arrival(
      "Relative packet arrival delay", LineStyle::kLine);
  TimeSeries time_series_play_time("Playout delay", LineStyle::kLine);
  TimeSeries time_series_target_time("Target delay", LineStyle::kLine,
                                     PointStyle::kHighlight);

  for (const auto& data : arrival_delay_ms) {
    const float x = ToCallTimeSec(data.first * 1000);  // ms to us.
    const float y = data.second;
    time_series_packet_arrival.points.emplace_back(TimeSeriesPoint(x, y));
  }
  for (const auto& data : corrected_arrival_delay_ms) {
    const float x = ToCallTimeSec(data.first * 1000);  // ms to us.
    const float y = data.second;
    time_series_relative_packet_arrival.points.emplace_back(
        TimeSeriesPoint(x, y));
  }
  for (const auto& data : playout_delay_ms) {
    const float x = ToCallTimeSec(data.first * 1000);  // ms to us.
    const float y = data.second;
    time_series_play_time.points.emplace_back(TimeSeriesPoint(x, y));
  }
  for (const auto& data : target_delay_ms) {
    const float x = ToCallTimeSec(data.first * 1000);  // ms to us.
    const float y = data.second;
    time_series_target_time.points.emplace_back(TimeSeriesPoint(x, y));
  }

  plot->AppendTimeSeries(std::move(time_series_packet_arrival));
  plot->AppendTimeSeries(std::move(time_series_relative_packet_arrival));
  plot->AppendTimeSeries(std::move(time_series_play_time));
  plot->AppendTimeSeries(std::move(time_series_target_time));

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Relative delay (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("NetEq timing for " + GetStreamName(kIncomingPacket, ssrc));
}

template <typename NetEqStatsType>
void EventLogAnalyzer::CreateNetEqStatsGraphInternal(
    const NetEqStatsGetterMap& neteq_stats,
    rtc::FunctionView<const std::vector<std::pair<int64_t, NetEqStatsType>>*(
        const test::NetEqStatsGetter*)> data_extractor,
    rtc::FunctionView<float(const NetEqStatsType&)> stats_extractor,
    const std::string& plot_name,
    Plot* plot) const {
  std::map<uint32_t, TimeSeries> time_series;

  for (const auto& st : neteq_stats) {
    const uint32_t ssrc = st.first;
    const std::vector<std::pair<int64_t, NetEqStatsType>>* data_vector =
        data_extractor(st.second.get());
    for (const auto& data : *data_vector) {
      const float time = ToCallTimeSec(data.first * 1000);  // ms to us.
      const float value = stats_extractor(data.second);
      time_series[ssrc].points.emplace_back(TimeSeriesPoint(time, value));
    }
  }

  for (auto& series : time_series) {
    series.second.label = GetStreamName(kIncomingPacket, series.first);
    series.second.line_style = LineStyle::kLine;
    plot->AppendTimeSeries(std::move(series.second));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, plot_name, kBottomMargin, kTopMargin);
  plot->SetTitle(plot_name);
}

void EventLogAnalyzer::CreateNetEqNetworkStatsGraph(
    const NetEqStatsGetterMap& neteq_stats,
    rtc::FunctionView<float(const NetEqNetworkStatistics&)> stats_extractor,
    const std::string& plot_name,
    Plot* plot) const {
  CreateNetEqStatsGraphInternal<NetEqNetworkStatistics>(
      neteq_stats,
      [](const test::NetEqStatsGetter* stats_getter) {
        return stats_getter->stats();
      },
      stats_extractor, plot_name, plot);
}

void EventLogAnalyzer::CreateNetEqLifetimeStatsGraph(
    const NetEqStatsGetterMap& neteq_stats,
    rtc::FunctionView<float(const NetEqLifetimeStatistics&)> stats_extractor,
    const std::string& plot_name,
    Plot* plot) const {
  CreateNetEqStatsGraphInternal<NetEqLifetimeStatistics>(
      neteq_stats,
      [](const test::NetEqStatsGetter* stats_getter) {
        return stats_getter->lifetime_stats();
      },
      stats_extractor, plot_name, plot);
}

void EventLogAnalyzer::CreateIceCandidatePairConfigGraph(Plot* plot) {
  std::map<uint32_t, TimeSeries> configs_by_cp_id;
  for (const auto& config : parsed_log_.ice_candidate_pair_configs()) {
    if (configs_by_cp_id.find(config.candidate_pair_id) ==
        configs_by_cp_id.end()) {
      const std::string candidate_pair_desc =
          GetCandidatePairLogDescriptionAsString(config);
      configs_by_cp_id[config.candidate_pair_id] =
          TimeSeries("[" + std::to_string(config.candidate_pair_id) + "]" +
                         candidate_pair_desc,
                     LineStyle::kNone, PointStyle::kHighlight);
      candidate_pair_desc_by_id_[config.candidate_pair_id] =
          candidate_pair_desc;
    }
    float x = ToCallTimeSec(config.log_time_us());
    float y = static_cast<float>(config.type);
    configs_by_cp_id[config.candidate_pair_id].points.emplace_back(x, y);
  }

  // TODO(qingsi): There can be a large number of candidate pairs generated by
  // certain calls and the frontend cannot render the chart in this case due to
  // the failure of generating a palette with the same number of colors.
  for (auto& kv : configs_by_cp_id) {
    plot->AppendTimeSeries(std::move(kv.second));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 3, "Numeric Config Type", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("[IceEventLog] ICE candidate pair configs");
}

std::string EventLogAnalyzer::GetCandidatePairLogDescriptionFromId(
    uint32_t candidate_pair_id) {
  if (candidate_pair_desc_by_id_.find(candidate_pair_id) !=
      candidate_pair_desc_by_id_.end()) {
    return candidate_pair_desc_by_id_[candidate_pair_id];
  }
  for (const auto& config : parsed_log_.ice_candidate_pair_configs()) {
    // TODO(qingsi): Add the handling of the "Updated" config event after the
    // visualization of property change for candidate pairs is introduced.
    if (candidate_pair_desc_by_id_.find(config.candidate_pair_id) ==
        candidate_pair_desc_by_id_.end()) {
      const std::string candidate_pair_desc =
          GetCandidatePairLogDescriptionAsString(config);
      candidate_pair_desc_by_id_[config.candidate_pair_id] =
          candidate_pair_desc;
    }
  }
  return candidate_pair_desc_by_id_[candidate_pair_id];
}

void EventLogAnalyzer::CreateIceConnectivityCheckGraph(Plot* plot) {
  std::map<uint32_t, TimeSeries> checks_by_cp_id;
  for (const auto& event : parsed_log_.ice_candidate_pair_events()) {
    if (checks_by_cp_id.find(event.candidate_pair_id) ==
        checks_by_cp_id.end()) {
      checks_by_cp_id[event.candidate_pair_id] = TimeSeries(
          "[" + std::to_string(event.candidate_pair_id) + "]" +
              GetCandidatePairLogDescriptionFromId(event.candidate_pair_id),
          LineStyle::kNone, PointStyle::kHighlight);
    }
    float x = ToCallTimeSec(event.log_time_us());
    float y = static_cast<float>(event.type);
    checks_by_cp_id[event.candidate_pair_id].points.emplace_back(x, y);
  }

  // TODO(qingsi): The same issue as in CreateIceCandidatePairConfigGraph.
  for (auto& kv : checks_by_cp_id) {
    plot->AppendTimeSeries(std::move(kv.second));
  }

  plot->SetXAxis(ToCallTimeSec(begin_time_), call_duration_s_, "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 4, "Numeric Connectivity State", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("[IceEventLog] ICE connectivity checks");
}

void EventLogAnalyzer::PrintNotifications(FILE* file) {
  fprintf(file, "========== TRIAGE NOTIFICATIONS ==========\n");
  for (const auto& alert : incoming_rtp_recv_time_gaps_) {
    fprintf(file, "%3.3lf s : %s\n", alert.Time(), alert.ToString().c_str());
  }
  for (const auto& alert : incoming_rtcp_recv_time_gaps_) {
    fprintf(file, "%3.3lf s : %s\n", alert.Time(), alert.ToString().c_str());
  }
  for (const auto& alert : outgoing_rtp_send_time_gaps_) {
    fprintf(file, "%3.3lf s : %s\n", alert.Time(), alert.ToString().c_str());
  }
  for (const auto& alert : outgoing_rtcp_send_time_gaps_) {
    fprintf(file, "%3.3lf s : %s\n", alert.Time(), alert.ToString().c_str());
  }
  for (const auto& alert : incoming_seq_num_jumps_) {
    fprintf(file, "%3.3lf s : %s\n", alert.Time(), alert.ToString().c_str());
  }
  for (const auto& alert : incoming_capture_time_jumps_) {
    fprintf(file, "%3.3lf s : %s\n", alert.Time(), alert.ToString().c_str());
  }
  for (const auto& alert : outgoing_seq_num_jumps_) {
    fprintf(file, "%3.3lf s : %s\n", alert.Time(), alert.ToString().c_str());
  }
  for (const auto& alert : outgoing_capture_time_jumps_) {
    fprintf(file, "%3.3lf s : %s\n", alert.Time(), alert.ToString().c_str());
  }
  for (const auto& alert : outgoing_high_loss_alerts_) {
    fprintf(file, "          : %s\n", alert.ToString().c_str());
  }
  fprintf(file, "========== END TRIAGE NOTIFICATIONS ==========\n");
}

void EventLogAnalyzer::CreateStreamGapAlerts(PacketDirection direction) {
  // With 100 packets/s (~800kbps), false positives would require 10 s without
  // data.
  constexpr int64_t kMaxSeqNumJump = 1000;
  // With a 90 kHz clock, false positives would require 10 s without data.
  constexpr int64_t kMaxCaptureTimeJump = 900000;

  int64_t end_time_us = log_segments_.empty()
                            ? std::numeric_limits<int64_t>::max()
                            : log_segments_.front().second;

  SeqNumUnwrapper<uint16_t> seq_num_unwrapper;
  absl::optional<int64_t> last_seq_num;
  SeqNumUnwrapper<uint32_t> capture_time_unwrapper;
  absl::optional<int64_t> last_capture_time;
  // Check for gaps in sequence numbers and capture timestamps.
  for (const auto& stream : parsed_log_.rtp_packets_by_ssrc(direction)) {
    for (const auto& packet : stream.packet_view) {
      if (packet.log_time_us() > end_time_us) {
        // Only process the first (LOG_START, LOG_END) segment.
        break;
      }

      int64_t seq_num = seq_num_unwrapper.Unwrap(packet.header.sequenceNumber);
      if (last_seq_num.has_value() &&
          std::abs(seq_num - last_seq_num.value()) > kMaxSeqNumJump) {
        Alert_SeqNumJump(direction, ToCallTimeSec(packet.log_time_us()),
                         packet.header.ssrc);
      }
      last_seq_num.emplace(seq_num);

      int64_t capture_time =
          capture_time_unwrapper.Unwrap(packet.header.timestamp);
      if (last_capture_time.has_value() &&
          std::abs(capture_time - last_capture_time.value()) >
              kMaxCaptureTimeJump) {
        Alert_CaptureTimeJump(direction, ToCallTimeSec(packet.log_time_us()),
                              packet.header.ssrc);
      }
      last_capture_time.emplace(capture_time);
    }
  }
}

void EventLogAnalyzer::CreateTransmissionGapAlerts(PacketDirection direction) {
  constexpr int64_t kMaxRtpTransmissionGap = 500000;
  constexpr int64_t kMaxRtcpTransmissionGap = 2000000;
  int64_t end_time_us = log_segments_.empty()
                            ? std::numeric_limits<int64_t>::max()
                            : log_segments_.front().second;

  // TODO(terelius): The parser could provide a list of all packets, ordered
  // by time, for each direction.
  std::multimap<int64_t, const LoggedRtpPacket*> rtp_in_direction;
  for (const auto& stream : parsed_log_.rtp_packets_by_ssrc(direction)) {
    for (const LoggedRtpPacket& rtp_packet : stream.packet_view)
      rtp_in_direction.emplace(rtp_packet.log_time_us(), &rtp_packet);
  }
  absl::optional<int64_t> last_rtp_time;
  for (const auto& kv : rtp_in_direction) {
    int64_t timestamp = kv.first;
    if (timestamp > end_time_us) {
      // Only process the first (LOG_START, LOG_END) segment.
      break;
    }
    int64_t duration = timestamp - last_rtp_time.value_or(0);
    if (last_rtp_time.has_value() && duration > kMaxRtpTransmissionGap) {
      // No packet sent/received for more than 500 ms.
      Alert_RtpLogTimeGap(direction, ToCallTimeSec(timestamp), duration / 1000);
    }
    last_rtp_time.emplace(timestamp);
  }

  absl::optional<int64_t> last_rtcp_time;
  if (direction == kIncomingPacket) {
    for (const auto& rtcp : parsed_log_.incoming_rtcp_packets()) {
      if (rtcp.log_time_us() > end_time_us) {
        // Only process the first (LOG_START, LOG_END) segment.
        break;
      }
      int64_t duration = rtcp.log_time_us() - last_rtcp_time.value_or(0);
      if (last_rtcp_time.has_value() && duration > kMaxRtcpTransmissionGap) {
        // No feedback sent/received for more than 2000 ms.
        Alert_RtcpLogTimeGap(direction, ToCallTimeSec(rtcp.log_time_us()),
                             duration / 1000);
      }
      last_rtcp_time.emplace(rtcp.log_time_us());
    }
  } else {
    for (const auto& rtcp : parsed_log_.outgoing_rtcp_packets()) {
      if (rtcp.log_time_us() > end_time_us) {
        // Only process the first (LOG_START, LOG_END) segment.
        break;
      }
      int64_t duration = rtcp.log_time_us() - last_rtcp_time.value_or(0);
      if (last_rtcp_time.has_value() && duration > kMaxRtcpTransmissionGap) {
        // No feedback sent/received for more than 2000 ms.
        Alert_RtcpLogTimeGap(direction, ToCallTimeSec(rtcp.log_time_us()),
                             duration / 1000);
      }
      last_rtcp_time.emplace(rtcp.log_time_us());
    }
  }
}

// TODO(terelius): Notifications could possibly be generated by the same code
// that produces the graphs. There is some code duplication that could be
// avoided, but that might be solved anyway when we move functionality from the
// analyzer to the parser.
void EventLogAnalyzer::CreateTriageNotifications() {
  CreateStreamGapAlerts(kIncomingPacket);
  CreateStreamGapAlerts(kOutgoingPacket);
  CreateTransmissionGapAlerts(kIncomingPacket);
  CreateTransmissionGapAlerts(kOutgoingPacket);

  int64_t end_time_us = log_segments_.empty()
                            ? std::numeric_limits<int64_t>::max()
                            : log_segments_.front().second;

  constexpr double kMaxLossFraction = 0.05;
  // Loss feedback
  int64_t total_lost_packets = 0;
  int64_t total_expected_packets = 0;
  for (auto& bwe_update : parsed_log_.bwe_loss_updates()) {
    if (bwe_update.log_time_us() > end_time_us) {
      // Only process the first (LOG_START, LOG_END) segment.
      break;
    }
    int64_t lost_packets = static_cast<double>(bwe_update.fraction_lost) / 255 *
                           bwe_update.expected_packets;
    total_lost_packets += lost_packets;
    total_expected_packets += bwe_update.expected_packets;
  }
  double avg_outgoing_loss =
      static_cast<double>(total_lost_packets) / total_expected_packets;
  if (avg_outgoing_loss > kMaxLossFraction) {
    Alert_OutgoingHighLoss(avg_outgoing_loss);
  }
}

}  // namespace webrtc
