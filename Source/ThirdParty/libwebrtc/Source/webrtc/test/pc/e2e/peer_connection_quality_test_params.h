/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PC_E2E_PEER_CONNECTION_QUALITY_TEST_PARAMS_H_
#define TEST_PC_E2E_PEER_CONNECTION_QUALITY_TEST_PARAMS_H_

#include <memory>
#include <string>
#include <vector>

#include "api/async_resolver_factory.h"
#include "api/call/call_factory_interface.h"
#include "api/fec_controller.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/transport/media/media_transport_interface.h"
#include "api/transport/network_control.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Contains most part from PeerConnectionFactoryDependencies. Also all fields
// are optional and defaults will be provided by fixture implementation if
// any will be omitted.
//
// Separate class was introduced to clarify which components can be
// overridden. For example worker and signaling threads will be provided by
// fixture implementation. The same is applicable to the media engine. So user
// can override only some parts of media engine like video encoder/decoder
// factories.
struct PeerConnectionFactoryComponents {
  std::unique_ptr<TaskQueueFactory> task_queue_factory;
  std::unique_ptr<CallFactoryInterface> call_factory;
  std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory;
  std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory;
  std::unique_ptr<NetworkControllerFactoryInterface> network_controller_factory;
  std::unique_ptr<MediaTransportFactory> media_transport_factory;
  std::unique_ptr<NetEqFactory> neteq_factory;

  // Will be passed to MediaEngineInterface, that will be used in
  // PeerConnectionFactory.
  std::unique_ptr<VideoEncoderFactory> video_encoder_factory;
  std::unique_ptr<VideoDecoderFactory> video_decoder_factory;
};

// Contains most parts from PeerConnectionDependencies. Also all fields are
// optional and defaults will be provided by fixture implementation if any
// will be omitted.
//
// Separate class was introduced to clarify which components can be
// overridden. For example observer, which is required to
// PeerConnectionDependencies, will be provided by fixture implementation,
// so client can't inject its own. Also only network manager can be overridden
// inside port allocator.
struct PeerConnectionComponents {
  explicit PeerConnectionComponents(rtc::NetworkManager* network_manager)
      : network_manager(network_manager) {
    RTC_CHECK(network_manager);
  }

  rtc::NetworkManager* const network_manager;
  std::unique_ptr<webrtc::AsyncResolverFactory> async_resolver_factory;
  std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator;
  std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier;
  std::unique_ptr<IceTransportFactory> ice_transport_factory;
};

// Contains all components, that can be overridden in peer connection. Also
// has a network thread, that will be used to communicate with another peers.
struct InjectableComponents {
  explicit InjectableComponents(rtc::Thread* network_thread,
                                rtc::NetworkManager* network_manager)
      : network_thread(network_thread),
        pcf_dependencies(std::make_unique<PeerConnectionFactoryComponents>()),
        pc_dependencies(
            std::make_unique<PeerConnectionComponents>(network_manager)) {
    RTC_CHECK(network_thread);
  }

  rtc::Thread* const network_thread;

  std::unique_ptr<PeerConnectionFactoryComponents> pcf_dependencies;
  std::unique_ptr<PeerConnectionComponents> pc_dependencies;
};

// Contains information about call media streams (up to 1 audio stream and
// unlimited amount of video streams) and rtc configuration, that will be used
// to set up peer connection.
struct Params {
  // If |video_configs| is empty - no video should be added to the test call.
  std::vector<PeerConnectionE2EQualityTestFixture::VideoConfig> video_configs;
  // If |audio_config| is set audio stream will be configured
  absl::optional<PeerConnectionE2EQualityTestFixture::AudioConfig> audio_config;
  // If |rtc_event_log_path| is set, an RTCEventLog will be saved in that
  // location and it will be available for further analysis.
  absl::optional<std::string> rtc_event_log_path;
  // If |aec_dump_path| is set, an AEC dump will be saved in that location and
  // it will be available for further analysis.
  absl::optional<std::string> aec_dump_path;

  PeerConnectionInterface::RTCConfiguration rtc_configuration;
  PeerConnectionInterface::BitrateParameters bitrate_params;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_PEER_CONNECTION_QUALITY_TEST_PARAMS_H_
