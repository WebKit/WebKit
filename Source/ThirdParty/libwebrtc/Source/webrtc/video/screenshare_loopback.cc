/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "rtc_base/flags.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/run_test.h"
#include "video/video_quality_test.h"

namespace webrtc {
namespace flags {

// Flags common with video loopback, with different default values.
WEBRTC_DEFINE_int(width, 1850, "Video width (crops source).");
size_t Width() {
  return static_cast<size_t>(FLAG_width);
}

WEBRTC_DEFINE_int(height, 1110, "Video height (crops source).");
size_t Height() {
  return static_cast<size_t>(FLAG_height);
}

WEBRTC_DEFINE_int(fps, 5, "Frames per second.");
int Fps() {
  return static_cast<int>(FLAG_fps);
}

WEBRTC_DEFINE_int(min_bitrate, 50, "Call and stream min bitrate in kbps.");
int MinBitrateKbps() {
  return static_cast<int>(FLAG_min_bitrate);
}

WEBRTC_DEFINE_int(start_bitrate, 300, "Call start bitrate in kbps.");
int StartBitrateKbps() {
  return static_cast<int>(FLAG_start_bitrate);
}

WEBRTC_DEFINE_int(target_bitrate, 200, "Stream target bitrate in kbps.");
int TargetBitrateKbps() {
  return static_cast<int>(FLAG_target_bitrate);
}

WEBRTC_DEFINE_int(max_bitrate, 1000, "Call and stream max bitrate in kbps.");
int MaxBitrateKbps() {
  return static_cast<int>(FLAG_max_bitrate);
}

WEBRTC_DEFINE_int(num_temporal_layers, 2, "Number of temporal layers to use.");
int NumTemporalLayers() {
  return static_cast<int>(FLAG_num_temporal_layers);
}

// Flags common with video loopback, with equal default values.
WEBRTC_DEFINE_string(codec, "VP8", "Video codec to use.");
std::string Codec() {
  return static_cast<std::string>(FLAG_codec);
}

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

WEBRTC_DEFINE_int(
    selected_tl,
    -1,
    "Temporal layer to show or analyze. -1 to disable filtering.");
int SelectedTL() {
  return static_cast<int>(FLAG_selected_tl);
}

WEBRTC_DEFINE_int(
    duration,
    0,
    "Duration of the test in seconds. If 0, rendered will be shown instead.");
int DurationSecs() {
  return static_cast<int>(FLAG_duration);
}

WEBRTC_DEFINE_string(output_filename, "", "Target graph data filename.");
std::string OutputFilename() {
  return static_cast<std::string>(FLAG_output_filename);
}

WEBRTC_DEFINE_string(graph_title,
                     "",
                     "If empty, title will be generated automatically.");
std::string GraphTitle() {
  return static_cast<std::string>(FLAG_graph_title);
}

WEBRTC_DEFINE_int(loss_percent, 0, "Percentage of packets randomly lost.");
int LossPercent() {
  return static_cast<int>(FLAG_loss_percent);
}

WEBRTC_DEFINE_int(link_capacity,
                  0,
                  "Capacity (kbps) of the fake link. 0 means infinite.");
int LinkCapacityKbps() {
  return static_cast<int>(FLAG_link_capacity);
}

WEBRTC_DEFINE_int(queue_size,
                  0,
                  "Size of the bottleneck link queue in packets.");
int QueueSize() {
  return static_cast<int>(FLAG_queue_size);
}

WEBRTC_DEFINE_int(avg_propagation_delay_ms,
                  0,
                  "Average link propagation delay in ms.");
int AvgPropagationDelayMs() {
  return static_cast<int>(FLAG_avg_propagation_delay_ms);
}

WEBRTC_DEFINE_int(std_propagation_delay_ms,
                  0,
                  "Link propagation delay standard deviation in ms.");
int StdPropagationDelayMs() {
  return static_cast<int>(FLAG_std_propagation_delay_ms);
}

WEBRTC_DEFINE_int(num_streams, 0, "Number of streams to show or analyze.");
int NumStreams() {
  return static_cast<int>(FLAG_num_streams);
}

WEBRTC_DEFINE_int(selected_stream,
                  0,
                  "ID of the stream to show or analyze. "
                  "Set to the number of streams to show them all.");
int SelectedStream() {
  return static_cast<int>(FLAG_selected_stream);
}

WEBRTC_DEFINE_int(num_spatial_layers, 1, "Number of spatial layers to use.");
int NumSpatialLayers() {
  return static_cast<int>(FLAG_num_spatial_layers);
}

WEBRTC_DEFINE_int(
    inter_layer_pred,
    0,
    "Inter-layer prediction mode. "
    "0 - enabled, 1 - disabled, 2 - enabled only for key pictures.");
InterLayerPredMode InterLayerPred() {
  if (FLAG_inter_layer_pred == 0) {
    return InterLayerPredMode::kOn;
  } else if (FLAG_inter_layer_pred == 1) {
    return InterLayerPredMode::kOff;
  } else {
    RTC_DCHECK_EQ(FLAG_inter_layer_pred, 2);
    return InterLayerPredMode::kOnKeyPic;
  }
}

WEBRTC_DEFINE_int(selected_sl,
                  -1,
                  "Spatial layer to show or analyze. -1 to disable filtering.");
int SelectedSL() {
  return static_cast<int>(FLAG_selected_sl);
}

WEBRTC_DEFINE_string(
    stream0,
    "",
    "Comma separated values describing VideoStream for stream #0.");
std::string Stream0() {
  return static_cast<std::string>(FLAG_stream0);
}

WEBRTC_DEFINE_string(
    stream1,
    "",
    "Comma separated values describing VideoStream for stream #1.");
std::string Stream1() {
  return static_cast<std::string>(FLAG_stream1);
}

WEBRTC_DEFINE_string(
    sl0,
    "",
    "Comma separated values describing SpatialLayer for layer #0.");
std::string SL0() {
  return static_cast<std::string>(FLAG_sl0);
}

WEBRTC_DEFINE_string(
    sl1,
    "",
    "Comma separated values describing SpatialLayer for layer #1.");
std::string SL1() {
  return static_cast<std::string>(FLAG_sl1);
}

WEBRTC_DEFINE_string(
    encoded_frame_path,
    "",
    "The base path for encoded frame logs. Created files will have "
    "the form <encoded_frame_path>.<n>.(recv|send.<m>).ivf");
std::string EncodedFramePath() {
  return static_cast<std::string>(FLAG_encoded_frame_path);
}

WEBRTC_DEFINE_bool(logs, false, "print logs to stderr");

WEBRTC_DEFINE_bool(send_side_bwe, true, "Use send-side bandwidth estimation");

WEBRTC_DEFINE_bool(generic_descriptor,
                   false,
                   "Use the generic frame descriptor.");

WEBRTC_DEFINE_bool(allow_reordering, false, "Allow packet reordering to occur");

WEBRTC_DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");

// Screenshare-specific flags.
WEBRTC_DEFINE_int(min_transmit_bitrate,
                  400,
                  "Min transmit bitrate incl. padding.");
int MinTransmitBitrateKbps() {
  return FLAG_min_transmit_bitrate;
}

WEBRTC_DEFINE_bool(
    generate_slides,
    false,
    "Whether to use randomly generated slides or read them from files.");
bool GenerateSlides() {
  return static_cast<int>(FLAG_generate_slides);
}

WEBRTC_DEFINE_int(slide_change_interval,
                  10,
                  "Interval (in seconds) between simulated slide changes.");
int SlideChangeInterval() {
  return static_cast<int>(FLAG_slide_change_interval);
}

WEBRTC_DEFINE_int(
    scroll_duration,
    0,
    "Duration (in seconds) during which a slide will be scrolled into place.");
int ScrollDuration() {
  return static_cast<int>(FLAG_scroll_duration);
}

WEBRTC_DEFINE_string(
    slides,
    "",
    "Comma-separated list of *.yuv files to display as slides.");
std::vector<std::string> Slides() {
  std::vector<std::string> slides;
  std::string slides_list = FLAG_slides;
  rtc::tokenize(slides_list, ',', &slides);
  return slides;
}

WEBRTC_DEFINE_bool(help, false, "prints this message");

}  // namespace flags

void Loopback() {
  BuiltInNetworkBehaviorConfig pipe_config;
  pipe_config.loss_percent = flags::LossPercent();
  pipe_config.link_capacity_kbps = flags::LinkCapacityKbps();
  pipe_config.queue_length_packets = flags::QueueSize();
  pipe_config.queue_delay_ms = flags::AvgPropagationDelayMs();
  pipe_config.delay_standard_deviation_ms = flags::StdPropagationDelayMs();
  pipe_config.allow_reordering = flags::FLAG_allow_reordering;

  BitrateConstraints call_bitrate_config;
  call_bitrate_config.min_bitrate_bps = flags::MinBitrateKbps() * 1000;
  call_bitrate_config.start_bitrate_bps = flags::StartBitrateKbps() * 1000;
  call_bitrate_config.max_bitrate_bps = -1;  // Don't cap bandwidth estimate.

  VideoQualityTest::Params params;
  params.call = {flags::FLAG_send_side_bwe, flags::FLAG_generic_descriptor,
                 call_bitrate_config};
  params.video[0] = {true,
                     flags::Width(),
                     flags::Height(),
                     flags::Fps(),
                     flags::MinBitrateKbps() * 1000,
                     flags::TargetBitrateKbps() * 1000,
                     flags::MaxBitrateKbps() * 1000,
                     false,
                     flags::Codec(),
                     flags::NumTemporalLayers(),
                     flags::SelectedTL(),
                     flags::MinTransmitBitrateKbps() * 1000,
                     false,  // ULPFEC disabled.
                     false,  // FlexFEC disabled.
                     false,  // Automatic scaling disabled.
                     ""};
  params.screenshare[0] = {true, flags::GenerateSlides(),
                           flags::SlideChangeInterval(),
                           flags::ScrollDuration(), flags::Slides()};
  params.analyzer = {"screenshare",
                     0.0,
                     0.0,
                     flags::DurationSecs(),
                     flags::OutputFilename(),
                     flags::GraphTitle()};
  params.config = pipe_config;
  params.logging = {flags::RtcEventLogName(), flags::RtpDumpName(),
                    flags::EncodedFramePath()};

  if (flags::NumStreams() > 1 && flags::Stream0().empty() &&
      flags::Stream1().empty()) {
    params.ss[0].infer_streams = true;
  }

  std::vector<std::string> stream_descriptors;
  stream_descriptors.push_back(flags::Stream0());
  stream_descriptors.push_back(flags::Stream1());
  std::vector<std::string> SL_descriptors;
  SL_descriptors.push_back(flags::SL0());
  SL_descriptors.push_back(flags::SL1());
  VideoQualityTest::FillScalabilitySettings(
      &params, 0, stream_descriptors, flags::NumStreams(),
      flags::SelectedStream(), flags::NumSpatialLayers(), flags::SelectedSL(),
      flags::InterLayerPred(), SL_descriptors);

  auto fixture = absl::make_unique<VideoQualityTest>(nullptr);
  if (flags::DurationSecs()) {
    fixture->RunWithAnalyzer(params);
  } else {
    fixture->RunWithRenderers(params);
  }
}
}  // namespace webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (webrtc::flags::FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  rtc::LogMessage::SetLogToStderr(webrtc::flags::FLAG_logs);

  webrtc::test::ValidateFieldTrialsStringOrDie(
      webrtc::flags::FLAG_force_fieldtrials);
  // InitFieldTrialsFromString stores the char*, so the char array must outlive
  // the application.
  webrtc::field_trial::InitFieldTrialsFromString(
      webrtc::flags::FLAG_force_fieldtrials);

  webrtc::test::RunTest(webrtc::Loopback);
  return 0;
}
