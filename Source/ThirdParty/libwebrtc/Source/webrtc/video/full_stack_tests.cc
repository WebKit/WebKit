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

#include "api/test/test_dependency_factory.h"
#include "media/base/vp9_profile.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/flags.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "video/video_quality_test.h"

namespace webrtc {
namespace flags {

WEBRTC_DEFINE_string(rtc_event_log_name,
                     "",
                     "Filename for rtc event log. Two files "
                     "with \"_send\" and \"_recv\" suffixes will be created.");
std::string RtcEventLogName() {
  return static_cast<std::string>(FLAG_rtc_event_log_name);
}
WEBRTC_DEFINE_string(rtp_dump_name,
                     "",
                     "Filename for dumped received RTP stream.");
std::string RtpDumpName() {
  return static_cast<std::string>(FLAG_rtp_dump_name);
}
WEBRTC_DEFINE_string(
    encoded_frame_path,
    "",
    "The base path for encoded frame logs. Created files will have "
    "the form <encoded_frame_path>.<n>.(recv|send.<m>).ivf");
std::string EncodedFramePath() {
  return static_cast<std::string>(FLAG_encoded_frame_path);
}
}  // namespace flags
}  // namespace webrtc

namespace webrtc {

namespace {
static const int kFullStackTestDurationSecs = 45;
const char kPacerPushBackExperiment[] =
    "WebRTC-PacerPushbackExperiment/Enabled/";
const char kVp8TrustedRateControllerFieldTrial[] =
    "WebRTC-LibvpxVp8TrustedRateController/Enabled/";

struct ParamsWithLogging : public VideoQualityTest::Params {
 public:
  ParamsWithLogging() {
    // Use these logging flags by default, for everything.
    logging = {flags::RtcEventLogName(), flags::RtpDumpName(),
               flags::EncodedFramePath()};
    this->config = BuiltInNetworkBehaviorConfig();
  }
};

std::unique_ptr<VideoQualityTestFixtureInterface>
CreateVideoQualityTestFixture() {
  // The components will normally be nullptr (= use defaults), but it's possible
  // for external test runners to override the list of injected components.
  auto components = TestDependencyFactory::GetInstance().CreateComponents();
  return absl::make_unique<VideoQualityTest>(std::move(components));
}

// Takes the current active field trials set, and appends some new trials.
std::string AppendFieldTrials(std::string new_trial_string) {
  return std::string(field_trial::GetFieldTrialString()) + new_trial_string;
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

class GenericDescriptorTest : public ::testing::TestWithParam<std::string> {
 public:
  GenericDescriptorTest()
      : field_trial_(GetParam()),
        generic_descriptor_enabled_(
            field_trial::IsEnabled("WebRTC-GenericDescriptor")) {}

  std::string GetTestName(std::string base) {
    if (generic_descriptor_enabled_)
      base += "_generic_descriptor";
    return base;
  }

  bool GenericDescriptorEnabled() const { return generic_descriptor_enabled_; }

 private:
  test::ScopedFieldTrials field_trial_;
  bool generic_descriptor_enabled_;
};

#if defined(RTC_ENABLE_VP9)
TEST(FullStackTest, ForemanCifWithoutPacketLossVp9) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,    288,   30,    700000,
                          700000, 700000, false, "VP9", 1,
                          0,      0,      false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_net_delay_0_0_plr_0_VP9", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST_P(GenericDescriptorTest, ForemanCifPlr5Vp9) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP9", 1,
                          0,      0,       false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {GetTestName("foreman_cif_delay_50_0_plr_5_VP9"), 0.0,
                          0.0, kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  foreman_cif.call.generic_descriptor = GenericDescriptorEnabled();
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, GeneratorWithoutPacketLossVp9Profile2) {
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
  auto fixture = CreateVideoQualityTestFixture();

  SdpVideoFormat::Parameters vp92 = {
      {kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile2)}};
  ParamsWithLogging generator;
  generator.call.send_side_bwe = true;
  generator.video[0] = {
      true, 352, 288, 30,    700000, 700000, 700000,          false, "VP9",
      1,    0,   0,   false, false,  false,  "GeneratorI010", 0,     vp92};
  generator.analyzer = {"generator_net_delay_0_0_plr_0_VP9Profile2", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  fixture->RunWithAnalyzer(generator);
}

TEST(FullStackTest, ForemanCifWithoutPacketLossMultiplexI420Frame) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,    288,   30,          700000,
                          700000, 700000, false, "multiplex", 1,
                          0,      0,      false, false,       false,
                          "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_net_delay_0_0_plr_0_Multiplex", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, GeneratorWithoutPacketLossMultiplexI420AFrame) {
  auto fixture = CreateVideoQualityTestFixture();

  ParamsWithLogging generator;
  generator.call.send_side_bwe = true;
  generator.video[0] = {true,   352,    288,   30,          700000,
                        700000, 700000, false, "multiplex", 1,
                        0,      0,      false, false,       false,
                        "GeneratorI420A"};
  generator.analyzer = {"generator_net_delay_0_0_plr_0_Multiplex", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  fixture->RunWithAnalyzer(generator);
}

#endif  // defined(RTC_ENABLE_VP9)

#if defined(WEBRTC_LINUX)
// Crashes on the linux trusty perf bot: bugs.webrtc.org/9129.
#define MAYBE_ParisQcifWithoutPacketLoss DISABLED_ParisQcifWithoutPacketLoss
#else
#define MAYBE_ParisQcifWithoutPacketLoss ParisQcifWithoutPacketLoss
#endif
TEST(FullStackTest, MAYBE_ParisQcifWithoutPacketLoss) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging paris_qcif;
  paris_qcif.call.send_side_bwe = true;
  paris_qcif.video[0] = {true,   176,    144,   30,    300000,
                         300000, 300000, false, "VP8", 1,
                         0,      0,      false, false, false, "paris_qcif"};
  paris_qcif.analyzer = {"net_delay_0_0_plr_0", 36.0, 0.96,
                         kFullStackTestDurationSecs};
  fixture->RunWithAnalyzer(paris_qcif);
}

TEST_P(GenericDescriptorTest, ForemanCifWithoutPacketLoss) {
  auto fixture = CreateVideoQualityTestFixture();
  // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,    288,   30,    700000,
                          700000, 700000, false, "VP8", 1,
                          0,      0,      false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {GetTestName("foreman_cif_net_delay_0_0_plr_0"), 0.0,
                          0.0, kFullStackTestDurationSecs};
  foreman_cif.call.generic_descriptor = GenericDescriptorEnabled();
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST_P(GenericDescriptorTest, ForemanCif30kbpsWithoutPacketLoss) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,  352,   288,   10,    30000,
                          30000, 30000, false, "VP8", 1,
                          0,     0,     false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {GetTestName("foreman_cif_30kbps_net_delay_0_0_plr_0"),
                          0.0, 0.0, kFullStackTestDurationSecs};
  foreman_cif.call.generic_descriptor = GenericDescriptorEnabled();
  fixture->RunWithAnalyzer(foreman_cif);
}

// TODO(webrtc:9722): Remove when experiment is cleaned up.
TEST_P(GenericDescriptorTest,
       ForemanCif30kbpsWithoutPacketLossTrustedRateControl) {
  test::ScopedFieldTrials override_field_trials(
      AppendFieldTrials(kVp8TrustedRateControllerFieldTrial));
  auto fixture = CreateVideoQualityTestFixture();

  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,  352,          288, 10, 30000, 30000, 30000,
                          false, "VP8",        1,   0,  0,     false, false,
                          false, "foreman_cif"};
  foreman_cif.analyzer = {
      GetTestName("foreman_cif_30kbps_net_delay_0_0_plr_0_trusted_rate_ctrl"),
      0.0, 0.0, kFullStackTestDurationSecs};
  foreman_cif.call.generic_descriptor = GenericDescriptorEnabled();
  fixture->RunWithAnalyzer(foreman_cif);
}

// Link capacity below default start rate. Automatic down scaling enabled.
TEST(FullStackTest, ForemanCifLink150kbpsWithoutPacketLoss) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       false,  false, true, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_link_150kbps_net_delay_0_0_plr_0",
                          0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.config->link_capacity_kbps = 150;
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST_P(GenericDescriptorTest, ForemanCifPlr5) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {GetTestName("foreman_cif_delay_50_0_plr_5"), 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  foreman_cif.call.generic_descriptor = GenericDescriptorEnabled();
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST_P(GenericDescriptorTest, ForemanCifPlr5Ulpfec) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       true,  false, false, "foreman_cif"};
  foreman_cif.analyzer = {GetTestName("foreman_cif_delay_50_0_plr_5_ulpfec"),
                          0.0, 0.0, kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  foreman_cif.call.generic_descriptor = GenericDescriptorEnabled();
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, ForemanCifPlr5Flexfec) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       false, true,  false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_flexfec", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, ForemanCif500kbpsPlr3Flexfec) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       false, true,  false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_delay_50_0_plr_3_flexfec", 0.0,
                          0.0, kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 3;
  foreman_cif.config->link_capacity_kbps = 500;
  foreman_cif.config->queue_delay_ms = 50;
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, ForemanCif500kbpsPlr3Ulpfec) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       true,  false, false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_delay_50_0_plr_3_ulpfec", 0.0,
                          0.0, kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 3;
  foreman_cif.config->link_capacity_kbps = 500;
  foreman_cif.config->queue_delay_ms = 50;
  fixture->RunWithAnalyzer(foreman_cif);
}

#if defined(WEBRTC_USE_H264)
TEST(FullStackTest, ForemanCifWithoutPacketlossH264) {
  auto fixture = CreateVideoQualityTestFixture();
  // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,    288,   30,     700000,
                          700000, 700000, false, "H264", 1,
                          0,      0,      false, false,  false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_net_delay_0_0_plr_0_H264", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, ForemanCif30kbpsWithoutPacketlossH264) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,  352,   288,   10,     30000,
                          30000, 30000, false, "H264", 1,
                          0,     0,     false, false,  false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_30kbps_net_delay_0_0_plr_0_H264", 0.0,
                          0.0, kFullStackTestDurationSecs};
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST_P(GenericDescriptorTest, ForemanCifPlr5H264) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,     30000,
                          500000, 2000000, false, "H264", 1,
                          0,      0,       false, false,  false, "foreman_cif"};
  foreman_cif.analyzer = {GetTestName("foreman_cif_delay_50_0_plr_5_H264"), 0.0,
                          0.0, kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  foreman_cif.call.generic_descriptor = GenericDescriptorEnabled();
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, ForemanCifPlr5H264SpsPpsIdrIsKeyframe) {
  test::ScopedFieldTrials override_field_trials(
      AppendFieldTrials("WebRTC-SpsPpsIdrIsH264Keyframe/Enabled/"));
  auto fixture = CreateVideoQualityTestFixture();

  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,     30000,
                          500000, 2000000, false, "H264", 1,
                          0,      0,       false, false,  false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_H264_sps_pps_idr", 0.0,
                          0.0, kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  fixture->RunWithAnalyzer(foreman_cif);
}

// Verify that this is worth the bot time, before enabling.
TEST(FullStackTest, ForemanCifPlr5H264Flexfec) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,     30000,
                          500000, 2000000, false, "H264", 1,
                          0,      0,       false, true,   false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_H264_flexfec", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  fixture->RunWithAnalyzer(foreman_cif);
}

// Ulpfec with H264 is an unsupported combination, so this test is only useful
// for debugging. It is therefore disabled by default.
TEST(FullStackTest, DISABLED_ForemanCifPlr5H264Ulpfec) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,     30000,
                          500000, 2000000, false, "H264", 1,
                          0,      0,       true,  false,  false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_delay_50_0_plr_5_H264_ulpfec", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.config->loss_percent = 5;
  foreman_cif.config->queue_delay_ms = 50;
  fixture->RunWithAnalyzer(foreman_cif);
}
#endif  // defined(WEBRTC_USE_H264)

TEST(FullStackTest, ForemanCif500kbps) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 0;
  foreman_cif.config->queue_delay_ms = 0;
  foreman_cif.config->link_capacity_kbps = 500;
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, ForemanCif500kbpsLimitedQueue) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_32pkts_queue", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 32;
  foreman_cif.config->queue_delay_ms = 0;
  foreman_cif.config->link_capacity_kbps = 500;
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, ForemanCif500kbps100ms) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_100ms", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 0;
  foreman_cif.config->queue_delay_ms = 100;
  foreman_cif.config->link_capacity_kbps = 500;
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST_P(GenericDescriptorTest, ForemanCif500kbps100msLimitedQueue) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {GetTestName("foreman_cif_500kbps_100ms_32pkts_queue"),
                          0.0, 0.0, kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 32;
  foreman_cif.config->queue_delay_ms = 100;
  foreman_cif.config->link_capacity_kbps = 500;
  foreman_cif.call.generic_descriptor = GenericDescriptorEnabled();
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, ForemanCif500kbps100msLimitedQueueRecvBwe) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = false;
  foreman_cif.video[0] = {true,   352,     288,   30,    30000,
                          500000, 2000000, false, "VP8", 1,
                          0,      0,       false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_500kbps_100ms_32pkts_queue_recv_bwe",
                          0.0, 0.0, kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 32;
  foreman_cif.config->queue_delay_ms = 100;
  foreman_cif.config->link_capacity_kbps = 500;
  fixture->RunWithAnalyzer(foreman_cif);
}

TEST(FullStackTest, ForemanCif1000kbps100msLimitedQueue) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging foreman_cif;
  foreman_cif.call.send_side_bwe = true;
  foreman_cif.video[0] = {true,    352,     288,   30,    30000,
                          2000000, 2000000, false, "VP8", 1,
                          0,       0,       false, false, false, "foreman_cif"};
  foreman_cif.analyzer = {"foreman_cif_1000kbps_100ms_32pkts_queue", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  foreman_cif.config->queue_length_packets = 32;
  foreman_cif.config->queue_delay_ms = 100;
  foreman_cif.config->link_capacity_kbps = 1000;
  fixture->RunWithAnalyzer(foreman_cif);
}

// TODO(sprang): Remove this if we have the similar ModerateLimits below?
TEST(FullStackTest, ConferenceMotionHd2000kbps100msLimitedQueue) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      true,    1280,    720,   50,    30000,
      3000000, 3000000, false, "VP8", 1,
      0,       0,       false, false, false, "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {"conference_motion_hd_2000kbps_100ms_32pkts_queue",
                             0.0, 0.0, kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 32;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity_kbps = 2000;
  fixture->RunWithAnalyzer(conf_motion_hd);
}

// TODO(webrtc:9722): Remove when experiment is cleaned up.
TEST(FullStackTest, ConferenceMotionHd1TLModerateLimitsWhitelistVp8) {
  test::ScopedFieldTrials override_field_trials(
      AppendFieldTrials(kVp8TrustedRateControllerFieldTrial));
  auto fixture = CreateVideoQualityTestFixture();

  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      true,    1280,    720,   50,    30000,
      3000000, 3000000, false, "VP8", 1,
      -1,      0,       false, false, false, "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {
      "conference_motion_hd_1tl_moderate_limits_trusted_rate_ctrl", 0.0, 0.0,
      kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 50;
  conf_motion_hd.config->loss_percent = 3;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity_kbps = 2000;
  fixture->RunWithAnalyzer(conf_motion_hd);
}

TEST_P(GenericDescriptorTest, ConferenceMotionHd2TLModerateLimits) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      true,    1280,    720,   50,    30000,
      3000000, 3000000, false, "VP8", 2,
      -1,      0,       false, false, false, "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {
      GetTestName("conference_motion_hd_2tl_moderate_limits"), 0.0, 0.0,
      kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 50;
  conf_motion_hd.config->loss_percent = 3;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity_kbps = 2000;
  conf_motion_hd.call.generic_descriptor = GenericDescriptorEnabled();
  fixture->RunWithAnalyzer(conf_motion_hd);
}

TEST(FullStackTest, ConferenceMotionHd3TLModerateLimits) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      true,    1280,    720,   50,    30000,
      3000000, 3000000, false, "VP8", 3,
      -1,      0,       false, false, false, "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {"conference_motion_hd_3tl_moderate_limits", 0.0,
                             0.0, kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 50;
  conf_motion_hd.config->loss_percent = 3;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity_kbps = 2000;
  fixture->RunWithAnalyzer(conf_motion_hd);
}

TEST(FullStackTest, ConferenceMotionHd4TLModerateLimits) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      true,    1280,    720,   50,    30000,
      3000000, 3000000, false, "VP8", 4,
      -1,      0,       false, false, false, "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {"conference_motion_hd_4tl_moderate_limits", 0.0,
                             0.0, kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 50;
  conf_motion_hd.config->loss_percent = 3;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity_kbps = 2000;
  fixture->RunWithAnalyzer(conf_motion_hd);
}

TEST(FullStackTest, ConferenceMotionHd3TLModerateLimitsAltTLPattern) {
  test::ScopedFieldTrials field_trial(
      AppendFieldTrials("WebRTC-UseShortVP8TL3Pattern/Enabled/"));
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      true,  1280,    720,     50,
      30000, 3000000, 3000000, false,
      "VP8", 3,       -1,      0,
      false, false,   false,   "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {"conference_motion_hd_3tl_alt_moderate_limits",
                             0.0, 0.0, kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 50;
  conf_motion_hd.config->loss_percent = 3;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity_kbps = 2000;
  fixture->RunWithAnalyzer(conf_motion_hd);
}

TEST(FullStackTest,
     ConferenceMotionHd3TLModerateLimitsAltTLPatternAndBaseHeavyTLAllocation) {
  auto fixture = CreateVideoQualityTestFixture();
  test::ScopedFieldTrials field_trial(
      AppendFieldTrials("WebRTC-UseShortVP8TL3Pattern/Enabled/"
                        "WebRTC-UseBaseHeavyVP8TL3RateAllocation/Enabled/"));
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      true,    1280,    720,   50,    30000,
      3000000, 3000000, false, "VP8", 3,
      -1,      0,       false, false, false, "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {
      "conference_motion_hd_3tl_alt_heavy_moderate_limits", 0.0, 0.0,
      kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 50;
  conf_motion_hd.config->loss_percent = 3;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity_kbps = 2000;
  fixture->RunWithAnalyzer(conf_motion_hd);
}

#if defined(RTC_ENABLE_VP9)
TEST(FullStackTest, ConferenceMotionHd2000kbps100msLimitedQueueVP9) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging conf_motion_hd;
  conf_motion_hd.call.send_side_bwe = true;
  conf_motion_hd.video[0] = {
      true,    1280,    720,   50,    30000,
      3000000, 3000000, false, "VP9", 1,
      0,       0,       false, false, false, "ConferenceMotion_1280_720_50"};
  conf_motion_hd.analyzer = {
      "conference_motion_hd_2000kbps_100ms_32pkts_queue_vp9", 0.0, 0.0,
      kFullStackTestDurationSecs};
  conf_motion_hd.config->queue_length_packets = 32;
  conf_motion_hd.config->queue_delay_ms = 100;
  conf_motion_hd.config->link_capacity_kbps = 2000;
  fixture->RunWithAnalyzer(conf_motion_hd);
}
#endif

TEST(FullStackTest, ScreenshareSlidesVP8_2TL) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {true,    1850,  1110,  5, 50000, 200000,
                          1000000, false, "VP8", 2, 1,     400000,
                          false,   false, false, ""};
  screenshare.screenshare[0] = {true, false, 10};
  screenshare.analyzer = {"screenshare_slides", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  fixture->RunWithAnalyzer(screenshare);
}

// TODO(bugs.webrtc.org/9840): Investigate why is this test flaky on MAC.
#if !defined(WEBRTC_MAC)
const char kScreenshareSimulcastExperiment[] =
    "WebRTC-SimulcastScreenshare/Enabled/";

TEST(FullStackTest, ScreenshareSlidesVP8_3TL_Simulcast) {
  test::ScopedFieldTrials field_trial(
      AppendFieldTrials(kScreenshareSimulcastExperiment));
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.screenshare[0] = {true, false, 10};
  screenshare.video[0] = {true,    1850,    1110,  5,     800000,
                          2500000, 2500000, false, "VP8", 3,
                          2,       400000,  false, false, false, ""};
  screenshare.analyzer = {"screenshare_slides_simulcast", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  ParamsWithLogging screenshare_params_high;
  screenshare_params_high.video[0] = {true,    1850,  1110,  5, 400000, 1000000,
                                      1000000, false, "VP8", 3, 0,      400000,
                                      false,   false, false, ""};
  VideoQualityTest::Params screenshare_params_low;
  screenshare_params_low.video[0] = {true,    1850,  1110,  5, 50000, 200000,
                                     1000000, false, "VP8", 2, 0,     400000,
                                     false,   false, false, ""};

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(screenshare_params_low, 0),
      VideoQualityTest::DefaultVideoStream(screenshare_params_high, 0)};
  screenshare.ss[0] = {
      streams, 1, 1, 0, InterLayerPredMode::kOn, std::vector<SpatialLayer>(),
      false};
  fixture->RunWithAnalyzer(screenshare);
}
#endif  // !defined(WEBRTC_MAC)

TEST(FullStackTest, ScreenshareSlidesVP8_2TL_Scroll) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging config;
  config.call.send_side_bwe = true;
  config.video[0] = {true,    1850,  1110 / 2, 5, 50000, 200000,
                     1000000, false, "VP8",    2, 1,     400000,
                     false,   false, false,    ""};
  config.screenshare[0] = {true, false, 10, 2};
  config.analyzer = {"screenshare_slides_scrolling", 0.0, 0.0,
                     kFullStackTestDurationSecs};
  fixture->RunWithAnalyzer(config);
}

TEST_P(GenericDescriptorTest, ScreenshareSlidesVP8_2TL_LossyNet) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {true,    1850,  1110,  5, 50000, 200000,
                          1000000, false, "VP8", 2, 1,     400000,
                          false,   false, false, ""};
  screenshare.screenshare[0] = {true, false, 10};
  screenshare.analyzer = {GetTestName("screenshare_slides_lossy_net"), 0.0, 0.0,
                          kFullStackTestDurationSecs};
  screenshare.config->loss_percent = 5;
  screenshare.config->queue_delay_ms = 200;
  screenshare.config->link_capacity_kbps = 500;
  screenshare.call.generic_descriptor = GenericDescriptorEnabled();
  fixture->RunWithAnalyzer(screenshare);
}

TEST(FullStackTest, ScreenshareSlidesVP8_2TL_VeryLossyNet) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {true,    1850,  1110,  5, 50000, 200000,
                          1000000, false, "VP8", 2, 1,     400000,
                          false,   false, false, ""};
  screenshare.screenshare[0] = {true, false, 10};
  screenshare.analyzer = {"screenshare_slides_very_lossy", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  screenshare.config->loss_percent = 10;
  screenshare.config->queue_delay_ms = 200;
  screenshare.config->link_capacity_kbps = 500;
  fixture->RunWithAnalyzer(screenshare);
}

TEST(FullStackTest, ScreenshareSlidesVP8_2TL_LossyNetRestrictedQueue) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {true,    1850,  1110,  5, 50000, 200000,
                          1000000, false, "VP8", 2, 1,     400000,
                          false,   false, false, ""};
  screenshare.screenshare[0] = {true, false, 10};
  screenshare.analyzer = {"screenshare_slides_lossy_limited", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  screenshare.config->loss_percent = 5;
  screenshare.config->link_capacity_kbps = 200;
  screenshare.config->queue_length_packets = 30;

  fixture->RunWithAnalyzer(screenshare);
}

TEST(FullStackTest, ScreenshareSlidesVP8_2TL_ModeratelyRestricted) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {true,    1850,  1110,  5, 50000, 200000,
                          1000000, false, "VP8", 2, 1,     400000,
                          false,   false, false, ""};
  screenshare.screenshare[0] = {true, false, 10};
  screenshare.analyzer = {"screenshare_slides_moderately_restricted", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  screenshare.config->loss_percent = 1;
  screenshare.config->link_capacity_kbps = 1200;
  screenshare.config->queue_length_packets = 30;

  fixture->RunWithAnalyzer(screenshare);
}

const ParamsWithLogging::Video kSvcVp9Video = {
    true,    1280,    720,   30,    800000,
    2500000, 2500000, false, "VP9", 3,
    2,       400000,  false, false, false, "ConferenceMotion_1280_720_50"};

const ParamsWithLogging::Video kSimulcastVp8VideoHigh = {
    true,    1280,    720,   30,    800000,
    2500000, 2500000, false, "VP8", 3,
    2,       400000,  false, false, false, "ConferenceMotion_1280_720_50"};

const ParamsWithLogging::Video kSimulcastVp8VideoMedium = {
    true,   640,    360,   30,    150000,
    500000, 700000, false, "VP8", 3,
    2,      400000, false, false, false, "ConferenceMotion_1280_720_50"};

const ParamsWithLogging::Video kSimulcastVp8VideoLow = {
    true,   320,    180,   30,    30000,
    150000, 200000, false, "VP8", 3,
    2,      400000, false, false, false, "ConferenceMotion_1280_720_50"};

#if defined(RTC_ENABLE_VP9)
TEST(FullStackTest, ScreenshareSlidesVP9_2SL) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging screenshare;
  screenshare.call.send_side_bwe = true;
  screenshare.video[0] = {true,   1850,    1110,  5,     50000,
                          200000, 2000000, false, "VP9", 1,
                          0,      400000,  false, false, false, ""};
  screenshare.screenshare[0] = {true, false, 10};
  screenshare.analyzer = {"screenshare_slides_vp9_2sl", 0.0, 0.0,
                          kFullStackTestDurationSecs};
  screenshare.ss[0] = {
      std::vector<VideoStream>(),  0,    2, 1, InterLayerPredMode::kOn,
      std::vector<SpatialLayer>(), false};
  fixture->RunWithAnalyzer(screenshare);
}

TEST(FullStackTest, VP9SVC_3SL_High) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSvcVp9Video;
  simulcast.analyzer = {"vp9svc_3sl_high", 0.0, 0.0,
                        kFullStackTestDurationSecs};

  simulcast.ss[0] = {
      std::vector<VideoStream>(),  0,    3, 2, InterLayerPredMode::kOn,
      std::vector<SpatialLayer>(), false};
  fixture->RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, VP9SVC_3SL_Medium) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSvcVp9Video;
  simulcast.analyzer = {"vp9svc_3sl_medium", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.ss[0] = {
      std::vector<VideoStream>(),  0,    3, 1, InterLayerPredMode::kOn,
      std::vector<SpatialLayer>(), false};
  fixture->RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, VP9SVC_3SL_Low) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSvcVp9Video;
  simulcast.analyzer = {"vp9svc_3sl_low", 0.0, 0.0, kFullStackTestDurationSecs};
  simulcast.ss[0] = {
      std::vector<VideoStream>(),  0,    3, 0, InterLayerPredMode::kOn,
      std::vector<SpatialLayer>(), false};
  fixture->RunWithAnalyzer(simulcast);
}

// bugs.webrtc.org/9506
#if !defined(WEBRTC_MAC)

TEST(FullStackTest, VP9KSVC_3SL_High) {
  webrtc::test::ScopedFieldTrials override_trials(
      AppendFieldTrials("WebRTC-Vp9IssueKeyFrameOnLayerDeactivation/Enabled/"));
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSvcVp9Video;
  simulcast.analyzer = {"vp9ksvc_3sl_high", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.ss[0] = {
      std::vector<VideoStream>(),  0,    3, 2, InterLayerPredMode::kOnKeyPic,
      std::vector<SpatialLayer>(), false};
  fixture->RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, VP9KSVC_3SL_Medium) {
  webrtc::test::ScopedFieldTrials override_trials(
      AppendFieldTrials("WebRTC-Vp9IssueKeyFrameOnLayerDeactivation/Enabled/"));
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSvcVp9Video;
  simulcast.analyzer = {"vp9ksvc_3sl_medium", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.ss[0] = {
      std::vector<VideoStream>(),  0,    3, 1, InterLayerPredMode::kOnKeyPic,
      std::vector<SpatialLayer>(), false};
  fixture->RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, VP9KSVC_3SL_Low) {
  webrtc::test::ScopedFieldTrials override_trials(
      AppendFieldTrials("WebRTC-Vp9IssueKeyFrameOnLayerDeactivation/Enabled/"));
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSvcVp9Video;
  simulcast.analyzer = {"vp9ksvc_3sl_low", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.ss[0] = {
      std::vector<VideoStream>(),  0,    3, 0, InterLayerPredMode::kOnKeyPic,
      std::vector<SpatialLayer>(), false};
  fixture->RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, VP9KSVC_3SL_Medium_Network_Restricted) {
  webrtc::test::ScopedFieldTrials override_trials(
      AppendFieldTrials("WebRTC-Vp9IssueKeyFrameOnLayerDeactivation/Enabled/"));
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSvcVp9Video;
  simulcast.analyzer = {"vp9ksvc_3sl_medium_network_restricted", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.ss[0] = {
      std::vector<VideoStream>(),  0,    3, -1, InterLayerPredMode::kOnKeyPic,
      std::vector<SpatialLayer>(), false};
  simulcast.config->link_capacity_kbps = 1000;
  simulcast.config->queue_delay_ms = 100;
  fixture->RunWithAnalyzer(simulcast);
}

// TODO(webrtc:9722): Remove when experiment is cleaned up.
TEST(FullStackTest, VP9KSVC_3SL_Medium_Network_Restricted_Trusted_Rate) {
  webrtc::test::ScopedFieldTrials override_trials(
      AppendFieldTrials("WebRTC-Vp9IssueKeyFrameOnLayerDeactivation/Enabled/"
                        "WebRTC-LibvpxVp9TrustedRateController/Enabled/"));
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSvcVp9Video;
  simulcast.analyzer = {"vp9ksvc_3sl_medium_network_restricted_trusted_rate",
                        0.0, 0.0, kFullStackTestDurationSecs};
  simulcast.ss[0] = {
      std::vector<VideoStream>(),  0,    3, -1, InterLayerPredMode::kOnKeyPic,
      std::vector<SpatialLayer>(), false};
  simulcast.config->link_capacity_kbps = 1000;
  simulcast.config->queue_delay_ms = 100;
  fixture->RunWithAnalyzer(simulcast);
}
#endif  // !defined(WEBRTC_MAC)

#endif  // defined(RTC_ENABLE_VP9)

// Android bots can't handle FullHD, so disable the test.
// TODO(bugs.webrtc.org/9220): Investigate source of flakiness on Mac.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_MAC)
#define MAYBE_SimulcastFullHdOveruse DISABLED_SimulcastFullHdOveruse
#else
#define MAYBE_SimulcastFullHdOveruse SimulcastFullHdOveruse
#endif

TEST(FullStackTest, MAYBE_SimulcastFullHdOveruse) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = {true,    1920,    1080,  30,    800000,
                        2500000, 2500000, false, "VP8", 3,
                        2,       400000,  false, false, false, "Generator"};
  simulcast.analyzer = {"simulcast_HD_high", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.config->loss_percent = 0;
  simulcast.config->queue_delay_ms = 100;
  std::vector<VideoStream> streams = {
    VideoQualityTest::DefaultVideoStream(simulcast, 0),
    VideoQualityTest::DefaultVideoStream(simulcast, 0),
    VideoQualityTest::DefaultVideoStream(simulcast, 0)
  };
  simulcast.ss[0] = {
      streams, 2, 1, 0, InterLayerPredMode::kOn, std::vector<SpatialLayer>(),
      true};
  webrtc::test::ScopedFieldTrials override_trials(AppendFieldTrials(
      "WebRTC-ForceSimulatedOveruseIntervalMs/1000-50000-300/"));
  fixture->RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, SimulcastVP8_3SL_High) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSimulcastVp8VideoHigh;
  simulcast.analyzer = {"simulcast_vp8_3sl_high", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.config->loss_percent = 0;
  simulcast.config->queue_delay_ms = 100;
  ParamsWithLogging video_params_high;
  video_params_high.video[0] = kSimulcastVp8VideoHigh;
  ParamsWithLogging video_params_medium;
  video_params_medium.video[0] = kSimulcastVp8VideoMedium;
  ParamsWithLogging video_params_low;
  video_params_low.video[0] = kSimulcastVp8VideoLow;

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(video_params_low, 0),
      VideoQualityTest::DefaultVideoStream(video_params_medium, 0),
      VideoQualityTest::DefaultVideoStream(video_params_high, 0)};
  simulcast.ss[0] = {
      streams, 2, 1, 0, InterLayerPredMode::kOn, std::vector<SpatialLayer>(),
      false};
  fixture->RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, SimulcastVP8_3SL_Medium) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSimulcastVp8VideoHigh;
  simulcast.analyzer = {"simulcast_vp8_3sl_medium", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.config->loss_percent = 0;
  simulcast.config->queue_delay_ms = 100;
  ParamsWithLogging video_params_high;
  video_params_high.video[0] = kSimulcastVp8VideoHigh;
  ParamsWithLogging video_params_medium;
  video_params_medium.video[0] = kSimulcastVp8VideoMedium;
  ParamsWithLogging video_params_low;
  video_params_low.video[0] = kSimulcastVp8VideoLow;

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(video_params_low, 0),
      VideoQualityTest::DefaultVideoStream(video_params_medium, 0),
      VideoQualityTest::DefaultVideoStream(video_params_high, 0)};
  simulcast.ss[0] = {
      streams, 1, 1, 0, InterLayerPredMode::kOn, std::vector<SpatialLayer>(),
      false};
  fixture->RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, SimulcastVP8_3SL_Low) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging simulcast;
  simulcast.call.send_side_bwe = true;
  simulcast.video[0] = kSimulcastVp8VideoHigh;
  simulcast.analyzer = {"simulcast_vp8_3sl_low", 0.0, 0.0,
                        kFullStackTestDurationSecs};
  simulcast.config->loss_percent = 0;
  simulcast.config->queue_delay_ms = 100;
  ParamsWithLogging video_params_high;
  video_params_high.video[0] = kSimulcastVp8VideoHigh;
  ParamsWithLogging video_params_medium;
  video_params_medium.video[0] = kSimulcastVp8VideoMedium;
  ParamsWithLogging video_params_low;
  video_params_low.video[0] = kSimulcastVp8VideoLow;

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(video_params_low, 0),
      VideoQualityTest::DefaultVideoStream(video_params_medium, 0),
      VideoQualityTest::DefaultVideoStream(video_params_high, 0)};
  simulcast.ss[0] = {
      streams, 0, 1, 0, InterLayerPredMode::kOn, std::vector<SpatialLayer>(),
      false};
  fixture->RunWithAnalyzer(simulcast);
}

TEST(FullStackTest, LargeRoomVP8_5thumb) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging large_room;
  large_room.call.send_side_bwe = true;
  large_room.video[0] = kSimulcastVp8VideoHigh;
  large_room.analyzer = {"largeroom_5thumb", 0.0, 0.0,
                         kFullStackTestDurationSecs};
  large_room.config->loss_percent = 0;
  large_room.config->queue_delay_ms = 100;
  ParamsWithLogging video_params_high;
  video_params_high.video[0] = kSimulcastVp8VideoHigh;
  ParamsWithLogging video_params_medium;
  video_params_medium.video[0] = kSimulcastVp8VideoMedium;
  ParamsWithLogging video_params_low;
  video_params_low.video[0] = kSimulcastVp8VideoLow;

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(video_params_low, 0),
      VideoQualityTest::DefaultVideoStream(video_params_medium, 0),
      VideoQualityTest::DefaultVideoStream(video_params_high, 0)};
  large_room.call.num_thumbnails = 5;
  large_room.ss[0] = {
      streams, 2, 1, 0, InterLayerPredMode::kOn, std::vector<SpatialLayer>(),
      false};
  fixture->RunWithAnalyzer(large_room);
}

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
// Fails on mobile devices:
// https://bugs.chromium.org/p/webrtc/issues/detail?id=7301
#define MAYBE_LargeRoomVP8_50thumb DISABLED_LargeRoomVP8_50thumb
#define MAYBE_LargeRoomVP8_15thumb DISABLED_LargeRoomVP8_15thumb
#else
#define MAYBE_LargeRoomVP8_50thumb LargeRoomVP8_50thumb
#define MAYBE_LargeRoomVP8_15thumb LargeRoomVP8_15thumb
#endif

TEST(FullStackTest, MAYBE_LargeRoomVP8_15thumb) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging large_room;
  large_room.call.send_side_bwe = true;
  large_room.video[0] = kSimulcastVp8VideoHigh;
  large_room.analyzer = {"largeroom_15thumb", 0.0, 0.0,
                         kFullStackTestDurationSecs};
  large_room.config->loss_percent = 0;
  large_room.config->queue_delay_ms = 100;
  ParamsWithLogging video_params_high;
  video_params_high.video[0] = kSimulcastVp8VideoHigh;
  ParamsWithLogging video_params_medium;
  video_params_medium.video[0] = kSimulcastVp8VideoMedium;
  ParamsWithLogging video_params_low;
  video_params_low.video[0] = kSimulcastVp8VideoLow;

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(video_params_low, 0),
      VideoQualityTest::DefaultVideoStream(video_params_medium, 0),
      VideoQualityTest::DefaultVideoStream(video_params_high, 0)};
  large_room.call.num_thumbnails = 15;
  large_room.ss[0] = {
      streams, 2, 1, 0, InterLayerPredMode::kOn, std::vector<SpatialLayer>(),
      false};
  fixture->RunWithAnalyzer(large_room);
}

TEST(FullStackTest, MAYBE_LargeRoomVP8_50thumb) {
  auto fixture = CreateVideoQualityTestFixture();
  ParamsWithLogging large_room;
  large_room.call.send_side_bwe = true;
  large_room.video[0] = kSimulcastVp8VideoHigh;
  large_room.analyzer = {"largeroom_50thumb", 0.0, 0.0,
                         kFullStackTestDurationSecs};
  large_room.config->loss_percent = 0;
  large_room.config->queue_delay_ms = 100;
  ParamsWithLogging video_params_high;
  video_params_high.video[0] = kSimulcastVp8VideoHigh;
  ParamsWithLogging video_params_medium;
  video_params_medium.video[0] = kSimulcastVp8VideoMedium;
  ParamsWithLogging video_params_low;
  video_params_low.video[0] = kSimulcastVp8VideoLow;

  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(video_params_low, 0),
      VideoQualityTest::DefaultVideoStream(video_params_medium, 0),
      VideoQualityTest::DefaultVideoStream(video_params_high, 0)};
  large_room.call.num_thumbnails = 50;
  large_room.ss[0] = {
      streams, 2, 1, 0, InterLayerPredMode::kOn, std::vector<SpatialLayer>(),
      false};
  fixture->RunWithAnalyzer(large_room);
}

INSTANTIATE_TEST_CASE_P(FullStackTest,
                        GenericDescriptorTest,
                        ::testing::Values("WebRTC-GenericDescriptor/Disabled/",
                                          "WebRTC-GenericDescriptor/Enabled/"));

class DualStreamsTest : public ::testing::TestWithParam<int> {};

// Disable dual video test on mobile device becuase it's too heavy.
// TODO(bugs.webrtc.org/9840): Investigate why is this test flaky on MAC.
#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS) && !defined(WEBRTC_MAC)
TEST_P(DualStreamsTest,
       ModeratelyRestricted_SlidesVp8_3TL_Simulcast_Video_Simulcast_High) {
  test::ScopedFieldTrials field_trial(
      AppendFieldTrials(std::string(kPacerPushBackExperiment) +
                        std::string(kScreenshareSimulcastExperiment)));
  const int first_stream = GetParam();
  ParamsWithLogging dual_streams;

  // Screenshare Settings.
  dual_streams.screenshare[first_stream] = {true, false, 10};
  dual_streams.video[first_stream] = {true,    1850,    1110,  5,     800000,
                                      2500000, 2500000, false, "VP8", 3,
                                      2,       400000,  false, false, false,
                                      ""};

  ParamsWithLogging screenshare_params_high;
  screenshare_params_high.video[0] = {true,    1850,  1110,  5, 400000, 1000000,
                                      1000000, false, "VP8", 3, 0,      400000,
                                      false,   false, false, ""};
  VideoQualityTest::Params screenshare_params_low;
  screenshare_params_low.video[0] = {true,    1850,  1110,  5, 50000, 200000,
                                     1000000, false, "VP8", 2, 0,     400000,
                                     false,   false, false, ""};
  std::vector<VideoStream> screenhsare_streams = {
      VideoQualityTest::DefaultVideoStream(screenshare_params_low, 0),
      VideoQualityTest::DefaultVideoStream(screenshare_params_high, 0)};

  dual_streams.ss[first_stream] = {
      screenhsare_streams,         1,    1, 0, InterLayerPredMode::kOn,
      std::vector<SpatialLayer>(), false};

  // Video settings.
  dual_streams.video[1 - first_stream] = kSimulcastVp8VideoHigh;

  ParamsWithLogging video_params_high;
  video_params_high.video[0] = kSimulcastVp8VideoHigh;
  ParamsWithLogging video_params_medium;
  video_params_medium.video[0] = kSimulcastVp8VideoMedium;
  ParamsWithLogging video_params_low;
  video_params_low.video[0] = kSimulcastVp8VideoLow;
  std::vector<VideoStream> streams = {
      VideoQualityTest::DefaultVideoStream(video_params_low, 0),
      VideoQualityTest::DefaultVideoStream(video_params_medium, 0),
      VideoQualityTest::DefaultVideoStream(video_params_high, 0)};

  dual_streams.ss[1 - first_stream] = {
      streams, 2, 1, 0, InterLayerPredMode::kOn, std::vector<SpatialLayer>(),
      false};

  // Call settings.
  dual_streams.call.send_side_bwe = true;
  dual_streams.call.dual_video = true;
  std::string test_label = "dualstreams_moderately_restricted_screenshare_" +
                           std::to_string(first_stream);
  dual_streams.analyzer = {test_label, 0.0, 0.0, kFullStackTestDurationSecs};
  dual_streams.config->loss_percent = 1;
  dual_streams.config->link_capacity_kbps = 7500;
  dual_streams.config->queue_length_packets = 30;
  dual_streams.config->queue_delay_ms = 100;

  auto fixture = CreateVideoQualityTestFixture();
  fixture->RunWithAnalyzer(dual_streams);
}
#endif  // !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS) &&
        // !defined(WEBRTC_MAC)

TEST_P(DualStreamsTest, Conference_Restricted) {
  test::ScopedFieldTrials field_trial(
      AppendFieldTrials(std::string(kPacerPushBackExperiment)));
  const int first_stream = GetParam();
  ParamsWithLogging dual_streams;

  // Screenshare Settings.
  dual_streams.screenshare[first_stream] = {true, false, 10};
  dual_streams.video[first_stream] = {true,    1850,    1110,  5,     800000,
                                      2500000, 2500000, false, "VP8", 3,
                                      2,       400000,  false, false, false,
                                      ""};
  // Video settings.
  dual_streams.video[1 - first_stream] = {
      true,   1280,   720,   30,    150000,
      500000, 700000, false, "VP8", 3,
      2,      400000, false, false, false, "ConferenceMotion_1280_720_50"};

  // Call settings.
  dual_streams.call.send_side_bwe = true;
  dual_streams.call.dual_video = true;
  std::string test_label = "dualstreams_conference_restricted_screenshare_" +
                           std::to_string(first_stream);
  dual_streams.analyzer = {test_label, 0.0, 0.0, kFullStackTestDurationSecs};
  dual_streams.config->loss_percent = 1;
  dual_streams.config->link_capacity_kbps = 5000;
  dual_streams.config->queue_length_packets = 30;
  dual_streams.config->queue_delay_ms = 100;

  auto fixture = CreateVideoQualityTestFixture();
  fixture->RunWithAnalyzer(dual_streams);
}

INSTANTIATE_TEST_CASE_P(FullStackTest,
                        DualStreamsTest,
                        ::testing::Values(0, 1));

}  // namespace webrtc
