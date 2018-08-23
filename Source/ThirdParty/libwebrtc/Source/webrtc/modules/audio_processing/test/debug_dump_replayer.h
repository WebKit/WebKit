/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_DEBUG_DUMP_REPLAYER_H_
#define MODULES_AUDIO_PROCESSING_TEST_DEBUG_DUMP_REPLAYER_H_

#include <memory>
#include <string>

#include "common_audio/channel_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/ignore_wundef.h"

RTC_PUSH_IGNORING_WUNDEF()
#include "modules/audio_processing/debug.pb.h"
RTC_POP_IGNORING_WUNDEF()

namespace webrtc {
namespace test {

class DebugDumpReplayer {
 public:
  DebugDumpReplayer();
  ~DebugDumpReplayer();

  // Set dump file
  bool SetDumpFile(const std::string& filename);

  // Return next event.
  absl::optional<audioproc::Event> GetNextEvent() const;

  // Run the next event. Returns true if succeeded.
  bool RunNextEvent();

  const ChannelBuffer<float>* GetOutput() const;
  StreamConfig GetOutputConfig() const;

 private:
  // Following functions are facilities for replaying debug dumps.
  void OnInitEvent(const audioproc::Init& msg);
  void OnStreamEvent(const audioproc::Stream& msg);
  void OnReverseStreamEvent(const audioproc::ReverseStream& msg);
  void OnConfigEvent(const audioproc::Config& msg);

  void MaybeRecreateApm(const audioproc::Config& msg);
  void ConfigureApm(const audioproc::Config& msg);

  void LoadNextMessage();

  // Buffer for APM input/output.
  std::unique_ptr<ChannelBuffer<float>> input_;
  std::unique_ptr<ChannelBuffer<float>> reverse_;
  std::unique_ptr<ChannelBuffer<float>> output_;

  std::unique_ptr<AudioProcessing> apm_;

  FILE* debug_file_;

  StreamConfig input_config_;
  StreamConfig reverse_config_;
  StreamConfig output_config_;

  bool has_next_event_;
  audioproc::Event next_event_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_DEBUG_DUMP_REPLAYER_H_
