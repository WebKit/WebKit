/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/audio_device.h"
#include "api/audio/audio_processing.h"
#include "api/audio/builtin_audio_processing_builder.h"
#include "api/audio_codecs/audio_decoder_factory_template.h"
#include "api/audio_codecs/audio_encoder_factory_template.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/create_peerconnection_factory.h"
#include "api/enable_media.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/stats/rtcstats_objects.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"

namespace webrtc {

void CreateSomeMediaDeps(PeerConnectionFactoryDependencies& media_deps) {
  media_deps.adm = AudioDeviceModule::CreateForTest(
      AudioDeviceModule::kDummyAudio, media_deps.task_queue_factory.get());
  media_deps.audio_encoder_factory =
      webrtc::CreateAudioEncoderFactory<webrtc::AudioEncoderOpus>();
  media_deps.audio_decoder_factory =
      webrtc::CreateAudioDecoderFactory<webrtc::AudioDecoderOpus>();
  media_deps.video_encoder_factory =
      std::make_unique<VideoEncoderFactoryTemplate<
          LibvpxVp8EncoderTemplateAdapter, LibvpxVp9EncoderTemplateAdapter,
          OpenH264EncoderTemplateAdapter, LibaomAv1EncoderTemplateAdapter>>();
  media_deps.video_decoder_factory =
      std::make_unique<VideoDecoderFactoryTemplate<
          LibvpxVp8DecoderTemplateAdapter, LibvpxVp9DecoderTemplateAdapter,
          OpenH264DecoderTemplateAdapter, Dav1dDecoderTemplateAdapter>>();
  media_deps.audio_processing_builder =
      std::make_unique<BuiltinAudioProcessingBuilder>();
}

webrtc::PeerConnectionFactoryDependencies CreateSomePcfDeps() {
  webrtc::PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.task_queue_factory = CreateDefaultTaskQueueFactory();
  pcf_deps.signaling_thread = rtc::Thread::Current();
  pcf_deps.network_thread = rtc::Thread::Current();
  pcf_deps.worker_thread = rtc::Thread::Current();
  pcf_deps.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>();
  CreateSomeMediaDeps(pcf_deps);
  EnableMedia(pcf_deps);
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
  PeerConnectionFactoryDependencies media_deps;
  media_deps.task_queue_factory = CreateDefaultTaskQueueFactory();
  CreateSomeMediaDeps(media_deps);

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
