/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains fake implementations, for use in unit tests, of the
// following classes:
//
//   webrtc::Call
//   webrtc::AudioSendStream
//   webrtc::AudioReceiveStream
//   webrtc::VideoSendStream
//   webrtc::VideoReceiveStream

#ifndef WEBRTC_MEDIA_ENGINE_FAKEWEBRTCCALL_H_
#define WEBRTC_MEDIA_ENGINE_FAKEWEBRTCCALL_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/buffer.h"
#include "webrtc/call/audio_receive_stream.h"
#include "webrtc/call/audio_send_stream.h"
#include "webrtc/call/call.h"
#include "webrtc/call/flexfec_receive_stream.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace cricket {
class FakeAudioSendStream final : public webrtc::AudioSendStream {
 public:
  struct TelephoneEvent {
    int payload_type = -1;
    int payload_frequency = -1;
    int event_code = 0;
    int duration_ms = 0;
  };

  explicit FakeAudioSendStream(
      int id, const webrtc::AudioSendStream::Config& config);

  int id() const { return id_; }
  const webrtc::AudioSendStream::Config& GetConfig() const;
  void SetStats(const webrtc::AudioSendStream::Stats& stats);
  TelephoneEvent GetLatestTelephoneEvent() const;
  bool IsSending() const { return sending_; }
  bool muted() const { return muted_; }

 private:
  // webrtc::AudioSendStream implementation.
  void Reconfigure(const webrtc::AudioSendStream::Config& config) override;

  void Start() override { sending_ = true; }
  void Stop() override { sending_ = false; }

  bool SendTelephoneEvent(int payload_type, int payload_frequency, int event,
                          int duration_ms) override;
  void SetMuted(bool muted) override;
  webrtc::AudioSendStream::Stats GetStats() const override;

  int id_ = -1;
  TelephoneEvent latest_telephone_event_;
  webrtc::AudioSendStream::Config config_;
  webrtc::AudioSendStream::Stats stats_;
  bool sending_ = false;
  bool muted_ = false;
};

class FakeAudioReceiveStream final : public webrtc::AudioReceiveStream {
 public:
  explicit FakeAudioReceiveStream(
      int id, const webrtc::AudioReceiveStream::Config& config);

  int id() const { return id_; }
  const webrtc::AudioReceiveStream::Config& GetConfig() const;
  void SetStats(const webrtc::AudioReceiveStream::Stats& stats);
  int received_packets() const { return received_packets_; }
  bool VerifyLastPacket(const uint8_t* data, size_t length) const;
  const webrtc::AudioSinkInterface* sink() const { return sink_.get(); }
  float gain() const { return gain_; }
  bool DeliverRtp(const uint8_t* packet,
                  size_t length,
                  const webrtc::PacketTime& packet_time);
  bool started() const { return started_; }

 private:
  // webrtc::AudioReceiveStream implementation.
  void Start() override { started_ = true; }
  void Stop() override { started_ = false; }

  webrtc::AudioReceiveStream::Stats GetStats() const override;
  int GetOutputLevel() const override { return 0; }
  void SetSink(std::unique_ptr<webrtc::AudioSinkInterface> sink) override;
  void SetGain(float gain) override;
  std::vector<webrtc::RtpSource> GetSources() const override {
    return std::vector<webrtc::RtpSource>();
  }

  int id_ = -1;
  webrtc::AudioReceiveStream::Config config_;
  webrtc::AudioReceiveStream::Stats stats_;
  int received_packets_ = 0;
  std::unique_ptr<webrtc::AudioSinkInterface> sink_;
  float gain_ = 1.0f;
  rtc::Buffer last_packet_;
  bool started_ = false;
};

class FakeVideoSendStream final
    : public webrtc::VideoSendStream,
      public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  FakeVideoSendStream(webrtc::VideoSendStream::Config config,
                      webrtc::VideoEncoderConfig encoder_config);
  ~FakeVideoSendStream() override;
  const webrtc::VideoSendStream::Config& GetConfig() const;
  const webrtc::VideoEncoderConfig& GetEncoderConfig() const;
  const std::vector<webrtc::VideoStream>& GetVideoStreams() const;

  bool IsSending() const;
  bool GetVp8Settings(webrtc::VideoCodecVP8* settings) const;
  bool GetVp9Settings(webrtc::VideoCodecVP9* settings) const;

  int GetNumberOfSwappedFrames() const;
  int GetLastWidth() const;
  int GetLastHeight() const;
  int64_t GetLastTimestamp() const;
  void SetStats(const webrtc::VideoSendStream::Stats& stats);
  int num_encoder_reconfigurations() const {
    return num_encoder_reconfigurations_;
  }

  void EnableEncodedFrameRecording(const std::vector<rtc::PlatformFile>& files,
                                   size_t byte_limit) override;

  bool resolution_scaling_enabled() const {
    return resolution_scaling_enabled_;
  }
  bool framerate_scaling_enabled() const { return framerate_scaling_enabled_; }
  void InjectVideoSinkWants(const rtc::VideoSinkWants& wants);

  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() const {
    return source_;
  }

 private:
  // rtc::VideoSinkInterface<VideoFrame> implementation.
  void OnFrame(const webrtc::VideoFrame& frame) override;

  // webrtc::VideoSendStream implementation.
  void Start() override;
  void Stop() override;
  void SetSource(rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
                 const webrtc::VideoSendStream::DegradationPreference&
                     degradation_preference) override;
  webrtc::VideoSendStream::Stats GetStats() override;
  void ReconfigureVideoEncoder(webrtc::VideoEncoderConfig config) override;

  bool sending_;
  webrtc::VideoSendStream::Config config_;
  webrtc::VideoEncoderConfig encoder_config_;
  std::vector<webrtc::VideoStream> video_streams_;
  rtc::VideoSinkWants sink_wants_;

  bool codec_settings_set_;
  union VpxSettings {
    webrtc::VideoCodecVP8 vp8;
    webrtc::VideoCodecVP9 vp9;
  } vpx_settings_;
  bool resolution_scaling_enabled_;
  bool framerate_scaling_enabled_;
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source_;
  int num_swapped_frames_;
  rtc::Optional<webrtc::VideoFrame> last_frame_;
  webrtc::VideoSendStream::Stats stats_;
  int num_encoder_reconfigurations_ = 0;
};

class FakeVideoReceiveStream final : public webrtc::VideoReceiveStream {
 public:
  explicit FakeVideoReceiveStream(webrtc::VideoReceiveStream::Config config);

  const webrtc::VideoReceiveStream::Config& GetConfig() const;

  bool IsReceiving() const;

  void InjectFrame(const webrtc::VideoFrame& frame);

  void SetStats(const webrtc::VideoReceiveStream::Stats& stats);

  void EnableEncodedFrameRecording(rtc::PlatformFile file,
                                   size_t byte_limit) override;

 private:
  // webrtc::VideoReceiveStream implementation.
  void Start() override;
  void Stop() override;

  webrtc::VideoReceiveStream::Stats GetStats() const override;

  webrtc::VideoReceiveStream::Config config_;
  bool receiving_;
  webrtc::VideoReceiveStream::Stats stats_;
};

class FakeFlexfecReceiveStream final : public webrtc::FlexfecReceiveStream {
 public:
  explicit FakeFlexfecReceiveStream(
      const webrtc::FlexfecReceiveStream::Config& config);

  const webrtc::FlexfecReceiveStream::Config& GetConfig() const;

 private:
  // webrtc::FlexfecReceiveStream implementation.
  void Start() override;
  void Stop() override;

  webrtc::FlexfecReceiveStream::Stats GetStats() const override;

  webrtc::FlexfecReceiveStream::Config config_;
  bool receiving_;
};

class FakeCall final : public webrtc::Call, public webrtc::PacketReceiver {
 public:
  explicit FakeCall(const webrtc::Call::Config& config);
  ~FakeCall() override;

  webrtc::Call::Config GetConfig() const;
  const std::vector<FakeVideoSendStream*>& GetVideoSendStreams();
  const std::vector<FakeVideoReceiveStream*>& GetVideoReceiveStreams();

  const std::vector<FakeAudioSendStream*>& GetAudioSendStreams();
  const FakeAudioSendStream* GetAudioSendStream(uint32_t ssrc);
  const std::vector<FakeAudioReceiveStream*>& GetAudioReceiveStreams();
  const FakeAudioReceiveStream* GetAudioReceiveStream(uint32_t ssrc);

  const std::vector<FakeFlexfecReceiveStream*>& GetFlexfecReceiveStreams();

  rtc::SentPacket last_sent_packet() const { return last_sent_packet_; }

  // This is useful if we care about the last media packet (with id populated)
  // but not the last ICE packet (with -1 ID).
  int last_sent_nonnegative_packet_id() const {
    return last_sent_nonnegative_packet_id_;
  }

  webrtc::NetworkState GetNetworkState(webrtc::MediaType media) const;
  int GetNumCreatedSendStreams() const;
  int GetNumCreatedReceiveStreams() const;
  void SetStats(const webrtc::Call::Stats& stats);

 private:
  webrtc::AudioSendStream* CreateAudioSendStream(
      const webrtc::AudioSendStream::Config& config) override;
  void DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) override;

  webrtc::AudioReceiveStream* CreateAudioReceiveStream(
      const webrtc::AudioReceiveStream::Config& config) override;
  void DestroyAudioReceiveStream(
      webrtc::AudioReceiveStream* receive_stream) override;

  webrtc::VideoSendStream* CreateVideoSendStream(
      webrtc::VideoSendStream::Config config,
      webrtc::VideoEncoderConfig encoder_config) override;
  void DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) override;

  webrtc::VideoReceiveStream* CreateVideoReceiveStream(
      webrtc::VideoReceiveStream::Config config) override;
  void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) override;

  webrtc::FlexfecReceiveStream* CreateFlexfecReceiveStream(
      const webrtc::FlexfecReceiveStream::Config& config) override;
  void DestroyFlexfecReceiveStream(
      webrtc::FlexfecReceiveStream* receive_stream) override;

  webrtc::PacketReceiver* Receiver() override;

  DeliveryStatus DeliverPacket(webrtc::MediaType media_type,
                               const uint8_t* packet,
                               size_t length,
                               const webrtc::PacketTime& packet_time) override;

  webrtc::Call::Stats GetStats() const override;

  void SetBitrateConfig(
      const webrtc::Call::Config::BitrateConfig& bitrate_config) override;
  void SetBitrateConfigMask(
      const webrtc::Call::Config::BitrateConfigMask& mask) override;
  void OnNetworkRouteChanged(const std::string& transport_name,
                             const rtc::NetworkRoute& network_route) override {}
  void SignalChannelNetworkState(webrtc::MediaType media,
                                 webrtc::NetworkState state) override;
  void OnTransportOverheadChanged(webrtc::MediaType media,
                                  int transport_overhead_per_packet) override;
  void OnSentPacket(const rtc::SentPacket& sent_packet) override;

  webrtc::Call::Config config_;
  webrtc::NetworkState audio_network_state_;
  webrtc::NetworkState video_network_state_;
  rtc::SentPacket last_sent_packet_;
  int last_sent_nonnegative_packet_id_ = -1;
  int next_stream_id_ = 665;
  webrtc::Call::Stats stats_;
  std::vector<FakeVideoSendStream*> video_send_streams_;
  std::vector<FakeAudioSendStream*> audio_send_streams_;
  std::vector<FakeVideoReceiveStream*> video_receive_streams_;
  std::vector<FakeAudioReceiveStream*> audio_receive_streams_;
  std::vector<FakeFlexfecReceiveStream*> flexfec_receive_streams_;

  int num_created_send_streams_;
  int num_created_receive_streams_;

  int audio_transport_overhead_;
  int video_transport_overhead_;
};

}  // namespace cricket
#endif  // WEBRTC_MEDIA_ENGINE_FAKEWEBRTCCALL_H_
