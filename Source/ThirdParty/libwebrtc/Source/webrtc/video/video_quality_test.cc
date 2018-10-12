/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "video/video_quality_test.h"

#include <stdio.h>
#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "media/engine/adm_helpers.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/vp8_encoder_simulcast_proxy.h"
#include "media/engine/webrtcvideoengine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/multiplex/include/multiplex_decoder_adapter.h"
#include "modules/video_coding/codecs/multiplex/include/multiplex_encoder_adapter.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/strings/string_builder.h"
#include "test/run_loop.h"
#include "test/testsupport/fileutils.h"
#include "test/video_renderer.h"
#include "video/video_analyzer.h"
#ifdef WEBRTC_WIN
#include "modules/audio_device/include/audio_device_factory.h"
#endif

namespace webrtc {

namespace {
constexpr char kSyncGroup[] = "av_sync";
constexpr int kOpusMinBitrateBps = 6000;
constexpr int kOpusBitrateFbBps = 32000;
constexpr int kFramesSentInQuickTest = 1;
constexpr uint32_t kThumbnailSendSsrcStart = 0xE0000;
constexpr uint32_t kThumbnailRtxSsrcStart = 0xF0000;

constexpr int kDefaultMaxQp = cricket::WebRtcVideoChannel::kDefaultQpMax;

class VideoStreamFactory
    : public VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  explicit VideoStreamFactory(const std::vector<VideoStream>& streams)
      : streams_(streams) {}

 private:
  std::vector<VideoStream> CreateEncoderStreams(
      int width,
      int height,
      const VideoEncoderConfig& encoder_config) override {
    // The highest layer must match the incoming resolution.
    std::vector<VideoStream> streams = streams_;
    streams[streams_.size() - 1].height = height;
    streams[streams_.size() - 1].width = width;

    streams[0].bitrate_priority = encoder_config.bitrate_priority;
    return streams;
  }

  std::vector<VideoStream> streams_;
};

// A decoder wrapper that writes the encoded frames to a file.
class FrameDumpingDecoder : public VideoDecoder {
 public:
  FrameDumpingDecoder(std::unique_ptr<VideoDecoder> decoder,
                      rtc::PlatformFile file)
      : decoder_(std::move(decoder)),
        writer_(IvfFileWriter::Wrap(rtc::File(file),
                                    /* byte_limit= */ 100000000)) {}

  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override {
    return decoder_->InitDecode(codec_settings, number_of_cores);
  }

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override {
    int32_t ret = decoder_->Decode(input_image, missing_frames,
                                   codec_specific_info, render_time_ms);
    writer_->WriteFrame(input_image, codec_specific_info->codecType);

    return ret;
  }

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override {
    return decoder_->RegisterDecodeCompleteCallback(callback);
  }

  int32_t Release() override { return decoder_->Release(); }

  // Returns true if the decoder prefer to decode frames late.
  // That is, it can not decode infinite number of frames before the decoded
  // frame is consumed.
  bool PrefersLateDecoding() const override {
    return decoder_->PrefersLateDecoding();
  }

  const char* ImplementationName() const override {
    return decoder_->ImplementationName();
  }

 private:
  std::unique_ptr<VideoDecoder> decoder_;
  std::unique_ptr<IvfFileWriter> writer_;
};

// An encoder wrapper that writes the encoded frames to file, one per simulcast
// layer.
class FrameDumpingEncoder : public VideoEncoder, private EncodedImageCallback {
 public:
  FrameDumpingEncoder(std::unique_ptr<VideoEncoder> encoder,
                      std::vector<rtc::PlatformFile> files)
      : encoder_(std::move(encoder)) {
    for (rtc::PlatformFile file : files) {
      writers_.push_back(
          IvfFileWriter::Wrap(rtc::File(file), /* byte_limit= */ 100000000));
    }
  }
  // Implement VideoEncoder
  int32_t InitEncode(const VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) override {
    return encoder_->InitEncode(codec_settings, number_of_cores,
                                max_payload_size);
  }
  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override {
    callback_ = callback;
    return encoder_->RegisterEncodeCompleteCallback(this);
  }
  int32_t Release() override { return encoder_->Release(); }
  int32_t Encode(const VideoFrame& frame,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) {
    return encoder_->Encode(frame, codec_specific_info, frame_types);
  }
  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override {
    return encoder_->SetChannelParameters(packet_loss, rtt);
  }
  int32_t SetRates(uint32_t bitrate, uint32_t framerate) override {
    return encoder_->SetRates(bitrate, framerate);
  }
  int32_t SetRateAllocation(const VideoBitrateAllocation& allocation,
                            uint32_t framerate) override {
    return encoder_->SetRateAllocation(allocation, framerate);
  }
  ScalingSettings GetScalingSettings() const override {
    return encoder_->GetScalingSettings();
  }
  bool SupportsNativeHandle() const override {
    return encoder_->SupportsNativeHandle();
  }
  const char* ImplementationName() const override {
    return encoder_->ImplementationName();
  }

 private:
  // Implement EncodedImageCallback
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info,
                        const RTPFragmentationHeader* fragmentation) override {
    if (codec_specific_info) {
      int simulcast_index;
      if (codec_specific_info->codecType == kVideoCodecVP9) {
        simulcast_index = 0;
      } else {
        simulcast_index = encoded_image.SpatialIndex().value_or(0);
      }
      RTC_DCHECK_GE(simulcast_index, 0);
      if (static_cast<size_t>(simulcast_index) < writers_.size()) {
        writers_[simulcast_index]->WriteFrame(encoded_image,
                                              codec_specific_info->codecType);
      }
    }

    return callback_->OnEncodedImage(encoded_image, codec_specific_info,
                                     fragmentation);
  }

  void OnDroppedFrame(DropReason reason) override {
    callback_->OnDroppedFrame(reason);
  }

  std::unique_ptr<VideoEncoder> encoder_;
  EncodedImageCallback* callback_ = nullptr;
  std::vector<std::unique_ptr<IvfFileWriter>> writers_;
};

}  // namespace

std::unique_ptr<VideoDecoder> VideoQualityTest::CreateVideoDecoder(
    const SdpVideoFormat& format) {
  std::unique_ptr<VideoDecoder> decoder;
  if (format.name == "multiplex") {
    decoder = absl::make_unique<MultiplexDecoderAdapter>(
        &internal_decoder_factory_, SdpVideoFormat(cricket::kVp9CodecName));
  } else {
    decoder = internal_decoder_factory_.CreateVideoDecoder(format);
  }
  if (!params_.logging.encoded_frame_base_path.empty()) {
    rtc::StringBuilder str;
    str << receive_logs_++;
    std::string path =
        params_.logging.encoded_frame_base_path + "." + str.str() + ".recv.ivf";
    decoder = absl::make_unique<FrameDumpingDecoder>(
        std::move(decoder), rtc::CreatePlatformFile(path));
  }
  return decoder;
}

std::unique_ptr<VideoEncoder> VideoQualityTest::CreateVideoEncoder(
    const SdpVideoFormat& format) {
  std::unique_ptr<VideoEncoder> encoder;
  if (format.name == "VP8") {
    encoder = absl::make_unique<VP8EncoderSimulcastProxy>(
        &internal_encoder_factory_, format);
  } else if (format.name == "multiplex") {
    encoder = absl::make_unique<MultiplexEncoderAdapter>(
        &internal_encoder_factory_, SdpVideoFormat(cricket::kVp9CodecName));
  } else {
    encoder = internal_encoder_factory_.CreateVideoEncoder(format);
  }
  if (!params_.logging.encoded_frame_base_path.empty()) {
    char ss_buf[100];
    rtc::SimpleStringBuilder sb(ss_buf);
    sb << send_logs_++;
    std::string prefix =
        params_.logging.encoded_frame_base_path + "." + sb.str() + ".send.";
    encoder = absl::make_unique<FrameDumpingEncoder>(
        std::move(encoder), std::vector<rtc::PlatformFile>(
                                {rtc::CreatePlatformFile(prefix + "1.ivf"),
                                 rtc::CreatePlatformFile(prefix + "2.ivf"),
                                 rtc::CreatePlatformFile(prefix + "3.ivf")}));
  }
  return encoder;
}

VideoQualityTest::VideoQualityTest(
    std::unique_ptr<InjectionComponents> injection_components)
    : clock_(Clock::GetRealTimeClock()),
      video_decoder_factory_([this](const SdpVideoFormat& format) {
        return this->CreateVideoDecoder(format);
      }),
      video_encoder_factory_([this](const SdpVideoFormat& format) {
        return this->CreateVideoEncoder(format);
      }),
      receive_logs_(0),
      send_logs_(0),
      injection_components_(std::move(injection_components)) {
  if (injection_components_ == nullptr) {
    injection_components_ = absl::make_unique<InjectionComponents>();
  }

  payload_type_map_ = test::CallTest::payload_type_map_;
  RTC_DCHECK(payload_type_map_.find(kPayloadTypeH264) ==
             payload_type_map_.end());
  RTC_DCHECK(payload_type_map_.find(kPayloadTypeVP8) ==
             payload_type_map_.end());
  RTC_DCHECK(payload_type_map_.find(kPayloadTypeVP9) ==
             payload_type_map_.end());
  payload_type_map_[kPayloadTypeH264] = webrtc::MediaType::VIDEO;
  payload_type_map_[kPayloadTypeVP8] = webrtc::MediaType::VIDEO;
  payload_type_map_[kPayloadTypeVP9] = webrtc::MediaType::VIDEO;

  fec_controller_factory_ =
      std::move(injection_components_->fec_controller_factory);
}

VideoQualityTest::Params::Params()
    : call({false, false, BitrateConstraints(), 0}),
      video{{false, 640, 480, 30, 50, 800, 800, false, "VP8", 1, -1, 0, false,
             false, false, ""},
            {false, 640, 480, 30, 50, 800, 800, false, "VP8", 1, -1, 0, false,
             false, false, ""}},
      audio({false, false, false, false}),
      screenshare{{false, false, 10, 0}, {false, false, 10, 0}},
      analyzer({"", 0.0, 0.0, 0, "", ""}),
      pipe(),
      config(absl::nullopt),
      ss{{std::vector<VideoStream>(), 0, 0, -1, InterLayerPredMode::kOn,
          std::vector<SpatialLayer>()},
         {std::vector<VideoStream>(), 0, 0, -1, InterLayerPredMode::kOn,
          std::vector<SpatialLayer>()}},
      logging({"", "", ""}) {}

VideoQualityTest::Params::~Params() = default;

VideoQualityTest::InjectionComponents::InjectionComponents() = default;

VideoQualityTest::InjectionComponents::~InjectionComponents() = default;

void VideoQualityTest::TestBody() {}

std::string VideoQualityTest::GenerateGraphTitle() const {
  rtc::StringBuilder ss;
  ss << params_.video[0].codec;
  ss << " (" << params_.video[0].target_bitrate_bps / 1000 << "kbps";
  ss << ", " << params_.video[0].fps << " FPS";
  if (params_.screenshare[0].scroll_duration)
    ss << ", " << params_.screenshare[0].scroll_duration << "s scroll";
  if (params_.ss[0].streams.size() > 1)
    ss << ", Stream #" << params_.ss[0].selected_stream;
  if (params_.ss[0].num_spatial_layers > 1)
    ss << ", Layer #" << params_.ss[0].selected_sl;
  ss << ")";
  return ss.Release();
}

void VideoQualityTest::CheckParamsAndInjectionComponents() {
  if (injection_components_ == nullptr) {
    injection_components_ = absl::make_unique<InjectionComponents>();
  }
  if (!params_.config && injection_components_->sender_network == nullptr &&
      injection_components_->receiver_network == nullptr) {
    // TODO(titovartem) replace with default config creation when removing
    // pipe.
    params_.config = params_.pipe;
  }
  RTC_CHECK(
      (params_.config && injection_components_->sender_network == nullptr &&
       injection_components_->receiver_network == nullptr) ||
      (!params_.config && injection_components_->sender_network != nullptr &&
       injection_components_->receiver_network != nullptr));
  for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
    // Iterate over primary and secondary video streams.
    if (!params_.video[video_idx].enabled)
      return;
    // Add a default stream in none specified.
    if (params_.ss[video_idx].streams.empty())
      params_.ss[video_idx].streams.push_back(
          VideoQualityTest::DefaultVideoStream(params_, video_idx));
    if (params_.ss[video_idx].num_spatial_layers == 0)
      params_.ss[video_idx].num_spatial_layers = 1;

    if (params_.config) {
      if (params_.config->loss_percent != 0 ||
          params_.config->queue_length_packets != 0) {
        // Since LayerFilteringTransport changes the sequence numbers, we can't
        // use that feature with pack loss, since the NACK request would end up
        // retransmitting the wrong packets.
        RTC_CHECK(params_.ss[video_idx].selected_sl == -1 ||
                  params_.ss[video_idx].selected_sl ==
                      params_.ss[video_idx].num_spatial_layers - 1);
        RTC_CHECK(params_.video[video_idx].selected_tl == -1 ||
                  params_.video[video_idx].selected_tl ==
                      params_.video[video_idx].num_temporal_layers - 1);
      }
    }

    // TODO(ivica): Should max_bitrate_bps == -1 represent inf max bitrate, as
    // it does in some parts of the code?
    RTC_CHECK_GE(params_.video[video_idx].max_bitrate_bps,
                 params_.video[video_idx].target_bitrate_bps);
    RTC_CHECK_GE(params_.video[video_idx].target_bitrate_bps,
                 params_.video[video_idx].min_bitrate_bps);
    RTC_CHECK_LT(params_.video[video_idx].selected_tl,
                 params_.video[video_idx].num_temporal_layers);
    RTC_CHECK_LE(params_.ss[video_idx].selected_stream,
                 params_.ss[video_idx].streams.size());
    for (const VideoStream& stream : params_.ss[video_idx].streams) {
      RTC_CHECK_GE(stream.min_bitrate_bps, 0);
      RTC_CHECK_GE(stream.target_bitrate_bps, stream.min_bitrate_bps);
      RTC_CHECK_GE(stream.max_bitrate_bps, stream.target_bitrate_bps);
    }
    // TODO(ivica): Should we check if the sum of all streams/layers is equal to
    // the total bitrate? We anyway have to update them in the case bitrate
    // estimator changes the total bitrates.
    RTC_CHECK_GE(params_.ss[video_idx].num_spatial_layers, 1);
    RTC_CHECK_LE(params_.ss[video_idx].selected_sl,
                 params_.ss[video_idx].num_spatial_layers);
    RTC_CHECK(
        params_.ss[video_idx].spatial_layers.empty() ||
        params_.ss[video_idx].spatial_layers.size() ==
            static_cast<size_t>(params_.ss[video_idx].num_spatial_layers));
    if (params_.video[video_idx].codec == "VP8") {
      RTC_CHECK_EQ(params_.ss[video_idx].num_spatial_layers, 1);
    } else if (params_.video[video_idx].codec == "VP9") {
      RTC_CHECK_EQ(params_.ss[video_idx].streams.size(), 1);
    }
    RTC_CHECK_GE(params_.call.num_thumbnails, 0);
    if (params_.call.num_thumbnails > 0) {
      RTC_CHECK_EQ(params_.ss[video_idx].num_spatial_layers, 1);
      RTC_CHECK_EQ(params_.ss[video_idx].streams.size(), 3);
      RTC_CHECK_EQ(params_.video[video_idx].num_temporal_layers, 3);
      RTC_CHECK_EQ(params_.video[video_idx].codec, "VP8");
    }
    // Dual streams with FEC not supported in tests yet.
    RTC_CHECK(!params_.video[video_idx].flexfec || num_video_streams_ == 1);
    RTC_CHECK(!params_.video[video_idx].ulpfec || num_video_streams_ == 1);
  }
}

// Static.
std::vector<int> VideoQualityTest::ParseCSV(const std::string& str) {
  // Parse comma separated nonnegative integers, where some elements may be
  // empty. The empty values are replaced with -1.
  // E.g. "10,-20,,30,40" --> {10, 20, -1, 30,40}
  // E.g. ",,10,,20," --> {-1, -1, 10, -1, 20, -1}
  std::vector<int> result;
  if (str.empty())
    return result;

  const char* p = str.c_str();
  int value = -1;
  int pos;
  while (*p) {
    if (*p == ',') {
      result.push_back(value);
      value = -1;
      ++p;
      continue;
    }
    RTC_CHECK_EQ(sscanf(p, "%d%n", &value, &pos), 1)
        << "Unexpected non-number value.";
    p += pos;
  }
  result.push_back(value);
  return result;
}

// Static.
VideoStream VideoQualityTest::DefaultVideoStream(const Params& params,
                                                 size_t video_idx) {
  VideoStream stream;
  stream.width = params.video[video_idx].width;
  stream.height = params.video[video_idx].height;
  stream.max_framerate = params.video[video_idx].fps;
  stream.min_bitrate_bps = params.video[video_idx].min_bitrate_bps;
  stream.target_bitrate_bps = params.video[video_idx].target_bitrate_bps;
  stream.max_bitrate_bps = params.video[video_idx].max_bitrate_bps;
  stream.max_qp = kDefaultMaxQp;
  stream.num_temporal_layers = params.video[video_idx].num_temporal_layers;
  stream.active = true;
  return stream;
}

// Static.
VideoStream VideoQualityTest::DefaultThumbnailStream() {
  VideoStream stream;
  stream.width = 320;
  stream.height = 180;
  stream.max_framerate = 7;
  stream.min_bitrate_bps = 7500;
  stream.target_bitrate_bps = 37500;
  stream.max_bitrate_bps = 50000;
  stream.max_qp = kDefaultMaxQp;
  return stream;
}

// Static.
void VideoQualityTest::FillScalabilitySettings(
    Params* params,
    size_t video_idx,
    const std::vector<std::string>& stream_descriptors,
    int num_streams,
    size_t selected_stream,
    int num_spatial_layers,
    int selected_sl,
    InterLayerPredMode inter_layer_pred,
    const std::vector<std::string>& sl_descriptors) {
  if (params->ss[video_idx].streams.empty() &&
      params->ss[video_idx].infer_streams) {
    webrtc::VideoEncoderConfig encoder_config;
    encoder_config.codec_type =
        PayloadStringToCodecType(params->video[video_idx].codec);
    encoder_config.content_type =
        params->screenshare[video_idx].enabled
            ? webrtc::VideoEncoderConfig::ContentType::kScreen
            : webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo;
    encoder_config.max_bitrate_bps = params->video[video_idx].max_bitrate_bps;
    encoder_config.min_transmit_bitrate_bps =
        params->video[video_idx].min_transmit_bps;
    encoder_config.number_of_streams = num_streams;
    encoder_config.spatial_layers = params->ss[video_idx].spatial_layers;
    encoder_config.simulcast_layers = std::vector<VideoStream>(num_streams);
    encoder_config.video_stream_factory =
        new rtc::RefCountedObject<cricket::EncoderStreamFactory>(
            params->video[video_idx].codec, kDefaultMaxQp,
            params->screenshare[video_idx].enabled, true);
    params->ss[video_idx].streams =
        encoder_config.video_stream_factory->CreateEncoderStreams(
            static_cast<int>(params->video[video_idx].width),
            static_cast<int>(params->video[video_idx].height), encoder_config);
  } else {
    // Read VideoStream and SpatialLayer elements from a list of comma separated
    // lists. To use a default value for an element, use -1 or leave empty.
    // Validity checks performed in CheckParamsAndInjectionComponents.
    RTC_CHECK(params->ss[video_idx].streams.empty());
    for (auto descriptor : stream_descriptors) {
      if (descriptor.empty())
        continue;
      VideoStream stream =
          VideoQualityTest::DefaultVideoStream(*params, video_idx);
      std::vector<int> v = VideoQualityTest::ParseCSV(descriptor);
      if (v[0] != -1)
        stream.width = static_cast<size_t>(v[0]);
      if (v[1] != -1)
        stream.height = static_cast<size_t>(v[1]);
      if (v[2] != -1)
        stream.max_framerate = v[2];
      if (v[3] != -1)
        stream.min_bitrate_bps = v[3];
      if (v[4] != -1)
        stream.target_bitrate_bps = v[4];
      if (v[5] != -1)
        stream.max_bitrate_bps = v[5];
      if (v.size() > 6 && v[6] != -1)
        stream.max_qp = v[6];
      if (v.size() > 7 && v[7] != -1) {
        stream.num_temporal_layers = v[7];
      } else {
        // Automatic TL thresholds for more than two layers not supported.
        RTC_CHECK_LE(params->video[video_idx].num_temporal_layers, 2);
      }
      params->ss[video_idx].streams.push_back(stream);
    }
  }

  params->ss[video_idx].num_spatial_layers = std::max(1, num_spatial_layers);
  params->ss[video_idx].selected_stream = selected_stream;

  params->ss[video_idx].selected_sl = selected_sl;
  params->ss[video_idx].inter_layer_pred = inter_layer_pred;
  RTC_CHECK(params->ss[video_idx].spatial_layers.empty());
  for (auto descriptor : sl_descriptors) {
    if (descriptor.empty())
      continue;
    std::vector<int> v = VideoQualityTest::ParseCSV(descriptor);
    RTC_CHECK_EQ(v.size(), 7);

    SpatialLayer layer = {0};
    layer.width = v[0];
    layer.height = v[1];
    layer.numberOfTemporalLayers = v[2];
    layer.maxBitrate = v[3];
    layer.minBitrate = v[4];
    layer.targetBitrate = v[5];
    layer.qpMax = v[6];
    layer.active = true;

    params->ss[video_idx].spatial_layers.push_back(layer);
  }
}

void VideoQualityTest::SetupVideo(Transport* send_transport,
                                  Transport* recv_transport) {
  size_t total_streams_used = 0;
  video_receive_configs_.clear();
  video_send_configs_.clear();
  video_encoder_configs_.clear();
  bool decode_all_receive_streams = true;
  size_t num_video_substreams = params_.ss[0].streams.size();
  RTC_CHECK(num_video_streams_ > 0);
  video_encoder_configs_.resize(num_video_streams_);
  for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
    video_send_configs_.push_back(VideoSendStream::Config(send_transport));
    video_encoder_configs_.push_back(VideoEncoderConfig());
    num_video_substreams = params_.ss[video_idx].streams.size();
    RTC_CHECK_GT(num_video_substreams, 0);
    for (size_t i = 0; i < num_video_substreams; ++i)
      video_send_configs_[video_idx].rtp.ssrcs.push_back(
          kVideoSendSsrcs[total_streams_used + i]);

    int payload_type;
    if (params_.video[video_idx].codec == "H264") {
      payload_type = kPayloadTypeH264;
    } else if (params_.video[video_idx].codec == "VP8") {
      payload_type = kPayloadTypeVP8;
    } else if (params_.video[video_idx].codec == "VP9") {
      payload_type = kPayloadTypeVP9;
    } else if (params_.video[video_idx].codec == "multiplex") {
      payload_type = kPayloadTypeVP9;
    } else {
      RTC_NOTREACHED() << "Codec not supported!";
      return;
    }
    video_send_configs_[video_idx].encoder_settings.encoder_factory =
        &video_encoder_factory_;

    video_send_configs_[video_idx].rtp.payload_name =
        params_.video[video_idx].codec;
    video_send_configs_[video_idx].rtp.payload_type = payload_type;
    video_send_configs_[video_idx].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    video_send_configs_[video_idx].rtp.rtx.payload_type = kSendRtxPayloadType;
    for (size_t i = 0; i < num_video_substreams; ++i) {
      video_send_configs_[video_idx].rtp.rtx.ssrcs.push_back(
          kSendRtxSsrcs[i + total_streams_used]);
    }
    video_send_configs_[video_idx].rtp.extensions.clear();
    if (params_.call.send_side_bwe) {
      video_send_configs_[video_idx].rtp.extensions.emplace_back(
          RtpExtension::kTransportSequenceNumberUri,
          test::kTransportSequenceNumberExtensionId);
    } else {
      video_send_configs_[video_idx].rtp.extensions.emplace_back(
          RtpExtension::kAbsSendTimeUri, test::kAbsSendTimeExtensionId);
    }

    if (params_.call.generic_descriptor) {
      // The generic descriptor is currently behind a field trial, so it needs
      // to be set for this flag to have any effect.
      // TODO(philipel): Remove this check when the experiment is removed.
      RTC_CHECK(field_trial::IsEnabled("WebRTC-GenericDescriptor"));

      video_send_configs_[video_idx].rtp.extensions.emplace_back(
          RtpExtension::kGenericFrameDescriptorUri,
          test::kGenericDescriptorExtensionId);
    }

    video_send_configs_[video_idx].rtp.extensions.emplace_back(
        RtpExtension::kVideoContentTypeUri, test::kVideoContentTypeExtensionId);
    video_send_configs_[video_idx].rtp.extensions.emplace_back(
        RtpExtension::kVideoTimingUri, test::kVideoTimingExtensionId);

    video_encoder_configs_[video_idx].video_format.name =
        params_.video[video_idx].codec;

    video_encoder_configs_[video_idx].video_format.parameters =
        params_.video[video_idx].sdp_params;

    video_encoder_configs_[video_idx].codec_type =
        PayloadStringToCodecType(params_.video[video_idx].codec);

    video_encoder_configs_[video_idx].min_transmit_bitrate_bps =
        params_.video[video_idx].min_transmit_bps;

    video_send_configs_[video_idx].suspend_below_min_bitrate =
        params_.video[video_idx].suspend_below_min_bitrate;

    video_encoder_configs_[video_idx].number_of_streams =
        params_.ss[video_idx].streams.size();
    video_encoder_configs_[video_idx].max_bitrate_bps = 0;
    for (size_t i = 0; i < params_.ss[video_idx].streams.size(); ++i) {
      video_encoder_configs_[video_idx].max_bitrate_bps +=
          params_.ss[video_idx].streams[i].max_bitrate_bps;
    }
    if (params_.ss[video_idx].infer_streams) {
      video_encoder_configs_[video_idx].simulcast_layers =
          std::vector<VideoStream>(params_.ss[video_idx].streams.size());
      video_encoder_configs_[video_idx].video_stream_factory =
          new rtc::RefCountedObject<cricket::EncoderStreamFactory>(
              params_.video[video_idx].codec,
              params_.ss[video_idx].streams[0].max_qp,
              params_.screenshare[video_idx].enabled, true);
    } else {
      video_encoder_configs_[video_idx].video_stream_factory =
          new rtc::RefCountedObject<VideoStreamFactory>(
              params_.ss[video_idx].streams);
    }

    video_encoder_configs_[video_idx].spatial_layers =
        params_.ss[video_idx].spatial_layers;
    decode_all_receive_streams = params_.ss[video_idx].selected_stream ==
                                 params_.ss[video_idx].streams.size();
    absl::optional<int> decode_sub_stream;
    if (!decode_all_receive_streams)
      decode_sub_stream = params_.ss[video_idx].selected_stream;
    CreateMatchingVideoReceiveConfigs(
        video_send_configs_[video_idx], recv_transport,
        params_.call.send_side_bwe, &video_decoder_factory_, decode_sub_stream,
        true, kNackRtpHistoryMs);

    if (params_.screenshare[video_idx].enabled) {
      // Fill out codec settings.
      video_encoder_configs_[video_idx].content_type =
          VideoEncoderConfig::ContentType::kScreen;
      degradation_preference_ = DegradationPreference::MAINTAIN_RESOLUTION;
      if (params_.video[video_idx].codec == "VP8") {
        VideoCodecVP8 vp8_settings = VideoEncoder::GetDefaultVp8Settings();
        vp8_settings.denoisingOn = false;
        vp8_settings.frameDroppingOn = false;
        vp8_settings.numberOfTemporalLayers = static_cast<unsigned char>(
            params_.video[video_idx].num_temporal_layers);
        video_encoder_configs_[video_idx].encoder_specific_settings =
            new rtc::RefCountedObject<
                VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8_settings);
      } else if (params_.video[video_idx].codec == "VP9") {
        VideoCodecVP9 vp9_settings = VideoEncoder::GetDefaultVp9Settings();
        vp9_settings.denoisingOn = false;
        vp9_settings.frameDroppingOn = false;
        vp9_settings.numberOfTemporalLayers = static_cast<unsigned char>(
            params_.video[video_idx].num_temporal_layers);
        vp9_settings.numberOfSpatialLayers = static_cast<unsigned char>(
            params_.ss[video_idx].num_spatial_layers);
        vp9_settings.interLayerPred = params_.ss[video_idx].inter_layer_pred;
        video_encoder_configs_[video_idx].encoder_specific_settings =
            new rtc::RefCountedObject<
                VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
      }
    } else if (params_.ss[video_idx].num_spatial_layers > 1) {
      // If SVC mode without screenshare, still need to set codec specifics.
      RTC_CHECK(params_.video[video_idx].codec == "VP9");
      VideoCodecVP9 vp9_settings = VideoEncoder::GetDefaultVp9Settings();
      vp9_settings.numberOfTemporalLayers = static_cast<unsigned char>(
          params_.video[video_idx].num_temporal_layers);
      vp9_settings.numberOfSpatialLayers =
          static_cast<unsigned char>(params_.ss[video_idx].num_spatial_layers);
      vp9_settings.interLayerPred = params_.ss[video_idx].inter_layer_pred;
      video_encoder_configs_[video_idx].encoder_specific_settings =
          new rtc::RefCountedObject<
              VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
    } else if (params_.video[video_idx].automatic_scaling) {
      if (params_.video[video_idx].codec == "VP8") {
        VideoCodecVP8 vp8_settings = VideoEncoder::GetDefaultVp8Settings();
        vp8_settings.automaticResizeOn = true;
        video_encoder_configs_[video_idx].encoder_specific_settings =
            new rtc::RefCountedObject<
                VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8_settings);
      } else if (params_.video[video_idx].codec == "VP9") {
        VideoCodecVP9 vp9_settings = VideoEncoder::GetDefaultVp9Settings();
        vp9_settings.automaticResizeOn = true;
        video_encoder_configs_[video_idx].encoder_specific_settings =
            new rtc::RefCountedObject<
                VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
      } else {
        RTC_NOTREACHED() << "Automatic scaling not supported for codec "
                         << params_.video[video_idx].codec << ", stream "
                         << video_idx;
      }
    }
    total_streams_used += num_video_substreams;
  }

  // FEC supported only for single video stream mode yet.
  if (params_.video[0].flexfec) {
    if (decode_all_receive_streams) {
      SetSendFecConfig(GetVideoSendConfig()->rtp.ssrcs);
    } else {
      SetSendFecConfig({kVideoSendSsrcs[params_.ss[0].selected_stream]});
    }

    CreateMatchingFecConfig(recv_transport, *GetVideoSendConfig());
    GetFlexFecConfig()->transport_cc = params_.call.send_side_bwe;
    if (params_.call.send_side_bwe) {
      GetFlexFecConfig()->rtp_header_extensions.push_back(
          RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       test::kTransportSequenceNumberExtensionId));
    } else {
      GetFlexFecConfig()->rtp_header_extensions.push_back(RtpExtension(
          RtpExtension::kAbsSendTimeUri, test::kAbsSendTimeExtensionId));
    }
  }

  if (params_.video[0].ulpfec) {
    SetSendUlpFecConfig(GetVideoSendConfig());
    if (decode_all_receive_streams) {
      for (auto& receive_config : video_receive_configs_) {
        SetReceiveUlpFecConfig(&receive_config);
      }
    } else {
      SetReceiveUlpFecConfig(
          &video_receive_configs_[params_.ss[0].selected_stream]);
    }
  }
}

void VideoQualityTest::SetupThumbnails(Transport* send_transport,
                                       Transport* recv_transport) {
  for (int i = 0; i < params_.call.num_thumbnails; ++i) {
    // Thumbnails will be send in the other way: from receiver_call to
    // sender_call.
    VideoSendStream::Config thumbnail_send_config(recv_transport);
    thumbnail_send_config.rtp.ssrcs.push_back(kThumbnailSendSsrcStart + i);
    // TODO(nisse): Could use a simpler VP8-only encoder factory.
    thumbnail_send_config.encoder_settings.encoder_factory =
        &video_encoder_factory_;
    thumbnail_send_config.rtp.payload_name = params_.video[0].codec;
    thumbnail_send_config.rtp.payload_type = kPayloadTypeVP8;
    thumbnail_send_config.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    thumbnail_send_config.rtp.rtx.payload_type = kSendRtxPayloadType;
    thumbnail_send_config.rtp.rtx.ssrcs.push_back(kThumbnailRtxSsrcStart + i);
    thumbnail_send_config.rtp.extensions.clear();
    if (params_.call.send_side_bwe) {
      thumbnail_send_config.rtp.extensions.push_back(
          RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       test::kTransportSequenceNumberExtensionId));
    } else {
      thumbnail_send_config.rtp.extensions.push_back(RtpExtension(
          RtpExtension::kAbsSendTimeUri, test::kAbsSendTimeExtensionId));
    }

    VideoEncoderConfig thumbnail_encoder_config;
    thumbnail_encoder_config.codec_type = kVideoCodecVP8;
    thumbnail_encoder_config.video_format.name = "VP8";
    thumbnail_encoder_config.min_transmit_bitrate_bps = 7500;
    thumbnail_send_config.suspend_below_min_bitrate =
        params_.video[0].suspend_below_min_bitrate;
    thumbnail_encoder_config.number_of_streams = 1;
    thumbnail_encoder_config.max_bitrate_bps = 50000;
    if (params_.ss[0].infer_streams) {
      thumbnail_encoder_config.video_stream_factory =
          new rtc::RefCountedObject<VideoStreamFactory>(params_.ss[0].streams);
    } else {
      thumbnail_encoder_config.simulcast_layers = std::vector<VideoStream>(1);
      thumbnail_encoder_config.video_stream_factory =
          new rtc::RefCountedObject<cricket::EncoderStreamFactory>(
              params_.video[0].codec, params_.ss[0].streams[0].max_qp,
              params_.screenshare[0].enabled, true);
    }
    thumbnail_encoder_config.spatial_layers = params_.ss[0].spatial_layers;

    thumbnail_encoder_configs_.push_back(thumbnail_encoder_config.Copy());
    thumbnail_send_configs_.push_back(thumbnail_send_config.Copy());

    AddMatchingVideoReceiveConfigs(
        &thumbnail_receive_configs_, thumbnail_send_config, send_transport,
        params_.call.send_side_bwe, &video_decoder_factory_, absl::nullopt,
        false, kNackRtpHistoryMs);
  }
  for (size_t i = 0; i < thumbnail_send_configs_.size(); ++i) {
    thumbnail_send_streams_.push_back(receiver_call_->CreateVideoSendStream(
        thumbnail_send_configs_[i].Copy(),
        thumbnail_encoder_configs_[i].Copy()));
  }
  for (size_t i = 0; i < thumbnail_receive_configs_.size(); ++i) {
    thumbnail_receive_streams_.push_back(sender_call_->CreateVideoReceiveStream(
        thumbnail_receive_configs_[i].Copy()));
  }
}

void VideoQualityTest::DestroyThumbnailStreams() {
  for (VideoSendStream* thumbnail_send_stream : thumbnail_send_streams_) {
    receiver_call_->DestroyVideoSendStream(thumbnail_send_stream);
  }
  thumbnail_send_streams_.clear();
  for (VideoReceiveStream* thumbnail_receive_stream :
       thumbnail_receive_streams_) {
    sender_call_->DestroyVideoReceiveStream(thumbnail_receive_stream);
  }
  thumbnail_send_streams_.clear();
  thumbnail_receive_streams_.clear();
  for (std::unique_ptr<test::TestVideoCapturer>& video_caputurer :
       thumbnail_capturers_) {
    video_caputurer.reset();
  }
}

void VideoQualityTest::SetupThumbnailCapturers(size_t num_thumbnail_streams) {
  VideoStream thumbnail = DefaultThumbnailStream();
  for (size_t i = 0; i < num_thumbnail_streams; ++i) {
    thumbnail_capturers_.emplace_back(test::FrameGeneratorCapturer::Create(
        static_cast<int>(thumbnail.width), static_cast<int>(thumbnail.height),
        absl::nullopt, absl::nullopt, thumbnail.max_framerate, clock_));
    RTC_DCHECK(thumbnail_capturers_.back());
  }
}

std::unique_ptr<test::FrameGenerator> VideoQualityTest::CreateFrameGenerator(
    size_t video_idx) {
  // Setup frame generator.
  const size_t kWidth = 1850;
  const size_t kHeight = 1110;
  std::unique_ptr<test::FrameGenerator> frame_generator;
  if (params_.screenshare[video_idx].generate_slides) {
    frame_generator = test::FrameGenerator::CreateSlideGenerator(
        kWidth, kHeight,
        params_.screenshare[video_idx].slide_change_interval *
            params_.video[video_idx].fps);
  } else {
    std::vector<std::string> slides = params_.screenshare[video_idx].slides;
    if (slides.size() == 0) {
      slides.push_back(test::ResourcePath("web_screenshot_1850_1110", "yuv"));
      slides.push_back(test::ResourcePath("presentation_1850_1110", "yuv"));
      slides.push_back(test::ResourcePath("photo_1850_1110", "yuv"));
      slides.push_back(test::ResourcePath("difficult_photo_1850_1110", "yuv"));
    }
    if (params_.screenshare[video_idx].scroll_duration == 0) {
      // Cycle image every slide_change_interval seconds.
      frame_generator = test::FrameGenerator::CreateFromYuvFile(
          slides, kWidth, kHeight,
          params_.screenshare[video_idx].slide_change_interval *
              params_.video[video_idx].fps);
    } else {
      RTC_CHECK_LE(params_.video[video_idx].width, kWidth);
      RTC_CHECK_LE(params_.video[video_idx].height, kHeight);
      RTC_CHECK_GT(params_.screenshare[video_idx].slide_change_interval, 0);
      const int kPauseDurationMs =
          (params_.screenshare[video_idx].slide_change_interval -
           params_.screenshare[video_idx].scroll_duration) *
          1000;
      RTC_CHECK_LE(params_.screenshare[video_idx].scroll_duration,
                   params_.screenshare[video_idx].slide_change_interval);

      frame_generator = test::FrameGenerator::CreateScrollingInputFromYuvFiles(
          clock_, slides, kWidth, kHeight, params_.video[video_idx].width,
          params_.video[video_idx].height,
          params_.screenshare[video_idx].scroll_duration * 1000,
          kPauseDurationMs);
    }
  }
  return frame_generator;
}

void VideoQualityTest::CreateCapturers() {
  RTC_DCHECK(video_sources_.empty());
  RTC_DCHECK(video_capturers_.empty());
  video_capturers_.resize(num_video_streams_);
  for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
    if (params_.screenshare[video_idx].enabled) {
      std::unique_ptr<test::FrameGenerator> frame_generator =
          CreateFrameGenerator(video_idx);
      test::FrameGeneratorCapturer* frame_generator_capturer =
          new test::FrameGeneratorCapturer(clock_, std::move(frame_generator),
                                           params_.video[video_idx].fps);
      EXPECT_TRUE(frame_generator_capturer->Init());
      video_capturers_[video_idx].reset(frame_generator_capturer);
    } else {
      if (params_.video[video_idx].clip_name == "Generator") {
        video_capturers_[video_idx].reset(test::FrameGeneratorCapturer::Create(
            static_cast<int>(params_.video[video_idx].width),
            static_cast<int>(params_.video[video_idx].height), absl::nullopt,
            absl::nullopt, params_.video[video_idx].fps, clock_));
      } else if (params_.video[video_idx].clip_name == "GeneratorI420A") {
        video_capturers_[video_idx].reset(test::FrameGeneratorCapturer::Create(
            static_cast<int>(params_.video[video_idx].width),
            static_cast<int>(params_.video[video_idx].height),
            test::FrameGenerator::OutputType::I420A, absl::nullopt,
            params_.video[video_idx].fps, clock_));
      } else if (params_.video[video_idx].clip_name == "GeneratorI010") {
        video_capturers_[video_idx].reset(test::FrameGeneratorCapturer::Create(
            static_cast<int>(params_.video[video_idx].width),
            static_cast<int>(params_.video[video_idx].height),
            test::FrameGenerator::OutputType::I010, absl::nullopt,
            params_.video[video_idx].fps, clock_));
      } else if (params_.video[video_idx].clip_name.empty()) {
        video_capturers_[video_idx].reset(test::VcmCapturer::Create(
            params_.video[video_idx].width, params_.video[video_idx].height,
            params_.video[video_idx].fps,
            params_.video[video_idx].capture_device_index));
        if (!video_capturers_[video_idx]) {
          // Failed to get actual camera, use chroma generator as backup.
          video_capturers_[video_idx].reset(
              test::FrameGeneratorCapturer::Create(
                  static_cast<int>(params_.video[video_idx].width),
                  static_cast<int>(params_.video[video_idx].height),
                  absl::nullopt, absl::nullopt, params_.video[video_idx].fps,
                  clock_));
        }
      } else {
        video_capturers_[video_idx].reset(
            test::FrameGeneratorCapturer::CreateFromYuvFile(
                test::ResourcePath(params_.video[video_idx].clip_name, "yuv"),
                params_.video[video_idx].width, params_.video[video_idx].height,
                params_.video[video_idx].fps, clock_));
        ASSERT_TRUE(video_capturers_[video_idx])
            << "Could not create capturer for "
            << params_.video[video_idx].clip_name
            << ".yuv. Is this resource file present?";
      }
    }
    RTC_DCHECK(video_capturers_[video_idx].get());
    video_sources_.push_back(video_capturers_[video_idx].get());
  }
}

void VideoQualityTest::StartAudioStreams() {
  audio_send_stream_->Start();
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    audio_recv_stream->Start();
}

void VideoQualityTest::StartThumbnailCapture() {
  for (std::unique_ptr<test::TestVideoCapturer>& capturer :
       thumbnail_capturers_)
    capturer->Start();
}

void VideoQualityTest::StopThumbnailCapture() {
  for (std::unique_ptr<test::TestVideoCapturer>& capturer :
       thumbnail_capturers_)
    capturer->Stop();
}

void VideoQualityTest::StartThumbnails() {
  for (VideoSendStream* send_stream : thumbnail_send_streams_)
    send_stream->Start();
  for (VideoReceiveStream* receive_stream : thumbnail_receive_streams_)
    receive_stream->Start();
}

void VideoQualityTest::StopThumbnails() {
  for (VideoReceiveStream* receive_stream : thumbnail_receive_streams_)
    receive_stream->Stop();
  for (VideoSendStream* send_stream : thumbnail_send_streams_)
    send_stream->Stop();
}

std::unique_ptr<test::LayerFilteringTransport>
VideoQualityTest::CreateSendTransport() {
  std::unique_ptr<NetworkBehaviorInterface> network_behavior = nullptr;
  if (injection_components_->sender_network == nullptr) {
    network_behavior = absl::make_unique<SimulatedNetwork>(*params_.config);
  } else {
    network_behavior = std::move(injection_components_->sender_network);
  }
  return absl::make_unique<test::LayerFilteringTransport>(
      &task_queue_,
      absl::make_unique<FakeNetworkPipe>(Clock::GetRealTimeClock(),
                                         std::move(network_behavior)),
      sender_call_.get(), kPayloadTypeVP8, kPayloadTypeVP9,
      params_.video[0].selected_tl, params_.ss[0].selected_sl,
      payload_type_map_, kVideoSendSsrcs[0],
      static_cast<uint32_t>(kVideoSendSsrcs[0] + params_.ss[0].streams.size() -
                            1));
}

std::unique_ptr<test::DirectTransport>
VideoQualityTest::CreateReceiveTransport() {
  std::unique_ptr<NetworkBehaviorInterface> network_behavior = nullptr;
  if (injection_components_->receiver_network == nullptr) {
    network_behavior = absl::make_unique<SimulatedNetwork>(*params_.config);
  } else {
    network_behavior = std::move(injection_components_->receiver_network);
  }
  return absl::make_unique<test::DirectTransport>(
      &task_queue_,
      absl::make_unique<FakeNetworkPipe>(Clock::GetRealTimeClock(),
                                         std::move(network_behavior)),
      receiver_call_.get(), payload_type_map_);
}

void VideoQualityTest::RunWithAnalyzer(const Params& params) {
  num_video_streams_ = params.call.dual_video ? 2 : 1;
  std::unique_ptr<test::LayerFilteringTransport> send_transport;
  std::unique_ptr<test::DirectTransport> recv_transport;
  FILE* graph_data_output_file = nullptr;
  std::unique_ptr<VideoAnalyzer> analyzer;

  params_ = params;
  // TODO(ivica): Merge with RunWithRenderer and use a flag / argument to
  // differentiate between the analyzer and the renderer case.
  CheckParamsAndInjectionComponents();

  if (!params_.analyzer.graph_data_output_filename.empty()) {
    graph_data_output_file =
        fopen(params_.analyzer.graph_data_output_filename.c_str(), "w");
    RTC_CHECK(graph_data_output_file)
        << "Can't open the file " << params_.analyzer.graph_data_output_filename
        << "!";
  }

  if (!params.logging.rtc_event_log_name.empty()) {
    send_event_log_ = RtcEventLog::Create(RtcEventLog::EncodingType::Legacy);
    recv_event_log_ = RtcEventLog::Create(RtcEventLog::EncodingType::Legacy);
    std::unique_ptr<RtcEventLogOutputFile> send_output(
        absl::make_unique<RtcEventLogOutputFile>(
            params.logging.rtc_event_log_name + "_send",
            RtcEventLog::kUnlimitedOutput));
    std::unique_ptr<RtcEventLogOutputFile> recv_output(
        absl::make_unique<RtcEventLogOutputFile>(
            params.logging.rtc_event_log_name + "_recv",
            RtcEventLog::kUnlimitedOutput));
    bool event_log_started =
        send_event_log_->StartLogging(std::move(send_output),
                                      RtcEventLog::kImmediateOutput) &&
        recv_event_log_->StartLogging(std::move(recv_output),
                                      RtcEventLog::kImmediateOutput);
    RTC_DCHECK(event_log_started);
  } else {
    send_event_log_ = RtcEventLog::CreateNull();
    recv_event_log_ = RtcEventLog::CreateNull();
  }

  Call::Config send_call_config(send_event_log_.get());
  Call::Config recv_call_config(recv_event_log_.get());
  send_call_config.bitrate_config = params.call.call_bitrate_config;
  recv_call_config.bitrate_config = params.call.call_bitrate_config;

  task_queue_.SendTask([this, &send_call_config, &recv_call_config,
                        &send_transport, &recv_transport]() {
    if (params_.audio.enabled)
      InitializeAudioDevice(
          &send_call_config, &recv_call_config, params_.audio.use_real_adm);

    CreateCalls(send_call_config, recv_call_config);
    send_transport = CreateSendTransport();
    recv_transport = CreateReceiveTransport();
  });

  std::string graph_title = params_.analyzer.graph_title;
  if (graph_title.empty())
    graph_title = VideoQualityTest::GenerateGraphTitle();
  bool is_quick_test_enabled = field_trial::IsEnabled("WebRTC-QuickPerfTest");
  analyzer = absl::make_unique<VideoAnalyzer>(
      send_transport.get(), params_.analyzer.test_label,
      params_.analyzer.avg_psnr_threshold, params_.analyzer.avg_ssim_threshold,
      is_quick_test_enabled
          ? kFramesSentInQuickTest
          : params_.analyzer.test_durations_secs * params_.video[0].fps,
      graph_data_output_file, graph_title,
      kVideoSendSsrcs[params_.ss[0].selected_stream],
      kSendRtxSsrcs[params_.ss[0].selected_stream],
      static_cast<size_t>(params_.ss[0].selected_stream),
      params.ss[0].selected_sl, params_.video[0].selected_tl,
      is_quick_test_enabled, clock_, params_.logging.rtp_dump_name);

  task_queue_.SendTask([&]() {
    analyzer->SetCall(sender_call_.get());
    analyzer->SetReceiver(receiver_call_->Receiver());
    send_transport->SetReceiver(analyzer.get());
    recv_transport->SetReceiver(sender_call_->Receiver());

    SetupVideo(analyzer.get(), recv_transport.get());
    SetupThumbnails(analyzer.get(), recv_transport.get());
    video_receive_configs_[params_.ss[0].selected_stream].renderer =
        analyzer.get();
    GetVideoSendConfig()->pre_encode_callback = analyzer->pre_encode_proxy();
    RTC_DCHECK(!GetVideoSendConfig()->post_encode_callback);
    GetVideoSendConfig()->post_encode_callback = analyzer.get();

    CreateFlexfecStreams();
    CreateVideoStreams();
    analyzer->SetSendStream(video_send_streams_[0]);
    if (video_receive_streams_.size() == 1)
      analyzer->SetReceiveStream(video_receive_streams_[0]);

    GetVideoSendStream()->SetSource(analyzer->OutputInterface(),
                                    degradation_preference_);
    SetupThumbnailCapturers(params_.call.num_thumbnails);
    for (size_t i = 0; i < thumbnail_send_streams_.size(); ++i) {
      thumbnail_send_streams_[i]->SetSource(thumbnail_capturers_[i].get(),
                                            degradation_preference_);
    }

    CreateCapturers();

    analyzer->SetSource(video_capturers_[0].get(), params_.ss[0].infer_streams);

    for (size_t video_idx = 1; video_idx < num_video_streams_; ++video_idx) {
      video_send_streams_[video_idx]->SetSource(
          video_capturers_[video_idx].get(), degradation_preference_);
    }

    if (params_.audio.enabled) {
      SetupAudio(send_transport.get());
      StartAudioStreams();
      analyzer->SetAudioReceiveStream(audio_receive_streams_[0]);
    }
    StartVideoStreams();
    StartThumbnails();
    analyzer->StartMeasuringCpuProcessTime();
    StartVideoCapture();
    StartThumbnailCapture();
  });

  analyzer->Wait();

  task_queue_.SendTask([&]() {
    StopThumbnailCapture();
    StopThumbnails();
    Stop();

    DestroyStreams();
    DestroyThumbnailStreams();

    if (graph_data_output_file)
      fclose(graph_data_output_file);

    video_capturers_.clear();
    send_transport.reset();
    recv_transport.reset();

    DestroyCalls();
  });
}

rtc::scoped_refptr<AudioDeviceModule> VideoQualityTest::CreateAudioDevice() {
#ifdef WEBRTC_WIN
    RTC_LOG(INFO) << "Using latest version of ADM on Windows";
    // We must initialize the COM library on a thread before we calling any of
    // the library functions. All COM functions in the ADM will return
    // CO_E_NOTINITIALIZED otherwise. The legacy ADM for Windows used internal
    // COM initialization but the new ADM requires COM to be initialized
    // externally.
    com_initializer_ = absl::make_unique<webrtc_win::ScopedCOMInitializer>(
        webrtc_win::ScopedCOMInitializer::kMTA);
    RTC_CHECK(com_initializer_->Succeeded());
    RTC_CHECK(webrtc_win::core_audio_utility::IsSupported());
    RTC_CHECK(webrtc_win::core_audio_utility::IsMMCSSSupported());
    return CreateWindowsCoreAudioAudioDeviceModule();
#else
    // Use legacy factory method on all platforms except Windows.
    return AudioDeviceModule::Create(AudioDeviceModule::kPlatformDefaultAudio);
#endif
}

void VideoQualityTest::InitializeAudioDevice(Call::Config* send_call_config,
                                             Call::Config* recv_call_config,
                                             bool use_real_adm) {
  rtc::scoped_refptr<AudioDeviceModule> audio_device;
  if (use_real_adm) {
    // Run test with real ADM (using default audio devices) if user has
    // explicitly set the --audio and --use_real_adm command-line flags.
    audio_device = CreateAudioDevice();
  } else {
    // By default, create a test ADM which fakes audio.
    audio_device = TestAudioDeviceModule::CreateTestAudioDeviceModule(
        TestAudioDeviceModule::CreatePulsedNoiseCapturer(32000, 48000),
        TestAudioDeviceModule::CreateDiscardRenderer(48000), 1.f);
  }
  RTC_CHECK(audio_device);

  AudioState::Config audio_state_config;
  audio_state_config.audio_mixer = AudioMixerImpl::Create();
  audio_state_config.audio_processing = AudioProcessingBuilder().Create();
  audio_state_config.audio_device_module = audio_device;
  send_call_config->audio_state = AudioState::Create(audio_state_config);
  recv_call_config->audio_state = AudioState::Create(audio_state_config);
  if (use_real_adm) {
    // The real ADM requires extra initialization: setting default devices,
    // setting up number of channels etc. Helper class also calls
    // AudioDeviceModule::Init().
    webrtc::adm_helpers::Init(audio_device.get());
  } else {
    audio_device->Init();
  }
  // Always initialize the ADM before injecting a valid audio transport.
  RTC_CHECK(audio_device->RegisterAudioCallback(
            send_call_config->audio_state->audio_transport()) == 0);
}

void VideoQualityTest::SetupAudio(Transport* transport) {
  AudioSendStream::Config audio_send_config(transport);
  audio_send_config.rtp.ssrc = kAudioSendSsrc;

  // Add extension to enable audio send side BWE, and allow audio bit rate
  // adaptation.
  audio_send_config.rtp.extensions.clear();
  audio_send_config.send_codec_spec = AudioSendStream::Config::SendCodecSpec(
      kAudioSendPayloadType,
      {"OPUS",
       48000,
       2,
       {{"usedtx", (params_.audio.dtx ? "1" : "0")}, {"stereo", "1"}}});

  if (params_.call.send_side_bwe) {
    audio_send_config.rtp.extensions.push_back(
        webrtc::RtpExtension(webrtc::RtpExtension::kTransportSequenceNumberUri,
                             test::kTransportSequenceNumberExtensionId));
    audio_send_config.min_bitrate_bps = kOpusMinBitrateBps;
    audio_send_config.max_bitrate_bps = kOpusBitrateFbBps;
    audio_send_config.send_codec_spec->transport_cc_enabled = true;
  }
  audio_send_config.encoder_factory = audio_encoder_factory_;
  SetAudioConfig(audio_send_config);

  std::string sync_group;
  if (params_.video[0].enabled && params_.audio.sync_video)
    sync_group = kSyncGroup;

  CreateMatchingAudioConfigs(transport, sync_group);
  CreateAudioStreams();
}

void VideoQualityTest::RunWithRenderers(const Params& params) {
  RTC_LOG(INFO) << __FUNCTION__;
  num_video_streams_ = params.call.dual_video ? 2 : 1;
  std::unique_ptr<test::LayerFilteringTransport> send_transport;
  std::unique_ptr<test::DirectTransport> recv_transport;
  std::unique_ptr<test::VideoRenderer> local_preview;
  std::vector<std::unique_ptr<test::VideoRenderer>> loopback_renderers;
  RtcEventLogNullImpl null_event_log;

  task_queue_.SendTask([&]() {
    params_ = params;
    CheckParamsAndInjectionComponents();

    // TODO(ivica): Remove bitrate_config and use the default Call::Config(), to
    // match the full stack tests.
    Call::Config send_call_config(&null_event_log);
    send_call_config.bitrate_config = params_.call.call_bitrate_config;
    Call::Config recv_call_config(&null_event_log);

    if (params_.audio.enabled)
      InitializeAudioDevice(
          &send_call_config, &recv_call_config, params_.audio.use_real_adm);

    CreateCalls(send_call_config, recv_call_config);

    // TODO(minyue): consider if this is a good transport even for audio only
    // calls.
    send_transport = CreateSendTransport();

    recv_transport = CreateReceiveTransport();

    // TODO(ivica): Use two calls to be able to merge with RunWithAnalyzer or at
    // least share as much code as possible. That way this test would also match
    // the full stack tests better.
    send_transport->SetReceiver(receiver_call_->Receiver());
    recv_transport->SetReceiver(sender_call_->Receiver());

    if (params_.video[0].enabled) {
      // Create video renderers.
      local_preview.reset(test::VideoRenderer::Create(
          "Local Preview", params_.video[0].width, params_.video[0].height));

      SetupVideo(send_transport.get(), recv_transport.get());
      GetVideoSendConfig()->pre_encode_callback = local_preview.get();

      size_t num_streams_processed = 0;
      for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
        const size_t selected_stream_id = params_.ss[video_idx].selected_stream;
        const size_t num_streams = params_.ss[video_idx].streams.size();
        if (selected_stream_id == num_streams) {
          for (size_t stream_id = 0; stream_id < num_streams; ++stream_id) {
            rtc::StringBuilder oss;
            oss << "Loopback Video #" << video_idx << " - Stream #"
                << static_cast<int>(stream_id);
            loopback_renderers.emplace_back(test::VideoRenderer::Create(
                oss.str().c_str(),
                params_.ss[video_idx].streams[stream_id].width,
                params_.ss[video_idx].streams[stream_id].height));
            video_receive_configs_[stream_id + num_streams_processed].renderer =
                loopback_renderers.back().get();
            if (params_.audio.enabled && params_.audio.sync_video)
              video_receive_configs_[stream_id + num_streams_processed]
                  .sync_group = kSyncGroup;
          }
        } else {
          rtc::StringBuilder oss;
          oss << "Loopback Video #" << video_idx;
          loopback_renderers.emplace_back(test::VideoRenderer::Create(
              oss.str().c_str(),
              params_.ss[video_idx].streams[selected_stream_id].width,
              params_.ss[video_idx].streams[selected_stream_id].height));
          video_receive_configs_[selected_stream_id + num_streams_processed]
              .renderer = loopback_renderers.back().get();
          if (params_.audio.enabled && params_.audio.sync_video)
            video_receive_configs_[num_streams_processed + selected_stream_id]
                .sync_group = kSyncGroup;
        }
        num_streams_processed += num_streams;
      }
      CreateFlexfecStreams();
      CreateVideoStreams();

      CreateCapturers();
      ConnectVideoSourcesToStreams();
    }

    if (params_.audio.enabled) {
      SetupAudio(send_transport.get());
    }

    Start();
  });

  test::PressEnterToContinue();

  task_queue_.SendTask([&]() {
    Stop();
    DestroyStreams();

    video_capturers_.clear();
    send_transport.reset();
    recv_transport.reset();

    local_preview.reset();
    loopback_renderers.clear();

    DestroyCalls();
  });
}

}  // namespace webrtc
