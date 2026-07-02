/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/libvpx_vp8_decoder.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/scoped_refptr.h"
#include "api/units/timestamp.h"
#include "api/video/color_space.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/exp_filter.h"
#include "system_wrappers/include/metrics.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_image.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace webrtc {
namespace {
// vpx_decoder.h documentation indicates decode deadline is time in us, with
// "Set to zero for unlimited.", but actual implementation requires this to be
// a mode with 0 meaning allow delay and 1 not allowing it.
constexpr long kDecodeDeadlineRealtime = 1;  // NOLINT

const char kVp8PostProcArmFieldTrial[] = "WebRTC-VP8-Postproc-Config-Arm";
const char kVp8PostProcFieldTrial[] = "WebRTC-VP8-Postproc-Config";

#if defined(WEBRTC_ARCH_ARM) || defined(WEBRTC_ARCH_ARM64) || \
    defined(WEBRTC_ANDROID)
constexpr bool kIsArm = true;
#else
constexpr bool kIsArm = false;
#endif

std::optional<LibvpxVp8Decoder::DeblockParams> DefaultDeblockParams() {
  return LibvpxVp8Decoder::DeblockParams(/*max_level=*/8,
                                         /*degrade_qp=*/60,
                                         /*min_qp=*/30);
}

std::optional<LibvpxVp8Decoder::DeblockParams>
GetPostProcParamsFromFieldTrialGroup(const FieldTrialsView& field_trials) {
  std::string group = field_trials.Lookup(kIsArm ? kVp8PostProcArmFieldTrial
                                                 : kVp8PostProcFieldTrial);
  if (group.empty()) {
    return DefaultDeblockParams();
  }

  LibvpxVp8Decoder::DeblockParams params;
  if (sscanf(group.c_str(), "Enabled-%d,%d,%d", &params.max_level,
             &params.min_qp, &params.degrade_qp) != 3) {
    return DefaultDeblockParams();
  }

  if (params.max_level < 0 || params.max_level > 16) {
    return DefaultDeblockParams();
  }

  if (params.min_qp < 0 || params.degrade_qp <= params.min_qp) {
    return DefaultDeblockParams();
  }

  return params;
}

}  // namespace

std::unique_ptr<VideoDecoder> CreateVp8Decoder(const Environment& env) {
  return std::make_unique<LibvpxVp8Decoder>(env);
}

class LibvpxVp8Decoder::QpSmoother {
 public:
  explicit QpSmoother(const Environment& env)
      : env_(env),
        last_sample_(env_.clock().CurrentTime()),
        smoother_(kAlpha) {}

  int GetAvg() const {
    float value = smoother_.filtered();
    return (value == ExpFilter::kValueUndefined) ? 0 : static_cast<int>(value);
  }

  void Add(float sample) {
    Timestamp now = env_.clock().CurrentTime();
    smoother_.Apply((now - last_sample_).ms<float>(), sample);
    last_sample_ = now;
  }

  void Reset() { smoother_.Reset(kAlpha); }

 private:
  static constexpr float kAlpha = 0.95f;

  const Environment env_;
  Timestamp last_sample_;
  ExpFilter smoother_;
};

LibvpxVp8Decoder::LibvpxVp8Decoder(const Environment& env)
    : use_postproc_(
          kIsArm ? env.field_trials().IsEnabled(kVp8PostProcArmFieldTrial)
                 : true),
      buffer_pool_(false, 300 /* max_number_of_buffers*/),
      decode_complete_callback_(nullptr),
      inited_(false),
      decoder_(nullptr),
      last_frame_width_(0),
      last_frame_height_(0),
      key_frame_required_(true),
      deblock_params_(use_postproc_ ? GetPostProcParamsFromFieldTrialGroup(
                                          env.field_trials())
                                    : std::nullopt),
      qp_smoother_(use_postproc_ ? std::make_unique<QpSmoother>(env)
                                 : nullptr) {}

LibvpxVp8Decoder::~LibvpxVp8Decoder() {
  inited_ = true;  // in order to do the actual release
  Release();
}

bool LibvpxVp8Decoder::Configure(const Settings& settings) {
  if (Release() < 0) {
    return false;
  }
  if (decoder_ == nullptr) {
    decoder_ = new vpx_codec_ctx_t;
    memset(decoder_, 0, sizeof(*decoder_));
  }
  vpx_codec_dec_cfg_t cfg;
  // Setting number of threads to a constant value (1)
  cfg.threads = 1;
  cfg.h = cfg.w = 0;  // set after decode

  vpx_codec_flags_t flags = use_postproc_ ? VPX_CODEC_USE_POSTPROC : 0;

  if (vpx_codec_dec_init(decoder_, vpx_codec_vp8_dx(), &cfg, flags)) {
    delete decoder_;
    decoder_ = nullptr;
    return false;
  }

  inited_ = true;

  // Always start with a complete key frame.
  key_frame_required_ = true;
  if (std::optional<int> buffer_pool_size = settings.buffer_pool_size()) {
    if (!buffer_pool_.Resize(*buffer_pool_size)) {
      return false;
    }
  }
  return true;
}

int LibvpxVp8Decoder::Decode(const EncodedImage& input_image,
                             int64_t render_time_ms) {
  return Decode(input_image, /*missing_frames=*/false, render_time_ms);
}

int LibvpxVp8Decoder::Decode(const EncodedImage& input_image,
                             bool /*missing_frames*/,
                             int64_t /*render_time_ms*/) {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (decode_complete_callback_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (input_image.data() == nullptr && input_image.size() > 0) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // Post process configurations.
  if (use_postproc_) {
    vp8_postproc_cfg_t ppcfg;
    // MFQE enabled to reduce key frame popping.
    ppcfg.post_proc_flag = VP8_MFQE;

    if (kIsArm) {
      RTC_DCHECK(deblock_params_.has_value());
    }
    if (deblock_params_.has_value()) {
      // For low resolutions, use stronger deblocking filter.
      int last_width_x_height = last_frame_width_ * last_frame_height_;
      if (last_width_x_height > 0 && last_width_x_height <= 320 * 240) {
        // Enable the deblock and demacroblocker based on qp thresholds.
        RTC_DCHECK(qp_smoother_);
        int qp = qp_smoother_->GetAvg();
        if (qp > deblock_params_->min_qp) {
          int level = deblock_params_->max_level;
          if (qp < deblock_params_->degrade_qp) {
            // Use lower level.
            level = deblock_params_->max_level *
                    (qp - deblock_params_->min_qp) /
                    (deblock_params_->degrade_qp - deblock_params_->min_qp);
          }
          // Deblocking level only affects VP8_DEMACROBLOCK.
          ppcfg.deblocking_level = std::max(level, 1);
          ppcfg.post_proc_flag |= VP8_DEBLOCK | VP8_DEMACROBLOCK;
        }
      }
    } else {
      // Non-arm with no explicit deblock params set.
      ppcfg.post_proc_flag |= VP8_DEBLOCK;
      // For VGA resolutions and lower, enable the demacroblocker postproc.
      if (last_frame_width_ * last_frame_height_ <= 640 * 360) {
        ppcfg.post_proc_flag |= VP8_DEMACROBLOCK;
      }
      // Strength of deblocking filter. Valid range:[0,16]
      ppcfg.deblocking_level = 3;
    }

    vpx_codec_control(decoder_, VP8_SET_POSTPROC, &ppcfg);
  }

  // Always start with a complete key frame.
  if (key_frame_required_) {
    if (!input_image.IsKey()) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    key_frame_required_ = false;
  }

  const uint8_t* buffer = input_image.data();
  if (input_image.size() == 0) {
    buffer = nullptr;  // Triggers full frame concealment.
  }
  if (vpx_codec_decode(decoder_, buffer, input_image.size(), nullptr,
                       kDecodeDeadlineRealtime)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  vpx_codec_iter_t iter = nullptr;
  vpx_image_t* img = vpx_codec_get_frame(decoder_, &iter);
  int qp;
  vpx_codec_err_t vpx_ret =
      vpx_codec_control(decoder_, VPXD_GET_LAST_QUANTIZER, &qp);
  RTC_DCHECK_EQ(vpx_ret, VPX_CODEC_OK);
  int ret = ReturnFrame(img, input_image.RtpTimestamp(), qp,
                        input_image.ColorSpace());
  if (ret != 0) {
    return ret;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int LibvpxVp8Decoder::ReturnFrame(const vpx_image_t* img,
                                  uint32_t timestamp,
                                  int qp,
                                  const ColorSpace* explicit_color_space) {
  if (img == nullptr) {
    // Decoder OK and NULL image => No show frame
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }
  if (qp_smoother_) {
    if (last_frame_width_ != static_cast<int>(img->d_w) ||
        last_frame_height_ != static_cast<int>(img->d_h)) {
      qp_smoother_->Reset();
    }
    qp_smoother_->Add(qp);
  }
  last_frame_width_ = img->d_w;
  last_frame_height_ = img->d_h;
  // Allocate memory for decoded image.
  scoped_refptr<VideoFrameBuffer> buffer;

  scoped_refptr<I420Buffer> i420_buffer =
      buffer_pool_.CreateI420Buffer(img->d_w, img->d_h);
  buffer = i420_buffer;
  if (i420_buffer) {
    libyuv::I420Copy(img->planes[VPX_PLANE_Y], img->stride[VPX_PLANE_Y],
                     img->planes[VPX_PLANE_U], img->stride[VPX_PLANE_U],
                     img->planes[VPX_PLANE_V], img->stride[VPX_PLANE_V],
                     i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                     i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                     i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                     img->d_w, img->d_h);
  }

  if (!buffer) {
    // Pool has too many pending frames.
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Video.LibvpxVp8Decoder.TooManyPendingFrames",
                          1);
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }

  VideoFrame decoded_image = VideoFrame::Builder()
                                 .set_video_frame_buffer(buffer)
                                 .set_rtp_timestamp(timestamp)
                                 .set_color_space(explicit_color_space)
                                 .build();
  decode_complete_callback_->Decoded(decoded_image, std::nullopt, qp);

  return WEBRTC_VIDEO_CODEC_OK;
}

int LibvpxVp8Decoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int LibvpxVp8Decoder::Release() {
  int ret_val = WEBRTC_VIDEO_CODEC_OK;

  if (decoder_ != nullptr) {
    if (inited_) {
      if (vpx_codec_destroy(decoder_)) {
        ret_val = WEBRTC_VIDEO_CODEC_MEMORY;
      }
    }
    delete decoder_;
    decoder_ = nullptr;
  }
  buffer_pool_.Release();
  inited_ = false;
  return ret_val;
}

VideoDecoder::DecoderInfo LibvpxVp8Decoder::GetDecoderInfo() const {
  DecoderInfo info;
  info.implementation_name = "libvpx";
  info.is_hardware_accelerated = false;
  return info;
}

const char* LibvpxVp8Decoder::ImplementationName() const {
  return "libvpx";
}
}  // namespace webrtc
