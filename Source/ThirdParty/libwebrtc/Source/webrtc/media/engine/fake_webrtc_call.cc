/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/fake_webrtc_call.h"

#include <utility>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/call/audio_sink.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/thread.h"

namespace cricket {

using ::webrtc::ParseRtpSsrc;

FakeAudioSendStream::FakeAudioSendStream(
    int id,
    const webrtc::AudioSendStream::Config& config)
    : id_(id), config_(config) {}

void FakeAudioSendStream::Reconfigure(
    const webrtc::AudioSendStream::Config& config) {
  config_ = config;
}

const webrtc::AudioSendStream::Config& FakeAudioSendStream::GetConfig() const {
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
                                             int payload_frequency,
                                             int event,
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

webrtc::AudioSendStream::Stats FakeAudioSendStream::GetStats(
    bool /*has_remote_tracks*/) const {
  return stats_;
}

FakeAudioReceiveStream::FakeAudioReceiveStream(
    int id,
    const webrtc::AudioReceiveStreamInterface::Config& config)
    : id_(id), config_(config) {}

const webrtc::AudioReceiveStreamInterface::Config&
FakeAudioReceiveStream::GetConfig() const {
  return config_;
}

void FakeAudioReceiveStream::SetStats(
    const webrtc::AudioReceiveStreamInterface::Stats& stats) {
  stats_ = stats;
}

bool FakeAudioReceiveStream::VerifyLastPacket(const uint8_t* data,
                                              size_t length) const {
  return last_packet_ == rtc::Buffer(data, length);
}

bool FakeAudioReceiveStream::DeliverRtp(const uint8_t* packet,
                                        size_t length,
                                        int64_t /* packet_time_us */) {
  ++received_packets_;
  last_packet_.SetData(packet, length);
  return true;
}

void FakeAudioReceiveStream::SetDepacketizerToDecoderFrameTransformer(
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  config_.frame_transformer = std::move(frame_transformer);
}

void FakeAudioReceiveStream::SetDecoderMap(
    std::map<int, webrtc::SdpAudioFormat> decoder_map) {
  config_.decoder_map = std::move(decoder_map);
}

void FakeAudioReceiveStream::SetNackHistory(int history_ms) {
  config_.rtp.nack.rtp_history_ms = history_ms;
}

void FakeAudioReceiveStream::SetNonSenderRttMeasurement(bool enabled) {
  config_.enable_non_sender_rtt = enabled;
}

void FakeAudioReceiveStream::SetFrameDecryptor(
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {
  config_.frame_decryptor = std::move(frame_decryptor);
}

void FakeAudioReceiveStream::SetRtpExtensions(
    std::vector<webrtc::RtpExtension> extensions) {
  config_.rtp.extensions = std::move(extensions);
}

const std::vector<webrtc::RtpExtension>&
FakeAudioReceiveStream::GetRtpExtensions() const {
  return config_.rtp.extensions;
}

webrtc::RtpHeaderExtensionMap FakeAudioReceiveStream::GetRtpExtensionMap()
    const {
  return webrtc::RtpHeaderExtensionMap(config_.rtp.extensions);
}

webrtc::AudioReceiveStreamInterface::Stats FakeAudioReceiveStream::GetStats(
    bool get_and_clear_legacy_stats) const {
  return stats_;
}

void FakeAudioReceiveStream::SetSink(webrtc::AudioSinkInterface* sink) {
  sink_ = sink;
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
      framerate_scaling_enabled_(false),
      source_(nullptr),
      num_swapped_frames_(0) {
  RTC_DCHECK(config.encoder_settings.encoder_factory != nullptr);
  RTC_DCHECK(config.encoder_settings.bitrate_allocator_factory != nullptr);
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

  *settings = codec_specific_settings_.vp8;
  return true;
}

bool FakeVideoSendStream::GetVp9Settings(
    webrtc::VideoCodecVP9* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = codec_specific_settings_.vp9;
  return true;
}

bool FakeVideoSendStream::GetH264Settings(
    webrtc::VideoCodecH264* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = codec_specific_settings_.h264;
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
  if (!last_frame_ || frame.width() != last_frame_->width() ||
      frame.height() != last_frame_->height() ||
      frame.rotation() != last_frame_->rotation()) {
    video_streams_ = encoder_config_.video_stream_factory->CreateEncoderStreams(
        frame.width(), frame.height(), encoder_config_);
  }
  last_frame_ = frame;
}

void FakeVideoSendStream::SetStats(
    const webrtc::VideoSendStream::Stats& stats) {
  stats_ = stats;
}

webrtc::VideoSendStream::Stats FakeVideoSendStream::GetStats() {
  return stats_;
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
  video_streams_ =
      config.video_stream_factory->CreateEncoderStreams(width, height, config);
  if (config.encoder_specific_settings != nullptr) {
    const unsigned char num_temporal_layers = static_cast<unsigned char>(
        video_streams_.back().num_temporal_layers.value_or(1));
    if (config_.rtp.payload_name == "VP8") {
      config.encoder_specific_settings->FillVideoCodecVp8(
          &codec_specific_settings_.vp8);
      if (!video_streams_.empty()) {
        codec_specific_settings_.vp8.numberOfTemporalLayers =
            num_temporal_layers;
      }
    } else if (config_.rtp.payload_name == "VP9") {
      config.encoder_specific_settings->FillVideoCodecVp9(
          &codec_specific_settings_.vp9);
      if (!video_streams_.empty()) {
        codec_specific_settings_.vp9.numberOfTemporalLayers =
            num_temporal_layers;
      }
    } else if (config_.rtp.payload_name == "H264") {
      codec_specific_settings_.h264.numberOfTemporalLayers =
          num_temporal_layers;
    } else {
      ADD_FAILURE() << "Unsupported encoder payload: "
                    << config_.rtp.payload_name;
    }
  }
  codec_settings_set_ = config.encoder_specific_settings != nullptr;
  encoder_config_ = std::move(config);
  ++num_encoder_reconfigurations_;
}

void FakeVideoSendStream::UpdateActiveSimulcastLayers(
    const std::vector<bool> active_layers) {
  sending_ = false;
  for (const bool active_layer : active_layers) {
    if (active_layer) {
      sending_ = true;
      break;
    }
  }
}

void FakeVideoSendStream::Start() {
  sending_ = true;
}

void FakeVideoSendStream::Stop() {
  sending_ = false;
}

void FakeVideoSendStream::AddAdaptationResource(
    rtc::scoped_refptr<webrtc::Resource> resource) {}

std::vector<rtc::scoped_refptr<webrtc::Resource>>
FakeVideoSendStream::GetAdaptationResources() {
  return {};
}

void FakeVideoSendStream::SetSource(
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
    const webrtc::DegradationPreference& degradation_preference) {
  if (source_)
    source_->RemoveSink(this);
  source_ = source;
  switch (degradation_preference) {
    case webrtc::DegradationPreference::MAINTAIN_FRAMERATE:
      resolution_scaling_enabled_ = true;
      framerate_scaling_enabled_ = false;
      break;
    case webrtc::DegradationPreference::MAINTAIN_RESOLUTION:
      resolution_scaling_enabled_ = false;
      framerate_scaling_enabled_ = true;
      break;
    case webrtc::DegradationPreference::BALANCED:
      resolution_scaling_enabled_ = true;
      framerate_scaling_enabled_ = true;
      break;
    case webrtc::DegradationPreference::DISABLED:
      resolution_scaling_enabled_ = false;
      framerate_scaling_enabled_ = false;
      break;
  }
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
    webrtc::VideoReceiveStreamInterface::Config config)
    : config_(std::move(config)), receiving_(false) {}

const webrtc::VideoReceiveStreamInterface::Config&
FakeVideoReceiveStream::GetConfig() const {
  return config_;
}

bool FakeVideoReceiveStream::IsReceiving() const {
  return receiving_;
}

void FakeVideoReceiveStream::InjectFrame(const webrtc::VideoFrame& frame) {
  config_.renderer->OnFrame(frame);
}

webrtc::VideoReceiveStreamInterface::Stats FakeVideoReceiveStream::GetStats()
    const {
  return stats_;
}

void FakeVideoReceiveStream::SetRtpExtensions(
    std::vector<webrtc::RtpExtension> extensions) {
  config_.rtp.extensions = std::move(extensions);
}

webrtc::RtpHeaderExtensionMap FakeVideoReceiveStream::GetRtpExtensionMap()
    const {
  return webrtc::RtpHeaderExtensionMap(config_.rtp.extensions);
}

void FakeVideoReceiveStream::Start() {
  receiving_ = true;
}

void FakeVideoReceiveStream::Stop() {
  receiving_ = false;
}

void FakeVideoReceiveStream::SetStats(
    const webrtc::VideoReceiveStreamInterface::Stats& stats) {
  stats_ = stats;
}

FakeFlexfecReceiveStream::FakeFlexfecReceiveStream(
    const webrtc::FlexfecReceiveStream::Config config)
    : config_(std::move(config)) {}

void FakeFlexfecReceiveStream::SetRtpExtensions(
    std::vector<webrtc::RtpExtension> extensions) {
  config_.rtp.extensions = std::move(extensions);
}

webrtc::RtpHeaderExtensionMap FakeFlexfecReceiveStream::GetRtpExtensionMap()
    const {
  return webrtc::RtpHeaderExtensionMap(config_.rtp.extensions);
}

const webrtc::FlexfecReceiveStream::Config&
FakeFlexfecReceiveStream::GetConfig() const {
  return config_;
}

void FakeFlexfecReceiveStream::OnRtpPacket(const webrtc::RtpPacketReceived&) {
  RTC_DCHECK_NOTREACHED() << "Not implemented.";
}

FakeCall::FakeCall(webrtc::test::ScopedKeyValueConfig* field_trials)
    : FakeCall(rtc::Thread::Current(), rtc::Thread::Current(), field_trials) {}

FakeCall::FakeCall(webrtc::TaskQueueBase* worker_thread,
                   webrtc::TaskQueueBase* network_thread,
                   webrtc::test::ScopedKeyValueConfig* field_trials)
    : network_thread_(network_thread),
      worker_thread_(worker_thread),
      audio_network_state_(webrtc::kNetworkUp),
      video_network_state_(webrtc::kNetworkUp),
      num_created_send_streams_(0),
      num_created_receive_streams_(0),
      trials_(field_trials ? field_trials : &fallback_trials_) {}

FakeCall::~FakeCall() {
  EXPECT_EQ(0u, video_send_streams_.size());
  EXPECT_EQ(0u, audio_send_streams_.size());
  EXPECT_EQ(0u, video_receive_streams_.size());
  EXPECT_EQ(0u, audio_receive_streams_.size());
}

const std::vector<FakeVideoSendStream*>& FakeCall::GetVideoSendStreams() {
  return video_send_streams_;
}

const std::vector<FakeVideoReceiveStream*>& FakeCall::GetVideoReceiveStreams() {
  return video_receive_streams_;
}

const FakeVideoReceiveStream* FakeCall::GetVideoReceiveStream(uint32_t ssrc) {
  for (const auto* p : GetVideoReceiveStreams()) {
    if (p->GetConfig().rtp.remote_ssrc == ssrc) {
      return p;
    }
  }
  return nullptr;
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
  FakeAudioSendStream* fake_stream =
      new FakeAudioSendStream(next_stream_id_++, config);
  audio_send_streams_.push_back(fake_stream);
  ++num_created_send_streams_;
  return fake_stream;
}

void FakeCall::DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) {
  auto it = absl::c_find(audio_send_streams_,
                         static_cast<FakeAudioSendStream*>(send_stream));
  if (it == audio_send_streams_.end()) {
    ADD_FAILURE() << "DestroyAudioSendStream called with unknown parameter.";
  } else {
    delete *it;
    audio_send_streams_.erase(it);
  }
}

webrtc::AudioReceiveStreamInterface* FakeCall::CreateAudioReceiveStream(
    const webrtc::AudioReceiveStreamInterface::Config& config) {
  audio_receive_streams_.push_back(
      new FakeAudioReceiveStream(next_stream_id_++, config));
  ++num_created_receive_streams_;
  return audio_receive_streams_.back();
}

void FakeCall::DestroyAudioReceiveStream(
    webrtc::AudioReceiveStreamInterface* receive_stream) {
  auto it = absl::c_find(audio_receive_streams_,
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
  auto it = absl::c_find(video_send_streams_,
                         static_cast<FakeVideoSendStream*>(send_stream));
  if (it == video_send_streams_.end()) {
    ADD_FAILURE() << "DestroyVideoSendStream called with unknown parameter.";
  } else {
    delete *it;
    video_send_streams_.erase(it);
  }
}

webrtc::VideoReceiveStreamInterface* FakeCall::CreateVideoReceiveStream(
    webrtc::VideoReceiveStreamInterface::Config config) {
  video_receive_streams_.push_back(
      new FakeVideoReceiveStream(std::move(config)));
  ++num_created_receive_streams_;
  return video_receive_streams_.back();
}

void FakeCall::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStreamInterface* receive_stream) {
  auto it = absl::c_find(video_receive_streams_,
                         static_cast<FakeVideoReceiveStream*>(receive_stream));
  if (it == video_receive_streams_.end()) {
    ADD_FAILURE() << "DestroyVideoReceiveStream called with unknown parameter.";
  } else {
    delete *it;
    video_receive_streams_.erase(it);
  }
}

webrtc::FlexfecReceiveStream* FakeCall::CreateFlexfecReceiveStream(
    const webrtc::FlexfecReceiveStream::Config config) {
  FakeFlexfecReceiveStream* fake_stream =
      new FakeFlexfecReceiveStream(std::move(config));
  flexfec_receive_streams_.push_back(fake_stream);
  ++num_created_receive_streams_;
  return fake_stream;
}

void FakeCall::DestroyFlexfecReceiveStream(
    webrtc::FlexfecReceiveStream* receive_stream) {
  auto it =
      absl::c_find(flexfec_receive_streams_,
                   static_cast<FakeFlexfecReceiveStream*>(receive_stream));
  if (it == flexfec_receive_streams_.end()) {
    ADD_FAILURE()
        << "DestroyFlexfecReceiveStream called with unknown parameter.";
  } else {
    delete *it;
    flexfec_receive_streams_.erase(it);
  }
}

void FakeCall::AddAdaptationResource(
    rtc::scoped_refptr<webrtc::Resource> resource) {}

webrtc::PacketReceiver* FakeCall::Receiver() {
  return this;
}

FakeCall::DeliveryStatus FakeCall::DeliverPacket(webrtc::MediaType media_type,
                                                 rtc::CopyOnWriteBuffer packet,
                                                 int64_t packet_time_us) {
  EXPECT_GE(packet.size(), 12u);
  RTC_DCHECK(media_type == webrtc::MediaType::AUDIO ||
             media_type == webrtc::MediaType::VIDEO);

  if (!webrtc::IsRtpPacket(packet)) {
    return DELIVERY_PACKET_ERROR;
  }

  uint32_t ssrc = ParseRtpSsrc(packet);
  if (media_type == webrtc::MediaType::VIDEO) {
    for (auto receiver : video_receive_streams_) {
      if (receiver->GetConfig().rtp.remote_ssrc == ssrc) {
        ++delivered_packets_by_ssrc_[ssrc];
        return DELIVERY_OK;
      }
    }
  }
  if (media_type == webrtc::MediaType::AUDIO) {
    for (auto receiver : audio_receive_streams_) {
      if (receiver->GetConfig().rtp.remote_ssrc == ssrc) {
        receiver->DeliverRtp(packet.cdata(), packet.size(), packet_time_us);
        ++delivered_packets_by_ssrc_[ssrc];
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

webrtc::TaskQueueBase* FakeCall::network_thread() const {
  return network_thread_;
}

webrtc::TaskQueueBase* FakeCall::worker_thread() const {
  return worker_thread_;
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

void FakeCall::OnAudioTransportOverheadChanged(
    int transport_overhead_per_packet) {}

void FakeCall::OnLocalSsrcUpdated(webrtc::AudioReceiveStreamInterface& stream,
                                  uint32_t local_ssrc) {
  auto& fake_stream = static_cast<FakeAudioReceiveStream&>(stream);
  fake_stream.SetLocalSsrc(local_ssrc);
}

void FakeCall::OnLocalSsrcUpdated(webrtc::VideoReceiveStreamInterface& stream,
                                  uint32_t local_ssrc) {
  auto& fake_stream = static_cast<FakeVideoReceiveStream&>(stream);
  fake_stream.SetLocalSsrc(local_ssrc);
}

void FakeCall::OnLocalSsrcUpdated(webrtc::FlexfecReceiveStream& stream,
                                  uint32_t local_ssrc) {
  auto& fake_stream = static_cast<FakeFlexfecReceiveStream&>(stream);
  fake_stream.SetLocalSsrc(local_ssrc);
}

void FakeCall::OnUpdateSyncGroup(webrtc::AudioReceiveStreamInterface& stream,
                                 absl::string_view sync_group) {
  auto& fake_stream = static_cast<FakeAudioReceiveStream&>(stream);
  fake_stream.SetSyncGroup(sync_group);
}

void FakeCall::OnSentPacket(const rtc::SentPacket& sent_packet) {
  last_sent_packet_ = sent_packet;
  if (sent_packet.packet_id >= 0) {
    last_sent_nonnegative_packet_id_ = sent_packet.packet_id;
  }
}

}  // namespace cricket
