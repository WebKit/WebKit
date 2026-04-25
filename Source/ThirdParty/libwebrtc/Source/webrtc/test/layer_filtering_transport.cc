/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/layer_filtering_transport.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>

#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/environment/environment.h"
#include "api/media_types.h"
#include "api/rtp_parameters.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/video_codec_type.h"
#include "call/call.h"
#include "call/simulated_packet_receiver.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/create_video_rtp_depacketizer.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "rtc_base/checks.h"
#include "test/direct_transport.h"

namespace webrtc {
namespace test {

LayerFilteringTransport::LayerFilteringTransport(
    const Environment& env,
    TaskQueueBase* task_queue,
    std::unique_ptr<SimulatedPacketReceiverInterface> pipe,
    Call* send_call,
    uint8_t vp8_video_payload_type,
    uint8_t vp9_video_payload_type,
    int selected_tl,
    int selected_sl,
    const std::map<uint8_t, MediaType>& payload_type_map,
    uint32_t ssrc_to_filter_min,
    uint32_t ssrc_to_filter_max,
    ArrayView<const RtpExtension> audio_extensions,
    ArrayView<const RtpExtension> video_extensions)
    : DirectTransport(env,
                      task_queue,
                      std::move(pipe),
                      send_call,
                      payload_type_map,
                      audio_extensions,
                      video_extensions),
      vp8_video_payload_type_(vp8_video_payload_type),
      vp9_video_payload_type_(vp9_video_payload_type),
      vp8_depacketizer_(CreateVideoRtpDepacketizer(kVideoCodecVP8)),
      vp9_depacketizer_(CreateVideoRtpDepacketizer(kVideoCodecVP9)),
      selected_tl_(selected_tl),
      selected_sl_(selected_sl),
      discarded_last_packet_(false),
      ssrc_to_filter_min_(ssrc_to_filter_min),
      ssrc_to_filter_max_(ssrc_to_filter_max) {}

LayerFilteringTransport::LayerFilteringTransport(
    const Environment& env,
    TaskQueueBase* task_queue,
    std::unique_ptr<SimulatedPacketReceiverInterface> pipe,
    Call* send_call,
    uint8_t vp8_video_payload_type,
    uint8_t vp9_video_payload_type,
    int selected_tl,
    int selected_sl,
    const std::map<uint8_t, MediaType>& payload_type_map,
    ArrayView<const RtpExtension> audio_extensions,
    ArrayView<const RtpExtension> video_extensions)
    : LayerFilteringTransport(env,
                              task_queue,
                              std::move(pipe),
                              send_call,
                              vp8_video_payload_type,
                              vp9_video_payload_type,
                              selected_tl,
                              selected_sl,
                              payload_type_map,
                              /*ssrc_to_filter_min=*/0,
                              /*ssrc_to_filter_max=*/0xFFFFFFFF,
                              audio_extensions,
                              video_extensions) {}

bool LayerFilteringTransport::DiscardedLastPacket() const {
  return discarded_last_packet_;
}

bool LayerFilteringTransport::SendRtp(ArrayView<const uint8_t> packet,
                                      const PacketOptions& options) {
  if (selected_tl_ == -1 && selected_sl_ == -1) {
    // Nothing to change, forward the packet immediately.
    return test::DirectTransport::SendRtp(packet, options);
  }

  RtpPacket rtp_packet;
  rtp_packet.Parse(packet);

  if (rtp_packet.Ssrc() < ssrc_to_filter_min_ ||
      rtp_packet.Ssrc() > ssrc_to_filter_max_) {
    // Nothing to change, forward the packet immediately.
    return test::DirectTransport::SendRtp(packet, options);
  }

  if (rtp_packet.PayloadType() == vp8_video_payload_type_ ||
      rtp_packet.PayloadType() == vp9_video_payload_type_) {
    const bool is_vp8 = rtp_packet.PayloadType() == vp8_video_payload_type_;
    VideoRtpDepacketizer& depacketizer =
        is_vp8 ? *vp8_depacketizer_ : *vp9_depacketizer_;
    if (auto parsed_payload = depacketizer.Parse(rtp_packet.PayloadBuffer())) {
      int temporal_idx;
      int spatial_idx;
      bool non_ref_for_inter_layer_pred;
      bool end_of_frame;

      if (is_vp8) {
        temporal_idx = std::get<RTPVideoHeaderVP8>(
                           parsed_payload->video_header.video_type_header)
                           .temporalIdx;
        spatial_idx = kNoSpatialIdx;
        num_active_spatial_layers_ = 1;
        non_ref_for_inter_layer_pred = false;
        end_of_frame = true;
      } else {
        const auto& vp9_header = std::get<RTPVideoHeaderVP9>(
            parsed_payload->video_header.video_type_header);
        temporal_idx = vp9_header.temporal_idx;
        spatial_idx = vp9_header.spatial_idx;
        non_ref_for_inter_layer_pred = vp9_header.non_ref_for_inter_layer_pred;
        end_of_frame = vp9_header.end_of_frame;
        if (vp9_header.ss_data_available) {
          RTC_DCHECK(vp9_header.temporal_idx == kNoTemporalIdx ||
                     vp9_header.temporal_idx == 0);
          num_active_spatial_layers_ = vp9_header.num_spatial_layers;
        }
      }

      if (spatial_idx == kNoSpatialIdx)
        num_active_spatial_layers_ = 1;

      RTC_CHECK_GT(num_active_spatial_layers_, 0);

      if (selected_sl_ >= 0 &&
          spatial_idx ==
              std::min(num_active_spatial_layers_ - 1, selected_sl_) &&
          end_of_frame) {
        // This layer is now the last in the superframe.
        rtp_packet.SetMarker(true);
      } else {
        const bool higher_temporal_layer =
            (selected_tl_ >= 0 && temporal_idx != kNoTemporalIdx &&
             temporal_idx > selected_tl_);

        const bool higher_spatial_layer =
            (selected_sl_ >= 0 && spatial_idx != kNoSpatialIdx &&
             spatial_idx > selected_sl_);

        // Filter out non-reference lower spatial layers since they are not
        // needed for decoding of target spatial layer.
        const bool lower_non_ref_spatial_layer =
            (selected_sl_ >= 0 && spatial_idx != kNoSpatialIdx &&
             spatial_idx <
                 std::min(num_active_spatial_layers_ - 1, selected_sl_) &&
             non_ref_for_inter_layer_pred);

        if (higher_temporal_layer || higher_spatial_layer ||
            lower_non_ref_spatial_layer) {
          // Truncate packet to a padding packet.
          rtp_packet.SetPayloadSize(0);
          rtp_packet.SetPadding(1);
          rtp_packet.SetMarker(false);
          discarded_last_packet_ = true;
        }
      }
    } else {
      RTC_DCHECK_NOTREACHED() << "Parse error";
    }
  }

  return test::DirectTransport::SendRtp(rtp_packet, options);
}

}  // namespace test
}  // namespace webrtc
