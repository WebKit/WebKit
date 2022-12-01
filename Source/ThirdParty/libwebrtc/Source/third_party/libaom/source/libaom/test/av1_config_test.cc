/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <string.h>

#include "common/av1_config.h"
#include "test/util.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

//
// Input buffers containing exactly one Sequence Header OBU.
//
// Each buffer is named according to the OBU storage format (Annex-B vs Low
// Overhead Bitstream Format) and the type of Sequence Header OBU ("Full"
// Sequence Header OBUs vs Sequence Header OBUs with the
// reduced_still_image_flag set).
//
const uint8_t kAnnexBFullSequenceHeaderObu[] = { 0x0c, 0x08, 0x00, 0x00, 0x00,
                                                 0x04, 0x45, 0x7e, 0x3e, 0xff,
                                                 0xfc, 0xc0, 0x20 };
const uint8_t kAnnexBReducedStillImageSequenceHeaderObu[] = {
  0x08, 0x08, 0x18, 0x22, 0x2b, 0xf1, 0xfe, 0xc0, 0x20
};

const uint8_t kLobfFullSequenceHeaderObu[] = { 0x0a, 0x0b, 0x00, 0x00, 0x00,
                                               0x04, 0x45, 0x7e, 0x3e, 0xff,
                                               0xfc, 0xc0, 0x20 };

const uint8_t kLobfReducedStillImageSequenceHeaderObu[] = { 0x0a, 0x07, 0x18,
                                                            0x22, 0x2b, 0xf1,
                                                            0xfe, 0xc0, 0x20 };

const uint8_t kAv1cAllZero[] = { 0, 0, 0, 0 };

// The size of AV1 config when no configOBUs are present at the end of the
// configuration structure.
const size_t kAv1cNoConfigObusSize = 4;

bool VerifyAv1c(const uint8_t *const obu_buffer, size_t obu_buffer_length,
                bool is_annexb) {
  Av1Config av1_config;
  memset(&av1_config, 0, sizeof(av1_config));
  bool parse_ok = get_av1config_from_obu(obu_buffer, obu_buffer_length,
                                         is_annexb, &av1_config) == 0;
  if (parse_ok) {
    EXPECT_EQ(1, av1_config.marker);
    EXPECT_EQ(1, av1_config.version);
    EXPECT_EQ(0, av1_config.seq_profile);
    EXPECT_EQ(0, av1_config.seq_level_idx_0);
    EXPECT_EQ(0, av1_config.seq_tier_0);
    EXPECT_EQ(0, av1_config.high_bitdepth);
    EXPECT_EQ(0, av1_config.twelve_bit);
    EXPECT_EQ(0, av1_config.monochrome);
    EXPECT_EQ(1, av1_config.chroma_subsampling_x);
    EXPECT_EQ(1, av1_config.chroma_subsampling_y);
    EXPECT_EQ(0, av1_config.chroma_sample_position);
    EXPECT_EQ(0, av1_config.initial_presentation_delay_present);
    EXPECT_EQ(0, av1_config.initial_presentation_delay_minus_one);
  }
  return parse_ok && ::testing::Test::HasFailure() == false;
}

TEST(Av1Config, ObuInvalidInputs) {
  Av1Config av1_config;
  memset(&av1_config, 0, sizeof(av1_config));
  ASSERT_EQ(-1, get_av1config_from_obu(nullptr, 0, 0, nullptr));
  ASSERT_EQ(-1, get_av1config_from_obu(&kLobfFullSequenceHeaderObu[0], 0, 0,
                                       nullptr));
  ASSERT_EQ(-1, get_av1config_from_obu(&kLobfFullSequenceHeaderObu[0],
                                       sizeof(kLobfFullSequenceHeaderObu), 0,
                                       nullptr));
  ASSERT_EQ(-1, get_av1config_from_obu(
                    nullptr, sizeof(kLobfFullSequenceHeaderObu), 0, nullptr));
  ASSERT_EQ(-1, get_av1config_from_obu(&kLobfFullSequenceHeaderObu[0], 0, 0,
                                       &av1_config));
}

TEST(Av1Config, ReadInvalidInputs) {
  Av1Config av1_config;
  memset(&av1_config, 0, sizeof(av1_config));
  size_t bytes_read = 0;
  ASSERT_EQ(-1, read_av1config(nullptr, 0, nullptr, nullptr));
  ASSERT_EQ(-1, read_av1config(nullptr, 4, nullptr, nullptr));
  ASSERT_EQ(-1, read_av1config(&kAv1cAllZero[0], 0, nullptr, nullptr));
  ASSERT_EQ(-1, read_av1config(&kAv1cAllZero[0], 4, &bytes_read, nullptr));
  ASSERT_EQ(-1, read_av1config(nullptr, 4, &bytes_read, &av1_config));
}

TEST(Av1Config, WriteInvalidInputs) {
  Av1Config av1_config;
  memset(&av1_config, 0, sizeof(av1_config));
  size_t bytes_written = 0;
  uint8_t av1c_buffer[4] = { 0 };
  ASSERT_EQ(-1, write_av1config(nullptr, 0, nullptr, nullptr));
  ASSERT_EQ(-1, write_av1config(&av1_config, 0, nullptr, nullptr));
  ASSERT_EQ(-1, write_av1config(&av1_config, 0, &bytes_written, nullptr));

  ASSERT_EQ(-1,
            write_av1config(&av1_config, 0, &bytes_written, &av1c_buffer[0]));
  ASSERT_EQ(-1, write_av1config(&av1_config, 4, &bytes_written, nullptr));
}

TEST(Av1Config, GetAv1ConfigFromLobfObu) {
  // Test parsing of a Sequence Header OBU with the reduced_still_picture_header
  // unset-- aka a full Sequence Header OBU.
  ASSERT_TRUE(VerifyAv1c(kLobfFullSequenceHeaderObu,
                         sizeof(kLobfFullSequenceHeaderObu), false));

  // Test parsing of a reduced still image Sequence Header OBU.
  ASSERT_TRUE(VerifyAv1c(kLobfReducedStillImageSequenceHeaderObu,
                         sizeof(kLobfReducedStillImageSequenceHeaderObu),
                         false));
}

TEST(Av1Config, GetAv1ConfigFromAnnexBObu) {
  // Test parsing of a Sequence Header OBU with the reduced_still_picture_header
  // unset-- aka a full Sequence Header OBU.
  ASSERT_TRUE(VerifyAv1c(kAnnexBFullSequenceHeaderObu,
                         sizeof(kAnnexBFullSequenceHeaderObu), true));

  // Test parsing of a reduced still image Sequence Header OBU.
  ASSERT_TRUE(VerifyAv1c(kAnnexBReducedStillImageSequenceHeaderObu,
                         sizeof(kAnnexBReducedStillImageSequenceHeaderObu),
                         true));
}

TEST(Av1Config, ReadWriteConfig) {
  Av1Config av1_config;
  memset(&av1_config, 0, sizeof(av1_config));

  // Test writing out the AV1 config.
  size_t bytes_written = 0;
  uint8_t av1c_buffer[4] = { 0 };
  ASSERT_EQ(0, write_av1config(&av1_config, sizeof(av1c_buffer), &bytes_written,
                               &av1c_buffer[0]));
  ASSERT_EQ(kAv1cNoConfigObusSize, bytes_written);
  for (size_t i = 0; i < kAv1cNoConfigObusSize; ++i) {
    ASSERT_EQ(kAv1cAllZero[i], av1c_buffer[i])
        << "Mismatch in output Av1Config at offset=" << i;
  }

  // Test reading the AV1 config.
  size_t bytes_read = 0;
  ASSERT_EQ(0, read_av1config(&kAv1cAllZero[0], sizeof(kAv1cAllZero),
                              &bytes_read, &av1_config));
  ASSERT_EQ(kAv1cNoConfigObusSize, bytes_read);
  ASSERT_EQ(0, write_av1config(&av1_config, sizeof(av1c_buffer), &bytes_written,
                               &av1c_buffer[0]));
  for (size_t i = 0; i < kAv1cNoConfigObusSize; ++i) {
    ASSERT_EQ(kAv1cAllZero[i], av1c_buffer[i])
        << "Mismatch in output Av1Config at offset=" << i;
  }
}

}  // namespace
