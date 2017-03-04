/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdio.h>
#include <time.h>

#include <stdarg.h>
#include <sys/stat.h>  // To check for directory existence.

#ifndef S_ISDIR  // Not defined in stat.h on Windows.
#define S_ISDIR(mode) (((mode)&S_IFMT) == S_IFDIR)
#endif

#include "gflags/gflags.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/video_coding/codecs/test/packet_manipulator.h"
#include "webrtc/modules/video_coding/codecs/test/stats.h"
#include "webrtc/modules/video_coding/codecs/test/videoprocessor.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/test/testsupport/frame_reader.h"
#include "webrtc/test/testsupport/frame_writer.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"
#include "webrtc/test/testsupport/packet_reader.h"

DEFINE_string(test_name, "Quality test", "The name of the test to run. ");
DEFINE_string(test_description,
              "",
              "A more detailed description about what "
              "the current test is about.");
DEFINE_string(input_filename,
              "",
              "Input file. "
              "The source video file to be encoded and decoded. Must be in "
              ".yuv format");
DEFINE_int32(width, -1, "Width in pixels of the frames in the input file.");
DEFINE_int32(height, -1, "Height in pixels of the frames in the input file.");
DEFINE_int32(framerate,
             30,
             "Frame rate of the input file, in FPS "
             "(frames-per-second). ");
DEFINE_string(output_dir,
              ".",
              "Output directory. "
              "The directory where the output file will be put. Must already "
              "exist.");
DEFINE_bool(use_single_core,
            false,
            "Force using a single core. If set to "
            "true, only one core will be used for processing. Using a single "
            "core is necessary to get a deterministic behavior for the"
            "encoded frames - using multiple cores will produce different "
            "encoded frames since multiple cores are competing to consume the "
            "byte budget for each frame in parallel. If set to false, "
            "the maximum detected number of cores will be used. ");
DEFINE_bool(disable_fixed_random_seed,
            false,
            "Set this flag to disable the"
            "usage of a fixed random seed for the random generator used "
            "for packet loss. Disabling this will cause consecutive runs "
            "loose packets at different locations, which is bad for "
            "reproducibility.");
DEFINE_string(output_filename,
              "",
              "Output file. "
              "The name of the output video file resulting of the processing "
              "of the source file. By default this is the same name as the "
              "input file with '_out' appended before the extension.");
DEFINE_int32(bitrate, 500, "Bit rate in kilobits/second.");
DEFINE_int32(keyframe_interval,
             0,
             "Forces a keyframe every Nth frame. "
             "0 means the encoder decides when to insert keyframes.  Note that "
             "the encoder may create a keyframe in other locations in addition "
             "to the interval that is set using this parameter.");
DEFINE_int32(temporal_layers,
             0,
             "The number of temporal layers to use "
             "(VP8 specific codec setting). Must be 0-4.");
DEFINE_int32(packet_size,
             1500,
             "Simulated network packet size in bytes (MTU). "
             "Used for packet loss simulation.");
DEFINE_int32(max_payload_size,
             1440,
             "Max payload size in bytes for the "
             "encoder.");
DEFINE_string(packet_loss_mode,
              "uniform",
              "Packet loss mode. Two different "
              "packet loss models are supported: uniform or burst. This "
              "setting has no effect unless packet_loss_rate is >0. ");
DEFINE_double(packet_loss_probability,
              0.0,
              "Packet loss probability. A value "
              "between 0.0 and 1.0 that defines the probability of a packet "
              "being lost. 0.1 means 10% and so on.");
DEFINE_int32(packet_loss_burst_length,
             1,
             "Packet loss burst length. Defines "
             "how many packets will be lost in a burst when a packet has been "
             "decided to be lost. Must be >=1.");
DEFINE_bool(csv,
            false,
            "CSV output. Enabling this will output all frame "
            "statistics at the end of execution. Recommended to run combined "
            "with --noverbose to avoid mixing output.");
DEFINE_bool(python,
            false,
            "Python output. Enabling this will output all frame "
            "statistics as a Python script at the end of execution. "
            "Recommended to run combine with --noverbose to avoid mixing "
            "output.");
DEFINE_bool(verbose,
            true,
            "Verbose mode. Prints a lot of debugging info. "
            "Suitable for tracking progress but not for capturing output. "
            "Disable with --noverbose flag.");

// Custom log method that only prints if the verbose flag is given.
// Supports all the standard printf parameters and formatting (just forwarded).
int Log(const char* format, ...) {
  int result = 0;
  if (FLAGS_verbose) {
    va_list args;
    va_start(args, format);
    result = vprintf(format, args);
    va_end(args);
  }
  return result;
}

// Validates the arguments given as command line flags and fills in the
// TestConfig struct with all configurations needed for video processing.
// Returns 0 if everything is OK, otherwise an exit code.
int HandleCommandLineFlags(webrtc::test::TestConfig* config) {
  // Validate the mandatory flags:
  if (FLAGS_input_filename.empty() || FLAGS_width == -1 || FLAGS_height == -1) {
    printf("%s\n", google::ProgramUsage());
    return 1;
  }
  config->name = FLAGS_test_name;
  config->description = FLAGS_test_description;

  // Verify the input file exists and is readable.
  FILE* test_file;
  test_file = fopen(FLAGS_input_filename.c_str(), "rb");
  if (test_file == NULL) {
    fprintf(stderr, "Cannot read the specified input file: %s\n",
            FLAGS_input_filename.c_str());
    return 2;
  }
  fclose(test_file);
  config->input_filename = FLAGS_input_filename;

  // Verify the output dir exists.
  struct stat dir_info;
  if (!(stat(FLAGS_output_dir.c_str(), &dir_info) == 0 &&
        S_ISDIR(dir_info.st_mode))) {
    fprintf(stderr, "Cannot find output directory: %s\n",
            FLAGS_output_dir.c_str());
    return 3;
  }
  config->output_dir = FLAGS_output_dir;

  // Manufacture an output filename if none was given.
  if (FLAGS_output_filename.empty()) {
    // Cut out the filename without extension from the given input file
    // (which may include a path)
    int startIndex = FLAGS_input_filename.find_last_of("/") + 1;
    if (startIndex == 0) {
      startIndex = 0;
    }
    FLAGS_output_filename =
        FLAGS_input_filename.substr(
            startIndex, FLAGS_input_filename.find_last_of(".") - startIndex) +
        "_out.yuv";
  }

  // Verify output file can be written.
  if (FLAGS_output_dir == ".") {
    config->output_filename = FLAGS_output_filename;
  } else {
    config->output_filename = FLAGS_output_dir + "/" + FLAGS_output_filename;
  }
  test_file = fopen(config->output_filename.c_str(), "wb");
  if (test_file == NULL) {
    fprintf(stderr, "Cannot write output file: %s\n",
            config->output_filename.c_str());
    return 4;
  }
  fclose(test_file);

  // Check single core flag.
  config->use_single_core = FLAGS_use_single_core;

  // Get codec specific configuration.
  webrtc::VideoCodingModule::Codec(webrtc::kVideoCodecVP8,
                                   config->codec_settings);

  // Check the temporal layers.
  if (FLAGS_temporal_layers < 0 ||
      FLAGS_temporal_layers > webrtc::kMaxTemporalStreams) {
    fprintf(stderr, "Temporal layers number must be 0-4, was: %d\n",
            FLAGS_temporal_layers);
    return 13;
  }
  config->codec_settings->VP8()->numberOfTemporalLayers = FLAGS_temporal_layers;

  // Check the bit rate.
  if (FLAGS_bitrate <= 0) {
    fprintf(stderr, "Bit rate must be >0 kbps, was: %d\n", FLAGS_bitrate);
    return 5;
  }
  config->codec_settings->startBitrate = FLAGS_bitrate;

  // Check the keyframe interval.
  if (FLAGS_keyframe_interval < 0) {
    fprintf(stderr, "Keyframe interval must be >=0, was: %d\n",
            FLAGS_keyframe_interval);
    return 6;
  }
  config->keyframe_interval = FLAGS_keyframe_interval;

  // Check packet size and max payload size.
  if (FLAGS_packet_size <= 0) {
    fprintf(stderr, "Packet size must be >0 bytes, was: %d\n",
            FLAGS_packet_size);
    return 7;
  }
  config->networking_config.packet_size_in_bytes =
      static_cast<size_t>(FLAGS_packet_size);

  if (FLAGS_max_payload_size <= 0) {
    fprintf(stderr, "Max payload size must be >0 bytes, was: %d\n",
            FLAGS_max_payload_size);
    return 8;
  }
  config->networking_config.max_payload_size_in_bytes =
      static_cast<size_t>(FLAGS_max_payload_size);

  // Check the width and height
  if (FLAGS_width <= 0 || FLAGS_height <= 0) {
    fprintf(stderr, "Width and height must be >0.");
    return 9;
  }
  config->codec_settings->width = FLAGS_width;
  config->codec_settings->height = FLAGS_height;
  config->codec_settings->maxFramerate = FLAGS_framerate;

  // Calculate the size of each frame to read (according to YUV spec).
  config->frame_length_in_bytes =
      3 * config->codec_settings->width * config->codec_settings->height / 2;

  // Check packet loss settings
  if (FLAGS_packet_loss_mode != "uniform" &&
      FLAGS_packet_loss_mode != "burst") {
    fprintf(stderr,
            "Unsupported packet loss mode, must be 'uniform' or "
            "'burst'\n.");
    return 10;
  }
  config->networking_config.packet_loss_mode = webrtc::test::kUniform;
  if (FLAGS_packet_loss_mode == "burst") {
    config->networking_config.packet_loss_mode = webrtc::test::kBurst;
  }

  if (FLAGS_packet_loss_probability < 0.0 ||
      FLAGS_packet_loss_probability > 1.0) {
    fprintf(stderr,
            "Invalid packet loss probability. Must be 0.0 - 1.0, "
            "was: %f\n",
            FLAGS_packet_loss_probability);
    return 11;
  }
  config->networking_config.packet_loss_probability =
      FLAGS_packet_loss_probability;

  if (FLAGS_packet_loss_burst_length < 1) {
    fprintf(stderr,
            "Invalid packet loss burst length, must be >=1, "
            "was: %d\n",
            FLAGS_packet_loss_burst_length);
    return 12;
  }
  config->networking_config.packet_loss_burst_length =
      FLAGS_packet_loss_burst_length;
  config->verbose = FLAGS_verbose;
  return 0;
}

void CalculateSsimVideoMetrics(webrtc::test::TestConfig* config,
                               webrtc::test::QualityMetricsResult* result) {
  Log("Calculating SSIM...\n");
  I420SSIMFromFiles(
      config->input_filename.c_str(), config->output_filename.c_str(),
      config->codec_settings->width, config->codec_settings->height, result);
  Log("  Average: %3.2f\n", result->average);
  Log("  Min    : %3.2f (frame %d)\n", result->min, result->min_frame_number);
  Log("  Max    : %3.2f (frame %d)\n", result->max, result->max_frame_number);
}

void CalculatePsnrVideoMetrics(webrtc::test::TestConfig* config,
                               webrtc::test::QualityMetricsResult* result) {
  Log("Calculating PSNR...\n");
  I420PSNRFromFiles(
      config->input_filename.c_str(), config->output_filename.c_str(),
      config->codec_settings->width, config->codec_settings->height, result);
  Log("  Average: %3.2f\n", result->average);
  Log("  Min    : %3.2f (frame %d)\n", result->min, result->min_frame_number);
  Log("  Max    : %3.2f (frame %d)\n", result->max, result->max_frame_number);
}

void PrintConfigurationSummary(const webrtc::test::TestConfig& config) {
  Log("Quality test with parameters:\n");
  Log("  Test name        : %s\n", config.name.c_str());
  Log("  Description      : %s\n", config.description.c_str());
  Log("  Input filename   : %s\n", config.input_filename.c_str());
  Log("  Output directory : %s\n", config.output_dir.c_str());
  Log("  Output filename  : %s\n", config.output_filename.c_str());
  Log("  Frame length     : %" PRIuS " bytes\n", config.frame_length_in_bytes);
  Log("  Packet size      : %" PRIuS " bytes\n",
      config.networking_config.packet_size_in_bytes);
  Log("  Max payload size : %" PRIuS " bytes\n",
      config.networking_config.max_payload_size_in_bytes);
  Log("  Packet loss:\n");
  Log("    Mode           : %s\n",
      PacketLossModeToStr(config.networking_config.packet_loss_mode));
  Log("    Probability    : %2.1f\n",
      config.networking_config.packet_loss_probability);
  Log("    Burst length   : %d packets\n",
      config.networking_config.packet_loss_burst_length);
}

void PrintCsvOutput(const webrtc::test::Stats& stats,
                    const webrtc::test::QualityMetricsResult& ssim_result,
                    const webrtc::test::QualityMetricsResult& psnr_result) {
  Log(
      "\nCSV output (recommended to run with --noverbose to skip the "
      "above output)\n");
  printf(
      "frame_number encoding_successful decoding_successful "
      "encode_return_code decode_return_code "
      "encode_time_in_us decode_time_in_us "
      "bit_rate_in_kbps encoded_frame_length_in_bytes frame_type "
      "packets_dropped total_packets "
      "ssim psnr\n");

  for (unsigned int i = 0; i < stats.stats_.size(); ++i) {
    const webrtc::test::FrameStatistic& f = stats.stats_[i];
    const webrtc::test::FrameResult& ssim = ssim_result.frames[i];
    const webrtc::test::FrameResult& psnr = psnr_result.frames[i];
    printf("%4d, %d, %d, %2d, %2d, %6d, %6d, %5d, %7" PRIuS
           ", %d, %2d, %2" PRIuS ", %5.3f, %5.2f\n",
           f.frame_number, f.encoding_successful, f.decoding_successful,
           f.encode_return_code, f.decode_return_code, f.encode_time_in_us,
           f.decode_time_in_us, f.bit_rate_in_kbps,
           f.encoded_frame_length_in_bytes, f.frame_type, f.packets_dropped,
           f.total_packets, ssim.value, psnr.value);
  }
}

void PrintPythonOutput(const webrtc::test::TestConfig& config,
                       const webrtc::test::Stats& stats,
                       const webrtc::test::QualityMetricsResult& ssim_result,
                       const webrtc::test::QualityMetricsResult& psnr_result) {
  Log(
      "\nPython output (recommended to run with --noverbose to skip the "
      "above output)\n");
  printf(
      "test_configuration = ["
      "{'name': 'name',                      'value': '%s'},\n"
      "{'name': 'description',               'value': '%s'},\n"
      "{'name': 'test_number',               'value': '%d'},\n"
      "{'name': 'input_filename',            'value': '%s'},\n"
      "{'name': 'output_filename',           'value': '%s'},\n"
      "{'name': 'output_dir',                'value': '%s'},\n"
      "{'name': 'packet_size_in_bytes',      'value': '%" PRIuS
      "'},\n"
      "{'name': 'max_payload_size_in_bytes', 'value': '%" PRIuS
      "'},\n"
      "{'name': 'packet_loss_mode',          'value': '%s'},\n"
      "{'name': 'packet_loss_probability',   'value': '%f'},\n"
      "{'name': 'packet_loss_burst_length',  'value': '%d'},\n"
      "{'name': 'exclude_frame_types',       'value': '%s'},\n"
      "{'name': 'frame_length_in_bytes',     'value': '%" PRIuS
      "'},\n"
      "{'name': 'use_single_core',           'value': '%s'},\n"
      "{'name': 'keyframe_interval;',        'value': '%d'},\n"
      "{'name': 'video_codec_type',          'value': '%s'},\n"
      "{'name': 'width',                     'value': '%d'},\n"
      "{'name': 'height',                    'value': '%d'},\n"
      "{'name': 'bit_rate_in_kbps',          'value': '%d'},\n"
      "]\n",
      config.name.c_str(), config.description.c_str(), config.test_number,
      config.input_filename.c_str(), config.output_filename.c_str(),
      config.output_dir.c_str(), config.networking_config.packet_size_in_bytes,
      config.networking_config.max_payload_size_in_bytes,
      PacketLossModeToStr(config.networking_config.packet_loss_mode),
      config.networking_config.packet_loss_probability,
      config.networking_config.packet_loss_burst_length,
      ExcludeFrameTypesToStr(config.exclude_frame_types),
      config.frame_length_in_bytes, config.use_single_core ? "True " : "False",
      config.keyframe_interval,
      CodecTypeToPayloadName(config.codec_settings->codecType)
          .value_or("Unknown"),
      config.codec_settings->width, config.codec_settings->height,
      config.codec_settings->startBitrate);
  printf(
      "frame_data_types = {"
      "'frame_number': ('number', 'Frame number'),\n"
      "'encoding_successful': ('boolean', 'Encoding successful?'),\n"
      "'decoding_successful': ('boolean', 'Decoding successful?'),\n"
      "'encode_time': ('number', 'Encode time (us)'),\n"
      "'decode_time': ('number', 'Decode time (us)'),\n"
      "'encode_return_code': ('number', 'Encode return code'),\n"
      "'decode_return_code': ('number', 'Decode return code'),\n"
      "'bit_rate': ('number', 'Bit rate (kbps)'),\n"
      "'encoded_frame_length': "
      "('number', 'Encoded frame length (bytes)'),\n"
      "'frame_type': ('string', 'Frame type'),\n"
      "'packets_dropped': ('number', 'Packets dropped'),\n"
      "'total_packets': ('number', 'Total packets'),\n"
      "'ssim': ('number', 'SSIM'),\n"
      "'psnr': ('number', 'PSNR (dB)'),\n"
      "}\n");
  printf("frame_data = [");
  for (unsigned int i = 0; i < stats.stats_.size(); ++i) {
    const webrtc::test::FrameStatistic& f = stats.stats_[i];
    const webrtc::test::FrameResult& ssim = ssim_result.frames[i];
    const webrtc::test::FrameResult& psnr = psnr_result.frames[i];
    printf(
        "{'frame_number': %d, "
        "'encoding_successful': %s, 'decoding_successful': %s, "
        "'encode_time': %d, 'decode_time': %d, "
        "'encode_return_code': %d, 'decode_return_code': %d, "
        "'bit_rate': %d, 'encoded_frame_length': %" PRIuS
        ", "
        "'frame_type': %s, 'packets_dropped': %d, "
        "'total_packets': %" PRIuS ", 'ssim': %f, 'psnr': %f},\n",
        f.frame_number, f.encoding_successful ? "True " : "False",
        f.decoding_successful ? "True " : "False", f.encode_time_in_us,
        f.decode_time_in_us, f.encode_return_code, f.decode_return_code,
        f.bit_rate_in_kbps, f.encoded_frame_length_in_bytes,
        f.frame_type == webrtc::kVideoFrameDelta ? "'Delta'" : "'Other'",
        f.packets_dropped, f.total_packets, ssim.value, psnr.value);
  }
  printf("]\n");
}

// Runs a quality measurement on the input file supplied to the program.
// The input file must be in YUV format.
int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Quality test application for video comparisons.\n"
      "Run " +
      program_name +
      " --helpshort for usage.\n"
      "Example usage:\n" +
      program_name +
      " --input_filename=filename.yuv --width=352 --height=288\n";
  google::SetUsageMessage(usage);

  google::ParseCommandLineFlags(&argc, &argv, true);

  // Create TestConfig and codec settings struct.
  webrtc::test::TestConfig config;
  webrtc::VideoCodec codec_settings;
  config.codec_settings = &codec_settings;

  int return_code = HandleCommandLineFlags(&config);
  // Exit if an invalid argument is supplied.
  if (return_code != 0) {
    return return_code;
  }

  PrintConfigurationSummary(config);

  webrtc::VP8Encoder* encoder = webrtc::VP8Encoder::Create();
  webrtc::VP8Decoder* decoder = webrtc::VP8Decoder::Create();
  webrtc::test::Stats stats;
  webrtc::test::YuvFrameReaderImpl frame_reader(config.input_filename,
                                                config.codec_settings->width,
                                                config.codec_settings->height);
  webrtc::test::YuvFrameWriterImpl frame_writer(config.output_filename,
                                                config.codec_settings->width,
                                                config.codec_settings->height);
  frame_reader.Init();
  frame_writer.Init();
  webrtc::test::PacketReader packet_reader;

  webrtc::test::PacketManipulatorImpl packet_manipulator(
      &packet_reader, config.networking_config, config.verbose);
  // By default the packet manipulator is seeded with a fixed random.
  // If disabled we must generate a new seed.
  if (FLAGS_disable_fixed_random_seed) {
    packet_manipulator.InitializeRandomSeed(time(NULL));
  }
  webrtc::test::VideoProcessor* processor =
      new webrtc::test::VideoProcessorImpl(
          encoder, decoder, &frame_reader, &frame_writer, &packet_manipulator,
          config, &stats, nullptr /* source_frame_writer */,
          nullptr /* encoded_frame_writer */,
          nullptr /* decoded_frame_writer */);
  processor->Init();

  int frame_number = 0;
  while (processor->ProcessFrame(frame_number)) {
    if (frame_number % 80 == 0) {
      Log("\n");  // make the output a bit nicer.
    }
    Log(".");
    frame_number++;
  }
  Log("\n");
  Log("Processed %d frames\n", frame_number);

  // Release encoder and decoder to make sure they have finished processing.
  encoder->Release();
  decoder->Release();

  // Verify statistics are correct:
  assert(frame_number == static_cast<int>(stats.stats_.size()));

  // Close the files before we start using them for SSIM/PSNR calculations.
  frame_reader.Close();
  frame_writer.Close();

  stats.PrintSummary();

  webrtc::test::QualityMetricsResult ssim_result;
  CalculateSsimVideoMetrics(&config, &ssim_result);
  webrtc::test::QualityMetricsResult psnr_result;
  CalculatePsnrVideoMetrics(&config, &psnr_result);

  if (FLAGS_csv) {
    PrintCsvOutput(stats, ssim_result, psnr_result);
  }
  if (FLAGS_python) {
    PrintPythonOutput(config, stats, ssim_result, psnr_result);
  }
  delete processor;
  delete encoder;
  delete decoder;
  Log("Quality test finished!");
  return 0;
}
