/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "modules/audio_coding/neteq/include/neteq.h"
#include "modules/audio_coding/neteq/tools/audio_sink.h"
#include "modules/audio_coding/neteq/tools/neteq_input.h"

namespace webrtc {
namespace test {

class NetEqTestErrorCallback {
 public:
  virtual ~NetEqTestErrorCallback() = default;
  virtual void OnInsertPacketError(const NetEqInput::PacketData& packet) {}
  virtual void OnGetAudioError() {}
};

class DefaultNetEqTestErrorCallback : public NetEqTestErrorCallback {
  void OnInsertPacketError(const NetEqInput::PacketData& packet) override;
  void OnGetAudioError() override;
};

class NetEqPostInsertPacket {
 public:
  virtual ~NetEqPostInsertPacket() = default;
  virtual void AfterInsertPacket(const NetEqInput::PacketData& packet,
                                 NetEq* neteq) = 0;
};

class NetEqGetAudioCallback {
 public:
  virtual ~NetEqGetAudioCallback() = default;
  virtual void BeforeGetAudio(NetEq* neteq) = 0;
  virtual void AfterGetAudio(int64_t time_now_ms,
                             const AudioFrame& audio_frame,
                             bool muted,
                             NetEq* neteq) = 0;
};

// Class that provides an input--output test for NetEq. The input (both packets
// and output events) is provided by a NetEqInput object, while the output is
// directed to an AudioSink object.
class NetEqTest {
 public:
  using DecoderMap = std::map<int, std::pair<NetEqDecoder, std::string> >;

  struct ExternalDecoderInfo {
    AudioDecoder* decoder;
    NetEqDecoder codec;
    std::string codec_name;
  };

  using ExtDecoderMap = std::map<int, ExternalDecoderInfo>;

  struct Callbacks {
    NetEqTestErrorCallback* error_callback = nullptr;
    NetEqPostInsertPacket* post_insert_packet = nullptr;
    NetEqGetAudioCallback* get_audio_callback = nullptr;
  };

  // Sets up the test with given configuration, codec mappings, input, ouput,
  // and callback objects for error reporting.
  NetEqTest(const NetEq::Config& config,
            const DecoderMap& codecs,
            const ExtDecoderMap& ext_codecs,
            std::unique_ptr<NetEqInput> input,
            std::unique_ptr<AudioSink> output,
            Callbacks callbacks);

  ~NetEqTest() = default;

  // Runs the test. Returns the duration of the produced audio in ms.
  int64_t Run();

  // Returns the statistics from NetEq.
  NetEqNetworkStatistics SimulationStats();

 private:
  void RegisterDecoders(const DecoderMap& codecs);
  void RegisterExternalDecoders(const ExtDecoderMap& codecs);

  std::unique_ptr<NetEq> neteq_;
  std::unique_ptr<NetEqInput> input_;
  std::unique_ptr<AudioSink> output_;
  Callbacks callbacks_;
  int sample_rate_hz_;
};

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_H_
