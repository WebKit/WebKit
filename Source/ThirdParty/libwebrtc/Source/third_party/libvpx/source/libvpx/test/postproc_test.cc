/*
 *  Copyright (c) 2026 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <memory>
#include <new>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "test/ivf_video_source.h"
#include "test/test_vectors.h"
#include "test/util.h"
#if CONFIG_WEBM_IO
#include "test/webm_video_source.h"
#endif
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

namespace {

struct TestParam {
  int flags;
  const char *filename;
};

std::ostream &operator<<(std::ostream &os, const TestParam &param) {
  std::string flags_str;
  if (param.flags == VP8_NOFILTERING) {
    flags_str = "default flags";
  } else {
    if (param.flags & VP8_DEBLOCK) flags_str += "deblock";
    if (param.flags & VP8_DEMACROBLOCK) {
      if (!flags_str.empty()) flags_str += ", ";
      flags_str += "demacroblock";
    }
    if (param.flags & VP8_ADDNOISE) {
      if (!flags_str.empty()) flags_str += ", ";
      flags_str += "addnoise";
    }
    if (param.flags & VP8_MFQE) {
      if (!flags_str.empty()) flags_str += ", ";
      flags_str += "mfqe";
    }
  }

  return os << "(" << flags_str << ", " << param.filename << ")";
}

class PostProcTest : public ::libvpx_test::DecoderTest,
                     public ::libvpx_test::CodecTestWithParam<TestParam> {
 protected:
  PostProcTest()
      : DecoderTest(GET_PARAM(0)), postproc_flags_(GET_PARAM(1).flags) {}

  ~PostProcTest() override = default;

  void PreDecodeFrameHook(const libvpx_test::CompressedVideoSource &video,
                          libvpx_test::Decoder *decoder) override {
    // VP8_NOFILTERING is used to test the default flags set when only
    // VPX_CODEC_USE_POSTPROC is used.
    if (video.frame_number() == 0 && postproc_flags_ != VP8_NOFILTERING) {
      const vp8_postproc_cfg_t vp8_pp_cfg = { postproc_flags_,
                                              /*deblocking_level=*/2,
                                              /*noise_level=*/10 };
      decoder->Control(VP8_SET_POSTPROC, &vp8_pp_cfg);
    }
  }

  void DecodeTest();

 private:
  int postproc_flags_;
};

void PostProcTest::DecodeTest() {
  const std::string filename = GET_PARAM(1).filename;
  vpx_codec_dec_cfg_t cfg = vpx_codec_dec_cfg_t();
  cfg.threads = 2;

  // Open compressed video file.
  std::unique_ptr<libvpx_test::CompressedVideoSource> video;
  if (filename.substr(filename.length() - 3, 3) == "ivf") {
    video.reset(new (std::nothrow) libvpx_test::IVFVideoSource(filename));
  } else if (filename.substr(filename.length() - 4, 4) == "webm") {
#if CONFIG_WEBM_IO
    video.reset(new (std::nothrow) libvpx_test::WebMVideoSource(filename));
#else
    GTEST_SKIP() << "WebM IO is disabled, skipping test vector " << filename;
#endif
  }
  ASSERT_NE(video, nullptr);
  video->Init();

  // Set decode config and flags.
  set_cfg(cfg);
  set_flags(VPX_CODEC_USE_POSTPROC);

  // Decode frame.
  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get(), cfg));
}

TEST_P(PostProcTest, Decode) { DecodeTest(); }

std::vector<int> GeneratePostProcFlags() {
  std::vector<int> flags;

  flags.push_back(VP8_NOFILTERING);
  // Add each flag (or flags) on their own, and with add noise enabled.
  static constexpr std::array<int, 7> kFlags = { VP8_DEBLOCK,
                                                 VP8_DEMACROBLOCK,
                                                 VP8_ADDNOISE,
                                                 VP8_MFQE,
                                                 VP8_MFQE | VP8_DEBLOCK,
                                                 VP8_MFQE | VP8_DEMACROBLOCK,
                                                 VP8_MFQE | VP8_DEBLOCK |
                                                     VP8_DEMACROBLOCK };
  for (const int flag : kFlags) {
    flags.push_back(flag);
    if (flag != VP8_ADDNOISE) {
      flags.push_back(flag | VP8_ADDNOISE);
    }
  }

  std::sort(flags.begin(), flags.end());
  return flags;
}

template <typename Iterator>
std::vector<TestParam> GenerateTestParams(const std::vector<int> &flags,
                                          Iterator begin, Iterator end) {
  std::vector<TestParam> params;
  for (const int flag : flags) {
    for (Iterator it = begin; it != end; ++it) {
      params.push_back({ flag, *it });
    }
  }
  return params;
}

template <size_t N>
std::vector<TestParam> GenerateTestParams(
    const std::vector<int> &flags,
    const std::array<const char *, N> &filenames) {
  return GenerateTestParams(flags, filenames.cbegin(), filenames.cend());
}

class PostProcTestInvalidFiles : public PostProcTest {
 protected:
  void HandlePeekResult(libvpx_test::Decoder *const /*decoder*/,
                        libvpx_test::CompressedVideoSource * /*video*/,
                        const vpx_codec_err_t /*res_peek*/) override {}

  bool HandleDecodeResult(const vpx_codec_err_t /*res_dec*/,
                          const libvpx_test::CompressedVideoSource & /*video*/,
                          libvpx_test::Decoder * /*decoder*/) override {
    // Ignore decode errors when decoding corrupted bitstreams.
    return true;
  }
};

TEST_P(PostProcTestInvalidFiles, Decode) { DecodeTest(); }

#if CONFIG_VP8_DECODER && CONFIG_POSTPROC
VP8_INSTANTIATE_TEST_SUITE(
    PostProcTest,
    ::testing::ValuesIn(GenerateTestParams(
        GeneratePostProcFlags(), libvpx_test::kVP8TestVectors,
        libvpx_test::kVP8TestVectors + libvpx_test::kNumVP8TestVectors)));

constexpr std::array<const char *, 5> kVP8InvalidFiles = {
  "invalid-bug-1443.ivf", "invalid-bug-148271109.ivf",
  "invalid-token-partition.ivf",
  "invalid-vp80-00-comprehensive-018.ivf.2kf_0x6.ivf",
  "invalid-vp80-00-comprehensive-s17661_r01-05_b6-.ivf"
};

VP8_INSTANTIATE_TEST_SUITE(PostProcTestInvalidFiles,
                           ::testing::ValuesIn(GenerateTestParams(
                               GeneratePostProcFlags(), kVP8InvalidFiles)));
#endif  // CONFIG_VP8_DECODER && CONFIG_POSTPROC

#if CONFIG_VP9_DECODER && CONFIG_VP9_POSTPROC
VP9_INSTANTIATE_TEST_SUITE(
    PostProcTest,
    ::testing::ValuesIn(GenerateTestParams(
        GeneratePostProcFlags(), libvpx_test::kVP9TestVectors,
        libvpx_test::kVP9TestVectors + libvpx_test::kNumVP9TestVectors)));

constexpr std::array<const char *, 27> kVP9InvalidFiles = {
  "invalid-crbug-1558.ivf", "invalid-crbug-1562.ivf",
  "invalid-crbug-629481.webm", "invalid-crbug-667044.webm",
  "invalid-vp90-01-v3.webm", "invalid-vp90-02-v2.webm",
  "invalid-vp90-03-v3.webm",
  "invalid-vp90-2-00-quantizer-00.webm.ivf.s5861_r01-05_b6-.v2.ivf",
  "invalid-vp90-2-00-quantizer-11.webm.ivf.s52984_r01-05_b6-.ivf",
  "invalid-vp90-2-00-quantizer-11.webm.ivf.s52984_r01-05_b6-z.ivf",
  // This file is disabled due to its high memory usage.
  // "invalid-vp90-2-00-quantizer-63.ivf.kf_65527x61446.ivf",
  "invalid-vp90-2-03-size-202x210.webm.ivf.s113306_r01-05_b6-.ivf",
  "invalid-vp90-2-03-size-224x196.webm.ivf.s44156_r01-05_b6-.ivf",
  "invalid-vp90-2-05-resize.ivf.s59293_r01-05_b6-.ivf",
  "invalid-vp90-2-07-frame_parallel-1.webm",
  "invalid-vp90-2-07-frame_parallel-2.webm",
  "invalid-vp90-2-07-frame_parallel-3.webm",
  "invalid-vp90-2-08-tile_1x2_frame_parallel.webm.ivf.s47039_r01-05_b6-.ivf",
  "invalid-vp90-2-08-tile_1x4_frame_parallel_all_key.webm",
  "invalid-vp90-2-08-tile_1x8_frame_parallel.webm.ivf.s288_r01-05_b6-.ivf",
  "invalid-vp90-2-09-aq2.webm.ivf.s3984_r01-05_b6-.v2.ivf",
  "invalid-vp90-2-09-subpixel-00.ivf.s19552_r01-05_b6-.v2.ivf",
  "invalid-vp90-2-09-subpixel-00.ivf.s20492_r01-05_b6-.v2.ivf",
  "invalid-vp90-2-10-show-existing-frame.webm.ivf.s180315_r01-05_b6-.ivf",
  "invalid-vp90-2-12-droppable_1.ivf.s3676_r01-05_b6-.ivf",
  "invalid-vp90-2-12-droppable_1.ivf.s73804_r01-05_b6-.ivf",
  "invalid-vp90-2-21-resize_inter_320x180_5_3-4.webm.ivf.s45551_r01-05_b6-.ivf",
  "invalid-vp91-2-mixedrefcsp-444to420.ivf"
};

VP9_INSTANTIATE_TEST_SUITE(PostProcTestInvalidFiles,
                           ::testing::ValuesIn(GenerateTestParams(
                               GeneratePostProcFlags(), kVP9InvalidFiles)));

#endif  // CONFIG_VP9_DECODER && CONFIG_VP9_POSTPROC

}  // namespace
