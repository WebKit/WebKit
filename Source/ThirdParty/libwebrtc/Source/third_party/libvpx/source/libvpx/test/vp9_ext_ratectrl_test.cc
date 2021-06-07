/*
 *  Copyright (c) 2020 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <new>

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/yuv_video_source.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "vpx/vpx_ext_ratectrl.h"

namespace {

constexpr int kModelMagicNumber = 51396;
constexpr unsigned int PrivMagicNumber = 5566;
constexpr int kFrameNum = 5;
constexpr int kLosslessCodingIndex = 2;

struct ToyRateCtrl {
  int magic_number;
  int coding_index;
};

vpx_rc_status_t rc_create_model(void *priv,
                                const vpx_rc_config_t *ratectrl_config,
                                vpx_rc_model_t *rate_ctrl_model_pt) {
  ToyRateCtrl *toy_rate_ctrl = new (std::nothrow) ToyRateCtrl;
  EXPECT_NE(toy_rate_ctrl, nullptr);
  toy_rate_ctrl->magic_number = kModelMagicNumber;
  toy_rate_ctrl->coding_index = -1;
  *rate_ctrl_model_pt = toy_rate_ctrl;
  EXPECT_EQ(priv, reinterpret_cast<void *>(PrivMagicNumber));
  EXPECT_EQ(ratectrl_config->frame_width, 352);
  EXPECT_EQ(ratectrl_config->frame_height, 288);
  EXPECT_EQ(ratectrl_config->show_frame_count, kFrameNum);
  EXPECT_EQ(ratectrl_config->target_bitrate_kbps, 24000);
  EXPECT_EQ(ratectrl_config->frame_rate_num, 30);
  EXPECT_EQ(ratectrl_config->frame_rate_den, 1);
  return VPX_RC_OK;
}

vpx_rc_status_t rc_send_firstpass_stats(
    vpx_rc_model_t rate_ctrl_model,
    const vpx_rc_firstpass_stats_t *first_pass_stats) {
  const ToyRateCtrl *toy_rate_ctrl =
      static_cast<ToyRateCtrl *>(rate_ctrl_model);
  EXPECT_EQ(toy_rate_ctrl->magic_number, kModelMagicNumber);
  EXPECT_EQ(first_pass_stats->num_frames, kFrameNum);
  for (int i = 0; i < first_pass_stats->num_frames; ++i) {
    EXPECT_DOUBLE_EQ(first_pass_stats->frame_stats[i].frame, i);
  }
  return VPX_RC_OK;
}

vpx_rc_status_t rc_get_encodeframe_decision(
    vpx_rc_model_t rate_ctrl_model,
    const vpx_rc_encodeframe_info_t *encode_frame_info,
    vpx_rc_encodeframe_decision_t *frame_decision) {
  ToyRateCtrl *toy_rate_ctrl = static_cast<ToyRateCtrl *>(rate_ctrl_model);
  toy_rate_ctrl->coding_index += 1;

  EXPECT_EQ(toy_rate_ctrl->magic_number, kModelMagicNumber);

  EXPECT_LT(encode_frame_info->show_index, kFrameNum);
  EXPECT_EQ(encode_frame_info->coding_index, toy_rate_ctrl->coding_index);

  if (encode_frame_info->coding_index == 0) {
    EXPECT_EQ(encode_frame_info->show_index, 0);
    EXPECT_EQ(encode_frame_info->gop_index, 0);
    EXPECT_EQ(encode_frame_info->frame_type, 0 /*kFrameTypeKey*/);
    EXPECT_EQ(encode_frame_info->ref_frame_valid_list[0],
              0);  // kRefFrameTypeLast
    EXPECT_EQ(encode_frame_info->ref_frame_valid_list[1],
              0);  // kRefFrameTypePast
    EXPECT_EQ(encode_frame_info->ref_frame_valid_list[2],
              0);  // kRefFrameTypeFuture
  }

  if (encode_frame_info->coding_index == 1) {
    EXPECT_EQ(encode_frame_info->show_index, 4);
    EXPECT_EQ(encode_frame_info->gop_index, 1);
    EXPECT_EQ(encode_frame_info->frame_type, 2 /*kFrameTypeAltRef*/);
    EXPECT_EQ(encode_frame_info->ref_frame_valid_list[0],
              1);  // kRefFrameTypeLast
    EXPECT_EQ(encode_frame_info->ref_frame_valid_list[1],
              0);  // kRefFrameTypePast
    EXPECT_EQ(encode_frame_info->ref_frame_valid_list[2],
              0);  // kRefFrameTypeFuture
    EXPECT_EQ(encode_frame_info->ref_frame_coding_indexes[0],
              0);  // kRefFrameTypeLast
  }

  if (encode_frame_info->coding_index >= 2 &&
      encode_frame_info->coding_index < 5) {
    // In the first group of pictures, coding_index and gop_index are equal.
    EXPECT_EQ(encode_frame_info->gop_index, encode_frame_info->coding_index);
    EXPECT_EQ(encode_frame_info->frame_type, 1 /*kFrameTypeInter*/);
  }

  if (encode_frame_info->coding_index == 5) {
    EXPECT_EQ(encode_frame_info->show_index, 4);
    EXPECT_EQ(encode_frame_info->gop_index, 0);
    EXPECT_EQ(encode_frame_info->frame_type, 3 /*kFrameTypeOverlay*/);
    EXPECT_EQ(encode_frame_info->ref_frame_valid_list[0],
              1);  // kRefFrameTypeLast
    EXPECT_EQ(encode_frame_info->ref_frame_valid_list[1],
              1);  // kRefFrameTypePast
    EXPECT_EQ(encode_frame_info->ref_frame_valid_list[2],
              1);  // kRefFrameTypeFuture
    EXPECT_EQ(encode_frame_info->ref_frame_coding_indexes[0],
              4);  // kRefFrameTypeLast
    EXPECT_EQ(encode_frame_info->ref_frame_coding_indexes[1],
              0);  // kRefFrameTypePast
    EXPECT_EQ(encode_frame_info->ref_frame_coding_indexes[2],
              1);  // kRefFrameTypeFuture
  }
  if (encode_frame_info->coding_index == kLosslessCodingIndex) {
    // We should get sse == 0 at rc_update_encodeframe_result()
    frame_decision->q_index = 0;
  } else {
    frame_decision->q_index = 100;
  }
  return VPX_RC_OK;
}

vpx_rc_status_t rc_update_encodeframe_result(
    vpx_rc_model_t rate_ctrl_model,
    const vpx_rc_encodeframe_result_t *encode_frame_result) {
  const ToyRateCtrl *toy_rate_ctrl =
      static_cast<ToyRateCtrl *>(rate_ctrl_model);
  EXPECT_EQ(toy_rate_ctrl->magic_number, kModelMagicNumber);

  const int64_t ref_pixel_count = 352 * 288 * 3 / 2;
  EXPECT_EQ(encode_frame_result->pixel_count, ref_pixel_count);
  if (toy_rate_ctrl->coding_index == kLosslessCodingIndex) {
    EXPECT_EQ(encode_frame_result->sse, 0);
  }
  return VPX_RC_OK;
}

vpx_rc_status_t rc_delete_model(vpx_rc_model_t rate_ctrl_model) {
  ToyRateCtrl *toy_rate_ctrl = static_cast<ToyRateCtrl *>(rate_ctrl_model);
  EXPECT_EQ(toy_rate_ctrl->magic_number, kModelMagicNumber);
  delete toy_rate_ctrl;
  return VPX_RC_OK;
}

class ExtRateCtrlTest : public ::libvpx_test::EncoderTest,
                        public ::testing::Test {
 protected:
  ExtRateCtrlTest() : EncoderTest(&::libvpx_test::kVP9) {}

  ~ExtRateCtrlTest() override = default;

  void SetUp() override {
    InitializeConfig();
    SetMode(::libvpx_test::kTwoPassGood);
  }

  void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                          ::libvpx_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      vpx_rc_funcs_t rc_funcs;
      rc_funcs.create_model = rc_create_model;
      rc_funcs.send_firstpass_stats = rc_send_firstpass_stats;
      rc_funcs.get_encodeframe_decision = rc_get_encodeframe_decision;
      rc_funcs.update_encodeframe_result = rc_update_encodeframe_result;
      rc_funcs.delete_model = rc_delete_model;
      rc_funcs.priv = reinterpret_cast<void *>(PrivMagicNumber);
      encoder->Control(VP9E_SET_EXTERNAL_RATE_CONTROL, &rc_funcs);
    }
  }
};

TEST_F(ExtRateCtrlTest, EncodeTest) {
  cfg_.rc_target_bitrate = 24000;

  std::unique_ptr<libvpx_test::VideoSource> video;
  video.reset(new (std::nothrow) libvpx_test::YUVVideoSource(
      "bus_352x288_420_f20_b8.yuv", VPX_IMG_FMT_I420, 352, 288, 30, 1, 0,
      kFrameNum));

  ASSERT_NE(video.get(), nullptr);
  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
}

}  // namespace
