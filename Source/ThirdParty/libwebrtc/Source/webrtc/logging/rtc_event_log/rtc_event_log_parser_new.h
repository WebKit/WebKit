/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_NEW_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_NEW_H_

#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>  // pair
#include <vector>

#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/ignore_wundef.h"

// Files generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "logging/rtc_event_log/rtc_event_log.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

namespace webrtc {

enum class BandwidthUsage;
struct AudioEncoderRuntimeConfig;

struct LoggedAlrStateEvent {
  int64_t timestamp_us;
  bool in_alr;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedAudioPlayoutEvent {
  int64_t timestamp_us;
  uint32_t ssrc;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedAudioNetworkAdaptationEvent {
  int64_t timestamp_us;
  AudioEncoderRuntimeConfig config;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedBweDelayBasedUpdate {
  int64_t timestamp_us;
  int32_t bitrate_bps;
  BandwidthUsage detector_state;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedBweLossBasedUpdate {
  int64_t timestamp_us;
  int32_t bitrate_bps;
  uint8_t fraction_lost;
  int32_t expected_packets;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedBweProbeClusterCreatedEvent {
  int64_t timestamp_us;
  int32_t id;
  int32_t bitrate_bps;
  uint32_t min_packets;
  uint32_t min_bytes;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedBweProbeSuccessEvent {
  int64_t timestamp_us;
  int32_t id;
  int32_t bitrate_bps;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedBweProbeFailureEvent {
  int64_t timestamp_us;
  int32_t id;
  ProbeFailureReason failure_reason;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedIceCandidatePairConfig {
  int64_t timestamp_us;
  IceCandidatePairConfigType type;
  uint32_t candidate_pair_id;
  IceCandidateType local_candidate_type;
  IceCandidatePairProtocol local_relay_protocol;
  IceCandidateNetworkType local_network_type;
  IceCandidatePairAddressFamily local_address_family;
  IceCandidateType remote_candidate_type;
  IceCandidatePairAddressFamily remote_address_family;
  IceCandidatePairProtocol candidate_pair_protocol;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedIceCandidatePairEvent {
  int64_t timestamp_us;
  IceCandidatePairEventType type;
  uint32_t candidate_pair_id;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedRtpPacket {
  LoggedRtpPacket(uint64_t timestamp_us,
                  RTPHeader header,
                  size_t header_length,
                  size_t total_length)
      : timestamp_us(timestamp_us),
        header(header),
        header_length(header_length),
        total_length(total_length) {}
  int64_t timestamp_us;
  // TODO(terelius): This allocates space for 15 CSRCs even if none are used.
  RTPHeader header;
  size_t header_length;
  size_t total_length;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedRtpPacketIncoming {
  LoggedRtpPacketIncoming(uint64_t timestamp_us,
                          RTPHeader header,
                          size_t header_length,
                          size_t total_length)
      : rtp(timestamp_us, header, header_length, total_length) {}
  LoggedRtpPacket rtp;
  int64_t log_time_us() const { return rtp.timestamp_us; }
  int64_t log_time_ms() const { return rtp.timestamp_us / 1000; }
};

struct LoggedRtpPacketOutgoing {
  LoggedRtpPacketOutgoing(uint64_t timestamp_us,
                          RTPHeader header,
                          size_t header_length,
                          size_t total_length)
      : rtp(timestamp_us, header, header_length, total_length) {}
  LoggedRtpPacket rtp;
  int64_t log_time_us() const { return rtp.timestamp_us; }
  int64_t log_time_ms() const { return rtp.timestamp_us / 1000; }
};

struct LoggedRtcpPacket {
  LoggedRtcpPacket(uint64_t timestamp_us,
                   const uint8_t* packet,
                   size_t total_length)
      : timestamp_us(timestamp_us), raw_data(packet, packet + total_length) {}
  int64_t timestamp_us;
  std::vector<uint8_t> raw_data;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedRtcpPacketIncoming {
  LoggedRtcpPacketIncoming(uint64_t timestamp_us,
                           const uint8_t* packet,
                           size_t total_length)
      : rtcp(timestamp_us, packet, total_length) {}
  LoggedRtcpPacket rtcp;
  int64_t log_time_us() const { return rtcp.timestamp_us; }
  int64_t log_time_ms() const { return rtcp.timestamp_us / 1000; }
};

struct LoggedRtcpPacketOutgoing {
  LoggedRtcpPacketOutgoing(uint64_t timestamp_us,
                           const uint8_t* packet,
                           size_t total_length)
      : rtcp(timestamp_us, packet, total_length) {}
  LoggedRtcpPacket rtcp;
  int64_t log_time_us() const { return rtcp.timestamp_us; }
  int64_t log_time_ms() const { return rtcp.timestamp_us / 1000; }
};

struct LoggedRtcpPacketReceiverReport {
  int64_t timestamp_us;
  rtcp::ReceiverReport rr;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedRtcpPacketSenderReport {
  int64_t timestamp_us;
  rtcp::SenderReport sr;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedRtcpPacketRemb {
  int64_t timestamp_us;
  rtcp::Remb remb;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedRtcpPacketNack {
  int64_t timestamp_us;
  rtcp::Nack nack;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedRtcpPacketTransportFeedback {
  int64_t timestamp_us;
  rtcp::TransportFeedback transport_feedback;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedStartEvent {
  explicit LoggedStartEvent(uint64_t timestamp_us)
      : timestamp_us(timestamp_us) {}
  int64_t timestamp_us;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedStopEvent {
  explicit LoggedStopEvent(uint64_t timestamp_us)
      : timestamp_us(timestamp_us) {}
  int64_t timestamp_us;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedAudioRecvConfig {
  LoggedAudioRecvConfig(int64_t timestamp_us, const rtclog::StreamConfig config)
      : timestamp_us(timestamp_us), config(config) {}
  int64_t timestamp_us;
  rtclog::StreamConfig config;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedAudioSendConfig {
  LoggedAudioSendConfig(int64_t timestamp_us, const rtclog::StreamConfig config)
      : timestamp_us(timestamp_us), config(config) {}
  int64_t timestamp_us;
  rtclog::StreamConfig config;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedVideoRecvConfig {
  LoggedVideoRecvConfig(int64_t timestamp_us, const rtclog::StreamConfig config)
      : timestamp_us(timestamp_us), config(config) {}
  int64_t timestamp_us;
  rtclog::StreamConfig config;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

struct LoggedVideoSendConfig {
  LoggedVideoSendConfig(int64_t timestamp_us,
                        const std::vector<rtclog::StreamConfig> configs)
      : timestamp_us(timestamp_us), configs(configs) {}
  int64_t timestamp_us;
  std::vector<rtclog::StreamConfig> configs;
  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }
};

template <typename T>
class PacketView;

template <typename T>
class PacketIterator {
  friend class PacketView<T>;

 public:
  // Standard iterator traits.
  using difference_type = std::ptrdiff_t;
  using value_type = T;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::bidirectional_iterator_tag;

  // The default-contructed iterator is meaningless, but is required by the
  // ForwardIterator concept.
  PacketIterator() : ptr_(nullptr), element_size_(0) {}
  PacketIterator(const PacketIterator& other)
      : ptr_(other.ptr_), element_size_(other.element_size_) {}
  PacketIterator(const PacketIterator&& other)
      : ptr_(other.ptr_), element_size_(other.element_size_) {}
  ~PacketIterator() = default;

  PacketIterator& operator=(const PacketIterator& other) {
    ptr_ = other.ptr_;
    element_size_ = other.element_size_;
    return *this;
  }
  PacketIterator& operator=(const PacketIterator&& other) {
    ptr_ = other.ptr_;
    element_size_ = other.element_size_;
    return *this;
  }

  bool operator==(const PacketIterator<T>& other) const {
    RTC_DCHECK_EQ(element_size_, other.element_size_);
    return ptr_ == other.ptr_;
  }
  bool operator!=(const PacketIterator<T>& other) const {
    RTC_DCHECK_EQ(element_size_, other.element_size_);
    return ptr_ != other.ptr_;
  }

  PacketIterator& operator++() {
    ptr_ += element_size_;
    return *this;
  }
  PacketIterator& operator--() {
    ptr_ -= element_size_;
    return *this;
  }
  PacketIterator operator++(int) {
    PacketIterator iter_copy(ptr_, element_size_);
    ptr_ += element_size_;
    return iter_copy;
  }
  PacketIterator operator--(int) {
    PacketIterator iter_copy(ptr_, element_size_);
    ptr_ -= element_size_;
    return iter_copy;
  }

  T& operator*() { return *reinterpret_cast<T*>(ptr_); }
  const T& operator*() const { return *reinterpret_cast<const T*>(ptr_); }

 private:
  PacketIterator(typename std::conditional<std::is_const<T>::value,
                                           const void*,
                                           void*>::type p,
                 size_t s)
      : ptr_(reinterpret_cast<decltype(ptr_)>(p)), element_size_(s) {}

  typename std::conditional<std::is_const<T>::value, const char*, char*>::type
      ptr_;
  size_t element_size_;
};

// Suppose that we have a struct S where we are only interested in a specific
// member M. Given an array of S, PacketView can be used to treat the array
// as an array of M, without exposing the type S to surrounding code and without
// accessing the member through a virtual function. In this case, we want to
// have a common view for incoming and outgoing RtpPackets, hence the PacketView
// name.
// Note that constructing a PacketView bypasses the typesystem, so the caller
// has to take extra care when constructing these objects. The implementation
// also requires that the containing struct is standard-layout (e.g. POD).
//
// Usage example:
// struct A {...};
// struct B { A a; ...};
// struct C { A a; ...};
// size_t len = 10;
// B* array1 = new B[len];
// C* array2 = new C[len];
//
// PacketView<A> view1 = PacketView<A>::Create<B>(array1, len, offsetof(B, a));
// PacketView<A> view2 = PacketView<A>::Create<C>(array2, len, offsetof(C, a));
//
// The following code works with either view1 or view2.
// void f(PacketView<A> view)
// for (A& a : view) {
//   DoSomething(a);
// }
template <typename T>
class PacketView {
 public:
  template <typename U>
  static PacketView Create(U* ptr, size_t num_elements, size_t offset) {
    static_assert(std::is_standard_layout<U>::value,
                  "PacketView can only be created for standard layout types.");
    static_assert(std::is_standard_layout<T>::value,
                  "PacketView can only be created for standard layout types.");
    return PacketView(ptr, num_elements, offset, sizeof(U));
  }

  using iterator = PacketIterator<T>;
  using const_iterator = PacketIterator<const T>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() { return iterator(data_, element_size_); }
  iterator end() {
    auto end_ptr = data_ + num_elements_ * element_size_;
    return iterator(end_ptr, element_size_);
  }

  const_iterator begin() const { return const_iterator(data_, element_size_); }
  const_iterator end() const {
    auto end_ptr = data_ + num_elements_ * element_size_;
    return const_iterator(end_ptr, element_size_);
  }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  size_t size() const { return num_elements_; }

  T& operator[](size_t i) {
    auto elem_ptr = data_ + i * element_size_;
    return *reinterpret_cast<T*>(elem_ptr);
  }

  const T& operator[](size_t i) const {
    auto elem_ptr = data_ + i * element_size_;
    return *reinterpret_cast<const T*>(elem_ptr);
  }

 private:
  PacketView(typename std::conditional<std::is_const<T>::value,
                                       const void*,
                                       void*>::type data,
             size_t num_elements,
             size_t offset,
             size_t element_size)
      : data_(reinterpret_cast<decltype(data_)>(data) + offset),
        num_elements_(num_elements),
        element_size_(element_size) {}

  typename std::conditional<std::is_const<T>::value, const char*, char*>::type
      data_;
  size_t num_elements_;
  size_t element_size_;
};

class ParsedRtcEventLogNew {
  friend class RtcEventLogTestHelper;

 public:
  enum class EventType {
    UNKNOWN_EVENT = 0,
    LOG_START = 1,
    LOG_END = 2,
    RTP_EVENT = 3,
    RTCP_EVENT = 4,
    AUDIO_PLAYOUT_EVENT = 5,
    LOSS_BASED_BWE_UPDATE = 6,
    DELAY_BASED_BWE_UPDATE = 7,
    VIDEO_RECEIVER_CONFIG_EVENT = 8,
    VIDEO_SENDER_CONFIG_EVENT = 9,
    AUDIO_RECEIVER_CONFIG_EVENT = 10,
    AUDIO_SENDER_CONFIG_EVENT = 11,
    AUDIO_NETWORK_ADAPTATION_EVENT = 16,
    BWE_PROBE_CLUSTER_CREATED_EVENT = 17,
    BWE_PROBE_FAILURE_EVENT = 18,
    BWE_PROBE_SUCCESS_EVENT = 19,
    ALR_STATE_EVENT = 20,
    ICE_CANDIDATE_PAIR_CONFIG = 21,
    ICE_CANDIDATE_PAIR_EVENT = 22,
  };

  enum class MediaType { ANY, AUDIO, VIDEO, DATA };
  enum class UnconfiguredHeaderExtensions {
    kDontParse,
    kAttemptWebrtcDefaultConfig
  };

  struct LoggedRtpStreamIncoming {
    uint32_t ssrc;
    std::vector<LoggedRtpPacketIncoming> incoming_packets;
  };

  struct LoggedRtpStreamOutgoing {
    uint32_t ssrc;
    std::vector<LoggedRtpPacketOutgoing> outgoing_packets;
  };

  struct LoggedRtpStreamView {
    LoggedRtpStreamView(uint32_t ssrc,
                        const LoggedRtpPacketIncoming* ptr,
                        size_t num_elements)
        : ssrc(ssrc),
          packet_view(PacketView<const LoggedRtpPacket>::Create(
              ptr,
              num_elements,
              offsetof(LoggedRtpPacketIncoming, rtp))) {}
    LoggedRtpStreamView(uint32_t ssrc,
                        const LoggedRtpPacketOutgoing* ptr,
                        size_t num_elements)
        : ssrc(ssrc),
          packet_view(PacketView<const LoggedRtpPacket>::Create(
              ptr,
              num_elements,
              offsetof(LoggedRtpPacketOutgoing, rtp))) {}
    uint32_t ssrc;
    PacketView<const LoggedRtpPacket> packet_view;
  };

  explicit ParsedRtcEventLogNew(
      UnconfiguredHeaderExtensions parse_unconfigured_header_extensions =
          UnconfiguredHeaderExtensions::kDontParse);

  // Clears previously parsed events and resets the ParsedRtcEventLogNew to an
  // empty state.
  void Clear();

  // Reads an RtcEventLog file and returns true if parsing was successful.
  bool ParseFile(const std::string& file_name);

  // Reads an RtcEventLog from a string and returns true if successful.
  bool ParseString(const std::string& s);

  // Reads an RtcEventLog from an istream and returns true if successful.
  bool ParseStream(
      std::istream& stream);  // no-presubmit-check TODO(webrtc:8982)

  // Returns the number of events in an EventStream.
  size_t GetNumberOfEvents() const;

  // Reads the arrival timestamp (in microseconds) from a rtclog::Event.
  int64_t GetTimestamp(size_t index) const;
  int64_t GetTimestamp(const rtclog::Event& event) const;

  // Reads the event type of the rtclog::Event at |index|.
  EventType GetEventType(size_t index) const;
  EventType GetEventType(const rtclog::Event& event) const;

  // Reads the header, direction, header length and packet length from the RTP
  // event at |index|, and stores the values in the corresponding output
  // parameters. Each output parameter can be set to nullptr if that value
  // isn't needed.
  // NB: The header must have space for at least IP_PACKET_SIZE bytes.
  // Returns: a pointer to a header extensions map acquired from parsing
  // corresponding Audio/Video Sender/Receiver config events.
  // Warning: if the same SSRC is reused by both video and audio streams during
  // call, extensions maps may be incorrect (the last one would be returned).
  const webrtc::RtpHeaderExtensionMap* GetRtpHeader(
      size_t index,
      PacketDirection* incoming,
      uint8_t* header,
      size_t* header_length,
      size_t* total_length,
      int* probe_cluster_id) const;
  const webrtc::RtpHeaderExtensionMap* GetRtpHeader(
      const rtclog::Event& event,
      PacketDirection* incoming,
      uint8_t* header,
      size_t* header_length,
      size_t* total_length,
      int* probe_cluster_id) const;

  // Reads packet, direction and packet length from the RTCP event at |index|,
  // and stores the values in the corresponding output parameters.
  // Each output parameter can be set to nullptr if that value isn't needed.
  // NB: The packet must have space for at least IP_PACKET_SIZE bytes.
  void GetRtcpPacket(size_t index,
                     PacketDirection* incoming,
                     uint8_t* packet,
                     size_t* length) const;
  void GetRtcpPacket(const rtclog::Event& event,
                     PacketDirection* incoming,
                     uint8_t* packet,
                     size_t* length) const;

  // Reads a video receive config event to a StreamConfig struct.
  // Only the fields that are stored in the protobuf will be written.
  rtclog::StreamConfig GetVideoReceiveConfig(size_t index) const;

  // Reads a video send config event to a StreamConfig struct. If the proto
  // contains multiple SSRCs and RTX SSRCs (this used to be the case for
  // simulcast streams) then we return one StreamConfig per SSRC,RTX_SSRC pair.
  // Only the fields that are stored in the protobuf will be written.
  std::vector<rtclog::StreamConfig> GetVideoSendConfig(size_t index) const;

  // Reads a audio receive config event to a StreamConfig struct.
  // Only the fields that are stored in the protobuf will be written.
  rtclog::StreamConfig GetAudioReceiveConfig(size_t index) const;

  // Reads a config event to a StreamConfig struct.
  // Only the fields that are stored in the protobuf will be written.
  rtclog::StreamConfig GetAudioSendConfig(size_t index) const;

  // Reads the SSRC from the audio playout event at |index|. The SSRC is stored
  // in the output parameter ssrc. The output parameter can be set to nullptr
  // and in that case the function only asserts that the event is well formed.
  LoggedAudioPlayoutEvent GetAudioPlayout(size_t index) const;

  // Reads bitrate, fraction loss (as defined in RFC 1889) and total number of
  // expected packets from the loss based BWE event at |index| and stores the
  // values in
  // the corresponding output parameters. Each output parameter can be set to
  // nullptr if that
  // value isn't needed.
  LoggedBweLossBasedUpdate GetLossBasedBweUpdate(size_t index) const;

  // Reads bitrate and detector_state from the delay based BWE event at |index|
  // and stores the values in the corresponding output parameters. Each output
  // parameter can be set to nullptr if that
  // value isn't needed.
  LoggedBweDelayBasedUpdate GetDelayBasedBweUpdate(size_t index) const;

  // Reads a audio network adaptation event to a (non-NULL)
  // AudioEncoderRuntimeConfig struct. Only the fields that are
  // stored in the protobuf will be written.
  LoggedAudioNetworkAdaptationEvent GetAudioNetworkAdaptation(
      size_t index) const;

  LoggedBweProbeClusterCreatedEvent GetBweProbeClusterCreated(
      size_t index) const;

  LoggedBweProbeFailureEvent GetBweProbeFailure(size_t index) const;
  LoggedBweProbeSuccessEvent GetBweProbeSuccess(size_t index) const;

  MediaType GetMediaType(uint32_t ssrc, PacketDirection direction) const;

  LoggedAlrStateEvent GetAlrState(size_t index) const;

  LoggedIceCandidatePairConfig GetIceCandidatePairConfig(size_t index) const;

  LoggedIceCandidatePairEvent GetIceCandidatePairEvent(size_t index) const;

  // Configured SSRCs.
  const std::set<uint32_t>& incoming_rtx_ssrcs() const {
    return incoming_rtx_ssrcs_;
  }

  const std::set<uint32_t>& incoming_video_ssrcs() const {
    return incoming_video_ssrcs_;
  }

  const std::set<uint32_t>& incoming_audio_ssrcs() const {
    return incoming_audio_ssrcs_;
  }

  const std::set<uint32_t>& outgoing_rtx_ssrcs() const {
    return outgoing_rtx_ssrcs_;
  }

  const std::set<uint32_t>& outgoing_video_ssrcs() const {
    return outgoing_video_ssrcs_;
  }

  const std::set<uint32_t>& outgoing_audio_ssrcs() const {
    return outgoing_audio_ssrcs_;
  }

  // Stream configurations.
  const std::vector<LoggedAudioRecvConfig>& audio_recv_configs() const {
    return audio_recv_configs_;
  }

  const std::vector<LoggedAudioSendConfig>& audio_send_configs() const {
    return audio_send_configs_;
  }

  const std::vector<LoggedVideoRecvConfig>& video_recv_configs() const {
    return video_recv_configs_;
  }

  const std::vector<LoggedVideoSendConfig>& video_send_configs() const {
    return video_send_configs_;
  }

  // Beginning and end of log segments.
  const std::vector<LoggedStartEvent>& start_log_events() const {
    return start_log_events_;
  }

  const std::vector<LoggedStopEvent>& stop_log_events() const {
    return stop_log_events_;
  }

  // Audio
  const std::map<uint32_t, std::vector<LoggedAudioPlayoutEvent>>&
  audio_playout_events() const {
    return audio_playout_events_;
  }

  const std::vector<LoggedAudioNetworkAdaptationEvent>&
  audio_network_adaptation_events() const {
    return audio_network_adaptation_events_;
  }

  // Bandwidth estimation
  const std::vector<LoggedBweProbeClusterCreatedEvent>&
  bwe_probe_cluster_created_events() const {
    return bwe_probe_cluster_created_events_;
  }

  const std::vector<LoggedBweProbeFailureEvent>& bwe_probe_failure_events()
      const {
    return bwe_probe_failure_events_;
  }

  const std::vector<LoggedBweProbeSuccessEvent>& bwe_probe_success_events()
      const {
    return bwe_probe_success_events_;
  }

  const std::vector<LoggedBweDelayBasedUpdate>& bwe_delay_updates() const {
    return bwe_delay_updates_;
  }

  const std::vector<LoggedBweLossBasedUpdate>& bwe_loss_updates() const {
    return bwe_loss_updates_;
  }

  const std::vector<LoggedAlrStateEvent>& alr_state_events() const {
    return alr_state_events_;
  }

  // ICE events
  const std::vector<LoggedIceCandidatePairConfig>& ice_candidate_pair_configs()
      const {
    return ice_candidate_pair_configs_;
  }

  const std::vector<LoggedIceCandidatePairEvent>& ice_candidate_pair_events()
      const {
    return ice_candidate_pair_events_;
  }

  // RTP
  const std::vector<LoggedRtpStreamIncoming>& incoming_rtp_packets_by_ssrc()
      const {
    return incoming_rtp_packets_by_ssrc_;
  }

  const std::vector<LoggedRtpStreamOutgoing>& outgoing_rtp_packets_by_ssrc()
      const {
    return outgoing_rtp_packets_by_ssrc_;
  }

  const std::vector<LoggedRtpStreamView>& rtp_packets_by_ssrc(
      PacketDirection direction) const {
    if (direction == kIncomingPacket)
      return incoming_rtp_packet_views_by_ssrc_;
    else
      return outgoing_rtp_packet_views_by_ssrc_;
  }

  // RTCP
  const std::vector<LoggedRtcpPacketIncoming>& incoming_rtcp_packets() const {
    return incoming_rtcp_packets_;
  }

  const std::vector<LoggedRtcpPacketOutgoing>& outgoing_rtcp_packets() const {
    return outgoing_rtcp_packets_;
  }

  const std::vector<LoggedRtcpPacketReceiverReport>& receiver_reports(
      PacketDirection direction) const {
    if (direction == kIncomingPacket) {
      return incoming_rr_;
    } else {
      return outgoing_rr_;
    }
  }

  const std::vector<LoggedRtcpPacketSenderReport>& sender_reports(
      PacketDirection direction) const {
    if (direction == kIncomingPacket) {
      return incoming_sr_;
    } else {
      return outgoing_sr_;
    }
  }

  const std::vector<LoggedRtcpPacketNack>& nacks(
      PacketDirection direction) const {
    if (direction == kIncomingPacket) {
      return incoming_nack_;
    } else {
      return outgoing_nack_;
    }
  }

  const std::vector<LoggedRtcpPacketRemb>& rembs(
      PacketDirection direction) const {
    if (direction == kIncomingPacket) {
      return incoming_remb_;
    } else {
      return outgoing_remb_;
    }
  }

  const std::vector<LoggedRtcpPacketTransportFeedback>& transport_feedbacks(
      PacketDirection direction) const {
    if (direction == kIncomingPacket) {
      return incoming_transport_feedback_;
    } else {
      return outgoing_transport_feedback_;
    }
  }

  int64_t first_timestamp() const { return first_timestamp_; }
  int64_t last_timestamp() const { return last_timestamp_; }

 private:
  bool ParseStreamInternal(
      std::istream& stream);  // no-presubmit-check TODO(webrtc:8982)

  void StoreParsedEvent(const rtclog::Event& event);

  rtclog::StreamConfig GetVideoReceiveConfig(const rtclog::Event& event) const;
  std::vector<rtclog::StreamConfig> GetVideoSendConfig(
      const rtclog::Event& event) const;
  rtclog::StreamConfig GetAudioReceiveConfig(const rtclog::Event& event) const;
  rtclog::StreamConfig GetAudioSendConfig(const rtclog::Event& event) const;

  LoggedAudioPlayoutEvent GetAudioPlayout(const rtclog::Event& event) const;

  LoggedBweLossBasedUpdate GetLossBasedBweUpdate(
      const rtclog::Event& event) const;
  LoggedBweDelayBasedUpdate GetDelayBasedBweUpdate(
      const rtclog::Event& event) const;

  LoggedAudioNetworkAdaptationEvent GetAudioNetworkAdaptation(
      const rtclog::Event& event) const;

  LoggedBweProbeClusterCreatedEvent GetBweProbeClusterCreated(
      const rtclog::Event& event) const;
  LoggedBweProbeFailureEvent GetBweProbeFailure(
      const rtclog::Event& event) const;
  LoggedBweProbeSuccessEvent GetBweProbeSuccess(
      const rtclog::Event& event) const;

  LoggedAlrStateEvent GetAlrState(const rtclog::Event& event) const;

  LoggedIceCandidatePairConfig GetIceCandidatePairConfig(
      const rtclog::Event& event) const;
  LoggedIceCandidatePairEvent GetIceCandidatePairEvent(
      const rtclog::Event& event) const;

  std::vector<rtclog::Event> events_;

  struct Stream {
    Stream(uint32_t ssrc,
           MediaType media_type,
           PacketDirection direction,
           webrtc::RtpHeaderExtensionMap map)
        : ssrc(ssrc),
          media_type(media_type),
          direction(direction),
          rtp_extensions_map(map) {}
    uint32_t ssrc;
    MediaType media_type;
    PacketDirection direction;
    webrtc::RtpHeaderExtensionMap rtp_extensions_map;
  };

  const UnconfiguredHeaderExtensions parse_unconfigured_header_extensions_;

  // Make a default extension map for streams without configuration information.
  // TODO(ivoc): Once configuration of audio streams is stored in the event log,
  //             this can be removed. Tracking bug: webrtc:6399
  RtpHeaderExtensionMap default_extension_map_;

  // Tracks what each stream is configured for. Note that a single SSRC can be
  // in several sets. For example, the SSRC used for sending video over RTX
  // will appear in both video_ssrcs_ and rtx_ssrcs_. In the unlikely case that
  // an SSRC is reconfigured to a different media type mid-call, it will also
  // appear in multiple sets.
  std::set<uint32_t> incoming_rtx_ssrcs_;
  std::set<uint32_t> incoming_video_ssrcs_;
  std::set<uint32_t> incoming_audio_ssrcs_;
  std::set<uint32_t> outgoing_rtx_ssrcs_;
  std::set<uint32_t> outgoing_video_ssrcs_;
  std::set<uint32_t> outgoing_audio_ssrcs_;

  // Maps an SSRC to the parsed  RTP headers in that stream. Header extensions
  // are parsed if the stream has been configured. This is only used for
  // grouping the events by SSRC during parsing; the events are moved to
  // incoming_rtp_packets_by_ssrc_ once the parsing is done.
  std::map<uint32_t, std::vector<LoggedRtpPacketIncoming>>
      incoming_rtp_packets_map_;
  std::map<uint32_t, std::vector<LoggedRtpPacketOutgoing>>
      outgoing_rtp_packets_map_;

  // RTP headers.
  std::vector<LoggedRtpStreamIncoming> incoming_rtp_packets_by_ssrc_;
  std::vector<LoggedRtpStreamOutgoing> outgoing_rtp_packets_by_ssrc_;
  std::vector<LoggedRtpStreamView> incoming_rtp_packet_views_by_ssrc_;
  std::vector<LoggedRtpStreamView> outgoing_rtp_packet_views_by_ssrc_;

  // Raw RTCP packets.
  std::vector<LoggedRtcpPacketIncoming> incoming_rtcp_packets_;
  std::vector<LoggedRtcpPacketOutgoing> outgoing_rtcp_packets_;

  // Parsed RTCP messages. Currently not separated based on SSRC.
  std::vector<LoggedRtcpPacketReceiverReport> incoming_rr_;
  std::vector<LoggedRtcpPacketReceiverReport> outgoing_rr_;
  std::vector<LoggedRtcpPacketSenderReport> incoming_sr_;
  std::vector<LoggedRtcpPacketSenderReport> outgoing_sr_;
  std::vector<LoggedRtcpPacketNack> incoming_nack_;
  std::vector<LoggedRtcpPacketNack> outgoing_nack_;
  std::vector<LoggedRtcpPacketRemb> incoming_remb_;
  std::vector<LoggedRtcpPacketRemb> outgoing_remb_;
  std::vector<LoggedRtcpPacketTransportFeedback> incoming_transport_feedback_;
  std::vector<LoggedRtcpPacketTransportFeedback> outgoing_transport_feedback_;

  std::vector<LoggedStartEvent> start_log_events_;
  std::vector<LoggedStopEvent> stop_log_events_;

  std::map<uint32_t, std::vector<LoggedAudioPlayoutEvent>>
      audio_playout_events_;

  std::vector<LoggedAudioNetworkAdaptationEvent>
      audio_network_adaptation_events_;

  std::vector<LoggedBweProbeClusterCreatedEvent>
      bwe_probe_cluster_created_events_;

  std::vector<LoggedBweProbeFailureEvent> bwe_probe_failure_events_;
  std::vector<LoggedBweProbeSuccessEvent> bwe_probe_success_events_;

  std::vector<LoggedBweDelayBasedUpdate> bwe_delay_updates_;

  // A list of all updates from the send-side loss-based bandwidth estimator.
  std::vector<LoggedBweLossBasedUpdate> bwe_loss_updates_;

  std::vector<LoggedAlrStateEvent> alr_state_events_;

  std::vector<LoggedIceCandidatePairConfig> ice_candidate_pair_configs_;

  std::vector<LoggedIceCandidatePairEvent> ice_candidate_pair_events_;

  std::vector<LoggedAudioRecvConfig> audio_recv_configs_;
  std::vector<LoggedAudioSendConfig> audio_send_configs_;
  std::vector<LoggedVideoRecvConfig> video_recv_configs_;
  std::vector<LoggedVideoSendConfig> video_send_configs_;

  uint8_t last_incoming_rtcp_packet_[IP_PACKET_SIZE];
  uint8_t last_incoming_rtcp_packet_length_;

  int64_t first_timestamp_;
  int64_t last_timestamp_;

  // The extension maps are mutable to allow us to insert the default
  // configuration when parsing an RTP header for an unconfigured stream.
  mutable std::map<uint32_t, webrtc::RtpHeaderExtensionMap>
      incoming_rtp_extensions_maps_;
  mutable std::map<uint32_t, webrtc::RtpHeaderExtensionMap>
      outgoing_rtp_extensions_maps_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_NEW_H_
