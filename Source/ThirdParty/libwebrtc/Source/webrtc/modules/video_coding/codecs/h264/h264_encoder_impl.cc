/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/video_coding/codecs/h264/h264_encoder_impl.h"

#include <limits>
#include <string>

#include "third_party/openh264/src/codec/api/svc/codec_api.h"
#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_ver.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

const bool kOpenH264EncoderDetailedLogging = false;

// Used by histograms. Values of entries should not be changed.
enum H264EncoderImplEvent {
  kH264EncoderEventInit = 0,
  kH264EncoderEventError = 1,
  kH264EncoderEventMax = 16,
};

int NumberOfThreads(int width, int height, int number_of_cores) {
  // TODO(hbos): In Chromium, multiple threads do not work with sandbox on Mac,
  // see crbug.com/583348. Until further investigated, only use one thread.
//  if (width * height >= 1920 * 1080 && number_of_cores > 8) {
//    return 8;  // 8 threads for 1080p on high perf machines.
//  } else if (width * height > 1280 * 960 && number_of_cores >= 6) {
//    return 3;  // 3 threads for 1080p.
//  } else if (width * height > 640 * 480 && number_of_cores >= 3) {
//    return 2;  // 2 threads for qHD/HD.
//  } else {
//    return 1;  // 1 thread for VGA or less.
//  }
// TODO(sprang): Also check sSliceArgument.uiSliceNum om GetEncoderPrams(),
//               before enabling multithreading here.
  return 1;
}

FrameType ConvertToVideoFrameType(EVideoFrameType type) {
  switch (type) {
    case videoFrameTypeIDR:
      return kVideoFrameKey;
    case videoFrameTypeSkip:
    case videoFrameTypeI:
    case videoFrameTypeP:
    case videoFrameTypeIPMixed:
      return kVideoFrameDelta;
    case videoFrameTypeInvalid:
      break;
  }
  RTC_NOTREACHED() << "Unexpected/invalid frame type: " << type;
  return kEmptyFrame;
}

}  // namespace

// Helper method used by H264EncoderImpl::Encode.
// Copies the encoded bytes from |info| to |encoded_image| and updates the
// fragmentation information of |frag_header|. The |encoded_image->_buffer| may
// be deleted and reallocated if a bigger buffer is required.
//
// After OpenH264 encoding, the encoded bytes are stored in |info| spread out
// over a number of layers and "NAL units". Each NAL unit is a fragment starting
// with the four-byte start code {0,0,0,1}. All of this data (including the
// start codes) is copied to the |encoded_image->_buffer| and the |frag_header|
// is updated to point to each fragment, with offsets and lengths set as to
// exclude the start codes.
static void RtpFragmentize(EncodedImage* encoded_image,
                           std::unique_ptr<uint8_t[]>* encoded_image_buffer,
                           const VideoFrameBuffer& frame_buffer,
                           SFrameBSInfo* info,
                           RTPFragmentationHeader* frag_header) {
  // Calculate minimum buffer size required to hold encoded data.
  size_t required_size = 0;
  size_t fragments_count = 0;
  for (int layer = 0; layer < info->iLayerNum; ++layer) {
    const SLayerBSInfo& layerInfo = info->sLayerInfo[layer];
    for (int nal = 0; nal < layerInfo.iNalCount; ++nal, ++fragments_count) {
      RTC_CHECK_GE(layerInfo.pNalLengthInByte[nal], 0);
      // Ensure |required_size| will not overflow.
      RTC_CHECK_LE(layerInfo.pNalLengthInByte[nal],
                   std::numeric_limits<size_t>::max() - required_size);
      required_size += layerInfo.pNalLengthInByte[nal];
    }
  }
  if (encoded_image->_size < required_size) {
    // Increase buffer size. Allocate enough to hold an unencoded image, this
    // should be more than enough to hold any encoded data of future frames of
    // the same size (avoiding possible future reallocation due to variations in
    // required size).
    encoded_image->_size = CalcBufferSize(
        VideoType::kI420, frame_buffer.width(), frame_buffer.height());
    if (encoded_image->_size < required_size) {
      // Encoded data > unencoded data. Allocate required bytes.
      LOG(LS_WARNING) << "Encoding produced more bytes than the original image "
                      << "data! Original bytes: " << encoded_image->_size
                      << ", encoded bytes: " << required_size << ".";
      encoded_image->_size = required_size;
    }
    encoded_image->_buffer = new uint8_t[encoded_image->_size];
    encoded_image_buffer->reset(encoded_image->_buffer);
  }

  // Iterate layers and NAL units, note each NAL unit as a fragment and copy
  // the data to |encoded_image->_buffer|.
  const uint8_t start_code[4] = {0, 0, 0, 1};
  frag_header->VerifyAndAllocateFragmentationHeader(fragments_count);
  size_t frag = 0;
  encoded_image->_length = 0;
  for (int layer = 0; layer < info->iLayerNum; ++layer) {
    const SLayerBSInfo& layerInfo = info->sLayerInfo[layer];
    // Iterate NAL units making up this layer, noting fragments.
    size_t layer_len = 0;
    for (int nal = 0; nal < layerInfo.iNalCount; ++nal, ++frag) {
      // Because the sum of all layer lengths, |required_size|, fits in a
      // |size_t|, we know that any indices in-between will not overflow.
      RTC_DCHECK_GE(layerInfo.pNalLengthInByte[nal], 4);
      RTC_DCHECK_EQ(layerInfo.pBsBuf[layer_len+0], start_code[0]);
      RTC_DCHECK_EQ(layerInfo.pBsBuf[layer_len+1], start_code[1]);
      RTC_DCHECK_EQ(layerInfo.pBsBuf[layer_len+2], start_code[2]);
      RTC_DCHECK_EQ(layerInfo.pBsBuf[layer_len+3], start_code[3]);
      frag_header->fragmentationOffset[frag] =
          encoded_image->_length + layer_len + sizeof(start_code);
      frag_header->fragmentationLength[frag] =
          layerInfo.pNalLengthInByte[nal] - sizeof(start_code);
      layer_len += layerInfo.pNalLengthInByte[nal];
    }
    // Copy the entire layer's data (including start codes).
    memcpy(encoded_image->_buffer + encoded_image->_length,
           layerInfo.pBsBuf,
           layer_len);
    encoded_image->_length += layer_len;
  }
}

H264EncoderImpl::H264EncoderImpl(const cricket::VideoCodec& codec)
    : openh264_encoder_(nullptr),
      width_(0),
      height_(0),
      max_frame_rate_(0.0f),
      target_bps_(0),
      max_bps_(0),
      mode_(kRealtimeVideo),
      frame_dropping_on_(false),
      key_frame_interval_(0),
      packetization_mode_(H264PacketizationMode::SingleNalUnit),
      max_payload_size_(0),
      number_of_cores_(0),
      encoded_image_callback_(nullptr),
      has_reported_init_(false),
      has_reported_error_(false) {
  RTC_CHECK(cricket::CodecNamesEq(codec.name, cricket::kH264CodecName));
  std::string packetization_mode_string;
  if (codec.GetParam(cricket::kH264FmtpPacketizationMode,
                     &packetization_mode_string) &&
      packetization_mode_string == "1") {
    packetization_mode_ = H264PacketizationMode::NonInterleaved;
  }
}

H264EncoderImpl::~H264EncoderImpl() {
  Release();
}

int32_t H264EncoderImpl::InitEncode(const VideoCodec* codec_settings,
                                    int32_t number_of_cores,
                                    size_t max_payload_size) {
  ReportInit();
  if (!codec_settings ||
      codec_settings->codecType != kVideoCodecH264) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings->maxFramerate == 0) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings->width < 1 || codec_settings->height < 1) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  int32_t release_ret = Release();
  if (release_ret != WEBRTC_VIDEO_CODEC_OK) {
    ReportError();
    return release_ret;
  }
  RTC_DCHECK(!openh264_encoder_);

  // Create encoder.
  if (WelsCreateSVCEncoder(&openh264_encoder_) != 0) {
    // Failed to create encoder.
    LOG(LS_ERROR) << "Failed to create OpenH264 encoder";
    RTC_DCHECK(!openh264_encoder_);
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_DCHECK(openh264_encoder_);
  if (kOpenH264EncoderDetailedLogging) {
    int trace_level = WELS_LOG_DETAIL;
    openh264_encoder_->SetOption(ENCODER_OPTION_TRACE_LEVEL,
                                 &trace_level);
  }
  // else WELS_LOG_DEFAULT is used by default.

  number_of_cores_ = number_of_cores;
  // Set internal settings from codec_settings
  width_ = codec_settings->width;
  height_ = codec_settings->height;
  max_frame_rate_ = static_cast<float>(codec_settings->maxFramerate);
  mode_ = codec_settings->mode;
  frame_dropping_on_ = codec_settings->H264().frameDroppingOn;
  key_frame_interval_ = codec_settings->H264().keyFrameInterval;
  max_payload_size_ = max_payload_size;

  // Codec_settings uses kbits/second; encoder uses bits/second.
  max_bps_ = codec_settings->maxBitrate * 1000;
  if (codec_settings->targetBitrate == 0)
    target_bps_ = codec_settings->startBitrate * 1000;
  else
    target_bps_ = codec_settings->targetBitrate * 1000;

  SEncParamExt encoder_params = CreateEncoderParams();

  // Initialize.
  if (openh264_encoder_->InitializeExt(&encoder_params) != 0) {
    LOG(LS_ERROR) << "Failed to initialize OpenH264 encoder";
    Release();
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  // TODO(pbos): Base init params on these values before submitting.
  int video_format = EVideoFormatType::videoFormatI420;
  openh264_encoder_->SetOption(ENCODER_OPTION_DATAFORMAT,
                               &video_format);

  // Initialize encoded image. Default buffer size: size of unencoded data.
  encoded_image_._size = CalcBufferSize(VideoType::kI420, codec_settings->width,
                                        codec_settings->height);
  encoded_image_._buffer = new uint8_t[encoded_image_._size];
  encoded_image_buffer_.reset(encoded_image_._buffer);
  encoded_image_._completeFrame = true;
  encoded_image_._encodedWidth = 0;
  encoded_image_._encodedHeight = 0;
  encoded_image_._length = 0;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264EncoderImpl::Release() {
  if (openh264_encoder_) {
    RTC_CHECK_EQ(0, openh264_encoder_->Uninitialize());
    WelsDestroySVCEncoder(openh264_encoder_);
    openh264_encoder_ = nullptr;
  }
  encoded_image_._buffer = nullptr;
  encoded_image_buffer_.reset();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264EncoderImpl::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  encoded_image_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264EncoderImpl::SetRateAllocation(
    const BitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  if (bitrate_allocation.get_sum_bps() <= 0 || framerate <= 0)
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

  target_bps_ = bitrate_allocation.get_sum_bps();
  max_frame_rate_ = static_cast<float>(framerate);

  SBitrateInfo target_bitrate;
  memset(&target_bitrate, 0, sizeof(SBitrateInfo));
  target_bitrate.iLayer = SPATIAL_LAYER_ALL,
  target_bitrate.iBitrate = target_bps_;
  openh264_encoder_->SetOption(ENCODER_OPTION_BITRATE,
                               &target_bitrate);
  openh264_encoder_->SetOption(ENCODER_OPTION_FRAME_RATE, &max_frame_rate_);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264EncoderImpl::Encode(const VideoFrame& input_frame,
                                const CodecSpecificInfo* codec_specific_info,
                                const std::vector<FrameType>* frame_types) {
  if (!IsInitialized()) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (!encoded_image_callback_) {
    LOG(LS_WARNING) << "InitEncode() has been called, but a callback function "
                    << "has not been set with RegisterEncodeCompleteCallback()";
    ReportError();
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  bool force_key_frame = false;
  if (frame_types != nullptr) {
    // We only support a single stream.
    RTC_DCHECK_EQ(frame_types->size(), 1);
    // Skip frame?
    if ((*frame_types)[0] == kEmptyFrame) {
      return WEBRTC_VIDEO_CODEC_OK;
    }
    // Force key frame?
    force_key_frame = (*frame_types)[0] == kVideoFrameKey;
  }
  if (force_key_frame) {
    // API doc says ForceIntraFrame(false) does nothing, but calling this
    // function forces a key frame regardless of the |bIDR| argument's value.
    // (If every frame is a key frame we get lag/delays.)
    openh264_encoder_->ForceIntraFrame(true);
  }
  rtc::scoped_refptr<const I420BufferInterface> frame_buffer =
      input_frame.video_frame_buffer()->ToI420();
  // EncodeFrame input.
  SSourcePicture picture;
  memset(&picture, 0, sizeof(SSourcePicture));
  picture.iPicWidth = frame_buffer->width();
  picture.iPicHeight = frame_buffer->height();
  picture.iColorFormat = EVideoFormatType::videoFormatI420;
  picture.uiTimeStamp = input_frame.ntp_time_ms();
  picture.iStride[0] = frame_buffer->StrideY();
  picture.iStride[1] = frame_buffer->StrideU();
  picture.iStride[2] = frame_buffer->StrideV();
  picture.pData[0] = const_cast<uint8_t*>(frame_buffer->DataY());
  picture.pData[1] = const_cast<uint8_t*>(frame_buffer->DataU());
  picture.pData[2] = const_cast<uint8_t*>(frame_buffer->DataV());

  // EncodeFrame output.
  SFrameBSInfo info;
  memset(&info, 0, sizeof(SFrameBSInfo));

  // Encode!
  int enc_ret = openh264_encoder_->EncodeFrame(&picture, &info);
  if (enc_ret != 0) {
    LOG(LS_ERROR) << "OpenH264 frame encoding failed, EncodeFrame returned "
                  << enc_ret << ".";
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  encoded_image_._encodedWidth = frame_buffer->width();
  encoded_image_._encodedHeight = frame_buffer->height();
  encoded_image_._timeStamp = input_frame.timestamp();
  encoded_image_.ntp_time_ms_ = input_frame.ntp_time_ms();
  encoded_image_.capture_time_ms_ = input_frame.render_time_ms();
  encoded_image_.rotation_ = input_frame.rotation();
  encoded_image_.content_type_ = (mode_ == kScreensharing)
                                     ? VideoContentType::SCREENSHARE
                                     : VideoContentType::UNSPECIFIED;
  encoded_image_.timing_.is_timing_frame = false;
  encoded_image_._frameType = ConvertToVideoFrameType(info.eFrameType);

  // Split encoded image up into fragments. This also updates |encoded_image_|.
  RTPFragmentationHeader frag_header;
  RtpFragmentize(&encoded_image_, &encoded_image_buffer_, *frame_buffer, &info,
                 &frag_header);

  // Encoder can skip frames to save bandwidth in which case
  // |encoded_image_._length| == 0.
  if (encoded_image_._length > 0) {
    // Parse QP.
    h264_bitstream_parser_.ParseBitstream(encoded_image_._buffer,
                                          encoded_image_._length);
    h264_bitstream_parser_.GetLastSliceQp(&encoded_image_.qp_);

    // Deliver encoded image.
    CodecSpecificInfo codec_specific;
    codec_specific.codecType = kVideoCodecH264;
    codec_specific.codecSpecific.H264.packetization_mode = packetization_mode_;
    encoded_image_callback_->OnEncodedImage(encoded_image_, &codec_specific,
                                            &frag_header);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* H264EncoderImpl::ImplementationName() const {
  return "OpenH264";
}

bool H264EncoderImpl::IsInitialized() const {
  return openh264_encoder_ != nullptr;
}

// Initialization parameters.
// There are two ways to initialize. There is SEncParamBase (cleared with
// memset(&p, 0, sizeof(SEncParamBase)) used in Initialize, and SEncParamExt
// which is a superset of SEncParamBase (cleared with GetDefaultParams) used
// in InitializeExt.
SEncParamExt H264EncoderImpl::CreateEncoderParams() const {
  RTC_DCHECK(openh264_encoder_);
  SEncParamExt encoder_params;
  openh264_encoder_->GetDefaultParams(&encoder_params);
  if (mode_ == kRealtimeVideo) {
    encoder_params.iUsageType = CAMERA_VIDEO_REAL_TIME;
  } else if (mode_ == kScreensharing) {
    encoder_params.iUsageType = SCREEN_CONTENT_REAL_TIME;
  } else {
    RTC_NOTREACHED();
  }
  encoder_params.iPicWidth = width_;
  encoder_params.iPicHeight = height_;
  encoder_params.iTargetBitrate = target_bps_;
  encoder_params.iMaxBitrate = max_bps_;
  // Rate Control mode
  encoder_params.iRCMode = RC_BITRATE_MODE;
  encoder_params.fMaxFrameRate = max_frame_rate_;

  // The following parameters are extension parameters (they're in SEncParamExt,
  // not in SEncParamBase).
  encoder_params.bEnableFrameSkip = frame_dropping_on_;
  // |uiIntraPeriod|    - multiple of GOP size
  // |keyFrameInterval| - number of frames
  encoder_params.uiIntraPeriod = key_frame_interval_;
  encoder_params.uiMaxNalSize = 0;
  // Threading model: use auto.
  //  0: auto (dynamic imp. internal encoder)
  //  1: single thread (default value)
  // >1: number of threads
  encoder_params.iMultipleThreadIdc = NumberOfThreads(
      encoder_params.iPicWidth, encoder_params.iPicHeight, number_of_cores_);
  // The base spatial layer 0 is the only one we use.
  encoder_params.sSpatialLayers[0].iVideoWidth = encoder_params.iPicWidth;
  encoder_params.sSpatialLayers[0].iVideoHeight = encoder_params.iPicHeight;
  encoder_params.sSpatialLayers[0].fFrameRate = encoder_params.fMaxFrameRate;
  encoder_params.sSpatialLayers[0].iSpatialBitrate =
      encoder_params.iTargetBitrate;
  encoder_params.sSpatialLayers[0].iMaxSpatialBitrate =
      encoder_params.iMaxBitrate;
  LOG(INFO) << "OpenH264 version is " << OPENH264_MAJOR << "."
            << OPENH264_MINOR;
  switch (packetization_mode_) {
    case H264PacketizationMode::SingleNalUnit:
      // Limit the size of the packets produced.
      encoder_params.sSpatialLayers[0].sSliceArgument.uiSliceNum = 1;
      encoder_params.sSpatialLayers[0].sSliceArgument.uiSliceMode =
          SM_SIZELIMITED_SLICE;
      encoder_params.sSpatialLayers[0].sSliceArgument.uiSliceSizeConstraint =
          static_cast<unsigned int>(max_payload_size_);
      break;
    case H264PacketizationMode::NonInterleaved:
      // When uiSliceMode = SM_FIXEDSLCNUM_SLICE, uiSliceNum = 0 means auto
      // design it with cpu core number.
      // TODO(sprang): Set to 0 when we understand why the rate controller borks
      //               when uiSliceNum > 1.
      encoder_params.sSpatialLayers[0].sSliceArgument.uiSliceNum = 1;
      encoder_params.sSpatialLayers[0].sSliceArgument.uiSliceMode =
          SM_FIXEDSLCNUM_SLICE;
      break;
  }
  return encoder_params;
}

void H264EncoderImpl::ReportInit() {
  if (has_reported_init_)
    return;
  RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264EncoderImpl.Event",
                            kH264EncoderEventInit,
                            kH264EncoderEventMax);
  has_reported_init_ = true;
}

void H264EncoderImpl::ReportError() {
  if (has_reported_error_)
    return;
  RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264EncoderImpl.Event",
                            kH264EncoderEventError,
                            kH264EncoderEventMax);
  has_reported_error_ = true;
}

int32_t H264EncoderImpl::SetChannelParameters(
    uint32_t packet_loss, int64_t rtt) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264EncoderImpl::SetPeriodicKeyFrames(bool enable) {
  return WEBRTC_VIDEO_CODEC_OK;
}

VideoEncoder::ScalingSettings H264EncoderImpl::GetScalingSettings() const {
  return VideoEncoder::ScalingSettings(true);
}

}  // namespace webrtc
