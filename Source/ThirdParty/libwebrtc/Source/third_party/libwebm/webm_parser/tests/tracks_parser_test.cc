// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/tracks_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/buffer_reader.h"

using webm::ElementParserTest;
using webm::Id;
using webm::TracksParser;

namespace {

class TracksParserTest : public ElementParserTest<TracksParser, Id::kTracks> {};

// TODO(mjbshaw): validate results via Callback.

TEST_F(TracksParserTest, DefaultValues) {
  ParseAndVerify();

  SetReaderData({
      0xAE,  // ID = 0xAE (TrackEntry).
      0x80,  // Size = 0.
  });
  ParseAndVerify();
}

TEST_F(TracksParserTest, RepeatedValues) {
  SetReaderData({
      0xAE,  // ID = 0xAE (TrackEntry).
      0x83,  // Size = 3.

      0xD7,  //   ID = 0xD7 (TrackNumber).
      0x81,  //   Size = 1.
      0x01,  //   Body (value = 1).

      0xAE,  // ID = 0xAE (TrackEntry).
      0x83,  // Size = 3.

      0xD7,  //   ID = 0xD7 (TrackNumber).
      0x81,  //   Size = 1.
      0x02,  //   Body (value = 2).
  });

  ParseAndVerify();
}

}  // namespace
