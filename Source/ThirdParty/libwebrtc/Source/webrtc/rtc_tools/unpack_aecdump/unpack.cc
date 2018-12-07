/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Commandline tool to unpack audioproc debug files.
//
// The debug files are dumped as protobuf blobs. For analysis, it's necessary
// to unpack the file into its component parts: audio and other data.

#include <stdio.h>

#include <memory>

#include "modules/audio_processing/test/protobuf_utils.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/flags.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/strings/string_builder.h"

RTC_PUSH_IGNORING_WUNDEF()
#include "modules/audio_processing/debug.pb.h"
RTC_POP_IGNORING_WUNDEF()

// TODO(andrew): unpack more of the data.
WEBRTC_DEFINE_string(input_file, "input", "The name of the input stream file.");
WEBRTC_DEFINE_string(output_file,
                     "ref_out",
                     "The name of the reference output stream file.");
WEBRTC_DEFINE_string(reverse_file,
                     "reverse",
                     "The name of the reverse input stream file.");
WEBRTC_DEFINE_string(delay_file, "delay.int32", "The name of the delay file.");
WEBRTC_DEFINE_string(drift_file, "drift.int32", "The name of the drift file.");
WEBRTC_DEFINE_string(level_file, "level.int32", "The name of the level file.");
WEBRTC_DEFINE_string(keypress_file,
                     "keypress.bool",
                     "The name of the keypress file.");
WEBRTC_DEFINE_string(callorder_file,
                     "callorder",
                     "The name of the render/capture call order file.");
WEBRTC_DEFINE_string(settings_file,
                     "settings.txt",
                     "The name of the settings file.");
WEBRTC_DEFINE_bool(full,
                   false,
                   "Unpack the full set of files (normally not needed).");
WEBRTC_DEFINE_bool(raw, false, "Write raw data instead of a WAV file.");
WEBRTC_DEFINE_bool(
    text,
    false,
    "Write non-audio files as text files instead of binary files.");
WEBRTC_DEFINE_bool(help, false, "Print this message.");

#define PRINT_CONFIG(field_name)                                         \
  if (msg.has_##field_name()) {                                          \
    fprintf(settings_file, "  " #field_name ": %d\n", msg.field_name()); \
  }

#define PRINT_CONFIG_FLOAT(field_name)                                   \
  if (msg.has_##field_name()) {                                          \
    fprintf(settings_file, "  " #field_name ": %f\n", msg.field_name()); \
  }

namespace webrtc {

using audioproc::Event;
using audioproc::ReverseStream;
using audioproc::Stream;
using audioproc::Init;

namespace {

void WriteData(const void* data,
               size_t size,
               FILE* file,
               const std::string& filename) {
  if (fwrite(data, size, 1, file) != 1) {
    printf("Error when writing to %s\n", filename.c_str());
    exit(1);
  }
}

void WriteCallOrderData(const bool render_call,
                        FILE* file,
                        const std::string& filename) {
  const char call_type = render_call ? 'r' : 'c';
  WriteData(&call_type, sizeof(call_type), file, filename.c_str());
}

bool WritingCallOrderFile() {
  return FLAG_full;
}

}  // namespace

int do_main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Commandline tool to unpack audioproc debug files.\n"
      "Example usage:\n" +
      program_name + " debug_dump.pb\n";

  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true) || FLAG_help ||
      argc < 2) {
    printf("%s", usage.c_str());
    if (FLAG_help) {
      rtc::FlagList::Print(nullptr, false);
      return 0;
    }
    return 1;
  }

  FILE* debug_file = OpenFile(argv[1], "rb");

  Event event_msg;
  int frame_count = 0;
  size_t reverse_samples_per_channel = 0;
  size_t input_samples_per_channel = 0;
  size_t output_samples_per_channel = 0;
  size_t num_reverse_channels = 0;
  size_t num_input_channels = 0;
  size_t num_output_channels = 0;
  std::unique_ptr<WavWriter> reverse_wav_file;
  std::unique_ptr<WavWriter> input_wav_file;
  std::unique_ptr<WavWriter> output_wav_file;
  std::unique_ptr<RawFile> reverse_raw_file;
  std::unique_ptr<RawFile> input_raw_file;
  std::unique_ptr<RawFile> output_raw_file;

  rtc::StringBuilder callorder_raw_name;
  callorder_raw_name << FLAG_callorder_file << ".char";
  FILE* callorder_char_file = WritingCallOrderFile()
                                  ? OpenFile(callorder_raw_name.str(), "wb")
                                  : nullptr;
  FILE* settings_file = OpenFile(FLAG_settings_file, "wb");

  while (ReadMessageFromFile(debug_file, &event_msg)) {
    if (event_msg.type() == Event::REVERSE_STREAM) {
      if (!event_msg.has_reverse_stream()) {
        printf("Corrupt input file: ReverseStream missing.\n");
        return 1;
      }

      const ReverseStream msg = event_msg.reverse_stream();
      if (msg.has_data()) {
        if (FLAG_raw && !reverse_raw_file) {
          reverse_raw_file.reset(
              new RawFile(std::string(FLAG_reverse_file) + ".pcm"));
        }
        // TODO(aluebs): Replace "num_reverse_channels *
        // reverse_samples_per_channel" with "msg.data().size() /
        // sizeof(int16_t)" and so on when this fix in audio_processing has made
        // it into stable: https://webrtc-codereview.appspot.com/15299004/
        WriteIntData(reinterpret_cast<const int16_t*>(msg.data().data()),
                     num_reverse_channels * reverse_samples_per_channel,
                     reverse_wav_file.get(), reverse_raw_file.get());
      } else if (msg.channel_size() > 0) {
        if (FLAG_raw && !reverse_raw_file) {
          reverse_raw_file.reset(
              new RawFile(std::string(FLAG_reverse_file) + ".float"));
        }
        std::unique_ptr<const float* []> data(
            new const float*[num_reverse_channels]);
        for (size_t i = 0; i < num_reverse_channels; ++i) {
          data[i] = reinterpret_cast<const float*>(msg.channel(i).data());
        }
        WriteFloatData(data.get(), reverse_samples_per_channel,
                       num_reverse_channels, reverse_wav_file.get(),
                       reverse_raw_file.get());
      }
      if (FLAG_full) {
        if (WritingCallOrderFile()) {
          WriteCallOrderData(true /* render_call */, callorder_char_file,
                             FLAG_callorder_file);
        }
      }
    } else if (event_msg.type() == Event::STREAM) {
      frame_count++;
      if (!event_msg.has_stream()) {
        printf("Corrupt input file: Stream missing.\n");
        return 1;
      }

      const Stream msg = event_msg.stream();
      if (msg.has_input_data()) {
        if (FLAG_raw && !input_raw_file) {
          input_raw_file.reset(
              new RawFile(std::string(FLAG_input_file) + ".pcm"));
        }
        WriteIntData(reinterpret_cast<const int16_t*>(msg.input_data().data()),
                     num_input_channels * input_samples_per_channel,
                     input_wav_file.get(), input_raw_file.get());
      } else if (msg.input_channel_size() > 0) {
        if (FLAG_raw && !input_raw_file) {
          input_raw_file.reset(
              new RawFile(std::string(FLAG_input_file) + ".float"));
        }
        std::unique_ptr<const float* []> data(
            new const float*[num_input_channels]);
        for (size_t i = 0; i < num_input_channels; ++i) {
          data[i] = reinterpret_cast<const float*>(msg.input_channel(i).data());
        }
        WriteFloatData(data.get(), input_samples_per_channel,
                       num_input_channels, input_wav_file.get(),
                       input_raw_file.get());
      }

      if (msg.has_output_data()) {
        if (FLAG_raw && !output_raw_file) {
          output_raw_file.reset(
              new RawFile(std::string(FLAG_output_file) + ".pcm"));
        }
        WriteIntData(reinterpret_cast<const int16_t*>(msg.output_data().data()),
                     num_output_channels * output_samples_per_channel,
                     output_wav_file.get(), output_raw_file.get());
      } else if (msg.output_channel_size() > 0) {
        if (FLAG_raw && !output_raw_file) {
          output_raw_file.reset(
              new RawFile(std::string(FLAG_output_file) + ".float"));
        }
        std::unique_ptr<const float* []> data(
            new const float*[num_output_channels]);
        for (size_t i = 0; i < num_output_channels; ++i) {
          data[i] =
              reinterpret_cast<const float*>(msg.output_channel(i).data());
        }
        WriteFloatData(data.get(), output_samples_per_channel,
                       num_output_channels, output_wav_file.get(),
                       output_raw_file.get());
      }

      if (FLAG_full) {
        if (WritingCallOrderFile()) {
          WriteCallOrderData(false /* render_call */, callorder_char_file,
                             FLAG_callorder_file);
        }
        if (msg.has_delay()) {
          static FILE* delay_file = OpenFile(FLAG_delay_file, "wb");
          int32_t delay = msg.delay();
          if (FLAG_text) {
            fprintf(delay_file, "%d\n", delay);
          } else {
            WriteData(&delay, sizeof(delay), delay_file, FLAG_delay_file);
          }
        }

        if (msg.has_drift()) {
          static FILE* drift_file = OpenFile(FLAG_drift_file, "wb");
          int32_t drift = msg.drift();
          if (FLAG_text) {
            fprintf(drift_file, "%d\n", drift);
          } else {
            WriteData(&drift, sizeof(drift), drift_file, FLAG_drift_file);
          }
        }

        if (msg.has_level()) {
          static FILE* level_file = OpenFile(FLAG_level_file, "wb");
          int32_t level = msg.level();
          if (FLAG_text) {
            fprintf(level_file, "%d\n", level);
          } else {
            WriteData(&level, sizeof(level), level_file, FLAG_level_file);
          }
        }

        if (msg.has_keypress()) {
          static FILE* keypress_file = OpenFile(FLAG_keypress_file, "wb");
          bool keypress = msg.keypress();
          if (FLAG_text) {
            fprintf(keypress_file, "%d\n", keypress);
          } else {
            WriteData(&keypress, sizeof(keypress), keypress_file,
                      FLAG_keypress_file);
          }
        }
      }
    } else if (event_msg.type() == Event::CONFIG) {
      if (!event_msg.has_config()) {
        printf("Corrupt input file: Config missing.\n");
        return 1;
      }
      const audioproc::Config msg = event_msg.config();

      fprintf(settings_file, "APM re-config at frame: %d\n", frame_count);

      PRINT_CONFIG(aec_enabled);
      PRINT_CONFIG(aec_delay_agnostic_enabled);
      PRINT_CONFIG(aec_drift_compensation_enabled);
      PRINT_CONFIG(aec_extended_filter_enabled);
      PRINT_CONFIG(aec_suppression_level);
      PRINT_CONFIG(aecm_enabled);
      PRINT_CONFIG(aecm_comfort_noise_enabled);
      PRINT_CONFIG(aecm_routing_mode);
      PRINT_CONFIG(agc_enabled);
      PRINT_CONFIG(agc_mode);
      PRINT_CONFIG(agc_limiter_enabled);
      PRINT_CONFIG(noise_robust_agc_enabled);
      PRINT_CONFIG(hpf_enabled);
      PRINT_CONFIG(ns_enabled);
      PRINT_CONFIG(ns_level);
      PRINT_CONFIG(transient_suppression_enabled);
      PRINT_CONFIG(pre_amplifier_enabled);
      PRINT_CONFIG_FLOAT(pre_amplifier_fixed_gain_factor);

      if (msg.has_experiments_description()) {
        fprintf(settings_file, "  experiments_description: %s\n",
                msg.experiments_description().c_str());
      }
    } else if (event_msg.type() == Event::INIT) {
      if (!event_msg.has_init()) {
        printf("Corrupt input file: Init missing.\n");
        return 1;
      }

      const Init msg = event_msg.init();
      // These should print out zeros if they're missing.
      fprintf(settings_file, "Init at frame: %d\n", frame_count);
      int input_sample_rate = msg.sample_rate();
      fprintf(settings_file, "  Input sample rate: %d\n", input_sample_rate);
      int output_sample_rate = msg.output_sample_rate();
      fprintf(settings_file, "  Output sample rate: %d\n", output_sample_rate);
      int reverse_sample_rate = msg.reverse_sample_rate();
      fprintf(settings_file, "  Reverse sample rate: %d\n",
              reverse_sample_rate);
      num_input_channels = msg.num_input_channels();
      fprintf(settings_file, "  Input channels: %" PRIuS "\n",
              num_input_channels);
      num_output_channels = msg.num_output_channels();
      fprintf(settings_file, "  Output channels: %" PRIuS "\n",
              num_output_channels);
      num_reverse_channels = msg.num_reverse_channels();
      fprintf(settings_file, "  Reverse channels: %" PRIuS "\n",
              num_reverse_channels);
      if (msg.has_timestamp_ms()) {
        const int64_t timestamp = msg.timestamp_ms();
        fprintf(settings_file, "  Timestamp in millisecond: %" PRId64 "\n",
                timestamp);
      }

      fprintf(settings_file, "\n");

      if (reverse_sample_rate == 0) {
        reverse_sample_rate = input_sample_rate;
      }
      if (output_sample_rate == 0) {
        output_sample_rate = input_sample_rate;
      }

      reverse_samples_per_channel =
          static_cast<size_t>(reverse_sample_rate / 100);
      input_samples_per_channel = static_cast<size_t>(input_sample_rate / 100);
      output_samples_per_channel =
          static_cast<size_t>(output_sample_rate / 100);

      if (!FLAG_raw) {
        // The WAV files need to be reset every time, because they cant change
        // their sample rate or number of channels.
        rtc::StringBuilder reverse_name;
        reverse_name << FLAG_reverse_file << frame_count << ".wav";
        reverse_wav_file.reset(new WavWriter(
            reverse_name.str(), reverse_sample_rate, num_reverse_channels));
        rtc::StringBuilder input_name;
        input_name << FLAG_input_file << frame_count << ".wav";
        input_wav_file.reset(new WavWriter(input_name.str(), input_sample_rate,
                                           num_input_channels));
        rtc::StringBuilder output_name;
        output_name << FLAG_output_file << frame_count << ".wav";
        output_wav_file.reset(new WavWriter(
            output_name.str(), output_sample_rate, num_output_channels));

        if (WritingCallOrderFile()) {
          rtc::StringBuilder callorder_name;
          callorder_name << FLAG_callorder_file << frame_count << ".char";
          callorder_char_file = OpenFile(callorder_name.str(), "wb");
        }
      }
    }
  }

  return 0;
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::do_main(argc, argv);
}
