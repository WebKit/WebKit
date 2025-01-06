/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstring>
#include <tuple>

#include "aom/aom_codec.h"
#include "aom/aom_decoder.h"
#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "aom/aomdx.h"
#include "config/aom_config.h"
#include "gtest/gtest.h"

namespace {
typedef std::tuple<const char *, const char *> KeyValParam;

class BaseKeyValAPI : public testing::Test {
 public:
  void SetUp() override {
#if CONFIG_AV1_ENCODER
    aom_codec_iface_t *iface_cx = aom_codec_av1_cx();
    aom_codec_enc_cfg_t enc_cfg;
#if CONFIG_REALTIME_ONLY
    const int usage = 1;
#else
    const int usage = 0;
#endif
    EXPECT_EQ(AOM_CODEC_OK,
              aom_codec_enc_config_default(iface_cx, &enc_cfg, usage));
    EXPECT_EQ(AOM_CODEC_OK,
              aom_codec_enc_init(&enc_, iface_cx, &enc_cfg, usage));
#endif
#if CONFIG_AV1_DECODER
    aom_codec_iface_t *iface_dx = aom_codec_av1_dx();
    aom_codec_dec_cfg_t dec_cfg = { 0, 0, 0, !FORCE_HIGHBITDEPTH_DECODING };

    EXPECT_EQ(AOM_CODEC_OK, aom_codec_dec_init(&dec_, iface_dx, &dec_cfg, 0));
#endif
  }

  void TearDown() override {
#if CONFIG_AV1_ENCODER
    EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc_));
#endif
#if CONFIG_AV1_DECODER
    EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&dec_));
#endif
  }

 protected:
#if CONFIG_AV1_ENCODER
  aom_codec_ctx_t enc_;
#endif
#if CONFIG_AV1_DECODER
  aom_codec_ctx_t dec_;
#endif
};

// Tests on encoder options.
// Need to add ones for the decoder in the future if it is also supported in the
// key & value API.
#if CONFIG_AV1_ENCODER
class EncValidTest : public BaseKeyValAPI,
                     public testing::WithParamInterface<KeyValParam> {};
class EncInvalidTest : public BaseKeyValAPI,
                       public testing::WithParamInterface<KeyValParam> {};

TEST_P(EncValidTest, Valid) {
  const char *key = std::get<0>(GetParam());
  const char *val = std::get<1>(GetParam());
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_set_option(&enc_, key, val));
}

TEST_P(EncInvalidTest, NullArg) {
  const char *key = std::get<0>(GetParam());
  const char *val = std::get<1>(GetParam());
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_set_option(nullptr, key, val));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_set_option(&enc_, nullptr, val));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_set_option(&enc_, key, nullptr));
}

TEST_P(EncInvalidTest, InvalidParam) {
  const char *key = std::get<0>(GetParam());
  const char *val = std::get<1>(GetParam());
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_set_option(&enc_, key, val));
  ASSERT_NE(aom_codec_error_detail(&enc_), nullptr);
  EXPECT_GT(strlen(aom_codec_error_detail(&enc_)), 0u);
}

// No test for ratio / list for now since the API does not support any of the
// parameters of these type.
// The string type typically involves reading a path/file, which brings
// potential fails.
const KeyValParam enc_valid_params[] = {
  std::make_tuple("auto-intra-tools-off", "1"),  // uint
  std::make_tuple("min-gf-interval", "10"),      // uint
  std::make_tuple("min-partition-size", "4"),    // int
  std::make_tuple("tune", "psnr"),               // enum
};

const KeyValParam enc_invalid_params[] = {
  // no match
  std::make_tuple("a-b-c", "10"),
  // uint
  std::make_tuple("min-gf-interval", "-1"),
  std::make_tuple("min-gf-interval", "1.1"),
  std::make_tuple("min-gf-interval", "abc"),
  // int
  std::make_tuple("min-partition-size", "1.1"),
  std::make_tuple("min-partition-size", "abc"),
  // enum
  std::make_tuple("tune", "PsnR1"),
  // out of range
  std::make_tuple("cq-level", "1000"),
};

INSTANTIATE_TEST_SUITE_P(KeyValAPI, EncValidTest,
                         testing::ValuesIn(enc_valid_params));

INSTANTIATE_TEST_SUITE_P(KeyValAPI, EncInvalidTest,
                         testing::ValuesIn(enc_invalid_params));
#endif  // CONFIG_AV1_ENCODER

}  // namespace
