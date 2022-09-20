/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>

#include "absl/algorithm/container.h"
#include "absl/base/macros.h"
#include "absl/container/inlined_vector.h"
#include "api/array_view.h"
#include "api/field_trials_view.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/interface/libvpx_interface.h"
#include "modules/video_coding/codecs/vp9/libvpx_vp9_encoder.h"
#include "modules/video_coding/frame_dependencies_calculator.h"
#include "rtc_base/numerics/safe_compare.h"
#include "test/fuzzers/fuzz_data_helper.h"

// Fuzzer simulates various svc configurations and libvpx encoder dropping
// layer frames.
// Validates vp9 encoder wrapper produces consistent frame references.
namespace webrtc {
namespace {

using test::FuzzDataHelper;

class FrameValidator : public EncodedImageCallback {
 public:
  ~FrameValidator() override = default;

  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info) override {
    RTC_CHECK(codec_specific_info);
    RTC_CHECK_EQ(codec_specific_info->codecType, kVideoCodecVP9);
    if (codec_specific_info->codecSpecific.VP9.first_frame_in_picture) {
      ++picture_id_;
    }
    int64_t frame_id = frame_id_++;
    LayerFrame& layer_frame = frames_[frame_id % kMaxFrameHistorySize];
    layer_frame.picture_id = picture_id_;
    layer_frame.spatial_id = encoded_image.SpatialIndex().value_or(0);
    layer_frame.info = *codec_specific_info;
    layer_frame.frame_id = frame_id;
    CheckVp9References(layer_frame);

    if (layer_frame.info.generic_frame_info.has_value()) {
      layer_frame.frame_dependencies =
          dependencies_calculator_.FromBuffersUsage(
              frame_id, layer_frame.info.generic_frame_info->encoder_buffers);

      CheckGenericReferences(layer_frame);
      CheckGenericAndCodecSpecificReferencesAreConsistent(layer_frame);
    } else {
      layer_frame.frame_dependencies = {};
    }

    return Result(Result::OK);
  }

 private:
  // With 4 spatial layers and patterns up to 8 pictures, it should be enought
  // to keep 32 last frames to validate dependencies.
  static constexpr size_t kMaxFrameHistorySize = 32;
  struct LayerFrame {
    const CodecSpecificInfoVP9& vp9() const { return info.codecSpecific.VP9; }
    int temporal_id() const {
      return vp9().temporal_idx == kNoTemporalIdx ? 0 : vp9().temporal_idx;
    }

    int64_t frame_id;
    int64_t picture_id;
    int spatial_id;
    absl::InlinedVector<int64_t, 5> frame_dependencies;
    CodecSpecificInfo info;
  };

  void CheckVp9References(const LayerFrame& layer_frame) {
    if (layer_frame.frame_id == 0) {
      RTC_CHECK(!layer_frame.vp9().inter_layer_predicted);
    } else {
      const LayerFrame& previous_frame = Frame(layer_frame.frame_id - 1);
      if (layer_frame.vp9().inter_layer_predicted) {
        RTC_CHECK(!previous_frame.vp9().non_ref_for_inter_layer_pred);
        RTC_CHECK_EQ(layer_frame.picture_id, previous_frame.picture_id);
      }
      if (previous_frame.picture_id == layer_frame.picture_id) {
        RTC_CHECK_GT(layer_frame.spatial_id, previous_frame.spatial_id);
        // The check below would fail for temporal shift structures. Remove it
        // or move it to !flexible_mode section when vp9 encoder starts
        // supporting such structures.
        RTC_CHECK_EQ(layer_frame.vp9().temporal_idx,
                     previous_frame.vp9().temporal_idx);
      }
    }
    if (!layer_frame.vp9().flexible_mode) {
      if (layer_frame.vp9().gof.num_frames_in_gof > 0) {
        gof_.CopyGofInfoVP9(layer_frame.vp9().gof);
      }
      RTC_CHECK_EQ(gof_.temporal_idx[layer_frame.vp9().gof_idx],
                   layer_frame.temporal_id());
    }
  }

  void CheckGenericReferences(const LayerFrame& layer_frame) const {
    const GenericFrameInfo& generic_info = *layer_frame.info.generic_frame_info;
    for (int64_t dependency_frame_id : layer_frame.frame_dependencies) {
      RTC_CHECK_GE(dependency_frame_id, 0);
      const LayerFrame& dependency = Frame(dependency_frame_id);
      RTC_CHECK(dependency.info.generic_frame_info.has_value());
      RTC_CHECK_GE(generic_info.spatial_id,
                   dependency.info.generic_frame_info->spatial_id);
      RTC_CHECK_GE(generic_info.temporal_id,
                   dependency.info.generic_frame_info->temporal_id);
    }
  }

  void CheckGenericAndCodecSpecificReferencesAreConsistent(
      const LayerFrame& layer_frame) const {
    const GenericFrameInfo& generic_info = *layer_frame.info.generic_frame_info;
    RTC_CHECK_EQ(generic_info.spatial_id, layer_frame.spatial_id);
    RTC_CHECK_EQ(generic_info.temporal_id, layer_frame.temporal_id());
    auto picture_id_diffs = rtc::MakeArrayView(layer_frame.vp9().p_diff,
                                               layer_frame.vp9().num_ref_pics);
    RTC_CHECK_EQ(layer_frame.frame_dependencies.size(),
                 picture_id_diffs.size() +
                     (layer_frame.vp9().inter_layer_predicted ? 1 : 0));
    for (int64_t dependency_frame_id : layer_frame.frame_dependencies) {
      RTC_CHECK_GE(dependency_frame_id, 0);
      const LayerFrame& dependency = Frame(dependency_frame_id);
      if (dependency.spatial_id != layer_frame.spatial_id) {
        RTC_CHECK(layer_frame.vp9().inter_layer_predicted);
        RTC_CHECK_EQ(layer_frame.picture_id, dependency.picture_id);
        RTC_CHECK_GT(layer_frame.spatial_id, dependency.spatial_id);
      } else {
        RTC_CHECK(layer_frame.vp9().inter_pic_predicted);
        RTC_CHECK_EQ(layer_frame.spatial_id, dependency.spatial_id);
        RTC_CHECK(absl::c_linear_search(
            picture_id_diffs, layer_frame.picture_id - dependency.picture_id));
      }
    }
  }

  const LayerFrame& Frame(int64_t frame_id) const {
    auto& frame = frames_[frame_id % kMaxFrameHistorySize];
    RTC_CHECK_EQ(frame.frame_id, frame_id);
    return frame;
  }

  GofInfoVP9 gof_;
  int64_t frame_id_ = 0;
  int64_t picture_id_ = 1;
  FrameDependenciesCalculator dependencies_calculator_;
  LayerFrame frames_[kMaxFrameHistorySize];
};

class FieldTrials : public FieldTrialsView {
 public:
  explicit FieldTrials(FuzzDataHelper& config)
      : flags_(config.ReadOrDefaultValue<uint8_t>(0)) {}

  ~FieldTrials() override = default;
  std::string Lookup(absl::string_view key) const override {
    static constexpr absl::string_view kBinaryFieldTrials[] = {
        "WebRTC-Vp9ExternalRefCtrl",
        "WebRTC-Vp9IssueKeyFrameOnLayerDeactivation",
    };
    for (size_t i = 0; i < ABSL_ARRAYSIZE(kBinaryFieldTrials); ++i) {
      if (key == kBinaryFieldTrials[i]) {
        return (flags_ & (1u << i)) ? "Enabled" : "Disabled";
      }
    }

    // Ignore following field trials.
    if (key == "WebRTC-CongestionWindow" ||
        key == "WebRTC-UseBaseHeavyVP8TL3RateAllocation" ||
        key == "WebRTC-SimulcastUpswitchHysteresisPercent" ||
        key == "WebRTC-SimulcastScreenshareUpswitchHysteresisPercent" ||
        key == "WebRTC-VideoRateControl" ||
        key == "WebRTC-VP9-PerformanceFlags" ||
        key == "WebRTC-VP9VariableFramerateScreenshare" ||
        key == "WebRTC-VP9QualityScaler") {
      return "";
    }
    // Crash when using unexpected field trial to decide if it should be fuzzed
    // or have a constant value.
    RTC_CHECK(false) << "Unfuzzed field trial " << key << "\n";
  }

 private:
  const uint8_t flags_;
};

VideoCodec CodecSettings(FuzzDataHelper& rng) {
  uint16_t config = rng.ReadOrDefaultValue<uint16_t>(0);
  // Test up to to 4 spatial and 4 temporal layers.
  int num_spatial_layers = 1 + (config & 0b11);
  int num_temporal_layers = 1 + ((config >> 2) & 0b11);

  VideoCodec codec_settings = {};
  codec_settings.codecType = kVideoCodecVP9;
  codec_settings.maxFramerate = 30;
  codec_settings.width = 320 << (num_spatial_layers - 1);
  codec_settings.height = 180 << (num_spatial_layers - 1);
  if (num_spatial_layers > 1) {
    for (int sid = 0; sid < num_spatial_layers; ++sid) {
      SpatialLayer& spatial_layer = codec_settings.spatialLayers[sid];
      codec_settings.width = 320 << sid;
      codec_settings.height = 180 << sid;
      spatial_layer.maxFramerate = codec_settings.maxFramerate;
      spatial_layer.numberOfTemporalLayers = num_temporal_layers;
    }
  }
  codec_settings.VP9()->numberOfSpatialLayers = num_spatial_layers;
  codec_settings.VP9()->numberOfTemporalLayers = num_temporal_layers;
  int inter_layer_pred = (config >> 4) & 0b11;
  // There are only 3 valid values.
  codec_settings.VP9()->interLayerPred = static_cast<InterLayerPredMode>(
      inter_layer_pred < 3 ? inter_layer_pred : 0);
  codec_settings.VP9()->flexibleMode = (config & (1u << 6)) != 0;
  codec_settings.SetFrameDropEnabled((config & (1u << 7)) != 0);
  codec_settings.mode = VideoCodecMode::kRealtimeVideo;
  return codec_settings;
}

VideoEncoder::Settings EncoderSettings() {
  return VideoEncoder::Settings(VideoEncoder::Capabilities(false),
                                /*number_of_cores=*/1,
                                /*max_payload_size=*/0);
}

struct LibvpxState {
  LibvpxState() {
    pkt.kind = VPX_CODEC_CX_FRAME_PKT;
    pkt.data.frame.buf = pkt_buffer;
    pkt.data.frame.sz = ABSL_ARRAYSIZE(pkt_buffer);
    layer_id.spatial_layer_id = -1;
  }

  uint8_t pkt_buffer[1000] = {};
  vpx_codec_enc_cfg_t config = {};
  vpx_codec_priv_output_cx_pkt_cb_pair_t callback = {};
  vpx_image_t img = {};
  vpx_svc_ref_frame_config_t ref_config = {};
  vpx_svc_layer_id_t layer_id = {};
  vpx_svc_frame_drop_t frame_drop = {};
  vpx_codec_cx_pkt pkt = {};
};

class StubLibvpx : public LibvpxInterface {
 public:
  explicit StubLibvpx(LibvpxState* state) : state_(state) { RTC_CHECK(state_); }

  vpx_codec_err_t codec_enc_config_default(vpx_codec_iface_t* iface,
                                           vpx_codec_enc_cfg_t* cfg,
                                           unsigned int usage) const override {
    state_->config = *cfg;
    return VPX_CODEC_OK;
  }

  vpx_codec_err_t codec_enc_init(vpx_codec_ctx_t* ctx,
                                 vpx_codec_iface_t* iface,
                                 const vpx_codec_enc_cfg_t* cfg,
                                 vpx_codec_flags_t flags) const override {
    RTC_CHECK(ctx);
    ctx->err = VPX_CODEC_OK;
    return VPX_CODEC_OK;
  }

  vpx_image_t* img_wrap(vpx_image_t* img,
                        vpx_img_fmt_t fmt,
                        unsigned int d_w,
                        unsigned int d_h,
                        unsigned int stride_align,
                        unsigned char* img_data) const override {
    state_->img.fmt = fmt;
    state_->img.d_w = d_w;
    state_->img.d_h = d_h;
    return &state_->img;
  }

  vpx_codec_err_t codec_encode(vpx_codec_ctx_t* ctx,
                               const vpx_image_t* img,
                               vpx_codec_pts_t pts,
                               uint64_t duration,
                               vpx_enc_frame_flags_t flags,
                               uint64_t deadline) const override {
    if (flags & VPX_EFLAG_FORCE_KF) {
      state_->pkt.data.frame.flags = VPX_FRAME_IS_KEY;
    } else {
      state_->pkt.data.frame.flags = 0;
    }
    state_->pkt.data.frame.duration = duration;
    return VPX_CODEC_OK;
  }

  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                void* param) const override {
    if (ctrl_id == VP9E_REGISTER_CX_CALLBACK) {
      state_->callback =
          *reinterpret_cast<vpx_codec_priv_output_cx_pkt_cb_pair_t*>(param);
    }
    return VPX_CODEC_OK;
  }

  vpx_codec_err_t codec_control(
      vpx_codec_ctx_t* ctx,
      vp8e_enc_control_id ctrl_id,
      vpx_svc_ref_frame_config_t* param) const override {
    switch (ctrl_id) {
      case VP9E_SET_SVC_REF_FRAME_CONFIG:
        state_->ref_config = *param;
        break;
      case VP9E_GET_SVC_REF_FRAME_CONFIG:
        *param = state_->ref_config;
        break;
      default:
        break;
    }
    return VPX_CODEC_OK;
  }

  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                vpx_svc_layer_id_t* param) const override {
    switch (ctrl_id) {
      case VP9E_SET_SVC_LAYER_ID:
        state_->layer_id = *param;
        break;
      case VP9E_GET_SVC_LAYER_ID:
        *param = state_->layer_id;
        break;
      default:
        break;
    }
    return VPX_CODEC_OK;
  }

  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                vpx_svc_frame_drop_t* param) const override {
    if (ctrl_id == VP9E_SET_SVC_FRAME_DROP_LAYER) {
      state_->frame_drop = *param;
    }
    return VPX_CODEC_OK;
  }

  vpx_codec_err_t codec_enc_config_set(
      vpx_codec_ctx_t* ctx,
      const vpx_codec_enc_cfg_t* cfg) const override {
    state_->config = *cfg;
    return VPX_CODEC_OK;
  }

  vpx_image_t* img_alloc(vpx_image_t* img,
                         vpx_img_fmt_t fmt,
                         unsigned int d_w,
                         unsigned int d_h,
                         unsigned int align) const override {
    return nullptr;
  }
  void img_free(vpx_image_t* img) const override {}
  vpx_codec_err_t codec_enc_init_multi(vpx_codec_ctx_t* ctx,
                                       vpx_codec_iface_t* iface,
                                       vpx_codec_enc_cfg_t* cfg,
                                       int num_enc,
                                       vpx_codec_flags_t flags,
                                       vpx_rational_t* dsf) const override {
    return VPX_CODEC_OK;
  }
  vpx_codec_err_t codec_destroy(vpx_codec_ctx_t* ctx) const override {
    return VPX_CODEC_OK;
  }
  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                uint32_t param) const override {
    return VPX_CODEC_OK;
  }
  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                int param) const override {
    return VPX_CODEC_OK;
  }
  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                int* param) const override {
    return VPX_CODEC_OK;
  }
  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                vpx_roi_map* param) const override {
    return VPX_CODEC_OK;
  }
  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                vpx_active_map* param) const override {
    return VPX_CODEC_OK;
  }
  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                vpx_scaling_mode* param) const override {
    return VPX_CODEC_OK;
  }
  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                vpx_svc_extra_cfg_t* param) const override {
    return VPX_CODEC_OK;
  }
  vpx_codec_err_t codec_control(
      vpx_codec_ctx_t* ctx,
      vp8e_enc_control_id ctrl_id,
      vpx_svc_spatial_layer_sync_t* param) const override {
    return VPX_CODEC_OK;
  }
  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                vpx_rc_funcs_t* param) const override {
    return VPX_CODEC_OK;
  }
  const vpx_codec_cx_pkt_t* codec_get_cx_data(
      vpx_codec_ctx_t* ctx,
      vpx_codec_iter_t* iter) const override {
    return nullptr;
  }
  const char* codec_error_detail(vpx_codec_ctx_t* ctx) const override {
    return nullptr;
  }
  const char* codec_error(vpx_codec_ctx_t* ctx) const override {
    return nullptr;
  }
  const char* codec_err_to_string(vpx_codec_err_t err) const override {
    return nullptr;
  }

 private:
  LibvpxState* const state_;
};

enum Actions {
  kEncode,
  kSetRates,
};

// When a layer frame is marked for drop, drops all layer frames from that
// pictures with larger spatial ids.
constexpr bool DropAbove(uint8_t layers_mask, int sid) {
  uint8_t full_mask = (uint8_t{1} << (sid + 1)) - 1;
  return (layers_mask & full_mask) != full_mask;
}
// inline unittests
static_assert(DropAbove(0b1011, /*sid=*/0) == false, "");
static_assert(DropAbove(0b1011, /*sid=*/1) == false, "");
static_assert(DropAbove(0b1011, /*sid=*/2) == true, "");
static_assert(DropAbove(0b1011, /*sid=*/3) == true, "");

// When a layer frame is marked for drop, drops all layer frames from that
// pictures with smaller spatial ids.
constexpr bool DropBelow(uint8_t layers_mask, int sid, int num_layers) {
  return (layers_mask >> sid) != (1 << (num_layers - sid)) - 1;
}
// inline unittests
static_assert(DropBelow(0b1101, /*sid=*/0, 4) == true, "");
static_assert(DropBelow(0b1101, /*sid=*/1, 4) == true, "");
static_assert(DropBelow(0b1101, /*sid=*/2, 4) == false, "");
static_assert(DropBelow(0b1101, /*sid=*/3, 4) == false, "");

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  FuzzDataHelper helper(rtc::MakeArrayView(data, size));

  FrameValidator validator;
  FieldTrials field_trials(helper);
  // Setup call callbacks for the fake
  LibvpxState state;

  // Initialize encoder
  LibvpxVp9Encoder encoder(cricket::VideoCodec(),
                           std::make_unique<StubLibvpx>(&state), field_trials);
  VideoCodec codec = CodecSettings(helper);
  if (encoder.InitEncode(&codec, EncoderSettings()) != WEBRTC_VIDEO_CODEC_OK) {
    return;
  }
  RTC_CHECK_EQ(encoder.RegisterEncodeCompleteCallback(&validator),
               WEBRTC_VIDEO_CODEC_OK);
  {
    // Enable all the layers initially. Encoder doesn't support producing
    // frames when no layers are enabled.
    LibvpxVp9Encoder::RateControlParameters parameters;
    parameters.framerate_fps = 30.0;
    for (int sid = 0; sid < codec.VP9()->numberOfSpatialLayers; ++sid) {
      for (int tid = 0; tid < codec.VP9()->numberOfTemporalLayers; ++tid) {
        parameters.bitrate.SetBitrate(sid, tid, 100'000);
      }
    }
    encoder.SetRates(parameters);
  }

  std::vector<VideoFrameType> frame_types(1);
  VideoFrame fake_image = VideoFrame::Builder()
                              .set_video_frame_buffer(I420Buffer::Create(
                                  int{codec.width}, int{codec.height}))
                              .build();

  // Start producing frames at random.
  while (helper.CanReadBytes(1)) {
    uint8_t action = helper.Read<uint8_t>();
    switch (action & 0b11) {
      case kEncode: {
        // bitmask of the action: SSSS-K00, where
        // four S bit indicate which spatial layers should be produced,
        // K bit indicates if frame should be a key frame.
        frame_types[0] = (action & 0b100) ? VideoFrameType::kVideoFrameKey
                                          : VideoFrameType::kVideoFrameDelta;
        encoder.Encode(fake_image, &frame_types);
        uint8_t encode_spatial_layers = (action >> 4);
        for (size_t sid = 0; sid < state.config.ss_number_layers; ++sid) {
          bool drop = true;
          switch (state.frame_drop.framedrop_mode) {
            case FULL_SUPERFRAME_DROP:
              drop = encode_spatial_layers == 0;
              break;
            case LAYER_DROP:
              drop = (encode_spatial_layers & (1 << sid)) == 0;
              break;
            case CONSTRAINED_LAYER_DROP:
              drop = DropBelow(encode_spatial_layers, sid,
                               state.config.ss_number_layers);
              break;
            case CONSTRAINED_FROM_ABOVE_DROP:
              drop = DropAbove(encode_spatial_layers, sid);
              break;
          }
          if (!drop) {
            state.layer_id.spatial_layer_id = sid;
            state.callback.output_cx_pkt(&state.pkt, state.callback.user_priv);
          }
        }
      } break;
      case kSetRates: {
        // bitmask of the action: (S3)(S1)(S0)01,
        // where Sx is number of temporal layers to enable for spatial layer x
        // In pariculat Sx = 0 indicates spatial layer x should be disabled.
        LibvpxVp9Encoder::RateControlParameters parameters;
        parameters.framerate_fps = 30.0;
        for (int sid = 0; sid < codec.VP9()->numberOfSpatialLayers; ++sid) {
          int temporal_layers = (action >> ((1 + sid) * 2)) & 0b11;
          for (int tid = 0; tid < temporal_layers; ++tid) {
            parameters.bitrate.SetBitrate(sid, tid, 100'000);
          }
        }
        // Ignore allocation that turns off all the layers. in such case
        // it is up to upper-layer code not to call Encode.
        if (parameters.bitrate.get_sum_bps() > 0) {
          encoder.SetRates(parameters);
        }
      } break;
      default:
        // Unspecificed values are noop.
        break;
    }
  }
}
}  // namespace webrtc
