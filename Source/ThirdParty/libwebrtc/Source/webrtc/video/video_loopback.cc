/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "video/video_loopback.h"

#include <stdio.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/types/optional.h"
#include "api/test/simulated_network.h"
#include "api/test/video_quality_test_fixture.h"
#include "api/transport/bitrate_settings.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/run_test.h"
#include "video/video_quality_test.h"

// Flags common with screenshare loopback, with different default values.
ABSL_FLAG(int, width, 640, "Video width.");

ABSL_FLAG(int, height, 480, "Video height.");

ABSL_FLAG(int, fps, 30, "Frames per second.");

ABSL_FLAG(int, capture_device_index, 0, "Capture device to select");

ABSL_FLAG(int, min_bitrate, 50, "Call and stream min bitrate in kbps.");

ABSL_FLAG(int, start_bitrate, 300, "Call start bitrate in kbps.");

ABSL_FLAG(int, target_bitrate, 800, "Stream target bitrate in kbps.");

ABSL_FLAG(int, max_bitrate, 800, "Call and stream max bitrate in kbps.");

ABSL_FLAG(bool,
          suspend_below_min_bitrate,
          false,
          "Suspends video below the configured min bitrate.");

ABSL_FLAG(int,
          num_temporal_layers,
          1,
          "Number of temporal layers. Set to 1-4 to override.");

ABSL_FLAG(int,
          inter_layer_pred,
          2,
          "Inter-layer prediction mode. "
          "0 - enabled, 1 - disabled, 2 - enabled only for key pictures.");

// Flags common with screenshare loopback, with equal default values.
ABSL_FLAG(std::string, codec, "VP8", "Video codec to use.");

ABSL_FLAG(int,
          selected_tl,
          -1,
          "Temporal layer to show or analyze. -1 to disable filtering.");

ABSL_FLAG(
    int,
    duration,
    0,
    "Duration of the test in seconds. If 0, rendered will be shown instead.");

ABSL_FLAG(std::string, output_filename, "", "Target graph data filename.");

ABSL_FLAG(std::string,
          graph_title,
          "",
          "If empty, title will be generated automatically.");

ABSL_FLAG(int, loss_percent, 0, "Percentage of packets randomly lost.");

ABSL_FLAG(int,
          avg_burst_loss_length,
          -1,
          "Average burst length of lost packets.");

ABSL_FLAG(int,
          link_capacity,
          0,
          "Capacity (kbps) of the fake link. 0 means infinite.");

ABSL_FLAG(int, queue_size, 0, "Size of the bottleneck link queue in packets.");

ABSL_FLAG(int,
          avg_propagation_delay_ms,
          0,
          "Average link propagation delay in ms.");

ABSL_FLAG(std::string,
          rtc_event_log_name,
          "",
          "Filename for rtc event log. Two files "
          "with \"_send\" and \"_recv\" suffixes will be created.");

ABSL_FLAG(std::string,
          rtp_dump_name,
          "",
          "Filename for dumped received RTP stream.");

ABSL_FLAG(int,
          std_propagation_delay_ms,
          0,
          "Link propagation delay standard deviation in ms.");

ABSL_FLAG(int, num_streams, 0, "Number of streams to show or analyze.");

ABSL_FLAG(int,
          selected_stream,
          0,
          "ID of the stream to show or analyze. "
          "Set to the number of streams to show them all.");

ABSL_FLAG(int, num_spatial_layers, 1, "Number of spatial layers to use.");

ABSL_FLAG(int,
          selected_sl,
          -1,
          "Spatial layer to show or analyze. -1 to disable filtering.");

ABSL_FLAG(std::string,
          stream0,
          "",
          "Comma separated values describing VideoStream for stream #0.");

ABSL_FLAG(std::string,
          stream1,
          "",
          "Comma separated values describing VideoStream for stream #1.");

ABSL_FLAG(std::string,
          sl0,
          "",
          "Comma separated values describing SpatialLayer for layer #0.");

ABSL_FLAG(std::string,
          sl1,
          "",
          "Comma separated values describing SpatialLayer for layer #1.");

ABSL_FLAG(std::string,
          sl2,
          "",
          "Comma separated values describing SpatialLayer for layer #2.");

ABSL_FLAG(std::string,
          encoded_frame_path,
          "",
          "The base path for encoded frame logs. Created files will have "
          "the form <encoded_frame_path>.<n>.(recv|send.<m>).ivf");

ABSL_FLAG(bool, logs, false, "print logs to stderr");

ABSL_FLAG(bool, send_side_bwe, true, "Use send-side bandwidth estimation");

ABSL_FLAG(bool, generic_descriptor, false, "Use the generic frame descriptor.");

ABSL_FLAG(bool, allow_reordering, false, "Allow packet reordering to occur");

ABSL_FLAG(bool, use_ulpfec, false, "Use RED+ULPFEC forward error correction.");

ABSL_FLAG(bool, use_flexfec, false, "Use FlexFEC forward error correction.");

ABSL_FLAG(bool, audio, false, "Add audio stream");

ABSL_FLAG(bool,
          use_real_adm,
          false,
          "Use real ADM instead of fake (no effect if audio is false)");

ABSL_FLAG(bool,
          audio_video_sync,
          false,
          "Sync audio and video stream (no effect if"
          " audio is false)");

ABSL_FLAG(bool,
          audio_dtx,
          false,
          "Enable audio DTX (no effect if audio is false)");

ABSL_FLAG(bool, video, true, "Add video stream");

ABSL_FLAG(
    std::string,
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enabled/"
    " will assign the group Enable to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");

// Video-specific flags.
ABSL_FLAG(std::string,
          clip,
          "",
          "Name of the clip to show. If empty, using chroma generator.");

namespace webrtc {
namespace {

size_t Width() {
  return static_cast<size_t>(absl::GetFlag(FLAGS_width));
}

size_t Height() {
  return static_cast<size_t>(absl::GetFlag(FLAGS_height));
}

int Fps() {
  return absl::GetFlag(FLAGS_fps);
}

size_t GetCaptureDevice() {
  return static_cast<size_t>(absl::GetFlag(FLAGS_capture_device_index));
}

int MinBitrateKbps() {
  return absl::GetFlag(FLAGS_min_bitrate);
}

int StartBitrateKbps() {
  return absl::GetFlag(FLAGS_start_bitrate);
}

int TargetBitrateKbps() {
  return absl::GetFlag(FLAGS_target_bitrate);
}

int MaxBitrateKbps() {
  return absl::GetFlag(FLAGS_max_bitrate);
}

int NumTemporalLayers() {
  return absl::GetFlag(FLAGS_num_temporal_layers);
}

InterLayerPredMode InterLayerPred() {
  if (absl::GetFlag(FLAGS_inter_layer_pred) == 0) {
    return InterLayerPredMode::kOn;
  } else if (absl::GetFlag(FLAGS_inter_layer_pred) == 1) {
    return InterLayerPredMode::kOff;
  } else {
    RTC_DCHECK_EQ(absl::GetFlag(FLAGS_inter_layer_pred), 2);
    return InterLayerPredMode::kOnKeyPic;
  }
}

std::string Codec() {
  return absl::GetFlag(FLAGS_codec);
}

int SelectedTL() {
  return absl::GetFlag(FLAGS_selected_tl);
}

int DurationSecs() {
  return absl::GetFlag(FLAGS_duration);
}

std::string OutputFilename() {
  return absl::GetFlag(FLAGS_output_filename);
}

std::string GraphTitle() {
  return absl::GetFlag(FLAGS_graph_title);
}

int LossPercent() {
  return static_cast<int>(absl::GetFlag(FLAGS_loss_percent));
}

int AvgBurstLossLength() {
  return static_cast<int>(absl::GetFlag(FLAGS_avg_burst_loss_length));
}

int LinkCapacityKbps() {
  return static_cast<int>(absl::GetFlag(FLAGS_link_capacity));
}

int QueueSize() {
  return static_cast<int>(absl::GetFlag(FLAGS_queue_size));
}

int AvgPropagationDelayMs() {
  return static_cast<int>(absl::GetFlag(FLAGS_avg_propagation_delay_ms));
}

std::string RtcEventLogName() {
  return absl::GetFlag(FLAGS_rtc_event_log_name);
}

std::string RtpDumpName() {
  return absl::GetFlag(FLAGS_rtp_dump_name);
}

int StdPropagationDelayMs() {
  return absl::GetFlag(FLAGS_std_propagation_delay_ms);
}

int NumStreams() {
  return absl::GetFlag(FLAGS_num_streams);
}

int SelectedStream() {
  return absl::GetFlag(FLAGS_selected_stream);
}

int NumSpatialLayers() {
  return absl::GetFlag(FLAGS_num_spatial_layers);
}

int SelectedSL() {
  return absl::GetFlag(FLAGS_selected_sl);
}

std::string Stream0() {
  return absl::GetFlag(FLAGS_stream0);
}

std::string Stream1() {
  return absl::GetFlag(FLAGS_stream1);
}

std::string SL0() {
  return absl::GetFlag(FLAGS_sl0);
}

std::string SL1() {
  return absl::GetFlag(FLAGS_sl1);
}

std::string SL2() {
  return absl::GetFlag(FLAGS_sl2);
}

std::string EncodedFramePath() {
  return absl::GetFlag(FLAGS_encoded_frame_path);
}

std::string Clip() {
  return absl::GetFlag(FLAGS_clip);
}

}  // namespace

void Loopback() {
  BuiltInNetworkBehaviorConfig pipe_config;
  pipe_config.loss_percent = LossPercent();
  pipe_config.avg_burst_loss_length = AvgBurstLossLength();
  pipe_config.link_capacity_kbps = LinkCapacityKbps();
  pipe_config.queue_length_packets = QueueSize();
  pipe_config.queue_delay_ms = AvgPropagationDelayMs();
  pipe_config.delay_standard_deviation_ms = StdPropagationDelayMs();
  pipe_config.allow_reordering = absl::GetFlag(FLAGS_allow_reordering);

  BitrateConstraints call_bitrate_config;
  call_bitrate_config.min_bitrate_bps = MinBitrateKbps() * 1000;
  call_bitrate_config.start_bitrate_bps = StartBitrateKbps() * 1000;
  call_bitrate_config.max_bitrate_bps = -1;  // Don't cap bandwidth estimate.

  VideoQualityTest::Params params;
  params.call = {absl::GetFlag(FLAGS_send_side_bwe),
                 absl::GetFlag(FLAGS_generic_descriptor), call_bitrate_config,
                 0};
  params.video[0] = {absl::GetFlag(FLAGS_video),
                     Width(),
                     Height(),
                     Fps(),
                     MinBitrateKbps() * 1000,
                     TargetBitrateKbps() * 1000,
                     MaxBitrateKbps() * 1000,
                     absl::GetFlag(FLAGS_suspend_below_min_bitrate),
                     Codec(),
                     NumTemporalLayers(),
                     SelectedTL(),
                     0,  // No min transmit bitrate.
                     absl::GetFlag(FLAGS_use_ulpfec),
                     absl::GetFlag(FLAGS_use_flexfec),
                     NumStreams() < 2,  // Automatic quality scaling.
                     Clip(),
                     GetCaptureDevice()};
  params.audio = {
      absl::GetFlag(FLAGS_audio), absl::GetFlag(FLAGS_audio_video_sync),
      absl::GetFlag(FLAGS_audio_dtx), absl::GetFlag(FLAGS_use_real_adm)};
  params.logging = {RtcEventLogName(), RtpDumpName(), EncodedFramePath()};
  params.screenshare[0].enabled = false;
  params.analyzer = {"video",          0.0,         0.0, DurationSecs(),
                     OutputFilename(), GraphTitle()};
  params.config = pipe_config;

  if (NumStreams() > 1 && Stream0().empty() && Stream1().empty()) {
    params.ss[0].infer_streams = true;
  }

  std::vector<std::string> stream_descriptors;
  stream_descriptors.push_back(Stream0());
  stream_descriptors.push_back(Stream1());
  std::vector<std::string> SL_descriptors;
  SL_descriptors.push_back(SL0());
  SL_descriptors.push_back(SL1());
  SL_descriptors.push_back(SL2());
  VideoQualityTest::FillScalabilitySettings(
      &params, 0, stream_descriptors, NumStreams(), SelectedStream(),
      NumSpatialLayers(), SelectedSL(), InterLayerPred(), SL_descriptors);

  auto fixture = std::make_unique<VideoQualityTest>(nullptr);
  if (DurationSecs()) {
    fixture->RunWithAnalyzer(params);
  } else {
    fixture->RunWithRenderers(params);
  }
}

int RunLoopbackTest(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  absl::ParseCommandLine(argc, argv);

  rtc::LogMessage::SetLogToStderr(absl::GetFlag(FLAGS_logs));

  // InitFieldTrialsFromString stores the char*, so the char array must outlive
  // the application.
  const std::string field_trials = absl::GetFlag(FLAGS_force_fieldtrials);
  webrtc::field_trial::InitFieldTrialsFromString(field_trials.c_str());

  webrtc::test::RunTest(webrtc::Loopback);
  return 0;
}
}  // namespace webrtc
