/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/fakewebrtccall.h"

#include <algorithm>
#include <utility>

#include "webrtc/api/call/audio_sink.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/platform_file.h"
#include "webrtc/base/gunit.h"
#include "webrtc/media/base/rtputils.h"

namespace cricket {
FakeAudioSendStream::FakeAudioSendStream(
    int id, const webrtc::AudioSendStream::Config& config)
    : id_(id), config_(config) {
  RTC_DCHECK(config.voe_channel_id != -1);
}

const webrtc::AudioSendStream::Config&
    FakeAudioSendStream::GetConfig() const {
  return config_;
}

void FakeAudioSendStream::SetStats(
    const webrtc::AudioSendStream::Stats& stats) {
  stats_ = stats;
}

FakeAudioSendStream::TelephoneEvent
    FakeAudioSendStream::GetLatestTelephoneEvent() const {
  return latest_telephone_event_;
}

bool FakeAudioSendStream::SendTelephoneEvent(int payload_type,
                                             int payload_frequency, int event,
                                             int duration_ms) {
  latest_telephone_event_.payload_type = payload_type;
  latest_telephone_event_.payload_frequency = payload_frequency;
  latest_telephone_event_.event_code = event;
  latest_telephone_event_.duration_ms = duration_ms;
  return true;
}

void FakeAudioSendStream::SetMuted(bool muted) {
  muted_ = muted;
}

webrtc::AudioSendStream::Stats FakeAudioSendStream::GetStats() const {
  return stats_;
}

FakeAudioReceiveStream::FakeAudioReceiveStream(
    int id, const webrtc::AudioReceiveStream::Config& config)
    : id_(id), config_(config) {
  RTC_DCHECK(config.voe_channel_id != -1);
}

const webrtc::AudioReceiveStream::Config&
    FakeAudioReceiveStream::GetConfig() const {
  return config_;
}

void FakeAudioReceiveStream::SetStats(
    const webrtc::AudioReceiveStream::Stats& stats) {
  stats_ = stats;
}

bool FakeAudioReceiveStream::VerifyLastPacket(const uint8_t* data,
                                              size_t length) const {
  return last_packet_ == rtc::Buffer(data, length);
}

bool FakeAudioReceiveStream::DeliverRtp(const uint8_t* packet,
                                        size_t length,
                                        const webrtc::PacketTime& packet_time) {
  ++received_packets_;
  last_packet_.SetData(packet, length);
  return true;
}

webrtc::AudioReceiveStream::Stats FakeAudioReceiveStream::GetStats() const {
  return stats_;
}

void FakeAudioReceiveStream::SetSink(
    std::unique_ptr<webrtc::AudioSinkInterface> sink) {
  sink_ = std::move(sink);
}

void FakeAudioReceiveStream::SetGain(float gain) {
  gain_ = gain;
}

FakeVideoSendStream::FakeVideoSendStream(
    webrtc::VideoSendStream::Config config,
    webrtc::VideoEncoderConfig encoder_config)
    : sending_(false),
      config_(std::move(config)),
      codec_settings_set_(false),
      resolution_scaling_enabled_(false),
      source_(nullptr),
      num_swapped_frames_(0) {
  RTC_DCHECK(config.encoder_settings.encoder != NULL);
  ReconfigureVideoEncoder(std::move(encoder_config));
}

FakeVideoSendStream::~FakeVideoSendStream() {
  if (source_)
    source_->RemoveSink(this);
}

const webrtc::VideoSendStream::Config& FakeVideoSendStream::GetConfig() const {
  return config_;
}

const webrtc::VideoEncoderConfig& FakeVideoSendStream::GetEncoderConfig()
    const {
  return encoder_config_;
}

const std::vector<webrtc::VideoStream>& FakeVideoSendStream::GetVideoStreams()
    const {
  return video_streams_;
}

bool FakeVideoSendStream::IsSending() const {
  return sending_;
}

bool FakeVideoSendStream::GetVp8Settings(
    webrtc::VideoCodecVP8* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = vpx_settings_.vp8;
  return true;
}

bool FakeVideoSendStream::GetVp9Settings(
    webrtc::VideoCodecVP9* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = vpx_settings_.vp9;
  return true;
}

int FakeVideoSendStream::GetNumberOfSwappedFrames() const {
  return num_swapped_frames_;
}

int FakeVideoSendStream::GetLastWidth() const {
  return last_frame_->width();
}

int FakeVideoSendStream::GetLastHeight() const {
  return last_frame_->height();
}

int64_t FakeVideoSendStream::GetLastTimestamp() const {
  RTC_DCHECK(last_frame_->ntp_time_ms() == 0);
  return last_frame_->render_time_ms();
}

void FakeVideoSendStream::OnFrame(const webrtc::VideoFrame& frame) {
  ++num_swapped_frames_;
  if (!last_frame_ ||
      frame.width() != last_frame_->width() ||
      frame.height() != last_frame_->height() ||
      frame.rotation() != last_frame_->rotation()) {
    video_streams_ = encoder_config_.video_stream_factory->CreateEncoderStreams(
        frame.width(), frame.height(), encoder_config_);
  }
  last_frame_ = rtc::Optional<webrtc::VideoFrame>(frame);
}

void FakeVideoSendStream::SetStats(
    const webrtc::VideoSendStream::Stats& stats) {
  stats_ = stats;
}

webrtc::VideoSendStream::Stats FakeVideoSendStream::GetStats() {
  return stats_;
}

void FakeVideoSendStream::EnableEncodedFrameRecording(
    const std::vector<rtc::PlatformFile>& files,
    size_t byte_limit) {
  for (rtc::PlatformFile file : files)
    rtc::ClosePlatformFile(file);
}

void FakeVideoSendStream::ReconfigureVideoEncoder(
    webrtc::VideoEncoderConfig config) {
  int width, height;
  if (last_frame_) {
    width = last_frame_->width();
    height = last_frame_->height();
  } else {
    width = height = 0;
  }
  video_streams_ = config.video_stream_factory->CreateEncoderStreams(
      width, height, config);
  if (config.encoder_specific_settings != NULL) {
    if (config_.encoder_settings.payload_name == "VP8") {
      config.encoder_specific_settings->FillVideoCodecVp8(&vpx_settings_.vp8);
      if (!video_streams_.empty()) {
        vpx_settings_.vp8.numberOfTemporalLayers = static_cast<unsigned char>(
            video_streams_.back().temporal_layer_thresholds_bps.size() + 1);
      }
    } else if (config_.encoder_settings.payload_name == "VP9") {
      config.encoder_specific_settings->FillVideoCodecVp9(&vpx_settings_.vp9);
      if (!video_streams_.empty()) {
        vpx_settings_.vp9.numberOfTemporalLayers = static_cast<unsigned char>(
            video_streams_.back().temporal_layer_thresholds_bps.size() + 1);
      }
    } else {
      ADD_FAILURE() << "Unsupported encoder payload: "
                    << config_.encoder_settings.payload_name;
    }
  }
  codec_settings_set_ = config.encoder_specific_settings != NULL;
  encoder_config_ = std::move(config);
  ++num_encoder_reconfigurations_;
}

void FakeVideoSendStream::Start() {
  sending_ = true;
}

void FakeVideoSendStream::Stop() {
  sending_ = false;
}

void FakeVideoSendStream::SetSource(
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
    const webrtc::VideoSendStream::DegradationPreference&
        degradation_preference) {
  RTC_DCHECK(source != source_);
  if (source_)
    source_->RemoveSink(this);
  source_ = source;
  resolution_scaling_enabled_ =
      degradation_preference !=
      webrtc::VideoSendStream::DegradationPreference::kMaintainResolution;
  if (source)
    source->AddOrUpdateSink(this, resolution_scaling_enabled_
                                      ? sink_wants_
                                      : rtc::VideoSinkWants());
}

void FakeVideoSendStream::InjectVideoSinkWants(
    const rtc::VideoSinkWants& wants) {
  sink_wants_ = wants;
  source_->AddOrUpdateSink(this, wants);
}

FakeVideoReceiveStream::FakeVideoReceiveStream(
    webrtc::VideoReceiveStream::Config config)
    : config_(std::move(config)), receiving_(false) {}

const webrtc::VideoReceiveStream::Config& FakeVideoReceiveStream::GetConfig()
    const {
  return config_;
}

bool FakeVideoReceiveStream::IsReceiving() const {
  return receiving_;
}

void FakeVideoReceiveStream::InjectFrame(const webrtc::VideoFrame& frame) {
  config_.renderer->OnFrame(frame);
}

webrtc::VideoReceiveStream::Stats FakeVideoReceiveStream::GetStats() const {
  return stats_;
}

void FakeVideoReceiveStream::Start() {
  receiving_ = true;
}

void FakeVideoReceiveStream::Stop() {
  receiving_ = false;
}

void FakeVideoReceiveStream::SetStats(
    const webrtc::VideoReceiveStream::Stats& stats) {
  stats_ = stats;
}

void FakeVideoReceiveStream::EnableEncodedFrameRecording(rtc::PlatformFile file,
                                                         size_t byte_limit) {
  rtc::ClosePlatformFile(file);
}

FakeFlexfecReceiveStream::FakeFlexfecReceiveStream(
    const webrtc::FlexfecReceiveStream::Config& config)
    : config_(config), receiving_(false) {}

const webrtc::FlexfecReceiveStream::Config&
FakeFlexfecReceiveStream::GetConfig() const {
  return config_;
}

void FakeFlexfecReceiveStream::Start() {
  receiving_ = true;
}

void FakeFlexfecReceiveStream::Stop() {
  receiving_ = false;
}

// TODO(brandtr): Implement when the stats have been designed.
webrtc::FlexfecReceiveStream::Stats FakeFlexfecReceiveStream::GetStats() const {
  return webrtc::FlexfecReceiveStream::Stats();
}

FakeCall::FakeCall(const webrtc::Call::Config& config)
    : config_(config),
      audio_network_state_(webrtc::kNetworkUp),
      video_network_state_(webrtc::kNetworkUp),
      num_created_send_streams_(0),
      num_created_receive_streams_(0) {}

FakeCall::~FakeCall() {
  EXPECT_EQ(0u, video_send_streams_.size());
  EXPECT_EQ(0u, audio_send_streams_.size());
  EXPECT_EQ(0u, video_receive_streams_.size());
  EXPECT_EQ(0u, audio_receive_streams_.size());
}

webrtc::Call::Config FakeCall::GetConfig() const {
  return config_;
}

const std::vector<FakeVideoSendStream*>& FakeCall::GetVideoSendStreams() {
  return video_send_streams_;
}

const std::vector<FakeVideoReceiveStream*>& FakeCall::GetVideoReceiveStreams() {
  return video_receive_streams_;
}

const std::vector<FakeAudioSendStream*>& FakeCall::GetAudioSendStreams() {
  return audio_send_streams_;
}

const FakeAudioSendStream* FakeCall::GetAudioSendStream(uint32_t ssrc) {
  for (const auto* p : GetAudioSendStreams()) {
    if (p->GetConfig().rtp.ssrc == ssrc) {
      return p;
    }
  }
  return nullptr;
}

const std::vector<FakeAudioReceiveStream*>& FakeCall::GetAudioReceiveStreams() {
  return audio_receive_streams_;
}

const FakeAudioReceiveStream* FakeCall::GetAudioReceiveStream(uint32_t ssrc) {
  for (const auto* p : GetAudioReceiveStreams()) {
    if (p->GetConfig().rtp.remote_ssrc == ssrc) {
      return p;
    }
  }
  return nullptr;
}

const std::vector<FakeFlexfecReceiveStream*>&
FakeCall::GetFlexfecReceiveStreams() {
  return flexfec_receive_streams_;
}

webrtc::NetworkState FakeCall::GetNetworkState(webrtc::MediaType media) const {
  switch (media) {
    case webrtc::MediaType::AUDIO:
      return audio_network_state_;
    case webrtc::MediaType::VIDEO:
      return video_network_state_;
    case webrtc::MediaType::DATA:
    case webrtc::MediaType::ANY:
      ADD_FAILURE() << "GetNetworkState called with unknown parameter.";
      return webrtc::kNetworkDown;
  }
  // Even though all the values for the enum class are listed above,the compiler
  // will emit a warning as the method may be called with a value outside of the
  // valid enum range, unless this case is also handled.
  ADD_FAILURE() << "GetNetworkState called with unknown parameter.";
  return webrtc::kNetworkDown;
}

webrtc::AudioSendStream* FakeCall::CreateAudioSendStream(
    const webrtc::AudioSendStream::Config& config) {
  FakeAudioSendStream* fake_stream = new FakeAudioSendStream(next_stream_id_++,
                                                             config);
  audio_send_streams_.push_back(fake_stream);
  ++num_created_send_streams_;
  return fake_stream;
}

void FakeCall::DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) {
  auto it = std::find(audio_send_streams_.begin(),
                      audio_send_streams_.end(),
                      static_cast<FakeAudioSendStream*>(send_stream));
  if (it == audio_send_streams_.end()) {
    ADD_FAILURE() << "DestroyAudioSendStream called with unknown parameter.";
  } else {
    delete *it;
    audio_send_streams_.erase(it);
  }
}

webrtc::AudioReceiveStream* FakeCall::CreateAudioReceiveStream(
    const webrtc::AudioReceiveStream::Config& config) {
  audio_receive_streams_.push_back(new FakeAudioReceiveStream(next_stream_id_++,
                                                              config));
  ++num_created_receive_streams_;
  return audio_receive_streams_.back();
}

void FakeCall::DestroyAudioReceiveStream(
    webrtc::AudioReceiveStream* receive_stream) {
  auto it = std::find(audio_receive_streams_.begin(),
                      audio_receive_streams_.end(),
                      static_cast<FakeAudioReceiveStream*>(receive_stream));
  if (it == audio_receive_streams_.end()) {
    ADD_FAILURE() << "DestroyAudioReceiveStream called with unknown parameter.";
  } else {
    delete *it;
    audio_receive_streams_.erase(it);
  }
}

webrtc::VideoSendStream* FakeCall::CreateVideoSendStream(
    webrtc::VideoSendStream::Config config,
    webrtc::VideoEncoderConfig encoder_config) {
  FakeVideoSendStream* fake_stream =
      new FakeVideoSendStream(std::move(config), std::move(encoder_config));
  video_send_streams_.push_back(fake_stream);
  ++num_created_send_streams_;
  return fake_stream;
}

void FakeCall::DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) {
  auto it = std::find(video_send_streams_.begin(),
                      video_send_streams_.end(),
                      static_cast<FakeVideoSendStream*>(send_stream));
  if (it == video_send_streams_.end()) {
    ADD_FAILURE() << "DestroyVideoSendStream called with unknown parameter.";
  } else {
    delete *it;
    video_send_streams_.erase(it);
  }
}

webrtc::VideoReceiveStream* FakeCall::CreateVideoReceiveStream(
    webrtc::VideoReceiveStream::Config config) {
  video_receive_streams_.push_back(
      new FakeVideoReceiveStream(std::move(config)));
  ++num_created_receive_streams_;
  return video_receive_streams_.back();
}

void FakeCall::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  auto it = std::find(video_receive_streams_.begin(),
                      video_receive_streams_.end(),
                      static_cast<FakeVideoReceiveStream*>(receive_stream));
  if (it == video_receive_streams_.end()) {
    ADD_FAILURE() << "DestroyVideoReceiveStream called with unknown parameter.";
  } else {
    delete *it;
    video_receive_streams_.erase(it);
  }
}

webrtc::FlexfecReceiveStream* FakeCall::CreateFlexfecReceiveStream(
    const webrtc::FlexfecReceiveStream::Config& config) {
  FakeFlexfecReceiveStream* fake_stream = new FakeFlexfecReceiveStream(config);
  flexfec_receive_streams_.push_back(fake_stream);
  ++num_created_receive_streams_;
  return fake_stream;
}

void FakeCall::DestroyFlexfecReceiveStream(
    webrtc::FlexfecReceiveStream* receive_stream) {
  auto it = std::find(flexfec_receive_streams_.begin(),
                      flexfec_receive_streams_.end(),
                      static_cast<FakeFlexfecReceiveStream*>(receive_stream));
  if (it == flexfec_receive_streams_.end()) {
    ADD_FAILURE()
        << "DestroyFlexfecReceiveStream called with unknown parameter.";
  } else {
    delete *it;
    flexfec_receive_streams_.erase(it);
  }
}

webrtc::PacketReceiver* FakeCall::Receiver() {
  return this;
}

FakeCall::DeliveryStatus FakeCall::DeliverPacket(
    webrtc::MediaType media_type,
    const uint8_t* packet,
    size_t length,
    const webrtc::PacketTime& packet_time) {
  EXPECT_GE(length, 12u);
  uint32_t ssrc;
  if (!GetRtpSsrc(packet, length, &ssrc))
    return DELIVERY_PACKET_ERROR;

  if (media_type == webrtc::MediaType::ANY ||
      media_type == webrtc::MediaType::VIDEO) {
    for (auto receiver : video_receive_streams_) {
      if (receiver->GetConfig().rtp.remote_ssrc == ssrc)
        return DELIVERY_OK;
    }
  }
  if (media_type == webrtc::MediaType::ANY ||
      media_type == webrtc::MediaType::AUDIO) {
    for (auto receiver : audio_receive_streams_) {
      if (receiver->GetConfig().rtp.remote_ssrc == ssrc) {
        receiver->DeliverRtp(packet, length, packet_time);
        return DELIVERY_OK;
      }
    }
  }
  return DELIVERY_UNKNOWN_SSRC;
}

void FakeCall::SetStats(const webrtc::Call::Stats& stats) {
  stats_ = stats;
}

int FakeCall::GetNumCreatedSendStreams() const {
  return num_created_send_streams_;
}

int FakeCall::GetNumCreatedReceiveStreams() const {
  return num_created_receive_streams_;
}

webrtc::Call::Stats FakeCall::GetStats() const {
  return stats_;
}

void FakeCall::SetBitrateConfig(
    const webrtc::Call::Config::BitrateConfig& bitrate_config) {
  config_.bitrate_config = bitrate_config;
}

void FakeCall::SignalChannelNetworkState(webrtc::MediaType media,
                                         webrtc::NetworkState state) {
  switch (media) {
    case webrtc::MediaType::AUDIO:
      audio_network_state_ = state;
      break;
    case webrtc::MediaType::VIDEO:
      video_network_state_ = state;
      break;
    case webrtc::MediaType::DATA:
    case webrtc::MediaType::ANY:
      ADD_FAILURE()
          << "SignalChannelNetworkState called with unknown parameter.";
  }
}

void FakeCall::OnTransportOverheadChanged(webrtc::MediaType media,
                                          int transport_overhead_per_packet) {
  switch (media) {
    case webrtc::MediaType::AUDIO:
      audio_transport_overhead_ = transport_overhead_per_packet;
      break;
    case webrtc::MediaType::VIDEO:
      video_transport_overhead_ = transport_overhead_per_packet;
      break;
    case webrtc::MediaType::DATA:
    case webrtc::MediaType::ANY:
      ADD_FAILURE()
          << "SignalChannelNetworkState called with unknown parameter.";
  }
}

void FakeCall::OnSentPacket(const rtc::SentPacket& sent_packet) {
  last_sent_packet_ = sent_packet;
  if (sent_packet.packet_id >= 0) {
    last_sent_nonnegative_packet_id_ = sent_packet.packet_id;
  }
}

}  // namespace cricket
