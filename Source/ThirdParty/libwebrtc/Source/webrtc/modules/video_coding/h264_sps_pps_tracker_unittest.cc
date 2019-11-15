/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/h264_sps_pps_tracker.h"

#include <string.h>

#include <vector>

#include "absl/types/variant.h"
#include "common_video/h264/h264_common.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/packet.h"
#include "test/gtest.h"

namespace webrtc {
namespace video_coding {

namespace {
const uint8_t start_code[] = {0, 0, 0, 1};

void ExpectSpsPpsIdr(const RTPVideoHeaderH264& codec_header,
                     uint8_t sps_id,
                     uint8_t pps_id) {
  bool contains_sps = false;
  bool contains_pps = false;
  bool contains_idr = false;
  for (const auto& nalu : codec_header.nalus) {
    if (nalu.type == H264::NaluType::kSps) {
      EXPECT_EQ(sps_id, nalu.sps_id);
      contains_sps = true;
    } else if (nalu.type == H264::NaluType::kPps) {
      EXPECT_EQ(sps_id, nalu.sps_id);
      EXPECT_EQ(pps_id, nalu.pps_id);
      contains_pps = true;
    } else if (nalu.type == H264::NaluType::kIdr) {
      EXPECT_EQ(pps_id, nalu.pps_id);
      contains_idr = true;
    }
  }
  EXPECT_TRUE(contains_sps);
  EXPECT_TRUE(contains_pps);
  EXPECT_TRUE(contains_idr);
}

class H264VcmPacket : public VCMPacket {
 public:
  H264VcmPacket() {
    video_header.codec = kVideoCodecH264;
    video_header.is_first_packet_in_frame = false;
    auto& type_header =
        video_header.video_type_header.emplace<RTPVideoHeaderH264>();
    type_header.nalus_length = 0;
    type_header.packetization_type = kH264SingleNalu;
  }

  RTPVideoHeaderH264& h264() {
    return absl::get<RTPVideoHeaderH264>(video_header.video_type_header);
  }
};

}  // namespace

class TestH264SpsPpsTracker : public ::testing::Test {
 public:
  void AddSps(H264VcmPacket* packet,
              uint8_t sps_id,
              std::vector<uint8_t>* data) {
    NaluInfo info;
    info.type = H264::NaluType::kSps;
    info.sps_id = sps_id;
    info.pps_id = -1;
    data->push_back(H264::NaluType::kSps);
    data->push_back(sps_id);  // The sps data, just a single byte.

    packet->h264().nalus[packet->h264().nalus_length++] = info;
  }

  void AddPps(H264VcmPacket* packet,
              uint8_t sps_id,
              uint8_t pps_id,
              std::vector<uint8_t>* data) {
    NaluInfo info;
    info.type = H264::NaluType::kPps;
    info.sps_id = sps_id;
    info.pps_id = pps_id;
    data->push_back(H264::NaluType::kPps);
    data->push_back(pps_id);  // The pps data, just a single byte.

    packet->h264().nalus[packet->h264().nalus_length++] = info;
  }

  void AddIdr(H264VcmPacket* packet, int pps_id) {
    NaluInfo info;
    info.type = H264::NaluType::kIdr;
    info.sps_id = -1;
    info.pps_id = pps_id;

    packet->h264().nalus[packet->h264().nalus_length++] = info;
  }

 protected:
  H264SpsPpsTracker tracker_;
};

TEST_F(TestH264SpsPpsTracker, NoNalus) {
  uint8_t data[] = {1, 2, 3};
  H264VcmPacket packet;
  packet.h264().packetization_type = kH264FuA;
  packet.dataPtr = data;
  packet.sizeBytes = sizeof(data);

  EXPECT_EQ(H264SpsPpsTracker::kInsert, tracker_.CopyAndFixBitstream(&packet));
  EXPECT_EQ(memcmp(packet.dataPtr, data, sizeof(data)), 0);
  delete[] packet.dataPtr;
}

TEST_F(TestH264SpsPpsTracker, FuAFirstPacket) {
  uint8_t data[] = {1, 2, 3};
  H264VcmPacket packet;
  packet.h264().packetization_type = kH264FuA;
  packet.h264().nalus_length = 1;
  packet.video_header.is_first_packet_in_frame = true;
  packet.dataPtr = data;
  packet.sizeBytes = sizeof(data);

  EXPECT_EQ(H264SpsPpsTracker::kInsert, tracker_.CopyAndFixBitstream(&packet));
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {1, 2, 3});
  EXPECT_EQ(memcmp(packet.dataPtr, expected.data(), expected.size()), 0);
  delete[] packet.dataPtr;
}

TEST_F(TestH264SpsPpsTracker, StapAIncorrectSegmentLength) {
  uint8_t data[] = {0, 0, 2, 0};
  H264VcmPacket packet;
  packet.h264().packetization_type = kH264StapA;
  packet.video_header.is_first_packet_in_frame = true;
  packet.dataPtr = data;
  packet.sizeBytes = sizeof(data);

  EXPECT_EQ(H264SpsPpsTracker::kDrop, tracker_.CopyAndFixBitstream(&packet));
}

TEST_F(TestH264SpsPpsTracker, SingleNaluInsertStartCode) {
  uint8_t data[] = {1, 2, 3};
  H264VcmPacket packet;
  packet.h264().nalus_length = 1;
  packet.dataPtr = data;
  packet.sizeBytes = sizeof(data);

  EXPECT_EQ(H264SpsPpsTracker::kInsert, tracker_.CopyAndFixBitstream(&packet));
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {1, 2, 3});
  EXPECT_EQ(memcmp(packet.dataPtr, expected.data(), expected.size()), 0);
  delete[] packet.dataPtr;
}

TEST_F(TestH264SpsPpsTracker, NoStartCodeInsertedForSubsequentFuAPacket) {
  std::vector<uint8_t> data = {1, 2, 3};
  H264VcmPacket packet;
  packet.h264().packetization_type = kH264FuA;

  // Since no NALU begin in this packet the nalus_length is zero.
  packet.h264().nalus_length = 0;

  packet.dataPtr = data.data();
  packet.sizeBytes = data.size();

  EXPECT_EQ(H264SpsPpsTracker::kInsert, tracker_.CopyAndFixBitstream(&packet));
  EXPECT_EQ(memcmp(packet.dataPtr, data.data(), data.size()), 0);
  delete[] packet.dataPtr;
}

TEST_F(TestH264SpsPpsTracker, IdrFirstPacketNoSpsPpsInserted) {
  std::vector<uint8_t> data = {1, 2, 3};
  H264VcmPacket packet;
  packet.video_header.is_first_packet_in_frame = true;

  AddIdr(&packet, 0);
  packet.dataPtr = data.data();
  packet.sizeBytes = data.size();

  EXPECT_EQ(H264SpsPpsTracker::kRequestKeyframe,
            tracker_.CopyAndFixBitstream(&packet));
}

TEST_F(TestH264SpsPpsTracker, IdrFirstPacketNoPpsInserted) {
  std::vector<uint8_t> data = {1, 2, 3};
  H264VcmPacket packet;
  packet.video_header.is_first_packet_in_frame = true;

  AddSps(&packet, 0, &data);
  AddIdr(&packet, 0);
  packet.dataPtr = data.data();
  packet.sizeBytes = data.size();

  EXPECT_EQ(H264SpsPpsTracker::kRequestKeyframe,
            tracker_.CopyAndFixBitstream(&packet));
}

TEST_F(TestH264SpsPpsTracker, IdrFirstPacketNoSpsInserted) {
  std::vector<uint8_t> data = {1, 2, 3};
  H264VcmPacket packet;
  packet.video_header.is_first_packet_in_frame = true;

  AddPps(&packet, 0, 0, &data);
  AddIdr(&packet, 0);
  packet.dataPtr = data.data();
  packet.sizeBytes = data.size();

  EXPECT_EQ(H264SpsPpsTracker::kRequestKeyframe,
            tracker_.CopyAndFixBitstream(&packet));
}

TEST_F(TestH264SpsPpsTracker, SpsPpsPacketThenIdrFirstPacket) {
  std::vector<uint8_t> data;
  H264VcmPacket sps_pps_packet;

  // Insert SPS/PPS
  AddSps(&sps_pps_packet, 0, &data);
  AddPps(&sps_pps_packet, 0, 1, &data);
  sps_pps_packet.dataPtr = data.data();
  sps_pps_packet.sizeBytes = data.size();
  EXPECT_EQ(H264SpsPpsTracker::kInsert,
            tracker_.CopyAndFixBitstream(&sps_pps_packet));
  delete[] sps_pps_packet.dataPtr;
  data.clear();

  // Insert first packet of the IDR
  H264VcmPacket idr_packet;
  idr_packet.video_header.is_first_packet_in_frame = true;
  AddIdr(&idr_packet, 1);
  data.insert(data.end(), {1, 2, 3});
  idr_packet.dataPtr = data.data();
  idr_packet.sizeBytes = data.size();
  EXPECT_EQ(H264SpsPpsTracker::kInsert,
            tracker_.CopyAndFixBitstream(&idr_packet));

  std::vector<uint8_t> expected;
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {1, 2, 3});
  EXPECT_EQ(memcmp(idr_packet.dataPtr, expected.data(), expected.size()), 0);
  delete[] idr_packet.dataPtr;
}

TEST_F(TestH264SpsPpsTracker, SpsPpsIdrInStapA) {
  std::vector<uint8_t> data;
  H264VcmPacket packet;
  packet.h264().packetization_type = kH264StapA;
  packet.video_header.is_first_packet_in_frame = true;  // Always true for StapA

  data.insert(data.end(), {0});     // First byte is ignored
  data.insert(data.end(), {0, 2});  // Length of segment
  AddSps(&packet, 13, &data);
  data.insert(data.end(), {0, 2});  // Length of segment
  AddPps(&packet, 13, 27, &data);
  data.insert(data.end(), {0, 5});  // Length of segment
  AddIdr(&packet, 27);
  data.insert(data.end(), {1, 2, 3, 2, 1});

  packet.dataPtr = data.data();
  packet.sizeBytes = data.size();
  EXPECT_EQ(H264SpsPpsTracker::kInsert, tracker_.CopyAndFixBitstream(&packet));

  std::vector<uint8_t> expected;
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {H264::NaluType::kSps, 13});
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {H264::NaluType::kPps, 27});
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {1, 2, 3, 2, 1});

  EXPECT_EQ(memcmp(packet.dataPtr, expected.data(), expected.size()), 0);
  delete[] packet.dataPtr;
}

TEST_F(TestH264SpsPpsTracker, SpsPpsOutOfBand) {
  constexpr uint8_t kData[] = {1, 2, 3};

  // Generated by "ffmpeg -r 30 -f avfoundation -i "default" out.h264" on macos.
  // width: 320, height: 240
  const std::vector<uint8_t> sps(
      {0x67, 0x7a, 0x00, 0x0d, 0xbc, 0xd9, 0x41, 0x41, 0xfa, 0x10, 0x00, 0x00,
       0x03, 0x00, 0x10, 0x00, 0x00, 0x03, 0x03, 0xc0, 0xf1, 0x42, 0x99, 0x60});
  const std::vector<uint8_t> pps({0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0});
  tracker_.InsertSpsPpsNalus(sps, pps);

  // Insert first packet of the IDR.
  H264VcmPacket idr_packet;
  idr_packet.video_header.is_first_packet_in_frame = true;
  AddIdr(&idr_packet, 0);
  idr_packet.dataPtr = kData;
  idr_packet.sizeBytes = sizeof(kData);
  EXPECT_EQ(1u, idr_packet.h264().nalus_length);
  EXPECT_EQ(H264SpsPpsTracker::kInsert,
            tracker_.CopyAndFixBitstream(&idr_packet));
  EXPECT_EQ(3u, idr_packet.h264().nalus_length);
  EXPECT_EQ(320, idr_packet.width());
  EXPECT_EQ(240, idr_packet.height());
  ExpectSpsPpsIdr(idr_packet.h264(), 0, 0);

  if (idr_packet.dataPtr != kData) {
    // In case CopyAndFixBitStream() prepends SPS/PPS nalus to the packet, it
    // uses new uint8_t[] to allocate memory. Caller of CopyAndFixBitStream()
    // needs to take care of freeing the memory.
    delete[] idr_packet.dataPtr;
  }
}

TEST_F(TestH264SpsPpsTracker, SpsPpsOutOfBandWrongNaluHeader) {
  constexpr uint8_t kData[] = {1, 2, 3};

  // Generated by "ffmpeg -r 30 -f avfoundation -i "default" out.h264" on macos.
  // Nalu headers manupilated afterwards.
  const std::vector<uint8_t> sps(
      {0xff, 0x7a, 0x00, 0x0d, 0xbc, 0xd9, 0x41, 0x41, 0xfa, 0x10, 0x00, 0x00,
       0x03, 0x00, 0x10, 0x00, 0x00, 0x03, 0x03, 0xc0, 0xf1, 0x42, 0x99, 0x60});
  const std::vector<uint8_t> pps({0xff, 0xeb, 0xe3, 0xcb, 0x22, 0xc0});
  tracker_.InsertSpsPpsNalus(sps, pps);

  // Insert first packet of the IDR.
  H264VcmPacket idr_packet;
  idr_packet.video_header.is_first_packet_in_frame = true;
  AddIdr(&idr_packet, 0);
  idr_packet.dataPtr = kData;
  idr_packet.sizeBytes = sizeof(kData);
  EXPECT_EQ(H264SpsPpsTracker::kRequestKeyframe,
            tracker_.CopyAndFixBitstream(&idr_packet));
}

TEST_F(TestH264SpsPpsTracker, SpsPpsOutOfBandIncompleteNalu) {
  constexpr uint8_t kData[] = {1, 2, 3};

  // Generated by "ffmpeg -r 30 -f avfoundation -i "default" out.h264" on macos.
  // Nalus damaged afterwards.
  const std::vector<uint8_t> sps({0x67, 0x7a, 0x00, 0x0d, 0xbc, 0xd9});
  const std::vector<uint8_t> pps({0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0});
  tracker_.InsertSpsPpsNalus(sps, pps);

  // Insert first packet of the IDR.
  H264VcmPacket idr_packet;
  idr_packet.video_header.is_first_packet_in_frame = true;
  AddIdr(&idr_packet, 0);
  idr_packet.dataPtr = kData;
  idr_packet.sizeBytes = sizeof(kData);
  EXPECT_EQ(H264SpsPpsTracker::kRequestKeyframe,
            tracker_.CopyAndFixBitstream(&idr_packet));
}

TEST_F(TestH264SpsPpsTracker, SaveRestoreWidthHeight) {
  std::vector<uint8_t> data;

  // Insert an SPS/PPS packet with width/height and make sure
  // that information is set on the first IDR packet.
  H264VcmPacket sps_pps_packet;
  AddSps(&sps_pps_packet, 0, &data);
  AddPps(&sps_pps_packet, 0, 1, &data);
  sps_pps_packet.dataPtr = data.data();
  sps_pps_packet.sizeBytes = data.size();
  sps_pps_packet.video_header.width = 320;
  sps_pps_packet.video_header.height = 240;
  EXPECT_EQ(H264SpsPpsTracker::kInsert,
            tracker_.CopyAndFixBitstream(&sps_pps_packet));
  delete[] sps_pps_packet.dataPtr;

  H264VcmPacket idr_packet;
  idr_packet.video_header.is_first_packet_in_frame = true;
  AddIdr(&idr_packet, 1);
  data.insert(data.end(), {1, 2, 3});
  idr_packet.dataPtr = data.data();
  idr_packet.sizeBytes = data.size();
  EXPECT_EQ(H264SpsPpsTracker::kInsert,
            tracker_.CopyAndFixBitstream(&idr_packet));

  EXPECT_EQ(320, idr_packet.width());
  EXPECT_EQ(240, idr_packet.height());
  delete[] idr_packet.dataPtr;
}

}  // namespace video_coding
}  // namespace webrtc
