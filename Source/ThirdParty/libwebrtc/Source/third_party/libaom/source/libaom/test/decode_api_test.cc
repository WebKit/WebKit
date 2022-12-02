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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"

#include "aom/aomdx.h"
#include "aom/aom_decoder.h"

namespace {

TEST(DecodeAPI, InvalidParams) {
  uint8_t buf[1] = { 0 };
  aom_codec_ctx_t dec;

  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_dec_init(nullptr, nullptr, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_dec_init(&dec, nullptr, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_decode(nullptr, nullptr, 0, nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_decode(nullptr, buf, 0, nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_decode(nullptr, buf, sizeof(buf), nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_decode(nullptr, nullptr, sizeof(buf), nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_destroy(nullptr));
  EXPECT_NE(aom_codec_error(nullptr), nullptr);
  EXPECT_EQ(aom_codec_error_detail(nullptr), nullptr);

  aom_codec_iface_t *iface = aom_codec_av1_dx();
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_dec_init(nullptr, iface, nullptr, 0));

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_dec_init(&dec, iface, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_decode(&dec, nullptr, sizeof(buf), nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_decode(&dec, buf, 0, nullptr));

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&dec));
}

TEST(DecodeAPI, InvalidControlId) {
  aom_codec_iface_t *iface = aom_codec_av1_dx();
  aom_codec_ctx_t dec;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_dec_init(&dec, iface, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_ERROR, aom_codec_control(&dec, -1, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_control(&dec, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&dec));
}

}  // namespace
