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
#include "rtc_base/stringencode.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/run_test.h"
#include "video/video_quality_test.h"

namespace webrtc {
namespace flags {

InterLayerPredMode IntToInterLayerPredMode(int inter_layer_pred) {
  if (inter_layer_pred == 0) {
    return InterLayerPredMode::kOn;
  } else if (inter_layer_pred == 1) {
    return InterLayerPredMode::kOff;
  } else {
    RTC_DCHECK_EQ(inter_layer_pred, 2);
    return InterLayerPredMode::kOnKeyPic;
  }
}

// Flags for video.
DEFINE_int(vwidth, 640, "Video width.");
size_t VideoWidth() {
  return static_cast<size_t>(FLAG_vwidth);
}

DEFINE_int(vheight, 480, "Video height.");
size_t VideoHeight() {
  return static_cast<size_t>(FLAG_vheight);
}

DEFINE_int(vfps, 30, "Video frames per second.");
int VideoFps() {
  return static_cast<int>(FLAG_vfps);
}

DEFINE_int(capture_device_index,
           0,
           "Capture device to select for video stream");
size_t GetCaptureDevice() {
  return static_cast<size_t>(FLAG_capture_device_index);
}

DEFINE_int(vtarget_bitrate, 400, "Video stream target bitrate in kbps.");
int VideoTargetBitrateKbps() {
  return static_cast<int>(FLAG_vtarget_bitrate);
}

DEFINE_int(vmin_bitrate, 100, "Video stream min bitrate in kbps.");
int VideoMinBitrateKbps() {
  return static_cast<int>(FLAG_vmin_bitrate);
}

DEFINE_int(vmax_bitrate, 2000, "Video stream max bitrate in kbps.");
int VideoMaxBitrateKbps() {
  return static_cast<int>(FLAG_vmax_bitrate);
}

DEFINE_bool(suspend_below_min_bitrate,
            false,
            "Suspends video below the configured min bitrate.");

DEFINE_int(vnum_temporal_layers,
           1,
           "Number of temporal layers for video. Set to 1-4 to override.");
int VideoNumTemporalLayers() {
  return static_cast<int>(FLAG_vnum_temporal_layers);
}

DEFINE_int(vnum_streams, 0, "Number of video streams to show or analyze.");
int VideoNumStreams() {
  return static_cast<int>(FLAG_vnum_streams);
}

DEFINE_int(vnum_spatial_layers, 1, "Number of video spatial layers to use.");
int VideoNumSpatialLayers() {
  return static_cast<int>(FLAG_vnum_spatial_layers);
}

DEFINE_int(vinter_layer_pred,
           2,
           "Video inter-layer prediction mode. "
           "0 - enabled, 1 - disabled, 2 - enabled only for key pictures.");
InterLayerPredMode VideoInterLayerPred() {
  return IntToInterLayerPredMode(FLAG_vinter_layer_pred);
}

DEFINE_string(
    vstream0,
    "",
    "Comma separated values describing VideoStream for video stream #0.");
std::string VideoStream0() {
  return static_cast<std::string>(FLAG_vstream0);
}

DEFINE_string(
    vstream1,
    "",
    "Comma separated values describing VideoStream for video stream #1.");
std::string VideoStream1() {
  return static_cast<std::string>(FLAG_vstream1);
}

DEFINE_string(
    vsl0,
    "",
    "Comma separated values describing SpatialLayer for video layer #0.");
std::string VideoSL0() {
  return static_cast<std::string>(FLAG_vsl0);
}

DEFINE_string(
    vsl1,
    "",
    "Comma separated values describing SpatialLayer for video layer #1.");
std::string VideoSL1() {
  return static_cast<std::string>(FLAG_vsl1);
}

DEFINE_int(vselected_tl,
           -1,
           "Temporal layer to show or analyze for screenshare. -1 to disable "
           "filtering.");
int VideoSelectedTL() {
  return static_cast<int>(FLAG_vselected_tl);
}

DEFINE_int(vselected_stream,
           0,
           "ID of the stream to show or analyze for screenshare."
           "Set to the number of streams to show them all.");
int VideoSelectedStream() {
  return static_cast<int>(FLAG_vselected_stream);
}

DEFINE_int(vselected_sl,
           -1,
           "Spatial layer to show or analyze for screenshare. -1 to disable "
           "filtering.");
int VideoSelectedSL() {
  return static_cast<int>(FLAG_vselected_sl);
}

// Flags for screenshare.
DEFINE_int(min_transmit_bitrate,
           400,
           "Min transmit bitrate incl. padding for screenshare.");
int ScreenshareMinTransmitBitrateKbps() {
  return FLAG_min_transmit_bitrate;
}

DEFINE_int(swidth, 1850, "Screenshare width (crops source).");
size_t ScreenshareWidth() {
  return static_cast<size_t>(FLAG_swidth);
}

DEFINE_int(sheight, 1110, "Screenshare height (crops source).");
size_t ScreenshareHeight() {
  return static_cast<size_t>(FLAG_sheight);
}

DEFINE_int(sfps, 5, "Frames per second for screenshare.");
int ScreenshareFps() {
  return static_cast<int>(FLAG_sfps);
}

DEFINE_int(starget_bitrate, 100, "Screenshare stream target bitrate in kbps.");
int ScreenshareTargetBitrateKbps() {
  return static_cast<int>(FLAG_starget_bitrate);
}

DEFINE_int(smin_bitrate, 100, "Screenshare stream min bitrate in kbps.");
int ScreenshareMinBitrateKbps() {
  return static_cast<int>(FLAG_smin_bitrate);
}

DEFINE_int(smax_bitrate, 2000, "Screenshare stream max bitrate in kbps.");
int ScreenshareMaxBitrateKbps() {
  return static_cast<int>(FLAG_smax_bitrate);
}

DEFINE_int(snum_temporal_layers,
           2,
           "Number of temporal layers to use in screenshare.");
int ScreenshareNumTemporalLayers() {
  return static_cast<int>(FLAG_snum_temporal_layers);
}

DEFINE_int(snum_streams,
           0,
           "Number of screenshare streams to show or analyze.");
int ScreenshareNumStreams() {
  return static_cast<int>(FLAG_snum_streams);
}

DEFINE_int(snum_spatial_layers,
           1,
           "Number of screenshare spatial layers to use.");
int ScreenshareNumSpatialLayers() {
  return static_cast<int>(FLAG_snum_spatial_layers);
}

DEFINE_int(sinter_layer_pred,
           0,
           "Screenshare inter-layer prediction mode. "
           "0 - enabled, 1 - disabled, 2 - enabled only for key pictures.");
InterLayerPredMode ScreenshareInterLayerPred() {
  return IntToInterLayerPredMode(FLAG_sinter_layer_pred);
}

DEFINE_string(
    sstream0,
    "",
    "Comma separated values describing VideoStream for screenshare stream #0.");
std::string ScreenshareStream0() {
  return static_cast<std::string>(FLAG_sstream0);
}

DEFINE_string(
    sstream1,
    "",
    "Comma separated values describing VideoStream for screenshare stream #1.");
std::string ScreenshareStream1() {
  return static_cast<std::string>(FLAG_sstream1);
}

DEFINE_string(
    ssl0,
    "",
    "Comma separated values describing SpatialLayer for screenshare layer #0.");
std::string ScreenshareSL0() {
  return static_cast<std::string>(FLAG_ssl0);
}

DEFINE_string(
    ssl1,
    "",
    "Comma separated values describing SpatialLayer for screenshare layer #1.");
std::string ScreenshareSL1() {
  return static_cast<std::string>(FLAG_ssl1);
}

DEFINE_int(sselected_tl,
           -1,
           "Temporal layer to show or analyze for screenshare. -1 to disable "
           "filtering.");
int ScreenshareSelectedTL() {
  return static_cast<int>(FLAG_sselected_tl);
}

DEFINE_int(sselected_stream,
           0,
           "ID of the stream to show or analyze for screenshare."
           "Set to the number of streams to show them all.");
int ScreenshareSelectedStream() {
  return static_cast<int>(FLAG_sselected_stream);
}

DEFINE_int(sselected_sl,
           -1,
           "Spatial layer to show or analyze for screenshare. -1 to disable "
           "filtering.");
int ScreenshareSelectedSL() {
  return static_cast<int>(FLAG_sselected_sl);
}

DEFINE_bool(
    generate_slides,
    false,
    "Whether to use randomly generated slides or read them from files.");
bool GenerateSlides() {
  return static_cast<int>(FLAG_generate_slides);
}

DEFINE_int(slide_change_interval,
           10,
           "Interval (in seconds) between simulated slide changes.");
int SlideChangeInterval() {
  return static_cast<int>(FLAG_slide_change_interval);
}

DEFINE_int(
    scroll_duration,
    0,
    "Duration (in seconds) during which a slide will be scrolled into place.");
int ScrollDuration() {
  return static_cast<int>(FLAG_scroll_duration);
}

DEFINE_string(slides,
              "",
              "Comma-separated list of *.yuv files to display as slides.");
std::vector<std::string> Slides() {
  std::vector<std::string> slides;
  std::string slides_list = FLAG_slides;
  rtc::tokenize(slides_list, ',', &slides);
  return slides;
}

// Flags common with screenshare and video loopback, with equal default values.
DEFINE_int(start_bitrate, 600, "Call start bitrate in kbps.");
int StartBitrateKbps() {
  return static_cast<int>(FLAG_start_bitrate);
}

DEFINE_string(codec, "VP8", "Video codec to use.");
std::string Codec() {
  return static_cast<std::string>(FLAG_codec);
}

DEFINE_bool(analyze_video,
            false,
            "Analyze video stream (if --duration is present)");
bool AnalyzeVideo() {
  return static_cast<bool>(FLAG_analyze_video);
}

DEFINE_bool(analyze_screenshare,
            false,
            "Analyze screenshare stream (if --duration is present)");
bool AnalyzeScreenshare() {
  return static_cast<bool>(FLAG_analyze_screenshare);
}

DEFINE_int(
    duration,
    0,
    "Duration of the test in seconds. If 0, rendered will be shown instead.");
int DurationSecs() {
  return static_cast<int>(FLAG_duration);
}

DEFINE_string(output_filename, "", "Target graph data filename.");
std::string OutputFilename() {
  return static_cast<std::string>(FLAG_output_filename);
}

DEFINE_string(graph_title,
              "",
              "If empty, title will be generated automatically.");
std::string GraphTitle() {
  return static_cast<std::string>(FLAG_graph_title);
}

DEFINE_int(loss_percent, 0, "Percentage of packets randomly lost.");
int LossPercent() {
  return static_cast<int>(FLAG_loss_percent);
}

DEFINE_int(avg_burst_loss_length, -1, "Average burst length of lost packets.");
int AvgBurstLossLength() {
  return static_cast<int>(FLAG_avg_burst_loss_length);
}

DEFINE_int(link_capacity,
           0,
           "Capacity (kbps) of the fake link. 0 means infinite.");
int LinkCapacityKbps() {
  return static_cast<int>(FLAG_link_capacity);
}

DEFINE_int(queue_size, 0, "Size of the bottleneck link queue in packets.");
int QueueSize() {
  return static_cast<int>(FLAG_queue_size);
}

DEFINE_int(avg_propagation_delay_ms,
           0,
           "Average link propagation delay in ms.");
int AvgPropagationDelayMs() {
  return static_cast<int>(FLAG_avg_propagation_delay_ms);
}

DEFINE_string(rtc_event_log_name,
              "",
              "Filename for rtc event log. Two files "
              "with \"_send\" and \"_recv\" suffixes will be created. "
              "Works only when --duration is set.");
std::string RtcEventLogName() {
  return static_cast<std::string>(FLAG_rtc_event_log_name);
}

DEFINE_string(rtp_dump_name, "", "Filename for dumped received RTP stream.");
std::string RtpDumpName() {
  return static_cast<std::string>(FLAG_rtp_dump_name);
}

DEFINE_int(std_propagation_delay_ms,
           0,
           "Link propagation delay standard deviation in ms.");
int StdPropagationDelayMs() {
  return static_cast<int>(FLAG_std_propagation_delay_ms);
}

DEFINE_string(encoded_frame_path,
              "",
              "The base path for encoded frame logs. Created files will have "
              "the form <encoded_frame_path>.<n>.(recv|send.<m>).ivf");
std::string EncodedFramePath() {
  return static_cast<std::string>(FLAG_encoded_frame_path);
}

DEFINE_bool(logs, false, "print logs to stderr");

DEFINE_bool(send_side_bwe, true, "Use send-side bandwidth estimation");

DEFINE_bool(generic_descriptor, false, "Use the generic frame descriptor.");

DEFINE_bool(allow_reordering, false, "Allow packet reordering to occur");

DEFINE_bool(use_ulpfec, false, "Use RED+ULPFEC forward error correction.");

DEFINE_bool(use_flexfec, false, "Use FlexFEC forward error correction.");

DEFINE_bool(audio, false, "Add audio stream");

DEFINE_bool(audio_video_sync,
            false,
            "Sync audio and video stream (no effect if"
            " audio is false)");

DEFINE_bool(audio_dtx, false, "Enable audio DTX (no effect if audio is false)");

DEFINE_bool(video, true, "Add video stream");

DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");

// Video-specific flags.
DEFINE_string(vclip,
              "",
              "Name of the clip to show. If empty, the camera is used. Use "
              "\"Generator\" for chroma generator.");
std::string VideoClip() {
  return static_cast<std::string>(FLAG_vclip);
}

DEFINE_bool(help, false, "prints this message");

}  // namespace flags

void Loopback() {
  int camera_idx, screenshare_idx;
  RTC_CHECK(!(flags::AnalyzeScreenshare() && flags::AnalyzeVideo()))
      << "Select only one of video or screenshare.";
  RTC_CHECK(!flags::DurationSecs() || flags::AnalyzeScreenshare() ||
            flags::AnalyzeVideo())
      << "If duration is set, exactly one of analyze_* flags should be set.";
  // Default: camera feed first, if nothing selected.
  if (flags::AnalyzeVideo() || !flags::AnalyzeScreenshare()) {
    camera_idx = 0;
    screenshare_idx = 1;
  } else {
    camera_idx = 1;
    screenshare_idx = 0;
  }

  DefaultNetworkSimulationConfig pipe_config;
  pipe_config.loss_percent = flags::LossPercent();
  pipe_config.avg_burst_loss_length = flags::AvgBurstLossLength();
  pipe_config.link_capacity_kbps = flags::LinkCapacityKbps();
  pipe_config.queue_length_packets = flags::QueueSize();
  pipe_config.queue_delay_ms = flags::AvgPropagationDelayMs();
  pipe_config.delay_standard_deviation_ms = flags::StdPropagationDelayMs();
  pipe_config.allow_reordering = flags::FLAG_allow_reordering;

  BitrateConstraints call_bitrate_config;
  call_bitrate_config.min_bitrate_bps =
      (flags::ScreenshareMinBitrateKbps() + flags::VideoMinBitrateKbps()) *
      1000;
  call_bitrate_config.start_bitrate_bps = flags::StartBitrateKbps() * 1000;
  call_bitrate_config.max_bitrate_bps =
      (flags::ScreenshareMaxBitrateKbps() + flags::VideoMaxBitrateKbps()) *
      1000;

  VideoQualityTest::Params params, camera_params, screenshare_params;
  params.call = {flags::FLAG_send_side_bwe, flags::FLAG_generic_descriptor,
                 call_bitrate_config, 0};
  params.call.dual_video = true;
  params.video[screenshare_idx] = {
      true,
      flags::ScreenshareWidth(),
      flags::ScreenshareHeight(),
      flags::ScreenshareFps(),
      flags::ScreenshareMinBitrateKbps() * 1000,
      flags::ScreenshareTargetBitrateKbps() * 1000,
      flags::ScreenshareMaxBitrateKbps() * 1000,
      false,
      flags::Codec(),
      flags::ScreenshareNumTemporalLayers(),
      flags::ScreenshareSelectedTL(),
      flags::ScreenshareMinTransmitBitrateKbps() * 1000,
      false,  // ULPFEC disabled.
      false,  // FlexFEC disabled.
      false,  // Automatic scaling disabled
      ""};
  params.video[camera_idx] = {flags::FLAG_video,
                              flags::VideoWidth(),
                              flags::VideoHeight(),
                              flags::VideoFps(),
                              flags::VideoMinBitrateKbps() * 1000,
                              flags::VideoTargetBitrateKbps() * 1000,
                              flags::VideoMaxBitrateKbps() * 1000,
                              flags::FLAG_suspend_below_min_bitrate,
                              flags::Codec(),
                              flags::VideoNumTemporalLayers(),
                              flags::VideoSelectedTL(),
                              0,  // No min transmit bitrate.
                              flags::FLAG_use_ulpfec,
                              flags::FLAG_use_flexfec,
                              false,
                              flags::VideoClip(),
                              flags::GetCaptureDevice()};
  params.audio = {flags::FLAG_audio, flags::FLAG_audio_video_sync,
                  flags::FLAG_audio_dtx};
  params.logging = {flags::FLAG_rtc_event_log_name, flags::FLAG_rtp_dump_name,
                    flags::FLAG_encoded_frame_path};
  params.analyzer = {"dual_streams",
                     0.0,
                     0.0,
                     flags::DurationSecs(),
                     flags::OutputFilename(),
                     flags::GraphTitle()};
  params.config = pipe_config;

  params.screenshare[camera_idx].enabled = false;
  params.screenshare[screenshare_idx] = {
      true, flags::GenerateSlides(), flags::SlideChangeInterval(),
      flags::ScrollDuration(), flags::Slides()};

  if (flags::VideoNumStreams() > 1 && flags::VideoStream0().empty() &&
      flags::VideoStream1().empty()) {
    params.ss[camera_idx].infer_streams = true;
  }

  if (flags::ScreenshareNumStreams() > 1 &&
      flags::ScreenshareStream0().empty() &&
      flags::ScreenshareStream1().empty()) {
    params.ss[screenshare_idx].infer_streams = true;
  }

  std::vector<std::string> stream_descriptors;
  stream_descriptors.push_back(flags::ScreenshareStream0());
  stream_descriptors.push_back(flags::ScreenshareStream1());
  std::vector<std::string> SL_descriptors;
  SL_descriptors.push_back(flags::ScreenshareSL0());
  SL_descriptors.push_back(flags::ScreenshareSL1());
  VideoQualityTest::FillScalabilitySettings(
      &params, screenshare_idx, stream_descriptors,
      flags::ScreenshareNumStreams(), flags::ScreenshareSelectedStream(),
      flags::ScreenshareNumSpatialLayers(), flags::ScreenshareSelectedSL(),
      flags::ScreenshareInterLayerPred(), SL_descriptors);

  stream_descriptors.clear();
  stream_descriptors.push_back(flags::VideoStream0());
  stream_descriptors.push_back(flags::VideoStream1());
  SL_descriptors.clear();
  SL_descriptors.push_back(flags::VideoSL0());
  SL_descriptors.push_back(flags::VideoSL1());
  VideoQualityTest::FillScalabilitySettings(
      &params, camera_idx, stream_descriptors, flags::VideoNumStreams(),
      flags::VideoSelectedStream(), flags::VideoNumSpatialLayers(),
      flags::VideoSelectedSL(), flags::VideoInterLayerPred(), SL_descriptors);

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
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true) != 0) {
    // Fail on unrecognized flags.
    return 1;
  }
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
