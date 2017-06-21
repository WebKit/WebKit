/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_CALL_CALL_H_
#define WEBRTC_CALL_CALL_H_

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/rtcerror.h"
#include "webrtc/base/networkroute.h"
#include "webrtc/base/platform_file.h"
#include "webrtc/base/socket.h"
#include "webrtc/call/audio_receive_stream.h"
#include "webrtc/call/audio_send_stream.h"
#include "webrtc/call/audio_state.h"
#include "webrtc/call/flexfec_receive_stream.h"
#include "webrtc/call/rtp_transport_controller_send_interface.h"
#include "webrtc/common_types.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class AudioProcessing;
class RtcEventLog;

enum class MediaType {
  ANY,
  AUDIO,
  VIDEO,
  DATA
};

// Like std::min, but considers non-positive values to be unset.
// TODO(zstein): Remove once all callers use rtc::Optional.
template <typename T>
static T MinPositive(T a, T b) {
  if (a <= 0) {
    return b;
  }
  if (b <= 0) {
    return a;
  }
  return std::min(a, b);
}

class PacketReceiver {
 public:
  enum DeliveryStatus {
    DELIVERY_OK,
    DELIVERY_UNKNOWN_SSRC,
    DELIVERY_PACKET_ERROR,
  };

  virtual DeliveryStatus DeliverPacket(MediaType media_type,
                                       const uint8_t* packet,
                                       size_t length,
                                       const PacketTime& packet_time) = 0;

 protected:
  virtual ~PacketReceiver() {}
};

// A Call instance can contain several send and/or receive streams. All streams
// are assumed to have the same remote endpoint and will share bitrate estimates
// etc.
class Call {
 public:
  struct Config {
    explicit Config(RtcEventLog* event_log) : event_log(event_log) {
      RTC_DCHECK(event_log);
    }

    static constexpr int kDefaultStartBitrateBps = 300000;

    // Bitrate config used until valid bitrate estimates are calculated. Also
    // used to cap total bitrate used. This comes from the remote connection.
    struct BitrateConfig {
      int min_bitrate_bps = 0;
      int start_bitrate_bps = kDefaultStartBitrateBps;
      int max_bitrate_bps = -1;
    } bitrate_config;

    // The local client's bitrate preferences. The actual configuration used
    // is a combination of this and |bitrate_config|. The combination is
    // currently more complicated than a simple mask operation (see
    // SetBitrateConfig and SetBitrateConfigMask). Assumes that 0 <= min <=
    // start <= max holds for set parameters.
    struct BitrateConfigMask {
      rtc::Optional<int> min_bitrate_bps;
      rtc::Optional<int> start_bitrate_bps;
      rtc::Optional<int> max_bitrate_bps;
    };

    // AudioState which is possibly shared between multiple calls.
    // TODO(solenberg): Change this to a shared_ptr once we can use C++11.
    rtc::scoped_refptr<AudioState> audio_state;

    // Audio Processing Module to be used in this call.
    // TODO(solenberg): Change this to a shared_ptr once we can use C++11.
    AudioProcessing* audio_processing = nullptr;

    // RtcEventLog to use for this call. Required.
    // Use webrtc::RtcEventLog::CreateNull() for a null implementation.
    RtcEventLog* event_log = nullptr;
  };

  struct Stats {
    std::string ToString(int64_t time_ms) const;

    int send_bandwidth_bps = 0;       // Estimated available send bandwidth.
    int max_padding_bitrate_bps = 0;  // Cumulative configured max padding.
    int recv_bandwidth_bps = 0;       // Estimated available receive bandwidth.
    int64_t pacer_delay_ms = 0;
    int64_t rtt_ms = -1;
  };

  static Call* Create(const Call::Config& config);

  // Allows mocking |transport_send| for testing.
  static Call* Create(
      const Call::Config& config,
      std::unique_ptr<RtpTransportControllerSendInterface> transport_send);

  virtual AudioSendStream* CreateAudioSendStream(
      const AudioSendStream::Config& config) = 0;
  virtual void DestroyAudioSendStream(AudioSendStream* send_stream) = 0;

  virtual AudioReceiveStream* CreateAudioReceiveStream(
      const AudioReceiveStream::Config& config) = 0;
  virtual void DestroyAudioReceiveStream(
      AudioReceiveStream* receive_stream) = 0;

  virtual VideoSendStream* CreateVideoSendStream(
      VideoSendStream::Config config,
      VideoEncoderConfig encoder_config) = 0;
  virtual void DestroyVideoSendStream(VideoSendStream* send_stream) = 0;

  virtual VideoReceiveStream* CreateVideoReceiveStream(
      VideoReceiveStream::Config configuration) = 0;
  virtual void DestroyVideoReceiveStream(
      VideoReceiveStream* receive_stream) = 0;

  // In order for a created VideoReceiveStream to be aware that it is
  // protected by a FlexfecReceiveStream, the latter should be created before
  // the former.
  virtual FlexfecReceiveStream* CreateFlexfecReceiveStream(
      const FlexfecReceiveStream::Config& config) = 0;
  virtual void DestroyFlexfecReceiveStream(
      FlexfecReceiveStream* receive_stream) = 0;

  // All received RTP and RTCP packets for the call should be inserted to this
  // PacketReceiver. The PacketReceiver pointer is valid as long as the
  // Call instance exists.
  virtual PacketReceiver* Receiver() = 0;

  // Returns the call statistics, such as estimated send and receive bandwidth,
  // pacing delay, etc.
  virtual Stats GetStats() const = 0;

  // The greater min and smaller max set by this and SetBitrateConfigMask will
  // be used. The latest non-negative start value from either call will be used.
  // Specifying a start bitrate (>0) will reset the current bitrate estimate.
  // This is due to how the 'x-google-start-bitrate' flag is currently
  // implemented. Passing -1 leaves the start bitrate unchanged. Behavior is not
  // guaranteed for other negative values or 0.
  virtual void SetBitrateConfig(
      const Config::BitrateConfig& bitrate_config) = 0;

  // The greater min and smaller max set by this and SetBitrateConfig will be
  // used. The latest non-negative start value form either call will be used.
  // Specifying a start bitrate will reset the current bitrate estimate.
  // Assumes 0 <= min <= start <= max holds for set parameters.
  virtual void SetBitrateConfigMask(
      const Config::BitrateConfigMask& bitrate_mask) = 0;

  // TODO(skvlad): When the unbundled case with multiple streams for the same
  // media type going over different networks is supported, track the state
  // for each stream separately. Right now it's global per media type.
  virtual void SignalChannelNetworkState(MediaType media,
                                         NetworkState state) = 0;

  virtual void OnTransportOverheadChanged(
      MediaType media,
      int transport_overhead_per_packet) = 0;

  virtual void OnNetworkRouteChanged(
      const std::string& transport_name,
      const rtc::NetworkRoute& network_route) = 0;

  virtual void OnSentPacket(const rtc::SentPacket& sent_packet) = 0;

  virtual ~Call() {}
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_CALL_H_
