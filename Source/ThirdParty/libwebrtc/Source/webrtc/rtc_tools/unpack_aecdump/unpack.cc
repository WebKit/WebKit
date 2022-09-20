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

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "api/function_view.h"
#include "common_audio/include/audio_util.h"
#include "common_audio/wav_file.h"
#include "modules/audio_processing/test/protobuf_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/arch.h"

RTC_PUSH_IGNORING_WUNDEF()
#include "modules/audio_processing/debug.pb.h"
RTC_POP_IGNORING_WUNDEF()

ABSL_FLAG(std::string,
          input_file,
          "input",
          "The name of the input stream file.");
ABSL_FLAG(std::string,
          output_file,
          "ref_out",
          "The name of the reference output stream file.");
ABSL_FLAG(std::string,
          reverse_file,
          "reverse",
          "The name of the reverse input stream file.");
ABSL_FLAG(std::string,
          delay_file,
          "delay.int32",
          "The name of the delay file.");
ABSL_FLAG(std::string,
          drift_file,
          "drift.int32",
          "The name of the drift file.");
ABSL_FLAG(std::string,
          level_file,
          "level.int32",
          "The name of the level file.");
ABSL_FLAG(std::string,
          keypress_file,
          "keypress.bool",
          "The name of the keypress file.");
ABSL_FLAG(std::string,
          callorder_file,
          "callorder",
          "The name of the render/capture call order file.");
ABSL_FLAG(std::string,
          settings_file,
          "settings.txt",
          "The name of the settings file.");
ABSL_FLAG(bool,
          full,
          false,
          "Unpack the full set of files (normally not needed).");
ABSL_FLAG(bool, raw, false, "Write raw data instead of a WAV file.");
ABSL_FLAG(bool,
          text,
          false,
          "Write non-audio files as text files instead of binary files.");
ABSL_FLAG(bool,
          use_init_suffix,
          false,
          "Use init index instead of capture frame count as file name suffix.");

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
using audioproc::Init;
using audioproc::ReverseStream;
using audioproc::Stream;

namespace {
class RawFile final {
 public:
  explicit RawFile(const std::string& filename)
      : file_handle_(fopen(filename.c_str(), "wb")) {}
  ~RawFile() { fclose(file_handle_); }

  RawFile(const RawFile&) = delete;
  RawFile& operator=(const RawFile&) = delete;

  void WriteSamples(const int16_t* samples, size_t num_samples) {
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert samples to little-endian when writing to PCM file"
#endif
    fwrite(samples, sizeof(*samples), num_samples, file_handle_);
  }

  void WriteSamples(const float* samples, size_t num_samples) {
    fwrite(samples, sizeof(*samples), num_samples, file_handle_);
  }

 private:
  FILE* file_handle_;
};

void WriteIntData(const int16_t* data,
                  size_t length,
                  WavWriter* wav_file,
                  RawFile* raw_file) {
  if (wav_file) {
    wav_file->WriteSamples(data, length);
  }
  if (raw_file) {
    raw_file->WriteSamples(data, length);
  }
}

void WriteFloatData(const float* const* data,
                    size_t samples_per_channel,
                    size_t num_channels,
                    WavWriter* wav_file,
                    RawFile* raw_file) {
  size_t length = num_channels * samples_per_channel;
  std::unique_ptr<float[]> buffer(new float[length]);
  Interleave(data, samples_per_channel, num_channels, buffer.get());
  if (raw_file) {
    raw_file->WriteSamples(buffer.get(), length);
  }
  // TODO(aluebs): Use ScaleToInt16Range() from audio_util
  for (size_t i = 0; i < length; ++i) {
    buffer[i] = buffer[i] > 0
                    ? buffer[i] * std::numeric_limits<int16_t>::max()
                    : -buffer[i] * std::numeric_limits<int16_t>::min();
  }
  if (wav_file) {
    wav_file->WriteSamples(buffer.get(), length);
  }
}

// Exits on failure; do not use in unit tests.
FILE* OpenFile(const std::string& filename, const char* mode) {
  FILE* file = fopen(filename.c_str(), mode);
  RTC_CHECK(file) << "Unable to open file " << filename;
  return file;
}

void WriteData(const void* data,
               size_t size,
               FILE* file,
               const std::string& filename) {
  RTC_CHECK_EQ(fwrite(data, size, 1, file), 1)
      << "Error when writing to " << filename.c_str();
}

void WriteCallOrderData(const bool render_call,
                        FILE* file,
                        const std::string& filename) {
  const char call_type = render_call ? 'r' : 'c';
  WriteData(&call_type, sizeof(call_type), file, filename.c_str());
}

bool WritingCallOrderFile() {
  return absl::GetFlag(FLAGS_full);
}

bool WritingRuntimeSettingFiles() {
  return absl::GetFlag(FLAGS_full);
}

// Exports RuntimeSetting AEC dump events to Audacity-readable files.
// This class is not RAII compliant.
class RuntimeSettingWriter {
 public:
  RuntimeSettingWriter(
      std::string name,
      rtc::FunctionView<bool(const Event)> is_exporter_for,
      rtc::FunctionView<std::string(const Event)> get_timeline_label)
      : setting_name_(std::move(name)),
        is_exporter_for_(is_exporter_for),
        get_timeline_label_(get_timeline_label) {}
  ~RuntimeSettingWriter() { Flush(); }

  bool IsExporterFor(const Event& event) const {
    return is_exporter_for_(event);
  }

  // Writes to file the payload of `event` using `frame_count` to calculate
  // timestamp.
  void WriteEvent(const Event& event, int frame_count) {
    RTC_DCHECK(is_exporter_for_(event));
    if (file_ == nullptr) {
      rtc::StringBuilder file_name;
      file_name << setting_name_ << frame_offset_ << ".txt";
      file_ = OpenFile(file_name.str(), "wb");
    }

    // Time in the current WAV file, in seconds.
    double time = (frame_count - frame_offset_) / 100.0;
    std::string label = get_timeline_label_(event);
    // In Audacity, all annotations are encoded as intervals.
    fprintf(file_, "%.6f\t%.6f\t%s \n", time, time, label.c_str());
  }

  // Handles an AEC dump initialization event, occurring at frame
  // `frame_offset`.
  void HandleInitEvent(int frame_offset) {
    Flush();
    frame_offset_ = frame_offset;
  }

 private:
  void Flush() {
    if (file_ != nullptr) {
      fclose(file_);
      file_ = nullptr;
    }
  }

  FILE* file_ = nullptr;
  int frame_offset_ = 0;
  const std::string setting_name_;
  const rtc::FunctionView<bool(Event)> is_exporter_for_;
  const rtc::FunctionView<std::string(Event)> get_timeline_label_;
};

// Returns RuntimeSetting exporters for runtime setting types defined in
// debug.proto.
std::vector<RuntimeSettingWriter> RuntimeSettingWriters() {
  return {
      RuntimeSettingWriter(
          "CapturePreGain",
          [](const Event& event) -> bool {
            return event.runtime_setting().has_capture_pre_gain();
          },
          [](const Event& event) -> std::string {
            return std::to_string(event.runtime_setting().capture_pre_gain());
          }),
      RuntimeSettingWriter(
          "CustomRenderProcessingRuntimeSetting",
          [](const Event& event) -> bool {
            return event.runtime_setting()
                .has_custom_render_processing_setting();
          },
          [](const Event& event) -> std::string {
            return std::to_string(
                event.runtime_setting().custom_render_processing_setting());
          }),
      RuntimeSettingWriter(
          "CaptureFixedPostGain",
          [](const Event& event) -> bool {
            return event.runtime_setting().has_capture_fixed_post_gain();
          },
          [](const Event& event) -> std::string {
            return std::to_string(
                event.runtime_setting().capture_fixed_post_gain());
          }),
      RuntimeSettingWriter(
          "PlayoutVolumeChange",
          [](const Event& event) -> bool {
            return event.runtime_setting().has_playout_volume_change();
          },
          [](const Event& event) -> std::string {
            return std::to_string(
                event.runtime_setting().playout_volume_change());
          })};
}

std::string GetWavFileIndex(int init_index, int frame_count) {
  rtc::StringBuilder suffix;
  if (absl::GetFlag(FLAGS_use_init_suffix)) {
    suffix << "_" << init_index;
  } else {
    suffix << frame_count;
  }
  return suffix.str();
}

}  // namespace

int do_main(int argc, char* argv[]) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  std::string program_name = args[0];
  std::string usage =
      "Commandline tool to unpack audioproc debug files.\n"
      "Example usage:\n" +
      program_name + " debug_dump.pb\n";

  if (args.size() < 2) {
    printf("%s", usage.c_str());
    return 1;
  }

  FILE* debug_file = OpenFile(args[1], "rb");

  Event event_msg;
  int frame_count = 0;
  int init_count = 0;
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
  callorder_raw_name << absl::GetFlag(FLAGS_callorder_file) << ".char";
  FILE* callorder_char_file = WritingCallOrderFile()
                                  ? OpenFile(callorder_raw_name.str(), "wb")
                                  : nullptr;
  FILE* settings_file = OpenFile(absl::GetFlag(FLAGS_settings_file), "wb");

  std::vector<RuntimeSettingWriter> runtime_setting_writers =
      RuntimeSettingWriters();

  while (ReadMessageFromFile(debug_file, &event_msg)) {
    if (event_msg.type() == Event::REVERSE_STREAM) {
      if (!event_msg.has_reverse_stream()) {
        printf("Corrupt input file: ReverseStream missing.\n");
        return 1;
      }

      const ReverseStream msg = event_msg.reverse_stream();
      if (msg.has_data()) {
        if (absl::GetFlag(FLAGS_raw) && !reverse_raw_file) {
          reverse_raw_file.reset(
              new RawFile(absl::GetFlag(FLAGS_reverse_file) + ".pcm"));
        }
        // TODO(aluebs): Replace "num_reverse_channels *
        // reverse_samples_per_channel" with "msg.data().size() /
        // sizeof(int16_t)" and so on when this fix in audio_processing has made
        // it into stable: https://webrtc-codereview.appspot.com/15299004/
        WriteIntData(reinterpret_cast<const int16_t*>(msg.data().data()),
                     num_reverse_channels * reverse_samples_per_channel,
                     reverse_wav_file.get(), reverse_raw_file.get());
      } else if (msg.channel_size() > 0) {
        if (absl::GetFlag(FLAGS_raw) && !reverse_raw_file) {
          reverse_raw_file.reset(
              new RawFile(absl::GetFlag(FLAGS_reverse_file) + ".float"));
        }
        std::unique_ptr<const float*[]> data(
            new const float*[num_reverse_channels]);
        for (size_t i = 0; i < num_reverse_channels; ++i) {
          data[i] = reinterpret_cast<const float*>(msg.channel(i).data());
        }
        WriteFloatData(data.get(), reverse_samples_per_channel,
                       num_reverse_channels, reverse_wav_file.get(),
                       reverse_raw_file.get());
      }
      if (absl::GetFlag(FLAGS_full)) {
        if (WritingCallOrderFile()) {
          WriteCallOrderData(true /* render_call */, callorder_char_file,
                             absl::GetFlag(FLAGS_callorder_file));
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
        if (absl::GetFlag(FLAGS_raw) && !input_raw_file) {
          input_raw_file.reset(
              new RawFile(absl::GetFlag(FLAGS_input_file) + ".pcm"));
        }
        WriteIntData(reinterpret_cast<const int16_t*>(msg.input_data().data()),
                     num_input_channels * input_samples_per_channel,
                     input_wav_file.get(), input_raw_file.get());
      } else if (msg.input_channel_size() > 0) {
        if (absl::GetFlag(FLAGS_raw) && !input_raw_file) {
          input_raw_file.reset(
              new RawFile(absl::GetFlag(FLAGS_input_file) + ".float"));
        }
        std::unique_ptr<const float*[]> data(
            new const float*[num_input_channels]);
        for (size_t i = 0; i < num_input_channels; ++i) {
          data[i] = reinterpret_cast<const float*>(msg.input_channel(i).data());
        }
        WriteFloatData(data.get(), input_samples_per_channel,
                       num_input_channels, input_wav_file.get(),
                       input_raw_file.get());
      }

      if (msg.has_output_data()) {
        if (absl::GetFlag(FLAGS_raw) && !output_raw_file) {
          output_raw_file.reset(
              new RawFile(absl::GetFlag(FLAGS_output_file) + ".pcm"));
        }
        WriteIntData(reinterpret_cast<const int16_t*>(msg.output_data().data()),
                     num_output_channels * output_samples_per_channel,
                     output_wav_file.get(), output_raw_file.get());
      } else if (msg.output_channel_size() > 0) {
        if (absl::GetFlag(FLAGS_raw) && !output_raw_file) {
          output_raw_file.reset(
              new RawFile(absl::GetFlag(FLAGS_output_file) + ".float"));
        }
        std::unique_ptr<const float*[]> data(
            new const float*[num_output_channels]);
        for (size_t i = 0; i < num_output_channels; ++i) {
          data[i] =
              reinterpret_cast<const float*>(msg.output_channel(i).data());
        }
        WriteFloatData(data.get(), output_samples_per_channel,
                       num_output_channels, output_wav_file.get(),
                       output_raw_file.get());
      }

      if (absl::GetFlag(FLAGS_full)) {
        if (WritingCallOrderFile()) {
          WriteCallOrderData(false /* render_call */, callorder_char_file,
                             absl::GetFlag(FLAGS_callorder_file));
        }
        if (msg.has_delay()) {
          static FILE* delay_file =
              OpenFile(absl::GetFlag(FLAGS_delay_file), "wb");
          int32_t delay = msg.delay();
          if (absl::GetFlag(FLAGS_text)) {
            fprintf(delay_file, "%d\n", delay);
          } else {
            WriteData(&delay, sizeof(delay), delay_file,
                      absl::GetFlag(FLAGS_delay_file));
          }
        }

        if (msg.has_drift()) {
          static FILE* drift_file =
              OpenFile(absl::GetFlag(FLAGS_drift_file), "wb");
          int32_t drift = msg.drift();
          if (absl::GetFlag(FLAGS_text)) {
            fprintf(drift_file, "%d\n", drift);
          } else {
            WriteData(&drift, sizeof(drift), drift_file,
                      absl::GetFlag(FLAGS_drift_file));
          }
        }

        if (msg.has_level()) {
          static FILE* level_file =
              OpenFile(absl::GetFlag(FLAGS_level_file), "wb");
          int32_t level = msg.level();
          if (absl::GetFlag(FLAGS_text)) {
            fprintf(level_file, "%d\n", level);
          } else {
            WriteData(&level, sizeof(level), level_file,
                      absl::GetFlag(FLAGS_level_file));
          }
        }

        if (msg.has_keypress()) {
          static FILE* keypress_file =
              OpenFile(absl::GetFlag(FLAGS_keypress_file), "wb");
          bool keypress = msg.keypress();
          if (absl::GetFlag(FLAGS_text)) {
            fprintf(keypress_file, "%d\n", keypress);
          } else {
            WriteData(&keypress, sizeof(keypress), keypress_file,
                      absl::GetFlag(FLAGS_keypress_file));
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

      ++init_count;
      const Init msg = event_msg.init();
      // These should print out zeros if they're missing.
      fprintf(settings_file, "Init #%d at frame: %d\n", init_count,
              frame_count);
      int input_sample_rate = msg.sample_rate();
      fprintf(settings_file, "  Input sample rate: %d\n", input_sample_rate);
      int output_sample_rate = msg.output_sample_rate();
      fprintf(settings_file, "  Output sample rate: %d\n", output_sample_rate);
      int reverse_sample_rate = msg.reverse_sample_rate();
      fprintf(settings_file, "  Reverse sample rate: %d\n",
              reverse_sample_rate);
      num_input_channels = msg.num_input_channels();
      fprintf(settings_file, "  Input channels: %zu\n", num_input_channels);
      num_output_channels = msg.num_output_channels();
      fprintf(settings_file, "  Output channels: %zu\n", num_output_channels);
      num_reverse_channels = msg.num_reverse_channels();
      fprintf(settings_file, "  Reverse channels: %zu\n", num_reverse_channels);
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

      if (!absl::GetFlag(FLAGS_raw)) {
        // The WAV files need to be reset every time, because they cant change
        // their sample rate or number of channels.

        std::string suffix = GetWavFileIndex(init_count, frame_count);
        rtc::StringBuilder reverse_name;
        reverse_name << absl::GetFlag(FLAGS_reverse_file) << suffix << ".wav";
        reverse_wav_file.reset(new WavWriter(
            reverse_name.str(), reverse_sample_rate, num_reverse_channels));
        rtc::StringBuilder input_name;
        input_name << absl::GetFlag(FLAGS_input_file) << suffix << ".wav";
        input_wav_file.reset(new WavWriter(input_name.str(), input_sample_rate,
                                           num_input_channels));
        rtc::StringBuilder output_name;
        output_name << absl::GetFlag(FLAGS_output_file) << suffix << ".wav";
        output_wav_file.reset(new WavWriter(
            output_name.str(), output_sample_rate, num_output_channels));

        if (WritingCallOrderFile()) {
          rtc::StringBuilder callorder_name;
          callorder_name << absl::GetFlag(FLAGS_callorder_file) << suffix
                         << ".char";
          callorder_char_file = OpenFile(callorder_name.str(), "wb");
        }

        if (WritingRuntimeSettingFiles()) {
          for (RuntimeSettingWriter& writer : runtime_setting_writers) {
            writer.HandleInitEvent(frame_count);
          }
        }
      }
    } else if (event_msg.type() == Event::RUNTIME_SETTING) {
      if (WritingRuntimeSettingFiles()) {
        for (RuntimeSettingWriter& writer : runtime_setting_writers) {
          if (writer.IsExporterFor(event_msg)) {
            writer.WriteEvent(event_msg, frame_count);
          }
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
