/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>

#include "webrtc/test/gtest.h"
#include "webrtc/video/video_quality_test.h"

namespace webrtc {

static const int kFullStackTestDurationSecs = 60;

class FullStackTest : public VideoQualityTest {
 public:
  void RunTest(const VideoQualityTest::Params &params) {
    RunWithAnalyzer(params);
  }

  void ForemanCifWithoutPacketLoss(const std::string& video_codec) {
    // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
    VideoQualityTest::Params foreman_cif;
    foreman_cif.video = {true, 352, 288, 30, 700000, 700000, 700000, false,
                         video_codec, 1, 0, 0, false, "", "foreman_cif"};
    foreman_cif.analyzer = {"foreman_cif_net_delay_0_0_plr_0_" + video_codec,
                            0.0, 0.0, kFullStackTestDurationSecs};
    RunTest(foreman_cif);
  }

  void ForemanCifPlr5(const std::string& video_codec) {
    VideoQualityTest::Params foreman_cif;
    foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false,
                         video_codec, 1, 0, 0, false, "", "foreman_cif"};
    foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_" + video_codec, 0.0,
                            0.0, kFullStackTestDurationSecs};
    foreman_cif.pipe.loss_percent = 5;
    foreman_cif.pipe.queue_delay_ms = 50;
    RunTest(foreman_cif);
  }
};

// VideoQualityTest::Params params = {
//   { ... },      // Common.
//   { ... },      // Video-specific settings.
//   { ... },      // Screenshare-specific settings.
//   { ... },      // Analyzer settings.
//   pipe,         // FakeNetworkPipe::Config
//   { ... },      // Spatial scalability.
//   logs          // bool
// };

#if !defined(RTC_DISABLE_VP9)
TEST_F(FullStackTest, ForemanCifWithoutPacketLossVp9) {
  ForemanCifWithoutPacketLoss("VP9");
}

TEST_F(FullStackTest, ForemanCifPlr5Vp9) {
  ForemanCifPlr5("VP9");
}
#endif  // !defined(RTC_DISABLE_VP9)

TEST_F(FullStackTest, ParisQcifWithoutPacketLoss) {
  VideoQualityTest::Params paris_qcif;
  paris_qcif.call.send_side_bwe = true;
  paris_qcif.video = {true, 176, 144, 30, 300000, 300000, 300000, false,
                      "VP8", 1, 0, 0, false, "", "paris_qcif"};
  paris_qcif.analyzer = {"net_delay_0_0_plr_0", 36.0, 0.96,
                         kFullStackTestDurationSecs};
  RunTest(paris_qcif);
}

TEST_F(FullStackTest, ForemanCifWithoutPacketLoss) {
  // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 700000, 700000, 700000, false, "VP8",
                       1, 0, 0, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_net_delay_0_0_plr_0", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCifPlr5) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.loss_percent = 5;
  foreman_cif.pipe.queue_delay_ms = 50;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.queue_length_packets = 0;
  foreman_cif.pipe.queue_delay_ms = 0;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbpsLimitedQueue) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_32pkts_queue", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.queue_length_packets = 32;
  foreman_cif.pipe.queue_delay_ms = 0;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps100ms) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_100ms", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.queue_length_packets = 0;
  foreman_cif.pipe.queue_delay_ms = 100;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps100msLimitedQueue) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_100ms_32pkts_queue", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.queue_length_packets = 32;
  foreman_cif.pipe.queue_delay_ms = 100;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps100msLimitedQueueRecvBwe) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, false,  "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_100ms_32pkts_queue", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.queue_length_packets = 32;
  foreman_cif.pipe.queue_delay_ms = 100;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif1000kbps100msLimitedQueue) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 2000000, 2000000, false,
                       "VP8", 1, 0, 0, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_1000kbps_100ms_32pkts_queue", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.queue_length_packets = 32;
  foreman_cif.pipe.queue_delay_ms = 100;
  foreman_cif.pipe.link_capacity_kbps = 1000;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ConferenceMotionHd2000kbps100msLimitedQueue) {
  VideoQualityTest::Params conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video = {true, 1280, 720, 50, 30000, 3000000, 3000000, false,
                          "VP8", 1, 0, 0, false, "",
                          "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {"conference_motion_hd_2000kbps_100ms_32pkts_queue",
                             0.0, 0.0, kFullStackTestDurationSecs};
  conf_motion_hd.pipe.queue_length_packets = 32;
  conf_motion_hd.pipe.queue_delay_ms = 100;
  conf_motion_hd.pipe.link_capacity_kbps = 2000;
  RunTest(conf_motion_hd);
}

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL) {
  VideoQualityTest::Params screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video = {true, 1850, 1110, 5, 50000, 200000, 2000000, false,
                       "VP8", 2, 1, 400000, false, "", ""};
  screenshare.screenshare = {true, 10};
  screenshare.analyzer = {"screenshare_slides", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  RunTest(screenshare);
}

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL_Scroll) {
  VideoQualityTest::Params config;
  config.call.send_side_bwe = true;
  config.video = {true, 1850, 1110 / 2, 5, 50000,  200000, 2000000, false,
                  "VP8", 2, 1, 400000, false, "", ""};
  config.screenshare = {true, 10, 2};
  config.analyzer = {"screenshare_slides_scrolling", 0.0, 0.0,
                     kFullStackTestDurationSecs};
  RunTest(config);
}

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL_LossyNet) {
  VideoQualityTest::Params screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video = {true, 1850,  1110, 5, 50000, 200000, 2000000, false,
                       "VP8", 2, 1, 400000, false, "", ""};
  screenshare.screenshare = {true, 10};
  screenshare.analyzer = {"screenshare_slides_lossy_net", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  screenshare.pipe.loss_percent = 5;
  screenshare.pipe.queue_delay_ms = 200;
  screenshare.pipe.link_capacity_kbps = 500;
  RunTest(screenshare);
}

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL_VeryLossyNet) {
  VideoQualityTest::Params screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video = {true, 1850, 1110, 5, 50000, 200000, 2000000, false,
                       "VP8", 2, 1, 400000, false, "", ""};
  screenshare.screenshare = {true, 10};
  screenshare.analyzer = {"screenshare_slides_very_lossy", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  screenshare.pipe.loss_percent = 10;
  screenshare.pipe.queue_delay_ms = 200;
  screenshare.pipe.link_capacity_kbps = 500;
  RunTest(screenshare);
}

#if !defined(RTC_DISABLE_VP9)
TEST_F(FullStackTest, ScreenshareSlidesVP9_2SL) {
  VideoQualityTest::Params screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video = {true, 1850, 1110, 5, 50000, 200000, 2000000, false,
                       "VP9", 1, 0, 400000, false, "", ""};
  screenshare.screenshare = {true, 10};
  screenshare.analyzer = {"screenshare_slides_vp9_2sl", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  screenshare.logs = false;
  screenshare.ss = {std::vector<VideoStream>(), 0, 2, 1};
  RunTest(screenshare);
}
#endif  // !defined(RTC_DISABLE_VP9)
}  // namespace webrtc
