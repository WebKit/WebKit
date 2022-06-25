// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/track_entry_parser.h"

#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::Audio;
using webm::ContentEncoding;
using webm::ContentEncodings;
using webm::DisplayUnit;
using webm::ElementParserTest;
using webm::Id;
using webm::TrackEntry;
using webm::TrackEntryParser;
using webm::TrackType;
using webm::Video;

namespace {

class TrackEntryParserTest
    : public ElementParserTest<TrackEntryParser, Id::kTrackEntry> {};

TEST_F(TrackEntryParserTest, DefaultParse) {
  EXPECT_CALL(callback_, OnTrackEntry(metadata_, TrackEntry{})).Times(1);

  ParseAndVerify();
}

TEST_F(TrackEntryParserTest, DefaultValues) {
  SetReaderData({
      0xD7,  // ID = 0xD7 (TrackNumber).
      0x20, 0x00, 0x00,  // Size = 0.

      0x73, 0xC5,  // ID = 0x73C5 (TrackUID).
      0x20, 0x00, 0x00,  // Size = 0.

      0x83,  // ID = 0x83 (TrackType).
      0x20, 0x00, 0x00,  // Size = 0.

      0xB9,  // ID = 0xB9 (FlagEnabled).
      0x20, 0x00, 0x00,  // Size = 0.

      0x88,  // ID = 0x88 (FlagDefault).
      0x20, 0x00, 0x00,  // Size = 0.

      0x55, 0xAA,  // ID = 0x55AA (FlagForced).
      0x20, 0x00, 0x00,  // Size = 0.

      0x9C,  // ID = 0x9C (FlagLacing).
      0x20, 0x00, 0x00,  // Size = 0.

      0x23, 0xE3, 0x83,  // ID = 0x23E383 (DefaultDuration).
      0x80,  // Size = 0.

      0x53, 0x6E,  // ID = 0x536E (Name).
      0x20, 0x00, 0x00,  // Size = 0.

      0x22, 0xB5, 0x9C,  // ID = 0x22B59C (Language).
      0x80,  // Size = 0.

      0x86,  // ID = 0x86 (CodecID).
      0x20, 0x00, 0x00,  // Size = 0.

      0x63, 0xA2,  // ID = 0x63A2 (CodecPrivate).
      0x20, 0x00, 0x00,  // Size = 0.

      0x25, 0x86, 0x88,  // ID = 0x258688 (CodecName).
      0x80,  // Size = 0.

      0x56, 0xAA,  // ID = 0x56AA (CodecDelay).
      0x20, 0x00, 0x00,  // Size = 0.

      0x56, 0xBB,  // ID = 0x56BB (SeekPreRoll).
      0x20, 0x00, 0x00,  // Size = 0.

      0xE0,  // ID = 0xE0 (Video).
      0x20, 0x00, 0x00,  // Size = 0.

      0xE1,  // ID = 0xE1 (Audio).
      0x20, 0x00, 0x00,  // Size = 0.

      0x6D, 0x80,  // ID = 0x6D80 (ContentEncodings).
      0x20, 0x00, 0x00,  // Size = 0.
  });

  TrackEntry track_entry;
  track_entry.track_number.Set(0, true);
  track_entry.track_uid.Set(0, true);
  track_entry.track_type.Set(TrackType{}, true);
  track_entry.is_enabled.Set(true, true);
  track_entry.is_default.Set(true, true);
  track_entry.is_forced.Set(false, true);
  track_entry.uses_lacing.Set(true, true);
  track_entry.default_duration.Set(0, true);
  track_entry.name.Set("", true);
  track_entry.language.Set("eng", true);
  track_entry.codec_id.Set("", true);
  track_entry.codec_private.Set(std::vector<std::uint8_t>{}, true);
  track_entry.codec_name.Set("", true);
  track_entry.codec_delay.Set(0, true);
  track_entry.seek_pre_roll.Set(0, true);
  track_entry.video.Set(Video{}, true);
  track_entry.audio.Set(Audio{}, true);
  track_entry.content_encodings.Set(ContentEncodings{}, true);

  EXPECT_CALL(callback_, OnTrackEntry(metadata_, track_entry)).Times(1);

  ParseAndVerify();
}

TEST_F(TrackEntryParserTest, CustomValues) {
  SetReaderData({
      0xD7,  // ID = 0xD7 (TrackNumber).
      0x20, 0x00, 0x01,  // Size = 1.
      0x01,  // Body (value = 1).

      0x73, 0xC5,  // ID = 0x73C5 (TrackUID).
      0x20, 0x00, 0x01,  // Size = 1.
      0x02,  // Body (value = 2).

      0x83,  // ID = 0x83 (TrackType).
      0x20, 0x00, 0x01,  // Size = 1.
      0x03,  // Body (value = complex).

      0xB9,  // ID = 0xB9 (FlagEnabled).
      0x20, 0x00, 0x01,  // Size = 1.
      0x00,  // Body (value = 0).

      0x88,  // ID = 0x88 (FlagDefault).
      0x20, 0x00, 0x01,  // Size = 1.
      0x00,  // Body (value = 0).

      0x55, 0xAA,  // ID = 0x55AA (FlagForced).
      0x20, 0x00, 0x01,  // Size = 1.
      0x01,  // Body (value = 1).

      0x9C,  // ID = 0x9C (FlagLacing).
      0x20, 0x00, 0x01,  // Size = 1.
      0x00,  // Body (value = 0).

      0x23, 0xE3, 0x83,  // ID = 0x23E383 (DefaultDuration).
      0x81,  // Size = 1.
      0x04,  // Body (value = 4).

      0x53, 0x6E,  // ID = 0x536E (Name).
      0x20, 0x00, 0x01,  // Size = 1.
      0x41,  // Body (value = "A").

      0x22, 0xB5, 0x9C,  // ID = 0x22B59C (Language).
      0x81,  // Size = 1.
      0x42,  // Body (value = "B").

      0x86,  // ID = 0x86 (CodecID).
      0x20, 0x00, 0x01,  // Size = 1.
      0x43,  // Body (value = "C").

      0x63, 0xA2,  // ID = 0x63A2 (CodecPrivate).
      0x20, 0x00, 0x01,  // Size = 1.
      0x00,  // Body.

      0x25, 0x86, 0x88,  // ID = 0x258688 (CodecName).
      0x81,  // Size = 1.
      0x44,  // Body (value = "D").

      0x56, 0xAA,  // ID = 0x56AA (CodecDelay).
      0x20, 0x00, 0x01,  // Size = 1.
      0x05,  // Body (value = 5).

      0x56, 0xBB,  // ID = 0x56BB (SeekPreRoll).
      0x20, 0x00, 0x01,  // Size = 1.
      0x06,  // Body (value = 6).

      0xE0,  // ID = 0xE0 (Video).
      0x20, 0x00, 0x06,  // Size = 6.

      0x54, 0xB2,  //   ID = 0x54B2 (DisplayUnit).
      0x20, 0x00, 0x01,  //   Size = 1.
      0x03,  //   Body (value = display aspect ratio).

      0xE1,  // ID = 0xE1 (Audio).
      0x20, 0x00, 0x05,  // Size = 5.

      0x9F,  //   ID = 0x9F (Channels).
      0x20, 0x00, 0x01,  //   Size = 1.
      0x08,  //   Body (value = 8).

      0x6D, 0x80,  // ID = 0x6D80 (ContentEncodings).
      0x20, 0x00, 0x0B,  // Size = 11.

      0x62, 0x40,  //   ID = 0x6240 (ContentEncoding).
      0x20, 0x00, 0x06,  //   Size = 6.

      0x50, 0x31,  //     ID = 0x5031 (ContentEncodingOrder).
      0x20, 0x00, 0x01,  //     Size = 1.
      0x01,  //     Body (value = 1).
  });

  TrackEntry track_entry;
  track_entry.track_number.Set(1, true);
  track_entry.track_uid.Set(2, true);
  track_entry.track_type.Set(TrackType::kComplex, true);
  track_entry.is_enabled.Set(false, true);
  track_entry.is_default.Set(false, true);
  track_entry.is_forced.Set(true, true);
  track_entry.uses_lacing.Set(false, true);
  track_entry.default_duration.Set(4, true);
  track_entry.name.Set("A", true);
  track_entry.language.Set("B", true);
  track_entry.codec_id.Set("C", true);
  track_entry.codec_private.Set(std::vector<std::uint8_t>{0x00}, true);
  track_entry.codec_name.Set("D", true);
  track_entry.codec_delay.Set(5, true);
  track_entry.seek_pre_roll.Set(6, true);

  Video expected_video;
  expected_video.display_unit.Set(DisplayUnit::kDisplayAspectRatio, true);
  track_entry.video.Set(expected_video, true);

  Audio expected_audio;
  expected_audio.channels.Set(8, true);
  track_entry.audio.Set(expected_audio, true);

  ContentEncoding expected_encoding;
  expected_encoding.order.Set(1, true);
  ContentEncodings expected_encodings;
  expected_encodings.encodings.emplace_back(expected_encoding, true);
  track_entry.content_encodings.Set(expected_encodings, true);

  EXPECT_CALL(callback_, OnTrackEntry(metadata_, track_entry)).Times(1);

  ParseAndVerify();
}

}  // namespace
