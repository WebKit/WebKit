/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom/aom_codec.h"
#include "av1/common/blockd.h"
#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/y4m_video_source.h"
#include "test/util.h"

// Verify the optimized implementation of get_partition_subsize() produces the
// same results as the Partition_Subsize lookup table in the spec.
TEST(BlockdTest, GetPartitionSubsize) {
  // The Partition_Subsize table in the spec (Section 9.3. Conversion tables).
  /* clang-format off */
  static const BLOCK_SIZE kPartitionSubsize[10][BLOCK_SIZES_ALL] = {
    {
                                    BLOCK_4X4,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X128,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X128,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X4,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X128,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X128,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X4,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }
  };
  /* clang-format on */

  for (int partition = 0; partition < 10; partition++) {
    for (int bsize = BLOCK_4X4; bsize < BLOCK_SIZES_ALL; bsize++) {
      EXPECT_EQ(kPartitionSubsize[partition][bsize],
                get_partition_subsize(static_cast<BLOCK_SIZE>(bsize),
                                      static_cast<PARTITION_TYPE>(partition)));
    }
  }
}

#if CONFIG_AV1_DECODER && CONFIG_AV1_ENCODER
namespace {
// This class is used to validate if sb_size configured is respected
// in the bitstream
class SuperBlockSizeTestLarge
    : public ::libaom_test::CodecTestWith3Params<
          libaom_test::TestMode, aom_superblock_size_t, aom_rc_mode>,
      public ::libaom_test::EncoderTest {
 protected:
  SuperBlockSizeTestLarge()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        superblock_size_(GET_PARAM(2)), rc_end_usage_(GET_PARAM(3)) {
    sb_size_violated_ = false;
  }
  ~SuperBlockSizeTestLarge() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = rc_end_usage_;
    cfg_.g_threads = 1;
    cfg_.g_lag_in_frames = 35;
    cfg_.rc_target_bitrate = 1000;
  }

  bool DoDecode() const override { return true; }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 5);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AV1E_SET_SUPERBLOCK_SIZE, superblock_size_);
    }
  }

  bool HandleDecodeResult(const aom_codec_err_t res_dec,
                          libaom_test::Decoder *decoder) override {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (AOM_CODEC_OK == res_dec &&
        superblock_size_ != AOM_SUPERBLOCK_SIZE_DYNAMIC) {
      aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
      aom_superblock_size_t sb_size;
      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_SB_SIZE, &sb_size);
      if (superblock_size_ != sb_size) {
        sb_size_violated_ = true;
      }
    }
    return AOM_CODEC_OK == res_dec;
  }

  ::libaom_test::TestMode encoding_mode_;
  aom_superblock_size_t superblock_size_;
  bool sb_size_violated_;
  aom_rc_mode rc_end_usage_;
};

TEST_P(SuperBlockSizeTestLarge, SuperBlockSizeTest) {
  ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 1);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(sb_size_violated_, false)
      << "Failed for SB size " << superblock_size_;
}

const ::libaom_test::TestMode kTestModes[] = {
#if CONFIG_REALTIME_ONLY
  ::libaom_test::kRealTime
#else
  ::libaom_test::kRealTime, ::libaom_test::kOnePassGood,
  ::libaom_test::kTwoPassGood
#endif
};

AV1_INSTANTIATE_TEST_SUITE(SuperBlockSizeTestLarge,
                           ::testing::ValuesIn(kTestModes),
                           ::testing::Values(AOM_SUPERBLOCK_SIZE_64X64,
                                             AOM_SUPERBLOCK_SIZE_128X128),
                           ::testing::Values(AOM_Q, AOM_VBR, AOM_CBR, AOM_CQ));
}  // namespace
#endif
