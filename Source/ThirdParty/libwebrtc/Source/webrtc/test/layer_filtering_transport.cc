/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"
#include "test/layer_filtering_transport.h"

namespace webrtc {
namespace test {

LayerFilteringTransport::LayerFilteringTransport(
    SingleThreadedTaskQueueForTesting* task_queue,
    std::unique_ptr<SimulatedPacketReceiverInterface> pipe,
    Call* send_call,
    uint8_t vp8_video_payload_type,
    uint8_t vp9_video_payload_type,
    int selected_tl,
    int selected_sl,
    const std::map<uint8_t, MediaType>& payload_type_map,
    uint32_t ssrc_to_filter_min,
    uint32_t ssrc_to_filter_max)
    : DirectTransport(task_queue, std::move(pipe), send_call, payload_type_map),
      vp8_video_payload_type_(vp8_video_payload_type),
      vp9_video_payload_type_(vp9_video_payload_type),
      selected_tl_(selected_tl),
      selected_sl_(selected_sl),
      discarded_last_packet_(false),
      ssrc_to_filter_min_(ssrc_to_filter_min),
      ssrc_to_filter_max_(ssrc_to_filter_max) {}

LayerFilteringTransport::LayerFilteringTransport(
    SingleThreadedTaskQueueForTesting* task_queue,
    std::unique_ptr<SimulatedPacketReceiverInterface> pipe,
    Call* send_call,
    uint8_t vp8_video_payload_type,
    uint8_t vp9_video_payload_type,
    int selected_tl,
    int selected_sl,
    const std::map<uint8_t, MediaType>& payload_type_map)
    : DirectTransport(task_queue, std::move(pipe), send_call, payload_type_map),
      vp8_video_payload_type_(vp8_video_payload_type),
      vp9_video_payload_type_(vp9_video_payload_type),
      selected_tl_(selected_tl),
      selected_sl_(selected_sl),
      discarded_last_packet_(false),
      ssrc_to_filter_min_(0),
      ssrc_to_filter_max_(0xFFFFFFFF) {}

bool LayerFilteringTransport::DiscardedLastPacket() const {
  return discarded_last_packet_;
}

bool LayerFilteringTransport::SendRtp(const uint8_t* packet,
                                      size_t length,
                                      const PacketOptions& options) {
  if (selected_tl_ == -1 && selected_sl_ == -1) {
    // Nothing to change, forward the packet immediately.
    return test::DirectTransport::SendRtp(packet, length, options);
  }

  bool set_marker_bit = false;
  RtpUtility::RtpHeaderParser parser(packet, length);
  RTPHeader header;
  parser.Parse(&header);

  if (header.ssrc < ssrc_to_filter_min_ || header.ssrc > ssrc_to_filter_max_) {
    // Nothing to change, forward the packet immediately.
    return test::DirectTransport::SendRtp(packet, length, options);
  }

  RTC_DCHECK_LE(length, IP_PACKET_SIZE);
  uint8_t temp_buffer[IP_PACKET_SIZE];
  memcpy(temp_buffer, packet, length);

  if (header.payloadType == vp8_video_payload_type_ ||
      header.payloadType == vp9_video_payload_type_) {
    const uint8_t* payload = packet + header.headerLength;
    RTC_DCHECK_GT(length, header.headerLength);
    const size_t payload_length = length - header.headerLength;
    RTC_DCHECK_GT(payload_length, header.paddingLength);
    const size_t payload_data_length = payload_length - header.paddingLength;

    const bool is_vp8 = header.payloadType == vp8_video_payload_type_;
    std::unique_ptr<RtpDepacketizer> depacketizer(
        RtpDepacketizer::Create(is_vp8 ? kVideoCodecVP8 : kVideoCodecVP9));
    RtpDepacketizer::ParsedPayload parsed_payload;
    if (depacketizer->Parse(&parsed_payload, payload, payload_data_length)) {
      int temporal_idx;
      int spatial_idx;
      bool non_ref_for_inter_layer_pred;
      bool end_of_frame;

      if (is_vp8) {
        temporal_idx = absl::get<RTPVideoHeaderVP8>(
                           parsed_payload.video_header().video_type_header)
                           .temporalIdx;
        spatial_idx = kNoSpatialIdx;
        num_active_spatial_layers_ = 1;
        non_ref_for_inter_layer_pred = false;
        end_of_frame = true;
      } else {
        const auto& vp9_header = absl::get<RTPVideoHeaderVP9>(
            parsed_payload.video_header().video_type_header);
        temporal_idx = vp9_header.temporal_idx;
        spatial_idx = vp9_header.spatial_idx;
        non_ref_for_inter_layer_pred = vp9_header.non_ref_for_inter_layer_pred;
        end_of_frame = vp9_header.end_of_frame;

        // The number of spatial layers is sent in ssData, which is included
        // only in the first packet of the first spatial layer of a key frame.
        if (!vp9_header.inter_pic_predicted &&
            vp9_header.beginning_of_frame == 1 && spatial_idx == 0) {
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
        set_marker_bit = true;
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
          length = header.headerLength + 1;
          temp_buffer[0] |= (1 << 5);  // P = 1.
          temp_buffer[1] &= 0x7F;      // M = 0.
          discarded_last_packet_ = true;
          temp_buffer[header.headerLength] = 1;  // One byte of padding.
        }
      }
    } else {
      RTC_NOTREACHED() << "Parse error";
    }
  }

  // We are discarding some of the packets (specifically, whole layers), so
  // make sure the marker bit is set properly, and that sequence numbers are
  // continuous.
  if (set_marker_bit)
    temp_buffer[1] |= kRtpMarkerBitMask;

  return test::DirectTransport::SendRtp(temp_buffer, length, options);
}

}  // namespace test
}  // namespace webrtc
