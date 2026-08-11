/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

const int kMaxErrorFrames = 12;
const int kMaxInvisibleErrorFrames = 12;
const int kMaxDroppableFrames = 12;
const int kMaxErrorResilientFrames = 12;
const int kMaxNoMFMVFrames = 12;
const int kMaxPrimRefNoneFrames = 12;
const int kMaxSFrames = 12;
const int kCpuUsed = 1;

class ErrorResilienceTestLarge
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  ErrorResilienceTestLarge()
      : EncoderTest(GET_PARAM(0)), psnr_(0.0), nframes_(0), mismatch_psnr_(0.0),
        mismatch_nframes_(0), encoding_mode_(GET_PARAM(1)), allow_mismatch_(0),
        enable_altref_(GET_PARAM(2)) {
    Reset();
  }

  ~ErrorResilienceTestLarge() override = default;

  void Reset() {
    error_nframes_ = 0;
    invisible_error_nframes_ = 0;
    droppable_nframes_ = 0;
    error_resilient_nframes_ = 0;
    nomfmv_nframes_ = 0;
    prim_ref_none_nframes_ = 0;
    s_nframes_ = 0;
  }

  void SetupEncoder(int bitrate, int lag) {
    const aom_rational timebase = { 33333333, 1000000000 };
    cfg_.g_timebase = timebase;
    cfg_.rc_target_bitrate = bitrate;
    cfg_.kf_mode = AOM_KF_DISABLED;
    cfg_.g_lag_in_frames = lag;
    init_flags_ = AOM_CODEC_USE_PSNR;
  }

  void SetUp() override { InitializeConfig(encoding_mode_); }

  void BeginPassHook(unsigned int /*pass*/) override {
    psnr_ = 0.0;
    nframes_ = 0;
    decoded_nframes_ = 0;
    mismatch_psnr_ = 0.0;
    mismatch_nframes_ = 0;
  }

  void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) override {
    psnr_ += pkt->data.psnr.psnr[0];
    nframes_++;
  }

  void PreEncodeFrameHook(libaom_test::VideoSource *video,
                          libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, kCpuUsed);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, enable_altref_);
    }
    frame_flags_ &=
        ~(AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF |
          AOM_EFLAG_NO_REF_FRAME_MVS | AOM_EFLAG_ERROR_RESILIENT |
          AOM_EFLAG_SET_S_FRAME | AOM_EFLAG_SET_PRIMARY_REF_NONE);
    if (droppable_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < droppable_nframes_; ++i) {
        if (droppable_frames_[i] == video->frame()) {
          std::cout << "             Encoding droppable frame: "
                    << droppable_frames_[i] << "\n";
          frame_flags_ |= (AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                           AOM_EFLAG_NO_UPD_ARF);
          break;
        }
      }
    }

    if (error_resilient_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < error_resilient_nframes_; ++i) {
        if (error_resilient_frames_[i] == video->frame()) {
          std::cout << "             Encoding error_resilient frame: "
                    << error_resilient_frames_[i] << "\n";
          frame_flags_ |= AOM_EFLAG_ERROR_RESILIENT;
          break;
        }
      }
    }

    if (nomfmv_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < nomfmv_nframes_; ++i) {
        if (nomfmv_frames_[i] == video->frame()) {
          std::cout << "             Encoding no mfmv frame: "
                    << nomfmv_frames_[i] << "\n";
          frame_flags_ |= AOM_EFLAG_NO_REF_FRAME_MVS;
          break;
        }
      }
    }

    if (prim_ref_none_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < prim_ref_none_nframes_; ++i) {
        if (prim_ref_none_frames_[i] == video->frame()) {
          std::cout << "             Encoding no PRIMARY_REF_NONE frame: "
                    << prim_ref_none_frames_[i] << "\n";
          frame_flags_ |= AOM_EFLAG_SET_PRIMARY_REF_NONE;
          break;
        }
      }
    }

    encoder->Control(AV1E_SET_S_FRAME_MODE, 0);
    if (s_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < s_nframes_; ++i) {
        if (s_frames_[i] == video->frame()) {
          std::cout << "             Encoding S frame: " << s_frames_[i]
                    << "\n";
          frame_flags_ |= AOM_EFLAG_SET_S_FRAME;
          break;
        }
      }
    }
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    // Check that the encode frame flags are correctly reflected
    // in the output frame flags.
    const int encode_flags = pkt->data.frame.flags >> 16;
    if ((encode_flags & (AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                         AOM_EFLAG_NO_UPD_ARF)) ==
        (AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF)) {
      ASSERT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_DROPPABLE,
                AOM_FRAME_IS_DROPPABLE);
    }
    if (encode_flags & AOM_EFLAG_SET_S_FRAME) {
      ASSERT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_SWITCH,
                AOM_FRAME_IS_SWITCH);
    }
    if (encode_flags & AOM_EFLAG_ERROR_RESILIENT) {
      ASSERT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_ERROR_RESILIENT,
                AOM_FRAME_IS_ERROR_RESILIENT);
    }
  }

  double GetAveragePsnr() const {
    if (nframes_) return psnr_ / nframes_;
    return 0.0;
  }

  double GetAverageMismatchPsnr() const {
    if (mismatch_nframes_) return mismatch_psnr_ / mismatch_nframes_;
    return 0.0;
  }

  bool DoDecode() const override {
    if (error_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < error_nframes_; ++i) {
        if (error_frames_[i] == nframes_ - 1) {
          std::cout << "             Skipping decoding frame: "
                    << error_frames_[i] << "\n";
          return false;
        }
      }
    }
    return true;
  }

  bool DoDecodeInvisible() const override {
    if (invisible_error_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < invisible_error_nframes_; ++i) {
        if (invisible_error_frames_[i] == nframes_ - 1) {
          std::cout << "             Skipping decoding all invisible frames in "
                       "frame pkt: "
                    << invisible_error_frames_[i] << "\n";
          return false;
        }
      }
    }
    return true;
  }

  void MismatchHook(const aom_image_t *img1, const aom_image_t *img2) override {
    if (allow_mismatch_) {
      double mismatch_psnr = compute_psnr(img1, img2);
      mismatch_psnr_ += mismatch_psnr;
      ++mismatch_nframes_;
      // std::cout << "Mismatch frame psnr: " << mismatch_psnr << "\n";
    } else {
      ::libaom_test::EncoderTest::MismatchHook(img1, img2);
    }
  }

  void DecompressedFrameHook(const aom_image_t &img,
                             aom_codec_pts_t pts) override {
    (void)img;
    (void)pts;
    ++decoded_nframes_;
  }

  void SetErrorFrames(int num, unsigned int *list) {
    if (num > kMaxErrorFrames)
      num = kMaxErrorFrames;
    else if (num < 0)
      num = 0;
    error_nframes_ = num;
    for (unsigned int i = 0; i < error_nframes_; ++i)
      error_frames_[i] = list[i];
  }

  void SetInvisibleErrorFrames(int num, unsigned int *list) {
    if (num > kMaxInvisibleErrorFrames)
      num = kMaxInvisibleErrorFrames;
    else if (num < 0)
      num = 0;
    invisible_error_nframes_ = num;
    for (unsigned int i = 0; i < invisible_error_nframes_; ++i)
      invisible_error_frames_[i] = list[i];
  }

  void SetDroppableFrames(int num, unsigned int *list) {
    if (num > kMaxDroppableFrames)
      num = kMaxDroppableFrames;
    else if (num < 0)
      num = 0;
    droppable_nframes_ = num;
    for (unsigned int i = 0; i < droppable_nframes_; ++i)
      droppable_frames_[i] = list[i];
  }

  void SetErrorResilientFrames(int num, unsigned int *list) {
    if (num > kMaxErrorResilientFrames)
      num = kMaxErrorResilientFrames;
    else if (num < 0)
      num = 0;
    error_resilient_nframes_ = num;
    for (unsigned int i = 0; i < error_resilient_nframes_; ++i)
      error_resilient_frames_[i] = list[i];
  }

  void SetNoMFMVFrames(int num, unsigned int *list) {
    if (num > kMaxNoMFMVFrames)
      num = kMaxNoMFMVFrames;
    else if (num < 0)
      num = 0;
    nomfmv_nframes_ = num;
    for (unsigned int i = 0; i < nomfmv_nframes_; ++i)
      nomfmv_frames_[i] = list[i];
  }

  void SetPrimaryRefNoneFrames(int num, unsigned int *list) {
    if (num > kMaxPrimRefNoneFrames)
      num = kMaxPrimRefNoneFrames;
    else if (num < 0)
      num = 0;
    prim_ref_none_nframes_ = num;
    for (unsigned int i = 0; i < prim_ref_none_nframes_; ++i)
      prim_ref_none_frames_[i] = list[i];
  }

  void SetSFrames(int num, unsigned int *list) {
    if (num > kMaxSFrames)
      num = kMaxSFrames;
    else if (num < 0)
      num = 0;
    s_nframes_ = num;
    for (unsigned int i = 0; i < s_nframes_; ++i) s_frames_[i] = list[i];
  }

  unsigned int GetMismatchFrames() { return mismatch_nframes_; }
  unsigned int GetEncodedFrames() { return nframes_; }
  unsigned int GetDecodedFrames() { return decoded_nframes_; }

  void SetAllowMismatch(int allow) { allow_mismatch_ = allow; }

 private:
  double psnr_;
  unsigned int nframes_;
  unsigned int decoded_nframes_;
  unsigned int error_nframes_;
  unsigned int invisible_error_nframes_;
  unsigned int droppable_nframes_;
  unsigned int error_resilient_nframes_;
  unsigned int nomfmv_nframes_;
  unsigned int prim_ref_none_nframes_;
  unsigned int s_nframes_;
  double mismatch_psnr_;
  unsigned int mismatch_nframes_;
  unsigned int error_frames_[kMaxErrorFrames];
  unsigned int invisible_error_frames_[kMaxInvisibleErrorFrames];
  unsigned int droppable_frames_[kMaxDroppableFrames];
  unsigned int error_resilient_frames_[kMaxErrorResilientFrames];
  unsigned int nomfmv_frames_[kMaxNoMFMVFrames];
  unsigned int prim_ref_none_frames_[kMaxPrimRefNoneFrames];
  unsigned int s_frames_[kMaxSFrames];
  libaom_test::TestMode encoding_mode_;
  int allow_mismatch_;
  int enable_altref_;
};

TEST_P(ErrorResilienceTestLarge, OnVersusOff) {
  SetupEncoder(2000, 10);
  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, 12);

  // Global error resilient mode OFF.
  cfg_.g_error_resilient = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  const double psnr_resilience_off = GetAveragePsnr();
  EXPECT_GT(psnr_resilience_off, 25.0);

  Reset();
  // Error resilient mode ON for certain frames
  unsigned int num_error_resilient_frames = 5;
  unsigned int error_resilient_frame_list[] = { 3, 5, 6, 9, 11 };
  SetErrorResilientFrames(num_error_resilient_frames,
                          error_resilient_frame_list);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  const double psnr_resilience_on = GetAveragePsnr();
  EXPECT_GT(psnr_resilience_on, 25.0);

  // Test that turning on error resilient mode hurts by 10% at most.
  if (psnr_resilience_off > 0.0) {
    const double psnr_ratio = psnr_resilience_on / psnr_resilience_off;
    EXPECT_GE(psnr_ratio, 0.9);
    EXPECT_LE(psnr_ratio, 1.1);
  }
}

// Check for successful decoding and no encoder/decoder mismatch
// if we lose (i.e., drop before decoding) a set of droppable
// frames (i.e., frames that don't update any reference buffers).
TEST_P(ErrorResilienceTestLarge, DropFramesWithoutRecovery) {
  if (GET_PARAM(1) == ::libaom_test::kOnePassGood && GET_PARAM(2) == 1) {
    fprintf(stderr, "Skipping test case #1 because of bug aomedia:3002\n");
    return;
  }
  SetupEncoder(500, 10);
  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, 20);

  // Set an arbitrary set of error frames same as droppable frames.
  unsigned int num_droppable_frames = 3;
  unsigned int droppable_frame_list[] = { 5, 11, 13 };
  SetDroppableFrames(num_droppable_frames, droppable_frame_list);
  SetErrorFrames(num_droppable_frames, droppable_frame_list);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  // Test that no mismatches have been found
  std::cout << "             Encoded frames: " << GetEncodedFrames() << "\n";
  std::cout << "             Decoded frames: " << GetDecodedFrames() << "\n";
  std::cout << "             Mismatch frames: " << GetMismatchFrames() << "\n";
  EXPECT_EQ(GetEncodedFrames() - GetDecodedFrames(), num_droppable_frames);
}

// Check for ParseAbility property of an error-resilient frame.
// Encode a frame in error-resilient mode (E-frame), and disallow all
// subsequent frames from using MFMV. If frames are dropped before the
// E frame, all frames starting from the E frame should be parse-able.
TEST_P(ErrorResilienceTestLarge, ParseAbilityTest) {
  SetupEncoder(500, 10);

  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, 15);

  SetAllowMismatch(1);

  // Note that an E-frame cannot be forced on a frame that is a
  // show_existing_frame, or a frame that comes directly after an invisible
  // frame. Currently, this will cause an assertion failure.
  // Set an arbitrary error resilient (E) frame
  unsigned int num_error_resilient_frames = 1;
  unsigned int error_resilient_frame_list[] = { 8 };
  SetErrorResilientFrames(num_error_resilient_frames,
                          error_resilient_frame_list);
  // Ensure that any invisible frames before the E frame are dropped
  SetInvisibleErrorFrames(num_error_resilient_frames,
                          error_resilient_frame_list);
  // Set all frames after the error resilient frame to not allow MFMV
  unsigned int num_post_error_resilient_frames = 6;
  unsigned int post_error_resilient_frame_list[] = { 9, 10, 11, 12, 13, 14 };
  SetNoMFMVFrames(num_post_error_resilient_frames,
                  post_error_resilient_frame_list);

  // Set a few frames before the E frame that are lost (not decoded)
  unsigned int num_error_frames = 5;
  unsigned int error_frame_list[] = { 3, 4, 5, 6, 7 };
  SetErrorFrames(num_error_frames, error_frame_list);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  std::cout << "             Encoded frames: " << GetEncodedFrames() << "\n";
  std::cout << "             Decoded frames: " << GetDecodedFrames() << "\n";
  std::cout << "             Mismatch frames: " << GetMismatchFrames() << "\n";
  EXPECT_EQ(GetEncodedFrames() - GetDecodedFrames(), num_error_frames);
  // All frames following the E-frame and the E-frame are expected to have
  // mismatches, but still be parse-able.
  EXPECT_LE(GetMismatchFrames(), num_post_error_resilient_frames + 1);
}

// Check for ParseAbility property of an S frame.
// Encode an S-frame. If frames are dropped before the S-frame, all frames
// starting from the S frame should be parse-able.
TEST_P(ErrorResilienceTestLarge, SFrameTest) {
  SetupEncoder(500, 10);

  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, 15);

  SetAllowMismatch(1);

  // Note that an S-frame cannot be forced on a frame that is a
  // show_existing_frame. This issue still needs to be addressed.
  // Set an arbitrary S-frame
  unsigned int num_s_frames = 1;
  unsigned int s_frame_list[] = { 6 };
  SetSFrames(num_s_frames, s_frame_list);
  // Ensure that any invisible frames before the S frame are dropped
  SetInvisibleErrorFrames(num_s_frames, s_frame_list);

  // Set a few frames before the S frame that are lost (not decoded)
  unsigned int num_error_frames = 4;
  unsigned int error_frame_list[] = { 2, 3, 4, 5 };
  SetErrorFrames(num_error_frames, error_frame_list);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  std::cout << "             Encoded frames: " << GetEncodedFrames() << "\n";
  std::cout << "             Decoded frames: " << GetDecodedFrames() << "\n";
  std::cout << "             Mismatch frames: " << GetMismatchFrames() << "\n";
  EXPECT_EQ(GetEncodedFrames() - GetDecodedFrames(), num_error_frames);
  // All frames following the S-frame and the S-frame are expected to have
  // mismatches, but still be parse-able.
  EXPECT_LE(GetMismatchFrames(), GetEncodedFrames() - s_frame_list[0]);
}

AV1_INSTANTIATE_TEST_SUITE(ErrorResilienceTestLarge, NONREALTIME_TEST_MODES,
                           ::testing::Values(0, 1));
}  // namespace
