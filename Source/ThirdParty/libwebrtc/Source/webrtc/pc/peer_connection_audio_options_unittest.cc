/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>
#include <vector>

#include "api/audio_options.h"
#include "api/create_modular_peer_connection_factory.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/set_local_description_observer_interface.h"
#include "media/base/fake_media_engine.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_proxy.h"
#include "pc/rtp_transceiver.h"
#include "pc/test/enable_fake_media.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"

namespace webrtc {
namespace {

using test::RunLoop;
using ::testing::IsNull;
using ::testing::NotNull;

// Helper observers that quit the run loop.
class QuitOnSuccessCreateObserver
    : public MockCreateSessionDescriptionObserver {
 public:
  explicit QuitOnSuccessCreateObserver(RunLoop& loop) : loop_(loop) {}
  void OnSuccess(SessionDescriptionInterface* desc) override {
    MockCreateSessionDescriptionObserver::OnSuccess(desc);
    loop_.Quit();
  }
  void OnFailure(RTCError error) override {
    MockCreateSessionDescriptionObserver::OnFailure(error);
    loop_.Quit();
  }

 private:
  RunLoop& loop_;
};

class QuitOnSuccessSetObserver : public SetLocalDescriptionObserverInterface {
 public:
  explicit QuitOnSuccessSetObserver(RunLoop& loop) : loop_(loop) {}
  static scoped_refptr<QuitOnSuccessSetObserver> Create(RunLoop& loop) {
    return make_ref_counted<QuitOnSuccessSetObserver>(loop);
  }
  void OnSetLocalDescriptionComplete(RTCError error) override {
    EXPECT_TRUE(error.ok());
    was_called_ = true;
    loop_.Quit();
  }
  bool called() const { return was_called_; }

 private:
  RunLoop& loop_;
  bool was_called_ = false;
};

}  // namespace

class PeerConnectionAudioOptionsTest : public ::testing::Test {
 public:
  PeerConnectionAudioOptionsTest()
      : worker_thread_(Thread::Create()),
        network_thread_(Thread::CreateWithSocketServer()) {
    network_thread_->Start();
    worker_thread_->Start();

    PeerConnectionFactoryDependencies dependencies;
    dependencies.network_thread = network_thread_.get();
    dependencies.worker_thread = worker_thread_.get();
    dependencies.signaling_thread = Thread::Current();

    EnableFakeMedia(dependencies, std::make_unique<FakeMediaEngine>());

    pc_factory_ = CreateModularPeerConnectionFactory(std::move(dependencies));
  }

  RTCError CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& configuration) {
    RTC_DCHECK(!pc_);
    auto result = pc_factory_->CreatePeerConnectionOrError(
        configuration, PeerConnectionDependencies(&observer_));
    if (!result.ok()) {
      return result.MoveError();
    }
    pc_ = result.MoveValue();
    observer_.SetPeerConnectionInterface(pc_.get());
    return result.MoveError();
  }

  // Returns a pointer to the internal PeerConnection implementation.
  PeerConnection* pc() {
    RTC_DCHECK(pc_);
    auto* proxy =
        static_cast<PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(
            pc_.get());
    return static_cast<PeerConnection*>(proxy->internal());
  }

  RunLoop loop_;
  std::unique_ptr<Thread> worker_thread_;
  std::unique_ptr<Thread> network_thread_;
  scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  scoped_refptr<PeerConnectionInterface> pc_;
  MockPeerConnectionObserver observer_;
};

TEST_F(PeerConnectionAudioOptionsTest, AudioOptionsAppliedOnCreateChannel) {
  PeerConnectionInterface::RTCConfiguration config;
  // Set specific audio jitter buffer options in the configuration.
  config.audio_jitter_buffer_max_packets = 123;
  config.audio_jitter_buffer_fast_accelerate = true;
  auto result = CreatePeerConnection(config);
  ASSERT_TRUE(result.ok());

  // Add an audio transceiver. Verify that the internal channel() has not been
  // created yet.
  auto transceiver_result = pc()->AddTransceiver(MediaType::AUDIO);
  ASSERT_TRUE(transceiver_result.ok());
  auto transceivers = pc()->GetTransceiversInternal();
  ASSERT_EQ(transceivers.size(), 1u);
  auto* transceiver_impl = transceivers[0]->internal();
  ASSERT_FALSE(transceiver_impl->HasChannel());

  // Create offer and set local description to trigger CreateChannel.
  auto offer_observer = make_ref_counted<QuitOnSuccessCreateObserver>(loop_);
  pc()->CreateOffer(offer_observer.get(),
                    PeerConnectionInterface::RTCOfferAnswerOptions());
  loop_.Run();
  EXPECT_TRUE(offer_observer->called());
  scoped_refptr<QuitOnSuccessSetObserver> sld_observer =
      QuitOnSuccessSetObserver::Create(loop_);
  pc()->SetLocalDescription(offer_observer->MoveDescription(), sld_observer);
  loop_.Run();
  EXPECT_TRUE(sld_observer->called());

  // Verify that now the channel() exists and that the options were applied to
  // the voice engine.
  ASSERT_TRUE(transceiver_impl->HasChannel());

  auto* media_channel = transceiver_impl->media_receive_channel();
  ASSERT_TRUE(media_channel);
  auto* voice_channel =
      static_cast<FakeVoiceMediaReceiveChannel*>(media_channel);
  EXPECT_EQ(voice_channel->options().audio_jitter_buffer_max_packets, 123);
  EXPECT_EQ(voice_channel->options().audio_jitter_buffer_fast_accelerate, true);
}
}  // namespace webrtc
