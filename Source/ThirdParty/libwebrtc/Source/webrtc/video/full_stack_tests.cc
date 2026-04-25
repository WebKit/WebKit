/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <optional>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "api/rtp_parameters.h"
#include "api/test/simulated_network.h"
#include "api/test/video_quality_test_fixture.h"
#include "api/units/data_rate.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/spatial_layer.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/vp9_profile.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "test/create_test_field_trials.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "video/config/video_encoder_config.h"
#include "video/video_quality_test.h"

ABSL_FLAG(std::string,
          rtc_event_log_name,
          "",
          "Filename for rtc event log. Two files "
          "with \"_send\" and \"_recv\" suffixes will be created.");
ABSL_FLAG(std::string,
          rtp_dump_name,
          "",
          "Filename for dumped received RTP stream.");
ABSL_FLAG(std::string,
          encoded_frame_path,
          "",
          "The base path for encoded frame logs. Created files will have "
          "the form <encoded_frame_path>.<n>.(recv|send.<m>).ivf");

namespace webrtc {

namespace {
const int kFullStackTestDurationSecs = 45;

struct ParamsWithLogging : public VideoQualityTest::Params {
 public:
  ParamsWithLogging() {
    // Use these logging flags by default, for everything.
    logging = {
        .rtc_event_log_name = absl::GetFlag(FLAGS_rtc_event_log_name),
        .rtp_dump_name = absl::GetFlag(FLAGS_rtp_dump_name),
        .encoded_frame_base_path = absl::GetFlag(FLAGS_encoded_frame_path)};
    this->config = BuiltInNetworkBehaviorConfig();
  }
};

std::string ClipNameToClipPath(const char* clip_name) {
  return test::ResourcePath(clip_name, "yuv");
}
}  // namespace

// VideoQualityTest::Params params = {
//   { ... },      // Common.
//   { ... },      // Video-specific settings.
//   { ... },      // Screenshare-specific settings.
//   { ... },      // Analyzer settings.
//   pipe,         // FakeNetworkPipe::Config
//   { ... },      // Spatial scalability.
//   logs          // bool
// };

#if defined(RTC_ENABLE_VP9)
TEST(FullStackTest, Foreman_Cif_Net_Delay_0_0_Plr_0_VP9) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 700000,
                          .target_bitrate_bps = 700000,
                          .max_bitrate_bps = 700000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP9",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {.test_label = "foreman_cif_net_delay_0_0_plr_0_VP9",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(GenericDescriptorTest,
     Foreman_Cif_Delay_50_0_Plr_5_VP9_Generic_Descriptor) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP9",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_delay_50_0_plr_5_VP9_generic_descriptor",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  foreman_cif.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Generator_Net_Delay_0_0_Plr_0_VP9Profile2) {
  // Profile 2 might not be available on some platforms until
  // https://bugs.chromium.org/p/webm/issues/detail?id=1544 is solved.
  bool profile_2_is_supported = false;
  for (const auto& codec : SupportedVP9Codecs()) {
    if (ParseSdpForVP9Profile(codec.parameters)
            .value_or(VP9Profile::kProfile0) == VP9Profile::kProfile2) {
      profile_2_is_supported = true;
    }
  }
  if (!profile_2_is_supported)
    return;
  VideoQualityTest fixture;

  CodecParameterMap vp92 = {
      {kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile2)}};
  ParamsWithLogging generator;
  generator.call.send_side_bwe = true;
  generator.video[0] = {.enabled = true,
                        .width = 352,
                        .height = 288,
                        .fps = 30,
                        .min_bitrate_bps = 700000,
                        .target_bitrate_bps = 700000,
                        .max_bitrate_bps = 700000,
                        .suspend_below_min_bitrate = false,
                        .codec = "VP9",
                        .num_temporal_layers = 1,
                        .selected_tl = 0,
                        .min_transmit_bps = 0,
                        .ulpfec = false,
                        .flexfec = false,
                        .automatic_scaling = true,
                        .clip_path = "GeneratorI010",
                        .capture_device_index = 0,
                        .sdp_params = vp92};
  generator.analyzer = {
      .test_label = "generator_net_delay_0_0_plr_0_VP9Profile2",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  fixture.RunWithAnalyzer(generator);
}

#endif  // defined(RTC_ENABLE_VP9)

#if defined(WEBRTC_LINUX)
// Crashes on the linux trusty perf bot: bugs.webrtc.org/9129.
#define MAYBE_Net_Delay_0_0_Plr_0 DISABLED_Net_Delay_0_0_Plr_0
#else
#define MAYBE_Net_Delay_0_0_Plr_0 Net_Delay_0_0_Plr_0
#endif
TEST(FullStackTest, MAYBE_Net_Delay_0_0_Plr_0) {
  VideoQualityTest fixture;
  ParamsWithLogging paris_qcif;
  paris_qcif.call.send_side_bwe = true;
  paris_qcif.video[0] = {.enabled = true,
                         .width = 176,
                         .height = 144,
                         .fps = 30,
                         .min_bitrate_bps = 300000,
                         .target_bitrate_bps = 300000,
                         .max_bitrate_bps = 300000,
                         .suspend_below_min_bitrate = false,
                         .codec = "VP8",
                         .num_temporal_layers = 1,
                         .selected_tl = 0,
                         .min_transmit_bps = 0,
                         .ulpfec = false,
                         .flexfec = false,
                         .automatic_scaling = true,
                         .clip_path = ClipNameToClipPath("paris_qcif")};
  paris_qcif.analyzer = {.test_label = "net_delay_0_0_plr_0",
                         .avg_psnr_threshold = 36.0,
                         .avg_ssim_threshold = 0.96,
                         .test_durations_secs = kFullStackTestDurationSecs};
  fixture.RunWithAnalyzer(paris_qcif);
}

TEST(GenericDescriptorTest,
     Foreman_Cif_Net_Delay_0_0_Plr_0_Generic_Descriptor) {
  VideoQualityTest fixture;
  // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 700000,
                          .target_bitrate_bps = 700000,
                          .max_bitrate_bps = 700000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_net_delay_0_0_plr_0_generic_descriptor",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(GenericDescriptorTest,
     Foreman_Cif_30kbps_Net_Delay_0_0_Plr_0_Generic_Descriptor) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 10,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 30000,
                          .max_bitrate_bps = 30000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_30kbps_net_delay_0_0_plr_0_generic_descriptor",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(foreman_cif);
}

// Link capacity below default start rate.
TEST(FullStackTest, Foreman_Cif_Link_150kbps_Net_Delay_0_0_Plr_0) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_link_150kbps_net_delay_0_0_plr_0",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(150);
  fixture.RunWithAnalyzer(foreman_cif);
}

// Restricted network and encoder overproducing by 30%.
TEST(FullStackTest,
     Foreman_Cif_Link_150kbps_Delay100ms_30pkts_Queue_Overshoot30) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif"),
                          .capture_device_index = 0,
                          .sdp_params = {},
                          .encoder_overshoot_factor = 1.30};
  foreman_cif.analyzer = {
      .test_label =
          "foreman_cif_link_150kbps_delay100ms_30pkts_queue_overshoot30",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(150);
  foreman_cif.config->queue_length_packets = 30;
  foreman_cif.config->queue_delay_ms = 100;
  fixture.RunWithAnalyzer(foreman_cif);
}

// Weak 3G-style link: 250kbps, 1% loss, 100ms delay, 15 packets queue.
// Packet rate and loss are low enough that loss will happen with ~3s interval.
// This triggers protection overhead to toggle between zero and non-zero.
// Link queue is restrictive enough to trigger loss on probes.
TEST(FullStackTest, Foreman_Cif_Link_250kbps_Delay100ms_10pkts_Loss1) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif"),
                          .capture_device_index = 0,
                          .sdp_params = {},
                          .encoder_overshoot_factor = 1.30};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_link_250kbps_delay100ms_10pkts_loss1",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(250);
  foreman_cif.config->queue_length_packets = 10;
  foreman_cif.config->queue_delay_ms = 100;
  foreman_cif.config->loss_percent = 1;
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(GenericDescriptorTest, Foreman_Cif_Delay_50_0_Plr_5_Generic_Descriptor) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_delay_50_0_plr_5_generic_descriptor",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  foreman_cif.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(GenericDescriptorTest,
     Foreman_Cif_Delay_50_0_Plr_5_Ulpfec_Generic_Descriptor) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = true,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_delay_50_0_plr_5_ulpfec_generic_descriptor",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  foreman_cif.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Foreman_Cif_Delay_50_0_Plr_5_Flexfec) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = true,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {.test_label = "foreman_cif_delay_50_0_plr_5_flexfec",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Foreman_Cif_500kbps_Delay_50_0_Plr_3_Flexfec) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = true,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_500kbps_delay_50_0_plr_3_flexfec",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 3;
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(500);
  foreman_cif.config->queue_delay_ms = 50;
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Foreman_Cif_500kbps_Delay_50_0_Plr_3_Ulpfec) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = true,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_500kbps_delay_50_0_plr_3_ulpfec",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 3;
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(500);
  foreman_cif.config->queue_delay_ms = 50;
  fixture.RunWithAnalyzer(foreman_cif);
}

#if defined(WEBRTC_USE_H264)
TEST(FullStackTest, Foreman_Cif_Net_Delay_0_0_Plr_0_H264) {
  VideoQualityTest fixture;
  // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 700000,
                          .target_bitrate_bps = 700000,
                          .max_bitrate_bps = 700000,
                          .suspend_below_min_bitrate = false,
                          .codec = "H264",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {.test_label = "foreman_cif_net_delay_0_0_plr_0_H264",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Foreman_Cif_30kbps_Net_Delay_0_0_Plr_0_H264) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 10,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 30000,
                          .max_bitrate_bps = 30000,
                          .suspend_below_min_bitrate = false,
                          .codec = "H264",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_30kbps_net_delay_0_0_plr_0_H264",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(GenericDescriptorTest,
     Foreman_Cif_Delay_50_0_Plr_5_H264_Generic_Descriptor) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "H264",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_delay_50_0_plr_5_H264_generic_descriptor",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  foreman_cif.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Foreman_Cif_Delay_50_0_Plr_5_H264_Sps_Pps_Idr) {
  VideoQualityTest fixture({.field_trials_ptr = CreateTestFieldTrialsPtr(
                                "WebRTC-SpsPpsIdrIsH264Keyframe/Enabled/")});
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "H264",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_delay_50_0_plr_5_H264_sps_pps_idr",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  fixture.RunWithAnalyzer(foreman_cif);
}

// Verify that this is worth the bot time, before enabling.
TEST(FullStackTest, Foreman_Cif_Delay_50_0_Plr_5_H264_Flexfec) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "H264",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = true,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_delay_50_0_plr_5_H264_flexfec",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  fixture.RunWithAnalyzer(foreman_cif);
}

// Ulpfec with H264 is an unsupported combination, so this test is only useful
// for debugging. It is therefore disabled by default.
TEST(FullStackTest, DISABLED_Foreman_Cif_Delay_50_0_Plr_5_H264_Ulpfec) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "H264",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = true,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_delay_50_0_plr_5_H264_ulpfec",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  fixture.RunWithAnalyzer(foreman_cif);
}
#endif  // defined(WEBRTC_USE_H264)

TEST(FullStackTest, Foreman_Cif_500kbps) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {.test_label = "foreman_cif_500kbps",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 0;
  foreman_cif.config->queue_delay_ms = 0;
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(500);
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Foreman_Cif_500kbps_32pkts_Queue) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {.test_label = "foreman_cif_500kbps_32pkts_queue",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 32;
  foreman_cif.config->queue_delay_ms = 0;
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(500);
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Foreman_Cif_500kbps_100ms) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {.test_label = "foreman_cif_500kbps_100ms",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 0;
  foreman_cif.config->queue_delay_ms = 100;
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(500);
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(GenericDescriptorTest,
     Foreman_Cif_500kbps_100ms_32pkts_Queue_Generic_Descriptor) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_500kbps_100ms_32pkts_queue_generic_descriptor",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 32;
  foreman_cif.config->queue_delay_ms = 100;
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(500);
  foreman_cif.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Foreman_Cif_500kbps_100ms_32pkts_Queue_Recv_Bwe) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = false;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 500000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_500kbps_100ms_32pkts_queue_recv_bwe",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 32;
  foreman_cif.config->queue_delay_ms = 100;
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(500);
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Foreman_Cif_1000kbps_100ms_32pkts_Queue) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 30,
                          .min_bitrate_bps = 30000,
                          .target_bitrate_bps = 2000000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 0,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = true,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {
      .test_label = "foreman_cif_1000kbps_100ms_32pkts_queue",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 32;
  foreman_cif.config->queue_delay_ms = 100;
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(1000);
  fixture.RunWithAnalyzer(foreman_cif);
}

// TODO(sprang): Remove this if we have the similar ModerateLimits below?
TEST(FullStackTest, Conference_Motion_Hd_2000kbps_100ms_32pkts_Queue) {
  VideoQualityTest fixture;
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      .enabled = true,
      .width = 1280,
      .height = 720,
      .fps = 50,
      .min_bitrate_bps = 30000,
      .target_bitrate_bps = 3000000,
      .max_bitrate_bps = 3000000,
      .suspend_below_min_bitrate = false,
      .codec = "VP8",
      .num_temporal_layers = 1,
      .selected_tl = 0,
      .min_transmit_bps = 0,
      .ulpfec = false,
      .flexfec = false,
      .automatic_scaling = false,
      .clip_path = ClipNameToClipPath("ConferenceMotion_1280_720_50")};
  conf_motion_hd.analyzer = {
      .test_label = "conference_motion_hd_2000kbps_100ms_32pkts_queue",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 32;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity = DataRate::KilobitsPerSec(2000);
  fixture.RunWithAnalyzer(conf_motion_hd);
}

TEST(GenericDescriptorTest,
     Conference_Motion_Hd_2tl_Moderate_Limits_Generic_Descriptor) {
  VideoQualityTest fixture;
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      .enabled = true,
      .width = 1280,
      .height = 720,
      .fps = 50,
      .min_bitrate_bps = 30000,
      .target_bitrate_bps = 3000000,
      .max_bitrate_bps = 3000000,
      .suspend_below_min_bitrate = false,
      .codec = "VP8",
      .num_temporal_layers = 2,
      .selected_tl = -1,
      .min_transmit_bps = 0,
      .ulpfec = false,
      .flexfec = false,
      .automatic_scaling = false,
      .clip_path = ClipNameToClipPath("ConferenceMotion_1280_720_50")};
  conf_motion_hd.analyzer = {
      .test_label =
          "conference_motion_hd_2tl_moderate_limits_generic_descriptor",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 50;
  conf_motion_hd.config->loss_percent = 3;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity = DataRate::KilobitsPerSec(2000);
  conf_motion_hd.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(conf_motion_hd);
}

TEST(FullStackTest, Conference_Motion_Hd_3tl_Moderate_Limits) {
  VideoQualityTest fixture;
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      .enabled = true,
      .width = 1280,
      .height = 720,
      .fps = 50,
      .min_bitrate_bps = 30000,
      .target_bitrate_bps = 3000000,
      .max_bitrate_bps = 3000000,
      .suspend_below_min_bitrate = false,
      .codec = "VP8",
      .num_temporal_layers = 3,
      .selected_tl = -1,
      .min_transmit_bps = 0,
      .ulpfec = false,
      .flexfec = false,
      .automatic_scaling = false,
      .clip_path = ClipNameToClipPath("ConferenceMotion_1280_720_50")};
  conf_motion_hd.analyzer = {
      .test_label = "conference_motion_hd_3tl_moderate_limits",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 50;
  conf_motion_hd.config->loss_percent = 3;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity = DataRate::KilobitsPerSec(2000);
  fixture.RunWithAnalyzer(conf_motion_hd);
}

TEST(FullStackTest, Conference_Motion_Hd_4tl_Moderate_Limits) {
  VideoQualityTest fixture;
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      .enabled = true,
      .width = 1280,
      .height = 720,
      .fps = 50,
      .min_bitrate_bps = 30000,
      .target_bitrate_bps = 3000000,
      .max_bitrate_bps = 3000000,
      .suspend_below_min_bitrate = false,
      .codec = "VP8",
      .num_temporal_layers = 4,
      .selected_tl = -1,
      .min_transmit_bps = 0,
      .ulpfec = false,
      .flexfec = false,
      .automatic_scaling = false,
      .clip_path = ClipNameToClipPath("ConferenceMotion_1280_720_50")};
  conf_motion_hd.analyzer = {
      .test_label = "conference_motion_hd_4tl_moderate_limits",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 50;
  conf_motion_hd.config->loss_percent = 3;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity = DataRate::KilobitsPerSec(2000);
  fixture.RunWithAnalyzer(conf_motion_hd);
}

TEST(FullStackTest, Foreman_Cif_30kbps_AV1) {
  VideoQualityTest fixture;
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {.enabled = true,
                          .width = 352,
                          .height = 288,
                          .fps = 10,
                          .min_bitrate_bps = 20'000,
                          .target_bitrate_bps = 30'000,
                          .max_bitrate_bps = 100'000,
                          .codec = "AV1",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .clip_path = ClipNameToClipPath("foreman_cif")};
  foreman_cif.analyzer = {.test_label = "foreman_cif_30kbps_AV1",
                          .test_durations_secs = kFullStackTestDurationSecs};
  foreman_cif.config->link_capacity = DataRate::KilobitsPerSec(30);
  foreman_cif.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, Conference_Motion_Hd_3tl_AV1) {
  VideoQualityTest fixture;
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      .enabled = true,
      .width = 1280,
      .height = 720,
      .fps = 50,
      .min_bitrate_bps = 20'000,
      .target_bitrate_bps = 500'000,
      .max_bitrate_bps = 1'000'000,
      .codec = "AV1",
      .num_temporal_layers = 3,
      .clip_path = ClipNameToClipPath("ConferenceMotion_1280_720_50")};

  conf_motion_hd.analyzer = {.test_label = "conference_motion_hd_3tl_AV1",
                             .test_durations_secs = kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 50;
  conf_motion_hd.config->loss_percent = 3;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity = DataRate::KilobitsPerSec(1000);
  conf_motion_hd.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(conf_motion_hd);
}

#if defined(WEBRTC_MAC)
// TODO(webrtc:351644561): Flaky on Mac x86/ARM.
#define MAYBE_Screenshare_Slides_Simulcast_AV1 \
  DISABLED_Screenshare_Slides_Simulcast_AV1
#else
#define MAYBE_Screenshare_Slides_Simulcast_AV1 Screenshare_Slides_Simulcast_AV1
#endif
TEST(FullStackTest, MAYBE_Screenshare_Slides_Simulcast_AV1) {
  VideoQualityTest fixture;
  ParamsWithLogging screenshare;
  screenshare.analyzer = {.test_label = "screenshare_slides_simulcast_AV1",
                          .test_durations_secs = kFullStackTestDurationSecs};
  screenshare.call.send_side_bwe = true;
  screenshare.screenshare[0] = {.enabled = true};
  screenshare.video[0] = {.enabled = true,
                          .width = 1850,
                          .height = 1110,
                          .fps = 30,
                          .min_bitrate_bps = 0,
                          .target_bitrate_bps = 0,
                          .max_bitrate_bps = 2500000,
                          .codec = "AV1",
                          .num_temporal_layers = 2};

  // Set `min_bitrate_bps` and `target_bitrate_bps` to zero to use WebRTC
  // defaults.
  VideoQualityTest::Params screenshare_params_low;
  screenshare_params_low.video[0] = {.enabled = true,
                                     .width = 1850,
                                     .height = 1110,
                                     .fps = 5,
                                     .min_bitrate_bps = 0,
                                     .target_bitrate_bps = 0,
                                     .max_bitrate_bps = 420'000,
                                     .codec = "AV1",
                                     .num_temporal_layers = 2};

  VideoQualityTest::Params screenshare_params_high;
  screenshare_params_high.video[0] = {.enabled = true,
                                      .width = 1850,
                                      .height = 1110,
                                      .fps = 30,
                                      .min_bitrate_bps = 0,
                                      .target_bitrate_bps = 0,
                                      .max_bitrate_bps = 2'500'000,
                                      .codec = "AV1",
                                      .num_temporal_layers = 2};

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(screenshare_params_low, 0),
      VideoQualityTest::DefaultVideoStream(screenshare_params_high, 0)};
  screenshare.ss[0] = {
      .streams = streams,
      .selected_stream = 1,
  };
  fixture.RunWithAnalyzer(screenshare);
}

#if defined(RTC_ENABLE_VP9)
TEST(FullStackTest, Conference_Motion_Hd_2000kbps_100ms_32pkts_Queue_Vp9) {
  VideoQualityTest fixture;
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      .enabled = true,
      .width = 1280,
      .height = 720,
      .fps = 50,
      .min_bitrate_bps = 30000,
      .target_bitrate_bps = 3000000,
      .max_bitrate_bps = 3000000,
      .suspend_below_min_bitrate = false,
      .codec = "VP9",
      .num_temporal_layers = 1,
      .selected_tl = 0,
      .min_transmit_bps = 0,
      .ulpfec = false,
      .flexfec = false,
      .automatic_scaling = false,
      .clip_path = ClipNameToClipPath("ConferenceMotion_1280_720_50")};
  conf_motion_hd.analyzer = {
      .test_label = "conference_motion_hd_2000kbps_100ms_32pkts_queue_vp9",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 32;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity = DataRate::KilobitsPerSec(2000);
  fixture.RunWithAnalyzer(conf_motion_hd);
}
#endif

TEST(FullStackTest, Screenshare_Slides) {
  VideoQualityTest fixture;
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {.enabled = true,
                          .width = 1850,
                          .height = 1110,
                          .fps = 5,
                          .min_bitrate_bps = 50000,
                          .target_bitrate_bps = 200000,
                          .max_bitrate_bps = 1000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 2,
                          .selected_tl = 1,
                          .min_transmit_bps = 400000,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = false,
                          .clip_path = ""};
  screenshare.screenshare[0] = {
      .enabled = true, .generate_slides = false, .slide_change_interval = 10};
  screenshare.analyzer = {.test_label = "screenshare_slides",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  fixture.RunWithAnalyzer(screenshare);
}

#if !defined(WEBRTC_MAC) && !defined(WEBRTC_WIN)
// TODO(bugs.webrtc.org/9840): Investigate why is this test flaky on Win/Mac.
TEST(FullStackTest, Screenshare_Slides_Simulcast) {
  VideoQualityTest fixture;
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.screenshare[0] = {
      .enabled = true, .generate_slides = false, .slide_change_interval = 10};
  screenshare.video[0] = {.enabled = true,
                          .width = 1850,
                          .height = 1110,
                          .fps = 30,
                          .min_bitrate_bps = 800000,
                          .target_bitrate_bps = 2500000,
                          .max_bitrate_bps = 2500000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 2,
                          .selected_tl = 1,
                          .min_transmit_bps = 400000,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = false,
                          .clip_path = ""};
  screenshare.analyzer = {.test_label = "screenshare_slides_simulcast",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  ParamsWithLogging screenshare_params_high;
  screenshare_params_high.video[0] = {.enabled = true,
                                      .width = 1850,
                                      .height = 1110,
                                      .fps = 60,
                                      .min_bitrate_bps = 600000,
                                      .target_bitrate_bps = 1250000,
                                      .max_bitrate_bps = 1250000,
                                      .suspend_below_min_bitrate = false,
                                      .codec = "VP8",
                                      .num_temporal_layers = 2,
                                      .selected_tl = 0,
                                      .min_transmit_bps = 400000,
                                      .ulpfec = false,
                                      .flexfec = false,
                                      .automatic_scaling = false,
                                      .clip_path = ""};
  VideoQualityTest::Params screenshare_params_low;
  screenshare_params_low.video[0] = {.enabled = true,
                                     .width = 1850,
                                     .height = 1110,
                                     .fps = 5,
                                     .min_bitrate_bps = 30000,
                                     .target_bitrate_bps = 200000,
                                     .max_bitrate_bps = 1000000,
                                     .suspend_below_min_bitrate = false,
                                     .codec = "VP8",
                                     .num_temporal_layers = 2,
                                     .selected_tl = 0,
                                     .min_transmit_bps = 400000,
                                     .ulpfec = false,
                                     .flexfec = false,
                                     .automatic_scaling = false,
                                     .clip_path = ""};

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(screenshare_params_low, 0),
      VideoQualityTest::DefaultVideoStream(screenshare_params_high, 0)};
  screenshare.ss[0] = {.streams = streams,
                       .selected_stream = 1,
                       .num_spatial_layers = 1,
                       .selected_sl = 0,
                       .inter_layer_pred = InterLayerPredMode::kOn,
                       .spatial_layers = std::vector<SpatialLayer>(),
                       .infer_streams = false};
  fixture.RunWithAnalyzer(screenshare);
}

#endif  // !defined(WEBRTC_MAC) && !defined(WEBRTC_WIN)

TEST(FullStackTest, Screenshare_Slides_Scrolling) {
  VideoQualityTest fixture;
  ParamsWithLogging config;
  config.call.send_side_bwe = true;
  config.video[0] = {.enabled = true,
                     .width = 1850,
                     .height = 1110 / 2,
                     .fps = 5,
                     .min_bitrate_bps = 50000,
                     .target_bitrate_bps = 200000,
                     .max_bitrate_bps = 1000000,
                     .suspend_below_min_bitrate = false,
                     .codec = "VP8",
                     .num_temporal_layers = 2,
                     .selected_tl = 1,
                     .min_transmit_bps = 400000,
                     .ulpfec = false,
                     .flexfec = false,
                     .automatic_scaling = false,
                     .clip_path = ""};
  config.screenshare[0] = {.enabled = true,
                           .generate_slides = false,
                           .slide_change_interval = 10,
                           .scroll_duration = 2};
  config.analyzer = {.test_label = "screenshare_slides_scrolling",
                     .avg_psnr_threshold = 0.0,
                     .avg_ssim_threshold = 0.0,
                     .test_durations_secs = kFullStackTestDurationSecs};
  fixture.RunWithAnalyzer(config);
}

TEST(GenericDescriptorTest, Screenshare_Slides_Lossy_Net_Generic_Descriptor) {
  VideoQualityTest fixture;
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {.enabled = true,
                          .width = 1850,
                          .height = 1110,
                          .fps = 5,
                          .min_bitrate_bps = 50000,
                          .target_bitrate_bps = 200000,
                          .max_bitrate_bps = 1000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 2,
                          .selected_tl = 1,
                          .min_transmit_bps = 400000,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = false,
                          .clip_path = ""};
  screenshare.screenshare[0] = {
      .enabled = true, .generate_slides = false, .slide_change_interval = 10};
  screenshare.analyzer = {
      .test_label = "screenshare_slides_lossy_net_generic_descriptor",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  screenshare.config->loss_percent = 5;
  screenshare.config->queue_delay_ms = 200;
  screenshare.config->link_capacity = DataRate::KilobitsPerSec(500);
  screenshare.call.generic_descriptor = true;
  fixture.RunWithAnalyzer(screenshare);
}

TEST(FullStackTest, Screenshare_Slides_Very_Lossy) {
  VideoQualityTest fixture;
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {.enabled = true,
                          .width = 1850,
                          .height = 1110,
                          .fps = 5,
                          .min_bitrate_bps = 50000,
                          .target_bitrate_bps = 200000,
                          .max_bitrate_bps = 1000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 2,
                          .selected_tl = 1,
                          .min_transmit_bps = 400000,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = false,
                          .clip_path = ""};
  screenshare.screenshare[0] = {
      .enabled = true, .generate_slides = false, .slide_change_interval = 10};
  screenshare.analyzer = {.test_label = "screenshare_slides_very_lossy",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  screenshare.config->loss_percent = 10;
  screenshare.config->queue_delay_ms = 200;
  screenshare.config->link_capacity = DataRate::KilobitsPerSec(500);
  fixture.RunWithAnalyzer(screenshare);
}

TEST(FullStackTest, Screenshare_Slides_Lossy_Limited) {
  VideoQualityTest fixture;
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {.enabled = true,
                          .width = 1850,
                          .height = 1110,
                          .fps = 5,
                          .min_bitrate_bps = 50000,
                          .target_bitrate_bps = 200000,
                          .max_bitrate_bps = 1000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 2,
                          .selected_tl = 1,
                          .min_transmit_bps = 400000,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = false,
                          .clip_path = ""};
  screenshare.screenshare[0] = {
      .enabled = true, .generate_slides = false, .slide_change_interval = 10};
  screenshare.analyzer = {.test_label = "screenshare_slides_lossy_limited",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  screenshare.config->loss_percent = 5;
  screenshare.config->link_capacity = DataRate::KilobitsPerSec(200);
  screenshare.config->queue_length_packets = 30;

  fixture.RunWithAnalyzer(screenshare);
}

TEST(FullStackTest, Screenshare_Slides_Moderately_Restricted) {
  VideoQualityTest fixture;
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {.enabled = true,
                          .width = 1850,
                          .height = 1110,
                          .fps = 5,
                          .min_bitrate_bps = 50000,
                          .target_bitrate_bps = 200000,
                          .max_bitrate_bps = 1000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP8",
                          .num_temporal_layers = 2,
                          .selected_tl = 1,
                          .min_transmit_bps = 400000,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = false,
                          .clip_path = ""};
  screenshare.screenshare[0] = {
      .enabled = true, .generate_slides = false, .slide_change_interval = 10};
  screenshare.analyzer = {
      .test_label = "screenshare_slides_moderately_restricted",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  screenshare.config->loss_percent = 1;
  screenshare.config->link_capacity = DataRate::KilobitsPerSec(1200);
  screenshare.config->queue_length_packets = 30;

  fixture.RunWithAnalyzer(screenshare);
}

// Since ParamsWithLogging::Video is not trivially destructible, we can't
// store these structs as const globals.
ParamsWithLogging::Video SvcVp9Video() {
  return ParamsWithLogging::Video{
      .enabled = true,
      .width = 1280,
      .height = 720,
      .fps = 30,
      .min_bitrate_bps = 800000,
      .target_bitrate_bps = 2500000,
      .max_bitrate_bps = 2500000,
      .suspend_below_min_bitrate = false,
      .codec = "VP9",
      .num_temporal_layers = 3,
      .selected_tl = 2,
      .min_transmit_bps = 400000,
      .ulpfec = false,
      .flexfec = false,
      .automatic_scaling = false,
      .clip_path = ClipNameToClipPath("ConferenceMotion_1280_720_50")};
}

ParamsWithLogging::Video SimulcastVp8VideoHigh() {
  return ParamsWithLogging::Video{
      .enabled = true,
      .width = 1280,
      .height = 720,
      .fps = 30,
      .min_bitrate_bps = 800000,
      .target_bitrate_bps = 2500000,
      .max_bitrate_bps = 2500000,
      .suspend_below_min_bitrate = false,
      .codec = "VP8",
      .num_temporal_layers = 3,
      .selected_tl = 2,
      .min_transmit_bps = 400000,
      .ulpfec = false,
      .flexfec = false,
      .automatic_scaling = false,
      .clip_path = ClipNameToClipPath("ConferenceMotion_1280_720_50")};
}

ParamsWithLogging::Video SimulcastVp8VideoMedium() {
  return ParamsWithLogging::Video{
      .enabled = true,
      .width = 640,
      .height = 360,
      .fps = 30,
      .min_bitrate_bps = 150000,
      .target_bitrate_bps = 500000,
      .max_bitrate_bps = 700000,
      .suspend_below_min_bitrate = false,
      .codec = "VP8",
      .num_temporal_layers = 3,
      .selected_tl = 2,
      .min_transmit_bps = 400000,
      .ulpfec = false,
      .flexfec = false,
      .automatic_scaling = false,
      .clip_path = ClipNameToClipPath("ConferenceMotion_1280_720_50")};
}

ParamsWithLogging::Video SimulcastVp8VideoLow() {
  return ParamsWithLogging::Video{
      .enabled = true,
      .width = 320,
      .height = 180,
      .fps = 30,
      .min_bitrate_bps = 30000,
      .target_bitrate_bps = 150000,
      .max_bitrate_bps = 200000,
      .suspend_below_min_bitrate = false,
      .codec = "VP8",
      .num_temporal_layers = 3,
      .selected_tl = 2,
      .min_transmit_bps = 400000,
      .ulpfec = false,
      .flexfec = false,
      .automatic_scaling = false,
      .clip_path = ClipNameToClipPath("ConferenceMotion_1280_720_50")};
}

#if defined(RTC_ENABLE_VP9)

TEST(FullStackTest, Screenshare_Slides_Vp9_3sl_High_Fps) {
  VideoQualityTest fixture;
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {.enabled = true,
                          .width = 1850,
                          .height = 1110,
                          .fps = 30,
                          .min_bitrate_bps = 50000,
                          .target_bitrate_bps = 200000,
                          .max_bitrate_bps = 2000000,
                          .suspend_below_min_bitrate = false,
                          .codec = "VP9",
                          .num_temporal_layers = 1,
                          .selected_tl = 0,
                          .min_transmit_bps = 400000,
                          .ulpfec = false,
                          .flexfec = false,
                          .automatic_scaling = false,
                          .clip_path = ""};
  screenshare.screenshare[0] = {
      .enabled = true, .generate_slides = false, .slide_change_interval = 10};
  screenshare.analyzer = {.test_label = "screenshare_slides_vp9_3sl_high_fps",
                          .avg_psnr_threshold = 0.0,
                          .avg_ssim_threshold = 0.0,
                          .test_durations_secs = kFullStackTestDurationSecs};
  screenshare.ss[0] = {.streams = std::vector<VideoStream>(),
                       .selected_stream = 0,
                       .num_spatial_layers = 3,
                       .selected_sl = 2,
                       .inter_layer_pred = InterLayerPredMode::kOn,
                       .spatial_layers = std::vector<SpatialLayer>(),
                       .infer_streams = true};
  fixture.RunWithAnalyzer(screenshare);
}

// TODO(http://bugs.webrtc.org/9506): investigate.
#if !defined(WEBRTC_MAC)

TEST(FullStackTest, Vp9ksvc_3sl_High) {
  VideoQualityTest fixture(
      {.field_trials_ptr = CreateTestFieldTrialsPtr(
           "WebRTC-Vp9IssueKeyFrameOnLayerDeactivation/Enabled/")});
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = SvcVp9Video();
  simulcast.analyzer = {.test_label = "vp9ksvc_3sl_high",
                        .avg_psnr_threshold = 0.0,
                        .avg_ssim_threshold = 0.0,
                        .test_durations_secs = kFullStackTestDurationSecs};
  simulcast.ss[0] = {.streams = std::vector<VideoStream>(),
                     .selected_stream = 0,
                     .num_spatial_layers = 3,
                     .selected_sl = 2,
                     .inter_layer_pred = InterLayerPredMode::kOnKeyPic,
                     .spatial_layers = std::vector<SpatialLayer>(),
                     .infer_streams = false};
  fixture.RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, Vp9ksvc_3sl_Low) {
  VideoQualityTest fixture(
      {.field_trials_ptr = CreateTestFieldTrialsPtr(
           "WebRTC-Vp9IssueKeyFrameOnLayerDeactivation/Enabled/")});
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = SvcVp9Video();
  simulcast.analyzer = {.test_label = "vp9ksvc_3sl_low",
                        .avg_psnr_threshold = 0.0,
                        .avg_ssim_threshold = 0.0,
                        .test_durations_secs = kFullStackTestDurationSecs};
  simulcast.ss[0] = {.streams = std::vector<VideoStream>(),
                     .selected_stream = 0,
                     .num_spatial_layers = 3,
                     .selected_sl = 0,
                     .inter_layer_pred = InterLayerPredMode::kOnKeyPic,
                     .spatial_layers = std::vector<SpatialLayer>(),
                     .infer_streams = false};
  fixture.RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, Vp9ksvc_3sl_Low_Bw_Limited) {
  VideoQualityTest fixture(
      {.field_trials_ptr = CreateTestFieldTrialsPtr(
           "WebRTC-Vp9IssueKeyFrameOnLayerDeactivation/Enabled/")});
  ParamsWithLogging simulcast;
  simulcast.config->link_capacity = DataRate::KilobitsPerSec(500);
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = SvcVp9Video();
  simulcast.analyzer = {.test_label = "vp9ksvc_3sl_low_bw_limited",
                        .avg_psnr_threshold = 0.0,
                        .avg_ssim_threshold = 0.0,
                        .test_durations_secs = kFullStackTestDurationSecs};
  simulcast.ss[0] = {.streams = std::vector<VideoStream>(),
                     .selected_stream = 0,
                     .num_spatial_layers = 3,
                     .selected_sl = 0,
                     .inter_layer_pred = InterLayerPredMode::kOnKeyPic,
                     .spatial_layers = std::vector<SpatialLayer>(),
                     .infer_streams = false};
  fixture.RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, Vp9ksvc_3sl_Medium_Network_Restricted) {
  VideoQualityTest fixture(
      {.field_trials_ptr = CreateTestFieldTrialsPtr(
           "WebRTC-Vp9IssueKeyFrameOnLayerDeactivation/Enabled/")});
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = SvcVp9Video();
  simulcast.analyzer = {.test_label = "vp9ksvc_3sl_medium_network_restricted",
                        .avg_psnr_threshold = 0.0,
                        .avg_ssim_threshold = 0.0,
                        .test_durations_secs = kFullStackTestDurationSecs};
  simulcast.ss[0] = {.streams = std::vector<VideoStream>(),
                     .selected_stream = 0,
                     .num_spatial_layers = 3,
                     .selected_sl = -1,
                     .inter_layer_pred = InterLayerPredMode::kOnKeyPic,
                     .spatial_layers = std::vector<SpatialLayer>(),
                     .infer_streams = false};
  simulcast.config->link_capacity = DataRate::KilobitsPerSec(1000);
  simulcast.config->queue_delay_ms = 100;
  fixture.RunWithAnalyzer(simulcast);
}

// TODO(webrtc:9722): Remove when experiment is cleaned up.
TEST(FullStackTest, Vp9ksvc_3sl_Medium_Network_Restricted_Trusted_Rate) {
  VideoQualityTest fixture(
      {.field_trials_ptr = CreateTestFieldTrialsPtr(
           "WebRTC-Vp9IssueKeyFrameOnLayerDeactivation/Enabled/")});
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = SvcVp9Video();
  simulcast.analyzer = {
      .test_label = "vp9ksvc_3sl_medium_network_restricted_trusted_rate",
      .avg_psnr_threshold = 0.0,
      .avg_ssim_threshold = 0.0,
      .test_durations_secs = kFullStackTestDurationSecs};
  simulcast.ss[0] = {.streams = std::vector<VideoStream>(),
                     .selected_stream = 0,
                     .num_spatial_layers = 3,
                     .selected_sl = -1,
                     .inter_layer_pred = InterLayerPredMode::kOnKeyPic,
                     .spatial_layers = std::vector<SpatialLayer>(),
                     .infer_streams = false};
  simulcast.config->link_capacity = DataRate::KilobitsPerSec(1000);
  simulcast.config->queue_delay_ms = 100;
  fixture.RunWithAnalyzer(simulcast);
}
#endif  // !defined(WEBRTC_MAC)

#endif  // defined(RTC_ENABLE_VP9)

// Android bots can't handle FullHD, so disable the test.
// TODO(bugs.webrtc.org/9220): Investigate source of flakiness on Mac.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_MAC)
#define MAYBE_Simulcast_HD_High DISABLED_Simulcast_HD_High
#else
#define MAYBE_Simulcast_HD_High Simulcast_HD_High
#endif

TEST(FullStackTest, MAYBE_Simulcast_HD_High) {
  VideoQualityTest fixture(
      {.field_trials_ptr = CreateTestFieldTrialsPtr(
           "WebRTC-ForceSimulatedOveruseIntervalMs/1000-50000-300/")});
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = {.enabled = true,
                        .width = 1920,
                        .height = 1080,
                        .fps = 30,
                        .min_bitrate_bps = 800000,
                        .target_bitrate_bps = 2500000,
                        .max_bitrate_bps = 2500000,
                        .suspend_below_min_bitrate = false,
                        .codec = "VP8",
                        .num_temporal_layers = 3,
                        .selected_tl = 2,
                        .min_transmit_bps = 400000,
                        .ulpfec = false,
                        .flexfec = false,
                        .automatic_scaling = false,
                        .clip_path = "Generator"};
  simulcast.analyzer = {.test_label = "simulcast_HD_high",
                        .avg_psnr_threshold = 0.0,
                        .avg_ssim_threshold = 0.0,
                        .test_durations_secs = kFullStackTestDurationSecs};
  simulcast.config->loss_percent = 0;
  simulcast.config->queue_delay_ms = 100;
  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(simulcast, 0),
      VideoQualityTest::DefaultVideoStream(simulcast, 0),
      VideoQualityTest::DefaultVideoStream(simulcast, 0)};
  simulcast.ss[0] = {.streams = streams,
                     .selected_stream = 2,
                     .num_spatial_layers = 1,
                     .selected_sl = 0,
                     .inter_layer_pred = InterLayerPredMode::kOn,
                     .spatial_layers = std::vector<SpatialLayer>(),
                     .infer_streams = true};
  fixture.RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, Simulcast_Vp8_3sl_High) {
  VideoQualityTest fixture;
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = SimulcastVp8VideoHigh();
  simulcast.analyzer = {.test_label = "simulcast_vp8_3sl_high",
                        .avg_psnr_threshold = 0.0,
                        .avg_ssim_threshold = 0.0,
                        .test_durations_secs = kFullStackTestDurationSecs};
  simulcast.config->loss_percent = 0;
  simulcast.config->queue_delay_ms = 100;
  ParamsWithLogging video_params_high;
  video_params_high.video[0] = SimulcastVp8VideoHigh();
  ParamsWithLogging video_params_medium;
  video_params_medium.video[0] = SimulcastVp8VideoMedium();
  ParamsWithLogging video_params_low;
  video_params_low.video[0] = SimulcastVp8VideoLow();

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(video_params_low, 0),
      VideoQualityTest::DefaultVideoStream(video_params_medium, 0),
      VideoQualityTest::DefaultVideoStream(video_params_high, 0)};
  simulcast.ss[0] = {.streams = streams,
                     .selected_stream = 2,
                     .num_spatial_layers = 1,
                     .selected_sl = 0,
                     .inter_layer_pred = InterLayerPredMode::kOn,
                     .spatial_layers = std::vector<SpatialLayer>(),
                     .infer_streams = false};
  fixture.RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, Simulcast_Vp8_3sl_Low) {
  VideoQualityTest fixture;
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = SimulcastVp8VideoHigh();
  simulcast.analyzer = {.test_label = "simulcast_vp8_3sl_low",
                        .avg_psnr_threshold = 0.0,
                        .avg_ssim_threshold = 0.0,
                        .test_durations_secs = kFullStackTestDurationSecs};
  simulcast.config->loss_percent = 0;
  simulcast.config->queue_delay_ms = 100;
  ParamsWithLogging video_params_high;
  video_params_high.video[0] = SimulcastVp8VideoHigh();
  ParamsWithLogging video_params_medium;
  video_params_medium.video[0] = SimulcastVp8VideoMedium();
  ParamsWithLogging video_params_low;
  video_params_low.video[0] = SimulcastVp8VideoLow();

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(video_params_low, 0),
      VideoQualityTest::DefaultVideoStream(video_params_medium, 0),
      VideoQualityTest::DefaultVideoStream(video_params_high, 0)};
  simulcast.ss[0] = {.streams = streams,
                     .selected_stream = 0,
                     .num_spatial_layers = 1,
                     .selected_sl = 0,
                     .inter_layer_pred = InterLayerPredMode::kOn,
                     .spatial_layers = std::vector<SpatialLayer>(),
                     .infer_streams = false};
  fixture.RunWithAnalyzer(simulcast);
}

// This test assumes ideal network conditions with target bandwidth being
// available and exercises WebRTC calls with a high target bitrate(100 Mbps).
// Android32 bots can't handle this high bitrate, so disable test for those.
#if defined(WEBRTC_ANDROID)
#define MAYBE_High_Bitrate_With_Fake_Codec DISABLED_High_Bitrate_With_Fake_Codec
#else
#define MAYBE_High_Bitrate_With_Fake_Codec High_Bitrate_With_Fake_Codec
#endif  // defined(WEBRTC_ANDROID)
TEST(FullStackTest, MAYBE_High_Bitrate_With_Fake_Codec) {
  VideoQualityTest fixture;
  const int target_bitrate = 100000000;
  ParamsWithLogging generator;
  generator.call.send_side_bwe = true;
  generator.call.call_bitrate_config.min_bitrate_bps = target_bitrate;
  generator.call.call_bitrate_config.start_bitrate_bps = target_bitrate;
  generator.call.call_bitrate_config.max_bitrate_bps = target_bitrate;
  generator.video[0] = {.enabled = true,
                        .width = 360,
                        .height = 240,
                        .fps = 30,
                        .min_bitrate_bps = target_bitrate / 2,
                        .target_bitrate_bps = target_bitrate,
                        .max_bitrate_bps = target_bitrate * 2,
                        .suspend_below_min_bitrate = false,
                        .codec = "FakeCodec",
                        .num_temporal_layers = 1,
                        .selected_tl = 0,
                        .min_transmit_bps = 0,
                        .ulpfec = false,
                        .flexfec = false,
                        .automatic_scaling = false,
                        .clip_path = "Generator"};
  generator.analyzer = {.test_label = "high_bitrate_with_fake_codec",
                        .avg_psnr_threshold = 0.0,
                        .avg_ssim_threshold = 0.0,
                        .test_durations_secs = kFullStackTestDurationSecs};
  fixture.RunWithAnalyzer(generator);
}

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
// Fails on mobile devices:
// https://bugs.chromium.org/p/webrtc/issues/detail?id=7301
#define MAYBE_Largeroom_50thumb DISABLED_Largeroom_50thumb
#else
#define MAYBE_Largeroom_50thumb Largeroom_50thumb
#endif

TEST(FullStackTest, MAYBE_Largeroom_50thumb) {
  VideoQualityTest fixture;
  ParamsWithLogging large_room;
  large_room.call.send_side_bwe = true;
  large_room.video[0] = SimulcastVp8VideoHigh();
  large_room.analyzer = {.test_label = "largeroom_50thumb",
                         .avg_psnr_threshold = 0.0,
                         .avg_ssim_threshold = 0.0,
                         .test_durations_secs = kFullStackTestDurationSecs};
  large_room.config->loss_percent = 0;
  large_room.config->queue_delay_ms = 100;
  ParamsWithLogging video_params_high;
  video_params_high.video[0] = SimulcastVp8VideoHigh();
  ParamsWithLogging video_params_medium;
  video_params_medium.video[0] = SimulcastVp8VideoMedium();
  ParamsWithLogging video_params_low;
  video_params_low.video[0] = SimulcastVp8VideoLow();

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(video_params_low, 0),
      VideoQualityTest::DefaultVideoStream(video_params_medium, 0),
      VideoQualityTest::DefaultVideoStream(video_params_high, 0)};
  large_room.call.num_thumbnails = 50;
  large_room.ss[0] = {.streams = streams,
                      .selected_stream = 2,
                      .num_spatial_layers = 1,
                      .selected_sl = 0,
                      .inter_layer_pred = InterLayerPredMode::kOn,
                      .spatial_layers = std::vector<SpatialLayer>(),
                      .infer_streams = false};
  fixture.RunWithAnalyzer(large_room);
}

}  // namespace webrtc
