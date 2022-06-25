/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/audio_decoder_factory_template.h"
#include "api/audio_codecs/audio_encoder_factory_template.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/call/call_factory_interface.h"
#include "api/create_peerconnection_factory.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/stats/rtcstats_objects.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "media/engine/webrtc_media_engine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

cricket::MediaEngineDependencies CreateSomeMediaDeps(
    TaskQueueFactory* task_queue_factory) {
  cricket::MediaEngineDependencies media_deps;
  media_deps.task_queue_factory = task_queue_factory;
  media_deps.adm = AudioDeviceModule::CreateForTest(
      AudioDeviceModule::kDummyAudio, task_queue_factory);
  media_deps.audio_encoder_factory =
      webrtc::CreateAudioEncoderFactory<webrtc::AudioEncoderOpus>();
  media_deps.audio_decoder_factory =
      webrtc::CreateAudioDecoderFactory<webrtc::AudioDecoderOpus>();
  media_deps.video_encoder_factory = CreateBuiltinVideoEncoderFactory();
  media_deps.video_decoder_factory = webrtc::CreateBuiltinVideoDecoderFactory();
  media_deps.audio_processing = webrtc::AudioProcessingBuilder().Create();
  return media_deps;
}

webrtc::PeerConnectionFactoryDependencies CreateSomePcfDeps() {
  webrtc::PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.task_queue_factory = CreateDefaultTaskQueueFactory();
  pcf_deps.signaling_thread = rtc::Thread::Current();
  pcf_deps.network_thread = rtc::Thread::Current();
  pcf_deps.worker_thread = rtc::Thread::Current();
  pcf_deps.call_factory = webrtc::CreateCallFactory();
  pcf_deps.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>(
      pcf_deps.task_queue_factory.get());
  auto media_deps = CreateSomeMediaDeps(pcf_deps.task_queue_factory.get());
  pcf_deps.media_engine = cricket::CreateMediaEngine(std::move(media_deps));
  return pcf_deps;
}

// NOTE: These "test cases" should pull in as much of WebRTC as possible to make
// sure most commonly used symbols are actually in libwebrtc.a. It's entirely
// possible these tests won't work at all times (maybe crash even), but that's
// fine.
void TestCase1ModularFactory() {
  auto pcf_deps = CreateSomePcfDeps();
  auto peer_connection_factory =
      webrtc::CreateModularPeerConnectionFactory(std::move(pcf_deps));
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  auto result = peer_connection_factory->CreatePeerConnectionOrError(
      rtc_config, PeerConnectionDependencies(nullptr));
  // Creation will fail because of null observer, but that's OK.
  printf("peer_connection creation=%s\n", result.ok() ? "succeeded" : "failed");
}

void TestCase2RegularFactory() {
  auto task_queue_factory = CreateDefaultTaskQueueFactory();
  auto media_deps = CreateSomeMediaDeps(task_queue_factory.get());

  auto peer_connection_factory = webrtc::CreatePeerConnectionFactory(
      rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
      std::move(media_deps.adm), std::move(media_deps.audio_encoder_factory),
      std::move(media_deps.audio_decoder_factory),
      std::move(media_deps.video_encoder_factory),
      std::move(media_deps.video_decoder_factory), nullptr, nullptr);
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  auto result = peer_connection_factory->CreatePeerConnectionOrError(
      rtc_config, PeerConnectionDependencies(nullptr));
  // Creation will fail because of null observer, but that's OK.
  printf("peer_connection creation=%s\n", result.ok() ? "succeeded" : "failed");
}

}  // namespace webrtc

int main(int argc, char** argv) {
  webrtc::TestCase1ModularFactory();
  webrtc::TestCase2RegularFactory();
  return 0;
}
