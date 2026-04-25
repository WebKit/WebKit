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

#include <cstddef>
#include <cstdint>

#include "api/array_view.h"
#include "api/units/data_rate.h"
#include "api/video/video_layers_allocation.h"
#include "rtc_base/buffer.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(RtpVideoLayersAllocationExtension, WriteEmptyLayersAllocationReturnsTrue) {
  VideoLayersAllocation written_allocation;
  Buffer buffer = Buffer::CreateWithCapacity(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  buffer.AppendData(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation),
      [&](ArrayView<uint8_t> buffer_view) {
        EXPECT_TRUE(RtpVideoLayersAllocationExtension::Write(
            buffer_view, written_allocation));
        return buffer_view.size();
      });
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParseLayersAllocationWithZeroSpatialLayers) {
  // We require the resolution_and_frame_rate_is_valid to be set to true in
  // order to send an "empty" allocation.
  VideoLayersAllocation written_allocation;
  written_allocation.resolution_and_frame_rate_is_valid = true;
  written_allocation.rtp_stream_index = 0;
  Buffer buffer = Buffer::CreateWithCapacity(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  buffer.AppendData(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation),
      [&](ArrayView<uint8_t> buffer_view) {
        EXPECT_TRUE(RtpVideoLayersAllocationExtension::Write(
            buffer_view, written_allocation));
        return buffer_view.size();
      });

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
          /*rtp_stream_index*/ .rtp_stream_index = 0,
          /*spatial_id*/ .spatial_id = 0,
          /*target_bitrate_per_temporal_layer*/
          .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(25),
                                                DataRate::KilobitsPerSec(50)},
          /*width*/ .width = 0,
          /*height*/ .height = 0,
          /*frame_rate_fps*/ .frame_rate_fps = 0,
      },
      {
          /*rtp_stream_index*/ .rtp_stream_index = 1,
          /*spatial_id*/ .spatial_id = 0,
          /*target_bitrate_per_temporal_layer*/
          .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(100),
                                                DataRate::KilobitsPerSec(200)},
          /*width*/ .width = 0,
          /*height*/ .height = 0,
          /*frame_rate_fps*/ .frame_rate_fps = 0,
      },
  };
  Buffer buffer = Buffer::CreateWithCapacity(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  buffer.AppendData(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation),
      [&](ArrayView<uint8_t> buffer_view) {
        EXPECT_TRUE(RtpVideoLayersAllocationExtension::Write(
            buffer_view, written_allocation));
        return buffer_view.size();
      });
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
      {/*rtp_stream_index*/ .rtp_stream_index = 0,
       /*spatial_id*/ .spatial_id = 0,
       /*target_bitrate_per_temporal_layer*/
       .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(50)},
       /*width*/ .width = 0,
       /*height*/ .height = 0,
       /*frame_rate_fps*/ .frame_rate_fps = 0},
      {/*rtp_stream_index*/ .rtp_stream_index = 1,
       /*spatial_id*/ .spatial_id = 0,
       /*target_bitrate_per_temporal_layer*/
       .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(100)},
       /*width*/ .width = 0,
       /*height*/ .height = 0,
       /*frame_rate_fps*/ .frame_rate_fps = 0},
      {/*rtp_stream_index*/ .rtp_stream_index = 1,
       /*spatial_id*/ .spatial_id = 1,
       /*target_bitrate_per_temporal_layer*/
       .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(200)},
       /*width*/ .width = 0,
       /*height*/ .height = 0,
       /*frame_rate_fps*/ .frame_rate_fps = 0},
  };
  Buffer buffer = Buffer::CreateWithCapacity(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  buffer.AppendData(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation),
      [&](ArrayView<uint8_t> buffer_view) {
        EXPECT_TRUE(RtpVideoLayersAllocationExtension::Write(
            buffer_view, written_allocation));
        return buffer_view.size();
      });
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
      {/*rtp_stream_index*/ .rtp_stream_index = 0,
       /*spatial_id*/ .spatial_id = 0,
       /*target_bitrate_per_temporal_layer*/
       .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(50)},
       /*width*/ .width = 0,
       /*height*/ .height = 0,
       /*frame_rate_fps*/ .frame_rate_fps = 0},
      {/*rtp_stream_index*/ .rtp_stream_index = 1,
       /*spatial_id*/ .spatial_id = 1,
       /*target_bitrate_per_temporal_layer*/
       .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(200)},
       /*width*/ .width = 0,
       /*height*/ .height = 0,
       /*frame_rate_fps*/ .frame_rate_fps = 0},
  };
  Buffer buffer = Buffer::CreateWithCapacity(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  buffer.AppendData(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation),
      [&](ArrayView<uint8_t> buffer_view) {
        EXPECT_TRUE(RtpVideoLayersAllocationExtension::Write(
            buffer_view, written_allocation));
        return buffer_view.size();
      });
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
      {/*rtp_stream_index*/ .rtp_stream_index = 0,
       /*spatial_id*/ .spatial_id = 0,
       /*target_bitrate_per_temporal_layer*/
       .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(50)},
       /*width*/ .width = 0,
       /*height*/ .height = 0,
       /*frame_rate_fps*/ .frame_rate_fps = 0},
      {/*rtp_stream_index*/ .rtp_stream_index = 2,
       /*spatial_id*/ .spatial_id = 0,
       /*target_bitrate_per_temporal_layer*/
       .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(200)},
       /*width*/ .width = 0,
       /*height*/ .height = 0,
       /*frame_rate_fps*/ .frame_rate_fps = 0},
  };
  Buffer buffer = Buffer::CreateWithCapacity(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  buffer.AppendData(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation),
      [&](ArrayView<uint8_t> buffer_view) {
        EXPECT_TRUE(RtpVideoLayersAllocationExtension::Write(
            buffer_view, written_allocation));
        return buffer_view.size();
      });
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
          /*rtp_stream_index*/ .rtp_stream_index = 0,
          /*spatial_id*/ .spatial_id = 0,
          /*target_bitrate_per_temporal_layer*/
          .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(25),
                                                DataRate::KilobitsPerSec(50)},
          /*width*/ .width = 0,
          /*height*/ .height = 0,
          /*frame_rate_fps*/ .frame_rate_fps = 0,
      },
      {
          /*rtp_stream_index*/ .rtp_stream_index = 1,
          /*spatial_id*/ .spatial_id = 0,
          /*target_bitrate_per_temporal_layer*/
          .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(100)},
          /*width*/ .width = 0,
          /*height*/ .height = 0,
          /*frame_rate_fps*/ .frame_rate_fps = 0,
      },
  };
  Buffer buffer = Buffer::CreateWithCapacity(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  buffer.AppendData(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation),
      [&](ArrayView<uint8_t> buffer_view) {
        EXPECT_TRUE(RtpVideoLayersAllocationExtension::Write(
            buffer_view, written_allocation));
        return buffer_view.size();
      });
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
          /*rtp_stream_index*/ .rtp_stream_index = 0,
          /*spatial_id*/ .spatial_id = 0,
          /*target_bitrate_per_temporal_layer*/
          .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(25),
                                                DataRate::KilobitsPerSec(50)},
          /*width*/ .width = 320,
          /*height*/ .height = 240,
          /*frame_rate_fps*/ .frame_rate_fps = 8,
      },
      {
          /*rtp_stream_index*/ .rtp_stream_index = 1,
          /*spatial_id*/ .spatial_id = 1,
          /*target_bitrate_per_temporal_layer*/
          .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(100),
                                                DataRate::KilobitsPerSec(200)},
          /*width*/ .width = 640,
          /*height*/ .height = 320,
          /*frame_rate_fps*/ .frame_rate_fps = 30,
      },
  };

  Buffer buffer = Buffer::CreateWithCapacity(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  buffer.AppendData(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation),
      [&](ArrayView<uint8_t> buffer_view) {
        EXPECT_TRUE(RtpVideoLayersAllocationExtension::Write(
            buffer_view, written_allocation));
        return buffer_view.size();
      });
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  EXPECT_EQ(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension,
     WriteEmptyAllocationCanHaveAnyRtpStreamIndex) {
  VideoLayersAllocation written_allocation;
  written_allocation.rtp_stream_index = 1;
  Buffer buffer = Buffer::CreateWithCapacity(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  buffer.AppendData(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation),
      [&](ArrayView<uint8_t> buffer_view) {
        EXPECT_TRUE(RtpVideoLayersAllocationExtension::Write(
            buffer_view, written_allocation));
        return buffer_view.size();
      });
}

TEST(RtpVideoLayersAllocationExtension, DiscardsOverLargeDataRate) {
  constexpr uint8_t buffer[] = {0x4b, 0xf6, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xcb, 0x78, 0xeb, 0x8d, 0xb5, 0x31};
  VideoLayersAllocation allocation;
  EXPECT_FALSE(RtpVideoLayersAllocationExtension::Parse(buffer, &allocation));
}

TEST(RtpVideoLayersAllocationExtension, DiscardsInvalidHeight) {
  VideoLayersAllocation written_allocation;
  written_allocation.rtp_stream_index = 0;
  written_allocation.resolution_and_frame_rate_is_valid = true;
  written_allocation.active_spatial_layers = {
      {
          /*rtp_stream_index*/ .rtp_stream_index = 0,
          /*spatial_id*/ .spatial_id = 0,
          /*target_bitrate_per_temporal_layer*/
          .target_bitrate_per_temporal_layer = {DataRate::KilobitsPerSec(25),
                                                DataRate::KilobitsPerSec(50)},
          /*width*/ .width = 320,
          /*height*/ .height = 240,
          /*frame_rate_fps*/ .frame_rate_fps = 8,
      },
  };
  Buffer buffer = Buffer::CreateWithCapacity(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation));
  size_t written = buffer.AppendData(
      RtpVideoLayersAllocationExtension::ValueSize(written_allocation),
      [&](ArrayView<uint8_t> buffer_view) {
        bool result = RtpVideoLayersAllocationExtension::Write(
            buffer_view, written_allocation);
        EXPECT_TRUE(result);
        return result ? buffer_view.size() : 0;
      });
  ASSERT_EQ(written,
            RtpVideoLayersAllocationExtension::ValueSize(written_allocation));

  // Modify the height to be invalid.
  buffer[buffer.size() - 3] = 0xff;
  buffer[buffer.size() - 2] = 0xff;
  VideoLayersAllocation allocation;
  EXPECT_FALSE(RtpVideoLayersAllocationExtension::Parse(buffer, &allocation));
}

}  // namespace
}  // namespace webrtc
