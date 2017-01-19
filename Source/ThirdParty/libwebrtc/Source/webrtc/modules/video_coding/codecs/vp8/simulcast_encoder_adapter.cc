/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/vp8/simulcast_encoder_adapter.h"

#include <algorithm>

// NOTE(ajm): Path provided by gyp.
#include "libyuv/scale.h"  // NOLINT

#include "webrtc/base/checks.h"
#include "webrtc/modules/video_coding/codecs/vp8/screenshare_layers.h"
#include "webrtc/modules/video_coding/utility/simulcast_rate_allocator.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace {

const unsigned int kDefaultMinQp = 2;
const unsigned int kDefaultMaxQp = 56;
// Max qp for lowest spatial resolution when doing simulcast.
const unsigned int kLowestResMaxQp = 45;

uint32_t SumStreamMaxBitrate(int streams, const webrtc::VideoCodec& codec) {
  uint32_t bitrate_sum = 0;
  for (int i = 0; i < streams; ++i) {
    bitrate_sum += codec.simulcastStream[i].maxBitrate;
  }
  return bitrate_sum;
}

int NumberOfStreams(const webrtc::VideoCodec& codec) {
  int streams =
      codec.numberOfSimulcastStreams < 1 ? 1 : codec.numberOfSimulcastStreams;
  uint32_t simulcast_max_bitrate = SumStreamMaxBitrate(streams, codec);
  if (simulcast_max_bitrate == 0) {
    streams = 1;
  }
  return streams;
}

bool ValidSimulcastResolutions(const webrtc::VideoCodec& codec,
                               int num_streams) {
  if (codec.width != codec.simulcastStream[num_streams - 1].width ||
      codec.height != codec.simulcastStream[num_streams - 1].height) {
    return false;
  }
  for (int i = 0; i < num_streams; ++i) {
    if (codec.width * codec.simulcastStream[i].height !=
        codec.height * codec.simulcastStream[i].width) {
      return false;
    }
  }
  return true;
}

int VerifyCodec(const webrtc::VideoCodec* inst) {
  if (inst == NULL) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->maxFramerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // allow zero to represent an unspecified maxBitRate
  if (inst->maxBitrate > 0 && inst->startBitrate > inst->maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->width <= 1 || inst->height <= 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->VP8().feedbackModeOn && inst->numberOfSimulcastStreams > 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->VP8().automaticResizeOn && inst->numberOfSimulcastStreams > 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

struct ScreenshareTemporalLayersFactory : webrtc::TemporalLayersFactory {
  ScreenshareTemporalLayersFactory() {}
  virtual ~ScreenshareTemporalLayersFactory() {}

  virtual webrtc::TemporalLayers* Create(int num_temporal_layers,
                                         uint8_t initial_tl0_pic_idx) const {
    return new webrtc::ScreenshareLayers(num_temporal_layers, rand(),
                                         webrtc::Clock::GetRealTimeClock());
  }
};

// An EncodedImageCallback implementation that forwards on calls to a
// SimulcastEncoderAdapter, but with the stream index it's registered with as
// the first parameter to Encoded.
class AdapterEncodedImageCallback : public webrtc::EncodedImageCallback {
 public:
  AdapterEncodedImageCallback(webrtc::SimulcastEncoderAdapter* adapter,
                              size_t stream_idx)
      : adapter_(adapter), stream_idx_(stream_idx) {}

  EncodedImageCallback::Result OnEncodedImage(
      const webrtc::EncodedImage& encoded_image,
      const webrtc::CodecSpecificInfo* codec_specific_info,
      const webrtc::RTPFragmentationHeader* fragmentation) override {
    return adapter_->OnEncodedImage(stream_idx_, encoded_image,
                                    codec_specific_info, fragmentation);
  }

 private:
  webrtc::SimulcastEncoderAdapter* const adapter_;
  const size_t stream_idx_;
};

}  // namespace

namespace webrtc {

SimulcastEncoderAdapter::SimulcastEncoderAdapter(VideoEncoderFactory* factory)
    : factory_(factory),
      encoded_complete_callback_(nullptr),
      implementation_name_("SimulcastEncoderAdapter") {
  memset(&codec_, 0, sizeof(webrtc::VideoCodec));
  rate_allocator_.reset(new SimulcastRateAllocator(codec_));
}

SimulcastEncoderAdapter::~SimulcastEncoderAdapter() {
  Release();
}

int SimulcastEncoderAdapter::Release() {
  // TODO(pbos): Keep the last encoder instance but call ::Release() on it, then
  // re-use this instance in ::InitEncode(). This means that changing
  // resolutions doesn't require reallocation of the first encoder, but only
  // reinitialization, which makes sense. Then Destroy this instance instead in
  // ~SimulcastEncoderAdapter().
  while (!streaminfos_.empty()) {
    VideoEncoder* encoder = streaminfos_.back().encoder;
    EncodedImageCallback* callback = streaminfos_.back().callback;
    factory_->Destroy(encoder);
    delete callback;
    streaminfos_.pop_back();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::InitEncode(const VideoCodec* inst,
                                        int number_of_cores,
                                        size_t max_payload_size) {
  if (number_of_cores < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  int ret = VerifyCodec(inst);
  if (ret < 0) {
    return ret;
  }

  ret = Release();
  if (ret < 0) {
    return ret;
  }

  int number_of_streams = NumberOfStreams(*inst);
  const bool doing_simulcast = (number_of_streams > 1);

  if (doing_simulcast && !ValidSimulcastResolutions(*inst, number_of_streams)) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  codec_ = *inst;
  rate_allocator_.reset(new SimulcastRateAllocator(codec_));
  std::vector<uint32_t> start_bitrates =
      rate_allocator_->GetAllocation(codec_.startBitrate);

  // Special mode when screensharing on a single stream.
  if (number_of_streams == 1 && inst->mode == kScreensharing) {
    screensharing_tl_factory_.reset(new ScreenshareTemporalLayersFactory());
    codec_.VP8()->tl_factory = screensharing_tl_factory_.get();
  }

  std::string implementation_name;
  // Create |number_of_streams| of encoder instances and init them.
  for (int i = 0; i < number_of_streams; ++i) {
    VideoCodec stream_codec;
    uint32_t start_bitrate_kbps = start_bitrates[i];
    if (!doing_simulcast) {
      stream_codec = codec_;
      stream_codec.numberOfSimulcastStreams = 1;
    } else {
      // Cap start bitrate to the min bitrate in order to avoid strange codec
      // behavior. Since sending sending will be false, this should not matter.
      start_bitrate_kbps =
          std::max(codec_.simulcastStream[i].minBitrate, start_bitrate_kbps);
      bool highest_resolution_stream = (i == (number_of_streams - 1));
      PopulateStreamCodec(&codec_, i, start_bitrate_kbps,
                          highest_resolution_stream, &stream_codec);
    }

    // TODO(ronghuawu): Remove once this is handled in VP8EncoderImpl.
    if (stream_codec.qpMax < kDefaultMinQp) {
      stream_codec.qpMax = kDefaultMaxQp;
    }

    VideoEncoder* encoder = factory_->Create();
    ret = encoder->InitEncode(&stream_codec, number_of_cores, max_payload_size);
    if (ret < 0) {
      Release();
      return ret;
    }
    EncodedImageCallback* callback = new AdapterEncodedImageCallback(this, i);
    encoder->RegisterEncodeCompleteCallback(callback);
    streaminfos_.push_back(StreamInfo(encoder, callback, stream_codec.width,
                                      stream_codec.height,
                                      start_bitrate_kbps > 0));
    if (i != 0)
      implementation_name += ", ";
    implementation_name += streaminfos_[i].encoder->ImplementationName();
  }
  if (doing_simulcast) {
    implementation_name_ =
        "SimulcastEncoderAdapter (" + implementation_name + ")";
  } else {
    implementation_name_ = implementation_name;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::Encode(
    const VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  if (!Initialized()) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (encoded_complete_callback_ == NULL) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // All active streams should generate a key frame if
  // a key frame is requested by any stream.
  bool send_key_frame = false;
  if (frame_types) {
    for (size_t i = 0; i < frame_types->size(); ++i) {
      if (frame_types->at(i) == kVideoFrameKey) {
        send_key_frame = true;
        break;
      }
    }
  }
  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    if (streaminfos_[stream_idx].key_frame_request &&
        streaminfos_[stream_idx].send_stream) {
      send_key_frame = true;
      break;
    }
  }

  int src_width = input_image.width();
  int src_height = input_image.height();
  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    // Don't encode frames in resolutions that we don't intend to send.
    if (!streaminfos_[stream_idx].send_stream)
      continue;

    std::vector<FrameType> stream_frame_types;
    if (send_key_frame) {
      stream_frame_types.push_back(kVideoFrameKey);
      streaminfos_[stream_idx].key_frame_request = false;
    } else {
      stream_frame_types.push_back(kVideoFrameDelta);
    }

    int dst_width = streaminfos_[stream_idx].width;
    int dst_height = streaminfos_[stream_idx].height;
    // If scaling isn't required, because the input resolution
    // matches the destination or the input image is empty (e.g.
    // a keyframe request for encoders with internal camera
    // sources) or the source image has a native handle, pass the image on
    // directly. Otherwise, we'll scale it to match what the encoder expects
    // (below).
    // For texture frames, the underlying encoder is expected to be able to
    // correctly sample/scale the source texture.
    // TODO(perkj): ensure that works going forward, and figure out how this
    // affects webrtc:5683.
    if ((dst_width == src_width && dst_height == src_height) ||
        input_image.IsZeroSize() ||
        input_image.video_frame_buffer()->native_handle()) {
      int ret = streaminfos_[stream_idx].encoder->Encode(
          input_image, codec_specific_info, &stream_frame_types);
      if (ret != WEBRTC_VIDEO_CODEC_OK) {
        return ret;
      }
    } else {
      // Aligning stride values based on width.
      rtc::scoped_refptr<I420Buffer> dst_buffer =
          I420Buffer::Create(dst_width, dst_height, dst_width,
                             (dst_width + 1) / 2, (dst_width + 1) / 2);
      libyuv::I420Scale(input_image.video_frame_buffer()->DataY(),
                        input_image.video_frame_buffer()->StrideY(),
                        input_image.video_frame_buffer()->DataU(),
                        input_image.video_frame_buffer()->StrideU(),
                        input_image.video_frame_buffer()->DataV(),
                        input_image.video_frame_buffer()->StrideV(),
                        src_width, src_height,
                        dst_buffer->MutableDataY(), dst_buffer->StrideY(),
                        dst_buffer->MutableDataU(), dst_buffer->StrideU(),
                        dst_buffer->MutableDataV(), dst_buffer->StrideV(),
                        dst_width, dst_height,
                        libyuv::kFilterBilinear);

      int ret = streaminfos_[stream_idx].encoder->Encode(
          VideoFrame(dst_buffer, input_image.timestamp(),
                     input_image.render_time_ms(), webrtc::kVideoRotation_0),
          codec_specific_info, &stream_frame_types);
      if (ret != WEBRTC_VIDEO_CODEC_OK) {
        return ret;
      }
    }
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  encoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::SetChannelParameters(uint32_t packet_loss,
                                                  int64_t rtt) {
  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    streaminfos_[stream_idx].encoder->SetChannelParameters(packet_loss, rtt);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::SetRates(uint32_t new_bitrate_kbit,
                                      uint32_t new_framerate) {
  if (!Initialized()) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (new_framerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_.maxBitrate > 0 && new_bitrate_kbit > codec_.maxBitrate) {
    new_bitrate_kbit = codec_.maxBitrate;
  }

  std::vector<uint32_t> stream_bitrates;
  if (new_bitrate_kbit > 0) {
    // Make sure the bitrate fits the configured min bitrates. 0 is a special
    // value that means paused, though, so leave it alone.
    if (new_bitrate_kbit < codec_.minBitrate) {
      new_bitrate_kbit = codec_.minBitrate;
    }
    if (codec_.numberOfSimulcastStreams > 0 &&
        new_bitrate_kbit < codec_.simulcastStream[0].minBitrate) {
      new_bitrate_kbit = codec_.simulcastStream[0].minBitrate;
    }
    stream_bitrates = rate_allocator_->GetAllocation(new_bitrate_kbit);
  }
  codec_.maxFramerate = new_framerate;

  // Disable any stream not in the current allocation.
  stream_bitrates.resize(streaminfos_.size(), 0U);

  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    uint32_t stream_bitrate_kbps = stream_bitrates[stream_idx];
    // Need a key frame if we have not sent this stream before.
    if (stream_bitrate_kbps > 0 && !streaminfos_[stream_idx].send_stream) {
      streaminfos_[stream_idx].key_frame_request = true;
    }
    streaminfos_[stream_idx].send_stream = stream_bitrate_kbps > 0;

    // TODO(holmer): This is a temporary hack for screensharing, where we
    // interpret the startBitrate as the encoder target bitrate. This is
    // to allow for a different max bitrate, so if the codec can't meet
    // the target we still allow it to overshoot up to the max before dropping
    // frames. This hack should be improved.
    if (codec_.targetBitrate > 0 &&
        (codec_.VP8()->numberOfTemporalLayers == 2 ||
         codec_.simulcastStream[0].numberOfTemporalLayers == 2)) {
      stream_bitrate_kbps = std::min(codec_.maxBitrate, stream_bitrate_kbps);
      // TODO(ronghuawu): Can't change max bitrate via the VideoEncoder
      // interface. And VP8EncoderImpl doesn't take negative framerate.
      // max_bitrate = std::min(codec_.maxBitrate, stream_bitrate_kbps);
      // new_framerate = -1;
    }

    streaminfos_[stream_idx].encoder->SetRates(stream_bitrate_kbps,
                                               new_framerate);
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

EncodedImageCallback::Result SimulcastEncoderAdapter::OnEncodedImage(
    size_t stream_idx,
    const EncodedImage& encodedImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const RTPFragmentationHeader* fragmentation) {
  CodecSpecificInfo stream_codec_specific = *codecSpecificInfo;
  stream_codec_specific.codec_name = implementation_name_.c_str();
  CodecSpecificInfoVP8* vp8Info = &(stream_codec_specific.codecSpecific.VP8);
  vp8Info->simulcastIdx = stream_idx;

  return encoded_complete_callback_->OnEncodedImage(
      encodedImage, &stream_codec_specific, fragmentation);
}

void SimulcastEncoderAdapter::PopulateStreamCodec(
    const webrtc::VideoCodec* inst,
    int stream_index,
    uint32_t start_bitrate_kbps,
    bool highest_resolution_stream,
    webrtc::VideoCodec* stream_codec) {
  *stream_codec = *inst;

  // Stream specific settings.
  stream_codec->VP8()->numberOfTemporalLayers =
      inst->simulcastStream[stream_index].numberOfTemporalLayers;
  stream_codec->numberOfSimulcastStreams = 0;
  stream_codec->width = inst->simulcastStream[stream_index].width;
  stream_codec->height = inst->simulcastStream[stream_index].height;
  stream_codec->maxBitrate = inst->simulcastStream[stream_index].maxBitrate;
  stream_codec->minBitrate = inst->simulcastStream[stream_index].minBitrate;
  stream_codec->qpMax = inst->simulcastStream[stream_index].qpMax;
  // Settings that are based on stream/resolution.
  if (stream_index == 0) {
    // Settings for lowest spatial resolutions.
    stream_codec->qpMax = kLowestResMaxQp;
  }
  if (!highest_resolution_stream) {
    // For resolutions below CIF, set the codec |complexity| parameter to
    // kComplexityHigher, which maps to cpu_used = -4.
    int pixels_per_frame = stream_codec->width * stream_codec->height;
    if (pixels_per_frame < 352 * 288) {
      stream_codec->VP8()->complexity = webrtc::kComplexityHigher;
    }
    // Turn off denoising for all streams but the highest resolution.
    stream_codec->VP8()->denoisingOn = false;
  }
  // TODO(ronghuawu): what to do with targetBitrate.

  stream_codec->startBitrate = start_bitrate_kbps;
}

bool SimulcastEncoderAdapter::Initialized() const {
  return !streaminfos_.empty();
}

void SimulcastEncoderAdapter::OnDroppedFrame() {
  streaminfos_[0].encoder->OnDroppedFrame();
}

bool SimulcastEncoderAdapter::SupportsNativeHandle() const {
  // We should not be calling this method before streaminfos_ are configured.
  RTC_DCHECK(!streaminfos_.empty());
  for (const auto& streaminfo : streaminfos_) {
    if (!streaminfo.encoder->SupportsNativeHandle())
      return false;
  }
  return true;
}

const char* SimulcastEncoderAdapter::ImplementationName() const {
  return implementation_name_.c_str();
}

}  // namespace webrtc
