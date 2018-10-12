/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/codecs/vp8/libvpx_vp8_encoder.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/field_trial.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {
namespace {
const char kVp8GfBoostFieldTrial[] = "WebRTC-VP8-GfBoost";

// QP is obtained from VP8-bitstream for HW, so the QP corresponds to the
// bitstream range of [0, 127] and not the user-level range of [0,63].
constexpr int kLowVp8QpThreshold = 29;
constexpr int kHighVp8QpThreshold = 95;

constexpr int kTokenPartitions = VP8_ONE_TOKENPARTITION;
constexpr uint32_t kVp832ByteAlign = 32u;

// VP8 denoiser states.
enum denoiserState : uint32_t {
  kDenoiserOff,
  kDenoiserOnYOnly,
  kDenoiserOnYUV,
  kDenoiserOnYUVAggressive,
  // Adaptive mode defaults to kDenoiserOnYUV on key frame, but may switch
  // to kDenoiserOnYUVAggressive based on a computed noise metric.
  kDenoiserOnAdaptive
};

// Greatest common divisior
static int GCD(int a, int b) {
  int c = a % b;
  while (c != 0) {
    a = b;
    b = c;
    c = a % b;
  }
  return b;
}

bool GetGfBoostPercentageFromFieldTrialGroup(int* boost_percentage) {
  std::string group = webrtc::field_trial::FindFullName(kVp8GfBoostFieldTrial);
  if (group.empty())
    return false;

  if (sscanf(group.c_str(), "Enabled-%d", boost_percentage) != 1)
    return false;

  if (*boost_percentage < 0 || *boost_percentage > 100)
    return false;

  return true;
}

static_assert(Vp8EncoderConfig::kMaxPeriodicity == VPX_TS_MAX_PERIODICITY,
              "Vp8EncoderConfig::kMaxPeriodicity must be kept in sync with the "
              "constant in libvpx.");
static_assert(Vp8EncoderConfig::kMaxLayers == VPX_TS_MAX_LAYERS,
              "Vp8EncoderConfig::kMaxLayers must be kept in sync with the "
              "constant in libvpx.");

static Vp8EncoderConfig GetEncoderConfig(vpx_codec_enc_cfg* vpx_config) {
  Vp8EncoderConfig config;

  config.ts_number_layers = vpx_config->ts_number_layers;
  memcpy(config.ts_target_bitrate, vpx_config->ts_target_bitrate,
         sizeof(unsigned int) * Vp8EncoderConfig::kMaxLayers);
  memcpy(config.ts_rate_decimator, vpx_config->ts_rate_decimator,
         sizeof(unsigned int) * Vp8EncoderConfig::kMaxLayers);
  config.ts_periodicity = vpx_config->ts_periodicity;
  memcpy(config.ts_layer_id, vpx_config->ts_layer_id,
         sizeof(unsigned int) * Vp8EncoderConfig::kMaxPeriodicity);
  config.rc_target_bitrate = vpx_config->rc_target_bitrate;
  config.rc_min_quantizer = vpx_config->rc_min_quantizer;
  config.rc_max_quantizer = vpx_config->rc_max_quantizer;

  return config;
}

static void FillInEncoderConfig(vpx_codec_enc_cfg* vpx_config,
                                const Vp8EncoderConfig& config) {
  vpx_config->ts_number_layers = config.ts_number_layers;
  memcpy(vpx_config->ts_target_bitrate, config.ts_target_bitrate,
         sizeof(unsigned int) * Vp8EncoderConfig::kMaxLayers);
  memcpy(vpx_config->ts_rate_decimator, config.ts_rate_decimator,
         sizeof(unsigned int) * Vp8EncoderConfig::kMaxLayers);
  vpx_config->ts_periodicity = config.ts_periodicity;
  memcpy(vpx_config->ts_layer_id, config.ts_layer_id,
         sizeof(unsigned int) * Vp8EncoderConfig::kMaxPeriodicity);
  vpx_config->rc_target_bitrate = config.rc_target_bitrate;
  vpx_config->rc_min_quantizer = config.rc_min_quantizer;
  vpx_config->rc_max_quantizer = config.rc_max_quantizer;
}

bool UpdateVpxConfiguration(TemporalLayers* temporal_layers,
                            vpx_codec_enc_cfg_t* cfg) {
  Vp8EncoderConfig config = GetEncoderConfig(cfg);
  const bool res = temporal_layers->UpdateConfiguration(&config);
  if (res)
    FillInEncoderConfig(cfg, config);
  return res;
}

}  // namespace

std::unique_ptr<VideoEncoder> VP8Encoder::Create() {
  return absl::make_unique<LibvpxVp8Encoder>();
}

vpx_enc_frame_flags_t LibvpxVp8Encoder::EncodeFlags(
    const TemporalLayers::FrameConfig& references) {
  RTC_DCHECK(!references.drop_frame);

  vpx_enc_frame_flags_t flags = 0;

  if ((references.last_buffer_flags & TemporalLayers::kReference) == 0)
    flags |= VP8_EFLAG_NO_REF_LAST;
  if ((references.last_buffer_flags & TemporalLayers::kUpdate) == 0)
    flags |= VP8_EFLAG_NO_UPD_LAST;
  if ((references.golden_buffer_flags & TemporalLayers::kReference) == 0)
    flags |= VP8_EFLAG_NO_REF_GF;
  if ((references.golden_buffer_flags & TemporalLayers::kUpdate) == 0)
    flags |= VP8_EFLAG_NO_UPD_GF;
  if ((references.arf_buffer_flags & TemporalLayers::kReference) == 0)
    flags |= VP8_EFLAG_NO_REF_ARF;
  if ((references.arf_buffer_flags & TemporalLayers::kUpdate) == 0)
    flags |= VP8_EFLAG_NO_UPD_ARF;
  if (references.freeze_entropy)
    flags |= VP8_EFLAG_NO_UPD_ENTROPY;

  return flags;
}

LibvpxVp8Encoder::LibvpxVp8Encoder()
    : LibvpxVp8Encoder(LibvpxInterface::CreateEncoder()) {}

LibvpxVp8Encoder::LibvpxVp8Encoder(std::unique_ptr<LibvpxInterface> interface)
    : libvpx_(std::move(interface)),
      use_gf_boost_(webrtc::field_trial::IsEnabled(kVp8GfBoostFieldTrial)),
      encoded_complete_callback_(nullptr),
      inited_(false),
      timestamp_(0),
      qp_max_(56),  // Setting for max quantizer.
      cpu_speed_default_(-6),
      number_of_cores_(0),
      rc_max_intra_target_(0),
      key_frame_request_(kMaxSimulcastStreams, false) {
  temporal_layers_.reserve(kMaxSimulcastStreams);
  temporal_layers_checkers_.reserve(kMaxSimulcastStreams);
  raw_images_.reserve(kMaxSimulcastStreams);
  encoded_images_.reserve(kMaxSimulcastStreams);
  send_stream_.reserve(kMaxSimulcastStreams);
  cpu_speed_.assign(kMaxSimulcastStreams, cpu_speed_default_);
  encoders_.reserve(kMaxSimulcastStreams);
  configurations_.reserve(kMaxSimulcastStreams);
  downsampling_factors_.reserve(kMaxSimulcastStreams);
}

LibvpxVp8Encoder::~LibvpxVp8Encoder() {
  Release();
}

int LibvpxVp8Encoder::Release() {
  int ret_val = WEBRTC_VIDEO_CODEC_OK;

  while (!encoded_images_.empty()) {
    EncodedImage& image = encoded_images_.back();
    delete[] image._buffer;
    encoded_images_.pop_back();
  }
  while (!encoders_.empty()) {
    vpx_codec_ctx_t& encoder = encoders_.back();
    if (inited_) {
      if (libvpx_->codec_destroy(&encoder)) {
        ret_val = WEBRTC_VIDEO_CODEC_MEMORY;
      }
    }
    encoders_.pop_back();
  }
  configurations_.clear();
  send_stream_.clear();
  cpu_speed_.clear();
  while (!raw_images_.empty()) {
    libvpx_->img_free(&raw_images_.back());
    raw_images_.pop_back();
  }
  temporal_layers_.clear();
  temporal_layers_checkers_.clear();
  inited_ = false;
  return ret_val;
}

int LibvpxVp8Encoder::SetRateAllocation(const VideoBitrateAllocation& bitrate,
                                        uint32_t new_framerate) {
  if (!inited_)
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

  if (encoders_[0].err)
    return WEBRTC_VIDEO_CODEC_ERROR;

  if (new_framerate < 1)
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

  if (bitrate.get_sum_bps() == 0) {
    // Encoder paused, turn off all encoding.
    const int num_streams = static_cast<size_t>(encoders_.size());
    for (int i = 0; i < num_streams; ++i)
      SetStreamState(false, i);
    return WEBRTC_VIDEO_CODEC_OK;
  }

  // At this point, bitrate allocation should already match codec settings.
  if (codec_.maxBitrate > 0)
    RTC_DCHECK_LE(bitrate.get_sum_kbps(), codec_.maxBitrate);
  RTC_DCHECK_GE(bitrate.get_sum_kbps(), codec_.minBitrate);
  if (codec_.numberOfSimulcastStreams > 0)
    RTC_DCHECK_GE(bitrate.get_sum_kbps(), codec_.simulcastStream[0].minBitrate);

  codec_.maxFramerate = new_framerate;

  if (encoders_.size() > 1) {
    // If we have more than 1 stream, reduce the qp_max for the low resolution
    // stream if frame rate is not too low. The trade-off with lower qp_max is
    // possibly more dropped frames, so we only do this if the frame rate is
    // above some threshold (base temporal layer is down to 1/4 for 3 layers).
    // We may want to condition this on bitrate later.
    if (new_framerate > 20) {
      configurations_[encoders_.size() - 1].rc_max_quantizer = 45;
    } else {
      // Go back to default value set in InitEncode.
      configurations_[encoders_.size() - 1].rc_max_quantizer = qp_max_;
    }
  }

  size_t stream_idx = encoders_.size() - 1;
  for (size_t i = 0; i < encoders_.size(); ++i, --stream_idx) {
    unsigned int target_bitrate_kbps =
        bitrate.GetSpatialLayerSum(stream_idx) / 1000;

    bool send_stream = target_bitrate_kbps > 0;
    if (send_stream || encoders_.size() > 1)
      SetStreamState(send_stream, stream_idx);

    configurations_[i].rc_target_bitrate = target_bitrate_kbps;
    if (send_stream) {
      temporal_layers_[stream_idx]->OnRatesUpdated(
          bitrate.GetTemporalLayerAllocation(stream_idx), new_framerate);
    }

    UpdateVpxConfiguration(temporal_layers_[stream_idx].get(),
                           &configurations_[i]);

    if (libvpx_->codec_enc_config_set(&encoders_[i], &configurations_[i])) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* LibvpxVp8Encoder::ImplementationName() const {
  return "libvpx";
}

void LibvpxVp8Encoder::SetStreamState(bool send_stream, int stream_idx) {
  if (send_stream && !send_stream_[stream_idx]) {
    // Need a key frame if we have not sent this stream before.
    key_frame_request_[stream_idx] = true;
  }
  send_stream_[stream_idx] = send_stream;
}

void LibvpxVp8Encoder::SetupTemporalLayers(const VideoCodec& codec) {
  RTC_DCHECK(temporal_layers_.empty());
  int num_streams = SimulcastUtility::NumberOfSimulcastStreams(codec);
  for (int i = 0; i < num_streams; ++i) {
    TemporalLayersType type;
    int num_temporal_layers =
        SimulcastUtility::NumberOfTemporalLayers(codec, i);
    if (SimulcastUtility::IsConferenceModeScreenshare(codec) && i == 0) {
      type = TemporalLayersType::kBitrateDynamic;
      // Legacy screenshare layers supports max 2 layers.
      num_temporal_layers = std::max<int>(2, num_temporal_layers);
    } else {
      type = TemporalLayersType::kFixedPattern;
    }
    temporal_layers_.emplace_back(
        TemporalLayers::CreateTemporalLayers(type, num_temporal_layers));
    temporal_layers_checkers_.emplace_back(
        TemporalLayersChecker::CreateTemporalLayersChecker(
            type, num_temporal_layers));
  }
}

int LibvpxVp8Encoder::InitEncode(const VideoCodec* inst,
                                 int number_of_cores,
                                 size_t /*maxPayloadSize */) {
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
  if (number_of_cores < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->VP8().automaticResizeOn && inst->numberOfSimulcastStreams > 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  int retVal = Release();
  if (retVal < 0) {
    return retVal;
  }

  int number_of_streams = SimulcastUtility::NumberOfSimulcastStreams(*inst);
  if (number_of_streams > 1 &&
      (!SimulcastUtility::ValidSimulcastResolutions(*inst, number_of_streams) ||
       !SimulcastUtility::ValidSimulcastTemporalLayers(*inst,
                                                       number_of_streams))) {
    return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
  }

  SetupTemporalLayers(*inst);

  number_of_cores_ = number_of_cores;
  timestamp_ = 0;
  codec_ = *inst;

  // Code expects simulcastStream resolutions to be correct, make sure they are
  // filled even when there are no simulcast layers.
  if (codec_.numberOfSimulcastStreams == 0) {
    codec_.simulcastStream[0].width = codec_.width;
    codec_.simulcastStream[0].height = codec_.height;
  }

  encoded_images_.resize(number_of_streams);
  encoders_.resize(number_of_streams);
  configurations_.resize(number_of_streams);
  downsampling_factors_.resize(number_of_streams);
  raw_images_.resize(number_of_streams);
  send_stream_.resize(number_of_streams);
  send_stream_[0] = true;  // For non-simulcast case.
  cpu_speed_.resize(number_of_streams);
  std::fill(key_frame_request_.begin(), key_frame_request_.end(), false);

  int idx = number_of_streams - 1;
  for (int i = 0; i < (number_of_streams - 1); ++i, --idx) {
    int gcd = GCD(inst->simulcastStream[idx].width,
                  inst->simulcastStream[idx - 1].width);
    downsampling_factors_[i].num = inst->simulcastStream[idx].width / gcd;
    downsampling_factors_[i].den = inst->simulcastStream[idx - 1].width / gcd;
    send_stream_[i] = false;
  }
  if (number_of_streams > 1) {
    send_stream_[number_of_streams - 1] = false;
    downsampling_factors_[number_of_streams - 1].num = 1;
    downsampling_factors_[number_of_streams - 1].den = 1;
  }
  for (int i = 0; i < number_of_streams; ++i) {
    // allocate memory for encoded image
    if (encoded_images_[i]._buffer != NULL) {
      delete[] encoded_images_[i]._buffer;
    }
    encoded_images_[i]._size =
        CalcBufferSize(VideoType::kI420, codec_.width, codec_.height);
    encoded_images_[i]._buffer = new uint8_t[encoded_images_[i]._size];
    encoded_images_[i]._completeFrame = true;
  }
  // populate encoder configuration with default values
  if (libvpx_->codec_enc_config_default(vpx_codec_vp8_cx(), &configurations_[0],
                                        0)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  // setting the time base of the codec
  configurations_[0].g_timebase.num = 1;
  configurations_[0].g_timebase.den = 90000;
  configurations_[0].g_lag_in_frames = 0;  // 0- no frame lagging

  // Set the error resilience mode for temporal layers (but not simulcast).
  configurations_[0].g_error_resilient =
      (SimulcastUtility::NumberOfTemporalLayers(*inst, 0) > 1)
          ? VPX_ERROR_RESILIENT_DEFAULT
          : 0;

  // rate control settings
  configurations_[0].rc_dropframe_thresh = FrameDropThreshold(0);
  configurations_[0].rc_end_usage = VPX_CBR;
  configurations_[0].g_pass = VPX_RC_ONE_PASS;
  // Handle resizing outside of libvpx.
  configurations_[0].rc_resize_allowed = 0;
  configurations_[0].rc_min_quantizer = 2;
  if (inst->qpMax >= configurations_[0].rc_min_quantizer) {
    qp_max_ = inst->qpMax;
  }
  configurations_[0].rc_max_quantizer = qp_max_;
  configurations_[0].rc_undershoot_pct = 100;
  configurations_[0].rc_overshoot_pct = 15;
  configurations_[0].rc_buf_initial_sz = 500;
  configurations_[0].rc_buf_optimal_sz = 600;
  configurations_[0].rc_buf_sz = 1000;

  // Set the maximum target size of any key-frame.
  rc_max_intra_target_ = MaxIntraTarget(configurations_[0].rc_buf_optimal_sz);

  if (inst->VP8().keyFrameInterval > 0) {
    configurations_[0].kf_mode = VPX_KF_AUTO;
    configurations_[0].kf_max_dist = inst->VP8().keyFrameInterval;
  } else {
    configurations_[0].kf_mode = VPX_KF_DISABLED;
  }

  // Allow the user to set the complexity for the base stream.
  switch (inst->VP8().complexity) {
    case VideoCodecComplexity::kComplexityHigh:
      cpu_speed_[0] = -5;
      break;
    case VideoCodecComplexity::kComplexityHigher:
      cpu_speed_[0] = -4;
      break;
    case VideoCodecComplexity::kComplexityMax:
      cpu_speed_[0] = -3;
      break;
    default:
      cpu_speed_[0] = -6;
      break;
  }
  cpu_speed_default_ = cpu_speed_[0];
  // Set encoding complexity (cpu_speed) based on resolution and/or platform.
  cpu_speed_[0] = SetCpuSpeed(inst->width, inst->height);
  for (int i = 1; i < number_of_streams; ++i) {
    cpu_speed_[i] =
        SetCpuSpeed(inst->simulcastStream[number_of_streams - 1 - i].width,
                    inst->simulcastStream[number_of_streams - 1 - i].height);
  }
  configurations_[0].g_w = inst->width;
  configurations_[0].g_h = inst->height;

  // Determine number of threads based on the image size and #cores.
  // TODO(fbarchard): Consider number of Simulcast layers.
  configurations_[0].g_threads = NumberOfThreads(
      configurations_[0].g_w, configurations_[0].g_h, number_of_cores);

  // Creating a wrapper to the image - setting image data to NULL.
  // Actual pointer will be set in encode. Setting align to 1, as it
  // is meaningless (no memory allocation is done here).
  libvpx_->img_wrap(&raw_images_[0], VPX_IMG_FMT_I420, inst->width,
                    inst->height, 1, NULL);

  // Note the order we use is different from webm, we have lowest resolution
  // at position 0 and they have highest resolution at position 0.
  int stream_idx = encoders_.size() - 1;
  SimulcastRateAllocator init_allocator(codec_);
  VideoBitrateAllocation allocation = init_allocator.GetAllocation(
      inst->startBitrate * 1000, inst->maxFramerate);
  std::vector<uint32_t> stream_bitrates;
  for (int i = 0; i == 0 || i < inst->numberOfSimulcastStreams; ++i) {
    uint32_t bitrate = allocation.GetSpatialLayerSum(i) / 1000;
    stream_bitrates.push_back(bitrate);
  }

  configurations_[0].rc_target_bitrate = stream_bitrates[stream_idx];
  if (stream_bitrates[stream_idx] > 0) {
    temporal_layers_[stream_idx]->OnRatesUpdated(
        allocation.GetTemporalLayerAllocation(stream_idx), inst->maxFramerate);
  }
  UpdateVpxConfiguration(temporal_layers_[stream_idx].get(),
                         &configurations_[0]);
  configurations_[0].rc_dropframe_thresh = FrameDropThreshold(stream_idx);

  --stream_idx;
  for (size_t i = 1; i < encoders_.size(); ++i, --stream_idx) {
    memcpy(&configurations_[i], &configurations_[0],
           sizeof(configurations_[0]));

    configurations_[i].g_w = inst->simulcastStream[stream_idx].width;
    configurations_[i].g_h = inst->simulcastStream[stream_idx].height;

    // Use 1 thread for lower resolutions.
    configurations_[i].g_threads = 1;

    configurations_[i].rc_dropframe_thresh = FrameDropThreshold(stream_idx);

    // Setting alignment to 32 - as that ensures at least 16 for all
    // planes (32 for Y, 16 for U,V). Libvpx sets the requested stride for
    // the y plane, but only half of it to the u and v planes.
    libvpx_->img_alloc(&raw_images_[i], VPX_IMG_FMT_I420,
                       inst->simulcastStream[stream_idx].width,
                       inst->simulcastStream[stream_idx].height,
                       kVp832ByteAlign);
    SetStreamState(stream_bitrates[stream_idx] > 0, stream_idx);
    configurations_[i].rc_target_bitrate = stream_bitrates[stream_idx];
    if (stream_bitrates[stream_idx] > 0) {
      temporal_layers_[stream_idx]->OnRatesUpdated(
          allocation.GetTemporalLayerAllocation(stream_idx),
          inst->maxFramerate);
    }
    UpdateVpxConfiguration(temporal_layers_[stream_idx].get(),
                           &configurations_[i]);
  }

  return InitAndSetControlSettings();
}

int LibvpxVp8Encoder::SetCpuSpeed(int width, int height) {
#if defined(WEBRTC_ARCH_ARM) || defined(WEBRTC_ARCH_ARM64) || \
    defined(WEBRTC_ANDROID)
  // On mobile platform, use a lower speed setting for lower resolutions for
  // CPUs with 4 or more cores.
  RTC_DCHECK_GT(number_of_cores_, 0);
  if (number_of_cores_ <= 3)
    return -12;

  if (width * height <= 352 * 288)
    return -8;
  else if (width * height <= 640 * 480)
    return -10;
  else
    return -12;
#else
  // For non-ARM, increase encoding complexity (i.e., use lower speed setting)
  // if resolution is below CIF. Otherwise, keep the default/user setting
  // (|cpu_speed_default_|) set on InitEncode via VP8().complexity.
  if (width * height < 352 * 288)
    return (cpu_speed_default_ < -4) ? -4 : cpu_speed_default_;
  else
    return cpu_speed_default_;
#endif
}

int LibvpxVp8Encoder::NumberOfThreads(int width, int height, int cpus) {
#if defined(WEBRTC_ANDROID)
  if (width * height >= 320 * 180) {
    if (cpus >= 4) {
      // 3 threads for CPUs with 4 and more cores since most of times only 4
      // cores will be active.
      return 3;
    } else if (cpus == 3 || cpus == 2) {
      return 2;
    } else {
      return 1;
    }
  }
  return 1;
#else
  if (width * height >= 1920 * 1080 && cpus > 8) {
    return 8;  // 8 threads for 1080p on high perf machines.
  } else if (width * height > 1280 * 960 && cpus >= 6) {
    // 3 threads for 1080p.
    return 3;
  } else if (width * height > 640 * 480 && cpus >= 3) {
    // Default 2 threads for qHD/HD, but allow 3 if core count is high enough,
    // as this will allow more margin for high-core/low clock machines or if
    // not built with highest optimization.
    if (cpus >= 6) {
      return 3;
    }
    return 2;
  } else {
    // 1 thread for VGA or less.
    return 1;
  }
#endif
}

int LibvpxVp8Encoder::InitAndSetControlSettings() {
  vpx_codec_flags_t flags = 0;
  flags |= VPX_CODEC_USE_OUTPUT_PARTITION;

  if (encoders_.size() > 1) {
    int error = libvpx_->codec_enc_init_multi(
        &encoders_[0], vpx_codec_vp8_cx(), &configurations_[0],
        encoders_.size(), flags, &downsampling_factors_[0]);
    if (error) {
      return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
  } else {
    if (libvpx_->codec_enc_init(&encoders_[0], vpx_codec_vp8_cx(),
                                &configurations_[0], flags)) {
      return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
  }
  // Enable denoising for the highest resolution stream, and for
  // the second highest resolution if we are doing more than 2
  // spatial layers/streams.
  // TODO(holmer): Investigate possibility of adding a libvpx API
  // for getting the denoised frame from the encoder and using that
  // when encoding lower resolution streams. Would it work with the
  // multi-res encoding feature?
  denoiserState denoiser_state = kDenoiserOnYOnly;
#if defined(WEBRTC_ARCH_ARM) || defined(WEBRTC_ARCH_ARM64) || \
    defined(WEBRTC_ANDROID)
  denoiser_state = kDenoiserOnYOnly;
#else
  denoiser_state = kDenoiserOnAdaptive;
#endif
  libvpx_->codec_control(
      &encoders_[0], VP8E_SET_NOISE_SENSITIVITY,
      codec_.VP8()->denoisingOn ? denoiser_state : kDenoiserOff);
  if (encoders_.size() > 2) {
    libvpx_->codec_control(
        &encoders_[1], VP8E_SET_NOISE_SENSITIVITY,
        codec_.VP8()->denoisingOn ? denoiser_state : kDenoiserOff);
  }
  for (size_t i = 0; i < encoders_.size(); ++i) {
    // Allow more screen content to be detected as static.
    libvpx_->codec_control(
        &(encoders_[i]), VP8E_SET_STATIC_THRESHOLD,
        codec_.mode == VideoCodecMode::kScreensharing ? 300u : 1u);
    libvpx_->codec_control(&(encoders_[i]), VP8E_SET_CPUUSED, cpu_speed_[i]);
    libvpx_->codec_control(
        &(encoders_[i]), VP8E_SET_TOKEN_PARTITIONS,
        static_cast<vp8e_token_partitions>(kTokenPartitions));
    libvpx_->codec_control(&(encoders_[i]), VP8E_SET_MAX_INTRA_BITRATE_PCT,
                           rc_max_intra_target_);
    // VP8E_SET_SCREEN_CONTENT_MODE 2 = screen content with more aggressive
    // rate control (drop frames on large target bitrate overshoot)
    libvpx_->codec_control(
        &(encoders_[i]), VP8E_SET_SCREEN_CONTENT_MODE,
        codec_.mode == VideoCodecMode::kScreensharing ? 2u : 0u);
    // Apply boost on golden frames (has only effect when resilience is off).
    if (use_gf_boost_ && configurations_[0].g_error_resilient == 0) {
      int gf_boost_percent;
      if (GetGfBoostPercentageFromFieldTrialGroup(&gf_boost_percent)) {
        libvpx_->codec_control(&(encoders_[i]), VP8E_SET_GF_CBR_BOOST_PCT,
                               gf_boost_percent);
      }
    }
  }
  inited_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

uint32_t LibvpxVp8Encoder::MaxIntraTarget(uint32_t optimalBuffersize) {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scalePar.
  // Max target size = scalePar * optimalBufferSize * targetBR[Kbps].
  // This values is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / frameRate.
  // The target in % is as follows:

  float scalePar = 0.5;
  uint32_t targetPct = optimalBuffersize * scalePar * codec_.maxFramerate / 10;

  // Don't go below 3 times the per frame bandwidth.
  const uint32_t minIntraTh = 300;
  return (targetPct < minIntraTh) ? minIntraTh : targetPct;
}

uint32_t LibvpxVp8Encoder::FrameDropThreshold(size_t spatial_idx) const {
  bool enable_frame_dropping = codec_.VP8().frameDroppingOn;
  // If temporal layers are used, they get to override the frame dropping
  // setting, as eg. ScreenshareLayers does not work as intended with frame
  // dropping on and DefaultTemporalLayers will have performance issues with
  // frame dropping off.
  if (temporal_layers_.size() <= spatial_idx) {
    enable_frame_dropping =
        temporal_layers_[spatial_idx]->SupportsEncoderFrameDropping();
  }
  return enable_frame_dropping ? 30 : 0;
}

int LibvpxVp8Encoder::Encode(const VideoFrame& frame,
                             const CodecSpecificInfo* codec_specific_info,
                             const std::vector<FrameType>* frame_types) {
  RTC_DCHECK_EQ(frame.width(), codec_.width);
  RTC_DCHECK_EQ(frame.height(), codec_.height);

  if (!inited_)
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  if (encoded_complete_callback_ == NULL)
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

  rtc::scoped_refptr<I420BufferInterface> input_image =
      frame.video_frame_buffer()->ToI420();
  // Since we are extracting raw pointers from |input_image| to
  // |raw_images_[0]|, the resolution of these frames must match.
  RTC_DCHECK_EQ(input_image->width(), raw_images_[0].d_w);
  RTC_DCHECK_EQ(input_image->height(), raw_images_[0].d_h);

  // Image in vpx_image_t format.
  // Input image is const. VP8's raw image is not defined as const.
  raw_images_[0].planes[VPX_PLANE_Y] =
      const_cast<uint8_t*>(input_image->DataY());
  raw_images_[0].planes[VPX_PLANE_U] =
      const_cast<uint8_t*>(input_image->DataU());
  raw_images_[0].planes[VPX_PLANE_V] =
      const_cast<uint8_t*>(input_image->DataV());

  raw_images_[0].stride[VPX_PLANE_Y] = input_image->StrideY();
  raw_images_[0].stride[VPX_PLANE_U] = input_image->StrideU();
  raw_images_[0].stride[VPX_PLANE_V] = input_image->StrideV();

  for (size_t i = 1; i < encoders_.size(); ++i) {
    // Scale the image down a number of times by downsampling factor
    libyuv::I420Scale(
        raw_images_[i - 1].planes[VPX_PLANE_Y],
        raw_images_[i - 1].stride[VPX_PLANE_Y],
        raw_images_[i - 1].planes[VPX_PLANE_U],
        raw_images_[i - 1].stride[VPX_PLANE_U],
        raw_images_[i - 1].planes[VPX_PLANE_V],
        raw_images_[i - 1].stride[VPX_PLANE_V], raw_images_[i - 1].d_w,
        raw_images_[i - 1].d_h, raw_images_[i].planes[VPX_PLANE_Y],
        raw_images_[i].stride[VPX_PLANE_Y], raw_images_[i].planes[VPX_PLANE_U],
        raw_images_[i].stride[VPX_PLANE_U], raw_images_[i].planes[VPX_PLANE_V],
        raw_images_[i].stride[VPX_PLANE_V], raw_images_[i].d_w,
        raw_images_[i].d_h, libyuv::kFilterBilinear);
  }
  bool send_key_frame = false;
  for (size_t i = 0; i < key_frame_request_.size() && i < send_stream_.size();
       ++i) {
    if (key_frame_request_[i] && send_stream_[i]) {
      send_key_frame = true;
      break;
    }
  }
  if (!send_key_frame && frame_types) {
    for (size_t i = 0; i < frame_types->size() && i < send_stream_.size();
         ++i) {
      if ((*frame_types)[i] == kVideoFrameKey && send_stream_[i]) {
        send_key_frame = true;
        break;
      }
    }
  }
  vpx_enc_frame_flags_t flags[kMaxSimulcastStreams];
  TemporalLayers::FrameConfig tl_configs[kMaxSimulcastStreams];
  for (size_t i = 0; i < encoders_.size(); ++i) {
    tl_configs[i] = temporal_layers_[i]->UpdateLayerConfig(frame.timestamp());
    if (tl_configs[i].drop_frame) {
      if (send_key_frame) {
        continue;
      }
      // Drop this frame.
      return WEBRTC_VIDEO_CODEC_OK;
    }
    flags[i] = EncodeFlags(tl_configs[i]);
  }
  if (send_key_frame) {
    // Adapt the size of the key frame when in screenshare with 1 temporal
    // layer.
    if (encoders_.size() == 1 &&
        codec_.mode == VideoCodecMode::kScreensharing &&
        codec_.VP8()->numberOfTemporalLayers <= 1) {
      const uint32_t forceKeyFrameIntraTh = 100;
      libvpx_->codec_control(&(encoders_[0]), VP8E_SET_MAX_INTRA_BITRATE_PCT,
                             forceKeyFrameIntraTh);
    }
    // Key frame request from caller.
    // Will update both golden and alt-ref.
    for (size_t i = 0; i < encoders_.size(); ++i) {
      flags[i] = VPX_EFLAG_FORCE_KF;
    }
    std::fill(key_frame_request_.begin(), key_frame_request_.end(), false);
  }

  // Set the encoder frame flags and temporal layer_id for each spatial stream.
  // Note that |temporal_layers_| are defined starting from lowest resolution at
  // position 0 to highest resolution at position |encoders_.size() - 1|,
  // whereas |encoder_| is from highest to lowest resolution.
  size_t stream_idx = encoders_.size() - 1;
  for (size_t i = 0; i < encoders_.size(); ++i, --stream_idx) {
    // Allow the layers adapter to temporarily modify the configuration. This
    // change isn't stored in configurations_ so change will be discarded at
    // the next update.
    vpx_codec_enc_cfg_t temp_config;
    memcpy(&temp_config, &configurations_[i], sizeof(vpx_codec_enc_cfg_t));
    if (UpdateVpxConfiguration(temporal_layers_[stream_idx].get(),
                               &temp_config)) {
      if (libvpx_->codec_enc_config_set(&encoders_[i], &temp_config))
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    libvpx_->codec_control(&encoders_[i], VP8E_SET_FRAME_FLAGS,
                           static_cast<int>(flags[stream_idx]));
    libvpx_->codec_control(&encoders_[i], VP8E_SET_TEMPORAL_LAYER_ID,
                           tl_configs[i].encoder_layer_id);
  }
  // TODO(holmer): Ideally the duration should be the timestamp diff of this
  // frame and the next frame to be encoded, which we don't have. Instead we
  // would like to use the duration of the previous frame. Unfortunately the
  // rate control seems to be off with that setup. Using the average input
  // frame rate to calculate an average duration for now.
  assert(codec_.maxFramerate > 0);
  uint32_t duration = 90000 / codec_.maxFramerate;

  int error = WEBRTC_VIDEO_CODEC_OK;
  int num_tries = 0;
  // If the first try returns WEBRTC_VIDEO_CODEC_TARGET_BITRATE_OVERSHOOT
  // the frame must be reencoded with the same parameters again because
  // target bitrate is exceeded and encoder state has been reset.
  while (num_tries == 0 ||
         (num_tries == 1 &&
          error == WEBRTC_VIDEO_CODEC_TARGET_BITRATE_OVERSHOOT)) {
    ++num_tries;
    // Note we must pass 0 for |flags| field in encode call below since they are
    // set above in |libvpx_interface_->vpx_codec_control_| function for each
    // encoder/spatial layer.
    error = libvpx_->codec_encode(&encoders_[0], &raw_images_[0], timestamp_,
                                  duration, 0, VPX_DL_REALTIME);
    // Reset specific intra frame thresholds, following the key frame.
    if (send_key_frame) {
      libvpx_->codec_control(&(encoders_[0]), VP8E_SET_MAX_INTRA_BITRATE_PCT,
                             rc_max_intra_target_);
    }
    if (error)
      return WEBRTC_VIDEO_CODEC_ERROR;
    timestamp_ += duration;
    // Examines frame timestamps only.
    error = GetEncodedPartitions(frame);
  }
  return error;
}

void LibvpxVp8Encoder::PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                                             const vpx_codec_cx_pkt_t& pkt,
                                             int stream_idx,
                                             int encoder_idx,
                                             uint32_t timestamp) {
  assert(codec_specific != NULL);
  codec_specific->codecType = kVideoCodecVP8;
  codec_specific->codec_name = ImplementationName();
  CodecSpecificInfoVP8* vp8Info = &(codec_specific->codecSpecific.VP8);
  vp8Info->keyIdx = kNoKeyIdx;  // TODO(hlundin) populate this
  vp8Info->nonReference = (pkt.data.frame.flags & VPX_FRAME_IS_DROPPABLE) != 0;

  int qp = 0;
  vpx_codec_control(&encoders_[encoder_idx], VP8E_GET_LAST_QUANTIZER_64, &qp);
  temporal_layers_[stream_idx]->OnEncodeDone(
      timestamp, encoded_images_[encoder_idx]._length,
      (pkt.data.frame.flags & VPX_FRAME_IS_KEY) != 0, qp, vp8Info);
}

int LibvpxVp8Encoder::GetEncodedPartitions(const VideoFrame& input_image) {
  int stream_idx = static_cast<int>(encoders_.size()) - 1;
  int result = WEBRTC_VIDEO_CODEC_OK;
  for (size_t encoder_idx = 0; encoder_idx < encoders_.size();
       ++encoder_idx, --stream_idx) {
    vpx_codec_iter_t iter = NULL;
    int part_idx = 0;
    encoded_images_[encoder_idx]._length = 0;
    encoded_images_[encoder_idx]._frameType = kVideoFrameDelta;
    RTPFragmentationHeader frag_info;
    // kTokenPartitions is number of bits used.
    frag_info.VerifyAndAllocateFragmentationHeader((1 << kTokenPartitions) + 1);
    CodecSpecificInfo codec_specific;
    bool is_keyframe = false;
    const vpx_codec_cx_pkt_t* pkt = NULL;
    while ((pkt = libvpx_->codec_get_cx_data(&encoders_[encoder_idx], &iter)) !=
           NULL) {
      switch (pkt->kind) {
        case VPX_CODEC_CX_FRAME_PKT: {
          size_t length = encoded_images_[encoder_idx]._length;
          if (pkt->data.frame.sz + length >
              encoded_images_[encoder_idx]._size) {
            uint8_t* buffer = new uint8_t[pkt->data.frame.sz + length];
            memcpy(buffer, encoded_images_[encoder_idx]._buffer, length);
            delete[] encoded_images_[encoder_idx]._buffer;
            encoded_images_[encoder_idx]._buffer = buffer;
            encoded_images_[encoder_idx]._size = pkt->data.frame.sz + length;
          }
          memcpy(&encoded_images_[encoder_idx]._buffer[length],
                 pkt->data.frame.buf, pkt->data.frame.sz);
          frag_info.fragmentationOffset[part_idx] = length;
          frag_info.fragmentationLength[part_idx] = pkt->data.frame.sz;
          frag_info.fragmentationPlType[part_idx] = 0;  // not known here
          frag_info.fragmentationTimeDiff[part_idx] = 0;
          encoded_images_[encoder_idx]._length += pkt->data.frame.sz;
          assert(length <= encoded_images_[encoder_idx]._size);
          ++part_idx;
          break;
        }
        default:
          break;
      }
      // End of frame
      if ((pkt->data.frame.flags & VPX_FRAME_IS_FRAGMENT) == 0) {
        // check if encoded frame is a key frame
        if (pkt->data.frame.flags & VPX_FRAME_IS_KEY) {
          encoded_images_[encoder_idx]._frameType = kVideoFrameKey;
          is_keyframe = true;
        }
        encoded_images_[encoder_idx].SetSpatialIndex(stream_idx);
        PopulateCodecSpecific(&codec_specific, *pkt, stream_idx, encoder_idx,
                              input_image.timestamp());
        break;
      }
    }
    encoded_images_[encoder_idx].SetTimestamp(input_image.timestamp());
    encoded_images_[encoder_idx].capture_time_ms_ =
        input_image.render_time_ms();
    encoded_images_[encoder_idx].rotation_ = input_image.rotation();
    encoded_images_[encoder_idx].content_type_ =
        (codec_.mode == VideoCodecMode::kScreensharing)
            ? VideoContentType::SCREENSHARE
            : VideoContentType::UNSPECIFIED;
    encoded_images_[encoder_idx].timing_.flags = VideoSendTiming::kInvalid;

    if (send_stream_[stream_idx]) {
      if (encoded_images_[encoder_idx]._length > 0) {
        TRACE_COUNTER_ID1("webrtc", "EncodedFrameSize", encoder_idx,
                          encoded_images_[encoder_idx]._length);
        encoded_images_[encoder_idx]._encodedHeight =
            codec_.simulcastStream[stream_idx].height;
        encoded_images_[encoder_idx]._encodedWidth =
            codec_.simulcastStream[stream_idx].width;
        int qp_128 = -1;
        libvpx_->codec_control(&encoders_[encoder_idx], VP8E_GET_LAST_QUANTIZER,
                               &qp_128);
        encoded_images_[encoder_idx].qp_ = qp_128;
        encoded_complete_callback_->OnEncodedImage(encoded_images_[encoder_idx],
                                                   &codec_specific, &frag_info);
      } else if (!temporal_layers_[stream_idx]
                      ->SupportsEncoderFrameDropping()) {
        result = WEBRTC_VIDEO_CODEC_TARGET_BITRATE_OVERSHOOT;
        if (encoded_images_[encoder_idx]._length == 0) {
          // Dropped frame that will be re-encoded.
          temporal_layers_[stream_idx]->OnEncodeDone(input_image.timestamp(), 0,
                                                     false, 0, nullptr);
        }
      }
    }
  }
  return result;
}

VideoEncoder::ScalingSettings LibvpxVp8Encoder::GetScalingSettings() const {
  const bool enable_scaling = encoders_.size() == 1 &&
                              configurations_[0].rc_dropframe_thresh > 0 &&
                              codec_.VP8().automaticResizeOn;
  return enable_scaling ? VideoEncoder::ScalingSettings(kLowVp8QpThreshold,
                                                        kHighVp8QpThreshold)
                        : VideoEncoder::ScalingSettings::kOff;
}

int LibvpxVp8Encoder::SetChannelParameters(uint32_t packetLoss, int64_t rtt) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int LibvpxVp8Encoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  encoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace webrtc
