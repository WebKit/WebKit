/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstdio>
#include <ostream>
#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/ivf_video_source.h"
#include "test/util.h"
#include "test/video_source.h"

namespace {

struct DecodeParam {
  int threads;
  const char *filename;
  const char *res_filename;  // If nullptr, the result filename is
                             // filename + ".res".
};

// Constructs result file name.
std::string GetResFilename(const DecodeParam &param) {
  if (param.res_filename != nullptr) return param.res_filename;
  const std::string filename = param.filename;
  return filename + ".res";
}

std::ostream &operator<<(std::ostream &os, const DecodeParam &dp) {
  return os << "threads: " << dp.threads << " file: " << dp.filename
            << " result file: " << GetResFilename(dp);
}

class InvalidFileTest : public ::libaom_test::DecoderTest,
                        public ::libaom_test::CodecTestWithParam<DecodeParam> {
 protected:
  InvalidFileTest() : DecoderTest(GET_PARAM(0)), res_file_(nullptr) {}

  virtual ~InvalidFileTest() {
    if (res_file_ != nullptr) fclose(res_file_);
  }

  void OpenResFile(const std::string &res_file_name) {
    res_file_ = libaom_test::OpenTestDataFile(res_file_name);
    ASSERT_NE(res_file_, nullptr)
        << "Result file open failed. Filename: " << res_file_name;
  }

  virtual void DecompressedFrameHook(const aom_image_t &img,
                                     const unsigned int /*frame_number*/) {
    EXPECT_NE(img.fb_priv, nullptr);
  }

  virtual bool HandleDecodeResult(
      const aom_codec_err_t res_dec,
      const libaom_test::CompressedVideoSource &video,
      libaom_test::Decoder *decoder) {
    EXPECT_NE(res_file_, nullptr);
    int expected_res_dec = -1;

    // Read integer result.
    const int res = fscanf(res_file_, "%d", &expected_res_dec);
    EXPECT_NE(res, EOF) << "Read result data failed";

    if (expected_res_dec != -1) {
      // Check results match.
      const DecodeParam input = GET_PARAM(1);
      if (input.threads > 1) {
        // The serial decode check is too strict for tile-threaded decoding as
        // there is no guarantee on the decode order nor which specific error
        // will take precedence. Currently a tile-level error is not forwarded
        // so the frame will simply be marked corrupt.
        EXPECT_TRUE(res_dec == expected_res_dec ||
                    res_dec == AOM_CODEC_CORRUPT_FRAME)
            << "Results don't match: frame number = " << video.frame_number()
            << ". (" << decoder->DecodeError()
            << "). Expected: " << expected_res_dec << " or "
            << AOM_CODEC_CORRUPT_FRAME;
      } else {
        EXPECT_EQ(expected_res_dec, res_dec)
            << "Results don't match: frame number = " << video.frame_number()
            << ". (" << decoder->DecodeError() << ")";
      }
    }

    return !HasFailure();
  }

  virtual void HandlePeekResult(libaom_test::Decoder *const /*decoder*/,
                                libaom_test::CompressedVideoSource * /*video*/,
                                const aom_codec_err_t /*res_peek*/) {}

  void RunTest() {
    const DecodeParam input = GET_PARAM(1);
    aom_codec_dec_cfg_t cfg = { 0, 0, 0, !FORCE_HIGHBITDEPTH_DECODING };
    cfg.threads = input.threads;
    libaom_test::IVFVideoSource decode_video(input.filename);
    decode_video.Init();

    // The result file holds a list of expected integer results, one for each
    // decoded frame.  Any result that doesn't match the file's list will
    // cause a test failure.
    const std::string res_filename = GetResFilename(input);
    OpenResFile(res_filename);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&decode_video, cfg));
  }

 private:
  FILE *res_file_;
};

TEST_P(InvalidFileTest, ReturnCode) { RunTest(); }

// If res_filename (the third field) is nullptr, then the result filename is
// filename + ".res" by default. Set res_filename to a string if the result
// filename differs from the default.
const DecodeParam kAV1InvalidFileTests[] = {
  // { threads, filename, res_filename }
  { 1, "invalid-bug-1814.ivf", nullptr },
  { 1, "invalid-chromium-906381.ivf", nullptr },
  { 1, "invalid-google-142530197.ivf", nullptr },
  { 1, "invalid-google-142530197-1.ivf", nullptr },
  { 4, "invalid-oss-fuzz-9463.ivf", "invalid-oss-fuzz-9463.ivf.res.2" },
  { 1, "invalid-oss-fuzz-9720.ivf", nullptr },
  { 1, "invalid-oss-fuzz-10389.ivf", "invalid-oss-fuzz-10389.ivf.res.4" },
  { 1, "invalid-oss-fuzz-11523.ivf", "invalid-oss-fuzz-11523.ivf.res.2" },
  { 4, "invalid-oss-fuzz-15363.ivf", nullptr },
  { 1, "invalid-oss-fuzz-16437.ivf", "invalid-oss-fuzz-16437.ivf.res.2" },
  { 1, "invalid-oss-fuzz-24706.ivf", nullptr },
#if CONFIG_AV1_HIGHBITDEPTH
  // These test vectors contain 10-bit or 12-bit video.
  { 1, "invalid-oss-fuzz-9288.ivf", nullptr },
  { 1, "invalid-oss-fuzz-9482.ivf", nullptr },
  { 1, "invalid-oss-fuzz-10061.ivf", nullptr },
  { 1, "invalid-oss-fuzz-10117-mc-buf-use-highbd.ivf", nullptr },
  { 1, "invalid-oss-fuzz-10227.ivf", nullptr },
  { 4, "invalid-oss-fuzz-10555.ivf", nullptr },
  { 1, "invalid-oss-fuzz-10705.ivf", nullptr },
  { 1, "invalid-oss-fuzz-10723.ivf", "invalid-oss-fuzz-10723.ivf.res.2" },
  { 1, "invalid-oss-fuzz-10779.ivf", nullptr },
  { 1, "invalid-oss-fuzz-11477.ivf", nullptr },
  { 1, "invalid-oss-fuzz-11479.ivf", "invalid-oss-fuzz-11479.ivf.res.2" },
  { 1, "invalid-oss-fuzz-33030.ivf", nullptr },
#endif
};

AV1_INSTANTIATE_TEST_SUITE(InvalidFileTest,
                           ::testing::ValuesIn(kAV1InvalidFileTests));

}  // namespace
