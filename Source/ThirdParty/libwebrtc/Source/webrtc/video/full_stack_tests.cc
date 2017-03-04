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

static const int kFullStackTestDurationSecs = 45;

class FullStackTest : public VideoQualityTest {
 public:
  void RunTest(const VideoQualityTest::Params &params) {
    RunWithAnalyzer(params);
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
  // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 700000, 700000, 700000, false,
                       "VP9", 1, 0, 0, false, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_net_delay_0_0_plr_0_VP9", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCifPlr5Vp9) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false,
                       "VP9", 1, 0, 0, false, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_VP9", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.loss_percent = 5;
  foreman_cif.pipe.queue_delay_ms = 50;
  RunTest(foreman_cif);
}
#endif  // !defined(RTC_DISABLE_VP9)

TEST_F(FullStackTest, ParisQcifWithoutPacketLoss) {
  VideoQualityTest::Params paris_qcif;
  paris_qcif.call.send_side_bwe = true;
  paris_qcif.video = {true, 176, 144, 30, 300000, 300000, 300000, false,
                      "VP8", 1, 0, 0, false, false, "", "paris_qcif"};
  paris_qcif.analyzer = {"net_delay_0_0_plr_0", 36.0, 0.96,
                         kFullStackTestDurationSecs};
  RunTest(paris_qcif);
}

TEST_F(FullStackTest, ForemanCifWithoutPacketLoss) {
  // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 700000, 700000, 700000, false, "VP8",
                       1, 0, 0, false, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_net_delay_0_0_plr_0", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif30kbpsWithoutPacketLoss) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 10, 30000, 30000, 30000, false,
                       "VP8", 1, 0, 0, false, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_30kbps_net_delay_0_0_plr_0", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCifPlr5) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, false, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.loss_percent = 5;
  foreman_cif.pipe.queue_delay_ms = 50;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCifPlr5Ulpfec) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, true, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_ulpfec", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.loss_percent = 5;
  foreman_cif.pipe.queue_delay_ms = 50;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCifPlr5Flexfec) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, false, true, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_flexfec", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.loss_percent = 5;
  foreman_cif.pipe.queue_delay_ms = 50;
  RunTest(foreman_cif);
}

#if defined(WEBRTC_USE_H264)
TEST_F(FullStackTest, ForemanCifWithoutPacketlossH264) {
  // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 700000, 700000, 700000, false,
                       "H264", 1, 0, 0, false, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_net_delay_0_0_plr_0_H264", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif30kbpsWithoutPacketlossH264) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 10, 30000, 30000, 30000, false, "H264",
                       1, 0, 0, false, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_30kbps_net_delay_0_0_plr_0_H264", 0.0,
                          0.0, kFullStackTestDurationSecs};
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCifPlr5H264) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false,
                       "H264", 1, 0, 0, false, false, "", "foreman_cif"};
  std::string fec_description;
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_H264", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.loss_percent = 5;
  foreman_cif.pipe.queue_delay_ms = 50;
  RunTest(foreman_cif);
}

// Verify that this is worth the bot time, before enabling.
TEST_F(FullStackTest, ForemanCifPlr5H264Flexfec) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false,
                       "H264", 1, 0, 0, false, true, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_H264_flexfec", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.loss_percent = 5;
  foreman_cif.pipe.queue_delay_ms = 50;
  RunTest(foreman_cif);
}

// Ulpfec with H264 is an unsupported combination, so this test is only useful
// for debugging. It is therefore disabled by default.
TEST_F(FullStackTest, DISABLED_ForemanCifPlr5H264Ulpfec) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false,
                       "H264", 1, 0, 0, true, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_H264_ulpfec", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.loss_percent = 5;
  foreman_cif.pipe.queue_delay_ms = 50;
  RunTest(foreman_cif);
}
#endif  // defined(WEBRTC_USE_H264)

TEST_F(FullStackTest, ForemanCif500kbps) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, false, false, "", "foreman_cif"};
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
                       1, 0, 0, false, false, "", "foreman_cif"};
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
                       1, 0, 0, false, false, "", "foreman_cif"};
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
                       1, 0, 0, false, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_100ms_32pkts_queue", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.pipe.queue_length_packets = 32;
  foreman_cif.pipe.queue_delay_ms = 100;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps100msLimitedQueueRecvBwe) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = false;
  foreman_cif.video = {true, 352, 288, 30, 30000, 500000, 2000000, false, "VP8",
                       1, 0, 0, false, false, "", "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_100ms_32pkts_queue_recv_bwe",
                          0.0, 0.0, kFullStackTestDurationSecs};
  foreman_cif.pipe.queue_length_packets = 32;
  foreman_cif.pipe.queue_delay_ms = 100;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif1000kbps100msLimitedQueue) {
  VideoQualityTest::Params foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video = {true, 352, 288, 30, 30000, 2000000, 2000000, false,
                       "VP8", 1, 0, 0, false, false, "", "foreman_cif"};
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
                          "VP8", 1, 0, 0, false, false, "",
                          "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {"conference_motion_hd_2000kbps_100ms_32pkts_queue",
                             0.0, 0.0, kFullStackTestDurationSecs};
  conf_motion_hd.pipe.queue_length_packets = 32;
  conf_motion_hd.pipe.queue_delay_ms = 100;
  conf_motion_hd.pipe.link_capacity_kbps = 2000;
  RunTest(conf_motion_hd);
}

#if !defined(RTC_DISABLE_VP9)
TEST_F(FullStackTest, ConferenceMotionHd2000kbps100msLimitedQueueVP9) {
  VideoQualityTest::Params conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video = {true, 1280, 720, 50, 30000, 3000000, 3000000, false,
                          "VP9", 1, 0, 0, false, false, "",
                          "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {
      "conference_motion_hd_2000kbps_100ms_32pkts_queue_vp9", 0.0, 0.0,
      kFullStackTestDurationSecs};
  conf_motion_hd.pipe.queue_length_packets = 32;
  conf_motion_hd.pipe.queue_delay_ms = 100;
  conf_motion_hd.pipe.link_capacity_kbps = 2000;
  RunTest(conf_motion_hd);
}
#endif

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL) {
  VideoQualityTest::Params screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video = {true, 1850, 1110, 5, 50000, 200000, 2000000, false,
                       "VP8", 2, 1, 400000, false, false, "", ""};
  screenshare.screenshare = {true, 10};
  screenshare.analyzer = {"screenshare_slides", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  RunTest(screenshare);
}

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL_Scroll) {
  VideoQualityTest::Params config;
  config.call.send_side_bwe = true;
  config.video = {true, 1850, 1110 / 2, 5, 50000,  200000, 2000000, false,
                  "VP8", 2, 1, 400000, false, false, "", ""};
  config.screenshare = {true, 10, 2};
  config.analyzer = {"screenshare_slides_scrolling", 0.0, 0.0,
                     kFullStackTestDurationSecs};
  RunTest(config);
}

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL_LossyNet) {
  VideoQualityTest::Params screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video = {true, 1850,  1110, 5, 50000, 200000, 2000000, false,
                       "VP8", 2, 1, 400000, false, false, "", ""};
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
                       "VP8", 2, 1, 400000, false, false, "", ""};
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
                       "VP9", 1, 0, 400000, false, false, "", ""};
  screenshare.screenshare = {true, 10};
  screenshare.analyzer = {"screenshare_slides_vp9_2sl", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  screenshare.logs = false;
  screenshare.ss = {std::vector<VideoStream>(), 0, 2, 1};
  RunTest(screenshare);
}

TEST_F(FullStackTest, VP9SVC_3SL_High) {
  VideoQualityTest::Params simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video = {true,   1280,    720,     50,
                     800000, 2500000, 2500000, false,
                     "VP9",  1,       0,       400000,
                     false,  false,   "",      "ConferenceMotion_1280_720_50"};
  simulcast.analyzer = {"vp9svc_3sl_high", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.logs = false;
  simulcast.ss = {std::vector<VideoStream>(), 0, 3, 2};
  RunTest(simulcast);
}

TEST_F(FullStackTest, VP9SVC_3SL_Medium) {
  VideoQualityTest::Params simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video = {true,   1280,    720,     50,
                     800000, 2500000, 2500000, false,
                     "VP9",  1,       0,       400000,
                     false,  false,   "",      "ConferenceMotion_1280_720_50"};
  simulcast.analyzer = {"vp9svc_3sl_medium", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.logs = false;
  simulcast.ss = {std::vector<VideoStream>(), 0, 3, 1};
  RunTest(simulcast);
}

TEST_F(FullStackTest, VP9SVC_3SL_Low) {
  VideoQualityTest::Params simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video = {true,   1280,    720,     50,
                     800000, 2500000, 2500000, false,
                     "VP9",  1,       0,       400000,
                     false,  false,   "",      "ConferenceMotion_1280_720_50"};
  simulcast.analyzer = {"vp9svc_3sl_low", 0.0, 0.0, kFullStackTestDurationSecs};
  simulcast.logs = false;
  simulcast.ss = {std::vector<VideoStream>(), 0, 3, 0};
  RunTest(simulcast);
}
#endif  // !defined(RTC_DISABLE_VP9)

TEST_F(FullStackTest, SimulcastVP8_3SL_High) {
  VideoQualityTest::Params simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video = {true,   1280,    720,     50,
                     800000, 2500000, 2500000, false,
                     "VP8",  1,       0,       400000,
                     false,  false,   "",      "ConferenceMotion_1280_720_50"};
  simulcast.analyzer = {"simulcast_vp8_3sl_high", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.pipe.loss_percent = 0;
  simulcast.pipe.queue_delay_ms = 100;
  VideoQualityTest::Params video_params_high;
  video_params_high.video = {
      true,   1280,    720,     50,
      800000, 2500000, 2500000, false,
      "VP8",  1,       0,       400000,
      false,  false,   "",      "ConferenceMotion_1280_720_50"};
  VideoQualityTest::Params video_params_medium;
  video_params_medium.video = {
      true,   640,    360,    50,
      150000, 500000, 700000, false,
      "VP8",  1,      0,      400000,
      false,  false,  "",     "ConferenceMotion_1280_720_50"};
  VideoQualityTest::Params video_params_low;
  video_params_low.video = {
      true,  320,    180,    50,
      30000, 150000, 200000, false,
      "VP8", 1,      0,      400000,
      false, false,  "",     "ConferenceMotion_1280_720_50"};

  std::vector<VideoStream> streams = {DefaultVideoStream(video_params_low),
                                      DefaultVideoStream(video_params_medium),
                                      DefaultVideoStream(video_params_high)};
  simulcast.ss = {streams, 2, 1, 0};
  RunTest(simulcast);
}

TEST_F(FullStackTest, SimulcastVP8_3SL_Medium) {
  VideoQualityTest::Params simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video = {true,   1280,    720,     50,
                     800000, 2500000, 2500000, false,
                     "VP8",  1,       0,       400000,
                     false,  false,   "",      "ConferenceMotion_1280_720_50"};
  simulcast.analyzer = {"simulcast_vp8_3sl_medium", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.pipe.loss_percent = 0;
  simulcast.pipe.queue_delay_ms = 100;
  VideoQualityTest::Params video_params_high;
  video_params_high.video = {
      true,   1280,    720,     50,
      800000, 2500000, 2500000, false,
      "VP8",  1,       0,       400000,
      false,  false,   "",      "ConferenceMotion_1280_720_50"};
  VideoQualityTest::Params video_params_medium;
  video_params_medium.video = {
      true,   640,    360,    50,
      150000, 500000, 700000, false,
      "VP8",  1,      0,      400000,
      false,  false,  "",     "ConferenceMotion_1280_720_50"};
  VideoQualityTest::Params video_params_low;
  video_params_low.video = {
      true,  320,    180,    50,
      30000, 150000, 200000, false,
      "VP8", 1,      0,      400000,
      false, false,  "",     "ConferenceMotion_1280_720_50"};

  std::vector<VideoStream> streams = {DefaultVideoStream(video_params_low),
                                      DefaultVideoStream(video_params_medium),
                                      DefaultVideoStream(video_params_high)};
  simulcast.ss = {streams, 1, 1, 0};
  RunTest(simulcast);
}

TEST_F(FullStackTest, SimulcastVP8_3SL_Low) {
  VideoQualityTest::Params simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video = {true,   1280,    720,     50,
                     800000, 2500000, 2500000, false,
                     "VP8",  1,       0,       400000,
                     false,  false,   "",      "ConferenceMotion_1280_720_50"};
  simulcast.analyzer = {"simulcast_vp8_3sl_low", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.pipe.loss_percent = 0;
  simulcast.pipe.queue_delay_ms = 100;
  VideoQualityTest::Params video_params_high;
  video_params_high.video = {
      true,   1280,    720,     50,
      800000, 2500000, 2500000, false,
      "VP8",  1,       0,       400000,
      false,  false,   "",      "ConferenceMotion_1280_720_50"};
  VideoQualityTest::Params video_params_medium;
  video_params_medium.video = {
      true,   640,    360,    50,
      150000, 500000, 700000, false,
      "VP8",  1,      0,      400000,
      false,  false,  "",     "ConferenceMotion_1280_720_50"};
  VideoQualityTest::Params video_params_low;
  video_params_low.video = {
      true,  320,    180,    50,
      30000, 150000, 200000, false,
      "VP8", 1,      0,      400000,
      false, false,  "",     "ConferenceMotion_1280_720_50"};

  std::vector<VideoStream> streams = {DefaultVideoStream(video_params_low),
                                      DefaultVideoStream(video_params_medium),
                                      DefaultVideoStream(video_params_high)};
  simulcast.ss = {streams, 0, 1, 0};
  RunTest(simulcast);
}

}  // namespace webrtc
