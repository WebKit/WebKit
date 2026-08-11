/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/peer_connection_interface.h"

#include <optional>
#include <string>
#include <vector>

#include "api/crypto/crypto_options.h"
#include "api/scoped_refptr.h"
#include "api/transport/enums.h"
#include "api/turn_customizer.h"
#include "api/units/time_delta.h"
#include "media/base/media_config.h"
#include "pc/media_factory.h"
#include "rtc_base/network.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/rtc_certificate.h"

namespace webrtc {

PeerConnectionInterface::IceServer::IceServer() = default;
PeerConnectionInterface::IceServer::IceServer(const IceServer& rhs) = default;
PeerConnectionInterface::IceServer::~IceServer() = default;

PeerConnectionInterface::RTCConfiguration::RTCConfiguration() = default;

PeerConnectionInterface::RTCConfiguration::RTCConfiguration(
    const RTCConfiguration& rhs) = default;

PeerConnectionInterface::RTCConfiguration::RTCConfiguration(
    RTCConfigurationType type) {
  if (type == RTCConfigurationType::kAggressive) {
    // These parameters are also defined in Java and IOS configurations,
    // so their values may be overwritten by the Java or IOS configuration.
    bundle_policy = kBundlePolicyMaxBundle;
    rtcp_mux_policy = kRtcpMuxPolicyRequire;
    ice_connection_receiving_timeout = kAggressiveIceConnectionReceivingTimeout;

    // These parameters are not defined in Java or IOS configuration,
    // so their values will not be overwritten.
    enable_ice_renomination = true;
    redetermine_role_on_ice_restart = false;
  }
}

PeerConnectionInterface::RTCConfiguration::~RTCConfiguration() = default;

PeerConnectionDependencies::PeerConnectionDependencies(
    PeerConnectionObserver* observer_in)
    : observer(observer_in) {}

PeerConnectionDependencies::PeerConnectionDependencies(
    PeerConnectionDependencies&&) = default;

PeerConnectionDependencies::~PeerConnectionDependencies() = default;

PeerConnectionFactoryDependencies::PeerConnectionFactoryDependencies() =
    default;

PeerConnectionFactoryDependencies::PeerConnectionFactoryDependencies(
    PeerConnectionFactoryDependencies&&) = default;

PeerConnectionFactoryDependencies::~PeerConnectionFactoryDependencies() =
    default;

bool PeerConnectionInterface::RTCConfiguration::operator==(
    const PeerConnectionInterface::RTCConfiguration& o) const {
  // This static_assert prevents us from accidentally breaking operator==.
  // Note: Order matters! Fields must be ordered the same as RTCConfiguration.
  struct stuff_being_tested_for_equality {
    IceServers servers;
    IceTransportsType type;
    BundlePolicy bundle_policy;
    RtcpMuxPolicy rtcp_mux_policy;
    std::vector<scoped_refptr<RTCCertificate>> certificates;
    int ice_candidate_pool_size;
    bool disable_ipv6_on_wifi;
    int max_ipv6_networks;
    bool disable_link_local_networks;
    std::optional<int> screencast_min_bitrate;
    TcpCandidatePolicy tcp_candidate_policy;
    CandidateNetworkPolicy candidate_network_policy;
    int audio_jitter_buffer_max_packets;
    bool audio_jitter_buffer_fast_accelerate;
    int audio_jitter_buffer_min_delay_ms;
    int ice_connection_receiving_timeout;
    int ice_backup_candidate_pair_ping_interval;
    ContinualGatheringPolicy continual_gathering_policy;
    bool prioritize_most_likely_ice_candidate_pairs;
    struct MediaConfig media_config;
    bool prune_turn_ports;
    PortPrunePolicy turn_port_prune_policy;
    bool presume_writable_when_fully_relayed;
    bool enable_ice_renomination;
    bool redetermine_role_on_ice_restart;
    bool surface_ice_candidates_on_ice_transport_type_changed;
    std::optional<int> ice_check_interval_strong_connectivity;
    std::optional<int> ice_check_interval_weak_connectivity;
    std::optional<int> ice_check_min_interval;
    std::optional<int> ice_unwritable_timeout;
    std::optional<int> ice_unwritable_min_checks;
    std::optional<int> ice_inactive_timeout;
    std::optional<int> stun_candidate_keepalive_interval;
    TurnCustomizer* turn_customizer;
    SdpSemantics sdp_semantics;
    std::optional<AdapterType> network_preference;
    CryptoOptions crypto_options;
    bool offer_extmap_allow_mixed;
    std::string turn_logging_id;
    bool enable_implicit_rollback;
    std::optional<int> report_usage_pattern_delay_ms;
    std::optional<int> stable_writable_connection_ping_interval_ms;
    VpnPreference vpn_preference;
    std::vector<NetworkMask> vpn_list;
    PortAllocatorConfig port_allocator_config;
    std::optional<TimeDelta> pacer_burst_interval;
    bool always_negotiate_data_channels;
    int max_sctp_streams;
    bool enable_sctp_snap;
  };
  static_assert(sizeof(stuff_being_tested_for_equality) == sizeof(*this),
                "Did you add something to RTCConfiguration and forget to "
                "update operator==?");
  return type == o.type && servers == o.servers &&
         bundle_policy == o.bundle_policy &&
         rtcp_mux_policy == o.rtcp_mux_policy &&
         tcp_candidate_policy == o.tcp_candidate_policy &&
         candidate_network_policy == o.candidate_network_policy &&
         audio_jitter_buffer_max_packets == o.audio_jitter_buffer_max_packets &&
         audio_jitter_buffer_fast_accelerate ==
             o.audio_jitter_buffer_fast_accelerate &&
         audio_jitter_buffer_min_delay_ms ==
             o.audio_jitter_buffer_min_delay_ms &&
         ice_connection_receiving_timeout ==
             o.ice_connection_receiving_timeout &&
         ice_backup_candidate_pair_ping_interval ==
             o.ice_backup_candidate_pair_ping_interval &&
         continual_gathering_policy == o.continual_gathering_policy &&
         certificates == o.certificates &&
         prioritize_most_likely_ice_candidate_pairs ==
             o.prioritize_most_likely_ice_candidate_pairs &&
         media_config == o.media_config &&
         disable_ipv6_on_wifi == o.disable_ipv6_on_wifi &&
         max_ipv6_networks == o.max_ipv6_networks &&
         disable_link_local_networks == o.disable_link_local_networks &&
         screencast_min_bitrate == o.screencast_min_bitrate &&
         ice_candidate_pool_size == o.ice_candidate_pool_size &&
         prune_turn_ports == o.prune_turn_ports &&
         turn_port_prune_policy == o.turn_port_prune_policy &&
         presume_writable_when_fully_relayed ==
             o.presume_writable_when_fully_relayed &&
         enable_ice_renomination == o.enable_ice_renomination &&
         redetermine_role_on_ice_restart == o.redetermine_role_on_ice_restart &&
         surface_ice_candidates_on_ice_transport_type_changed ==
             o.surface_ice_candidates_on_ice_transport_type_changed &&
         ice_check_interval_strong_connectivity ==
             o.ice_check_interval_strong_connectivity &&
         ice_check_interval_weak_connectivity ==
             o.ice_check_interval_weak_connectivity &&
         ice_check_min_interval == o.ice_check_min_interval &&
         ice_unwritable_timeout == o.ice_unwritable_timeout &&
         ice_unwritable_min_checks == o.ice_unwritable_min_checks &&
         ice_inactive_timeout == o.ice_inactive_timeout &&
         stun_candidate_keepalive_interval ==
             o.stun_candidate_keepalive_interval &&
         turn_customizer == o.turn_customizer &&
         sdp_semantics == o.sdp_semantics &&
         network_preference == o.network_preference &&
         crypto_options == o.crypto_options &&
         offer_extmap_allow_mixed == o.offer_extmap_allow_mixed &&
         turn_logging_id == o.turn_logging_id &&
         enable_implicit_rollback == o.enable_implicit_rollback &&
         report_usage_pattern_delay_ms == o.report_usage_pattern_delay_ms &&
         stable_writable_connection_ping_interval_ms ==
             o.stable_writable_connection_ping_interval_ms &&
         vpn_preference == o.vpn_preference && vpn_list == o.vpn_list &&
         port_allocator_config.min_port == o.port_allocator_config.min_port &&
         port_allocator_config.max_port == o.port_allocator_config.max_port &&
         port_allocator_config.flags == o.port_allocator_config.flags &&
         pacer_burst_interval == o.pacer_burst_interval &&
         always_negotiate_data_channels == o.always_negotiate_data_channels &&
         max_sctp_streams == o.max_sctp_streams;
}

bool PeerConnectionInterface::RTCConfiguration::operator!=(
    const PeerConnectionInterface::RTCConfiguration& o) const {
  return !(*this == o);
}

}  // namespace webrtc
