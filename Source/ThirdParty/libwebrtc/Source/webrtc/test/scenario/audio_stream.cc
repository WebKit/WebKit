/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/audio_stream.h"

#include "rtc_base/bitrateallocationstrategy.h"
#include "test/call_test.h"

#if WEBRTC_ENABLE_PROTOBUF
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_coding/audio_network_adaptor/config.pb.h"
#else
#include "modules/audio_coding/audio_network_adaptor/config.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()
#endif

namespace webrtc {
namespace test {
namespace {
absl::optional<std::string> CreateAdaptationString(
    AudioStreamConfig::NetworkAdaptation config) {
#if WEBRTC_ENABLE_PROTOBUF

  audio_network_adaptor::config::ControllerManager cont_conf;
  if (config.frame.max_rate_for_60_ms.IsFinite()) {
    auto controller =
        cont_conf.add_controllers()->mutable_frame_length_controller();
    controller->set_fl_decreasing_packet_loss_fraction(
        config.frame.min_packet_loss_for_decrease);
    controller->set_fl_increasing_packet_loss_fraction(
        config.frame.max_packet_loss_for_increase);

    controller->set_fl_20ms_to_60ms_bandwidth_bps(
        config.frame.min_rate_for_20_ms.bps<int32_t>());
    controller->set_fl_60ms_to_20ms_bandwidth_bps(
        config.frame.max_rate_for_60_ms.bps<int32_t>());

    if (config.frame.max_rate_for_120_ms.IsFinite()) {
      controller->set_fl_60ms_to_120ms_bandwidth_bps(
          config.frame.min_rate_for_60_ms.bps<int32_t>());
      controller->set_fl_120ms_to_60ms_bandwidth_bps(
          config.frame.max_rate_for_120_ms.bps<int32_t>());
    }
  }
  cont_conf.add_controllers()->mutable_bitrate_controller();
  std::string config_string = cont_conf.SerializeAsString();
  return config_string;
#else
  RTC_LOG(LS_ERROR) << "audio_network_adaptation is enabled"
                       " but WEBRTC_ENABLE_PROTOBUF is false.\n"
                       "Ignoring settings.";
  return absl::nullopt;
#endif  // WEBRTC_ENABLE_PROTOBUF
}
}  // namespace

SendAudioStream::SendAudioStream(
    CallClient* sender,
    AudioStreamConfig config,
    rtc::scoped_refptr<AudioEncoderFactory> encoder_factory,
    Transport* send_transport)
    : sender_(sender), config_(config) {
  AudioSendStream::Config send_config(send_transport,
                                      /*media_transport=*/nullptr);
  ssrc_ = sender->GetNextAudioSsrc();
  send_config.rtp.ssrc = ssrc_;
  SdpAudioFormat::Parameters sdp_params;
  if (config.source.channels == 2)
    sdp_params["stereo"] = "1";
  if (config.encoder.initial_frame_length != TimeDelta::ms(20))
    sdp_params["ptime"] =
        std::to_string(config.encoder.initial_frame_length.ms());

  // SdpAudioFormat::num_channels indicates that the encoder is capable of
  // stereo, but the actual channel count used is based on the "stereo"
  // parameter.
  send_config.send_codec_spec = AudioSendStream::Config::SendCodecSpec(
      CallTest::kAudioSendPayloadType, {"opus", 48000, 2, sdp_params});
  RTC_DCHECK_LE(config.source.channels, 2);
  send_config.encoder_factory = encoder_factory;

  if (config.encoder.fixed_rate)
    send_config.send_codec_spec->target_bitrate_bps =
        config.encoder.fixed_rate->bps();

  if (config.network_adaptation) {
    send_config.audio_network_adaptor_config =
        CreateAdaptationString(config.adapt);
  }
  if (config.encoder.allocate_bitrate ||
      config.stream.in_bandwidth_estimation) {
    DataRate min_rate = DataRate::Infinity();
    DataRate max_rate = DataRate::Infinity();
    if (config.encoder.fixed_rate) {
      min_rate = *config.encoder.fixed_rate;
      max_rate = *config.encoder.fixed_rate;
    } else {
      min_rate = *config.encoder.min_rate;
      max_rate = *config.encoder.max_rate;
    }
    if (field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")) {
      TimeDelta min_frame_length = TimeDelta::ms(20);
      // Note, depends on WEBRTC_OPUS_SUPPORT_120MS_PTIME being set, which is
      // the default.
      TimeDelta max_frame_length = TimeDelta::ms(120);
      DataSize rtp_overhead = DataSize::bytes(12);
      // Note that this does not include rtp extension overhead and will not
      // follow updates in the transport overhead over time.
      DataSize total_overhead =
          sender_->transport_.packet_overhead() + rtp_overhead;

      min_rate += total_overhead / max_frame_length;
      // In WebRTCVoiceEngine the max rate is also based on the max frame
      // length.
      max_rate += total_overhead / min_frame_length;
    }
    send_config.min_bitrate_bps = min_rate.bps();
    send_config.max_bitrate_bps = max_rate.bps();
  }

  if (config.stream.in_bandwidth_estimation) {
    send_config.send_codec_spec->transport_cc_enabled = true;
    send_config.rtp.extensions = {
        {RtpExtension::kTransportSequenceNumberUri, 8}};
  }

  if (config.encoder.priority_rate) {
    send_config.track_id = sender->GetNextPriorityId();
    sender_->call_->SetBitrateAllocationStrategy(
        absl::make_unique<rtc::AudioPriorityBitrateAllocationStrategy>(
            send_config.track_id,
            config.encoder.priority_rate->bps<uint32_t>()));
  }
  send_stream_ = sender_->call_->CreateAudioSendStream(send_config);
  if (field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")) {
    sender->call_->OnAudioTransportOverheadChanged(
        sender_->transport_.packet_overhead().bytes());
  }
}

SendAudioStream::~SendAudioStream() {
  sender_->call_->DestroyAudioSendStream(send_stream_);
}

void SendAudioStream::Start() {
  send_stream_->Start();
  sender_->call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
}

ColumnPrinter SendAudioStream::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "audio_target_rate",
      [this](rtc::SimpleStringBuilder& sb) {
        AudioSendStream::Stats stats = send_stream_->GetStats();
        sb.AppendFormat("%.0lf", stats.target_bitrate_bps / 8.0);
      },
      64);
}

ReceiveAudioStream::ReceiveAudioStream(
    CallClient* receiver,
    AudioStreamConfig config,
    SendAudioStream* send_stream,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    Transport* feedback_transport)
    : receiver_(receiver), config_(config) {
  AudioReceiveStream::Config recv_config;
  recv_config.rtp.local_ssrc = CallTest::kReceiverLocalAudioSsrc;
  recv_config.rtcp_send_transport = feedback_transport;
  recv_config.rtp.remote_ssrc = send_stream->ssrc_;
  receiver->ssrc_media_types_[recv_config.rtp.remote_ssrc] = MediaType::AUDIO;
  if (config.stream.in_bandwidth_estimation) {
    recv_config.rtp.transport_cc = true;
    recv_config.rtp.extensions = {
        {RtpExtension::kTransportSequenceNumberUri, 8}};
  }
  receiver_->AddExtensions(recv_config.rtp.extensions);
  recv_config.decoder_factory = decoder_factory;
  recv_config.decoder_map = {
      {CallTest::kAudioSendPayloadType, {"opus", 48000, 2}}};
  recv_config.sync_group = config.render.sync_group;
  receive_stream_ = receiver_->call_->CreateAudioReceiveStream(recv_config);
}
ReceiveAudioStream::~ReceiveAudioStream() {
  receiver_->call_->DestroyAudioReceiveStream(receive_stream_);
}

void ReceiveAudioStream::Start() {
  receive_stream_->Start();
  receiver_->call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
}

AudioStreamPair::~AudioStreamPair() = default;

AudioStreamPair::AudioStreamPair(
    CallClient* sender,
    rtc::scoped_refptr<AudioEncoderFactory> encoder_factory,
    CallClient* receiver,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    AudioStreamConfig config)
    : config_(config),
      send_stream_(sender, config, encoder_factory, &sender->transport_),
      receive_stream_(receiver,
                      config,
                      &send_stream_,
                      decoder_factory,
                      &receiver->transport_) {}

}  // namespace test
}  // namespace webrtc
