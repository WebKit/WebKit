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

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/adaptation/resource.h"
#include "api/array_view.h"
#include "api/audio_codecs/audio_format.h"
#include "api/call/audio_sink.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/environment/environment.h"
#include "api/frame_transformer_interface.h"
#include "api/make_ref_counted.h"
#include "api/media_types.h"
#include "api/rtc_error.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/timestamp.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call.h"
#include "call/flexfec_receive_stream.h"
#include "call/packet_receiver.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "media/base/media_channel.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"
#include "video/config/encoder_stream_factory.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {


FakeAudioSendStream::FakeAudioSendStream(int id,
                                         const AudioSendStream::Config& config)
    : id_(id), config_(config) {}

void FakeAudioSendStream::Reconfigure(const AudioSendStream::Config& config,
                                      SetParametersCallback callback) {
  config_ = config;
  InvokeSetParametersCallback(callback, RTCError::OK());
}

const AudioSendStream::Config& FakeAudioSendStream::GetConfig() const {
  return config_;
}

void FakeAudioSendStream::SetStats(const AudioSendStream::Stats& stats) {
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

AudioSendStream::Stats FakeAudioSendStream::GetStats() const {
  return stats_;
}

AudioSendStream::Stats FakeAudioSendStream::GetStats(
    bool /*has_remote_tracks*/) const {
  return stats_;
}

FakeAudioReceiveStream::FakeAudioReceiveStream(
    int id,
    const AudioReceiveStreamInterface::Config& config)
    : id_(id), config_(config) {}

const AudioReceiveStreamInterface::Config& FakeAudioReceiveStream::GetConfig()
    const {
  return config_;
}

void FakeAudioReceiveStream::SetStats(
    const AudioReceiveStreamInterface::Stats& stats) {
  stats_ = stats;
}

bool FakeAudioReceiveStream::VerifyLastPacket(
    ArrayView<const uint8_t> data) const {
  return last_packet_ == Buffer(data.data(), data.size());
}

bool FakeAudioReceiveStream::DeliverRtp(ArrayView<const uint8_t> packet,
                                        int64_t /* packet_time_us */) {
  ++received_packets_;
  last_packet_.SetData(packet);
  return true;
}

void FakeAudioReceiveStream::SetDepacketizerToDecoderFrameTransformer(
    scoped_refptr<FrameTransformerInterface> frame_transformer) {
  config_.frame_transformer = std::move(frame_transformer);
}

void FakeAudioReceiveStream::SetDecoderMap(
    std::map<int, SdpAudioFormat> decoder_map) {
  config_.decoder_map = std::move(decoder_map);
}

void FakeAudioReceiveStream::SetNackHistory(int history_ms) {
  config_.rtp.nack.rtp_history_ms = history_ms;
}

void FakeAudioReceiveStream::SetRtcpMode(RtcpMode mode) {
  config_.rtp.rtcp_mode = mode;
}

void FakeAudioReceiveStream::SetNonSenderRttMeasurement(bool enabled) {
  config_.enable_non_sender_rtt = enabled;
}

void FakeAudioReceiveStream::SetFrameDecryptor(
    scoped_refptr<FrameDecryptorInterface> frame_decryptor) {
  config_.frame_decryptor = std::move(frame_decryptor);
}

AudioReceiveStreamInterface::Stats FakeAudioReceiveStream::GetStats(
    bool /* get_and_clear_legacy_stats */) const {
  return stats_;
}

void FakeAudioReceiveStream::SetSink(AudioSinkInterface* sink) {
  sink_ = sink;
}

void FakeAudioReceiveStream::SetGain(float gain) {
  gain_ = gain;
}

FakeVideoSendStream::FakeVideoSendStream(const Environment& env,
                                         VideoSendStream::Config config,
                                         VideoEncoderConfig encoder_config)
    : env_(env),
      sending_(false),
      config_(std::move(config)),
      codec_settings_set_(false),
      resolution_scaling_enabled_(false),
      framerate_scaling_enabled_(false),
      source_(nullptr),
      num_swapped_frames_(0) {
  RTC_DCHECK(config_.encoder_settings.encoder_factory != nullptr);
  RTC_DCHECK(config_.encoder_settings.bitrate_allocator_factory != nullptr);
  ReconfigureVideoEncoder(std::move(encoder_config));
}

FakeVideoSendStream::~FakeVideoSendStream() {
  if (source_)
    source_->RemoveSink(this);
}

const VideoSendStream::Config& FakeVideoSendStream::GetConfig() const {
  return config_;
}

const VideoEncoderConfig& FakeVideoSendStream::GetEncoderConfig() const {
  return encoder_config_;
}

const std::vector<VideoStream>& FakeVideoSendStream::GetVideoStreams() const {
  return video_streams_;
}

bool FakeVideoSendStream::IsSending() const {
  return sending_;
}

bool FakeVideoSendStream::GetVp8Settings(VideoCodecVP8* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = codec_specific_settings_.vp8;
  return true;
}

bool FakeVideoSendStream::GetVp9Settings(VideoCodecVP9* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = codec_specific_settings_.vp9;
  return true;
}

bool FakeVideoSendStream::GetH264Settings(VideoCodecH264* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = codec_specific_settings_.h264;
  return true;
}

bool FakeVideoSendStream::GetAv1Settings(VideoCodecAV1* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = codec_specific_settings_.av1;
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

void FakeVideoSendStream::OnFrame(const VideoFrame& frame) {
  ++num_swapped_frames_;
  if (!last_frame_ || frame.width() != last_frame_->width() ||
      frame.height() != last_frame_->height() ||
      frame.rotation() != last_frame_->rotation()) {
    if (encoder_config_.video_stream_factory) {
      // Note: only tests set their own EncoderStreamFactory...
      video_streams_ =
          encoder_config_.video_stream_factory->CreateEncoderStreams(
              env_.field_trials(), frame.width(), frame.height(),
              encoder_config_);
    } else {
      VideoEncoder::EncoderInfo encoder_info;
      auto factory = make_ref_counted<EncoderStreamFactory>(encoder_info);

      video_streams_ = factory->CreateEncoderStreams(
          env_.field_trials(), frame.width(), frame.height(), encoder_config_);
    }
  }
  last_frame_ = frame;
}

void FakeVideoSendStream::SetStats(const VideoSendStream::Stats& stats) {
  stats_ = stats;
}

VideoSendStream::Stats FakeVideoSendStream::GetStats() {
  return stats_;
}

void FakeVideoSendStream::SetCsrcs(ArrayView<const uint32_t> csrcs) {}

void FakeVideoSendStream::ReconfigureVideoEncoder(VideoEncoderConfig config) {
  ReconfigureVideoEncoder(std::move(config), nullptr);
}

void FakeVideoSendStream::ReconfigureVideoEncoder(
    VideoEncoderConfig config,
    SetParametersCallback callback) {
  int width, height;
  if (last_frame_) {
    width = last_frame_->width();
    height = last_frame_->height();
  } else {
    width = height = 0;
  }
  if (config.video_stream_factory) {
    // Note: only tests set their own EncoderStreamFactory...
    video_streams_ = config.video_stream_factory->CreateEncoderStreams(
        env_.field_trials(), width, height, config);
  } else {
    VideoEncoder::EncoderInfo encoder_info;
    auto factory = make_ref_counted<EncoderStreamFactory>(encoder_info);

    video_streams_ = factory->CreateEncoderStreams(env_.field_trials(), width,
                                                   height, config);
  }

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
    } else if (config_.rtp.payload_name == "AV1") {
      config.encoder_specific_settings->FillVideoCodecAv1(
          &codec_specific_settings_.av1);
    } else {
      ADD_FAILURE() << "Unsupported encoder payload: "
                    << config_.rtp.payload_name;
    }
  }
  codec_settings_set_ = config.encoder_specific_settings != nullptr;
  encoder_config_ = std::move(config);
  ++num_encoder_reconfigurations_;
  InvokeSetParametersCallback(callback, RTCError::OK());
}

void FakeVideoSendStream::Start() {
  sending_ = true;
}

void FakeVideoSendStream::Stop() {
  sending_ = false;
}

void FakeVideoSendStream::AddAdaptationResource(
    scoped_refptr<Resource> /* resource */) {}

std::vector<scoped_refptr<Resource>>
FakeVideoSendStream::GetAdaptationResources() {
  return {};
}

void FakeVideoSendStream::SetSource(
    VideoSourceInterface<VideoFrame>* source,
    const DegradationPreference& degradation_preference) {
  if (source_)
    source_->RemoveSink(this);
  source_ = source;
  switch (degradation_preference) {
    case DegradationPreference::MAINTAIN_FRAMERATE:
      resolution_scaling_enabled_ = true;
      framerate_scaling_enabled_ = false;
      break;
    case DegradationPreference::MAINTAIN_RESOLUTION:
      resolution_scaling_enabled_ = false;
      framerate_scaling_enabled_ = true;
      break;
    case DegradationPreference::BALANCED:
      resolution_scaling_enabled_ = true;
      framerate_scaling_enabled_ = true;
      break;
    case DegradationPreference::MAINTAIN_FRAMERATE_AND_RESOLUTION:
      resolution_scaling_enabled_ = false;
      framerate_scaling_enabled_ = false;
      break;
  }
  if (source)
    source->AddOrUpdateSink(
        this, resolution_scaling_enabled_ ? sink_wants_ : VideoSinkWants());
}

void FakeVideoSendStream::GenerateKeyFrame(
    const std::vector<std::string>& rids) {
  keyframes_requested_by_rid_ = rids;
}

void FakeVideoSendStream::InjectVideoSinkWants(const VideoSinkWants& wants) {
  sink_wants_ = wants;
  source_->AddOrUpdateSink(this, wants);
}

FakeVideoReceiveStream::FakeVideoReceiveStream(
    VideoReceiveStreamInterface::Config config)
    : config_(std::move(config)), receiving_(false) {}

const VideoReceiveStreamInterface::Config& FakeVideoReceiveStream::GetConfig()
    const {
  return config_;
}

bool FakeVideoReceiveStream::IsReceiving() const {
  return receiving_;
}

void FakeVideoReceiveStream::InjectFrame(const VideoFrame& frame) {
  config_.renderer->OnFrame(frame);
}

VideoReceiveStreamInterface::Stats FakeVideoReceiveStream::GetStats() const {
  return stats_;
}

void FakeVideoReceiveStream::Start() {
  receiving_ = true;
}

void FakeVideoReceiveStream::Stop() {
  receiving_ = false;
}

void FakeVideoReceiveStream::SetStats(
    const VideoReceiveStreamInterface::Stats& stats) {
  stats_ = stats;
}

FakeFlexfecReceiveStream::FakeFlexfecReceiveStream(
    const FlexfecReceiveStream::Config config)
    : config_(std::move(config)) {}

const FlexfecReceiveStream::Config& FakeFlexfecReceiveStream::GetConfig()
    const {
  return config_;
}

void FakeFlexfecReceiveStream::OnRtpPacket(const RtpPacketReceived&) {
  RTC_DCHECK_NOTREACHED() << "Not implemented.";
}

FakeCall::FakeCall(const Environment& env)
    : FakeCall(env, Thread::Current(), Thread::Current()) {}

FakeCall::FakeCall(const Environment& env,
                   TaskQueueBase* worker_thread,
                   TaskQueueBase* network_thread)
    : env_(env),
      network_thread_(network_thread),
      worker_thread_(worker_thread),
      audio_network_state_(kNetworkUp),
      video_network_state_(kNetworkUp),
      num_created_send_streams_(0),
      num_created_receive_streams_(0) {}

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

NetworkState FakeCall::GetNetworkState(MediaType media) const {
  switch (media) {
    case MediaType::AUDIO:
      return audio_network_state_;
    case MediaType::VIDEO:
      return video_network_state_;
    case MediaType::DATA:
    case MediaType::ANY:
    case MediaType::UNSUPPORTED:
      ADD_FAILURE() << "GetNetworkState called with unknown parameter.";
      return kNetworkDown;
  }
  // Even though all the values for the enum class are listed above,the compiler
  // will emit a warning as the method may be called with a value outside of the
  // valid enum range, unless this case is also handled.
  ADD_FAILURE() << "GetNetworkState called with unknown parameter.";
  return kNetworkDown;
}

AudioSendStream* FakeCall::CreateAudioSendStream(
    const AudioSendStream::Config& config) {
  FakeAudioSendStream* fake_stream =
      new FakeAudioSendStream(next_stream_id_++, config);
  audio_send_streams_.push_back(fake_stream);
  ++num_created_send_streams_;
  return fake_stream;
}

void FakeCall::DestroyAudioSendStream(AudioSendStream* send_stream) {
  auto it = absl::c_find(audio_send_streams_,
                         static_cast<FakeAudioSendStream*>(send_stream));
  if (it == audio_send_streams_.end()) {
    ADD_FAILURE() << "DestroyAudioSendStream called with unknown parameter.";
  } else {
    delete *it;
    audio_send_streams_.erase(it);
  }
}

AudioReceiveStreamInterface* FakeCall::CreateAudioReceiveStream(
    const AudioReceiveStreamInterface::Config& config) {
  audio_receive_streams_.push_back(
      new FakeAudioReceiveStream(next_stream_id_++, config));
  ++num_created_receive_streams_;
  return audio_receive_streams_.back();
}

void FakeCall::DestroyAudioReceiveStream(
    AudioReceiveStreamInterface* receive_stream) {
  auto it = absl::c_find(audio_receive_streams_,
                         static_cast<FakeAudioReceiveStream*>(receive_stream));
  if (it == audio_receive_streams_.end()) {
    ADD_FAILURE() << "DestroyAudioReceiveStream called with unknown parameter.";
  } else {
    delete *it;
    audio_receive_streams_.erase(it);
  }
}

VideoSendStream* FakeCall::CreateVideoSendStream(
    VideoSendStream::Config config,
    VideoEncoderConfig encoder_config) {
  FakeVideoSendStream* fake_stream = new FakeVideoSendStream(
      env_, std::move(config), std::move(encoder_config));
  video_send_streams_.push_back(fake_stream);
  ++num_created_send_streams_;
  return fake_stream;
}

void FakeCall::DestroyVideoSendStream(VideoSendStream* send_stream) {
  auto it = absl::c_find(video_send_streams_,
                         static_cast<FakeVideoSendStream*>(send_stream));
  if (it == video_send_streams_.end()) {
    ADD_FAILURE() << "DestroyVideoSendStream called with unknown parameter.";
  } else {
    delete *it;
    video_send_streams_.erase(it);
  }
}

VideoReceiveStreamInterface* FakeCall::CreateVideoReceiveStream(
    VideoReceiveStreamInterface::Config config) {
  video_receive_streams_.push_back(
      new FakeVideoReceiveStream(std::move(config)));
  ++num_created_receive_streams_;
  return video_receive_streams_.back();
}

void FakeCall::DestroyVideoReceiveStream(
    VideoReceiveStreamInterface* receive_stream) {
  auto it = absl::c_find(video_receive_streams_,
                         static_cast<FakeVideoReceiveStream*>(receive_stream));
  if (it == video_receive_streams_.end()) {
    ADD_FAILURE() << "DestroyVideoReceiveStream called with unknown parameter.";
  } else {
    delete *it;
    video_receive_streams_.erase(it);
  }
}

FlexfecReceiveStream* FakeCall::CreateFlexfecReceiveStream(
    const FlexfecReceiveStream::Config config) {
  FakeFlexfecReceiveStream* fake_stream =
      new FakeFlexfecReceiveStream(std::move(config));
  flexfec_receive_streams_.push_back(fake_stream);
  ++num_created_receive_streams_;
  return fake_stream;
}

void FakeCall::DestroyFlexfecReceiveStream(
    FlexfecReceiveStream* receive_stream) {
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

void FakeCall::AddAdaptationResource(scoped_refptr<Resource> /* resource */) {}

PacketReceiver* FakeCall::Receiver() {
  return this;
}

void FakeCall::DeliverRtpPacket(
    MediaType media_type,
    RtpPacketReceived packet,
    OnUndemuxablePacketHandler undemuxable_packet_handler) {
  if (!DeliverPacketInternal(media_type, packet.Ssrc(), packet.Buffer(),
                             packet.arrival_time())) {
    if (undemuxable_packet_handler(packet)) {
      DeliverPacketInternal(media_type, packet.Ssrc(), packet.Buffer(),
                            packet.arrival_time());
    }
  }
  last_received_rtp_packet_ = packet;
}

bool FakeCall::DeliverPacketInternal(MediaType media_type,
                                     uint32_t ssrc,
                                     const CopyOnWriteBuffer& packet,
                                     Timestamp arrival_time) {
  EXPECT_GE(packet.size(), 12u);
  RTC_DCHECK(arrival_time.IsFinite());
  RTC_DCHECK(media_type == MediaType::AUDIO || media_type == MediaType::VIDEO);

  if (media_type == MediaType::VIDEO) {
    for (auto receiver : video_receive_streams_) {
      if (receiver->GetConfig().rtp.remote_ssrc == ssrc ||
          receiver->GetConfig().rtp.rtx_ssrc == ssrc) {
        ++delivered_packets_by_ssrc_[ssrc];
        return true;
      }
    }
  }
  if (media_type == MediaType::AUDIO) {
    for (auto receiver : audio_receive_streams_) {
      if (receiver->GetConfig().rtp.remote_ssrc == ssrc) {
        receiver->DeliverRtp(packet, arrival_time.us());
        ++delivered_packets_by_ssrc_[ssrc];
        return true;
      }
    }
  }
  return false;
}

void FakeCall::SetStats(const Call::Stats& stats) {
  stats_ = stats;
}

int FakeCall::GetNumCreatedSendStreams() const {
  return num_created_send_streams_;
}

int FakeCall::GetNumCreatedReceiveStreams() const {
  return num_created_receive_streams_;
}

Call::Stats FakeCall::GetStats() const {
  return stats_;
}

TaskQueueBase* FakeCall::network_thread() const {
  return network_thread_;
}

TaskQueueBase* FakeCall::worker_thread() const {
  return worker_thread_;
}

void FakeCall::SignalChannelNetworkState(MediaType media, NetworkState state) {
  switch (media) {
    case MediaType::AUDIO:
      audio_network_state_ = state;
      break;
    case MediaType::VIDEO:
      video_network_state_ = state;
      break;
    case MediaType::DATA:
    case MediaType::ANY:
    case MediaType::UNSUPPORTED:
      ADD_FAILURE()
          << "SignalChannelNetworkState called with unknown parameter.";
  }
}

void FakeCall::OnAudioTransportOverheadChanged(
    int /* transport_overhead_per_packet */) {}

void FakeCall::OnLocalSsrcUpdated(AudioReceiveStreamInterface& stream,
                                  uint32_t local_ssrc) {
  auto& fake_stream = static_cast<FakeAudioReceiveStream&>(stream);
  fake_stream.SetLocalSsrc(local_ssrc);
}

void FakeCall::OnLocalSsrcUpdated(VideoReceiveStreamInterface& stream,
                                  uint32_t local_ssrc) {
  auto& fake_stream = static_cast<FakeVideoReceiveStream&>(stream);
  fake_stream.SetLocalSsrc(local_ssrc);
}

void FakeCall::OnLocalSsrcUpdated(FlexfecReceiveStream& stream,
                                  uint32_t local_ssrc) {
  auto& fake_stream = static_cast<FakeFlexfecReceiveStream&>(stream);
  fake_stream.SetLocalSsrc(local_ssrc);
}

void FakeCall::OnUpdateSyncGroup(AudioReceiveStreamInterface& stream,
                                 absl::string_view sync_group) {
  auto& fake_stream = static_cast<FakeAudioReceiveStream&>(stream);
  fake_stream.SetSyncGroup(sync_group);
}

void FakeCall::OnSentPacket(const SentPacketInfo& sent_packet) {
  last_sent_packet_ = sent_packet;
  if (sent_packet.packet_id >= 0) {
    last_sent_nonnegative_packet_id_ = sent_packet.packet_id;
  }
}

}  // namespace webrtc
