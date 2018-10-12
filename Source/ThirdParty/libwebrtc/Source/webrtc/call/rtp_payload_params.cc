/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_payload_params.h"

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {
void PopulateRtpWithCodecSpecifics(const CodecSpecificInfo& info,
                                   absl::optional<int> spatial_index,
                                   RTPVideoHeader* rtp) {
  rtp->codec = info.codecType;
  switch (info.codecType) {
    case kVideoCodecVP8: {
      auto& vp8_header = rtp->video_type_header.emplace<RTPVideoHeaderVP8>();
      vp8_header.InitRTPVideoHeaderVP8();
      vp8_header.nonReference = info.codecSpecific.VP8.nonReference;
      vp8_header.temporalIdx = info.codecSpecific.VP8.temporalIdx;
      vp8_header.layerSync = info.codecSpecific.VP8.layerSync;
      vp8_header.keyIdx = info.codecSpecific.VP8.keyIdx;
      rtp->simulcastIdx = spatial_index.value_or(0);
      return;
    }
    case kVideoCodecVP9: {
      auto& vp9_header = rtp->video_type_header.emplace<RTPVideoHeaderVP9>();
      vp9_header.InitRTPVideoHeaderVP9();
      vp9_header.inter_pic_predicted =
          info.codecSpecific.VP9.inter_pic_predicted;
      vp9_header.flexible_mode = info.codecSpecific.VP9.flexible_mode;
      vp9_header.ss_data_available = info.codecSpecific.VP9.ss_data_available;
      vp9_header.non_ref_for_inter_layer_pred =
          info.codecSpecific.VP9.non_ref_for_inter_layer_pred;
      vp9_header.temporal_idx = info.codecSpecific.VP9.temporal_idx;
      vp9_header.temporal_up_switch = info.codecSpecific.VP9.temporal_up_switch;
      vp9_header.inter_layer_predicted =
          info.codecSpecific.VP9.inter_layer_predicted;
      vp9_header.gof_idx = info.codecSpecific.VP9.gof_idx;
      vp9_header.num_spatial_layers = info.codecSpecific.VP9.num_spatial_layers;
      if (vp9_header.num_spatial_layers > 1) {
        vp9_header.spatial_idx = spatial_index.value_or(kNoSpatialIdx);
      } else {
        vp9_header.spatial_idx = kNoSpatialIdx;
      }
      if (info.codecSpecific.VP9.ss_data_available) {
        vp9_header.spatial_layer_resolution_present =
            info.codecSpecific.VP9.spatial_layer_resolution_present;
        if (info.codecSpecific.VP9.spatial_layer_resolution_present) {
          for (size_t i = 0; i < info.codecSpecific.VP9.num_spatial_layers;
               ++i) {
            vp9_header.width[i] = info.codecSpecific.VP9.width[i];
            vp9_header.height[i] = info.codecSpecific.VP9.height[i];
          }
        }
        vp9_header.gof.CopyGofInfoVP9(info.codecSpecific.VP9.gof);
      }

      vp9_header.num_ref_pics = info.codecSpecific.VP9.num_ref_pics;
      for (int i = 0; i < info.codecSpecific.VP9.num_ref_pics; ++i) {
        vp9_header.pid_diff[i] = info.codecSpecific.VP9.p_diff[i];
      }
      vp9_header.end_of_picture = info.codecSpecific.VP9.end_of_picture;
      return;
    }
    case kVideoCodecH264: {
      auto& h264_header = rtp->video_type_header.emplace<RTPVideoHeaderH264>();
      h264_header.packetization_mode =
          info.codecSpecific.H264.packetization_mode;
      rtp->simulcastIdx = spatial_index.value_or(0);
      return;
    }
    case kVideoCodecMultiplex:
    case kVideoCodecGeneric:
      rtp->codec = kVideoCodecGeneric;
      rtp->simulcastIdx = spatial_index.value_or(0);
      return;
    default:
      return;
  }
}

void SetVideoTiming(const EncodedImage& image, VideoSendTiming* timing) {
  if (image.timing_.flags == VideoSendTiming::TimingFrameFlags::kInvalid ||
      image.timing_.flags == VideoSendTiming::TimingFrameFlags::kNotTriggered) {
    timing->flags = VideoSendTiming::TimingFrameFlags::kInvalid;
    return;
  }

  timing->encode_start_delta_ms = VideoSendTiming::GetDeltaCappedMs(
      image.capture_time_ms_, image.timing_.encode_start_ms);
  timing->encode_finish_delta_ms = VideoSendTiming::GetDeltaCappedMs(
      image.capture_time_ms_, image.timing_.encode_finish_ms);
  timing->packetization_finish_delta_ms = 0;
  timing->pacer_exit_delta_ms = 0;
  timing->network_timestamp_delta_ms = 0;
  timing->network2_timestamp_delta_ms = 0;
  timing->flags = image.timing_.flags;
}
}  // namespace

RtpPayloadParams::RtpPayloadParams(const uint32_t ssrc,
                                   const RtpPayloadState* state)
    : ssrc_(ssrc),
      generic_picture_id_experiment_(
          field_trial::IsEnabled("WebRTC-GenericPictureId")),
      generic_descriptor_experiment_(
          field_trial::IsEnabled("WebRTC-GenericDescriptor")) {
  for (auto& spatial_layer : last_shared_frame_id_)
    spatial_layer.fill(-1);

  Random random(rtc::TimeMicros());
  state_.picture_id =
      state ? state->picture_id : (random.Rand<int16_t>() & 0x7FFF);
  state_.tl0_pic_idx = state ? state->tl0_pic_idx : (random.Rand<uint8_t>());
}

RtpPayloadParams::RtpPayloadParams(const RtpPayloadParams& other) = default;

RtpPayloadParams::~RtpPayloadParams() {}

RTPVideoHeader RtpPayloadParams::GetRtpVideoHeader(
    const EncodedImage& image,
    const CodecSpecificInfo* codec_specific_info,
    int64_t shared_frame_id) {
  RTPVideoHeader rtp_video_header;
  if (codec_specific_info) {
    PopulateRtpWithCodecSpecifics(*codec_specific_info, image.SpatialIndex(),
                                  &rtp_video_header);
  }
  rtp_video_header.rotation = image.rotation_;
  rtp_video_header.content_type = image.content_type_;
  rtp_video_header.playout_delay = image.playout_delay_;

  SetVideoTiming(image, &rtp_video_header.video_timing);

  const bool is_keyframe = image._frameType == kVideoFrameKey;
  const bool first_frame_in_picture =
      (codec_specific_info && codec_specific_info->codecType == kVideoCodecVP9)
          ? codec_specific_info->codecSpecific.VP9.first_frame_in_picture
          : true;

  SetCodecSpecific(&rtp_video_header, first_frame_in_picture);

  if (generic_descriptor_experiment_)
    SetGeneric(shared_frame_id, is_keyframe, &rtp_video_header);

  return rtp_video_header;
}

uint32_t RtpPayloadParams::ssrc() const {
  return ssrc_;
}

RtpPayloadState RtpPayloadParams::state() const {
  return state_;
}

void RtpPayloadParams::SetCodecSpecific(RTPVideoHeader* rtp_video_header,
                                        bool first_frame_in_picture) {
  // Always set picture id. Set tl0_pic_idx iff temporal index is set.
  if (first_frame_in_picture) {
    state_.picture_id = (static_cast<uint16_t>(state_.picture_id) + 1) & 0x7FFF;
  }
  if (rtp_video_header->codec == kVideoCodecVP8) {
    auto& vp8_header =
        absl::get<RTPVideoHeaderVP8>(rtp_video_header->video_type_header);
    vp8_header.pictureId = state_.picture_id;

    if (vp8_header.temporalIdx != kNoTemporalIdx) {
      if (vp8_header.temporalIdx == 0) {
        ++state_.tl0_pic_idx;
      }
      vp8_header.tl0PicIdx = state_.tl0_pic_idx;
    }
  }
  if (rtp_video_header->codec == kVideoCodecVP9) {
    auto& vp9_header =
        absl::get<RTPVideoHeaderVP9>(rtp_video_header->video_type_header);
    vp9_header.picture_id = state_.picture_id;

    // Note that in the case that we have no temporal layers but we do have
    // spatial layers, packets will carry layering info with a temporal_idx of
    // zero, and we then have to set and increment tl0_pic_idx.
    if (vp9_header.temporal_idx != kNoTemporalIdx ||
        vp9_header.spatial_idx != kNoSpatialIdx) {
      if (first_frame_in_picture &&
          (vp9_header.temporal_idx == 0 ||
           vp9_header.temporal_idx == kNoTemporalIdx)) {
        ++state_.tl0_pic_idx;
      }
      vp9_header.tl0_pic_idx = state_.tl0_pic_idx;
    }
  }
  // There are currently two generic descriptors in WebRTC. The old descriptor
  // can not share a picture id space between simulcast streams, so we use the
  // |picture_id| in this case. We let the |picture_id| tag along in |frame_id|
  // until the old generic format can be removed.
  // TODO(philipel): Remove this when the new generic format has been fully
  //                 implemented.
  if (generic_picture_id_experiment_ &&
      rtp_video_header->codec == kVideoCodecGeneric) {
    rtp_video_header->generic.emplace().frame_id = state_.picture_id;
  }
}

void RtpPayloadParams::SetGeneric(int64_t frame_id,
                                  bool is_keyframe,
                                  RTPVideoHeader* rtp_video_header) {
  if (rtp_video_header->codec == kVideoCodecVP8) {
    Vp8ToGeneric(frame_id, is_keyframe, rtp_video_header);
  }

  // TODO(philipel): Implement VP9 to new generic descriptor.
  // TODO(philipel): Implement H264 to new generic descriptor.
  // TODO(philipel): Implement generic codec to new generic descriptor.
}

void RtpPayloadParams::Vp8ToGeneric(int64_t shared_frame_id,
                                    bool is_keyframe,
                                    RTPVideoHeader* rtp_video_header) {
  const auto& vp8_header =
      absl::get<RTPVideoHeaderVP8>(rtp_video_header->video_type_header);
  const int spatial_index = 0;
  const int temporal_index =
      vp8_header.temporalIdx != kNoTemporalIdx ? vp8_header.temporalIdx : 0;

  if (temporal_index >= RtpGenericFrameDescriptor::kMaxTemporalLayers ||
      spatial_index >= RtpGenericFrameDescriptor::kMaxSpatialLayers) {
    RTC_LOG(LS_WARNING) << "Temporal and/or spatial index is too high to be "
                           "used with generic frame descriptor.";
    return;
  }

  RTPVideoHeader::GenericDescriptorInfo& generic =
      rtp_video_header->generic.emplace();

  generic.frame_id = shared_frame_id;
  generic.spatial_index = spatial_index;
  generic.temporal_index = temporal_index;

  if (is_keyframe) {
    RTC_DCHECK_EQ(temporal_index, 0);
    last_shared_frame_id_[spatial_index].fill(-1);
    last_shared_frame_id_[spatial_index][temporal_index] = shared_frame_id;
    return;
  }

  if (vp8_header.layerSync) {
    int64_t tl0_frame_id = last_shared_frame_id_[spatial_index][0];

    for (int i = 1; i < RtpGenericFrameDescriptor::kMaxTemporalLayers; ++i) {
      if (last_shared_frame_id_[spatial_index][i] < tl0_frame_id) {
        last_shared_frame_id_[spatial_index][i] = -1;
      }
    }

    RTC_DCHECK_GE(tl0_frame_id, 0);
    RTC_DCHECK_LT(tl0_frame_id, shared_frame_id);
    generic.dependencies.push_back(tl0_frame_id);
  } else {
    for (int i = 0; i <= temporal_index; ++i) {
      int64_t frame_id = last_shared_frame_id_[spatial_index][i];

      if (frame_id != -1) {
        RTC_DCHECK_LT(frame_id, shared_frame_id);
        generic.dependencies.push_back(frame_id);
      }
    }
  }

  last_shared_frame_id_[spatial_index][temporal_index] = shared_frame_id;
}

}  // namespace webrtc
