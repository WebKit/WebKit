/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/video_stream.h"

#include <algorithm>
#include <utility>

#include "media/base/mediaconstants.h"
#include "media/engine/internaldecoderfactory.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/webrtcvideoengine.h"
#include "test/call_test.h"
#include "test/fake_encoder.h"
#include "test/function_video_encoder_factory.h"
#include "test/scenario/hardware_codecs.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {
namespace {
constexpr int kDefaultMaxQp = cricket::WebRtcVideoChannel::kDefaultQpMax;
const int kVideoRotationRtpExtensionId = 4;
uint8_t CodecTypeToPayloadType(VideoCodecType codec_type) {
  switch (codec_type) {
    case VideoCodecType::kVideoCodecGeneric:
      return CallTest::kFakeVideoSendPayloadType;
    case VideoCodecType::kVideoCodecVP8:
      return CallTest::kPayloadTypeVP8;
    case VideoCodecType::kVideoCodecVP9:
      return CallTest::kPayloadTypeVP9;
    case VideoCodecType::kVideoCodecH264:
      return CallTest::kPayloadTypeH264;
    default:
      RTC_NOTREACHED();
  }
  return {};
}
std::string CodecTypeToCodecName(VideoCodecType codec_type) {
  switch (codec_type) {
    case VideoCodecType::kVideoCodecGeneric:
      return "";
    case VideoCodecType::kVideoCodecVP8:
      return cricket::kVp8CodecName;
    case VideoCodecType::kVideoCodecVP9:
      return cricket::kVp9CodecName;
    case VideoCodecType::kVideoCodecH264:
      return cricket::kH264CodecName;
    default:
      RTC_NOTREACHED();
  }
  return {};
}
std::vector<RtpExtension> GetVideoRtpExtensions(
    const VideoStreamConfig config) {
  return {RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       kTransportSequenceNumberExtensionId),
          RtpExtension(RtpExtension::kVideoContentTypeUri,
                       kVideoContentTypeExtensionId),
          RtpExtension(RtpExtension::kVideoRotationUri,
                       kVideoRotationRtpExtensionId)};
}

VideoSendStream::Config CreateVideoSendStreamConfig(VideoStreamConfig config,
                                                    std::vector<uint32_t> ssrcs,
                                                    Transport* send_transport) {
  VideoSendStream::Config send_config(send_transport);
  send_config.rtp.payload_name = CodecTypeToPayloadString(config.encoder.codec);
  send_config.rtp.payload_type = CodecTypeToPayloadType(config.encoder.codec);

  send_config.rtp.ssrcs = ssrcs;
  send_config.rtp.extensions = GetVideoRtpExtensions(config);

  if (config.stream.use_flexfec) {
    send_config.rtp.flexfec.payload_type = CallTest::kFlexfecPayloadType;
    send_config.rtp.flexfec.ssrc = CallTest::kFlexfecSendSsrc;
    send_config.rtp.flexfec.protected_media_ssrcs = ssrcs;
  }
  if (config.stream.use_ulpfec) {
    send_config.rtp.ulpfec.red_payload_type = CallTest::kRedPayloadType;
    send_config.rtp.ulpfec.ulpfec_payload_type = CallTest::kUlpfecPayloadType;
    send_config.rtp.ulpfec.red_rtx_payload_type = CallTest::kRtxRedPayloadType;
  }
  return send_config;
}
rtc::scoped_refptr<VideoEncoderConfig::EncoderSpecificSettings>
CreateEncoderSpecificSettings(VideoStreamConfig config) {
  using Codec = VideoStreamConfig::Encoder::Codec;
  switch (config.encoder.codec) {
    case Codec::kVideoCodecH264: {
      VideoCodecH264 h264_settings = VideoEncoder::GetDefaultH264Settings();
      h264_settings.frameDroppingOn = true;
      h264_settings.keyFrameInterval =
          config.encoder.key_frame_interval.value_or(0);
      return new rtc::RefCountedObject<
          VideoEncoderConfig::H264EncoderSpecificSettings>(h264_settings);
    }
    case Codec::kVideoCodecVP8: {
      VideoCodecVP8 vp8_settings = VideoEncoder::GetDefaultVp8Settings();
      vp8_settings.frameDroppingOn = true;
      vp8_settings.keyFrameInterval =
          config.encoder.key_frame_interval.value_or(0);
      vp8_settings.automaticResizeOn = true;
      vp8_settings.denoisingOn = config.encoder.denoising;
      return new rtc::RefCountedObject<
          VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8_settings);
    }
    case Codec::kVideoCodecVP9: {
      VideoCodecVP9 vp9_settings = VideoEncoder::GetDefaultVp9Settings();
      vp9_settings.frameDroppingOn = true;
      vp9_settings.keyFrameInterval =
          config.encoder.key_frame_interval.value_or(0);
      vp9_settings.automaticResizeOn = true;
      vp9_settings.denoisingOn = config.encoder.denoising;
      vp9_settings.interLayerPred = InterLayerPredMode::kOnKeyPic;
      return new rtc::RefCountedObject<
          VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
    }
    default:
      return nullptr;
  }
}

VideoEncoderConfig CreateVideoEncoderConfig(VideoStreamConfig config) {
  size_t num_streams = config.encoder.num_simulcast_streams;
  VideoEncoderConfig encoder_config;
  encoder_config.codec_type = config.encoder.codec;
  encoder_config.content_type = VideoEncoderConfig::ContentType::kRealtimeVideo;
  encoder_config.video_format =
      SdpVideoFormat(CodecTypeToPayloadString(config.encoder.codec), {});
  encoder_config.number_of_streams = num_streams;
  encoder_config.simulcast_layers = std::vector<VideoStream>(num_streams);

  std::string cricket_codec = CodecTypeToCodecName(config.encoder.codec);
  if (!cricket_codec.empty()) {
    encoder_config.video_stream_factory =
        new rtc::RefCountedObject<cricket::EncoderStreamFactory>(
            cricket_codec, kDefaultMaxQp, false, false);
  } else {
    encoder_config.video_stream_factory =
        new rtc::RefCountedObject<DefaultVideoStreamFactory>();
  }
  if (config.encoder.max_data_rate) {
    encoder_config.max_bitrate_bps = config.encoder.max_data_rate->bps();
  } else {
    encoder_config.max_bitrate_bps = 10000000;  // 10 mbit
  }
  encoder_config.encoder_specific_settings =
      CreateEncoderSpecificSettings(config);
  return encoder_config;
}
}  // namespace

SendVideoStream::SendVideoStream(CallClient* sender,
                                 VideoStreamConfig config,
                                 Transport* send_transport)
    : sender_(sender), config_(config) {
  for (size_t i = 0; i < config.encoder.num_simulcast_streams; ++i) {
    ssrcs_.push_back(sender->GetNextVideoSsrc());
    rtx_ssrcs_.push_back(sender->GetNextRtxSsrc());
  }

  using Capture = VideoStreamConfig::Source::Capture;
  switch (config.source.capture) {
    case Capture::kGenerator:
      frame_generator_ = test::FrameGeneratorCapturer::Create(
          config.source.width, config.source.height,
          config.source.generator.pixel_format, absl::nullopt,
          config.source.framerate, sender_->clock_);
      video_capturer_.reset(frame_generator_);
      break;
    case Capture::kVideoFile:
      frame_generator_ = test::FrameGeneratorCapturer::CreateFromYuvFile(
          test::ResourcePath(config.source.video_file.name, "yuv"),
          config.source.width, config.source.height, config.source.framerate,
          sender_->clock_);
      RTC_CHECK(frame_generator_)
          << "Could not create capturer for " << config.source.video_file.name
          << ".yuv. Is this resource file present?";
      video_capturer_.reset(frame_generator_);
      break;
  }

  using Encoder = VideoStreamConfig::Encoder;
  using Codec = VideoStreamConfig::Encoder::Codec;
  switch (config.encoder.implementation) {
    case Encoder::Implementation::kFake:
      if (config.encoder.codec == Codec::kVideoCodecGeneric) {
        encoder_factory_ =
            absl::make_unique<FunctionVideoEncoderFactory>([this, config]() {
              auto encoder =
                  absl::make_unique<test::FakeEncoder>(sender_->clock_);
              if (config.encoder.fake.max_rate.IsFinite())
                encoder->SetMaxBitrate(config.encoder.fake.max_rate.kbps());
              return encoder;
            });
      } else {
        RTC_NOTREACHED();
      }
      break;
    case VideoStreamConfig::Encoder::Implementation::kSoftware:
      encoder_factory_.reset(new InternalEncoderFactory());
      break;
    case VideoStreamConfig::Encoder::Implementation::kHardware:
      encoder_factory_ = CreateHardwareEncoderFactory();
      break;
  }
  RTC_CHECK(encoder_factory_);

  VideoSendStream::Config send_config =
      CreateVideoSendStreamConfig(config, ssrcs_, send_transport);
  send_config.encoder_settings.encoder_factory = encoder_factory_.get();
  VideoEncoderConfig encoder_config = CreateVideoEncoderConfig(config);

  send_stream_ = sender_->call_->CreateVideoSendStream(
      std::move(send_config), std::move(encoder_config));

  send_stream_->SetSource(video_capturer_.get(),
                          config.encoder.degradation_preference);
}

SendVideoStream::~SendVideoStream() {
  sender_->call_->DestroyVideoSendStream(send_stream_);
}

void SendVideoStream::Start() {
  send_stream_->Start();
  video_capturer_->Start();
}

bool SendVideoStream::TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                                       uint64_t receiver,
                                       Timestamp at_time) {
  // Removes added overhead before delivering RTCP packet to sender.
  RTC_DCHECK_GE(packet.size(), config_.stream.packet_overhead.bytes());
  packet.SetSize(packet.size() - config_.stream.packet_overhead.bytes());
  sender_->DeliverPacket(MediaType::VIDEO, packet, at_time);
  return true;
}

void SendVideoStream::SetCaptureFramerate(int framerate) {
  RTC_CHECK(frame_generator_)
      << "Framerate change only implemented for generators";
  frame_generator_->ChangeFramerate(framerate);
}

void SendVideoStream::SetMaxFramerate(absl::optional<int> max_framerate) {
  VideoEncoderConfig encoder_config = CreateVideoEncoderConfig(config_);
  RTC_DCHECK_EQ(encoder_config.simulcast_layers.size(), 1);
  encoder_config.simulcast_layers[0].max_framerate = max_framerate.value_or(-1);
  send_stream_->ReconfigureVideoEncoder(std::move(encoder_config));
}

VideoSendStream::Stats SendVideoStream::GetStats() const {
  return send_stream_->GetStats();
}

ColumnPrinter SendVideoStream::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "video_target_rate video_sent_rate width height",
      [this](rtc::SimpleStringBuilder& sb) {
        VideoSendStream::Stats video_stats = send_stream_->GetStats();
        int width = 0;
        int height = 0;
        for (auto stream_stat : video_stats.substreams) {
          width = std::max(width, stream_stat.second.width);
          height = std::max(height, stream_stat.second.height);
        }
        sb.AppendFormat("%.0lf %.0lf %i %i",
                        video_stats.target_media_bitrate_bps / 8.0,
                        video_stats.media_bitrate_bps / 8.0, width, height);
      },
      64);
}

ReceiveVideoStream::ReceiveVideoStream(CallClient* receiver,
                                       VideoStreamConfig config,
                                       SendVideoStream* send_stream,
                                       size_t chosen_stream,
                                       Transport* feedback_transport)
    : receiver_(receiver),
      config_(config),
      decoder_factory_(absl::make_unique<InternalDecoderFactory>()) {
  renderer_ = absl::make_unique<FakeVideoRenderer>();
  VideoReceiveStream::Config recv_config(feedback_transport);
  recv_config.rtp.remb = !config.stream.packet_feedback;
  recv_config.rtp.transport_cc = config.stream.packet_feedback;
  recv_config.rtp.local_ssrc = CallTest::kReceiverLocalVideoSsrc;
  recv_config.rtp.extensions = GetVideoRtpExtensions(config);
  RTC_DCHECK(!config.stream.use_rtx ||
             config.stream.nack_history_time > TimeDelta::Zero());
  recv_config.rtp.nack.rtp_history_ms = config.stream.nack_history_time.ms();
  recv_config.rtp.protected_by_flexfec = config.stream.use_flexfec;
  recv_config.renderer = renderer_.get();
  if (config.stream.use_rtx) {
    recv_config.rtp.rtx_ssrc = send_stream->rtx_ssrcs_[chosen_stream];
    recv_config.rtp
        .rtx_associated_payload_types[CallTest::kSendRtxPayloadType] =
        CodecTypeToPayloadType(config.encoder.codec);
  }
  recv_config.rtp.remote_ssrc = send_stream->ssrcs_[chosen_stream];
  VideoReceiveStream::Decoder decoder =
      CreateMatchingDecoder(CodecTypeToPayloadType(config.encoder.codec),
                            CodecTypeToPayloadString(config.encoder.codec));
  decoder.decoder_factory = decoder_factory_.get();
  recv_config.decoders.push_back(decoder);

  if (config.stream.use_flexfec) {
    RTC_CHECK_EQ(config.encoder.num_simulcast_streams, 1);
    FlexfecReceiveStream::Config flexfec_config(feedback_transport);
    flexfec_config.payload_type = CallTest::kFlexfecPayloadType;
    flexfec_config.remote_ssrc = CallTest::kFlexfecSendSsrc;
    flexfec_config.protected_media_ssrcs = send_stream->rtx_ssrcs_;
    flexfec_config.local_ssrc = recv_config.rtp.local_ssrc;
    flecfec_stream_ =
        receiver_->call_->CreateFlexfecReceiveStream(flexfec_config);
  }
  if (config.stream.use_ulpfec) {
    recv_config.rtp.red_payload_type = CallTest::kRedPayloadType;
    recv_config.rtp.ulpfec_payload_type = CallTest::kUlpfecPayloadType;
    recv_config.rtp.rtx_associated_payload_types[CallTest::kRtxRedPayloadType] =
        CallTest::kRedPayloadType;
  }
  receive_stream_ =
      receiver_->call_->CreateVideoReceiveStream(std::move(recv_config));
}

ReceiveVideoStream::~ReceiveVideoStream() {
  receiver_->call_->DestroyVideoReceiveStream(receive_stream_);
  if (flecfec_stream_)
    receiver_->call_->DestroyFlexfecReceiveStream(flecfec_stream_);
}

bool ReceiveVideoStream::TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                                          uint64_t receiver,
                                          Timestamp at_time) {
  RTC_DCHECK_GE(packet.size(), config_.stream.packet_overhead.bytes());
  packet.SetSize(packet.size() - config_.stream.packet_overhead.bytes());
  receiver_->DeliverPacket(MediaType::VIDEO, packet, at_time);
  return true;
}

VideoStreamPair::~VideoStreamPair() = default;

VideoStreamPair::VideoStreamPair(CallClient* sender,
                                 std::vector<NetworkNode*> send_link,
                                 uint64_t send_receiver_id,
                                 CallClient* receiver,
                                 std::vector<NetworkNode*> return_link,
                                 uint64_t return_receiver_id,
                                 VideoStreamConfig config)
    : config_(config),
      send_link_(send_link),
      return_link_(return_link),
      send_transport_(sender,
                      send_link.front(),
                      send_receiver_id,
                      config.stream.packet_overhead),
      return_transport_(receiver,
                        return_link.front(),
                        return_receiver_id,
                        config.stream.packet_overhead),
      send_stream_(sender, config, &send_transport_),
      receive_stream_(receiver,
                      config,
                      &send_stream_,
                      /*chosen_stream=*/0,
                      &return_transport_) {
  NetworkNode::Route(send_transport_.ReceiverId(), send_link_,
                     &receive_stream_);
  NetworkNode::Route(return_transport_.ReceiverId(), return_link_,
                     &send_stream_);
}

}  // namespace test
}  // namespace webrtc
