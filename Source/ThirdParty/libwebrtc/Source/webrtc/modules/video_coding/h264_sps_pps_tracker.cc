/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/h264_sps_pps_tracker.h"

#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/h264/h264_common.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/packet_buffer.h"

namespace webrtc {
namespace video_coding {

namespace {
const uint8_t start_code_h264[] = {0, 0, 0, 1};
}  // namespace

H264SpsPpsTracker::PacketAction H264SpsPpsTracker::CopyAndFixBitstream(
    VCMPacket* packet) {
  RTC_DCHECK(packet->codec == kVideoCodecH264);

  const uint8_t* data = packet->dataPtr;
  const size_t data_size = packet->sizeBytes;
  const RTPVideoHeader& video_header = packet->video_header;
  const RTPVideoHeaderH264& codec_header = video_header.codecHeader.H264;

  // Packets that only contains SPS/PPS are not decodable by themselves, and
  // to avoid frames being created containing only these two nalus we don't
  // insert them into the PacketBuffer. Instead we save the SPS/PPS and
  // prepend the bitstream of first packet of an IDR referring to the
  // corresponding SPS/PPS id.
  bool insert_packet = codec_header.nalus_length == 0 ? true : false;

  int pps_id = -1;
  size_t required_size = 0;
  for (size_t i = 0; i < codec_header.nalus_length; ++i) {
    const NaluInfo& nalu = codec_header.nalus[i];
    switch (nalu.type) {
      case H264::NaluType::kSps: {
        // Save SPS.
        sps_data_[nalu.sps_id].size = nalu.size;
        sps_data_[nalu.sps_id].data.reset(new uint8_t[nalu.size]);
        memcpy(sps_data_[nalu.sps_id].data.get(), data + nalu.offset,
               nalu.size);
        break;
      }
      case H264::NaluType::kPps: {
        // Save PPS.
        pps_data_[nalu.pps_id].sps_id = nalu.sps_id;
        pps_data_[nalu.pps_id].size = nalu.size;
        pps_data_[nalu.pps_id].data.reset(new uint8_t[nalu.size]);
        memcpy(pps_data_[nalu.pps_id].data.get(), data + nalu.offset,
               nalu.size);
        break;
      }
      case H264::NaluType::kIdr: {
        // If this is the first packet of an IDR, make sure we have the required
        // SPS/PPS and also calculate how much extra space we need in the buffer
        // to prepend the SPS/PPS to the bitstream with start codes.
        if (video_header.isFirstPacket) {
          if (nalu.pps_id == -1) {
            LOG(LS_WARNING) << "No PPS id in IDR nalu.";
            return kRequestKeyframe;
          }

          auto pps = pps_data_.find(nalu.pps_id);
          if (pps == pps_data_.end()) {
            LOG(LS_WARNING) << "No PPS with id << " << nalu.pps_id
                            << " received";
            return kRequestKeyframe;
          }

          auto sps = sps_data_.find(pps->second.sps_id);
          if (sps == sps_data_.end()) {
            LOG(LS_WARNING) << "No SPS with id << "
                            << pps_data_[nalu.pps_id].sps_id << " received";
            return kRequestKeyframe;
          }

          pps_id = nalu.pps_id;
          required_size += pps->second.size + sizeof(start_code_h264);
          required_size += sps->second.size + sizeof(start_code_h264);
        }
        FALLTHROUGH();
      }
      default: {
        // Something other than an SPS/PPS nalu in this packet, then it should
        // be inserted into the PacketBuffer.
        insert_packet = true;
      }
    }
  }

  if (!insert_packet)
    return kDrop;

  // Calculate how much space we need for the rest of the bitstream.
  if (codec_header.packetization_type == kH264StapA) {
    const uint8_t* nalu_ptr = data + 1;
    while (nalu_ptr < data + data_size) {
      RTC_DCHECK(video_header.isFirstPacket);
      required_size += sizeof(start_code_h264);

      // The first two bytes describe the length of a segment.
      uint16_t segment_length = nalu_ptr[0] << 8 | nalu_ptr[1];
      nalu_ptr += 2;

      required_size += segment_length;
      nalu_ptr += segment_length;
    }
  } else {
    if (video_header.isFirstPacket)
      required_size += sizeof(start_code_h264);
    required_size += data_size;
  }

  // Then we copy to the new buffer.
  uint8_t* buffer = new uint8_t[required_size];
  uint8_t* insert_at = buffer;

  // If pps_id != -1 then we have the SPS/PPS and they should be prepended
  // to the bitstream with start codes inserted.
  if (pps_id != -1) {
    // Insert SPS.
    memcpy(insert_at, start_code_h264, sizeof(start_code_h264));
    insert_at += sizeof(start_code_h264);
    memcpy(insert_at, sps_data_[pps_data_[pps_id].sps_id].data.get(),
           sps_data_[pps_data_[pps_id].sps_id].size);
    insert_at += sps_data_[pps_data_[pps_id].sps_id].size;

    // Insert PPS.
    memcpy(insert_at, start_code_h264, sizeof(start_code_h264));
    insert_at += sizeof(start_code_h264);
    memcpy(insert_at, pps_data_[pps_id].data.get(), pps_data_[pps_id].size);
    insert_at += pps_data_[pps_id].size;
  }

  // Copy the rest of the bitstream and insert start codes.
  if (codec_header.packetization_type == kH264StapA) {
    const uint8_t* nalu_ptr = data + 1;
    while (nalu_ptr < data + data_size) {
      memcpy(insert_at, start_code_h264, sizeof(start_code_h264));
      insert_at += sizeof(start_code_h264);

      // The first two bytes describe the length of a segment.
      uint16_t segment_length = nalu_ptr[0] << 8 | nalu_ptr[1];
      nalu_ptr += 2;

      size_t copy_end = nalu_ptr - data + segment_length;
      if (copy_end > data_size) {
        delete[] buffer;
        return kDrop;
      }

      memcpy(insert_at, nalu_ptr, segment_length);
      insert_at += segment_length;
      nalu_ptr += segment_length;
    }
  } else {
    if (video_header.isFirstPacket) {
      memcpy(insert_at, start_code_h264, sizeof(start_code_h264));
      insert_at += sizeof(start_code_h264);
    }
    memcpy(insert_at, data, data_size);
  }

  packet->dataPtr = buffer;
  packet->sizeBytes = required_size;
  return kInsert;
}

}  // namespace video_coding
}  // namespace webrtc
