/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_video_layers_allocation_extension.h"

#include "api/video/video_layers_allocation.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/buffer.h"

#include "test/gmock.h"

namespace webrtc {
namespace {

TEST(RtpVideoLayersAllocationExtension, WriteEmptyLayersAllocationReturnsTrue) {
  VideoLayersAllocation written_allocation;
  rtc::Buffer buffer(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Write(buffer, written_allocation));
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParseLayersAllocationWithZeroSpatialLayers) {
  // We require the resolution_and_frame_rate_is_valid to be set to true in
  // order to send an "empty" allocation.
  VideoLayersAllocation written_allocation;
  written_allocation.resolution_and_frame_rate_is_valid = true;
  written_allocation.rtp_stream_index = 0;

  rtc::Buffer buffer(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Write(buffer, written_allocation));

  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  EXPECT_EQ(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParse2SpatialWith2TemporalLayers) {
  VideoLayersAllocation written_allocation;
  written_allocation.rtp_stream_index = 1;
  written_allocation.active_spatial_layers = {
      {
          /*rtp_stream_index*/ 0,
          /*spatial_id*/ 0,
          /*target_bitrate_per_temporal_layer*/
          {DataRate::KilobitsPerSec(25), DataRate::KilobitsPerSec(50)},
          /*width*/ 0,
          /*height*/ 0,
          /*frame_rate_fps*/ 0,
      },
      {
          /*rtp_stream_index*/ 1,
          /*spatial_id*/ 0,
          /*target_bitrate_per_temporal_layer*/
          {DataRate::KilobitsPerSec(100), DataRate::KilobitsPerSec(200)},
          /*width*/ 0,
          /*height*/ 0,
          /*frame_rate_fps*/ 0,
      },
  };
  rtc::Buffer buffer(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Write(buffer, written_allocation));
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  EXPECT_EQ(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParseAllocationWithDifferentNumerOfSpatialLayers) {
  VideoLayersAllocation written_allocation;
  written_allocation.rtp_stream_index = 1;
  written_allocation.active_spatial_layers = {
      {/*rtp_stream_index*/ 0,
       /*spatial_id*/ 0,
       /*target_bitrate_per_temporal_layer*/ {DataRate::KilobitsPerSec(50)},
       /*width*/ 0,
       /*height*/ 0,
       /*frame_rate_fps*/ 0},
      {/*rtp_stream_index*/ 1,
       /*spatial_id*/ 0,
       /*target_bitrate_per_temporal_layer*/ {DataRate::KilobitsPerSec(100)},
       /*width*/ 0,
       /*height*/ 0,
       /*frame_rate_fps*/ 0},
      {/*rtp_stream_index*/ 1,
       /*spatial_id*/ 1,
       /*target_bitrate_per_temporal_layer*/ {DataRate::KilobitsPerSec(200)},
       /*width*/ 0,
       /*height*/ 0,
       /*frame_rate_fps*/ 0},
  };
  rtc::Buffer buffer(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Write(buffer, written_allocation));
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  EXPECT_EQ(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParseAllocationWithSkippedLowerSpatialLayer) {
  VideoLayersAllocation written_allocation;
  written_allocation.rtp_stream_index = 1;
  written_allocation.active_spatial_layers = {
      {/*rtp_stream_index*/ 0,
       /*spatial_id*/ 0,
       /*target_bitrate_per_temporal_layer*/ {DataRate::KilobitsPerSec(50)},
       /*width*/ 0,
       /*height*/ 0,
       /*frame_rate_fps*/ 0},
      {/*rtp_stream_index*/ 1,
       /*spatial_id*/ 1,
       /*target_bitrate_per_temporal_layer*/ {DataRate::KilobitsPerSec(200)},
       /*width*/ 0,
       /*height*/ 0,
       /*frame_rate_fps*/ 0},
  };
  rtc::Buffer buffer(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Write(buffer, written_allocation));
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  EXPECT_EQ(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParseAllocationWithSkippedRtpStreamIds) {
  VideoLayersAllocation written_allocation;
  written_allocation.rtp_stream_index = 2;
  written_allocation.active_spatial_layers = {
      {/*rtp_stream_index*/ 0,
       /*spatial_id*/ 0,
       /*target_bitrate_per_temporal_layer*/ {DataRate::KilobitsPerSec(50)},
       /*width*/ 0,
       /*height*/ 0,
       /*frame_rate_fps*/ 0},
      {/*rtp_stream_index*/ 2,
       /*spatial_id*/ 0,
       /*target_bitrate_per_temporal_layer*/ {DataRate::KilobitsPerSec(200)},
       /*width*/ 0,
       /*height*/ 0,
       /*frame_rate_fps*/ 0},
  };
  rtc::Buffer buffer(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Write(buffer, written_allocation));
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  EXPECT_EQ(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParseAllocationWithDifferentNumerOfTemporalLayers) {
  VideoLayersAllocation written_allocation;
  written_allocation.rtp_stream_index = 1;
  written_allocation.active_spatial_layers = {
      {
          /*rtp_stream_index*/ 0,
          /*spatial_id*/ 0,
          /*target_bitrate_per_temporal_layer*/
          {DataRate::KilobitsPerSec(25), DataRate::KilobitsPerSec(50)},
          /*width*/ 0,
          /*height*/ 0,
          /*frame_rate_fps*/ 0,
      },
      {
          /*rtp_stream_index*/ 1,
          /*spatial_id*/ 0,
          /*target_bitrate_per_temporal_layer*/ {DataRate::KilobitsPerSec(100)},
          /*width*/ 0,
          /*height*/ 0,
          /*frame_rate_fps*/ 0,
      },
  };
  rtc::Buffer buffer(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Write(buffer, written_allocation));
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  EXPECT_EQ(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParseAllocationWithResolution) {
  VideoLayersAllocation written_allocation;
  written_allocation.rtp_stream_index = 1;
  written_allocation.resolution_and_frame_rate_is_valid = true;
  written_allocation.active_spatial_layers = {
      {
          /*rtp_stream_index*/ 0,
          /*spatial_id*/ 0,
          /*target_bitrate_per_temporal_layer*/
          {DataRate::KilobitsPerSec(25), DataRate::KilobitsPerSec(50)},
          /*width*/ 320,
          /*height*/ 240,
          /*frame_rate_fps*/ 8,
      },
      {
          /*rtp_stream_index*/ 1,
          /*spatial_id*/ 1,
          /*target_bitrate_per_temporal_layer*/
          {DataRate::KilobitsPerSec(100), DataRate::KilobitsPerSec(200)},
          /*width*/ 640,
          /*height*/ 320,
          /*frame_rate_fps*/ 30,
      },
  };

  rtc::Buffer buffer(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Write(buffer, written_allocation));
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  EXPECT_EQ(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension,
     WriteEmptyAllocationCanHaveAnyRtpStreamIndex) {
  VideoLayersAllocation written_allocation;
  written_allocation.rtp_stream_index = 1;
  rtc::Buffer buffer(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Write(buffer, written_allocation));
}

}  // namespace
}  // namespace webrtc
