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
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace video_coding {
namespace {

using ::testing::ElementsAreArray;
using ::testing::SizeIs;

const uint8_t start_code[] = {0, 0, 0, 1};

rtc::ArrayView<const uint8_t> Bitstream(
    const H264SpsPpsTracker::FixedBitstream& fixed) {
  return fixed.bitstream;
}

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

class H264VideoHeader : public RTPVideoHeader {
 public:
  H264VideoHeader() {
    codec = kVideoCodecH264;
    is_first_packet_in_frame = false;
    auto& h264_header = video_type_header.emplace<RTPVideoHeaderH264>();
    h264_header.packetization_type = kH264SingleNalu;
  }

  RTPVideoHeaderH264& h264() {
    return absl::get<RTPVideoHeaderH264>(video_type_header);
  }
};

}  // namespace

class TestH264SpsPpsTracker : public ::testing::Test {
 public:
  void AddSps(H264VideoHeader* header,
              uint8_t sps_id,
              std::vector<uint8_t>* data) {
    NaluInfo info;
    info.type = H264::NaluType::kSps;
    info.sps_id = sps_id;
    info.pps_id = -1;
    data->push_back(H264::NaluType::kSps);
    data->push_back(sps_id);  // The sps data, just a single byte.

    header->h264().nalus.push_back(info);
  }

  void AddPps(H264VideoHeader* header,
              uint8_t sps_id,
              uint8_t pps_id,
              std::vector<uint8_t>* data) {
    NaluInfo info;
    info.type = H264::NaluType::kPps;
    info.sps_id = sps_id;
    info.pps_id = pps_id;
    data->push_back(H264::NaluType::kPps);
    data->push_back(pps_id);  // The pps data, just a single byte.

    header->h264().nalus.push_back(info);
  }

  void AddIdr(H264VideoHeader* header, int pps_id) {
    NaluInfo info;
    info.type = H264::NaluType::kIdr;
    info.sps_id = -1;
    info.pps_id = pps_id;

    header->h264().nalus.push_back(info);
  }

 protected:
  H264SpsPpsTracker tracker_;
};

TEST_F(TestH264SpsPpsTracker, NoNalus) {
  uint8_t data[] = {1, 2, 3};
  H264VideoHeader header;
  header.h264().packetization_type = kH264FuA;

  H264SpsPpsTracker::FixedBitstream fixed =
      tracker_.CopyAndFixBitstream(data, &header);

  EXPECT_EQ(fixed.action, H264SpsPpsTracker::kInsert);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(data));
}

TEST_F(TestH264SpsPpsTracker, FuAFirstPacket) {
  uint8_t data[] = {1, 2, 3};
  H264VideoHeader header;
  header.h264().packetization_type = kH264FuA;
  header.h264().nalus.resize(1);
  header.is_first_packet_in_frame = true;

  H264SpsPpsTracker::FixedBitstream fixed =
      tracker_.CopyAndFixBitstream(data, &header);

  EXPECT_EQ(fixed.action, H264SpsPpsTracker::kInsert);
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {1, 2, 3});
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(TestH264SpsPpsTracker, StapAIncorrectSegmentLength) {
  uint8_t data[] = {0, 0, 2, 0};
  H264VideoHeader header;
  header.h264().packetization_type = kH264StapA;
  header.is_first_packet_in_frame = true;

  EXPECT_EQ(tracker_.CopyAndFixBitstream(data, &header).action,
            H264SpsPpsTracker::kDrop);
}

TEST_F(TestH264SpsPpsTracker, ConsecutiveStapA) {
  // When the GenericFrameDescriptor or DependencyDescriptor RTP header
  // extensions are used, we may receive a series of StapA packets where only
  // the first packet has is_first_packet_in_frame = true set.
  std::vector<uint8_t> data;
  H264VideoHeader first_header;
  first_header.h264().packetization_type = kH264StapA;
  first_header.is_first_packet_in_frame = true;

  // SPS in first packet.
  data.insert(data.end(), {0});     // First byte is ignored
  data.insert(data.end(), {0, 2});  // Length of segment
  AddSps(&first_header, 13, &data);
  H264SpsPpsTracker::FixedBitstream first_fixed =
      tracker_.CopyAndFixBitstream(data, &first_header);
  EXPECT_THAT(first_fixed.action, H264SpsPpsTracker::kInsert);

  H264VideoHeader second_header;
  second_header.h264().packetization_type = kH264StapA;
  second_header.is_first_packet_in_frame = false;

  // PPS and IDR in second packet.
  data.insert(data.end(), {0, 2});  // Length of segment
  AddPps(&second_header, 13, 27, &data);
  data.insert(data.end(), {0, 5});  // Length of segment
  AddIdr(&second_header, 27);
  data.insert(data.end(), {1, 2, 3, 2, 1});

  H264SpsPpsTracker::FixedBitstream fixed =
      tracker_.CopyAndFixBitstream(data, &second_header);

  EXPECT_THAT(fixed.action, H264SpsPpsTracker::kInsert);
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {H264::NaluType::kSps, 13});
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {H264::NaluType::kPps, 27});
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {1, 2, 3, 2, 1});
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(TestH264SpsPpsTracker, SingleNaluInsertStartCode) {
  uint8_t data[] = {1, 2, 3};
  H264VideoHeader header;
  header.h264().nalus.resize(1);

  H264SpsPpsTracker::FixedBitstream fixed =
      tracker_.CopyAndFixBitstream(data, &header);

  EXPECT_EQ(fixed.action, H264SpsPpsTracker::kInsert);
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {1, 2, 3});
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(TestH264SpsPpsTracker, NoStartCodeInsertedForSubsequentFuAPacket) {
  std::vector<uint8_t> data = {1, 2, 3};
  H264VideoHeader header;
  header.h264().packetization_type = kH264FuA;
  // Since no NALU begin in this packet the nalus are empty.
  header.h264().nalus.clear();

  H264SpsPpsTracker::FixedBitstream fixed =
      tracker_.CopyAndFixBitstream(data, &header);

  EXPECT_EQ(fixed.action, H264SpsPpsTracker::kInsert);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(data));
}

TEST_F(TestH264SpsPpsTracker, IdrFirstPacketNoSpsPpsInserted) {
  std::vector<uint8_t> data = {1, 2, 3};
  H264VideoHeader header;
  header.is_first_packet_in_frame = true;
  AddIdr(&header, 0);

  EXPECT_EQ(tracker_.CopyAndFixBitstream(data, &header).action,
            H264SpsPpsTracker::kRequestKeyframe);
}

TEST_F(TestH264SpsPpsTracker, IdrFirstPacketNoPpsInserted) {
  std::vector<uint8_t> data = {1, 2, 3};
  H264VideoHeader header;
  header.is_first_packet_in_frame = true;
  AddSps(&header, 0, &data);
  AddIdr(&header, 0);

  EXPECT_EQ(tracker_.CopyAndFixBitstream(data, &header).action,
            H264SpsPpsTracker::kRequestKeyframe);
}

TEST_F(TestH264SpsPpsTracker, IdrFirstPacketNoSpsInserted) {
  std::vector<uint8_t> data = {1, 2, 3};
  H264VideoHeader header;
  header.is_first_packet_in_frame = true;
  AddPps(&header, 0, 0, &data);
  AddIdr(&header, 0);

  EXPECT_EQ(tracker_.CopyAndFixBitstream(data, &header).action,
            H264SpsPpsTracker::kRequestKeyframe);
}

TEST_F(TestH264SpsPpsTracker, SpsPpsPacketThenIdrFirstPacket) {
  std::vector<uint8_t> data;
  H264VideoHeader sps_pps_header;
  // Insert SPS/PPS
  AddSps(&sps_pps_header, 0, &data);
  AddPps(&sps_pps_header, 0, 1, &data);

  EXPECT_EQ(tracker_.CopyAndFixBitstream(data, &sps_pps_header).action,
            H264SpsPpsTracker::kInsert);

  // Insert first packet of the IDR
  H264VideoHeader idr_header;
  idr_header.is_first_packet_in_frame = true;
  AddIdr(&idr_header, 1);
  data = {1, 2, 3};

  H264SpsPpsTracker::FixedBitstream fixed =
      tracker_.CopyAndFixBitstream(data, &idr_header);
  EXPECT_EQ(fixed.action, H264SpsPpsTracker::kInsert);

  std::vector<uint8_t> expected;
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {1, 2, 3});
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(TestH264SpsPpsTracker, SpsPpsIdrInStapA) {
  std::vector<uint8_t> data;
  H264VideoHeader header;
  header.h264().packetization_type = kH264StapA;
  header.is_first_packet_in_frame = true;  // Always true for StapA

  data.insert(data.end(), {0});     // First byte is ignored
  data.insert(data.end(), {0, 2});  // Length of segment
  AddSps(&header, 13, &data);
  data.insert(data.end(), {0, 2});  // Length of segment
  AddPps(&header, 13, 27, &data);
  data.insert(data.end(), {0, 5});  // Length of segment
  AddIdr(&header, 27);
  data.insert(data.end(), {1, 2, 3, 2, 1});

  H264SpsPpsTracker::FixedBitstream fixed =
      tracker_.CopyAndFixBitstream(data, &header);

  EXPECT_THAT(fixed.action, H264SpsPpsTracker::kInsert);

  std::vector<uint8_t> expected;
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {H264::NaluType::kSps, 13});
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {H264::NaluType::kPps, 27});
  expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
  expected.insert(expected.end(), {1, 2, 3, 2, 1});
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
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
  H264VideoHeader idr_header;
  idr_header.is_first_packet_in_frame = true;
  AddIdr(&idr_header, 0);
  EXPECT_THAT(idr_header.h264().nalus, SizeIs(1));

  H264SpsPpsTracker::FixedBitstream fixed =
      tracker_.CopyAndFixBitstream(kData, &idr_header);

  EXPECT_THAT(idr_header.h264().nalus, SizeIs(3));
  EXPECT_EQ(idr_header.width, 320u);
  EXPECT_EQ(idr_header.height, 240u);
  ExpectSpsPpsIdr(idr_header.h264(), 0, 0);
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
  H264VideoHeader idr_header;
  idr_header.is_first_packet_in_frame = true;
  AddIdr(&idr_header, 0);

  EXPECT_EQ(tracker_.CopyAndFixBitstream(kData, &idr_header).action,
            H264SpsPpsTracker::kRequestKeyframe);
}

TEST_F(TestH264SpsPpsTracker, SpsPpsOutOfBandIncompleteNalu) {
  constexpr uint8_t kData[] = {1, 2, 3};

  // Generated by "ffmpeg -r 30 -f avfoundation -i "default" out.h264" on macos.
  // Nalus damaged afterwards.
  const std::vector<uint8_t> sps({0x67, 0x7a, 0x00, 0x0d, 0xbc, 0xd9});
  const std::vector<uint8_t> pps({0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0});
  tracker_.InsertSpsPpsNalus(sps, pps);

  // Insert first packet of the IDR.
  H264VideoHeader idr_header;
  idr_header.is_first_packet_in_frame = true;
  AddIdr(&idr_header, 0);

  EXPECT_EQ(tracker_.CopyAndFixBitstream(kData, &idr_header).action,
            H264SpsPpsTracker::kRequestKeyframe);
}

TEST_F(TestH264SpsPpsTracker, SaveRestoreWidthHeight) {
  std::vector<uint8_t> data;

  // Insert an SPS/PPS packet with width/height and make sure
  // that information is set on the first IDR packet.
  H264VideoHeader sps_pps_header;
  AddSps(&sps_pps_header, 0, &data);
  AddPps(&sps_pps_header, 0, 1, &data);
  sps_pps_header.width = 320;
  sps_pps_header.height = 240;

  EXPECT_EQ(tracker_.CopyAndFixBitstream(data, &sps_pps_header).action,
            H264SpsPpsTracker::kInsert);

  H264VideoHeader idr_header;
  idr_header.is_first_packet_in_frame = true;
  AddIdr(&idr_header, 1);
  data.insert(data.end(), {1, 2, 3});

  EXPECT_EQ(tracker_.CopyAndFixBitstream(data, &idr_header).action,
            H264SpsPpsTracker::kInsert);

  EXPECT_EQ(idr_header.width, 320);
  EXPECT_EQ(idr_header.height, 240);
}

}  // namespace video_coding
}  // namespace webrtc
