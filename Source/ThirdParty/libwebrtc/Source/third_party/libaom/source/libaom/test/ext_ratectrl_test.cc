/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "aom/aom_ext_ratectrl.h"
#include "gtest/gtest.h"

namespace {

const int kFrameNum = 5;

// A mock rate control model.
struct MockRateCtrlModel {};

// A mock private data for rate control callbacks.
struct MockRC {};
struct MockRC g_priv;

// A flag to indicate if create_model() is called.
bool is_create_model_called = false;

// A flag to indicate if delete_model() is called.
bool is_delete_model_called = false;

// A flag to indicate if send_firstpass_stats() is called.
bool is_send_firstpass_stats_called = false;

// A flag to indicate if send_tpl_gop_stats() is called.
bool is_send_extrc_tpl_gop_stats_called = false;

// A flag to indicate if get_gop_decision() is called.
bool is_get_gop_decision_called = false;

// A flag to indicate if update_encodeframe_result() is called.
bool is_update_encodeframe_result_called = false;

// A flag to indicate if get_key_frame_decision() is called.
bool is_get_key_frame_decision_called = false;

// Variables to store the parameters passed to update_encodeframe_result().
int64_t bit_count = 0;
int actual_encoding_qindex = 0;

// Construct a single ALTREF GOP.
const int kGopFrameCount = kFrameNum + 1;
aom_rc_gop_frame_t gop_frame_list[kGopFrameCount];

aom_rc_status_t mock_get_key_frame_decision(
    aom_rc_model_t /*ratectrl_model*/,
    aom_rc_key_frame_decision_t *key_frame_decision) {
  key_frame_decision->key_frame_group_size = 1;
  is_get_key_frame_decision_called = true;
  return AOM_RC_OK;
}

aom_rc_status_t mock_get_gop_decision(aom_rc_model_t /*ratectrl_model*/,
                                      aom_rc_gop_decision_t *gop_decision) {
  gop_decision->gop_frame_count = kGopFrameCount;
  gop_decision->gop_frame_list = gop_frame_list;
  static const aom_rc_ref_name_t ref_names[] = {
    AOM_RC_LAST_FRAME,   AOM_RC_LAST2_FRAME,  AOM_RC_LAST3_FRAME,
    AOM_RC_GOLDEN_FRAME, AOM_RC_BWDREF_FRAME, AOM_RC_ALTREF2_FRAME,
    AOM_RC_ALTREF_FRAME
  };
  for (int i = 0; i < kGopFrameCount; ++i) {
    auto current_gop_frame = &gop_decision->gop_frame_list[i];
    current_gop_frame->coding_idx = i;
    // Set all references to buffer 0 (KF) by default.
    for (int j = 0; j < 7; ++j) {
      current_gop_frame->ref_frame_list.name[j] = ref_names[j];
      current_gop_frame->ref_frame_list.index[j] = 0;
    }
    current_gop_frame->update_ref_idx = -1;
    current_gop_frame->primary_ref_frame.name = AOM_RC_INVALID_REF_FRAME;
    current_gop_frame->primary_ref_frame.index = -1;

    if (i == 0) {
      // Key frame
      current_gop_frame->is_key_frame = true;
      current_gop_frame->update_type = AOM_RC_KF_UPDATE;
      current_gop_frame->layer_depth = 0;
      current_gop_frame->display_idx = 0;
      current_gop_frame->update_ref_idx = 0;
      current_gop_frame->order_idx = 0;
    } else {
      current_gop_frame->is_key_frame = false;
      if (i == 1) {
        // ALTREF
        current_gop_frame->update_type = AOM_RC_ARF_UPDATE;
        current_gop_frame->layer_depth = 1;
        current_gop_frame->display_idx = 1;
        current_gop_frame->update_ref_idx = 1;
        current_gop_frame->order_idx = 4;
      } else if (i == 5) {
        // Overlay frame
        current_gop_frame->is_key_frame = false;
        current_gop_frame->update_type = AOM_RC_OVERLAY_UPDATE;
        current_gop_frame->layer_depth = AOM_RC_MAX_ARF_LAYERS - 1;
        current_gop_frame->display_idx = 4;
        current_gop_frame->order_idx = 4;
      } else {
        // Leaf frames
        current_gop_frame->is_key_frame = false;
        current_gop_frame->update_type = AOM_RC_LF_UPDATE;
        current_gop_frame->layer_depth = AOM_RC_MAX_ARF_LAYERS - 1;
        current_gop_frame->display_idx = i - 1;  // Key in the front
        current_gop_frame->order_idx = i - 1;
        current_gop_frame->update_ref_idx = 2;
      }
      // For leaf and overlay frames, set ALTREF to buffer 1.
      if (i >= 2) {
        current_gop_frame->ref_frame_list.index[AOM_RC_ALTREF_FRAME] = 1;
      }
      // For leaf frames after the first leaf, use buffer 2 as LAST.
      if (i > 2 && i < 5) {
        current_gop_frame->ref_frame_list.index[AOM_RC_LAST_FRAME] = 2;
      }

      current_gop_frame->primary_ref_frame.name = AOM_RC_LAST_FRAME;
      current_gop_frame->primary_ref_frame.index = (i > 2 && i < 5) ? 2 : 0;
    }
  }
  gop_decision->global_order_idx_offset = 0;
  is_get_gop_decision_called = true;
  return AOM_RC_OK;
}

aom_rc_status_t mock_create_model(void *priv,
                                  const aom_rc_config_t *ratectrl_config,
                                  aom_rc_model_t *ratectrl_model) {
  (void)priv;
  (void)ratectrl_config;
  EXPECT_NE(ratectrl_model, nullptr);
  *ratectrl_model = (aom_rc_model_t)(new MockRateCtrlModel());
  is_create_model_called = true;
  return AOM_RC_OK;
}

aom_rc_status_t mock_delete_model(aom_rc_model_t ratectrl_model) {
  EXPECT_NE(ratectrl_model, nullptr);
  delete (MockRateCtrlModel *)ratectrl_model;
  is_delete_model_called = true;
  return AOM_RC_OK;
}

aom_rc_status_t mock_send_firstpass_stats(
    aom_rc_model_t ratectrl_model,
    const aom_rc_firstpass_stats_t *firstpass_stats) {
  EXPECT_NE(ratectrl_model, nullptr);
  EXPECT_NE(firstpass_stats, nullptr);
  EXPECT_EQ(firstpass_stats->num_frames, kFrameNum);
  EXPECT_NE(firstpass_stats->frame_stats, nullptr);
  is_send_firstpass_stats_called = true;
  return AOM_RC_OK;
}

aom_rc_status_t mock_send_extrc_tpl_gop_stats(
    aom_rc_model_t ratectrl_model, const AomTplGopStats *extrc_tpl_gop_stats) {
  EXPECT_NE(ratectrl_model, nullptr);
  EXPECT_NE(extrc_tpl_gop_stats, nullptr);
  EXPECT_GT(extrc_tpl_gop_stats->size, 0);
  EXPECT_NE(extrc_tpl_gop_stats->frame_stats_list, nullptr);
  is_send_extrc_tpl_gop_stats_called = true;
  return AOM_RC_OK;
}

aom_rc_status_t mock_update_encodeframe_result(
    aom_rc_model_t /* ratectrl_model */,
    const aom_rc_encodeframe_result_t *encode_frame_result) {
  EXPECT_NE(encode_frame_result, nullptr);
  bit_count = encode_frame_result->bit_count;
  actual_encoding_qindex = encode_frame_result->actual_encoding_qindex;
  is_update_encodeframe_result_called = true;
  return AOM_RC_OK;
}

class ExtRateCtrlTest : public ::libaom_test::EncoderTest,
                        public ::libaom_test::CodecTestWith2Params<int, int> {
 protected:
  ExtRateCtrlTest() : EncoderTest(GET_PARAM(0)), cpu_used_(GET_PARAM(2)) {
    aom_rc_funcs_t *rc_funcs = &rc_funcs_;
    rc_funcs->priv = &g_priv;
    rc_funcs->rc_type = AOM_RC_QP;
    rc_funcs->create_model = mock_create_model;
    rc_funcs->delete_model = mock_delete_model;
    rc_funcs->send_firstpass_stats = mock_send_firstpass_stats;
    rc_funcs->send_tpl_gop_stats = mock_send_extrc_tpl_gop_stats;
    rc_funcs->get_gop_decision = nullptr;
    rc_funcs->get_key_frame_decision = nullptr;
    rc_funcs->get_encodeframe_decision = nullptr;
    rc_funcs->update_encodeframe_result = nullptr;
  }
  ~ExtRateCtrlTest() override = default;

  void SetUp() override {
    InitializeConfig(static_cast<libaom_test::TestMode>(GET_PARAM(1)));
    cfg_.g_threads = 1;
    cfg_.g_limit = kFrameNum;
    is_create_model_called = false;
    is_delete_model_called = false;
    is_send_firstpass_stats_called = false;
    is_send_extrc_tpl_gop_stats_called = false;
    is_get_gop_decision_called = false;
    is_update_encodeframe_result_called = false;
    is_get_key_frame_decision_called = false;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_EXTERNAL_RATE_CONTROL, &rc_funcs_);
    }
    current_encoder_ = encoder;
  }

  ::libaom_test::Encoder *current_encoder_;
  aom_rc_funcs_t rc_funcs_;
  int cpu_used_;
};

TEST_P(ExtRateCtrlTest, TestExternalRateCtrl) {
  ::libaom_test::Y4mVideoSource video("screendata.y4m", 0, kFrameNum);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  EXPECT_TRUE(is_create_model_called);
  EXPECT_TRUE(is_send_firstpass_stats_called);
  EXPECT_TRUE(is_send_extrc_tpl_gop_stats_called);
  EXPECT_TRUE(is_delete_model_called);
}

AV1_INSTANTIATE_TEST_SUITE(ExtRateCtrlTest,
                           ::testing::Values(::libaom_test::kTwoPassGood),
                           ::testing::Values(3));

// Corresponds to QP 43 for 8-bit video.
const int kFrameQindex = 172;

aom_rc_status_t mock_get_encodeframe_decision_const_q(
    aom_rc_model_t /*ratectrl_model*/, int /*frame_gop_index*/,
    aom_rc_encodeframe_decision_t *frame_decision) {
  // Set constant QP.
  frame_decision->q_index = kFrameQindex;
  frame_decision->sb_params_list = NULL;  // Initialize to NULL
  return AOM_RC_OK;
}

class ExtRateCtrlQpTest : public ExtRateCtrlTest {
 protected:
  ExtRateCtrlQpTest() {
    rc_funcs_.get_encodeframe_decision = mock_get_encodeframe_decision_const_q;
  }
  ~ExtRateCtrlQpTest() override = default;

  void SetUp() override {
    InitializeConfig(static_cast<libaom_test::TestMode>(GET_PARAM(1)));
    cfg_.g_threads = 1;
    cfg_.g_limit = kFrameNum;
    is_create_model_called = false;
    is_delete_model_called = false;
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) return;
    int q_index = -1;
    current_encoder_->Control(AOME_GET_LAST_QUANTIZER, &q_index);
    EXPECT_EQ(q_index, kFrameQindex);
  }
};

TEST_P(ExtRateCtrlQpTest, TestExternalRateCtrlConstQp) {
  ::libaom_test::Y4mVideoSource video("screendata.y4m", 0, kFrameNum);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  EXPECT_TRUE(is_create_model_called);
  EXPECT_TRUE(is_delete_model_called);
}

AV1_INSTANTIATE_TEST_SUITE(ExtRateCtrlQpTest,
                           ::testing::Values(::libaom_test::kTwoPassGood),
                           ::testing::Values(3));

class ExtRateCtrlUpdateEncodeFrameResultTest : public ExtRateCtrlTest {
 protected:
  ExtRateCtrlUpdateEncodeFrameResultTest() {
    rc_funcs_.rc_type = AOM_RC_QP;
    rc_funcs_.get_encodeframe_decision = mock_get_encodeframe_decision_const_q;
    rc_funcs_.update_encodeframe_result = mock_update_encodeframe_result;
  }
  ~ExtRateCtrlUpdateEncodeFrameResultTest() override = default;

  void SetUp() override {
    ExtRateCtrlTest::SetUp();
    is_update_encodeframe_result_called = false;
    bit_count = 0;
    actual_encoding_qindex = 0;
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) return;
    int q_index = -1;
    current_encoder_->Control(AOME_GET_LAST_QUANTIZER, &q_index);
    EXPECT_EQ(q_index, kFrameQindex);
    EXPECT_EQ(actual_encoding_qindex, kFrameQindex);
    // The second packet includes both invisible and visible frames. To verify
    // frame size, there's no move_offset (obu_header_size + obu_payload_size)
    // for vis_frame_size.
    const int pkt_size_padding = pkt_count_ == 1 ? 0 : 2;
    // Due to av1 packing invisible and visible frames together, we can only
    // verify the size of the visible frame in the packet using vis_frame_size.
    EXPECT_EQ((bit_count >> 3) + pkt_size_padding,
              pkt->data.frame.vis_frame_size);
    pkt_count_++;
  }

 private:
  int pkt_count_ = 0;
};

TEST_P(ExtRateCtrlUpdateEncodeFrameResultTest,
       TestExternalRateCtrlUpdateEncodeFrameResult) {
  ::libaom_test::Y4mVideoSource video("screendata.y4m", 0, kFrameNum);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  EXPECT_TRUE(is_create_model_called);
  EXPECT_TRUE(is_delete_model_called);
  EXPECT_TRUE(is_update_encodeframe_result_called);
}

AV1_INSTANTIATE_TEST_SUITE(ExtRateCtrlUpdateEncodeFrameResultTest,
                           ::testing::Values(::libaom_test::kTwoPassGood),
                           ::testing::Values(3));

class ExtRateCtrlGopTest : public ExtRateCtrlTest {
 protected:
  ExtRateCtrlGopTest() {
    rc_funcs_.rc_type = AOM_RC_GOP;
    rc_funcs_.get_gop_decision = mock_get_gop_decision;
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    if (cfg_.g_pass == AOM_RC_FIRST_PASS) return;
    encoder->Control(AV1E_GET_GOP_INFO, &gop_info_);
  }

  void FramePktHook(const aom_codec_cx_pkt_t * /*pkt*/) override {
    // This is verified here (not in PostEncodeFrameHook) because
    // PostEncodeFrameHook is also called when encoder is reading frames into
    // lookahead buffer, when GOP structure hasn't been determined.
    ASSERT_EQ(gop_info_.gop_size, kGopFrameCount);
  }

  ~ExtRateCtrlGopTest() override = default;
  aom_gop_info_t gop_info_;
};

TEST_P(ExtRateCtrlGopTest, TestExternalRateCtrlGop) {
  ::libaom_test::Y4mVideoSource video("screendata.y4m", 0, kFrameNum);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  EXPECT_TRUE(is_create_model_called);
  EXPECT_TRUE(is_get_gop_decision_called);
  EXPECT_TRUE(is_delete_model_called);
}

AV1_INSTANTIATE_TEST_SUITE(ExtRateCtrlGopTest,
                           ::testing::Values(::libaom_test::kTwoPassGood),
                           ::testing::Values(3));

class ExtRateCtrlKeyFrameTest : public ExtRateCtrlTest {
 protected:
  ExtRateCtrlKeyFrameTest() {
    rc_funcs_.rc_type = AOM_RC_GOP;
    rc_funcs_.get_key_frame_decision = mock_get_key_frame_decision;
  }

  ~ExtRateCtrlKeyFrameTest() override = default;

  void SetUp() override {
    ExtRateCtrlTest::SetUp();
    is_get_key_frame_decision_called = false;
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) return;
    EXPECT_TRUE(pkt->data.frame.flags & AOM_FRAME_IS_KEY);
  }
};

TEST_P(ExtRateCtrlKeyFrameTest, TestExternalRateCtrlKeyFrame) {
  ::libaom_test::Y4mVideoSource video("screendata.y4m", 0, kFrameNum);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  EXPECT_TRUE(is_create_model_called);
  EXPECT_TRUE(is_get_key_frame_decision_called);
  EXPECT_TRUE(is_delete_model_called);
}

AV1_INSTANTIATE_TEST_SUITE(ExtRateCtrlKeyFrameTest,
                           ::testing::Values(::libaom_test::kTwoPassGood),
                           ::testing::Values(3));
}  // namespace
