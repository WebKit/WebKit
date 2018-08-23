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
#include "rtc_base/random.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

namespace {
void PopulateRtpWithCodecSpecifics(const CodecSpecificInfo& info,
                                   RTPVideoHeader* rtp) {
  rtp->codec = info.codecType;
  switch (info.codecType) {
    case kVideoCodecVP8: {
      rtp->vp8().InitRTPVideoHeaderVP8();
      rtp->vp8().nonReference = info.codecSpecific.VP8.nonReference;
      rtp->vp8().temporalIdx = info.codecSpecific.VP8.temporalIdx;
      rtp->vp8().layerSync = info.codecSpecific.VP8.layerSync;
      rtp->vp8().keyIdx = info.codecSpecific.VP8.keyIdx;
      rtp->simulcastIdx = info.codecSpecific.VP8.simulcastIdx;
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
      vp9_header.spatial_idx = info.codecSpecific.VP9.spatial_idx;
      vp9_header.temporal_up_switch = info.codecSpecific.VP9.temporal_up_switch;
      vp9_header.inter_layer_predicted =
          info.codecSpecific.VP9.inter_layer_predicted;
      vp9_header.gof_idx = info.codecSpecific.VP9.gof_idx;
      vp9_header.num_spatial_layers = info.codecSpecific.VP9.num_spatial_layers;

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
      rtp->simulcastIdx = info.codecSpecific.H264.simulcast_idx;
      return;
    }
    case kVideoCodecMultiplex:
    case kVideoCodecGeneric:
      rtp->codec = kVideoCodecGeneric;
      rtp->simulcastIdx = info.codecSpecific.generic.simulcast_idx;
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
    : ssrc_(ssrc) {
  Random random(rtc::TimeMicros());
  state_.picture_id =
      state ? state->picture_id : (random.Rand<int16_t>() & 0x7FFF);
  state_.tl0_pic_idx = state ? state->tl0_pic_idx : (random.Rand<uint8_t>());
}
RtpPayloadParams::~RtpPayloadParams() {}

RTPVideoHeader RtpPayloadParams::GetRtpVideoHeader(
    const EncodedImage& image,
    const CodecSpecificInfo* codec_specific_info) {
  RTPVideoHeader rtp_video_header;
  if (codec_specific_info) {
    PopulateRtpWithCodecSpecifics(*codec_specific_info, &rtp_video_header);
  }
  rtp_video_header.rotation = image.rotation_;
  rtp_video_header.content_type = image.content_type_;
  rtp_video_header.playout_delay = image.playout_delay_;

  SetVideoTiming(image, &rtp_video_header.video_timing);

  // Sets picture id and tl0 pic idx.
  const bool first_frame_in_picture =
      (codec_specific_info && codec_specific_info->codecType == kVideoCodecVP9)
          ? codec_specific_info->codecSpecific.VP9.first_frame_in_picture
          : true;
  Set(&rtp_video_header, first_frame_in_picture);
  return rtp_video_header;
}

uint32_t RtpPayloadParams::ssrc() const {
  return ssrc_;
}

RtpPayloadState RtpPayloadParams::state() const {
  return state_;
}

void RtpPayloadParams::Set(RTPVideoHeader* rtp_video_header,
                           bool first_frame_in_picture) {
  // Always set picture id. Set tl0_pic_idx iff temporal index is set.
  if (first_frame_in_picture) {
    state_.picture_id = (static_cast<uint16_t>(state_.picture_id) + 1) & 0x7FFF;
  }
  if (rtp_video_header->codec == kVideoCodecVP8) {
    rtp_video_header->vp8().pictureId = state_.picture_id;

    if (rtp_video_header->vp8().temporalIdx != kNoTemporalIdx) {
      if (rtp_video_header->vp8().temporalIdx == 0) {
        ++state_.tl0_pic_idx;
      }
      rtp_video_header->vp8().tl0PicIdx = state_.tl0_pic_idx;
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
}
}  // namespace webrtc
