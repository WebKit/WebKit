/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>

#include "webrtc/base/flags.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_parser.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/tools/event_log_visualizer/analyzer.h"
#include "webrtc/tools/event_log_visualizer/plot_base.h"
#include "webrtc/tools/event_log_visualizer/plot_python.h"

DEFINE_bool(incoming, true, "Plot statistics for incoming packets.");
DEFINE_bool(outgoing, true, "Plot statistics for outgoing packets.");
DEFINE_bool(plot_all, true, "Plot all different data types.");
DEFINE_bool(plot_packets,
            false,
            "Plot bar graph showing the size of each packet.");
DEFINE_bool(plot_audio_playout,
            false,
            "Plot bar graph showing the time between each audio playout.");
DEFINE_bool(plot_audio_level,
            false,
            "Plot line graph showing the audio level.");
DEFINE_bool(
    plot_sequence_number,
    false,
    "Plot the difference in sequence number between consecutive packets.");
DEFINE_bool(
    plot_delay_change,
    false,
    "Plot the difference in 1-way path delay between consecutive packets.");
DEFINE_bool(plot_accumulated_delay_change,
            false,
            "Plot the accumulated 1-way path delay change, or the path delay "
            "change compared to the first packet.");
DEFINE_bool(plot_total_bitrate,
            false,
            "Plot the total bitrate used by all streams.");
DEFINE_bool(plot_stream_bitrate,
            false,
            "Plot the bitrate used by each stream.");
DEFINE_bool(plot_bwe,
            false,
            "Run the bandwidth estimator with the logged rtp and rtcp and plot "
            "the output.");
DEFINE_bool(plot_network_delay_feedback,
            false,
            "Compute network delay based on sent packets and the received "
            "transport feedback.");
DEFINE_bool(plot_fraction_loss,
            false,
            "Plot packet loss in percent for outgoing packets (as perceived by "
            "the send-side bandwidth estimator).");
DEFINE_bool(plot_timestamps,
            false,
            "Plot the rtp timestamps of all rtp and rtcp packets over time.");
DEFINE_bool(audio_encoder_bitrate_bps,
            false,
            "Plot the audio encoder target bitrate.");
DEFINE_bool(audio_encoder_frame_length_ms,
            false,
            "Plot the audio encoder frame length.");
DEFINE_bool(
    audio_encoder_uplink_packet_loss_fraction,
    false,
    "Plot the uplink packet loss fraction which is send to the audio encoder.");
DEFINE_bool(audio_encoder_fec, false, "Plot the audio encoder FEC.");
DEFINE_bool(audio_encoder_dtx, false, "Plot the audio encoder DTX.");
DEFINE_bool(audio_encoder_num_channels,
            false,
            "Plot the audio encoder number of channels.");
DEFINE_bool(plot_audio_jitter_buffer,
            false,
            "Plot the audio jitter buffer delay profile.");
DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enabled/"
    " will assign the group Enabled to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");
DEFINE_bool(help, false, "prints this message");

int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "A tool for visualizing WebRTC event logs.\n"
      "Example usage:\n" +
      program_name + " <logfile> | python\n" + "Run " + program_name +
      " --help for a list of command line options\n";
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  if (argc != 2) {
    // Print usage information.
    std::cout << usage;
    return 0;
  }

  webrtc::test::SetExecutablePath(argv[0]);
  webrtc::test::InitFieldTrialsFromString(FLAG_force_fieldtrials);

  std::string filename = argv[1];

  webrtc::ParsedRtcEventLog parsed_log;

  if (!parsed_log.ParseFile(filename)) {
    std::cerr << "Could not parse the entire log file." << std::endl;
    std::cerr << "Proceeding to analyze the first "
              << parsed_log.GetNumberOfEvents() << " events in the file."
              << std::endl;
  }

  webrtc::plotting::EventLogAnalyzer analyzer(parsed_log);
  std::unique_ptr<webrtc::plotting::PlotCollection> collection(
      new webrtc::plotting::PythonPlotCollection());

  if (FLAG_plot_all || FLAG_plot_packets) {
    if (FLAG_incoming) {
      analyzer.CreatePacketGraph(webrtc::PacketDirection::kIncomingPacket,
                                 collection->AppendNewPlot());
      analyzer.CreateAccumulatedPacketsGraph(
          webrtc::PacketDirection::kIncomingPacket,
          collection->AppendNewPlot());
    }
    if (FLAG_outgoing) {
      analyzer.CreatePacketGraph(webrtc::PacketDirection::kOutgoingPacket,
                                 collection->AppendNewPlot());
      analyzer.CreateAccumulatedPacketsGraph(
          webrtc::PacketDirection::kOutgoingPacket,
          collection->AppendNewPlot());
    }
  }

  if (FLAG_plot_all || FLAG_plot_audio_playout) {
    analyzer.CreatePlayoutGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_plot_audio_level) {
    analyzer.CreateAudioLevelGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_plot_sequence_number) {
    if (FLAG_incoming) {
      analyzer.CreateSequenceNumberGraph(collection->AppendNewPlot());
    }
  }

  if (FLAG_plot_all || FLAG_plot_delay_change) {
    if (FLAG_incoming) {
      analyzer.CreateDelayChangeGraph(collection->AppendNewPlot());
    }
  }

  if (FLAG_plot_all || FLAG_plot_accumulated_delay_change) {
    if (FLAG_incoming) {
      analyzer.CreateAccumulatedDelayChangeGraph(collection->AppendNewPlot());
    }
  }

  if (FLAG_plot_all || FLAG_plot_fraction_loss) {
    analyzer.CreateFractionLossGraph(collection->AppendNewPlot());
    analyzer.CreateIncomingPacketLossGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_plot_total_bitrate) {
    if (FLAG_incoming) {
      analyzer.CreateTotalBitrateGraph(webrtc::PacketDirection::kIncomingPacket,
                                       collection->AppendNewPlot());
    }
    if (FLAG_outgoing) {
      analyzer.CreateTotalBitrateGraph(webrtc::PacketDirection::kOutgoingPacket,
                                       collection->AppendNewPlot());
    }
  }

  if (FLAG_plot_all || FLAG_plot_stream_bitrate) {
    if (FLAG_incoming) {
      analyzer.CreateStreamBitrateGraph(
          webrtc::PacketDirection::kIncomingPacket,
          collection->AppendNewPlot());
    }
    if (FLAG_outgoing) {
      analyzer.CreateStreamBitrateGraph(
          webrtc::PacketDirection::kOutgoingPacket,
          collection->AppendNewPlot());
    }
  }

  if (FLAG_plot_all || FLAG_plot_bwe) {
    analyzer.CreateBweSimulationGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_plot_network_delay_feedback) {
    analyzer.CreateNetworkDelayFeedbackGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_plot_timestamps) {
    analyzer.CreateTimestampGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_audio_encoder_bitrate_bps) {
    analyzer.CreateAudioEncoderTargetBitrateGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_audio_encoder_frame_length_ms) {
    analyzer.CreateAudioEncoderFrameLengthGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_audio_encoder_uplink_packet_loss_fraction) {
    analyzer.CreateAudioEncoderUplinkPacketLossFractionGraph(
        collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_audio_encoder_fec) {
    analyzer.CreateAudioEncoderEnableFecGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_audio_encoder_dtx) {
    analyzer.CreateAudioEncoderEnableDtxGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_audio_encoder_num_channels) {
    analyzer.CreateAudioEncoderNumChannelsGraph(collection->AppendNewPlot());
  }

  if (FLAG_plot_all || FLAG_plot_audio_jitter_buffer) {
    analyzer.CreateAudioJitterBufferGraph(
        webrtc::test::ResourcePath(
            "audio_processing/conversational_speech/EN_script2_F_sp2_B1",
            "wav"),
        48000, collection->AppendNewPlot());
  }

  collection->Draw();

  return 0;
}
