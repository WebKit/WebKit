/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "modules/video_coding/codecs/vp9/vp9_impl.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "vpx/vp8cx.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vpx_encoder.h"

#include "absl/memory/memory.h"
#include "api/video/color_space.h"
#include "api/video/i010_buffer.h"
#include "common_video/include/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/codecs/vp9/svc_rate_allocator.h"
#include "rtc_base/checks.h"
#include "rtc_base/keep_ref_until_done.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {
// Maps from gof_idx to encoder internal reference frame buffer index. These
// maps work for 1,2 and 3 temporal layers with GOF length of 1,2 and 4 frames.
uint8_t kRefBufIdx[4] = {0, 0, 0, 1};
uint8_t kUpdBufIdx[4] = {0, 0, 1, 0};

// Only positive speeds, range for real-time coding currently is: 5 - 8.
// Lower means slower/better quality, higher means fastest/lower quality.
int GetCpuSpeed(int width, int height) {
#if defined(WEBRTC_ARCH_ARM) || defined(WEBRTC_ARCH_ARM64) || defined(ANDROID)
  return 8;
#else
  // For smaller resolutions, use lower speed setting (get some coding gain at
  // the cost of increased encoding complexity).
  if (width * height <= 352 * 288)
    return 5;
  else
    return 7;
#endif
}
// Helper class for extracting VP9 colorspace.
ColorSpace ExtractVP9ColorSpace(vpx_color_space_t space_t,
                                vpx_color_range_t range_t,
                                unsigned int bit_depth) {
  ColorSpace::PrimaryID primaries = ColorSpace::PrimaryID::kInvalid;
  ColorSpace::TransferID transfer = ColorSpace::TransferID::kInvalid;
  ColorSpace::MatrixID matrix = ColorSpace::MatrixID::kInvalid;
  switch (space_t) {
    case VPX_CS_BT_601:
    case VPX_CS_SMPTE_170:
      primaries = ColorSpace::PrimaryID::kSMPTE170M;
      transfer = ColorSpace::TransferID::kSMPTE170M;
      matrix = ColorSpace::MatrixID::kSMPTE170M;
      break;
    case VPX_CS_SMPTE_240:
      primaries = ColorSpace::PrimaryID::kSMPTE240M;
      transfer = ColorSpace::TransferID::kSMPTE240M;
      matrix = ColorSpace::MatrixID::kSMPTE240M;
      break;
    case VPX_CS_BT_709:
      primaries = ColorSpace::PrimaryID::kBT709;
      transfer = ColorSpace::TransferID::kBT709;
      matrix = ColorSpace::MatrixID::kBT709;
      break;
    case VPX_CS_BT_2020:
      primaries = ColorSpace::PrimaryID::kBT2020;
      switch (bit_depth) {
        case 8:
          transfer = ColorSpace::TransferID::kBT709;
          break;
        case 10:
          transfer = ColorSpace::TransferID::kBT2020_10;
          break;
        default:
          RTC_NOTREACHED();
          break;
      }
      matrix = ColorSpace::MatrixID::kBT2020_NCL;
      break;
    case VPX_CS_SRGB:
      primaries = ColorSpace::PrimaryID::kBT709;
      transfer = ColorSpace::TransferID::kIEC61966_2_1;
      matrix = ColorSpace::MatrixID::kBT709;
      break;
    default:
      break;
  }

  ColorSpace::RangeID range = ColorSpace::RangeID::kInvalid;
  switch (range_t) {
    case VPX_CR_STUDIO_RANGE:
      range = ColorSpace::RangeID::kLimited;
      break;
    case VPX_CR_FULL_RANGE:
      range = ColorSpace::RangeID::kFull;
      break;
    default:
      break;
  }
  return ColorSpace(primaries, transfer, matrix, range);
}
}  // namespace

std::vector<SdpVideoFormat> SupportedVP9Codecs() {
  // TODO(emircan): Add Profile 2 support after fixing browser_tests.
  std::vector<SdpVideoFormat> supported_formats{SdpVideoFormat(
      cricket::kVp9CodecName,
      {{kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile0)}})};
  return supported_formats;
}

std::unique_ptr<VP9Encoder> VP9Encoder::Create() {
  return absl::make_unique<VP9EncoderImpl>(cricket::VideoCodec());
}

std::unique_ptr<VP9Encoder> VP9Encoder::Create(
    const cricket::VideoCodec& codec) {
  return absl::make_unique<VP9EncoderImpl>(codec);
}

void VP9EncoderImpl::EncoderOutputCodedPacketCallback(vpx_codec_cx_pkt* pkt,
                                                      void* user_data) {
  VP9EncoderImpl* enc = static_cast<VP9EncoderImpl*>(user_data);
  enc->GetEncodedLayerFrame(pkt);
}

VP9EncoderImpl::VP9EncoderImpl(const cricket::VideoCodec& codec)
    : encoded_image_(),
      encoded_complete_callback_(nullptr),
      profile_(
          ParseSdpForVP9Profile(codec.params).value_or(VP9Profile::kProfile0)),
      inited_(false),
      timestamp_(0),
      cpu_speed_(3),
      rc_max_intra_target_(0),
      encoder_(nullptr),
      config_(nullptr),
      raw_(nullptr),
      input_image_(nullptr),
      force_key_frame_(true),
      pics_since_key_(0),
      num_temporal_layers_(0),
      num_spatial_layers_(0),
      num_active_spatial_layers_(0),
      layer_deactivation_requires_key_frame_(webrtc::field_trial::IsEnabled(
          "WebRTC-Vp9IssueKeyFrameOnLayerDeactivation")),
      is_svc_(false),
      inter_layer_pred_(InterLayerPredMode::kOn),
      external_ref_control_(
          webrtc::field_trial::IsEnabled("WebRTC-Vp9ExternalRefCtrl")),
      is_flexible_mode_(false) {
  memset(&codec_, 0, sizeof(codec_));
  memset(&svc_params_, 0, sizeof(vpx_svc_extra_cfg_t));
}

VP9EncoderImpl::~VP9EncoderImpl() {
  Release();
}

int VP9EncoderImpl::Release() {
  int ret_val = WEBRTC_VIDEO_CODEC_OK;

  if (encoded_image_._buffer != nullptr) {
    delete[] encoded_image_._buffer;
    encoded_image_._buffer = nullptr;
  }
  if (encoder_ != nullptr) {
    if (inited_) {
      if (vpx_codec_destroy(encoder_)) {
        ret_val = WEBRTC_VIDEO_CODEC_MEMORY;
      }
    }
    delete encoder_;
    encoder_ = nullptr;
  }
  if (config_ != nullptr) {
    delete config_;
    config_ = nullptr;
  }
  if (raw_ != nullptr) {
    vpx_img_free(raw_);
    raw_ = nullptr;
  }
  inited_ = false;
  return ret_val;
}

bool VP9EncoderImpl::ExplicitlyConfiguredSpatialLayers() const {
  // We check target_bitrate_bps of the 0th layer to see if the spatial layers
  // (i.e. bitrates) were explicitly configured.
  return codec_.spatialLayers[0].targetBitrate > 0;
}

bool VP9EncoderImpl::SetSvcRates(
    const VideoBitrateAllocation& bitrate_allocation) {
  config_->rc_target_bitrate = bitrate_allocation.get_sum_kbps();

  if (ExplicitlyConfiguredSpatialLayers()) {
    const bool layer_activation_requires_key_frame =
        inter_layer_pred_ == InterLayerPredMode::kOff ||
        inter_layer_pred_ == InterLayerPredMode::kOnKeyPic;

    for (size_t sl_idx = 0; sl_idx < num_spatial_layers_; ++sl_idx) {
      const bool was_layer_active = (config_->ss_target_bitrate[sl_idx] > 0);
      config_->ss_target_bitrate[sl_idx] =
          bitrate_allocation.GetSpatialLayerSum(sl_idx) / 1000;

      for (size_t tl_idx = 0; tl_idx < num_temporal_layers_; ++tl_idx) {
        config_->layer_target_bitrate[sl_idx * num_temporal_layers_ + tl_idx] =
            bitrate_allocation.GetTemporalLayerSum(sl_idx, tl_idx) / 1000;
      }

      const bool is_active_layer = (config_->ss_target_bitrate[sl_idx] > 0);
      if (!was_layer_active && is_active_layer &&
          layer_activation_requires_key_frame) {
        force_key_frame_ = true;
      } else if (was_layer_active && !is_active_layer &&
                 layer_deactivation_requires_key_frame_) {
        force_key_frame_ = true;
      }

      if (!was_layer_active) {
        // Reset frame rate controller if layer is resumed after pause.
        framerate_controller_[sl_idx].Reset();
      }

      framerate_controller_[sl_idx].SetTargetRate(
          std::min(static_cast<float>(codec_.maxFramerate),
                   codec_.spatialLayers[sl_idx].maxFramerate));
    }
  } else {
    float rate_ratio[VPX_MAX_LAYERS] = {0};
    float total = 0;
    for (int i = 0; i < num_spatial_layers_; ++i) {
      if (svc_params_.scaling_factor_num[i] <= 0 ||
          svc_params_.scaling_factor_den[i] <= 0) {
        RTC_LOG(LS_ERROR) << "Scaling factors not specified!";
        return false;
      }
      rate_ratio[i] = static_cast<float>(svc_params_.scaling_factor_num[i]) /
                      svc_params_.scaling_factor_den[i];
      total += rate_ratio[i];
    }

    for (int i = 0; i < num_spatial_layers_; ++i) {
      RTC_CHECK_GT(total, 0);
      config_->ss_target_bitrate[i] = static_cast<unsigned int>(
          config_->rc_target_bitrate * rate_ratio[i] / total);
      if (num_temporal_layers_ == 1) {
        config_->layer_target_bitrate[i] = config_->ss_target_bitrate[i];
      } else if (num_temporal_layers_ == 2) {
        config_->layer_target_bitrate[i * num_temporal_layers_] =
            config_->ss_target_bitrate[i] * 2 / 3;
        config_->layer_target_bitrate[i * num_temporal_layers_ + 1] =
            config_->ss_target_bitrate[i];
      } else if (num_temporal_layers_ == 3) {
        config_->layer_target_bitrate[i * num_temporal_layers_] =
            config_->ss_target_bitrate[i] / 2;
        config_->layer_target_bitrate[i * num_temporal_layers_ + 1] =
            config_->layer_target_bitrate[i * num_temporal_layers_] +
            (config_->ss_target_bitrate[i] / 4);
        config_->layer_target_bitrate[i * num_temporal_layers_ + 2] =
            config_->ss_target_bitrate[i];
      } else {
        RTC_LOG(LS_ERROR) << "Unsupported number of temporal layers: "
                          << num_temporal_layers_;
        return false;
      }

      framerate_controller_[i].SetTargetRate(codec_.maxFramerate);
    }
  }

  num_active_spatial_layers_ = 0;
  for (int i = 0; i < num_spatial_layers_; ++i) {
    if (config_->ss_target_bitrate[i] > 0) {
      ++num_active_spatial_layers_;
    }
  }
  RTC_DCHECK_GT(num_active_spatial_layers_, 0);

  return true;
}

int VP9EncoderImpl::SetRateAllocation(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t frame_rate) {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (encoder_->err) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (frame_rate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // Update bit rate
  if (codec_.maxBitrate > 0 &&
      bitrate_allocation.get_sum_kbps() > codec_.maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  codec_.maxFramerate = frame_rate;

  if (!SetSvcRates(bitrate_allocation)) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // Update encoder context
  if (vpx_codec_enc_config_set(encoder_, config_)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP9EncoderImpl::InitEncode(const VideoCodec* inst,
                               int number_of_cores,
                               size_t /*max_payload_size*/) {
  if (inst == nullptr) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->maxFramerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // Allow zero to represent an unspecified maxBitRate
  if (inst->maxBitrate > 0 && inst->startBitrate > inst->maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->width < 1 || inst->height < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (number_of_cores < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->VP9().numberOfTemporalLayers > 3) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // libvpx probably does not support more than 3 spatial layers.
  if (inst->VP9().numberOfSpatialLayers > 3) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  int ret_val = Release();
  if (ret_val < 0) {
    return ret_val;
  }
  if (encoder_ == nullptr) {
    encoder_ = new vpx_codec_ctx_t;
  }
  if (config_ == nullptr) {
    config_ = new vpx_codec_enc_cfg_t;
  }
  timestamp_ = 0;
  if (&codec_ != inst) {
    codec_ = *inst;
  }

  force_key_frame_ = true;
  pics_since_key_ = 0;

  num_spatial_layers_ = inst->VP9().numberOfSpatialLayers;
  RTC_DCHECK_GT(num_spatial_layers_, 0);
  num_temporal_layers_ = inst->VP9().numberOfTemporalLayers;
  if (num_temporal_layers_ == 0) {
    num_temporal_layers_ = 1;
  }

  framerate_controller_ = std::vector<FramerateController>(
      num_spatial_layers_, FramerateController(codec_.maxFramerate));

  is_svc_ = (num_spatial_layers_ > 1 || num_temporal_layers_ > 1);

  // Allocate memory for encoded image
  if (encoded_image_._buffer != nullptr) {
    delete[] encoded_image_._buffer;
  }
  encoded_image_._size =
      CalcBufferSize(VideoType::kI420, codec_.width, codec_.height);
  encoded_image_._buffer = new uint8_t[encoded_image_._size];
  encoded_image_._completeFrame = true;
  // Populate encoder configuration with default values.
  if (vpx_codec_enc_config_default(vpx_codec_vp9_cx(), config_, 0)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  vpx_img_fmt img_fmt = VPX_IMG_FMT_NONE;
  unsigned int bits_for_storage = 8;
  switch (profile_) {
    case VP9Profile::kProfile0:
      img_fmt = VPX_IMG_FMT_I420;
      bits_for_storage = 8;
      config_->g_bit_depth = VPX_BITS_8;
      config_->g_profile = 0;
      config_->g_input_bit_depth = 8;
      break;
    case VP9Profile::kProfile2:
      img_fmt = VPX_IMG_FMT_I42016;
      bits_for_storage = 16;
      config_->g_bit_depth = VPX_BITS_10;
      config_->g_profile = 2;
      config_->g_input_bit_depth = 10;
      break;
  }

  // Creating a wrapper to the image - setting image data to nullptr. Actual
  // pointer will be set in encode. Setting align to 1, as it is meaningless
  // (actual memory is not allocated).
  raw_ =
      vpx_img_wrap(nullptr, img_fmt, codec_.width, codec_.height, 1, nullptr);
  raw_->bit_depth = bits_for_storage;

  config_->g_w = codec_.width;
  config_->g_h = codec_.height;
  config_->rc_target_bitrate = inst->startBitrate;  // in kbit/s
  config_->g_error_resilient = is_svc_ ? VPX_ERROR_RESILIENT_DEFAULT : 0;
  // Setting the time base of the codec.
  config_->g_timebase.num = 1;
  config_->g_timebase.den = 90000;
  config_->g_lag_in_frames = 0;  // 0- no frame lagging
  config_->g_threads = 1;
  // Rate control settings.
  config_->rc_dropframe_thresh = inst->VP9().frameDroppingOn ? 30 : 0;
  config_->rc_end_usage = VPX_CBR;
  config_->g_pass = VPX_RC_ONE_PASS;
  config_->rc_min_quantizer = 2;
  config_->rc_max_quantizer = 52;
  config_->rc_undershoot_pct = 50;
  config_->rc_overshoot_pct = 50;
  config_->rc_buf_initial_sz = 500;
  config_->rc_buf_optimal_sz = 600;
  config_->rc_buf_sz = 1000;
  // Set the maximum target size of any key-frame.
  rc_max_intra_target_ = MaxIntraTarget(config_->rc_buf_optimal_sz);
  if (inst->VP9().keyFrameInterval > 0) {
    config_->kf_mode = VPX_KF_AUTO;
    config_->kf_max_dist = inst->VP9().keyFrameInterval;
    // Needs to be set (in svc mode) to get correct periodic key frame interval
    // (will have no effect in non-svc).
    config_->kf_min_dist = config_->kf_max_dist;
  } else {
    config_->kf_mode = VPX_KF_DISABLED;
  }
  config_->rc_resize_allowed = inst->VP9().automaticResizeOn ? 1 : 0;
  // Determine number of threads based on the image size and #cores.
  config_->g_threads =
      NumberOfThreads(config_->g_w, config_->g_h, number_of_cores);

  cpu_speed_ = GetCpuSpeed(config_->g_w, config_->g_h);

  is_flexible_mode_ = inst->VP9().flexibleMode;

  if (num_temporal_layers_ == 1) {
    gof_.SetGofInfoVP9(kTemporalStructureMode1);
    config_->temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_NOLAYERING;
    config_->ts_number_layers = 1;
    config_->ts_rate_decimator[0] = 1;
    config_->ts_periodicity = 1;
    config_->ts_layer_id[0] = 0;
  } else if (num_temporal_layers_ == 2) {
    gof_.SetGofInfoVP9(kTemporalStructureMode2);
    config_->temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_0101;
    config_->ts_number_layers = 2;
    config_->ts_rate_decimator[0] = 2;
    config_->ts_rate_decimator[1] = 1;
    config_->ts_periodicity = 2;
    config_->ts_layer_id[0] = 0;
    config_->ts_layer_id[1] = 1;
  } else if (num_temporal_layers_ == 3) {
    gof_.SetGofInfoVP9(kTemporalStructureMode3);
    config_->temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_0212;
    config_->ts_number_layers = 3;
    config_->ts_rate_decimator[0] = 4;
    config_->ts_rate_decimator[1] = 2;
    config_->ts_rate_decimator[2] = 1;
    config_->ts_periodicity = 4;
    config_->ts_layer_id[0] = 0;
    config_->ts_layer_id[1] = 2;
    config_->ts_layer_id[2] = 1;
    config_->ts_layer_id[3] = 2;
  } else {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  inter_layer_pred_ = inst->VP9().interLayerPred;

  ref_buf_.clear();

  return InitAndSetControlSettings(inst);
}

int VP9EncoderImpl::NumberOfThreads(int width,
                                    int height,
                                    int number_of_cores) {
  // Keep the number of encoder threads equal to the possible number of column
  // tiles, which is (1, 2, 4, 8). See comments below for VP9E_SET_TILE_COLUMNS.
  if (width * height >= 1280 * 720 && number_of_cores > 4) {
    return 4;
  } else if (width * height >= 640 * 360 && number_of_cores > 2) {
    return 2;
  } else {
// Use 2 threads for low res on ARM.
#if defined(WEBRTC_ARCH_ARM) || defined(WEBRTC_ARCH_ARM64) || \
    defined(WEBRTC_ANDROID)
    if (width * height >= 320 * 180 && number_of_cores > 2) {
      return 2;
    }
#endif
    // 1 thread less than VGA.
    return 1;
  }
}

int VP9EncoderImpl::InitAndSetControlSettings(const VideoCodec* inst) {
  // Set QP-min/max per spatial and temporal layer.
  int tot_num_layers = num_spatial_layers_ * num_temporal_layers_;
  for (int i = 0; i < tot_num_layers; ++i) {
    svc_params_.max_quantizers[i] = config_->rc_max_quantizer;
    svc_params_.min_quantizers[i] = config_->rc_min_quantizer;
  }
  config_->ss_number_layers = num_spatial_layers_;
  if (ExplicitlyConfiguredSpatialLayers()) {
    for (int i = 0; i < num_spatial_layers_; ++i) {
      const auto& layer = codec_.spatialLayers[i];
      RTC_CHECK_GT(layer.width, 0);
      const int scale_factor = codec_.width / layer.width;
      RTC_DCHECK_GT(scale_factor, 0);

      // Ensure scaler factor is integer.
      if (scale_factor * layer.width != codec_.width) {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
      }

      // Ensure scale factor is the same in both dimensions.
      if (scale_factor * layer.height != codec_.height) {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
      }

      // Ensure scale factor is power of two.
      const bool is_pow_of_two = (scale_factor & (scale_factor - 1)) == 0;
      if (!is_pow_of_two) {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
      }

      svc_params_.scaling_factor_num[i] = 1;
      svc_params_.scaling_factor_den[i] = scale_factor;

      RTC_DCHECK_GT(codec_.spatialLayers[i].maxFramerate, 0);
      RTC_DCHECK_LE(codec_.spatialLayers[i].maxFramerate, codec_.maxFramerate);
      if (i > 0) {
        // Frame rate of high spatial layer is supposed to be equal or higher
        // than frame rate of low spatial layer.
        RTC_DCHECK_GE(codec_.spatialLayers[i].maxFramerate,
                      codec_.spatialLayers[i - 1].maxFramerate);
      }
    }
  } else {
    int scaling_factor_num = 256;
    for (int i = num_spatial_layers_ - 1; i >= 0; --i) {
      // 1:2 scaling in each dimension.
      svc_params_.scaling_factor_num[i] = scaling_factor_num;
      svc_params_.scaling_factor_den[i] = 256;
    }
  }

  SvcRateAllocator init_allocator(codec_);
  VideoBitrateAllocation allocation = init_allocator.GetAllocation(
      inst->startBitrate * 1000, inst->maxFramerate);
  if (!SetSvcRates(allocation)) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  const vpx_codec_err_t rv = vpx_codec_enc_init(
      encoder_, vpx_codec_vp9_cx(), config_,
      config_->g_bit_depth == VPX_BITS_8 ? 0 : VPX_CODEC_USE_HIGHBITDEPTH);
  if (rv != VPX_CODEC_OK) {
    RTC_LOG(LS_ERROR) << "Init error: " << vpx_codec_err_to_string(rv);
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  vpx_codec_control(encoder_, VP8E_SET_CPUUSED, cpu_speed_);
  vpx_codec_control(encoder_, VP8E_SET_MAX_INTRA_BITRATE_PCT,
                    rc_max_intra_target_);
  vpx_codec_control(encoder_, VP9E_SET_AQ_MODE,
                    inst->VP9().adaptiveQpMode ? 3 : 0);

  vpx_codec_control(encoder_, VP9E_SET_FRAME_PARALLEL_DECODING, 0);

  if (is_svc_) {
    vpx_codec_control(encoder_, VP9E_SET_SVC, 1);
    vpx_codec_control(encoder_, VP9E_SET_SVC_PARAMETERS, &svc_params_);
  }

  if (num_spatial_layers_ > 1) {
    switch (inter_layer_pred_) {
      case InterLayerPredMode::kOn:
        vpx_codec_control(encoder_, VP9E_SET_SVC_INTER_LAYER_PRED, 0);
        break;
      case InterLayerPredMode::kOff:
        vpx_codec_control(encoder_, VP9E_SET_SVC_INTER_LAYER_PRED, 1);
        break;
      case InterLayerPredMode::kOnKeyPic:
        vpx_codec_control(encoder_, VP9E_SET_SVC_INTER_LAYER_PRED, 2);
        break;
      default:
        RTC_NOTREACHED();
    }

    // Configure encoder to drop entire superframe whenever it needs to drop
    // a layer. This mode is prefered over per-layer dropping which causes
    // quality flickering and is not compatible with RTP non-flexible mode.
    vpx_svc_frame_drop_t svc_drop_frame;
    memset(&svc_drop_frame, 0, sizeof(svc_drop_frame));
    svc_drop_frame.framedrop_mode = FULL_SUPERFRAME_DROP;
    svc_drop_frame.max_consec_drop = std::numeric_limits<int>::max();
    for (size_t i = 0; i < num_spatial_layers_; ++i) {
      svc_drop_frame.framedrop_thresh[i] = config_->rc_dropframe_thresh;
    }
    vpx_codec_control(encoder_, VP9E_SET_SVC_FRAME_DROP_LAYER, &svc_drop_frame);
  }

  // Register callback for getting each spatial layer.
  vpx_codec_priv_output_cx_pkt_cb_pair_t cbp = {
      VP9EncoderImpl::EncoderOutputCodedPacketCallback,
      reinterpret_cast<void*>(this)};
  vpx_codec_control(encoder_, VP9E_REGISTER_CX_CALLBACK,
                    reinterpret_cast<void*>(&cbp));

  // Control function to set the number of column tiles in encoding a frame, in
  // log2 unit: e.g., 0 = 1 tile column, 1 = 2 tile columns, 2 = 4 tile columns.
  // The number tile columns will be capped by the encoder based on image size
  // (minimum width of tile column is 256 pixels, maximum is 4096).
  vpx_codec_control(encoder_, VP9E_SET_TILE_COLUMNS, (config_->g_threads >> 1));

  // Turn on row-based multithreading.
  vpx_codec_control(encoder_, VP9E_SET_ROW_MT, 1);

#if !defined(WEBRTC_ARCH_ARM) && !defined(WEBRTC_ARCH_ARM64) && \
    !defined(ANDROID)
  // Do not enable the denoiser on ARM since optimization is pending.
  // Denoiser is on by default on other platforms.
  vpx_codec_control(encoder_, VP9E_SET_NOISE_SENSITIVITY,
                    inst->VP9().denoisingOn ? 1 : 0);
#endif

  if (codec_.mode == VideoCodecMode::kScreensharing) {
    // Adjust internal parameters to screen content.
    vpx_codec_control(encoder_, VP9E_SET_TUNE_CONTENT, 1);
  }
  // Enable encoder skip of static/low content blocks.
  vpx_codec_control(encoder_, VP8E_SET_STATIC_THRESHOLD, 1);
  inited_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

uint32_t VP9EncoderImpl::MaxIntraTarget(uint32_t optimal_buffer_size) {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scale_par.
  // Max target size = scale_par * optimal_buffer_size * targetBR[Kbps].
  // This value is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / framerate.
  // The target in % is as follows:
  float scale_par = 0.5;
  uint32_t target_pct =
      optimal_buffer_size * scale_par * codec_.maxFramerate / 10;
  // Don't go below 3 times the per frame bandwidth.
  const uint32_t min_intra_size = 300;
  return (target_pct < min_intra_size) ? min_intra_size : target_pct;
}

int VP9EncoderImpl::Encode(const VideoFrame& input_image,
                           const CodecSpecificInfo* codec_specific_info,
                           const std::vector<FrameType>* frame_types) {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (encoded_complete_callback_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // We only support one stream at the moment.
  if (frame_types && !frame_types->empty()) {
    if ((*frame_types)[0] == kVideoFrameKey) {
      force_key_frame_ = true;
    }
  }

  if (VideoCodecMode::kScreensharing == codec_.mode && !force_key_frame_) {
    // Skip encoding spatial layer frames if their target frame rate is lower
    // than actual input frame rate.
    vpx_svc_layer_id_t layer_id = {0};
    const size_t gof_idx = (pics_since_key_ + 1) % gof_.num_frames_in_gof;
    layer_id.temporal_layer_id = gof_.temporal_idx[gof_idx];

    const uint32_t frame_timestamp_ms =
        1000 * input_image.timestamp() / kVideoPayloadTypeFrequency;

    for (uint8_t sl_idx = 0; sl_idx < num_active_spatial_layers_; ++sl_idx) {
      if (framerate_controller_[sl_idx].DropFrame(frame_timestamp_ms)) {
        ++layer_id.spatial_layer_id;
      } else {
        break;
      }
    }

    RTC_DCHECK_LE(layer_id.spatial_layer_id, num_active_spatial_layers_);
    if (layer_id.spatial_layer_id >= num_active_spatial_layers_) {
      // Drop entire picture.
      return WEBRTC_VIDEO_CODEC_OK;
    }

    vpx_codec_control(encoder_, VP9E_SET_SVC_LAYER_ID, &layer_id);
  }

  RTC_DCHECK_EQ(input_image.width(), raw_->d_w);
  RTC_DCHECK_EQ(input_image.height(), raw_->d_h);

  // Set input image for use in the callback.
  // This was necessary since you need some information from input_image.
  // You can save only the necessary information (such as timestamp) instead of
  // doing this.
  input_image_ = &input_image;

  // Keep reference to buffer until encode completes.
  rtc::scoped_refptr<I420BufferInterface> i420_buffer;
  rtc::scoped_refptr<I010BufferInterface> i010_buffer;
  switch (profile_) {
    case VP9Profile::kProfile0: {
      i420_buffer = input_image.video_frame_buffer()->ToI420();
      // Image in vpx_image_t format.
      // Input image is const. VPX's raw image is not defined as const.
      raw_->planes[VPX_PLANE_Y] = const_cast<uint8_t*>(i420_buffer->DataY());
      raw_->planes[VPX_PLANE_U] = const_cast<uint8_t*>(i420_buffer->DataU());
      raw_->planes[VPX_PLANE_V] = const_cast<uint8_t*>(i420_buffer->DataV());
      raw_->stride[VPX_PLANE_Y] = i420_buffer->StrideY();
      raw_->stride[VPX_PLANE_U] = i420_buffer->StrideU();
      raw_->stride[VPX_PLANE_V] = i420_buffer->StrideV();
      break;
    }
    case VP9Profile::kProfile2: {
      // We can inject kI010 frames directly for encode. All other formats
      // should be converted to it.
      switch (input_image.video_frame_buffer()->type()) {
        case VideoFrameBuffer::Type::kI010: {
          i010_buffer = input_image.video_frame_buffer()->GetI010();
          break;
        }
        default: {
          i010_buffer =
              I010Buffer::Copy(*input_image.video_frame_buffer()->ToI420());
        }
      }
      raw_->planes[VPX_PLANE_Y] = const_cast<uint8_t*>(
          reinterpret_cast<const uint8_t*>(i010_buffer->DataY()));
      raw_->planes[VPX_PLANE_U] = const_cast<uint8_t*>(
          reinterpret_cast<const uint8_t*>(i010_buffer->DataU()));
      raw_->planes[VPX_PLANE_V] = const_cast<uint8_t*>(
          reinterpret_cast<const uint8_t*>(i010_buffer->DataV()));
      raw_->stride[VPX_PLANE_Y] = i010_buffer->StrideY() * 2;
      raw_->stride[VPX_PLANE_U] = i010_buffer->StrideU() * 2;
      raw_->stride[VPX_PLANE_V] = i010_buffer->StrideV() * 2;
      break;
    }
  }

  vpx_enc_frame_flags_t flags = 0;
  if (force_key_frame_) {
    flags = VPX_EFLAG_FORCE_KF;
  }

  if (external_ref_control_) {
    vpx_svc_ref_frame_config_t ref_config = SetReferences(force_key_frame_);
    vpx_codec_control(encoder_, VP9E_SET_SVC_REF_FRAME_CONFIG, &ref_config);
  }

  // TODO(ssilkin): Frame duration should be specified per spatial layer
  // since their frame rate can be different. For now calculate frame duration
  // based on target frame rate of the highest spatial layer, which frame rate
  // is supposed to be equal or higher than frame rate of low spatial layers.
  // Also, timestamp should represent actual time passed since previous frame
  // (not 'expected' time). Then rate controller can drain buffer more
  // accurately.
  RTC_DCHECK_GE(framerate_controller_.size(), num_active_spatial_layers_);
  float target_framerate_fps =
      (codec_.mode == VideoCodecMode::kScreensharing)
          ? framerate_controller_[num_active_spatial_layers_ - 1]
                .GetTargetRate()
          : codec_.maxFramerate;
  uint32_t duration = static_cast<uint32_t>(90000 / target_framerate_fps);
  const vpx_codec_err_t rv = vpx_codec_encode(encoder_, raw_, timestamp_,
                                              duration, flags, VPX_DL_REALTIME);
  if (rv != VPX_CODEC_OK) {
    RTC_LOG(LS_ERROR) << "Encoding error: " << vpx_codec_err_to_string(rv)
                      << "\n"
                      << "Details: " << vpx_codec_error(encoder_) << "\n"
                      << vpx_codec_error_detail(encoder_);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  timestamp_ += duration;

  const bool end_of_picture = true;
  DeliverBufferedFrame(end_of_picture);

  return WEBRTC_VIDEO_CODEC_OK;
}

void VP9EncoderImpl::PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                                           absl::optional<int>* spatial_idx,
                                           const vpx_codec_cx_pkt& pkt,
                                           uint32_t timestamp,
                                           bool first_frame_in_picture) {
  RTC_CHECK(codec_specific != nullptr);
  codec_specific->codecType = kVideoCodecVP9;
  codec_specific->codec_name = ImplementationName();
  CodecSpecificInfoVP9* vp9_info = &(codec_specific->codecSpecific.VP9);

  vp9_info->first_frame_in_picture = first_frame_in_picture;
  vp9_info->flexible_mode = is_flexible_mode_;
  vp9_info->ss_data_available =
      (pkt.data.frame.flags & VPX_FRAME_IS_KEY) ? true : false;

  vpx_svc_layer_id_t layer_id = {0};
  vpx_codec_control(encoder_, VP9E_GET_SVC_LAYER_ID, &layer_id);

  RTC_CHECK_GT(num_temporal_layers_, 0);
  RTC_CHECK_GT(num_active_spatial_layers_, 0);
  if (num_temporal_layers_ == 1) {
    RTC_CHECK_EQ(layer_id.temporal_layer_id, 0);
    vp9_info->temporal_idx = kNoTemporalIdx;
  } else {
    vp9_info->temporal_idx = layer_id.temporal_layer_id;
  }
  if (num_active_spatial_layers_ == 1) {
    RTC_CHECK_EQ(layer_id.spatial_layer_id, 0);
    *spatial_idx = absl::nullopt;
  } else {
    *spatial_idx = layer_id.spatial_layer_id;
  }
  if (layer_id.spatial_layer_id != 0) {
    vp9_info->ss_data_available = false;
  }

  // TODO(asapersson): this info has to be obtained from the encoder.
  vp9_info->temporal_up_switch = false;

  if (pkt.data.frame.flags & VPX_FRAME_IS_KEY) {
    pics_since_key_ = 0;
  } else if (first_frame_in_picture) {
    ++pics_since_key_;
  }

  const bool is_key_pic = (pics_since_key_ == 0);
  const bool is_inter_layer_pred_allowed =
      (inter_layer_pred_ == InterLayerPredMode::kOn ||
       (inter_layer_pred_ == InterLayerPredMode::kOnKeyPic && is_key_pic));

  // Always set inter_layer_predicted to true on high layer frame if inter-layer
  // prediction (ILP) is allowed even if encoder didn't actually use it.
  // Setting inter_layer_predicted to false would allow receiver to decode high
  // layer frame without decoding low layer frame. If that would happen (e.g.
  // if low layer frame is lost) then receiver won't be able to decode next high
  // layer frame which uses ILP.
  vp9_info->inter_layer_predicted =
      first_frame_in_picture ? false : is_inter_layer_pred_allowed;

  // Mark all low spatial layer frames as references (not just frames of
  // active low spatial layers) if inter-layer prediction is enabled since
  // these frames are indirect references of high spatial layer, which can
  // later be enabled without key frame.
  vp9_info->non_ref_for_inter_layer_pred =
      !is_inter_layer_pred_allowed ||
      layer_id.spatial_layer_id + 1 == num_spatial_layers_;

  // Always populate this, so that the packetizer can properly set the marker
  // bit.
  vp9_info->num_spatial_layers = num_active_spatial_layers_;

  vp9_info->num_ref_pics = 0;
  if (vp9_info->flexible_mode) {
    vp9_info->gof_idx = kNoGofIdx;
    FillReferenceIndices(pkt, pics_since_key_, vp9_info->inter_layer_predicted,
                         vp9_info);
    // TODO(webrtc:9794): Add fake reference to empty reference list to
    // workaround the frame buffer issue on receiver.
  } else {
    vp9_info->gof_idx =
        static_cast<uint8_t>(pics_since_key_ % gof_.num_frames_in_gof);
    vp9_info->temporal_up_switch = gof_.temporal_up_switch[vp9_info->gof_idx];
    vp9_info->num_ref_pics = gof_.num_ref_pics[vp9_info->gof_idx];
  }

  vp9_info->inter_pic_predicted = (!is_key_pic && vp9_info->num_ref_pics > 0);

  if (vp9_info->ss_data_available) {
    vp9_info->spatial_layer_resolution_present = true;
    for (size_t i = 0; i < num_active_spatial_layers_; ++i) {
      vp9_info->width[i] = codec_.width * svc_params_.scaling_factor_num[i] /
                           svc_params_.scaling_factor_den[i];
      vp9_info->height[i] = codec_.height * svc_params_.scaling_factor_num[i] /
                            svc_params_.scaling_factor_den[i];
    }
    if (vp9_info->flexible_mode) {
      vp9_info->gof.num_frames_in_gof = 0;
    } else {
      vp9_info->gof.CopyGofInfoVP9(gof_);
    }
  }
}

void VP9EncoderImpl::FillReferenceIndices(const vpx_codec_cx_pkt& pkt,
                                          const size_t pic_num,
                                          const bool inter_layer_predicted,
                                          CodecSpecificInfoVP9* vp9_info) {
  vpx_svc_layer_id_t layer_id = {0};
  vpx_codec_control(encoder_, VP9E_GET_SVC_LAYER_ID, &layer_id);

  const bool is_key_frame =
      (pkt.data.frame.flags & VPX_FRAME_IS_KEY) ? true : false;

  std::vector<RefFrameBuffer> ref_buf_list;

  if (is_svc_) {
    vpx_svc_ref_frame_config_t enc_layer_conf = {{0}};
    vpx_codec_control(encoder_, VP9E_GET_SVC_REF_FRAME_CONFIG, &enc_layer_conf);

    if (enc_layer_conf.reference_last[layer_id.spatial_layer_id]) {
      const size_t fb_idx =
          enc_layer_conf.lst_fb_idx[layer_id.spatial_layer_id];
      RTC_DCHECK(ref_buf_.find(fb_idx) != ref_buf_.end());
      if (std::find(ref_buf_list.begin(), ref_buf_list.end(),
                    ref_buf_.at(fb_idx)) == ref_buf_list.end()) {
        ref_buf_list.push_back(ref_buf_.at(fb_idx));
      }
    }

    if (enc_layer_conf.reference_alt_ref[layer_id.spatial_layer_id]) {
      const size_t fb_idx =
          enc_layer_conf.alt_fb_idx[layer_id.spatial_layer_id];
      RTC_DCHECK(ref_buf_.find(fb_idx) != ref_buf_.end());
      if (std::find(ref_buf_list.begin(), ref_buf_list.end(),
                    ref_buf_.at(fb_idx)) == ref_buf_list.end()) {
        ref_buf_list.push_back(ref_buf_.at(fb_idx));
      }
    }

    if (enc_layer_conf.reference_golden[layer_id.spatial_layer_id]) {
      const size_t fb_idx =
          enc_layer_conf.gld_fb_idx[layer_id.spatial_layer_id];
      RTC_DCHECK(ref_buf_.find(fb_idx) != ref_buf_.end());
      if (std::find(ref_buf_list.begin(), ref_buf_list.end(),
                    ref_buf_.at(fb_idx)) == ref_buf_list.end()) {
        ref_buf_list.push_back(ref_buf_.at(fb_idx));
      }
    }
  } else if (!is_key_frame) {
    RTC_DCHECK_EQ(num_spatial_layers_, 1);
    RTC_DCHECK_EQ(num_temporal_layers_, 1);
    // In non-SVC mode encoder doesn't provide reference list. Assume each frame
    // refers previous one, which is stored in buffer 0.
    ref_buf_list.push_back(ref_buf_.at(0));
  }

  size_t max_ref_temporal_layer_id = 0;

  vp9_info->num_ref_pics = 0;
  for (const RefFrameBuffer& ref_buf : ref_buf_list) {
    RTC_DCHECK_LE(ref_buf.pic_num, pic_num);
    if (ref_buf.pic_num < pic_num) {
      if (inter_layer_pred_ != InterLayerPredMode::kOn) {
        // RTP spec limits temporal prediction to the same spatial layer.
        // It is safe to ignore this requirement if inter-layer prediction is
        // enabled for all frames when all base frames are relayed to receiver.
        RTC_DCHECK_EQ(ref_buf.spatial_layer_id, layer_id.spatial_layer_id);
      }
      RTC_DCHECK_LE(ref_buf.temporal_layer_id, layer_id.temporal_layer_id);

      const size_t p_diff = pic_num - ref_buf.pic_num;
      RTC_DCHECK_LE(p_diff, 127UL);

      vp9_info->p_diff[vp9_info->num_ref_pics] = static_cast<uint8_t>(p_diff);
      ++vp9_info->num_ref_pics;

      max_ref_temporal_layer_id =
          std::max(max_ref_temporal_layer_id, ref_buf.temporal_layer_id);
    } else {
      RTC_DCHECK(inter_layer_predicted);
      // RTP spec only allows to use previous spatial layer for inter-layer
      // prediction.
      RTC_DCHECK_EQ(ref_buf.spatial_layer_id + 1, layer_id.spatial_layer_id);
    }
  }

  vp9_info->temporal_up_switch =
      (max_ref_temporal_layer_id <
       static_cast<size_t>(layer_id.temporal_layer_id));
}

void VP9EncoderImpl::UpdateReferenceBuffers(const vpx_codec_cx_pkt& pkt,
                                            const size_t pic_num) {
  vpx_svc_layer_id_t layer_id = {0};
  vpx_codec_control(encoder_, VP9E_GET_SVC_LAYER_ID, &layer_id);

  const bool is_key_frame =
      (pkt.data.frame.flags & VPX_FRAME_IS_KEY) ? true : false;

  RefFrameBuffer frame_buf(pic_num, layer_id.spatial_layer_id,
                           layer_id.temporal_layer_id);

  if (is_key_frame && layer_id.spatial_layer_id == 0) {
    // Key frame updates all ref buffers.
    for (size_t i = 0; i < kNumVp9Buffers; ++i) {
      ref_buf_[i] = frame_buf;
    }
  } else if (is_svc_) {
    vpx_svc_ref_frame_config_t enc_layer_conf = {{0}};
    vpx_codec_control(encoder_, VP9E_GET_SVC_REF_FRAME_CONFIG, &enc_layer_conf);

    if (enc_layer_conf.update_last[layer_id.spatial_layer_id]) {
      ref_buf_[enc_layer_conf.lst_fb_idx[layer_id.spatial_layer_id]] =
          frame_buf;
    }

    if (enc_layer_conf.update_alt_ref[layer_id.spatial_layer_id]) {
      ref_buf_[enc_layer_conf.alt_fb_idx[layer_id.spatial_layer_id]] =
          frame_buf;
    }

    if (enc_layer_conf.update_golden[layer_id.spatial_layer_id]) {
      ref_buf_[enc_layer_conf.gld_fb_idx[layer_id.spatial_layer_id]] =
          frame_buf;
    }
  } else {
    RTC_DCHECK_EQ(num_spatial_layers_, 1);
    RTC_DCHECK_EQ(num_temporal_layers_, 1);
    // In non-svc mode encoder doesn't provide reference list. Assume each frame
    // is reference and stored in buffer 0.
    ref_buf_[0] = frame_buf;
  }
}

vpx_svc_ref_frame_config_t VP9EncoderImpl::SetReferences(bool is_key_pic) {
  // kRefBufIdx, kUpdBufIdx need to be updated to support longer GOFs.
  RTC_DCHECK_LE(gof_.num_frames_in_gof, 4);

  vpx_svc_ref_frame_config_t ref_config;
  memset(&ref_config, 0, sizeof(ref_config));

  const size_t num_temporal_refs = std::max(1, num_temporal_layers_ - 1);
  const bool is_inter_layer_pred_allowed =
      inter_layer_pred_ == InterLayerPredMode::kOn ||
      (inter_layer_pred_ == InterLayerPredMode::kOnKeyPic && is_key_pic);
  absl::optional<int> last_updated_buf_idx;

  // Put temporal reference to LAST and spatial reference to GOLDEN. Update
  // frame buffer (i.e. store encoded frame) if current frame is a temporal
  // reference (i.e. it belongs to a low temporal layer) or it is a spatial
  // reference. In later case, always store spatial reference in the last
  // reference frame buffer.
  // For the case of 3 temporal and 3 spatial layers we need 6 frame buffers
  // for temporal references plus 1 buffer for spatial reference. 7 buffers
  // in total.

  for (size_t sl_idx = 0; sl_idx < num_active_spatial_layers_; ++sl_idx) {
    const size_t gof_idx = pics_since_key_ % gof_.num_frames_in_gof;

    if (!is_key_pic) {
      // Set up temporal reference.
      const int buf_idx = sl_idx * num_temporal_refs + kRefBufIdx[gof_idx];

      // Last reference frame buffer is reserved for spatial reference. It is
      // not supposed to be used for temporal prediction.
      RTC_DCHECK_LT(buf_idx, kNumVp9Buffers - 1);

      // Sanity check that reference picture number is smaller than current
      // picture number.
      const size_t curr_pic_num = pics_since_key_ + 1;
      RTC_DCHECK_LT(ref_buf_[buf_idx].pic_num, curr_pic_num);
      const size_t pid_diff = curr_pic_num - ref_buf_[buf_idx].pic_num;

      // Below code assumes single temporal referecence.
      RTC_DCHECK_EQ(gof_.num_ref_pics[gof_idx], 1);
      if (pid_diff == gof_.pid_diff[gof_idx][0]) {
        ref_config.lst_fb_idx[sl_idx] = buf_idx;
        ref_config.reference_last[sl_idx] = 1;
      } else {
        // This reference doesn't match with one specified by GOF. This can
        // only happen if spatial layer is enabled dynamically without key
        // frame. Spatial prediction is supposed to be enabled in this case.
        RTC_DCHECK(is_inter_layer_pred_allowed);
      }
    }

    if (is_inter_layer_pred_allowed && sl_idx > 0) {
      // Set up spatial reference.
      RTC_DCHECK(last_updated_buf_idx);
      ref_config.gld_fb_idx[sl_idx] = *last_updated_buf_idx;
      ref_config.reference_golden[sl_idx] = 1;
    } else {
      RTC_DCHECK(ref_config.reference_last[sl_idx] != 0 || sl_idx == 0 ||
                 inter_layer_pred_ == InterLayerPredMode::kOff);
    }

    last_updated_buf_idx.reset();

    if (gof_.temporal_idx[gof_idx] <= num_temporal_layers_ - 1) {
      last_updated_buf_idx = sl_idx * num_temporal_refs + kUpdBufIdx[gof_idx];

      // Ensure last frame buffer is not used for temporal prediction (it is
      // reserved for spatial reference).
      RTC_DCHECK_LT(*last_updated_buf_idx, kNumVp9Buffers - 1);
    } else if (is_inter_layer_pred_allowed) {
      last_updated_buf_idx = kNumVp9Buffers - 1;
    }

    if (last_updated_buf_idx) {
      ref_config.update_buffer_slot[sl_idx] = 1 << *last_updated_buf_idx;
    }
  }

  return ref_config;
}

int VP9EncoderImpl::GetEncodedLayerFrame(const vpx_codec_cx_pkt* pkt) {
  RTC_DCHECK_EQ(pkt->kind, VPX_CODEC_CX_FRAME_PKT);

  if (pkt->data.frame.sz == 0) {
    // Ignore dropped frame.
    return WEBRTC_VIDEO_CODEC_OK;
  }

  vpx_svc_layer_id_t layer_id = {0};
  vpx_codec_control(encoder_, VP9E_GET_SVC_LAYER_ID, &layer_id);

  const bool first_frame_in_picture = encoded_image_._length == 0;
  // Ensure we don't buffer layers of previous picture (superframe).
  RTC_DCHECK(first_frame_in_picture || layer_id.spatial_layer_id > 0);

  const bool end_of_picture = false;
  DeliverBufferedFrame(end_of_picture);

  if (pkt->data.frame.sz > encoded_image_._size) {
    delete[] encoded_image_._buffer;
    encoded_image_._size = pkt->data.frame.sz;
    encoded_image_._buffer = new uint8_t[encoded_image_._size];
  }
  memcpy(encoded_image_._buffer, pkt->data.frame.buf, pkt->data.frame.sz);
  encoded_image_._length = pkt->data.frame.sz;

  const bool is_key_frame =
      (pkt->data.frame.flags & VPX_FRAME_IS_KEY) ? true : false;
  // Ensure encoder issued key frame on request.
  RTC_DCHECK(is_key_frame || !force_key_frame_);

  // Check if encoded frame is a key frame.
  encoded_image_._frameType = kVideoFrameDelta;
  if (is_key_frame) {
    encoded_image_._frameType = kVideoFrameKey;
    force_key_frame_ = false;
  }
  RTC_DCHECK_LE(encoded_image_._length, encoded_image_._size);

  memset(&codec_specific_, 0, sizeof(codec_specific_));
  absl::optional<int> spatial_index;
  PopulateCodecSpecific(&codec_specific_, &spatial_index, *pkt,
                        input_image_->timestamp(), first_frame_in_picture);
  encoded_image_.SetSpatialIndex(spatial_index);

  if (is_flexible_mode_) {
    UpdateReferenceBuffers(*pkt, pics_since_key_);
  }

  TRACE_COUNTER1("webrtc", "EncodedFrameSize", encoded_image_._length);
  encoded_image_.SetTimestamp(input_image_->timestamp());
  encoded_image_.capture_time_ms_ = input_image_->render_time_ms();
  encoded_image_.rotation_ = input_image_->rotation();
  encoded_image_.content_type_ = (codec_.mode == VideoCodecMode::kScreensharing)
                                     ? VideoContentType::SCREENSHARE
                                     : VideoContentType::UNSPECIFIED;
  encoded_image_._encodedHeight =
      pkt->data.frame.height[layer_id.spatial_layer_id];
  encoded_image_._encodedWidth =
      pkt->data.frame.width[layer_id.spatial_layer_id];
  encoded_image_.timing_.flags = VideoSendTiming::kInvalid;
  int qp = -1;
  vpx_codec_control(encoder_, VP8E_GET_LAST_QUANTIZER, &qp);
  encoded_image_.qp_ = qp;

  return WEBRTC_VIDEO_CODEC_OK;
}

void VP9EncoderImpl::DeliverBufferedFrame(bool end_of_picture) {
  if (encoded_image_._length > 0) {
    codec_specific_.codecSpecific.VP9.end_of_picture = end_of_picture;

    // No data partitioning in VP9, so 1 partition only.
    int part_idx = 0;
    RTPFragmentationHeader frag_info;
    frag_info.VerifyAndAllocateFragmentationHeader(1);
    frag_info.fragmentationOffset[part_idx] = 0;
    frag_info.fragmentationLength[part_idx] = encoded_image_._length;
    frag_info.fragmentationPlType[part_idx] = 0;
    frag_info.fragmentationTimeDiff[part_idx] = 0;

    encoded_complete_callback_->OnEncodedImage(encoded_image_, &codec_specific_,
                                               &frag_info);
    encoded_image_._length = 0;

    if (codec_.mode == VideoCodecMode::kScreensharing) {
      const uint8_t spatial_idx = encoded_image_.SpatialIndex().value_or(0);
      const uint32_t frame_timestamp_ms =
          1000 * encoded_image_.Timestamp() / kVideoPayloadTypeFrequency;
      framerate_controller_[spatial_idx].AddFrame(frame_timestamp_ms);
    }
  }
}

int VP9EncoderImpl::SetChannelParameters(uint32_t packet_loss, int64_t rtt) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP9EncoderImpl::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  encoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* VP9EncoderImpl::ImplementationName() const {
  return "libvpx";
}

std::unique_ptr<VP9Decoder> VP9Decoder::Create() {
  return absl::make_unique<VP9DecoderImpl>();
}

VP9DecoderImpl::VP9DecoderImpl()
    : decode_complete_callback_(nullptr),
      inited_(false),
      decoder_(nullptr),
      key_frame_required_(true) {}

VP9DecoderImpl::~VP9DecoderImpl() {
  inited_ = true;  // in order to do the actual release
  Release();
  int num_buffers_in_use = frame_buffer_pool_.GetNumBuffersInUse();
  if (num_buffers_in_use > 0) {
    // The frame buffers are reference counted and frames are exposed after
    // decoding. There may be valid usage cases where previous frames are still
    // referenced after ~VP9DecoderImpl that is not a leak.
    RTC_LOG(LS_INFO) << num_buffers_in_use << " Vp9FrameBuffers are still "
                     << "referenced during ~VP9DecoderImpl.";
  }
}

int VP9DecoderImpl::InitDecode(const VideoCodec* inst, int number_of_cores) {
  int ret_val = Release();
  if (ret_val < 0) {
    return ret_val;
  }
  if (decoder_ == nullptr) {
    decoder_ = new vpx_codec_ctx_t;
  }
  vpx_codec_dec_cfg_t cfg;
  // Setting number of threads to a constant value (1)
  cfg.threads = 1;
  cfg.h = cfg.w = 0;  // set after decode
  vpx_codec_flags_t flags = 0;
  if (vpx_codec_dec_init(decoder_, vpx_codec_vp9_dx(), &cfg, flags)) {
    return WEBRTC_VIDEO_CODEC_MEMORY;
  }

  if (!frame_buffer_pool_.InitializeVpxUsePool(decoder_)) {
    return WEBRTC_VIDEO_CODEC_MEMORY;
  }

  inited_ = true;
  // Always start with a complete key frame.
  key_frame_required_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP9DecoderImpl::Decode(const EncodedImage& input_image,
                           bool missing_frames,
                           const CodecSpecificInfo* codec_specific_info,
                           int64_t /*render_time_ms*/) {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (decode_complete_callback_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  // Always start with a complete key frame.
  if (key_frame_required_) {
    if (input_image._frameType != kVideoFrameKey)
      return WEBRTC_VIDEO_CODEC_ERROR;
    // We have a key frame - is it complete?
    if (input_image._completeFrame) {
      key_frame_required_ = false;
    } else {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  vpx_codec_iter_t iter = nullptr;
  vpx_image_t* img;
  uint8_t* buffer = input_image._buffer;
  if (input_image._length == 0) {
    buffer = nullptr;  // Triggers full frame concealment.
  }
  // During decode libvpx may get and release buffers from |frame_buffer_pool_|.
  // In practice libvpx keeps a few (~3-4) buffers alive at a time.
  if (vpx_codec_decode(decoder_, buffer,
                       static_cast<unsigned int>(input_image._length), 0,
                       VPX_DL_REALTIME)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  // |img->fb_priv| contains the image data, a reference counted Vp9FrameBuffer.
  // It may be released by libvpx during future vpx_codec_decode or
  // vpx_codec_destroy calls.
  img = vpx_codec_get_frame(decoder_, &iter);
  int qp;
  vpx_codec_err_t vpx_ret =
      vpx_codec_control(decoder_, VPXD_GET_LAST_QUANTIZER, &qp);
  RTC_DCHECK_EQ(vpx_ret, VPX_CODEC_OK);
  int ret =
      ReturnFrame(img, input_image.Timestamp(), input_image.ntp_time_ms_, qp);
  if (ret != 0) {
    return ret;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP9DecoderImpl::ReturnFrame(const vpx_image_t* img,
                                uint32_t timestamp,
                                int64_t ntp_time_ms,
                                int qp) {
  if (img == nullptr) {
    // Decoder OK and nullptr image => No show frame.
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }

  // This buffer contains all of |img|'s image data, a reference counted
  // Vp9FrameBuffer. (libvpx is done with the buffers after a few
  // vpx_codec_decode calls or vpx_codec_destroy).
  Vp9FrameBufferPool::Vp9FrameBuffer* img_buffer =
      static_cast<Vp9FrameBufferPool::Vp9FrameBuffer*>(img->fb_priv);

  // The buffer can be used directly by the VideoFrame (without copy) by
  // using a Wrapped*Buffer.
  rtc::scoped_refptr<VideoFrameBuffer> img_wrapped_buffer;
  switch (img->bit_depth) {
    case 8:
      img_wrapped_buffer = WrapI420Buffer(
          img->d_w, img->d_h, img->planes[VPX_PLANE_Y],
          img->stride[VPX_PLANE_Y], img->planes[VPX_PLANE_U],
          img->stride[VPX_PLANE_U], img->planes[VPX_PLANE_V],
          img->stride[VPX_PLANE_V],
          // WrappedI420Buffer's mechanism for allowing the release of its frame
          // buffer is through a callback function. This is where we should
          // release |img_buffer|.
          rtc::KeepRefUntilDone(img_buffer));
      break;
    case 10:
      img_wrapped_buffer = WrapI010Buffer(
          img->d_w, img->d_h,
          reinterpret_cast<const uint16_t*>(img->planes[VPX_PLANE_Y]),
          img->stride[VPX_PLANE_Y] / 2,
          reinterpret_cast<const uint16_t*>(img->planes[VPX_PLANE_U]),
          img->stride[VPX_PLANE_U] / 2,
          reinterpret_cast<const uint16_t*>(img->planes[VPX_PLANE_V]),
          img->stride[VPX_PLANE_V] / 2, rtc::KeepRefUntilDone(img_buffer));
      break;
    default:
      RTC_NOTREACHED();
      return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }

  VideoFrame decoded_image = VideoFrame::Builder()
                                 .set_video_frame_buffer(img_wrapped_buffer)
                                 .set_timestamp_ms(0)
                                 .set_timestamp_rtp(timestamp)
                                 .set_ntp_time_ms(ntp_time_ms)
                                 .set_rotation(webrtc::kVideoRotation_0)
                                 .set_color_space(ExtractVP9ColorSpace(
                                     img->cs, img->range, img->bit_depth))
                                 .build();
  decode_complete_callback_->Decoded(decoded_image, absl::nullopt, qp);
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP9DecoderImpl::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP9DecoderImpl::Release() {
  int ret_val = WEBRTC_VIDEO_CODEC_OK;

  if (decoder_ != nullptr) {
    if (inited_) {
      // When a codec is destroyed libvpx will release any buffers of
      // |frame_buffer_pool_| it is currently using.
      if (vpx_codec_destroy(decoder_)) {
        ret_val = WEBRTC_VIDEO_CODEC_MEMORY;
      }
    }
    delete decoder_;
    decoder_ = nullptr;
  }
  // Releases buffers from the pool. Any buffers not in use are deleted. Buffers
  // still referenced externally are deleted once fully released, not returning
  // to the pool.
  frame_buffer_pool_.ClearPool();
  inited_ = false;
  return ret_val;
}

const char* VP9DecoderImpl::ImplementationName() const {
  return "libvpx";
}

}  // namespace webrtc
