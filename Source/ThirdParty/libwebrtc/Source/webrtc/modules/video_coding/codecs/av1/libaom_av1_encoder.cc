/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/macros.h"
#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/svc/create_scalability_structure.h"
#include "modules/video_coding/svc/scalable_video_controller.h"
#include "modules/video_coding/svc/scalable_video_controller_no_layering.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "third_party/libaom/source/libaom/aom/aom_codec.h"
#include "third_party/libaom/source/libaom/aom/aom_encoder.h"
#include "third_party/libaom/source/libaom/aom/aomcx.h"

namespace webrtc {
namespace {

// Encoder configuration parameters
constexpr int kQpMin = 10;
constexpr int kUsageProfile = AOM_USAGE_REALTIME;
constexpr int kMinQindex = 145;   // Min qindex threshold for QP scaling.
constexpr int kMaxQindex = 205;   // Max qindex threshold for QP scaling.
constexpr int kBitDepth = 8;
constexpr int kLagInFrames = 0;  // No look ahead.
constexpr int kRtpTicksPerSecond = 90000;
constexpr float kMinimumFrameRate = 1.0;

// Only positive speeds, range for real-time coding currently is: 6 - 8.
// Lower means slower/better quality, higher means fastest/lower quality.
int GetCpuSpeed(int width, int height, int number_of_cores) {
  // For smaller resolutions, use lower speed setting (get some coding gain at
  // the cost of increased encoding complexity).
  if (number_of_cores > 4 && width * height < 320 * 180)
    return 6;
  else if (width * height >= 1280 * 720)
    return 9;
  else if (width * height >= 640 * 360)
    return 8;
  else
    return 7;
}

aom_superblock_size_t GetSuperblockSize(int width, int height, int threads) {
  int resolution = width * height;
  if (threads >= 4 && resolution >= 960 * 540 && resolution < 1920 * 1080)
    return AOM_SUPERBLOCK_SIZE_64X64;
  else
    return AOM_SUPERBLOCK_SIZE_DYNAMIC;
}

class LibaomAv1Encoder final : public VideoEncoder {
 public:
  LibaomAv1Encoder();
  ~LibaomAv1Encoder();

  int InitEncode(const VideoCodec* codec_settings,
                 const Settings& settings) override;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* encoded_image_callback) override;

  int32_t Release() override;

  int32_t Encode(const VideoFrame& frame,
                 const std::vector<VideoFrameType>* frame_types) override;

  void SetRates(const RateControlParameters& parameters) override;

  EncoderInfo GetEncoderInfo() const override;

 private:
  // Determine number of encoder threads to use.
  int NumberOfThreads(int width, int height, int number_of_cores);

  bool SvcEnabled() const { return svc_params_.has_value(); }
  // Fills svc_params_ memeber value. Returns false on error.
  bool SetSvcParams(ScalableVideoController::StreamLayersConfig svc_config);
  // Configures the encoder with layer for the next frame.
  void SetSvcLayerId(
      const ScalableVideoController::LayerFrameConfig& layer_frame);
  // Configures the encoder which buffers next frame updates and can reference.
  void SetSvcRefFrameConfig(
      const ScalableVideoController::LayerFrameConfig& layer_frame);

  std::unique_ptr<ScalableVideoController> svc_controller_;
  bool inited_;
  absl::optional<aom_svc_params_t> svc_params_;
  VideoCodec encoder_settings_;
  aom_image_t* frame_for_encode_;
  aom_codec_ctx_t ctx_;
  aom_codec_enc_cfg_t cfg_;
  EncodedImageCallback* encoded_image_callback_;
};

int32_t VerifyCodecSettings(const VideoCodec& codec_settings) {
  if (codec_settings.width < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings.height < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // maxBitrate == 0 represents an unspecified maxBitRate.
  if (codec_settings.maxBitrate > 0 &&
      codec_settings.minBitrate > codec_settings.maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings.maxBitrate > 0 &&
      codec_settings.startBitrate > codec_settings.maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings.startBitrate < codec_settings.minBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings.maxFramerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

LibaomAv1Encoder::LibaomAv1Encoder()
    : inited_(false),
      frame_for_encode_(nullptr),
      encoded_image_callback_(nullptr) {}

LibaomAv1Encoder::~LibaomAv1Encoder() {
  Release();
}

int LibaomAv1Encoder::InitEncode(const VideoCodec* codec_settings,
                                 const Settings& settings) {
  if (codec_settings == nullptr) {
    RTC_LOG(LS_WARNING) << "No codec settings provided to "
                           "LibaomAv1Encoder.";
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (settings.number_of_cores < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inited_) {
    RTC_LOG(LS_WARNING) << "Initing LibaomAv1Encoder without first releasing.";
    Release();
  }
  encoder_settings_ = *codec_settings;

  // Sanity checks for encoder configuration.
  const int32_t result = VerifyCodecSettings(encoder_settings_);
  if (result < 0) {
    RTC_LOG(LS_WARNING) << "Incorrect codec settings provided to "
                           "LibaomAv1Encoder.";
    return result;
  }
  if (encoder_settings_.numberOfSimulcastStreams > 1) {
    RTC_LOG(LS_WARNING) << "Simulcast is not implemented by LibaomAv1Encoder.";
    return result;
  }
  absl::string_view scalability_mode = encoder_settings_.ScalabilityMode();
  if (scalability_mode.empty()) {
    RTC_LOG(LS_WARNING) << "Scalability mode is not set, using 'NONE'.";
    scalability_mode = "NONE";
  }
  svc_controller_ = CreateScalabilityStructure(scalability_mode);
  if (svc_controller_ == nullptr) {
    RTC_LOG(LS_WARNING) << "Failed to set scalability mode "
                        << scalability_mode;
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  if (!SetSvcParams(svc_controller_->StreamConfig())) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Initialize encoder configuration structure with default values
  aom_codec_err_t ret =
      aom_codec_enc_config_default(aom_codec_av1_cx(), &cfg_, kUsageProfile);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on aom_codec_enc_config_default.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Overwrite default config with input encoder settings & RTC-relevant values.
  cfg_.g_w = encoder_settings_.width;
  cfg_.g_h = encoder_settings_.height;
  cfg_.g_threads =
      NumberOfThreads(cfg_.g_w, cfg_.g_h, settings.number_of_cores);
  cfg_.g_timebase.num = 1;
  cfg_.g_timebase.den = kRtpTicksPerSecond;
  cfg_.rc_target_bitrate = encoder_settings_.maxBitrate;  // kilobits/sec.
  cfg_.g_input_bit_depth = kBitDepth;
  cfg_.kf_mode = AOM_KF_DISABLED;
  cfg_.rc_min_quantizer = kQpMin;
  cfg_.rc_max_quantizer = encoder_settings_.qpMax;
  cfg_.g_usage = kUsageProfile;
  cfg_.g_error_resilient = 0;
  // Low-latency settings.
  cfg_.rc_end_usage = AOM_CBR;          // Constant Bit Rate (CBR) mode
  cfg_.g_pass = AOM_RC_ONE_PASS;        // One-pass rate control
  cfg_.g_lag_in_frames = kLagInFrames;  // No look ahead when lag equals 0.

  // Creating a wrapper to the image - setting image data to nullptr. Actual
  // pointer will be set in encode. Setting align to 1, as it is meaningless
  // (actual memory is not allocated).
  frame_for_encode_ =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg_.g_w, cfg_.g_h, 1);

  // Flag options: AOM_CODEC_USE_PSNR and AOM_CODEC_USE_HIGHBITDEPTH
  aom_codec_flags_t flags = 0;

  // Initialize an encoder instance.
  ret = aom_codec_enc_init(&ctx_, aom_codec_av1_cx(), &cfg_, flags);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on aom_codec_enc_init.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  inited_ = true;

  // Set control parameters
  ret = aom_codec_control(
      &ctx_, AOME_SET_CPUUSED,
      GetCpuSpeed(cfg_.g_w, cfg_.g_h, settings.number_of_cores));
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_CPUUSED.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_CDEF, 1);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_CDEF.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_TPL_MODEL, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_TPL_MODEL.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_DELTAQ_MODE, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_DELTAQ_MODE.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_ORDER_HINT, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_ORDER_HINT.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_AQ_MODE, 3);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_AQ_MODE.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (SvcEnabled()) {
    ret = aom_codec_control(&ctx_, AV1E_SET_SVC_PARAMS, &*svc_params_);
    if (ret != AOM_CODEC_OK) {
      RTC_LOG(LS_WARNING) << "LibaomAV1Encoder::EncodeInit returned " << ret
                          << " on control AV1E_SET_SVC_PARAMS.";
      return false;
    }
  }

  ret = aom_codec_control(&ctx_, AOME_SET_MAX_INTRA_BITRATE_PCT, 300);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_MAX_INTRA_BITRATE_PCT.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_COEFF_COST_UPD_FREQ, 3);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_COEFF_COST_UPD_FREQ.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_MODE_COST_UPD_FREQ, 3);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_MODE_COST_UPD_FREQ.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_MV_COST_UPD_FREQ, 3);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_MV_COST_UPD_FREQ.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (cfg_.g_threads == 4 && cfg_.g_w == 640 &&
      (cfg_.g_h == 360 || cfg_.g_h == 480)) {
    ret = aom_codec_control(&ctx_, AV1E_SET_TILE_ROWS,
                            static_cast<int>(log2(cfg_.g_threads)));
    if (ret != AOM_CODEC_OK) {
      RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                          << " on control AV1E_SET_TILE_ROWS.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  } else {
    ret = aom_codec_control(&ctx_, AV1E_SET_TILE_COLUMNS,
                            static_cast<int>(log2(cfg_.g_threads)));
    if (ret != AOM_CODEC_OK) {
      RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                          << " on control AV1E_SET_TILE_COLUMNS.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_ROW_MT, 1);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ROW_MT.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_OBMC, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_OBMC.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_NOISE_SENSITIVITY, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_NOISE_SENSITIVITY.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_WARPED_MOTION, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_WARPED_MOTION.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_GLOBAL_MOTION.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_REF_FRAME_MVS, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_REF_FRAME_MVS.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret =
      aom_codec_control(&ctx_, AV1E_SET_SUPERBLOCK_SIZE,
                        GetSuperblockSize(cfg_.g_w, cfg_.g_h, cfg_.g_threads));
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_SUPERBLOCK_SIZE.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_CFL_INTRA, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_CFL_INTRA.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_SMOOTH_INTRA, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_SMOOTH_INTRA.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_ANGLE_DELTA, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_ANGLE_DELTA.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_FILTER_INTRA, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_FILTER_INTRA.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ret = aom_codec_control(&ctx_, AV1E_SET_INTRA_DEFAULT_TX_ONLY, 1);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING)
        << "LibaomAv1Encoder::EncodeInit returned " << ret
        << " on control AOM_CTRL_AV1E_SET_INTRA_DEFAULT_TX_ONLY.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int LibaomAv1Encoder::NumberOfThreads(int width,
                                      int height,
                                      int number_of_cores) {
  // Keep the number of encoder threads equal to the possible number of
  // column/row tiles, which is (1, 2, 4, 8). See comments below for
  // AV1E_SET_TILE_COLUMNS/ROWS.
  if (width * height >= 640 * 360 && number_of_cores > 4) {
    return 4;
  } else if (width * height >= 320 * 180 && number_of_cores > 2) {
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

bool LibaomAv1Encoder::SetSvcParams(
    ScalableVideoController::StreamLayersConfig svc_config) {
  bool svc_enabled =
      svc_config.num_spatial_layers > 1 || svc_config.num_temporal_layers > 1;
  if (!svc_enabled) {
    svc_params_ = absl::nullopt;
    return true;
  }
  if (svc_config.num_spatial_layers < 1 || svc_config.num_spatial_layers > 4) {
    RTC_LOG(LS_WARNING) << "Av1 supports up to 4 spatial layers. "
                        << svc_config.num_spatial_layers << " configured.";
    return false;
  }
  if (svc_config.num_temporal_layers < 1 ||
      svc_config.num_temporal_layers > 8) {
    RTC_LOG(LS_WARNING) << "Av1 supports up to 8 temporal layers. "
                        << svc_config.num_temporal_layers << " configured.";
    return false;
  }
  aom_svc_params_t& svc_params = svc_params_.emplace();
  svc_params.number_spatial_layers = svc_config.num_spatial_layers;
  svc_params.number_temporal_layers = svc_config.num_temporal_layers;

  int num_layers =
      svc_config.num_spatial_layers * svc_config.num_temporal_layers;
  for (int i = 0; i < num_layers; ++i) {
    svc_params.min_quantizers[i] = kQpMin;
    svc_params.max_quantizers[i] = encoder_settings_.qpMax;
  }

  // Assume each temporal layer doubles framerate.
  for (int tid = 0; tid < svc_config.num_temporal_layers; ++tid) {
    svc_params.framerate_factor[tid] =
        1 << (svc_config.num_temporal_layers - tid - 1);
  }

  for (int sid = 0; sid < svc_config.num_spatial_layers; ++sid) {
    svc_params.scaling_factor_num[sid] = svc_config.scaling_factor_num[sid];
    svc_params.scaling_factor_den[sid] = svc_config.scaling_factor_den[sid];
  }

  return true;
}

void LibaomAv1Encoder::SetSvcLayerId(
    const ScalableVideoController::LayerFrameConfig& layer_frame) {
  aom_svc_layer_id_t layer_id = {};
  layer_id.spatial_layer_id = layer_frame.SpatialId();
  layer_id.temporal_layer_id = layer_frame.TemporalId();
  aom_codec_err_t ret =
      aom_codec_control(&ctx_, AV1E_SET_SVC_LAYER_ID, &layer_id);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::Encode returned " << ret
                        << " on control AV1E_SET_SVC_LAYER_ID.";
  }
}

void LibaomAv1Encoder::SetSvcRefFrameConfig(
    const ScalableVideoController::LayerFrameConfig& layer_frame) {
  // Buffer name to use for each layer_frame.buffers position. In particular
  // when there are 2 buffers are referenced, prefer name them last and golden,
  // because av1 bitstream format has dedicated fields for these two names.
  // See last_frame_idx and golden_frame_idx in the av1 spec
  // https://aomediacodec.github.io/av1-spec/av1-spec.pdf
  static constexpr int kPreferedSlotName[] = {0,  // Last
                                              3,  // Golden
                                              1, 2, 4, 5, 6};
  static constexpr int kAv1NumBuffers = 8;

  aom_svc_ref_frame_config_t ref_frame_config = {};
  RTC_CHECK_LE(layer_frame.Buffers().size(), ABSL_ARRAYSIZE(kPreferedSlotName));
  for (size_t i = 0; i < layer_frame.Buffers().size(); ++i) {
    const CodecBufferUsage& buffer = layer_frame.Buffers()[i];
    int slot_name = kPreferedSlotName[i];
    RTC_CHECK_GE(buffer.id, 0);
    RTC_CHECK_LT(buffer.id, kAv1NumBuffers);
    ref_frame_config.ref_idx[slot_name] = buffer.id;
    if (buffer.referenced) {
      ref_frame_config.reference[slot_name] = 1;
    }
    if (buffer.updated) {
      ref_frame_config.refresh[buffer.id] = 1;
    }
  }
  aom_codec_err_t ret = aom_codec_control(&ctx_, AV1E_SET_SVC_REF_FRAME_CONFIG,
                                          &ref_frame_config);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::Encode returned " << ret
                        << " on control AV1_SET_SVC_REF_FRAME_CONFIG.";
  }
}

int32_t LibaomAv1Encoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* encoded_image_callback) {
  encoded_image_callback_ = encoded_image_callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t LibaomAv1Encoder::Release() {
  if (frame_for_encode_ != nullptr) {
    aom_img_free(frame_for_encode_);
    frame_for_encode_ = nullptr;
  }
  if (inited_) {
    if (aom_codec_destroy(&ctx_)) {
      return WEBRTC_VIDEO_CODEC_MEMORY;
    }
    inited_ = false;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t LibaomAv1Encoder::Encode(
    const VideoFrame& frame,
    const std::vector<VideoFrameType>* frame_types) {
  if (!inited_ || encoded_image_callback_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  bool keyframe_required =
      frame_types != nullptr &&
      absl::c_linear_search(*frame_types, VideoFrameType::kVideoFrameKey);

  std::vector<ScalableVideoController::LayerFrameConfig> layer_frames =
      svc_controller_->NextFrameConfig(keyframe_required);

  if (layer_frames.empty()) {
    RTC_LOG(LS_ERROR) << "SVCController returned no configuration for a frame.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Convert input frame to I420, if needed.
  VideoFrame prepped_input_frame = frame;
  if (prepped_input_frame.video_frame_buffer()->type() !=
          VideoFrameBuffer::Type::kI420 &&
      prepped_input_frame.video_frame_buffer()->type() !=
          VideoFrameBuffer::Type::kI420A) {
    rtc::scoped_refptr<I420BufferInterface> converted_buffer(
        prepped_input_frame.video_frame_buffer()->ToI420());
    // The buffer should now be a mapped I420 or I420A format, but some buffer
    // implementations incorrectly return the wrong buffer format, such as
    // kNative. As a workaround to this, we perform ToI420() a second time.
    // TODO(https://crbug.com/webrtc/12602): When Android buffers have a correct
    // ToI420() implementaion, remove his workaround.
    if (converted_buffer->type() != VideoFrameBuffer::Type::kI420 &&
        converted_buffer->type() != VideoFrameBuffer::Type::kI420A) {
      converted_buffer = converted_buffer->ToI420();
      RTC_CHECK(converted_buffer->type() == VideoFrameBuffer::Type::kI420 ||
                converted_buffer->type() == VideoFrameBuffer::Type::kI420A);
    }
    prepped_input_frame = VideoFrame(converted_buffer, frame.timestamp(),
                                     frame.render_time_ms(), frame.rotation());
  }

  // Set frame_for_encode_ data pointers and strides.
  auto i420_buffer = prepped_input_frame.video_frame_buffer()->GetI420();
  frame_for_encode_->planes[AOM_PLANE_Y] =
      const_cast<unsigned char*>(i420_buffer->DataY());
  frame_for_encode_->planes[AOM_PLANE_U] =
      const_cast<unsigned char*>(i420_buffer->DataU());
  frame_for_encode_->planes[AOM_PLANE_V] =
      const_cast<unsigned char*>(i420_buffer->DataV());
  frame_for_encode_->stride[AOM_PLANE_Y] = i420_buffer->StrideY();
  frame_for_encode_->stride[AOM_PLANE_U] = i420_buffer->StrideU();
  frame_for_encode_->stride[AOM_PLANE_V] = i420_buffer->StrideV();

  const uint32_t duration =
      kRtpTicksPerSecond / static_cast<float>(encoder_settings_.maxFramerate);

  for (size_t i = 0; i < layer_frames.size(); ++i) {
    ScalableVideoController::LayerFrameConfig& layer_frame = layer_frames[i];
    const bool end_of_picture = i == layer_frames.size() - 1;

    aom_enc_frame_flags_t flags =
        layer_frame.IsKeyframe() ? AOM_EFLAG_FORCE_KF : 0;

    if (SvcEnabled()) {
      SetSvcLayerId(layer_frame);
      SetSvcRefFrameConfig(layer_frame);

      aom_codec_err_t ret =
          aom_codec_control(&ctx_, AV1E_SET_ERROR_RESILIENT_MODE,
                            layer_frame.TemporalId() > 0 ? 1 : 0);
      if (ret != AOM_CODEC_OK) {
        RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::Encode returned " << ret
                            << " on control AV1E_SET_ERROR_RESILIENT_MODE.";
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
    }

    // Encode a frame.
    aom_codec_err_t ret = aom_codec_encode(&ctx_, frame_for_encode_,
                                           frame.timestamp(), duration, flags);
    if (ret != AOM_CODEC_OK) {
      RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::Encode returned " << ret
                          << " on aom_codec_encode.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Get encoded image data.
    EncodedImage encoded_image;
    aom_codec_iter_t iter = nullptr;
    int data_pkt_count = 0;
    while (const aom_codec_cx_pkt_t* pkt =
               aom_codec_get_cx_data(&ctx_, &iter)) {
      if (pkt->kind == AOM_CODEC_CX_FRAME_PKT && pkt->data.frame.sz > 0) {
        if (data_pkt_count > 0) {
          RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::Encoder returned more than "
                                 "one data packet for an input video frame.";
          Release();
        }
        encoded_image.SetEncodedData(EncodedImageBuffer::Create(
            /*data=*/static_cast<const uint8_t*>(pkt->data.frame.buf),
            /*size=*/pkt->data.frame.sz));

        if ((pkt->data.frame.flags & AOM_EFLAG_FORCE_KF) != 0) {
          layer_frame.Keyframe();
        }
        encoded_image._frameType = layer_frame.IsKeyframe()
                                       ? VideoFrameType::kVideoFrameKey
                                       : VideoFrameType::kVideoFrameDelta;
        encoded_image.SetTimestamp(frame.timestamp());
        encoded_image.capture_time_ms_ = frame.render_time_ms();
        encoded_image.rotation_ = frame.rotation();
        encoded_image.content_type_ = VideoContentType::UNSPECIFIED;
        // If encoded image width/height info are added to aom_codec_cx_pkt_t,
        // use those values in lieu of the values in frame.
        encoded_image._encodedHeight = frame.height();
        encoded_image._encodedWidth = frame.width();
        encoded_image.timing_.flags = VideoSendTiming::kInvalid;
        int qp = -1;
        ret = aom_codec_control(&ctx_, AOME_GET_LAST_QUANTIZER, &qp);
        if (ret != AOM_CODEC_OK) {
          RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::Encode returned " << ret
                              << " on control AOME_GET_LAST_QUANTIZER.";
          return WEBRTC_VIDEO_CODEC_ERROR;
        }
        encoded_image.qp_ = qp;
        encoded_image.SetColorSpace(frame.color_space());
        ++data_pkt_count;
      }
    }

    // Deliver encoded image data.
    if (encoded_image.size() > 0) {
      CodecSpecificInfo codec_specific_info;
      codec_specific_info.codecType = kVideoCodecAV1;
      codec_specific_info.end_of_picture = end_of_picture;
      bool is_keyframe = layer_frame.IsKeyframe();
      codec_specific_info.generic_frame_info =
          svc_controller_->OnEncodeDone(std::move(layer_frame));
      if (is_keyframe && codec_specific_info.generic_frame_info) {
        codec_specific_info.template_structure =
            svc_controller_->DependencyStructure();
        auto& resolutions = codec_specific_info.template_structure->resolutions;
        if (SvcEnabled()) {
          resolutions.resize(svc_params_->number_spatial_layers);
          for (int sid = 0; sid < svc_params_->number_spatial_layers; ++sid) {
            int n = svc_params_->scaling_factor_num[sid];
            int d = svc_params_->scaling_factor_den[sid];
            resolutions[sid] =
                RenderResolution(cfg_.g_w * n / d, cfg_.g_h * n / d);
          }
        } else {
          resolutions = {RenderResolution(cfg_.g_w, cfg_.g_h)};
        }
      }
      encoded_image_callback_->OnEncodedImage(encoded_image,
                                              &codec_specific_info);
    }
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

void LibaomAv1Encoder::SetRates(const RateControlParameters& parameters) {
  if (!inited_) {
    RTC_LOG(LS_WARNING) << "SetRates() while encoder is not initialized";
    return;
  }
  if (parameters.framerate_fps < kMinimumFrameRate) {
    RTC_LOG(LS_WARNING) << "Unsupported framerate (must be >= "
                        << kMinimumFrameRate
                        << " ): " << parameters.framerate_fps;
    return;
  }
  if (parameters.bitrate.get_sum_bps() == 0) {
    RTC_LOG(LS_WARNING) << "Attempt to set target bit rate to zero";
    return;
  }

  svc_controller_->OnRatesUpdated(parameters.bitrate);
  cfg_.rc_target_bitrate = parameters.bitrate.get_sum_kbps();

  if (SvcEnabled()) {
    for (int sid = 0; sid < svc_params_->number_spatial_layers; ++sid) {
      // libaom bitrate for spatial id S and temporal id T means bitrate
      // of frames with spatial_id=S and temporal_id<=T
      // while `parameters.bitrate` provdies bitrate of frames with
      // spatial_id=S and temporal_id=T
      int accumulated_bitrate_bps = 0;
      for (int tid = 0; tid < svc_params_->number_temporal_layers; ++tid) {
        int layer_index = sid * svc_params_->number_temporal_layers + tid;
        accumulated_bitrate_bps += parameters.bitrate.GetBitrate(sid, tid);
        // `svc_params.layer_target_bitrate` expects bitrate in kbps.
        svc_params_->layer_target_bitrate[layer_index] =
            accumulated_bitrate_bps / 1000;
      }
    }
    aom_codec_control(&ctx_, AV1E_SET_SVC_PARAMS, &*svc_params_);
  }

  // Set frame rate to closest integer value.
  encoder_settings_.maxFramerate =
      static_cast<uint32_t>(parameters.framerate_fps + 0.5);

  // Update encoder context.
  aom_codec_err_t error_code = aom_codec_enc_config_set(&ctx_, &cfg_);
  if (error_code != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "Error configuring encoder, error code: "
                        << error_code;
  }
}

VideoEncoder::EncoderInfo LibaomAv1Encoder::GetEncoderInfo() const {
  EncoderInfo info;
  info.supports_native_handle = false;
  info.implementation_name = "libaom";
  info.has_trusted_rate_controller = true;
  info.is_hardware_accelerated = false;
  info.scaling_settings = VideoEncoder::ScalingSettings(kMinQindex, kMaxQindex);
  info.preferred_pixel_formats = {VideoFrameBuffer::Type::kI420};
  if (SvcEnabled()) {
    for (int sid = 0; sid < svc_params_->number_spatial_layers; ++sid) {
      info.fps_allocation[sid].resize(svc_params_->number_temporal_layers);
      for (int tid = 0; tid < svc_params_->number_temporal_layers; ++tid) {
        info.fps_allocation[sid][tid] =
            encoder_settings_.maxFramerate / svc_params_->framerate_factor[tid];
      }
    }
  }
  return info;
}

}  // namespace

const bool kIsLibaomAv1EncoderSupported = true;

std::unique_ptr<VideoEncoder> CreateLibaomAv1Encoder() {
  return std::make_unique<LibaomAv1Encoder>();
}

}  // namespace webrtc
