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

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "vpx/vp8.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

namespace webrtc {
namespace {
constexpr int kVp8ErrorPropagationTh = 30;
// vpx_decoder.h documentation indicates decode deadline is time in us, with
// "Set to zero for unlimited.", but actual implementation requires this to be
// a mode with 0 meaning allow delay and 1 not allowing it.
constexpr long kDecodeDeadlineRealtime = 1;  // NOLINT

const char kVp8PostProcArmFieldTrial[] = "WebRTC-VP8-Postproc-Config-Arm";

void GetPostProcParamsFromFieldTrialGroup(
    LibvpxVp8Decoder::DeblockParams* deblock_params) {
  std::string group =
      webrtc::field_trial::FindFullName(kVp8PostProcArmFieldTrial);
  if (group.empty())
    return;

  LibvpxVp8Decoder::DeblockParams params;
  if (sscanf(group.c_str(), "Enabled-%d,%d,%d", &params.max_level,
             &params.min_qp, &params.degrade_qp) != 3)
    return;

  if (params.max_level < 0 || params.max_level > 16)
    return;

  if (params.min_qp < 0 || params.degrade_qp <= params.min_qp)
    return;

  *deblock_params = params;
}

}  // namespace

std::unique_ptr<VideoDecoder> VP8Decoder::Create() {
  return absl::make_unique<LibvpxVp8Decoder>();
}

class LibvpxVp8Decoder::QpSmoother {
 public:
  QpSmoother() : last_sample_ms_(rtc::TimeMillis()), smoother_(kAlpha) {}

  int GetAvg() const {
    float value = smoother_.filtered();
    return (value == rtc::ExpFilter::kValueUndefined) ? 0
                                                      : static_cast<int>(value);
  }

  void Add(float sample) {
    int64_t now_ms = rtc::TimeMillis();
    smoother_.Apply(static_cast<float>(now_ms - last_sample_ms_), sample);
    last_sample_ms_ = now_ms;
  }

  void Reset() { smoother_.Reset(kAlpha); }

 private:
  const float kAlpha = 0.95f;
  int64_t last_sample_ms_;
  rtc::ExpFilter smoother_;
};

LibvpxVp8Decoder::LibvpxVp8Decoder()
    : use_postproc_arm_(
          webrtc::field_trial::IsEnabled(kVp8PostProcArmFieldTrial)),
      buffer_pool_(false, 300 /* max_number_of_buffers*/),
      decode_complete_callback_(NULL),
      inited_(false),
      decoder_(NULL),
      propagation_cnt_(-1),
      last_frame_width_(0),
      last_frame_height_(0),
      key_frame_required_(true),
      qp_smoother_(use_postproc_arm_ ? new QpSmoother() : nullptr) {
  if (use_postproc_arm_)
    GetPostProcParamsFromFieldTrialGroup(&deblock_);
}

LibvpxVp8Decoder::~LibvpxVp8Decoder() {
  inited_ = true;  // in order to do the actual release
  Release();
}

int LibvpxVp8Decoder::InitDecode(const VideoCodec* inst, int number_of_cores) {
  int ret_val = Release();
  if (ret_val < 0) {
    return ret_val;
  }
  if (decoder_ == NULL) {
    decoder_ = new vpx_codec_ctx_t;
    memset(decoder_, 0, sizeof(*decoder_));
  }
  vpx_codec_dec_cfg_t cfg;
  // Setting number of threads to a constant value (1)
  cfg.threads = 1;
  cfg.h = cfg.w = 0;  // set after decode

#if defined(WEBRTC_ARCH_ARM) || defined(WEBRTC_ARCH_ARM64) || \
    defined(WEBRTC_ANDROID)
  vpx_codec_flags_t flags = use_postproc_arm_ ? VPX_CODEC_USE_POSTPROC : 0;
#else
  vpx_codec_flags_t flags = VPX_CODEC_USE_POSTPROC;
#endif

  if (vpx_codec_dec_init(decoder_, vpx_codec_vp8_dx(), &cfg, flags)) {
    delete decoder_;
    decoder_ = nullptr;
    return WEBRTC_VIDEO_CODEC_MEMORY;
  }

  propagation_cnt_ = -1;
  inited_ = true;

  // Always start with a complete key frame.
  key_frame_required_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

int LibvpxVp8Decoder::Decode(const EncodedImage& input_image,
                             bool missing_frames,
                             int64_t /*render_time_ms*/) {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (decode_complete_callback_ == NULL) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (input_image.data() == NULL && input_image.size() > 0) {
    // Reset to avoid requesting key frames too often.
    if (propagation_cnt_ > 0)
      propagation_cnt_ = 0;
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

// Post process configurations.
#if defined(WEBRTC_ARCH_ARM) || defined(WEBRTC_ARCH_ARM64) || \
    defined(WEBRTC_ANDROID)
  if (use_postproc_arm_) {
    vp8_postproc_cfg_t ppcfg;
    ppcfg.post_proc_flag = VP8_MFQE;
    // For low resolutions, use stronger deblocking filter.
    int last_width_x_height = last_frame_width_ * last_frame_height_;
    if (last_width_x_height > 0 && last_width_x_height <= 320 * 240) {
      // Enable the deblock and demacroblocker based on qp thresholds.
      RTC_DCHECK(qp_smoother_);
      int qp = qp_smoother_->GetAvg();
      if (qp > deblock_.min_qp) {
        int level = deblock_.max_level;
        if (qp < deblock_.degrade_qp) {
          // Use lower level.
          level = deblock_.max_level * (qp - deblock_.min_qp) /
                  (deblock_.degrade_qp - deblock_.min_qp);
        }
        // Deblocking level only affects VP8_DEMACROBLOCK.
        ppcfg.deblocking_level = std::max(level, 1);
        ppcfg.post_proc_flag |= VP8_DEBLOCK | VP8_DEMACROBLOCK;
      }
    }
    vpx_codec_control(decoder_, VP8_SET_POSTPROC, &ppcfg);
  }
#else
  vp8_postproc_cfg_t ppcfg;
  // MFQE enabled to reduce key frame popping.
  ppcfg.post_proc_flag = VP8_MFQE | VP8_DEBLOCK;
  // For VGA resolutions and lower, enable the demacroblocker postproc.
  if (last_frame_width_ * last_frame_height_ <= 640 * 360) {
    ppcfg.post_proc_flag |= VP8_DEMACROBLOCK;
  }
  // Strength of deblocking filter. Valid range:[0,16]
  ppcfg.deblocking_level = 3;
  vpx_codec_control(decoder_, VP8_SET_POSTPROC, &ppcfg);
#endif

  // Always start with a complete key frame.
  if (key_frame_required_) {
    if (input_image._frameType != VideoFrameType::kVideoFrameKey)
      return WEBRTC_VIDEO_CODEC_ERROR;
    // We have a key frame - is it complete?
    if (input_image._completeFrame) {
      key_frame_required_ = false;
    } else {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  // Restrict error propagation using key frame requests.
  // Reset on a key frame refresh.
  if (input_image._frameType == VideoFrameType::kVideoFrameKey &&
      input_image._completeFrame) {
    propagation_cnt_ = -1;
    // Start count on first loss.
  } else if ((!input_image._completeFrame || missing_frames) &&
             propagation_cnt_ == -1) {
    propagation_cnt_ = 0;
  }
  if (propagation_cnt_ >= 0) {
    propagation_cnt_++;
  }

  vpx_codec_iter_t iter = NULL;
  vpx_image_t* img;
  int ret;

  // Check for missing frames.
  if (missing_frames) {
    // Call decoder with zero data length to signal missing frames.
    if (vpx_codec_decode(decoder_, NULL, 0, 0, kDecodeDeadlineRealtime)) {
      // Reset to avoid requesting key frames too often.
      if (propagation_cnt_ > 0)
        propagation_cnt_ = 0;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    img = vpx_codec_get_frame(decoder_, &iter);
    iter = NULL;
  }

  const uint8_t* buffer = input_image.data();
  if (input_image.size() == 0) {
    buffer = NULL;  // Triggers full frame concealment.
  }
  if (vpx_codec_decode(decoder_, buffer, input_image.size(), 0,
                       kDecodeDeadlineRealtime)) {
    // Reset to avoid requesting key frames too often.
    if (propagation_cnt_ > 0) {
      propagation_cnt_ = 0;
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  img = vpx_codec_get_frame(decoder_, &iter);
  int qp;
  vpx_codec_err_t vpx_ret =
      vpx_codec_control(decoder_, VPXD_GET_LAST_QUANTIZER, &qp);
  RTC_DCHECK_EQ(vpx_ret, VPX_CODEC_OK);
  ret = ReturnFrame(img, input_image.Timestamp(), qp, input_image.ColorSpace());
  if (ret != 0) {
    // Reset to avoid requesting key frames too often.
    if (ret < 0 && propagation_cnt_ > 0)
      propagation_cnt_ = 0;
    return ret;
  }
  // Check Vs. threshold
  if (propagation_cnt_ > kVp8ErrorPropagationTh) {
    // Reset to avoid requesting key frames too often.
    propagation_cnt_ = 0;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int LibvpxVp8Decoder::ReturnFrame(
    const vpx_image_t* img,
    uint32_t timestamp,
    int qp,
    const webrtc::ColorSpace* explicit_color_space) {
  if (img == NULL) {
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
  rtc::scoped_refptr<I420Buffer> buffer =
      buffer_pool_.CreateBuffer(img->d_w, img->d_h);
  if (!buffer.get()) {
    // Pool has too many pending frames.
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Video.LibvpxVp8Decoder.TooManyPendingFrames",
                          1);
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }

  libyuv::I420Copy(img->planes[VPX_PLANE_Y], img->stride[VPX_PLANE_Y],
                   img->planes[VPX_PLANE_U], img->stride[VPX_PLANE_U],
                   img->planes[VPX_PLANE_V], img->stride[VPX_PLANE_V],
                   buffer->MutableDataY(), buffer->StrideY(),
                   buffer->MutableDataU(), buffer->StrideU(),
                   buffer->MutableDataV(), buffer->StrideV(), img->d_w,
                   img->d_h);

  VideoFrame decoded_image = VideoFrame::Builder()
                                 .set_video_frame_buffer(buffer)
                                 .set_timestamp_rtp(timestamp)
                                 .set_color_space(explicit_color_space)
                                 .build();
  decode_complete_callback_->Decoded(decoded_image, absl::nullopt, qp);

  return WEBRTC_VIDEO_CODEC_OK;
}

int LibvpxVp8Decoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int LibvpxVp8Decoder::Release() {
  int ret_val = WEBRTC_VIDEO_CODEC_OK;

  if (decoder_ != NULL) {
    if (inited_) {
      if (vpx_codec_destroy(decoder_)) {
        ret_val = WEBRTC_VIDEO_CODEC_MEMORY;
      }
    }
    delete decoder_;
    decoder_ = NULL;
  }
  buffer_pool_.Release();
  inited_ = false;
  return ret_val;
}

const char* LibvpxVp8Decoder::ImplementationName() const {
  return "libvpx";
}
}  // namespace webrtc
