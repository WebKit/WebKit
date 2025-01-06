/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/libaom_av1_encoder_factory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "absl/types/variant.h"
#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/video/resolution.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/rational.h"
#include "rtc_base/strings/string_builder.h"
#include "third_party/libaom/source/libaom/aom/aom_codec.h"
#include "third_party/libaom/source/libaom/aom/aom_encoder.h"
#include "third_party/libaom/source/libaom/aom/aom_image.h"
#include "third_party/libaom/source/libaom/aom/aomcx.h"

#define SET_OR_RETURN(param_id, param_value)                          \
  do {                                                                \
    if (!SetEncoderControlParameters(&ctx_, param_id, param_value)) { \
      return;                                                         \
    }                                                                 \
  } while (0)

#define SET_OR_RETURN_FALSE(param_id, param_value)                    \
  do {                                                                \
    if (!SetEncoderControlParameters(&ctx_, param_id, param_value)) { \
      return false;                                                   \
    }                                                                 \
  } while (0)

namespace webrtc {

using FrameEncodeSettings = VideoEncoderInterface::FrameEncodeSettings;
using Cbr = FrameEncodeSettings::Cbr;
using Cqp = FrameEncodeSettings::Cqp;
using aom_img_ptr = std::unique_ptr<aom_image_t, decltype(&aom_img_free)>;

namespace {
// MaxQp defined here:
// http://google3/third_party/libaom/git_root/av1/av1_cx_iface.c;l=3510;rcl=527067478
constexpr int kMaxQp = 63;
constexpr int kNumBuffers = 8;
constexpr int kMaxReferences = 3;
constexpr int kMinEffortLevel = -2;
constexpr int kMaxEffortLevel = 2;
constexpr int kMaxSpatialLayersWtf = 4;
constexpr int kMaxTemporalLayers = 4;
constexpr int kRtpTicksPerSecond = 90000;
constexpr std::array<VideoFrameBuffer::Type, 2> kSupportedInputFormats = {
    VideoFrameBuffer::Type::kI420, VideoFrameBuffer::Type::kNV12};

constexpr std::array<Rational, 7> kSupportedScalingFactors = {
    {{8, 1}, {4, 1}, {2, 1}, {1, 1}, {1, 2}, {1, 4}, {1, 8}}};

std::optional<Rational> GetScalingFactor(const Resolution& from,
                                         const Resolution& to) {
  auto it = absl::c_find_if(kSupportedScalingFactors, [&](const Rational& r) {
    return (from.width * r.numerator / r.denominator) == to.width &&
           (from.height * r.numerator / r.denominator) == to.height;
  });

  if (it != kSupportedScalingFactors.end()) {
    return *it;
  }

  return {};
}

class LibaomAv1Encoder : public VideoEncoderInterface {
 public:
  LibaomAv1Encoder() = default;
  ~LibaomAv1Encoder() override;

  bool InitEncode(
      const VideoEncoderFactoryInterface::StaticEncoderSettings& settings,
      const std::map<std::string, std::string>& encoder_specific_settings);

  void Encode(rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer,
              const TemporalUnitSettings& tu_settings,
              std::vector<FrameEncodeSettings> frame_settings) override;

 private:
  aom_img_ptr image_to_encode_ = aom_img_ptr(nullptr, aom_img_free);
  aom_codec_ctx_t ctx_;
  aom_codec_enc_cfg_t cfg_;

  std::optional<VideoCodecMode> current_content_type_;
  std::array<std::optional<int>, kMaxSpatialLayersWtf> current_effort_level_;
  int max_number_of_threads_;
  std::array<std::optional<Resolution>, 8> last_resolution_in_buffer_;
};

template <typename T>
bool SetEncoderControlParameters(aom_codec_ctx_t* ctx, int id, T value) {
  aom_codec_err_t error_code = aom_codec_control(ctx, id, value);
  if (error_code != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "aom_codec_control returned " << error_code
                        << " with id:  " << id << ".";
  }
  return error_code == AOM_CODEC_OK;
}

LibaomAv1Encoder::~LibaomAv1Encoder() {
  aom_codec_destroy(&ctx_);
}

bool LibaomAv1Encoder::InitEncode(
    const VideoEncoderFactoryInterface::StaticEncoderSettings& settings,
    const std::map<std::string, std::string>& encoder_specific_settings) {
  if (!encoder_specific_settings.empty()) {
    RTC_LOG(LS_ERROR)
        << "libaom av1 encoder accepts no encoder specific settings";
    return false;
  }

  if (aom_codec_err_t ret = aom_codec_enc_config_default(
          aom_codec_av1_cx(), &cfg_, AOM_USAGE_REALTIME);
      ret != AOM_CODEC_OK) {
    RTC_LOG(LS_ERROR) << "aom_codec_enc_config_default returned " << ret;
    return false;
  }

  max_number_of_threads_ = settings.max_number_of_threads;

  // The encode resolution is set dynamically for each call to `Encode`, but for
  // `aom_codec_enc_init` to not fail we set it here as well.
  cfg_.g_w = settings.max_encode_dimensions.width;
  cfg_.g_h = settings.max_encode_dimensions.height;
  cfg_.g_timebase.num = 1;
  // TD: does 90khz timebase make sense, use microseconds instead maybe?
  cfg_.g_timebase.den = kRtpTicksPerSecond;
  cfg_.g_input_bit_depth = settings.encoding_format.bit_depth;
  cfg_.kf_mode = AOM_KF_DISABLED;
  // TD: rc_undershoot_pct and rc_overshoot_pct should probably be removed.
  cfg_.rc_undershoot_pct = 50;
  cfg_.rc_overshoot_pct = 50;
  auto* cbr =
      absl::get_if<VideoEncoderFactoryInterface::StaticEncoderSettings::Cbr>(
          &settings.rc_mode);
  cfg_.rc_buf_initial_sz = cbr ? cbr->target_buffer_size.ms() : 600;
  cfg_.rc_buf_optimal_sz = cbr ? cbr->target_buffer_size.ms() : 600;
  cfg_.rc_buf_sz = cbr ? cbr->max_buffer_size.ms() : 1000;
  cfg_.g_usage = AOM_USAGE_REALTIME;
  cfg_.g_pass = AOM_RC_ONE_PASS;
  cfg_.g_lag_in_frames = 0;
  cfg_.g_error_resilient = 0;
  cfg_.rc_end_usage = cbr ? AOM_CBR : AOM_Q;

  if (aom_codec_err_t ret =
          aom_codec_enc_init(&ctx_, aom_codec_av1_cx(), &cfg_, /*flags=*/0);
      ret != AOM_CODEC_OK) {
    RTC_LOG(LS_ERROR) << "aom_codec_enc_init returned " << ret;
    return false;
  }

  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_CDEF, 1);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_TPL_MODEL, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_DELTAQ_MODE, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_ORDER_HINT, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_AQ_MODE, 3);
  SET_OR_RETURN_FALSE(AOME_SET_MAX_INTRA_BITRATE_PCT, 300);
  SET_OR_RETURN_FALSE(AV1E_SET_COEFF_COST_UPD_FREQ, 3);
  SET_OR_RETURN_FALSE(AV1E_SET_MODE_COST_UPD_FREQ, 3);
  SET_OR_RETURN_FALSE(AV1E_SET_MV_COST_UPD_FREQ, 3);
  SET_OR_RETURN_FALSE(AV1E_SET_ROW_MT, 1);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_OBMC, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_NOISE_SENSITIVITY, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_WARPED_MOTION, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_REF_FRAME_MVS, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_CFL_INTRA, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_SMOOTH_INTRA, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_ANGLE_DELTA, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_FILTER_INTRA, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_INTRA_DEFAULT_TX_ONLY, 1);
  SET_OR_RETURN_FALSE(AV1E_SET_DISABLE_TRELLIS_QUANT, 1);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_DIST_WTD_COMP, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_DIFF_WTD_COMP, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_DUAL_FILTER, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_INTERINTRA_COMP, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_INTERINTRA_WEDGE, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_INTRA_EDGE_FILTER, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_INTRABC, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_MASKED_COMP, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_PAETH_INTRA, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_QM, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_RECT_PARTITIONS, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_RESTORATION, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_SMOOTH_INTERINTRA, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_ENABLE_TX64, 0);
  SET_OR_RETURN_FALSE(AV1E_SET_MAX_REFERENCE_FRAMES, 3);

  return true;
}

struct ThreadTilesAndSuperblockSizeInfo {
  int num_threads;
  int exp_tile_rows;
  int exp_tile_colums;
  aom_superblock_size_t superblock_size;
};

ThreadTilesAndSuperblockSizeInfo GetThreadingTilesAndSuperblockSize(
    int width,
    int height,
    int max_number_of_threads) {
  ThreadTilesAndSuperblockSizeInfo res;
  const int num_pixels = width * height;
  if (num_pixels >= 1920 * 1080 && max_number_of_threads > 8) {
    res.num_threads = 8;
    res.exp_tile_rows = 2;
    res.exp_tile_colums = 1;
  } else if (num_pixels >= 640 * 360 && max_number_of_threads > 4) {
    res.num_threads = 4;
    res.exp_tile_rows = 1;
    res.exp_tile_colums = 1;
  } else if (num_pixels >= 320 * 180 && max_number_of_threads > 2) {
    res.num_threads = 2;
    res.exp_tile_rows = 1;
    res.exp_tile_colums = 0;
  } else {
    res.num_threads = 1;
    res.exp_tile_rows = 0;
    res.exp_tile_colums = 0;
  }

  if (res.num_threads > 4 && num_pixels >= 960 * 540) {
    res.superblock_size = AOM_SUPERBLOCK_SIZE_64X64;
  } else {
    res.superblock_size = AOM_SUPERBLOCK_SIZE_DYNAMIC;
  }

  RTC_LOG(LS_WARNING) << __FUNCTION__ << " res.num_threads=" << res.num_threads
                      << " res.exp_tile_rows=" << res.exp_tile_rows
                      << " res.exp_tile_colums=" << res.exp_tile_colums
                      << " res.superblock_size=" << res.superblock_size;

  return res;
}

bool ValidateEncodeParams(
    const webrtc::VideoFrameBuffer& /* frame_buffer */,
    const VideoEncoderInterface::TemporalUnitSettings& /* tu_settings */,
    const std::vector<VideoEncoderInterface::FrameEncodeSettings>&
        frame_settings,
    const std::array<std::optional<Resolution>, 8>& last_resolution_in_buffer,
    aom_rc_mode rc_mode) {
  if (frame_settings.empty()) {
    RTC_LOG(LS_ERROR) << "No frame settings provided.";
    return false;
  }

  auto in_range = [](int low, int high, int val) {
    return low <= val && val < high;
  };

  for (size_t i = 0; i < frame_settings.size(); ++i) {
    const VideoEncoderInterface::FrameEncodeSettings& settings =
        frame_settings[i];

    if (!settings.frame_output) {
      RTC_LOG(LS_ERROR) << "No frame output provided.";
      return false;
    }

    if (!in_range(kMinEffortLevel, kMaxEffortLevel + 1,
                  settings.effort_level)) {
      RTC_LOG(LS_ERROR) << "Unsupported effort level " << settings.effort_level;
      return false;
    }

    if (!in_range(0, kMaxSpatialLayersWtf, settings.spatial_id)) {
      RTC_LOG(LS_ERROR) << "invalid spatial id " << settings.spatial_id;
      return false;
    }

    if (!in_range(0, kMaxTemporalLayers, settings.temporal_id)) {
      RTC_LOG(LS_ERROR) << "invalid temporal id " << settings.temporal_id;
      return false;
    }

    if ((settings.frame_type == FrameType::kKeyframe ||
         settings.frame_type == FrameType::kStartFrame) &&
        !settings.reference_buffers.empty()) {
      RTC_LOG(LS_ERROR) << "Reference buffers can not be used for keyframes.";
      return false;
    }

    if ((settings.frame_type == FrameType::kKeyframe ||
         settings.frame_type == FrameType::kStartFrame) &&
        !settings.update_buffer) {
      RTC_LOG(LS_ERROR)
          << "Buffer to update must be specified for keyframe/startframe";
      return false;
    }

    if (settings.update_buffer &&
        !in_range(0, kNumBuffers, *settings.update_buffer)) {
      RTC_LOG(LS_ERROR) << "Invalid update buffer id.";
      return false;
    }

    if (settings.reference_buffers.size() > kMaxReferences) {
      RTC_LOG(LS_ERROR) << "Too many referenced buffers.";
      return false;
    }

    for (size_t j = 0; j < settings.reference_buffers.size(); ++j) {
      if (!in_range(0, kNumBuffers, settings.reference_buffers[j])) {
        RTC_LOG(LS_ERROR) << "Invalid reference buffer id.";
        return false;
      }

      // Figure out which frame resolution a certain buffer will hold when the
      // frame described by `settings` is encoded.
      std::optional<Resolution> referenced_resolution;
      bool keyframe_on_previous_layer = false;

      // Will some other frame in this temporal unit update the buffer?
      for (size_t k = 0; k < i; ++k) {
        if (frame_settings[k].frame_type == FrameType::kKeyframe) {
          keyframe_on_previous_layer = true;
          referenced_resolution.reset();
        }
        if (frame_settings[k].update_buffer == settings.reference_buffers[j]) {
          referenced_resolution = frame_settings[k].resolution;
        }
      }

      // Not updated by another frame in the temporal unit, what is the
      // resolution of the last frame stored into that buffer?
      if (!referenced_resolution && !keyframe_on_previous_layer) {
        referenced_resolution =
            last_resolution_in_buffer[settings.reference_buffers[j]];
      }

      if (!referenced_resolution) {
        RTC_LOG(LS_ERROR) << "Referenced buffer holds no frame.";
        return false;
      }

      if (!GetScalingFactor(*referenced_resolution, settings.resolution)) {
        RTC_LOG(LS_ERROR)
            << "Required resolution scaling factor not supported.";
        return false;
      }

      for (size_t l = i + 1; l < settings.reference_buffers.size(); ++l) {
        if (settings.reference_buffers[i] == settings.reference_buffers[l]) {
          RTC_LOG(LS_ERROR) << "Duplicate reference buffer specified.";
          return false;
        }
      }
    }

    if ((rc_mode == AOM_CBR &&
         absl::holds_alternative<Cqp>(settings.rate_options)) ||
        (rc_mode == AOM_Q &&
         absl::holds_alternative<Cbr>(settings.rate_options))) {
      RTC_LOG(LS_ERROR) << "Invalid rate options, encoder configured with "
                        << (rc_mode == AOM_CBR ? "AOM_CBR" : "AOM_Q");
      return false;
    }

    for (size_t j = i + 1; j < frame_settings.size(); ++j) {
      if (settings.spatial_id >= frame_settings[j].spatial_id) {
        RTC_LOG(LS_ERROR) << "Frame spatial id specified out of order.";
        return false;
      }
    }
  }

  return true;
}

void PrepareInputImage(const VideoFrameBuffer& input_buffer,
                       aom_img_ptr& out_aom_image) {
  aom_img_fmt_t input_format;
  switch (input_buffer.type()) {
    case VideoFrameBuffer::Type::kI420:
      input_format = AOM_IMG_FMT_I420;
      break;
    case VideoFrameBuffer::Type::kNV12:
      input_format = AOM_IMG_FMT_NV12;
      break;
    default:
      RTC_CHECK_NOTREACHED();
      return;
  }

  if (!out_aom_image || out_aom_image->fmt != input_format ||
      static_cast<int>(out_aom_image->w) != input_buffer.width() ||
      static_cast<int>(out_aom_image->h) != input_buffer.height()) {
    out_aom_image.reset(
        aom_img_wrap(/*img=*/nullptr, input_format, input_buffer.width(),
                     input_buffer.height(), /*align=*/1, /*img_data=*/nullptr));

    RTC_LOG(LS_WARNING) << __FUNCTION__ << " input_format=" << input_format
                        << " input_buffer.width()=" << input_buffer.width()
                        << " input_buffer.height()=" << input_buffer.height()
                        << " w=" << out_aom_image->w
                        << " h=" << out_aom_image->h
                        << " d_w=" << out_aom_image->d_w
                        << " d_h=" << out_aom_image->d_h
                        << " r_w=" << out_aom_image->r_w
                        << " r_h=" << out_aom_image->r_h;
  }

  if (input_format == AOM_IMG_FMT_I420) {
    const I420BufferInterface* i420_buffer = input_buffer.GetI420();
    RTC_DCHECK(i420_buffer);
    out_aom_image->planes[AOM_PLANE_Y] =
        const_cast<unsigned char*>(i420_buffer->DataY());
    out_aom_image->planes[AOM_PLANE_U] =
        const_cast<unsigned char*>(i420_buffer->DataU());
    out_aom_image->planes[AOM_PLANE_V] =
        const_cast<unsigned char*>(i420_buffer->DataV());
    out_aom_image->stride[AOM_PLANE_Y] = i420_buffer->StrideY();
    out_aom_image->stride[AOM_PLANE_U] = i420_buffer->StrideU();
    out_aom_image->stride[AOM_PLANE_V] = i420_buffer->StrideV();
  } else {
    const NV12BufferInterface* nv12_buffer = input_buffer.GetNV12();
    RTC_DCHECK(nv12_buffer);
    out_aom_image->planes[AOM_PLANE_Y] =
        const_cast<unsigned char*>(nv12_buffer->DataY());
    out_aom_image->planes[AOM_PLANE_U] =
        const_cast<unsigned char*>(nv12_buffer->DataUV());
    out_aom_image->planes[AOM_PLANE_V] = nullptr;
    out_aom_image->stride[AOM_PLANE_Y] = nv12_buffer->StrideY();
    out_aom_image->stride[AOM_PLANE_U] = nv12_buffer->StrideUV();
    out_aom_image->stride[AOM_PLANE_V] = 0;
  }
}

aom_svc_ref_frame_config_t GetSvcRefFrameConfig(
    const VideoEncoderInterface::FrameEncodeSettings& settings) {
  // Buffer alias to use for each position. In particular when there are two
  // buffers being used, prefer to alias them as LAST and GOLDEN, since the AV1
  // bitstream format has dedicated fields for them. See last_frame_idx and
  // golden_frame_idx in the av1 spec
  // https://aomediacodec.github.io/av1-spec/av1-spec.pdf.

  // Libaom is also compiled for RTC, which limits the number of references to
  // at most three, and they must be aliased as LAST, GOLDEN and ALTREF. Also
  // note that libaom favors LAST the most, and GOLDEN second most, so buffers
  // should be specified in order of how useful they are for prediction. Libaom
  // could be updated to make LAST, GOLDEN and ALTREF equivalent, but that is
  // not a priority for now. All aliases can be used to update buffers.
  // TD: Automatically select LAST, GOLDEN and ALTREF depending on previous
  //       buffer usage.
  static constexpr int kPreferedAlias[] = {0,  // LAST
                                           3,  // GOLDEN
                                           6,  // ALTREF
                                           1, 2, 4, 5};

  aom_svc_ref_frame_config_t ref_frame_config = {};

  int alias_index = 0;
  if (!settings.reference_buffers.empty()) {
    for (size_t i = 0; i < settings.reference_buffers.size(); ++i) {
      ref_frame_config.ref_idx[kPreferedAlias[alias_index]] =
          settings.reference_buffers[i];
      ref_frame_config.reference[kPreferedAlias[alias_index]] = 1;
      alias_index++;
    }

    // Delta frames must not alias unused buffers, and since start frames only
    // update some buffers it is not safe to leave unused aliases to simply
    // point to buffer 0.
    for (size_t i = settings.reference_buffers.size();
         i < std::size(ref_frame_config.ref_idx); ++i) {
      ref_frame_config.ref_idx[kPreferedAlias[i]] =
          settings.reference_buffers.back();
    }
  }

  if (settings.update_buffer) {
    if (!absl::c_linear_search(settings.reference_buffers,
                               *settings.update_buffer)) {
      ref_frame_config.ref_idx[kPreferedAlias[alias_index]] =
          *settings.update_buffer;
      alias_index++;
    }
    ref_frame_config.refresh[*settings.update_buffer] = 1;
  }

  char buf[256];
  rtc::SimpleStringBuilder sb(buf);
  sb << " spatial_id=" << settings.spatial_id;
  sb << "  ref_idx=[ ";
  for (auto r : ref_frame_config.ref_idx) {
    sb << r << " ";
  }
  sb << "]  reference=[ ";
  for (auto r : ref_frame_config.reference) {
    sb << r << " ";
  }
  sb << "]  refresh=[ ";
  for (auto r : ref_frame_config.refresh) {
    sb << r << " ";
  }
  sb << "]";

  RTC_LOG(LS_WARNING) << __FUNCTION__ << sb.str();

  return ref_frame_config;
}

aom_svc_params_t GetSvcParams(
    const webrtc::VideoFrameBuffer& frame_buffer,
    const std::vector<VideoEncoderInterface::FrameEncodeSettings>&
        frame_settings) {
  aom_svc_params_t svc_params = {};
  svc_params.number_spatial_layers = frame_settings.back().spatial_id + 1;
  svc_params.number_temporal_layers = kMaxTemporalLayers;

  // TD: What about svc_params.framerate_factor?
  // If `framerate_factors` are left at 0 then configured bitrate values will
  // not be picked up by libaom.
  for (int tid = 0; tid < svc_params.number_temporal_layers; ++tid) {
    svc_params.framerate_factor[tid] = 1;
  }

  // If the scaling factor is left at zero for unused layers a division by zero
  // will happen inside libaom, default all layers to one.
  for (int sid = 0; sid < svc_params.number_spatial_layers; ++sid) {
    svc_params.scaling_factor_num[sid] = 1;
    svc_params.scaling_factor_den[sid] = 1;
  }

  for (const VideoEncoderInterface::FrameEncodeSettings& settings :
       frame_settings) {
    std::optional<Rational> scaling_factor = GetScalingFactor(
        {frame_buffer.width(), frame_buffer.height()}, settings.resolution);
    RTC_CHECK(scaling_factor);
    svc_params.scaling_factor_num[settings.spatial_id] =
        scaling_factor->numerator;
    svc_params.scaling_factor_den[settings.spatial_id] =
        scaling_factor->denominator;

    const int flat_layer_id =
        settings.spatial_id * svc_params.number_temporal_layers +
        settings.temporal_id;

    RTC_LOG(LS_WARNING) << __FUNCTION__ << " flat_layer_id=" << flat_layer_id
                        << " num="
                        << svc_params.scaling_factor_num[settings.spatial_id]
                        << " den="
                        << svc_params.scaling_factor_den[settings.spatial_id];

    absl::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, Cbr>) {
            // Libaom calculates the total bitrate across all spatial layers by
            // summing the bitrate of the last temporal layer in each spatial
            // layer. This means the bitrate for the top temporal layer always
            // has to be set even if that temporal layer is not being encoded.
            const int last_temporal_layer_in_spatial_layer_id =
                settings.spatial_id * svc_params.number_temporal_layers +
                (kMaxTemporalLayers - 1);
            svc_params
                .layer_target_bitrate[last_temporal_layer_in_spatial_layer_id] =
                arg.target_bitrate.kbps();

            svc_params.layer_target_bitrate[flat_layer_id] =
                arg.target_bitrate.kbps();
            // When libaom is configured with `AOM_CBR` it will still limit QP
            // to stay between `min_quantizers` and `max_quantizers'. Set
            // `max_quantizers` to max QP to avoid the encoder overshooting.
            svc_params.max_quantizers[flat_layer_id] = kMaxQp;
            svc_params.min_quantizers[flat_layer_id] = 0;
          } else if constexpr (std::is_same_v<T, Cqp>) {
            // When libaom is configured with `AOM_Q` it will still look at the
            // `layer_target_bitrate` to determine whether the layer is disabled
            // or not. Set `layer_target_bitrate` to 1 so that libaom knows the
            // layer is active.
            svc_params.layer_target_bitrate[flat_layer_id] = 1;
            svc_params.max_quantizers[flat_layer_id] = arg.target_qp;
            svc_params.min_quantizers[flat_layer_id] = arg.target_qp;
            RTC_LOG(LS_WARNING) << __FUNCTION__ << " svc_params.qp["
                                << flat_layer_id << "]=" << arg.target_qp;
            // TD: Does libaom look at both max and min? Shouldn't it just be
            //       one of them
          }
        },
        settings.rate_options);
  }

  char buf[512];
  rtc::SimpleStringBuilder sb(buf);
  sb << "GetSvcParams" << " layer bitrates kbps";
  for (int s = 0; s < svc_params.number_spatial_layers; ++s) {
    sb << " S" << s << "=[ ";
    for (int t = 0; t < svc_params.number_temporal_layers; ++t) {
      int id = s * svc_params.number_temporal_layers + t;
      sb << "T" << t << "=" << svc_params.layer_target_bitrate[id] << " ";
    }
    sb << "]";
  }

  RTC_LOG(LS_WARNING) << sb.str();

  return svc_params;
}

void LibaomAv1Encoder::Encode(
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer,
    const TemporalUnitSettings& tu_settings,
    std::vector<FrameEncodeSettings> frame_settings) {
  absl::Cleanup on_return = [&] {
    // On return call `EncodeComplete` with EncodingError result unless they
    // were already called with an EncodedData result.
    for (FrameEncodeSettings& settings : frame_settings) {
      if (settings.frame_output) {
        settings.frame_output->EncodeComplete(EncodingError());
      }
    }
  };

  if (!ValidateEncodeParams(*frame_buffer, tu_settings, frame_settings,
                            last_resolution_in_buffer_, cfg_.rc_end_usage)) {
    return;
  }

  if (current_content_type_ != tu_settings.content_hint) {
    if (tu_settings.content_hint == VideoCodecMode::kScreensharing) {
      // TD: Set speed 11?
      SET_OR_RETURN(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN);
      SET_OR_RETURN(AV1E_SET_ENABLE_PALETTE, 1);
    } else {
      SET_OR_RETURN(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_DEFAULT);
      SET_OR_RETURN(AV1E_SET_ENABLE_PALETTE, 0);
    }
    current_content_type_ = tu_settings.content_hint;
  }

  if (cfg_.rc_end_usage == AOM_CBR) {
    DataRate accum_rate = DataRate::Zero();
    for (const FrameEncodeSettings& settings : frame_settings) {
      accum_rate += absl::get<Cbr>(settings.rate_options).target_bitrate;
    }
    cfg_.rc_target_bitrate = accum_rate.kbps();
    RTC_LOG(LS_WARNING) << __FUNCTION__
                        << " cfg_.rc_target_bitrate=" << cfg_.rc_target_bitrate;
  }

  if (static_cast<int>(cfg_.g_w) != frame_buffer->width() ||
      static_cast<int>(cfg_.g_h) != frame_buffer->height()) {
    RTC_LOG(LS_WARNING) << __FUNCTION__ << " resolution changed from "
                        << cfg_.g_w << "x" << cfg_.g_h << " to "
                        << frame_buffer->width() << "x"
                        << frame_buffer->height();
    ThreadTilesAndSuperblockSizeInfo ttsbi = GetThreadingTilesAndSuperblockSize(
        frame_buffer->width(), frame_buffer->height(), max_number_of_threads_);
    SET_OR_RETURN(AV1E_SET_SUPERBLOCK_SIZE, ttsbi.superblock_size);
    SET_OR_RETURN(AV1E_SET_TILE_ROWS, ttsbi.exp_tile_rows);
    SET_OR_RETURN(AV1E_SET_TILE_COLUMNS, ttsbi.exp_tile_colums);
    cfg_.g_threads = ttsbi.num_threads;
    cfg_.g_w = frame_buffer->width();
    cfg_.g_h = frame_buffer->height();
  }

  PrepareInputImage(*frame_buffer, image_to_encode_);

  // The bitrates caluclated internally in libaom when `AV1E_SET_SVC_PARAMS` is
  // called depends on the currently configured `cfg_.rc_target_bitrate`. If the
  // total target bitrate is not updated first a division by zero could happen.
  if (aom_codec_err_t ret = aom_codec_enc_config_set(&ctx_, &cfg_);
      ret != AOM_CODEC_OK) {
    RTC_LOG(LS_ERROR) << "aom_codec_enc_config_set returned " << ret;
    return;
  }
  aom_svc_params_t svc_params = GetSvcParams(*frame_buffer, frame_settings);
  SET_OR_RETURN(AV1E_SET_SVC_PARAMS, &svc_params);

  // The libaom AV1 encoder requires that `aom_codec_encode` is called for
  // every spatial layer, even if no frame should be encoded for that layer.
  std::array<FrameEncodeSettings*, kMaxSpatialLayersWtf>
      settings_for_spatial_id;
  settings_for_spatial_id.fill(nullptr);
  FrameEncodeSettings settings_for_unused_layer;
  for (FrameEncodeSettings& settings : frame_settings) {
    settings_for_spatial_id[settings.spatial_id] = &settings;
  }

  for (int sid = frame_settings[0].spatial_id;
       sid < svc_params.number_spatial_layers; ++sid) {
    const bool layer_enabled = settings_for_spatial_id[sid] != nullptr;
    FrameEncodeSettings& settings = layer_enabled
                                        ? *settings_for_spatial_id[sid]
                                        : settings_for_unused_layer;

    aom_svc_layer_id_t layer_id = {
        .spatial_layer_id = sid,
        .temporal_layer_id = settings.temporal_id,
    };
    SET_OR_RETURN(AV1E_SET_SVC_LAYER_ID, &layer_id);
    aom_svc_ref_frame_config_t ref_config = GetSvcRefFrameConfig(settings);
    SET_OR_RETURN(AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_config);

    // TD: Duration can't be zero, what does it matter when the layer is
    // not being encoded?
    TimeDelta duration = TimeDelta::Millis(1);
    if (layer_enabled) {
      if (const Cbr* cbr = absl::get_if<Cbr>(&settings.rate_options)) {
        duration = cbr->duration;
      } else {
        // TD: What should duration be when Cqp is used?
        duration = TimeDelta::Millis(1);
      }

      if (settings.effort_level != current_effort_level_[settings.spatial_id]) {
        // For RTC we use speed level 6 to 10, with 8 being the default. Note
        // that low effort means higher speed.
        SET_OR_RETURN(AOME_SET_CPUUSED, 8 - settings.effort_level);
        current_effort_level_[settings.spatial_id] = settings.effort_level;
      }
    }

    RTC_LOG(LS_WARNING)
        << __FUNCTION__ << " timestamp="
        << (tu_settings.presentation_timestamp.ms() * kRtpTicksPerSecond / 1000)
        << "  duration=" << (duration.ms() * kRtpTicksPerSecond / 1000)
        << "  type="
        << (settings.frame_type == FrameType::kKeyframe ? "key" : "delta");
    aom_codec_err_t ret = aom_codec_encode(
        &ctx_, &*image_to_encode_, tu_settings.presentation_timestamp.ms() * 90,
        duration.ms() * 90,
        settings.frame_type == FrameType::kKeyframe ? AOM_EFLAG_FORCE_KF : 0);
    if (ret != AOM_CODEC_OK) {
      RTC_LOG(LS_WARNING) << "aom_codec_encode returned " << ret;
      return;
    }

    if (!layer_enabled) {
      continue;
    }

    if (settings.frame_type == FrameType::kKeyframe) {
      last_resolution_in_buffer_ = {};
    }

    if (settings.update_buffer) {
      last_resolution_in_buffer_[*settings.update_buffer] = settings.resolution;
    }

    EncodedData result;
    aom_codec_iter_t iter = nullptr;
    bool bitstream_produced = false;
    while (const aom_codec_cx_pkt_t* pkt =
               aom_codec_get_cx_data(&ctx_, &iter)) {
      if (pkt->kind == AOM_CODEC_CX_FRAME_PKT && pkt->data.frame.sz > 0) {
        SET_OR_RETURN(AOME_GET_LAST_QUANTIZER_64, &result.encoded_qp);
        result.frame_type = pkt->data.frame.flags & AOM_EFLAG_FORCE_KF
                                ? FrameType::kKeyframe
                                : FrameType::kDeltaFrame;
        rtc::ArrayView<uint8_t> output_buffer =
            settings.frame_output->GetBitstreamOutputBuffer(
                DataSize::Bytes(pkt->data.frame.sz));
        if (output_buffer.size() != pkt->data.frame.sz) {
          return;
        }
        memcpy(output_buffer.data(), pkt->data.frame.buf, pkt->data.frame.sz);
        bitstream_produced = true;
        break;
      }
    }

    if (!bitstream_produced) {
      return;
    } else {
      RTC_CHECK(settings.frame_output);
      settings.frame_output->EncodeComplete(result);
      // To avoid invoking any callback more than once.
      settings.frame_output = nullptr;
    }
  }
}
}  // namespace

std::string LibaomAv1EncoderFactory::CodecName() const {
  return "AV1";
}

std::string LibaomAv1EncoderFactory::ImplementationName() const {
  return "Libaom";
}

std::map<std::string, std::string> LibaomAv1EncoderFactory::CodecSpecifics()
    const {
  return {};
}

// clang-format off
// The formater and cpplint have conflicting ideas.
VideoEncoderFactoryInterface::Capabilities
LibaomAv1EncoderFactory::GetEncoderCapabilities() const {
  return {
      .prediction_constraints = {
           .num_buffers = kNumBuffers,
           .max_references = kMaxReferences,
           .max_temporal_layers = kMaxTemporalLayers,
           .buffer_space_type = VideoEncoderFactoryInterface::Capabilities::
               PredictionConstraints::BufferSpaceType::kSingleKeyframe,
           .max_spatial_layers = kMaxSpatialLayersWtf,
           .scaling_factors = {kSupportedScalingFactors.begin(),
                               kSupportedScalingFactors.end()},
           .supported_frame_types = {FrameType::kKeyframe,
                                     FrameType::kStartFrame,
                                     FrameType::kDeltaFrame}},
      .input_constraints = {
              .min = {.width = 64, .height = 36},
              .max = {.width = 3840, .height = 2160},
              .pixel_alignment = 1,
              .input_formats = {kSupportedInputFormats.begin(),
                                kSupportedInputFormats.end()},
          },
      .encoding_formats = {{.sub_sampling = EncodingFormat::k420,
                            .bit_depth = 8}},
      .rate_control = {
           .qp_range = {0, kMaxQp},
           .rc_modes = {VideoEncoderFactoryInterface::RateControlMode::kCbr,
                        VideoEncoderFactoryInterface::RateControlMode::kCqp}},
      .performance = {.encode_on_calling_thread = true,
                      .min_max_effort_level = {kMinEffortLevel,
                                               kMaxEffortLevel}},
  };
}
// clang-format on

std::unique_ptr<VideoEncoderInterface> LibaomAv1EncoderFactory::CreateEncoder(
    const StaticEncoderSettings& settings,
    const std::map<std::string, std::string>& encoder_specific_settings) {
  auto encoder = std::make_unique<LibaomAv1Encoder>();
  if (!encoder->InitEncode(settings, encoder_specific_settings)) {
    return nullptr;
  }
  return encoder;
}

}  // namespace webrtc
