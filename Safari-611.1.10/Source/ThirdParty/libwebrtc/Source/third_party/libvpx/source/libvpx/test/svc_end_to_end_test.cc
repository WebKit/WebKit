/*
 *  Copyright (c) 2018 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./vpx_config.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/svc_test.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "vpx/vpx_codec.h"
#include "vpx_ports/bitops.h"

namespace svc_test {
namespace {

typedef enum {
  // Inter-layer prediction is on on all frames.
  INTER_LAYER_PRED_ON,
  // Inter-layer prediction is off on all frames.
  INTER_LAYER_PRED_OFF,
  // Inter-layer prediction is off on non-key frames and non-sync frames.
  INTER_LAYER_PRED_OFF_NONKEY,
  // Inter-layer prediction is on on all frames, but constrained such
  // that any layer S (> 0) can only predict from previous spatial
  // layer S-1, from the same superframe.
  INTER_LAYER_PRED_ON_CONSTRAINED
} INTER_LAYER_PRED;

class ScalePartitionOnePassCbrSvc
    : public OnePassCbrSvc,
      public ::testing::TestWithParam<const ::libvpx_test::CodecFactory *> {
 public:
  ScalePartitionOnePassCbrSvc()
      : OnePassCbrSvc(GetParam()), mismatch_nframes_(0), num_nonref_frames_(0) {
    SetMode(::libvpx_test::kRealTime);
  }

 protected:
  virtual ~ScalePartitionOnePassCbrSvc() {}

  virtual void SetUp() {
    InitializeConfig();
    speed_setting_ = 7;
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    PreEncodeFrameHookSetup(video, encoder);
  }

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    // Keep track of number of non-reference frames, needed for mismatch check.
    // Non-reference frames are top spatial and temporal layer frames,
    // for TL > 0.
    if (temporal_layer_id_ == number_temporal_layers_ - 1 &&
        temporal_layer_id_ > 0 &&
        pkt->data.frame.spatial_layer_encoded[number_spatial_layers_ - 1])
      num_nonref_frames_++;
  }

  virtual void MismatchHook(const vpx_image_t * /*img1*/,
                            const vpx_image_t * /*img2*/) {
    ++mismatch_nframes_;
  }

  virtual void SetConfig(const int /*num_temporal_layer*/) {}

  unsigned int GetMismatchFrames() const { return mismatch_nframes_; }
  unsigned int GetNonRefFrames() const { return num_nonref_frames_; }

 private:
  unsigned int mismatch_nframes_;
  unsigned int num_nonref_frames_;
};

TEST_P(ScalePartitionOnePassCbrSvc, OnePassCbrSvc3SL3TL1080P) {
  SetSvcConfig(3, 3);
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.g_threads = 1;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.rc_target_bitrate = 800;
  cfg_.kf_max_dist = 9999;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.g_error_resilient = 1;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.temporal_layering_mode = 3;
  ::libvpx_test::I420VideoSource video(
      "slides_code_term_web_plot.1920_1080.yuv", 1920, 1080, 30, 1, 0, 100);
  // For this 3 temporal layer case, pattern repeats every 4 frames, so choose
  // 4 key neighboring key frame periods (so key frame will land on 0-2-1-2).
  AssignLayerBitrates();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#if CONFIG_VP9_DECODER
  // The non-reference frames are expected to be mismatched frames as the
  // encoder will avoid loopfilter on these frames.
  EXPECT_EQ(GetNonRefFrames(), GetMismatchFrames());
#endif
}

// Params: Inter layer prediction modes.
class SyncFrameOnePassCbrSvc : public OnePassCbrSvc,
                               public ::libvpx_test::CodecTestWithParam<int> {
 public:
  SyncFrameOnePassCbrSvc()
      : OnePassCbrSvc(GET_PARAM(0)), current_video_frame_(0),
        frame_to_start_decode_(0), frame_to_sync_(0),
        inter_layer_pred_mode_(GET_PARAM(1)), decode_to_layer_before_sync_(-1),
        decode_to_layer_after_sync_(-1), denoiser_on_(0),
        intra_only_test_(false), mismatch_nframes_(0), num_nonref_frames_(0) {
    SetMode(::libvpx_test::kRealTime);
    memset(&svc_layer_sync_, 0, sizeof(svc_layer_sync_));
  }

 protected:
  virtual ~SyncFrameOnePassCbrSvc() {}

  virtual void SetUp() {
    InitializeConfig();
    speed_setting_ = 7;
  }

  virtual bool DoDecode() const {
    return current_video_frame_ >= frame_to_start_decode_;
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    current_video_frame_ = video->frame();
    PreEncodeFrameHookSetup(video, encoder);
    if (video->frame() == 0) {
      // Do not turn off inter-layer pred completely because simulcast mode
      // fails.
      if (inter_layer_pred_mode_ != INTER_LAYER_PRED_OFF)
        encoder->Control(VP9E_SET_SVC_INTER_LAYER_PRED, inter_layer_pred_mode_);
      encoder->Control(VP9E_SET_NOISE_SENSITIVITY, denoiser_on_);
      if (intra_only_test_)
        // Decoder sets the color_space for Intra-only frames
        // to BT_601 (see line 1810 in vp9_decodeframe.c).
        // So set it here in these tess to avoid encoder-decoder
        // mismatch check on color space setting.
        encoder->Control(VP9E_SET_COLOR_SPACE, VPX_CS_BT_601);
    }
    if (video->frame() == frame_to_sync_) {
      encoder->Control(VP9E_SET_SVC_SPATIAL_LAYER_SYNC, &svc_layer_sync_);
    }
  }

#if CONFIG_VP9_DECODER
  virtual void PreDecodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Decoder *decoder) {
    if (video->frame() < frame_to_sync_) {
      if (decode_to_layer_before_sync_ >= 0)
        decoder->Control(VP9_DECODE_SVC_SPATIAL_LAYER,
                         decode_to_layer_before_sync_);
    } else {
      if (decode_to_layer_after_sync_ >= 0)
        decoder->Control(VP9_DECODE_SVC_SPATIAL_LAYER,
                         decode_to_layer_after_sync_);
    }
  }
#endif

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    // Keep track of number of non-reference frames, needed for mismatch check.
    // Non-reference frames are top spatial and temporal layer frames,
    // for TL > 0.
    if (temporal_layer_id_ == number_temporal_layers_ - 1 &&
        temporal_layer_id_ > 0 &&
        pkt->data.frame.spatial_layer_encoded[number_spatial_layers_ - 1] &&
        current_video_frame_ >= frame_to_sync_)
      num_nonref_frames_++;

    if (intra_only_test_ && current_video_frame_ == frame_to_sync_) {
      // Intra-only frame is only generated for spatial layers > 1 and <= 3,
      // among other conditions (see constraint in set_intra_only_frame(). If
      // intra-only is no allowed then encoder will insert key frame instead.
      const bool key_frame =
          (pkt->data.frame.flags & VPX_FRAME_IS_KEY) ? true : false;
      if (number_spatial_layers_ == 1 || number_spatial_layers_ > 3)
        ASSERT_TRUE(key_frame);
      else
        ASSERT_FALSE(key_frame);
    }
  }

  virtual void MismatchHook(const vpx_image_t * /*img1*/,
                            const vpx_image_t * /*img2*/) {
    if (current_video_frame_ >= frame_to_sync_) ++mismatch_nframes_;
  }

  unsigned int GetMismatchFrames() const { return mismatch_nframes_; }
  unsigned int GetNonRefFrames() const { return num_nonref_frames_; }

  unsigned int current_video_frame_;
  unsigned int frame_to_start_decode_;
  unsigned int frame_to_sync_;
  int inter_layer_pred_mode_;
  int decode_to_layer_before_sync_;
  int decode_to_layer_after_sync_;
  int denoiser_on_;
  bool intra_only_test_;
  vpx_svc_spatial_layer_sync_t svc_layer_sync_;

 private:
  virtual void SetConfig(const int num_temporal_layer) {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = VPX_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 1;
    cfg_.g_threads = 1;
    cfg_.rc_dropframe_thresh = 30;
    cfg_.kf_max_dist = 9999;
    if (num_temporal_layer == 3) {
      cfg_.ts_rate_decimator[0] = 4;
      cfg_.ts_rate_decimator[1] = 2;
      cfg_.ts_rate_decimator[2] = 1;
      cfg_.temporal_layering_mode = 3;
    } else if (num_temporal_layer == 2) {
      cfg_.ts_rate_decimator[0] = 2;
      cfg_.ts_rate_decimator[1] = 1;
      cfg_.temporal_layering_mode = 2;
    } else if (num_temporal_layer == 1) {
      cfg_.ts_rate_decimator[0] = 1;
      cfg_.temporal_layering_mode = 1;
    }
  }

  unsigned int mismatch_nframes_;
  unsigned int num_nonref_frames_;
};

// Test for sync layer for 1 pass CBR SVC: 3 spatial layers and
// 3 temporal layers. Only start decoding on the sync layer.
// Full sync: insert key frame on base layer.
TEST_P(SyncFrameOnePassCbrSvc, OnePassCbrSvc3SL3TLFullSync) {
  SetSvcConfig(3, 3);
  // Sync is on base layer so the frame to sync and the frame to start decoding
  // is the same.
  frame_to_start_decode_ = 20;
  frame_to_sync_ = 20;
  decode_to_layer_before_sync_ = -1;
  decode_to_layer_after_sync_ = 2;

  // Set up svc layer sync structure.
  svc_layer_sync_.base_layer_intra_only = 0;
  svc_layer_sync_.spatial_layer_sync[0] = 1;

  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);

  cfg_.rc_target_bitrate = 600;
  AssignLayerBitrates();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#if CONFIG_VP9_DECODER
  // The non-reference frames are expected to be mismatched frames as the
  // encoder will avoid loopfilter on these frames.
  EXPECT_EQ(GetNonRefFrames(), GetMismatchFrames());
#endif
}

// Test for sync layer for 1 pass CBR SVC: 2 spatial layers and
// 3 temporal layers. Decoding QVGA before sync frame and decode up to
// VGA on and after sync.
TEST_P(SyncFrameOnePassCbrSvc, OnePassCbrSvc2SL3TLSyncToVGA) {
  SetSvcConfig(2, 3);
  frame_to_start_decode_ = 0;
  frame_to_sync_ = 100;
  decode_to_layer_before_sync_ = 0;
  decode_to_layer_after_sync_ = 1;

  // Set up svc layer sync structure.
  svc_layer_sync_.base_layer_intra_only = 0;
  svc_layer_sync_.spatial_layer_sync[0] = 0;
  svc_layer_sync_.spatial_layer_sync[1] = 1;

  ::libvpx_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  cfg_.rc_target_bitrate = 400;
  AssignLayerBitrates();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#if CONFIG_VP9_DECODER
  // The non-reference frames are expected to be mismatched frames as the
  // encoder will avoid loopfilter on these frames.
  EXPECT_EQ(GetNonRefFrames(), GetMismatchFrames());
#endif
}

// Test for sync layer for 1 pass CBR SVC: 3 spatial layers and
// 3 temporal layers. Decoding QVGA and VGA before sync frame and decode up to
// HD on and after sync.
TEST_P(SyncFrameOnePassCbrSvc, OnePassCbrSvc3SL3TLSyncToHD) {
  SetSvcConfig(3, 3);
  frame_to_start_decode_ = 0;
  frame_to_sync_ = 20;
  decode_to_layer_before_sync_ = 1;
  decode_to_layer_after_sync_ = 2;

  // Set up svc layer sync structure.
  svc_layer_sync_.base_layer_intra_only = 0;
  svc_layer_sync_.spatial_layer_sync[0] = 0;
  svc_layer_sync_.spatial_layer_sync[1] = 0;
  svc_layer_sync_.spatial_layer_sync[2] = 1;

  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  cfg_.rc_target_bitrate = 600;
  AssignLayerBitrates();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#if CONFIG_VP9_DECODER
  // The non-reference frames are expected to be mismatched frames as the
  // encoder will avoid loopfilter on these frames.
  EXPECT_EQ(GetNonRefFrames(), GetMismatchFrames());
#endif
}

// Test for sync layer for 1 pass CBR SVC: 3 spatial layers and
// 3 temporal layers. Decoding QVGA before sync frame and decode up to
// HD on and after sync.
TEST_P(SyncFrameOnePassCbrSvc, OnePassCbrSvc3SL3TLSyncToVGAHD) {
  SetSvcConfig(3, 3);
  frame_to_start_decode_ = 0;
  frame_to_sync_ = 20;
  decode_to_layer_before_sync_ = 0;
  decode_to_layer_after_sync_ = 2;

  // Set up svc layer sync structure.
  svc_layer_sync_.base_layer_intra_only = 0;
  svc_layer_sync_.spatial_layer_sync[0] = 0;
  svc_layer_sync_.spatial_layer_sync[1] = 1;
  svc_layer_sync_.spatial_layer_sync[2] = 1;

  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  cfg_.rc_target_bitrate = 600;
  AssignLayerBitrates();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#if CONFIG_VP9_DECODER
  // The non-reference frames are expected to be mismatched frames as the
  // encoder will avoid loopfilter on these frames.
  EXPECT_EQ(GetNonRefFrames(), GetMismatchFrames());
#endif
}

#if CONFIG_VP9_TEMPORAL_DENOISING
// Test for sync layer for 1 pass CBR SVC: 2 spatial layers and
// 3 temporal layers. Decoding QVGA before sync frame and decode up to
// VGA on and after sync.
TEST_P(SyncFrameOnePassCbrSvc, OnePassCbrSvc2SL3TLSyncFrameVGADenoise) {
  SetSvcConfig(2, 3);
  frame_to_start_decode_ = 0;
  frame_to_sync_ = 100;
  decode_to_layer_before_sync_ = 0;
  decode_to_layer_after_sync_ = 1;

  denoiser_on_ = 1;
  // Set up svc layer sync structure.
  svc_layer_sync_.base_layer_intra_only = 0;
  svc_layer_sync_.spatial_layer_sync[0] = 0;
  svc_layer_sync_.spatial_layer_sync[1] = 1;

  ::libvpx_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  cfg_.rc_target_bitrate = 400;
  AssignLayerBitrates();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#if CONFIG_VP9_DECODER
  // The non-reference frames are expected to be mismatched frames as the
  // encoder will avoid loopfilter on these frames.
  EXPECT_EQ(GetNonRefFrames(), GetMismatchFrames());
#endif
}
#endif

// Start decoding from beginning of sequence, during sequence insert intra-only
// on base/qvga layer. Decode all layers.
TEST_P(SyncFrameOnePassCbrSvc, OnePassCbrSvc3SL3TLSyncFrameIntraOnlyQVGA) {
  SetSvcConfig(3, 3);
  frame_to_start_decode_ = 0;
  frame_to_sync_ = 20;
  decode_to_layer_before_sync_ = 2;
  // The superframe containing intra-only layer will have 4 frames. Thus set the
  // layer to decode after sync frame to 3.
  decode_to_layer_after_sync_ = 3;
  intra_only_test_ = true;

  // Set up svc layer sync structure.
  svc_layer_sync_.base_layer_intra_only = 1;
  svc_layer_sync_.spatial_layer_sync[0] = 1;
  svc_layer_sync_.spatial_layer_sync[1] = 0;
  svc_layer_sync_.spatial_layer_sync[2] = 0;

  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  cfg_.rc_target_bitrate = 600;
  AssignLayerBitrates();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#if CONFIG_VP9_DECODER
  // The non-reference frames are expected to be mismatched frames as the
  // encoder will avoid loopfilter on these frames.
  EXPECT_EQ(GetNonRefFrames(), GetMismatchFrames());
#endif
}

// Start decoding from beginning of sequence, during sequence insert intra-only
// on base/qvga layer and sync_layer on middle/VGA layer. Decode all layers.
TEST_P(SyncFrameOnePassCbrSvc, OnePassCbrSvc3SL3TLSyncFrameIntraOnlyVGA) {
  SetSvcConfig(3, 3);
  frame_to_start_decode_ = 0;
  frame_to_sync_ = 20;
  decode_to_layer_before_sync_ = 2;
  // The superframe containing intra-only layer will have 4 frames. Thus set the
  // layer to decode after sync frame to 3.
  decode_to_layer_after_sync_ = 3;
  intra_only_test_ = true;

  // Set up svc layer sync structure.
  svc_layer_sync_.base_layer_intra_only = 1;
  svc_layer_sync_.spatial_layer_sync[0] = 1;
  svc_layer_sync_.spatial_layer_sync[1] = 1;
  svc_layer_sync_.spatial_layer_sync[2] = 0;

  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  cfg_.rc_target_bitrate = 600;
  AssignLayerBitrates();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#if CONFIG_VP9_DECODER
  // The non-reference frames are expected to be mismatched frames as the
  // encoder will avoid loopfilter on these frames.
  EXPECT_EQ(GetNonRefFrames(), GetMismatchFrames());
#endif
}

// Start decoding from sync frame, insert intra-only on base/qvga layer. Decode
// all layers. For 1 spatial layer, it inserts a key frame.
TEST_P(SyncFrameOnePassCbrSvc, OnePassCbrSvc1SL3TLSyncFrameIntraOnlyQVGA) {
  SetSvcConfig(1, 3);
  frame_to_start_decode_ = 20;
  frame_to_sync_ = 20;
  decode_to_layer_before_sync_ = 0;
  decode_to_layer_after_sync_ = 0;
  intra_only_test_ = true;

  // Set up svc layer sync structure.
  svc_layer_sync_.base_layer_intra_only = 1;
  svc_layer_sync_.spatial_layer_sync[0] = 1;

  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  cfg_.rc_target_bitrate = 600;
  AssignLayerBitrates();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#if CONFIG_VP9_DECODER
  // The non-reference frames are expected to be mismatched frames as the
  // encoder will avoid loopfilter on these frames.
  EXPECT_EQ(GetNonRefFrames(), GetMismatchFrames());
#endif
}

VP9_INSTANTIATE_TEST_CASE(SyncFrameOnePassCbrSvc, ::testing::Range(0, 3));

INSTANTIATE_TEST_CASE_P(
    VP9, ScalePartitionOnePassCbrSvc,
    ::testing::Values(
        static_cast<const libvpx_test::CodecFactory *>(&libvpx_test::kVP9)));

}  // namespace
}  // namespace svc_test
