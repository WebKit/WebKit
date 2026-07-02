// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/block_parser.h"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "src/parser_utils.h"
#include "test_utils/element_parser_test.h"
#include "webm/id.h"
#include "webm/status.h"

using testing::_;
using testing::Between;
using testing::DoAll;
using testing::Exactly;
using testing::InSequence;
using testing::Invoke;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;

using webm::Action;
using webm::Block;
using webm::BlockParser;
using webm::ElementParserTest;
using webm::FrameMetadata;
using webm::Id;
using webm::kUnknownElementSize;
using webm::Lacing;
using webm::ReadByte;
using webm::Reader;
using webm::SimpleBlock;
using webm::SimpleBlockParser;
using webm::Status;

namespace {

// Represents a single block and its expected parsing results for use in tests.
struct TestData {
  // Block data.
  std::vector<std::uint8_t> data;

  // Expected results.
  std::uint64_t expected_track_number;
  std::int16_t expected_timecode;
  Lacing expected_lacing;
  bool expected_is_visible;
  bool expected_is_key_frame;
  bool expected_is_discardable;
  int expected_num_frames;

  std::uint64_t expected_frame_start_position;
  std::vector<std::uint64_t> expected_frame_sizes;
};

// Test data for an EBML-laced block containing only one frame.
const TestData ebml_lacing_one_frame = {
    // Data.
    {
        0x81,  // Track number = 1.
        0x00,
        0x00,  // Timecode = 0.
        0x86,  // Flags = key_frame | ebml_lacing.
        0x00,  // Lace count - 1 = 0 (1 frame).

        // Lace data (1 frame).
        // Frame 0.
        0x00,
    },

    // Expected results.
    1,  // expected_track_number
    0,  // expected_timecode
    Lacing::kEbml,  // expected_lacing
    true,  // expected_is_visible
    true,  // expected_is_key_frame
    false,  // expected_is_discardable
    1,  // expected_num_frames

    5,  // expected_frame_start_position
    // expected_frame_sizes
    {1},
};

// Test data for a Xiph-laced block containing only one frame.
const TestData xiph_lacing_one_frame = {
    // Data.
    {
        0x81,  // Track number = 1.
        0x00,
        0x00,  // Timecode = 0.
        0x82,  // Flags = key_frame | xiph_lacing.
        0x00,  // Lace count - 1 = 0 (1 frame).

        // Lace data (1 frame).
        // Frame 0.
        0x00,
    },

    // Expected results.
    1,  // expected_track_number
    0,  // expected_timecode
    Lacing::kXiph,  // expected_lacing
    true,  // expected_is_visible
    true,  // expected_is_key_frame
    false,  // expected_is_discardable
    1,  // expected_num_frames

    5,  // expected_frame_start_position
    // expected_frame_sizes
    {1},
};

// Test data for a fixed-laced block containing only one frame.
const TestData fixed_lacing_one_frame = {
    // Data.
    {
        0x81,  // Track number = 1.
        0x00,
        0x00,  // Timecode = 0.
        0x84,  // Flags = key_frame | fixed_lacing.
        0x00,  // Lace count - 1 = 0 (1 frame).

        // Lace data (1 frame).
        0x00,
    },

    // Expected results.
    1,  // expected_track_number
    0,  // expected_timecode
    Lacing::kFixed,  // expected_lacing
    true,  // expected_is_visible
    true,  // expected_is_key_frame
    false,  // expected_is_discardable
    1,  // expected_num_frames

    5,  // expected_frame_start_position
    // expected_frame_sizes
    {1},
};

// Test data for an EBML-laced block.
// clang-format off
const TestData ebml_lacing = {
    // Data.
    {
        0x81,  // Track number = 1.
        0x00, 0x00,  // Timecode = 0.
        0x86,  // Flags = key_frame | ebml_lacing.
        0x05,  // Lace count - 1 = 5 (6 frames).

        // Lace data (6 frames).
        0xFF,  // Lace 0 size = 127.
        0x5F, 0x81,  // Lace 1 size = 1.
        0xC0,  // Lace 2 size = 2.
        0xFF,  // Lace 3 size = 66.
        0x81,  // Lace 4 size = 4.
        // Lace 5 size inferred to be 5.

        // Lace data (6 frames).
        // Frame 0.
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

        // Frame 1.
        0x01,

        // Frame 2.
        0x02, 0x02,

        // Frame 3.
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03,

        // Frame 4.
        0x04, 0x04, 0x04, 0x04,

        // Frame 5.
        0x05, 0x05, 0x05, 0x05, 0x05,
    },

    // Expected results.
    1,  // expected_track_number
    0,  // expected_timecode
    Lacing::kEbml,  // expected_lacing
    true,  // expected_is_visible
    true,  // expected_is_key_frame
    false,  // expected_is_discardable
    6,  // expected_num_frames

    11,  // expected_frame_start_position
    // expected_frame_sizes
    {127, 1, 2, 66, 4, 5},
};

// Test data for a Xiph-laced block.
const TestData xiph_lacing = {
    // Data.
    {
        0x81,  // Track number = 1.
        0x00, 0x00,  // Timecode = 0.
        0x82,  // Flags = key_frame | xiph_lacing.
        0x03,  // Lace count - 1 = 3 (4 frames).

        // Lace sizes.
        0xFF, 0xFF, 0x00,  // Lace 0 size = 510.
        0xFF, 0x01,  // Lace 1 size = 256.
        0x02,  // Lace 2 size = 2.
        // Lace 3 size inferred to be 3.

        // Lace data (4 frames).
        // Frame 0.
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

        // Frame 1.
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01,

        // Frame 2.
        0x02, 0x02,

        // Frame 3.
        0x03, 0x03, 0x03,
    },

    // Expected results.
    1,  // expected_track_number
    0,  // expected_timecode
    Lacing::kXiph,  // expected_lacing
    true,  // expected_is_visible
    true,  // expected_is_key_frame
    false,  // expected_is_discardable
    4,  // expected_num_frames

    11,  // expected_frame_start_position
    // expected_frame_sizes
    {510, 256, 2, 3},
};
// clang-format on

// Test data for a fixed-laced block.
const TestData fixed_lacing = {
    // Data.
    {
        0x81,  // Track number = 1.
        0x00, 0x00,  // Timecode = 0.
        0x84,  // Flags = key_frame | fixed_lacing.
        0x03,  // Lace count - 1 = 3 (4 frames).

        // Lace data (4 frames).
        0x00, 0x00,  // Frame 0.
        0x01, 0x01,  // Frame 1.
        0x02, 0x02,  // Frame 2.
        0x03, 0x03,  // Frame 3.
    },

    // Expected results.
    1,  // expected_track_number
    0,  // expected_timecode
    Lacing::kFixed,  // expected_lacing
    true,  // expected_is_visible
    true,  // expected_is_key_frame
    false,  // expected_is_discardable
    4,  // expected_num_frames

    5,  // expected_frame_start_position
    // expected_frame_sizes
    {2, 2, 2, 2},
};

// Test data for a block that has no lacing.
const TestData no_lacing = {
    // Data.
    {
        0x40, 0x01,  // Track number = 1.
        0x00, 0x00,  // Timecode = 0.
        0x80,  // Flags = key_frame.

        // Lace data (1 frame).
        0x00, 0x00, 0x00,  // Frame 0.
    },

    // Expected results.
    1,  // expected_track_number
    0,  // expected_timecode
    Lacing::kNone,  // expected_lacing
    true,  // expected_is_visible
    true,  // expected_is_key_frame
    false,  // expected_is_discardable
    1,  // expected_num_frames

    5,  // expected_frame_start_position
    // expected_frame_sizes
    {3},
};

// Test data for a block that has no flags set in the header.
const TestData no_flags = {
    // Data.
    {
        0x81,  // Track number = 1.
        0x00,
        0x00,  // Timecode = 0.
        0x00,  // Flags = 0.

        // Lace data (1 frame).
        0x00,
    },

    // Expected results.
    1,  // expected_track_number
    0,  // expected_timecode
    Lacing::kNone,  // expected_lacing
    true,  // expected_is_visible
    false,  // expected_is_key_frame
    false,  // expected_is_discardable
    1,  // expected_num_frames

    4,  // expected_frame_start_position
    // expected_frame_sizes
    {1},
};

// Test data for a block that has all Block (ID = Id::kBlock (0xA1)) flags set
// in the header (and no other flags).
const TestData block_flags = {
    // Data.
    {
        0x82,  // Track number = 2.
        0xFE,
        0xDC,  // Timecode = -292.
        0x08,  // Flags = invisible.

        // Lace data (1 frame).
        0x00,
    },

    // Expected results.
    2,  // expected_track_number
    -292,  // expected_timecode
    Lacing::kNone,  // expected_lacing
    false,  // expected_is_visible
    false,  // expected_is_key_frame
    false,  // expected_is_discardable
    1,  // expected_num_frames

    4,  // expected_frame_start_position
    // expected_frame_sizes
    {1},
};

// Test data for a block that has all SimpleBlock (ID = Id::kSimpleBlock (0xA3))
// flags set in the header.
const TestData simple_block_flags = {
    // Data.
    {
        0x41,
        0x23,  // Track number = 291.
        0x12,
        0x34,  // Timecode = 4660.
        0x89,  // Flags = key_frame | invisible | discardable.

        // Lace data (1 frame).
        0x00,
    },

    // Expected results.
    291,  // expected_track_number
    4660,  // expected_timecode
    Lacing::kNone,  // expected_lacing
    false,  // expected_is_visible
    true,  // expected_is_key_frame
    true,  // expected_is_discardable
    1,  // expected_num_frames

    5,  // expected_frame_start_position
    // expected_frame_sizes
    {1},
};

// Checks that the Block matches the expected results in the TestData.
void ValidateBlock(const TestData& test_data, const Block& actual) {
  EXPECT_EQ(test_data.expected_track_number, actual.track_number);
  EXPECT_EQ(test_data.expected_timecode, actual.timecode);
  EXPECT_EQ(test_data.expected_is_visible, actual.is_visible);
  EXPECT_EQ(test_data.expected_lacing, actual.lacing);
  EXPECT_EQ(test_data.expected_num_frames, actual.num_frames);
}

// Checks that the SimpleBlock matches the expected results in the TestData.
void ValidateBlock(const TestData& test_data, const SimpleBlock& actual) {
  ValidateBlock(test_data, static_cast<const Block&>(actual));
  EXPECT_EQ(test_data.expected_is_key_frame, actual.is_key_frame);
  EXPECT_EQ(test_data.expected_is_discardable, actual.is_discardable);
}

// Constructs a SimpleBlock populated with the expected results for this test.
SimpleBlock ExpectedSimpleBlock(const TestData& test_data) {
  SimpleBlock expected;
  expected.track_number = test_data.expected_track_number;
  expected.timecode = test_data.expected_timecode;
  expected.lacing = test_data.expected_lacing;
  expected.is_visible = test_data.expected_is_visible;
  expected.is_key_frame = test_data.expected_is_key_frame;
  expected.is_discardable = test_data.expected_is_discardable;
  expected.num_frames = test_data.expected_num_frames;
  return expected;
}

// Simple functor that can be used for Callback::OnFrame() that will validate
// the frame metadata and frame byte values.
struct FrameHandler {
  // The expected value for the frame metadata.
  FrameMetadata expected_metadata;

  // The expected value for each byte in the frame.
  std::uint8_t expected_frame_byte_value;

  // Can be used for Callback::OnFrame() to consume data from the reader and
  // validate the results.
  Status operator()(const FrameMetadata& metadata, Reader* reader,
                    std::uint64_t* bytes_remaining) const {
    EXPECT_EQ(expected_metadata, metadata);

    std::uint8_t frame_byte_value;
    Status status;
    do {
      status = ReadByte(reader, &frame_byte_value);
      if (!status.completed_ok()) {
        break;
      }

      EXPECT_EQ(expected_frame_byte_value, frame_byte_value);

      --*bytes_remaining;
    } while (*bytes_remaining > 0);

    return status;
  }
};

template <typename T, Id id>
class BasicBlockParserTest : public ElementParserTest<T, id> {
 public:
  // Sets expectations for a normal (i.e. successful parse) test.
  void SetExpectations(const TestData& test_data, bool incremental,
                       bool set_action) {
    InSequence dummy;

    const SimpleBlock expected_simple_block = ExpectedSimpleBlock(test_data);
    const Block expected_block = ExpectedSimpleBlock(test_data);

    FrameMetadata metadata = FirstFrameMetadata(test_data);
    if (std::is_same<T, SimpleBlockParser>::value) {
      auto& expectation = EXPECT_CALL(
          callback_, OnSimpleBlockBegin(metadata.parent_element,
                                        expected_simple_block, NotNull()));
      if (set_action) {
        expectation.Times(1);
      } else {
        expectation.WillOnce(Return(Status(Status::kOkCompleted)));
      }
      EXPECT_CALL(callback_, OnBlockBegin(_, _, _)).Times(0);
    } else {
      auto& expectation = EXPECT_CALL(
          callback_,
          OnBlockBegin(metadata.parent_element, expected_block, NotNull()));
      if (set_action) {
        expectation.Times(1);
      } else {
        expectation.WillOnce(Return(Status(Status::kOkCompleted)));
      }
      EXPECT_CALL(callback_, OnSimpleBlockBegin(_, _, _)).Times(0);
    }

    std::uint8_t expected_frame_byte_value = 0;
    for (const std::uint64_t frame_size : test_data.expected_frame_sizes) {
      metadata.size = frame_size;
      const FrameHandler frame_handler = {metadata, expected_frame_byte_value};

      // Incremental parsing will call OnFrame once for every byte, plus
      // maybe one more time if the first call reads zero bytes (if the reader
      // is blocked).
      const int this_frame_size = static_cast<int>(frame_size);
      EXPECT_CALL(callback_, OnFrame(metadata, NotNull(), NotNull()))
          .Times(incremental ? Between(this_frame_size, this_frame_size + 1)
                             : Exactly(1))
          .WillRepeatedly(Invoke(frame_handler));

      metadata.position += metadata.size;
      ++expected_frame_byte_value;
    }

    if (std::is_same<T, SimpleBlockParser>::value) {
      EXPECT_CALL(callback_, OnSimpleBlockEnd(metadata.parent_element,
                                              expected_simple_block))
          .Times(1);
      EXPECT_CALL(callback_, OnBlockEnd(_, _)).Times(0);
    } else {
      EXPECT_CALL(callback_,
                  OnBlockEnd(metadata.parent_element, expected_block))
          .Times(1);
      EXPECT_CALL(callback_, OnSimpleBlockEnd(_, _)).Times(0);
    }
  }

  // Runs a single test using the provided test data.
  void RunTest(const TestData& test_data) {
    SetReaderData(test_data.data);
    SetExpectations(test_data, false, true);

    ParseAndVerify();

    ValidateBlock(test_data, parser_.value());
  }

  // Same as RunTest(), except it forces parsers to parse one byte at a time.
  void RunIncrementalTest(const TestData& test_data) {
    SetReaderData(test_data.data);
    SetExpectations(test_data, true, true);

    IncrementalParseAndVerify();

    ValidateBlock(test_data, parser_.value());
  }

  // Tests invalid element sizes.
  void TestInvalidElementSize() {
    TestInit(0, Status::kInvalidElementSize);
    TestInit(4, Status::kInvalidElementSize);
    TestInit(kUnknownElementSize, Status::kInvalidElementSize);
  }

  // Tests invalid blocks by feeding only the header of the block into the
  // parser.
  void TestInvalidHeaderOnly(const TestData& test_data) {
    EXPECT_CALL(callback_, OnFrame(_, _, _)).Times(0);
    EXPECT_CALL(callback_, OnBlockEnd(_, _)).Times(0);
    EXPECT_CALL(callback_, OnSimpleBlockEnd(_, _)).Times(0);

    SetReaderData(test_data.data);

    ParseAndExpectResult(Status::kInvalidElementValue,
                         test_data.expected_frame_start_position);
  }

  // Tests an invalid fixed-lace block that has inconsistent frame sizes.
  void TestInvalidFixedLaceSizes() {
    SetReaderData({
        0x81,  // Track number = 1.
        0x00, 0x00,  // Timecode = 0.
        0x84,  // Flags = key_frame | fixed_lacing.
        0x01,  // Lace count - 1 = 1 (2 frames).

        // Lace data (2 frames).
        0x00, 0x00,  // Frame 0.
        0x01,  // Frame 1 (invalid: inconsistent frame size).
    });

    EXPECT_CALL(callback_, OnFrame(_, _, _)).Times(0);
    EXPECT_CALL(callback_, OnBlockEnd(_, _)).Times(0);
    EXPECT_CALL(callback_, OnSimpleBlockEnd(_, _)).Times(0);

    ParseAndExpectResult(Status::kInvalidElementValue);
  }

  // Tests setting the action to Action::kSkip in Callback::OnSimpleBlockBegin
  // for the SimpleBlockParser.
  void TestSimpleBlockSkip(const TestData& test_data) {
    SetReaderData(test_data.data);

    const SimpleBlock expected_simple_block = ExpectedSimpleBlock(test_data);
    const FrameMetadata metadata = FirstFrameMetadata(test_data);

    EXPECT_CALL(callback_, OnSimpleBlockBegin(metadata.parent_element,
                                              expected_simple_block, NotNull()))
        .WillOnce(Return(Status(Status::kOkPartial)))
        .WillOnce(DoAll(SetArgPointee<2>(Action::kSkip),
                        Return(Status(Status::kOkCompleted))));

    EXPECT_CALL(callback_, OnFrame(_, _, _)).Times(0);
    EXPECT_CALL(callback_, OnBlockEnd(_, _)).Times(0);
    EXPECT_CALL(callback_, OnSimpleBlockEnd(_, _)).Times(0);

    IncrementalParseAndVerify();
  }

 protected:
  using ElementParserTest<T, id>::callback_;
  using ElementParserTest<T, id>::IncrementalParseAndVerify;
  using ElementParserTest<T, id>::metadata_;
  using ElementParserTest<T, id>::ParseAndExpectResult;
  using ElementParserTest<T, id>::ParseAndVerify;
  using ElementParserTest<T, id>::parser_;
  using ElementParserTest<T, id>::SetReaderData;
  using ElementParserTest<T, id>::TestInit;

 private:
  // Gets the FrameMetadata for the very first frame in the test data.
  FrameMetadata FirstFrameMetadata(const TestData& test_data) {
    FrameMetadata metadata;
    metadata.parent_element = metadata_;
    metadata.parent_element.size = test_data.data.size();
    metadata.position = test_data.expected_frame_start_position +
                        metadata.parent_element.position +
                        metadata.parent_element.header_size;
    metadata.size = test_data.expected_frame_sizes[0];
    return metadata;
  }
};

class BlockParserTest : public BasicBlockParserTest<BlockParser, Id::kBlock> {};

TEST_F(BlockParserTest, InvalidElementSize) { TestInvalidElementSize(); }

TEST_F(BlockParserTest, InvalidHeaderOnlyNoLacing) {
  TestInvalidHeaderOnly(no_lacing);
}

TEST_F(BlockParserTest, InvalidHeaderOnlyFixedLacing) {
  TestInvalidHeaderOnly(fixed_lacing);
}

TEST_F(BlockParserTest, InvalidFixedLaceSizes) { TestInvalidFixedLaceSizes(); }

TEST_F(BlockParserTest, BlockNoFlags) { RunTest(no_flags); }

TEST_F(BlockParserTest, BlockFlags) { RunTest(block_flags); }

TEST_F(BlockParserTest, EbmlLacingOneFrame) { RunTest(ebml_lacing_one_frame); }

TEST_F(BlockParserTest, EbmlLacing) { RunTest(ebml_lacing); }

TEST_F(BlockParserTest, XiphLacingOneFrame) { RunTest(xiph_lacing_one_frame); }

TEST_F(BlockParserTest, XiphLacing) { RunTest(xiph_lacing); }

TEST_F(BlockParserTest, FixedLacingOneFrame) {
  RunTest(fixed_lacing_one_frame);
}

TEST_F(BlockParserTest, FixedLacing) { RunTest(fixed_lacing); }

TEST_F(BlockParserTest, NoLacing) { RunTest(no_lacing); }

TEST_F(BlockParserTest, BlockWithPositionAndHeaderSize) {
  metadata_.position = 15;
  metadata_.header_size = 3;
  RunTest(no_lacing);
}

TEST_F(BlockParserTest, IncrementalBlockFlags) {
  RunIncrementalTest(block_flags);
}

TEST_F(BlockParserTest, IncrementalEbmlLacingOneFrame) {
  RunIncrementalTest(ebml_lacing_one_frame);
}

TEST_F(BlockParserTest, IncrementalEbmlLacing) {
  RunIncrementalTest(ebml_lacing);
}

TEST_F(BlockParserTest, IncrementalXiphLacingOneFrame) {
  RunIncrementalTest(xiph_lacing_one_frame);
}

TEST_F(BlockParserTest, IncrementalXiphLacing) {
  RunIncrementalTest(xiph_lacing);
}

TEST_F(BlockParserTest, IncrementalFixedLacingOneFrame) {
  RunIncrementalTest(fixed_lacing_one_frame);
}

TEST_F(BlockParserTest, IncrementalFixedLacing) {
  RunIncrementalTest(fixed_lacing);
}

TEST_F(BlockParserTest, DefaultActionIsRead) {
  SetReaderData(fixed_lacing_one_frame.data);
  SetExpectations(fixed_lacing_one_frame, false, false);
  ParseAndVerify();
  ValidateBlock(fixed_lacing_one_frame, parser_.value());
}

TEST_F(BlockParserTest, IncrementalNoLacing) { RunIncrementalTest(no_lacing); }

class SimpleBlockParserTest
    : public BasicBlockParserTest<SimpleBlockParser, Id::kSimpleBlock> {};

TEST_F(SimpleBlockParserTest, InvalidElementSize) { TestInvalidElementSize(); }

TEST_F(SimpleBlockParserTest, InvalidHeaderOnlyNoLacing) {
  TestInvalidHeaderOnly(no_lacing);
}

TEST_F(SimpleBlockParserTest, InvalidHeaderOnlyFixedLacing) {
  TestInvalidHeaderOnly(fixed_lacing);
}

TEST_F(SimpleBlockParserTest, InvalidFixedLaceSizes) {
  TestInvalidFixedLaceSizes();
}

TEST_F(SimpleBlockParserTest, SimpleBlockSkip) {
  TestSimpleBlockSkip(no_flags);
}

TEST_F(SimpleBlockParserTest, SimpleBlockNoFlags) { RunTest(no_flags); }

TEST_F(SimpleBlockParserTest, SimpleBlockFlags) { RunTest(simple_block_flags); }

TEST_F(SimpleBlockParserTest, EbmlLacingOneFrame) {
  RunTest(ebml_lacing_one_frame);
}

TEST_F(SimpleBlockParserTest, EbmlLacing) { RunTest(ebml_lacing); }

TEST_F(SimpleBlockParserTest, XiphLacingOneFrame) {
  RunTest(xiph_lacing_one_frame);
}

TEST_F(SimpleBlockParserTest, XiphLacing) { RunTest(xiph_lacing); }

TEST_F(SimpleBlockParserTest, FixedLacingOneFrame) {
  RunTest(fixed_lacing_one_frame);
}

TEST_F(SimpleBlockParserTest, FixedLacing) { RunTest(fixed_lacing); }

TEST_F(SimpleBlockParserTest, NoLacing) { RunTest(no_lacing); }

TEST_F(BlockParserTest, SimpleBlockWithPositionAndHeaderSize) {
  metadata_.position = 16;
  metadata_.header_size = 4;
  RunTest(no_lacing);
}

TEST_F(SimpleBlockParserTest, IncrementalSimpleBlockFlags) {
  RunIncrementalTest(simple_block_flags);
}

TEST_F(SimpleBlockParserTest, IncrementalEbmlLacingOneFrame) {
  RunIncrementalTest(ebml_lacing_one_frame);
}

TEST_F(SimpleBlockParserTest, IncrementalEbmlLacing) {
  RunIncrementalTest(ebml_lacing);
}

TEST_F(SimpleBlockParserTest, IncrementalXiphLacingOneFrame) {
  RunIncrementalTest(xiph_lacing_one_frame);
}

TEST_F(SimpleBlockParserTest, IncrementalXiphLacing) {
  RunIncrementalTest(xiph_lacing);
}

TEST_F(SimpleBlockParserTest, IncrementalFixedLacingOneFrame) {
  RunIncrementalTest(fixed_lacing_one_frame);
}

TEST_F(SimpleBlockParserTest, IncrementalFixedLacing) {
  RunIncrementalTest(fixed_lacing);
}

TEST_F(SimpleBlockParserTest, IncrementalNoLacing) {
  RunIncrementalTest(no_lacing);
}

TEST_F(SimpleBlockParserTest, DefaultActionIsRead) {
  SetReaderData(fixed_lacing_one_frame.data);
  SetExpectations(fixed_lacing_one_frame, false, false);
  ParseAndVerify();
  ValidateBlock(fixed_lacing_one_frame, parser_.value());
}

}  // namespace
