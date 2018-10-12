/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/scenario_config.h"

namespace webrtc {
namespace test {

TransportControllerConfig::Rates::Rates() = default;
TransportControllerConfig::Rates::Rates(
    const TransportControllerConfig::Rates&) = default;
TransportControllerConfig::Rates::~Rates() = default;

PacketStreamConfig::PacketStreamConfig() = default;
PacketStreamConfig::PacketStreamConfig(const PacketStreamConfig&) = default;
PacketStreamConfig::~PacketStreamConfig() = default;

VideoStreamConfig::Encoder::Encoder() = default;
VideoStreamConfig::Encoder::Encoder(const VideoStreamConfig::Encoder&) =
    default;
VideoStreamConfig::Encoder::~Encoder() = default;

VideoStreamConfig::Stream::Stream() = default;
VideoStreamConfig::Stream::Stream(const VideoStreamConfig::Stream&) = default;
VideoStreamConfig::Stream::~Stream() = default;

AudioStreamConfig::AudioStreamConfig() = default;
AudioStreamConfig::AudioStreamConfig(const AudioStreamConfig&) = default;
AudioStreamConfig::~AudioStreamConfig() = default;

AudioStreamConfig::Encoder::Encoder() = default;
AudioStreamConfig::Encoder::Encoder(const AudioStreamConfig::Encoder&) =
    default;
AudioStreamConfig::Encoder::~Encoder() = default;

AudioStreamConfig::Stream::Stream() = default;
AudioStreamConfig::Stream::Stream(const AudioStreamConfig::Stream&) = default;
AudioStreamConfig::Stream::~Stream() = default;

NetworkNodeConfig::NetworkNodeConfig() = default;
NetworkNodeConfig::NetworkNodeConfig(const NetworkNodeConfig&) = default;
NetworkNodeConfig::~NetworkNodeConfig() = default;

NetworkNodeConfig::Simulation::Simulation() = default;
NetworkNodeConfig::Simulation::Simulation(
    const NetworkNodeConfig::Simulation&) = default;
NetworkNodeConfig::Simulation::~Simulation() = default;

CrossTrafficConfig::CrossTrafficConfig() = default;
CrossTrafficConfig::CrossTrafficConfig(const CrossTrafficConfig&) = default;
CrossTrafficConfig::~CrossTrafficConfig() = default;

}  // namespace test
}  // namespace webrtc
