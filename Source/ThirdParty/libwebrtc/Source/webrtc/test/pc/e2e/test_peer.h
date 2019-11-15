/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_TEST_PEER_H_
#define TEST_PC_E2E_TEST_PEER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "media/base/media_engine.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/network.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread.h"
#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"
#include "test/pc/e2e/peer_connection_quality_test_params.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Describes a single participant in the call.
class TestPeer final : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;
  using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
  using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;
  using EchoEmulationConfig =
      PeerConnectionE2EQualityTestFixture::EchoEmulationConfig;

  struct RemotePeerAudioConfig {
    RemotePeerAudioConfig(AudioConfig config)
        : sampling_frequency_in_hz(config.sampling_frequency_in_hz),
          output_file_name(config.output_dump_file_name) {}

    int sampling_frequency_in_hz;
    absl::optional<std::string> output_file_name;
  };

  static absl::optional<RemotePeerAudioConfig> CreateRemoteAudioConfig(
      absl::optional<AudioConfig> config);

  // Setups all components, that should be provided to WebRTC
  // PeerConnectionFactory and PeerConnection creation methods,
  // also will setup dependencies, that are required for media analyzers
  // injection.
  //
  // |signaling_thread| will be provided by test fixture implementation.
  // |params| - describes current peer parameters, like current peer video
  // streams and audio streams
  static std::unique_ptr<TestPeer> CreateTestPeer(
      std::unique_ptr<InjectableComponents> components,
      std::unique_ptr<Params> params,
      std::unique_ptr<MockPeerConnectionObserver> observer,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
      rtc::Thread* signaling_thread,
      absl::optional<RemotePeerAudioConfig> remote_audio_config,
      double bitrate_multiplier,
      absl::optional<EchoEmulationConfig> echo_emulation_config,
      rtc::TaskQueue* task_queue);

  Params* params() const { return params_.get(); }
  void DetachAecDump() { audio_processing_->DetachAecDump(); }

  // Adds provided |candidates| to the owned peer connection.
  bool AddIceCandidates(
      std::vector<std::unique_ptr<IceCandidateInterface>> candidates);

 private:
  TestPeer(rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
           rtc::scoped_refptr<PeerConnectionInterface> pc,
           std::unique_ptr<MockPeerConnectionObserver> observer,
           std::unique_ptr<Params> params,
           rtc::scoped_refptr<AudioProcessing> audio_processing);

  std::unique_ptr<Params> params_;
  rtc::scoped_refptr<AudioProcessing> audio_processing_;

  std::vector<std::unique_ptr<IceCandidateInterface>> remote_ice_candidates_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_TEST_PEER_H_
