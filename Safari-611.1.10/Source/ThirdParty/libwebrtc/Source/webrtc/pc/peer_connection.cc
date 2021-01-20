/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/peer_connection.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/match.h"
#include "api/jsep_ice_candidate.h"
#include "api/jsep_session_description.h"
#include "api/media_stream_proxy.h"
#include "api/media_stream_track_proxy.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log_output_file.h"
#include "api/rtp_parameters.h"
#include "api/uma_metrics.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "call/call.h"
#include "logging/rtc_event_log/ice_logger.h"
#include "media/base/rid_description.h"
#include "media/sctp/sctp_transport.h"
#include "pc/audio_rtp_receiver.h"
#include "pc/audio_track.h"
#include "pc/channel.h"
#include "pc/channel_manager.h"
#include "pc/dtmf_sender.h"
#include "pc/media_stream.h"
#include "pc/media_stream_observer.h"
#include "pc/remote_audio_source.h"
#include "pc/rtp_media_utils.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_sender.h"
#include "pc/sctp_transport.h"
#include "pc/sctp_utils.h"
#include "pc/sdp_offer_answer.h"
#include "pc/sdp_utils.h"
#include "pc/stream_collection.h"
#include "pc/video_rtp_receiver.h"
#include "pc/video_track.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"

using cricket::ContentInfo;
using cricket::ContentInfos;
using cricket::MediaContentDescription;
using cricket::MediaProtocolType;
using cricket::RidDescription;
using cricket::RidDirection;
using cricket::SessionDescription;
using cricket::SimulcastDescription;
using cricket::SimulcastLayer;
using cricket::SimulcastLayerList;
using cricket::StreamParams;
using cricket::TransportInfo;

using cricket::LOCAL_PORT_TYPE;
using cricket::PRFLX_PORT_TYPE;
using cricket::RELAY_PORT_TYPE;
using cricket::STUN_PORT_TYPE;

namespace webrtc {

// Error messages
const char kSessionError[] = "Session error code: ";
const char kSessionErrorDesc[] = "Session error description: ";

namespace {

// UMA metric names.
const char kSimulcastNumberOfEncodings[] =
    "WebRTC.PeerConnection.Simulcast.NumberOfSendEncodings";

static const char kDefaultStreamId[] = "default";
static const char kDefaultAudioSenderId[] = "defaulta0";
static const char kDefaultVideoSenderId[] = "defaultv0";

// The length of RTCP CNAMEs.
static const int kRtcpCnameLength = 16;

enum {
  MSG_SET_SESSIONDESCRIPTION_SUCCESS = 0,
  MSG_SET_SESSIONDESCRIPTION_FAILED,
  MSG_CREATE_SESSIONDESCRIPTION_FAILED,
  MSG_GETSTATS,
  MSG_REPORT_USAGE_PATTERN,
};

static const int REPORT_USAGE_PATTERN_DELAY_MS = 60000;

struct SetSessionDescriptionMsg : public rtc::MessageData {
  explicit SetSessionDescriptionMsg(
      webrtc::SetSessionDescriptionObserver* observer)
      : observer(observer) {}

  rtc::scoped_refptr<webrtc::SetSessionDescriptionObserver> observer;
  RTCError error;
};

struct CreateSessionDescriptionMsg : public rtc::MessageData {
  explicit CreateSessionDescriptionMsg(
      webrtc::CreateSessionDescriptionObserver* observer)
      : observer(observer) {}

  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserver> observer;
  RTCError error;
};

struct GetStatsMsg : public rtc::MessageData {
  GetStatsMsg(webrtc::StatsObserver* observer,
              webrtc::MediaStreamTrackInterface* track)
      : observer(observer), track(track) {}
  rtc::scoped_refptr<webrtc::StatsObserver> observer;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track;
};

// Check if we can send |new_stream| on a PeerConnection.
bool CanAddLocalMediaStream(webrtc::StreamCollectionInterface* current_streams,
                            webrtc::MediaStreamInterface* new_stream) {
  if (!new_stream || !current_streams) {
    return false;
  }
  if (current_streams->find(new_stream->id()) != nullptr) {
    RTC_LOG(LS_ERROR) << "MediaStream with ID " << new_stream->id()
                      << " is already added.";
    return false;
  }
  return true;
}

// Add options to |session_options| from |rtp_data_channels|.
void AddRtpDataChannelOptions(
    const std::map<std::string, rtc::scoped_refptr<RtpDataChannel>>&
        rtp_data_channels,
    cricket::MediaDescriptionOptions* data_media_description_options) {
  if (!data_media_description_options) {
    return;
  }
  // Check for data channels.
  for (const auto& kv : rtp_data_channels) {
    const RtpDataChannel* channel = kv.second;
    if (channel->state() == RtpDataChannel::kConnecting ||
        channel->state() == RtpDataChannel::kOpen) {
      // Legacy RTP data channels are signaled with the track/stream ID set to
      // the data channel's label.
      data_media_description_options->AddRtpDataChannel(channel->label(),
                                                        channel->label());
    }
  }
}

uint32_t ConvertIceTransportTypeToCandidateFilter(
    PeerConnectionInterface::IceTransportsType type) {
  switch (type) {
    case PeerConnectionInterface::kNone:
      return cricket::CF_NONE;
    case PeerConnectionInterface::kRelay:
      return cricket::CF_RELAY;
    case PeerConnectionInterface::kNoHost:
      return (cricket::CF_ALL & ~cricket::CF_HOST);
    case PeerConnectionInterface::kAll:
      return cricket::CF_ALL;
    default:
      RTC_NOTREACHED();
  }
  return cricket::CF_NONE;
}

IceCandidatePairType GetIceCandidatePairCounter(
    const cricket::Candidate& local,
    const cricket::Candidate& remote) {
  const auto& l = local.type();
  const auto& r = remote.type();
  const auto& host = LOCAL_PORT_TYPE;
  const auto& srflx = STUN_PORT_TYPE;
  const auto& relay = RELAY_PORT_TYPE;
  const auto& prflx = PRFLX_PORT_TYPE;
  if (l == host && r == host) {
    bool local_hostname =
        !local.address().hostname().empty() && local.address().IsUnresolvedIP();
    bool remote_hostname = !remote.address().hostname().empty() &&
                           remote.address().IsUnresolvedIP();
    bool local_private = IPIsPrivate(local.address().ipaddr());
    bool remote_private = IPIsPrivate(remote.address().ipaddr());
    if (local_hostname) {
      if (remote_hostname) {
        return kIceCandidatePairHostNameHostName;
      } else if (remote_private) {
        return kIceCandidatePairHostNameHostPrivate;
      } else {
        return kIceCandidatePairHostNameHostPublic;
      }
    } else if (local_private) {
      if (remote_hostname) {
        return kIceCandidatePairHostPrivateHostName;
      } else if (remote_private) {
        return kIceCandidatePairHostPrivateHostPrivate;
      } else {
        return kIceCandidatePairHostPrivateHostPublic;
      }
    } else {
      if (remote_hostname) {
        return kIceCandidatePairHostPublicHostName;
      } else if (remote_private) {
        return kIceCandidatePairHostPublicHostPrivate;
      } else {
        return kIceCandidatePairHostPublicHostPublic;
      }
    }
  }
  if (l == host && r == srflx)
    return kIceCandidatePairHostSrflx;
  if (l == host && r == relay)
    return kIceCandidatePairHostRelay;
  if (l == host && r == prflx)
    return kIceCandidatePairHostPrflx;
  if (l == srflx && r == host)
    return kIceCandidatePairSrflxHost;
  if (l == srflx && r == srflx)
    return kIceCandidatePairSrflxSrflx;
  if (l == srflx && r == relay)
    return kIceCandidatePairSrflxRelay;
  if (l == srflx && r == prflx)
    return kIceCandidatePairSrflxPrflx;
  if (l == relay && r == host)
    return kIceCandidatePairRelayHost;
  if (l == relay && r == srflx)
    return kIceCandidatePairRelaySrflx;
  if (l == relay && r == relay)
    return kIceCandidatePairRelayRelay;
  if (l == relay && r == prflx)
    return kIceCandidatePairRelayPrflx;
  if (l == prflx && r == host)
    return kIceCandidatePairPrflxHost;
  if (l == prflx && r == srflx)
    return kIceCandidatePairPrflxSrflx;
  if (l == prflx && r == relay)
    return kIceCandidatePairPrflxRelay;
  return kIceCandidatePairMax;
}


absl::optional<int> RTCConfigurationToIceConfigOptionalInt(
    int rtc_configuration_parameter) {
  if (rtc_configuration_parameter ==
      webrtc::PeerConnectionInterface::RTCConfiguration::kUndefined) {
    return absl::nullopt;
  }
  return rtc_configuration_parameter;
}

// Check if the changes of IceTransportsType motives an ice restart.
bool NeedIceRestart(bool surface_ice_candidates_on_ice_transport_type_changed,
                    PeerConnectionInterface::IceTransportsType current,
                    PeerConnectionInterface::IceTransportsType modified) {
  if (current == modified) {
    return false;
  }

  if (!surface_ice_candidates_on_ice_transport_type_changed) {
    return true;
  }

  auto current_filter = ConvertIceTransportTypeToCandidateFilter(current);
  auto modified_filter = ConvertIceTransportTypeToCandidateFilter(modified);

  // If surface_ice_candidates_on_ice_transport_type_changed is true and we
  // extend the filter, then no ice restart is needed.
  return (current_filter & modified_filter) != current_filter;
}

}  // namespace

bool PeerConnectionInterface::RTCConfiguration::operator==(
    const PeerConnectionInterface::RTCConfiguration& o) const {
  // This static_assert prevents us from accidentally breaking operator==.
  // Note: Order matters! Fields must be ordered the same as RTCConfiguration.
  struct stuff_being_tested_for_equality {
    IceServers servers;
    IceTransportsType type;
    BundlePolicy bundle_policy;
    RtcpMuxPolicy rtcp_mux_policy;
    std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates;
    int ice_candidate_pool_size;
    bool disable_ipv6;
    bool disable_ipv6_on_wifi;
    int max_ipv6_networks;
    bool disable_link_local_networks;
    bool enable_rtp_data_channel;
    absl::optional<int> screencast_min_bitrate;
    absl::optional<bool> combined_audio_video_bwe;
    absl::optional<bool> enable_dtls_srtp;
    TcpCandidatePolicy tcp_candidate_policy;
    CandidateNetworkPolicy candidate_network_policy;
    int audio_jitter_buffer_max_packets;
    bool audio_jitter_buffer_fast_accelerate;
    int audio_jitter_buffer_min_delay_ms;
    bool audio_jitter_buffer_enable_rtx_handling;
    int ice_connection_receiving_timeout;
    int ice_backup_candidate_pair_ping_interval;
    ContinualGatheringPolicy continual_gathering_policy;
    bool prioritize_most_likely_ice_candidate_pairs;
    struct cricket::MediaConfig media_config;
    bool prune_turn_ports;
    PortPrunePolicy turn_port_prune_policy;
    bool presume_writable_when_fully_relayed;
    bool enable_ice_renomination;
    bool redetermine_role_on_ice_restart;
    bool surface_ice_candidates_on_ice_transport_type_changed;
    absl::optional<int> ice_check_interval_strong_connectivity;
    absl::optional<int> ice_check_interval_weak_connectivity;
    absl::optional<int> ice_check_min_interval;
    absl::optional<int> ice_unwritable_timeout;
    absl::optional<int> ice_unwritable_min_checks;
    absl::optional<int> ice_inactive_timeout;
    absl::optional<int> stun_candidate_keepalive_interval;
    webrtc::TurnCustomizer* turn_customizer;
    SdpSemantics sdp_semantics;
    absl::optional<rtc::AdapterType> network_preference;
    bool active_reset_srtp_params;
    absl::optional<CryptoOptions> crypto_options;
    bool offer_extmap_allow_mixed;
    std::string turn_logging_id;
    bool enable_implicit_rollback;
    absl::optional<bool> allow_codec_switching;
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
         audio_jitter_buffer_enable_rtx_handling ==
             o.audio_jitter_buffer_enable_rtx_handling &&
         ice_connection_receiving_timeout ==
             o.ice_connection_receiving_timeout &&
         ice_backup_candidate_pair_ping_interval ==
             o.ice_backup_candidate_pair_ping_interval &&
         continual_gathering_policy == o.continual_gathering_policy &&
         certificates == o.certificates &&
         prioritize_most_likely_ice_candidate_pairs ==
             o.prioritize_most_likely_ice_candidate_pairs &&
         media_config == o.media_config && disable_ipv6 == o.disable_ipv6 &&
         disable_ipv6_on_wifi == o.disable_ipv6_on_wifi &&
         max_ipv6_networks == o.max_ipv6_networks &&
         disable_link_local_networks == o.disable_link_local_networks &&
         enable_rtp_data_channel == o.enable_rtp_data_channel &&
         screencast_min_bitrate == o.screencast_min_bitrate &&
         combined_audio_video_bwe == o.combined_audio_video_bwe &&
         enable_dtls_srtp == o.enable_dtls_srtp &&
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
         active_reset_srtp_params == o.active_reset_srtp_params &&
         crypto_options == o.crypto_options &&
         offer_extmap_allow_mixed == o.offer_extmap_allow_mixed &&
         turn_logging_id == o.turn_logging_id &&
         enable_implicit_rollback == o.enable_implicit_rollback &&
         allow_codec_switching == o.allow_codec_switching;
}

bool PeerConnectionInterface::RTCConfiguration::operator!=(
    const PeerConnectionInterface::RTCConfiguration& o) const {
  return !(*this == o);
}

// Generate a RTCP CNAME when a PeerConnection is created.
std::string GenerateRtcpCname() {
  std::string cname;
  if (!rtc::CreateRandomString(kRtcpCnameLength, &cname)) {
    RTC_LOG(LS_ERROR) << "Failed to generate CNAME.";
    RTC_NOTREACHED();
  }
  return cname;
}

PeerConnection::PeerConnection(PeerConnectionFactory* factory,
                               std::unique_ptr<RtcEventLog> event_log,
                               std::unique_ptr<Call> call)
    : factory_(factory),
      event_log_(std::move(event_log)),
      event_log_ptr_(event_log_.get()),
      rtcp_cname_(GenerateRtcpCname()),
      local_streams_(StreamCollection::Create()),
      remote_streams_(StreamCollection::Create()),
      call_(std::move(call)),
      call_ptr_(call_.get()),
      sdp_handler_(this),
      data_channel_controller_(this) {}

PeerConnection::~PeerConnection() {
  TRACE_EVENT0("webrtc", "PeerConnection::~PeerConnection");
  RTC_DCHECK_RUN_ON(signaling_thread());

  sdp_handler_.PrepareForShutdown();

  // Need to stop transceivers before destroying the stats collector because
  // AudioRtpSender has a reference to the StatsCollector it will update when
  // stopping.
  for (const auto& transceiver : transceivers_.List()) {
    transceiver->StopInternal();
  }

  stats_.reset(nullptr);
  if (stats_collector_) {
    stats_collector_->WaitForPendingRequest();
    stats_collector_ = nullptr;
  }

  // Don't destroy BaseChannels until after stats has been cleaned up so that
  // the last stats request can still read from the channels.
  DestroyAllChannels();

  RTC_LOG(LS_INFO) << "Session: " << session_id() << " is destroyed.";

  sdp_handler_.ResetSessionDescFactory();
  transport_controller_.reset();

  // port_allocator_ lives on the network thread and should be destroyed there.
  network_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
    RTC_DCHECK_RUN_ON(network_thread());
    port_allocator_.reset();
  });
  // call_ and event_log_ must be destroyed on the worker thread.
  worker_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
    RTC_DCHECK_RUN_ON(worker_thread());
    call_safety_.reset();
    call_.reset();
    // The event log must outlive call (and any other object that uses it).
    event_log_.reset();
  });

  // Process all pending notifications in the message queue. If we don't do
  // this, requests will linger and not know they succeeded or failed.
  rtc::MessageList list;
  signaling_thread()->Clear(this, rtc::MQID_ANY, &list);
  for (auto& msg : list) {
    if (msg.message_id == MSG_CREATE_SESSIONDESCRIPTION_FAILED) {
      // Processing CreateOffer() and CreateAnswer() messages ensures their
      // observers are invoked even if the PeerConnection is destroyed early.
      OnMessage(&msg);
    } else {
      // TODO(hbos): Consider processing all pending messages. This would mean
      // that SetLocalDescription() and SetRemoteDescription() observers are
      // informed of successes and failures; this is currently NOT the case.
      delete msg.pdata;
    }
  }
}

void PeerConnection::DestroyAllChannels() {
  // Destroy video channels first since they may have a pointer to a voice
  // channel.
  for (const auto& transceiver : transceivers_.List()) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      DestroyTransceiverChannel(transceiver);
    }
  }
  for (const auto& transceiver : transceivers_.List()) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      DestroyTransceiverChannel(transceiver);
    }
  }
  DestroyDataChannelTransport();
}

bool PeerConnection::Initialize(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    PeerConnectionDependencies dependencies) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "PeerConnection::Initialize");

  RTCError config_error = ValidateConfiguration(configuration);
  if (!config_error.ok()) {
    RTC_LOG(LS_ERROR) << "Invalid configuration: " << config_error.message();
    return false;
  }

  if (!dependencies.allocator) {
    RTC_LOG(LS_ERROR)
        << "PeerConnection initialized without a PortAllocator? "
           "This shouldn't happen if using PeerConnectionFactory.";
    return false;
  }

  if (!dependencies.observer) {
    // TODO(deadbeef): Why do we do this?
    RTC_LOG(LS_ERROR) << "PeerConnection initialized without a "
                         "PeerConnectionObserver";
    return false;
  }

  observer_ = dependencies.observer;
  async_resolver_factory_ = std::move(dependencies.async_resolver_factory);
  port_allocator_ = std::move(dependencies.allocator);
  packet_socket_factory_ = std::move(dependencies.packet_socket_factory);
  ice_transport_factory_ = std::move(dependencies.ice_transport_factory);
  tls_cert_verifier_ = std::move(dependencies.tls_cert_verifier);

  cricket::ServerAddresses stun_servers;
  std::vector<cricket::RelayServerConfig> turn_servers;

  RTCErrorType parse_error =
      ParseIceServers(configuration.servers, &stun_servers, &turn_servers);
  if (parse_error != RTCErrorType::NONE) {
    return false;
  }

  // Add the turn logging id to all turn servers
  for (cricket::RelayServerConfig& turn_server : turn_servers) {
    turn_server.turn_logging_id = configuration.turn_logging_id;
  }

  // The port allocator lives on the network thread and should be initialized
  // there.
  const auto pa_result =
      network_thread()->Invoke<InitializePortAllocatorResult>(
          RTC_FROM_HERE,
          rtc::Bind(&PeerConnection::InitializePortAllocator_n, this,
                    stun_servers, turn_servers, configuration));

  // If initialization was successful, note if STUN or TURN servers
  // were supplied.
  if (!stun_servers.empty()) {
    NoteUsageEvent(UsageEvent::STUN_SERVER_ADDED);
  }
  if (!turn_servers.empty()) {
    NoteUsageEvent(UsageEvent::TURN_SERVER_ADDED);
  }

  // Send information about IPv4/IPv6 status.
  PeerConnectionAddressFamilyCounter address_family;
  if (pa_result.enable_ipv6) {
    address_family = kPeerConnection_IPv6;
  } else {
    address_family = kPeerConnection_IPv4;
  }
  RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.IPMetrics", address_family,
                            kPeerConnectionAddressFamilyCounter_Max);

  const PeerConnectionFactoryInterface::Options& options = factory_->options();

  // RFC 3264: The numeric value of the session id and version in the
  // o line MUST be representable with a "64 bit signed integer".
  // Due to this constraint session id |session_id_| is max limited to
  // LLONG_MAX.
  session_id_ = rtc::ToString(rtc::CreateRandomId64() & LLONG_MAX);
  JsepTransportController::Config config;
  config.redetermine_role_on_ice_restart =
      configuration.redetermine_role_on_ice_restart;
  config.ssl_max_version = factory_->options().ssl_max_version;
  config.disable_encryption = options.disable_encryption;
  config.bundle_policy = configuration.bundle_policy;
  config.rtcp_mux_policy = configuration.rtcp_mux_policy;
  // TODO(bugs.webrtc.org/9891) - Remove options.crypto_options then remove this
  // stub.
  config.crypto_options = configuration.crypto_options.has_value()
                              ? *configuration.crypto_options
                              : options.crypto_options;
  config.transport_observer = this;
  config.rtcp_handler = InitializeRtcpCallback();
  config.event_log = event_log_ptr_;
#if defined(ENABLE_EXTERNAL_AUTH)
  config.enable_external_auth = true;
#endif
  config.active_reset_srtp_params = configuration.active_reset_srtp_params;

  // Obtain a certificate from RTCConfiguration if any were provided (optional).
  rtc::scoped_refptr<rtc::RTCCertificate> certificate;
  if (!configuration.certificates.empty()) {
    // TODO(hbos,torbjorng): Decide on certificate-selection strategy instead of
    // just picking the first one. The decision should be made based on the DTLS
    // handshake. The DTLS negotiations need to know about all certificates.
    certificate = configuration.certificates[0];
  }

  if (options.disable_encryption) {
    dtls_enabled_ = false;
  } else {
    // Enable DTLS by default if we have an identity store or a certificate.
    dtls_enabled_ = (dependencies.cert_generator || certificate);
    // |configuration| can override the default |dtls_enabled_| value.
    if (configuration.enable_dtls_srtp) {
      dtls_enabled_ = *(configuration.enable_dtls_srtp);
    }
  }

  if (configuration.enable_rtp_data_channel) {
    // Enable creation of RTP data channels if the kEnableRtpDataChannels is
    // set. It takes precendence over the disable_sctp_data_channels
    // PeerConnectionFactoryInterface::Options.
    data_channel_controller_.set_data_channel_type(cricket::DCT_RTP);
  } else {
    // DTLS has to be enabled to use SCTP.
    if (!options.disable_sctp_data_channels && dtls_enabled_) {
      data_channel_controller_.set_data_channel_type(cricket::DCT_SCTP);
      config.sctp_factory = factory_->sctp_transport_factory();
    }
  }

  config.ice_transport_factory = ice_transport_factory_.get();

  transport_controller_.reset(new JsepTransportController(
      signaling_thread(), network_thread(), port_allocator_.get(),
      async_resolver_factory_.get(), config));
  transport_controller_->SignalStandardizedIceConnectionState.connect(
      this, &PeerConnection::SetStandardizedIceConnectionState);
  transport_controller_->SignalConnectionState.connect(
      this, &PeerConnection::SetConnectionState);
  transport_controller_->SignalIceGatheringState.connect(
      this, &PeerConnection::OnTransportControllerGatheringState);
  transport_controller_->SignalIceCandidatesGathered.connect(
      this, &PeerConnection::OnTransportControllerCandidatesGathered);
  transport_controller_->SignalIceCandidateError.connect(
      this, &PeerConnection::OnTransportControllerCandidateError);
  transport_controller_->SignalIceCandidatesRemoved.connect(
      this, &PeerConnection::OnTransportControllerCandidatesRemoved);
  transport_controller_->SignalDtlsHandshakeError.connect(
      this, &PeerConnection::OnTransportControllerDtlsHandshakeError);
  transport_controller_->SignalIceCandidatePairChanged.connect(
      this, &PeerConnection::OnTransportControllerCandidateChanged);

  transport_controller_->SignalIceConnectionState.AddReceiver(
      [this](cricket::IceConnectionState s) {
        RTC_DCHECK_RUN_ON(signaling_thread());
        OnTransportControllerConnectionState(s);
      });

  stats_.reset(new StatsCollector(this));
  stats_collector_ = RTCStatsCollector::Create(this);

  configuration_ = configuration;

  transport_controller_->SetIceConfig(ParseIceConfig(configuration));

  video_options_.screencast_min_bitrate_kbps =
      configuration.screencast_min_bitrate;
  audio_options_.combined_audio_video_bwe =
      configuration.combined_audio_video_bwe;

  audio_options_.audio_jitter_buffer_max_packets =
      configuration.audio_jitter_buffer_max_packets;

  audio_options_.audio_jitter_buffer_fast_accelerate =
      configuration.audio_jitter_buffer_fast_accelerate;

  audio_options_.audio_jitter_buffer_min_delay_ms =
      configuration.audio_jitter_buffer_min_delay_ms;

  audio_options_.audio_jitter_buffer_enable_rtx_handling =
      configuration.audio_jitter_buffer_enable_rtx_handling;

  // Whether the certificate generator/certificate is null or not determines
  // what PeerConnectionDescriptionFactory will do, so make sure that we give it
  // the right instructions by clearing the variables if needed.
  if (!dtls_enabled_) {
    dependencies.cert_generator.reset();
    certificate = nullptr;
  } else if (certificate) {
    // Favor generated certificate over the certificate generator.
    dependencies.cert_generator.reset();
  }

  auto webrtc_session_desc_factory =
      std::make_unique<WebRtcSessionDescriptionFactory>(
          signaling_thread(), channel_manager(), this, session_id(),
          std::move(dependencies.cert_generator), certificate,
          &ssrc_generator_);
  webrtc_session_desc_factory->SignalCertificateReady.connect(
      this, &PeerConnection::OnCertificateReady);

  if (options.disable_encryption) {
    webrtc_session_desc_factory->SetSdesPolicy(cricket::SEC_DISABLED);
  }

  webrtc_session_desc_factory->set_enable_encrypted_rtp_header_extensions(
      GetCryptoOptions().srtp.enable_encrypted_rtp_header_extensions);
  webrtc_session_desc_factory->set_is_unified_plan(IsUnifiedPlan());
  sdp_handler_.SetSessionDescFactory(std::move(webrtc_session_desc_factory));

  // Add default audio/video transceivers for Plan B SDP.
  if (!IsUnifiedPlan()) {
    transceivers_.Add(RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
        signaling_thread(), new RtpTransceiver(cricket::MEDIA_TYPE_AUDIO)));
    transceivers_.Add(RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
        signaling_thread(), new RtpTransceiver(cricket::MEDIA_TYPE_VIDEO)));
  }
  int delay_ms =
      return_histogram_very_quickly_ ? 0 : REPORT_USAGE_PATTERN_DELAY_MS;
  signaling_thread()->PostDelayed(RTC_FROM_HERE, delay_ms, this,
                                  MSG_REPORT_USAGE_PATTERN, nullptr);

  if (dependencies.video_bitrate_allocator_factory) {
    video_bitrate_allocator_factory_ =
        std::move(dependencies.video_bitrate_allocator_factory);
  } else {
    video_bitrate_allocator_factory_ =
        CreateBuiltinVideoBitrateAllocatorFactory();
  }
  return true;
}

RTCError PeerConnection::ValidateConfiguration(
    const RTCConfiguration& config) const {
  return cricket::P2PTransportChannel::ValidateIceConfig(
      ParseIceConfig(config));
}

rtc::scoped_refptr<StreamCollectionInterface> PeerConnection::local_streams() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(!IsUnifiedPlan()) << "local_streams is not available with Unified "
                                 "Plan SdpSemantics. Please use GetSenders "
                                 "instead.";
  return local_streams_;
}

rtc::scoped_refptr<StreamCollectionInterface> PeerConnection::remote_streams() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(!IsUnifiedPlan()) << "remote_streams is not available with Unified "
                                 "Plan SdpSemantics. Please use GetReceivers "
                                 "instead.";
  return remote_streams_;
}

bool PeerConnection::AddStream(MediaStreamInterface* local_stream) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(!IsUnifiedPlan()) << "AddStream is not available with Unified Plan "
                                 "SdpSemantics. Please use AddTrack instead.";
  TRACE_EVENT0("webrtc", "PeerConnection::AddStream");
  if (IsClosed()) {
    return false;
  }
  if (!CanAddLocalMediaStream(local_streams_, local_stream)) {
    return false;
  }

  local_streams_->AddStream(local_stream);
  MediaStreamObserver* observer = new MediaStreamObserver(local_stream);
  observer->SignalAudioTrackAdded.connect(this,
                                          &PeerConnection::OnAudioTrackAdded);
  observer->SignalAudioTrackRemoved.connect(
      this, &PeerConnection::OnAudioTrackRemoved);
  observer->SignalVideoTrackAdded.connect(this,
                                          &PeerConnection::OnVideoTrackAdded);
  observer->SignalVideoTrackRemoved.connect(
      this, &PeerConnection::OnVideoTrackRemoved);
  stream_observers_.push_back(std::unique_ptr<MediaStreamObserver>(observer));

  for (const auto& track : local_stream->GetAudioTracks()) {
    AddAudioTrack(track.get(), local_stream);
  }
  for (const auto& track : local_stream->GetVideoTracks()) {
    AddVideoTrack(track.get(), local_stream);
  }

  stats_->AddStream(local_stream);
  sdp_handler_.UpdateNegotiationNeeded();
  return true;
}

void PeerConnection::RemoveStream(MediaStreamInterface* local_stream) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(!IsUnifiedPlan()) << "RemoveStream is not available with Unified "
                                 "Plan SdpSemantics. Please use RemoveTrack "
                                 "instead.";
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveStream");
  if (!IsClosed()) {
    for (const auto& track : local_stream->GetAudioTracks()) {
      RemoveAudioTrack(track.get(), local_stream);
    }
    for (const auto& track : local_stream->GetVideoTracks()) {
      RemoveVideoTrack(track.get(), local_stream);
    }
  }
  local_streams_->RemoveStream(local_stream);
  stream_observers_.erase(
      std::remove_if(
          stream_observers_.begin(), stream_observers_.end(),
          [local_stream](const std::unique_ptr<MediaStreamObserver>& observer) {
            return observer->stream()->id().compare(local_stream->id()) == 0;
          }),
      stream_observers_.end());

  if (IsClosed()) {
    return;
  }
  sdp_handler_.UpdateNegotiationNeeded();
}

RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> PeerConnection::AddTrack(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "PeerConnection::AddTrack");
  if (!track) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, "Track is null.");
  }
  if (!(track->kind() == MediaStreamTrackInterface::kAudioKind ||
        track->kind() == MediaStreamTrackInterface::kVideoKind)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Track has invalid kind: " + track->kind());
  }
  if (IsClosed()) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_STATE,
                         "PeerConnection is closed.");
  }
  if (FindSenderForTrack(track)) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Sender already exists for track " + track->id() + ".");
  }
  auto sender_or_error =
      (IsUnifiedPlan() ? AddTrackUnifiedPlan(track, stream_ids)
                       : AddTrackPlanB(track, stream_ids));
  if (sender_or_error.ok()) {
    sdp_handler_.UpdateNegotiationNeeded();
    stats_->AddTrack(track);
  }
  return sender_or_error;
}

RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>>
PeerConnection::AddTrackPlanB(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
  if (stream_ids.size() > 1u) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_OPERATION,
                         "AddTrack with more than one stream is not "
                         "supported with Plan B semantics.");
  }
  std::vector<std::string> adjusted_stream_ids = stream_ids;
  if (adjusted_stream_ids.empty()) {
    adjusted_stream_ids.push_back(rtc::CreateRandomUuid());
  }
  cricket::MediaType media_type =
      (track->kind() == MediaStreamTrackInterface::kAudioKind
           ? cricket::MEDIA_TYPE_AUDIO
           : cricket::MEDIA_TYPE_VIDEO);
  auto new_sender =
      CreateSender(media_type, track->id(), track, adjusted_stream_ids, {});
  if (track->kind() == MediaStreamTrackInterface::kAudioKind) {
    new_sender->internal()->SetMediaChannel(voice_media_channel());
    GetAudioTransceiver()->internal()->AddSender(new_sender);
    const RtpSenderInfo* sender_info =
        FindSenderInfo(local_audio_sender_infos_,
                       new_sender->internal()->stream_ids()[0], track->id());
    if (sender_info) {
      new_sender->internal()->SetSsrc(sender_info->first_ssrc);
    }
  } else {
    RTC_DCHECK_EQ(MediaStreamTrackInterface::kVideoKind, track->kind());
    new_sender->internal()->SetMediaChannel(video_media_channel());
    GetVideoTransceiver()->internal()->AddSender(new_sender);
    const RtpSenderInfo* sender_info =
        FindSenderInfo(local_video_sender_infos_,
                       new_sender->internal()->stream_ids()[0], track->id());
    if (sender_info) {
      new_sender->internal()->SetSsrc(sender_info->first_ssrc);
    }
  }
  return rtc::scoped_refptr<RtpSenderInterface>(new_sender);
}

RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>>
PeerConnection::AddTrackUnifiedPlan(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
  auto transceiver = FindFirstTransceiverForAddedTrack(track);
  if (transceiver) {
    RTC_LOG(LS_INFO) << "Reusing an existing "
                     << cricket::MediaTypeToString(transceiver->media_type())
                     << " transceiver for AddTrack.";
    if (transceiver->stopping()) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "The existing transceiver is stopping.");
    }

    if (transceiver->direction() == RtpTransceiverDirection::kRecvOnly) {
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kSendRecv);
    } else if (transceiver->direction() == RtpTransceiverDirection::kInactive) {
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kSendOnly);
    }
    transceiver->sender()->SetTrack(track);
    transceiver->internal()->sender_internal()->set_stream_ids(stream_ids);
    transceiver->internal()->set_reused_for_addtrack(true);
  } else {
    cricket::MediaType media_type =
        (track->kind() == MediaStreamTrackInterface::kAudioKind
             ? cricket::MEDIA_TYPE_AUDIO
             : cricket::MEDIA_TYPE_VIDEO);
    RTC_LOG(LS_INFO) << "Adding " << cricket::MediaTypeToString(media_type)
                     << " transceiver in response to a call to AddTrack.";
    std::string sender_id = track->id();
    // Avoid creating a sender with an existing ID by generating a random ID.
    // This can happen if this is the second time AddTrack has created a sender
    // for this track.
    if (FindSenderById(sender_id)) {
      sender_id = rtc::CreateRandomUuid();
    }
    auto sender = CreateSender(media_type, sender_id, track, stream_ids, {});
    auto receiver = CreateReceiver(media_type, rtc::CreateRandomUuid());
    transceiver = CreateAndAddTransceiver(sender, receiver);
    transceiver->internal()->set_created_by_addtrack(true);
    transceiver->internal()->set_direction(RtpTransceiverDirection::kSendRecv);
  }
  return transceiver->sender();
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::FindFirstTransceiverForAddedTrack(
    rtc::scoped_refptr<MediaStreamTrackInterface> track) {
  RTC_DCHECK(track);
  for (auto transceiver : transceivers_.List()) {
    if (!transceiver->sender()->track() &&
        cricket::MediaTypeToString(transceiver->media_type()) ==
            track->kind() &&
        !transceiver->internal()->has_ever_been_used_to_send() &&
        !transceiver->stopped()) {
      return transceiver;
    }
  }
  return nullptr;
}

bool PeerConnection::RemoveTrack(RtpSenderInterface* sender) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveTrack");
  return RemoveTrackNew(sender).ok();
}

RTCError PeerConnection::RemoveTrackNew(
    rtc::scoped_refptr<RtpSenderInterface> sender) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!sender) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, "Sender is null.");
  }
  if (IsClosed()) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_STATE,
                         "PeerConnection is closed.");
  }
  if (IsUnifiedPlan()) {
    auto transceiver = FindTransceiverBySender(sender);
    if (!transceiver || !sender->track()) {
      return RTCError::OK();
    }
    sender->SetTrack(nullptr);
    if (transceiver->direction() == RtpTransceiverDirection::kSendRecv) {
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kRecvOnly);
    } else if (transceiver->direction() == RtpTransceiverDirection::kSendOnly) {
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kInactive);
    }
  } else {
    bool removed;
    if (sender->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      removed = GetAudioTransceiver()->internal()->RemoveSender(sender);
    } else {
      RTC_DCHECK_EQ(cricket::MEDIA_TYPE_VIDEO, sender->media_type());
      removed = GetVideoTransceiver()->internal()->RemoveSender(sender);
    }
    if (!removed) {
      LOG_AND_RETURN_ERROR(
          RTCErrorType::INVALID_PARAMETER,
          "Couldn't find sender " + sender->id() + " to remove.");
    }
  }
  sdp_handler_.UpdateNegotiationNeeded();
  return RTCError::OK();
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::FindTransceiverBySender(
    rtc::scoped_refptr<RtpSenderInterface> sender) {
  return transceivers_.FindBySender(sender);
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnection::AddTransceiver(
    rtc::scoped_refptr<MediaStreamTrackInterface> track) {
  return AddTransceiver(track, RtpTransceiverInit());
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnection::AddTransceiver(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const RtpTransceiverInit& init) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(IsUnifiedPlan())
      << "AddTransceiver is only available with Unified Plan SdpSemantics";
  if (!track) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, "track is null");
  }
  cricket::MediaType media_type;
  if (track->kind() == MediaStreamTrackInterface::kAudioKind) {
    media_type = cricket::MEDIA_TYPE_AUDIO;
  } else if (track->kind() == MediaStreamTrackInterface::kVideoKind) {
    media_type = cricket::MEDIA_TYPE_VIDEO;
  } else {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Track kind is not audio or video");
  }
  return AddTransceiver(media_type, track, init);
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnection::AddTransceiver(cricket::MediaType media_type) {
  return AddTransceiver(media_type, RtpTransceiverInit());
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnection::AddTransceiver(cricket::MediaType media_type,
                               const RtpTransceiverInit& init) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(IsUnifiedPlan())
      << "AddTransceiver is only available with Unified Plan SdpSemantics";
  if (!(media_type == cricket::MEDIA_TYPE_AUDIO ||
        media_type == cricket::MEDIA_TYPE_VIDEO)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "media type is not audio or video");
  }
  return AddTransceiver(media_type, nullptr, init);
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnection::AddTransceiver(
    cricket::MediaType media_type,
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const RtpTransceiverInit& init,
    bool update_negotiation_needed) {
  RTC_DCHECK((media_type == cricket::MEDIA_TYPE_AUDIO ||
              media_type == cricket::MEDIA_TYPE_VIDEO));
  if (track) {
    RTC_DCHECK_EQ(media_type,
                  (track->kind() == MediaStreamTrackInterface::kAudioKind
                       ? cricket::MEDIA_TYPE_AUDIO
                       : cricket::MEDIA_TYPE_VIDEO));
  }

  RTC_HISTOGRAM_COUNTS_LINEAR(kSimulcastNumberOfEncodings,
                              init.send_encodings.size(), 0, 7, 8);

  size_t num_rids = absl::c_count_if(init.send_encodings,
                                     [](const RtpEncodingParameters& encoding) {
                                       return !encoding.rid.empty();
                                     });
  if (num_rids > 0 && num_rids != init.send_encodings.size()) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "RIDs must be provided for either all or none of the send encodings.");
  }

  if (num_rids > 0 && absl::c_any_of(init.send_encodings,
                                     [](const RtpEncodingParameters& encoding) {
                                       return !IsLegalRsidName(encoding.rid);
                                     })) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Invalid RID value provided.");
  }

  if (absl::c_any_of(init.send_encodings,
                     [](const RtpEncodingParameters& encoding) {
                       return encoding.ssrc.has_value();
                     })) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::UNSUPPORTED_PARAMETER,
        "Attempted to set an unimplemented parameter of RtpParameters.");
  }

  RtpParameters parameters;
  parameters.encodings = init.send_encodings;

  // Encodings are dropped from the tail if too many are provided.
  if (parameters.encodings.size() > kMaxSimulcastStreams) {
    parameters.encodings.erase(
        parameters.encodings.begin() + kMaxSimulcastStreams,
        parameters.encodings.end());
  }

  // Single RID should be removed.
  if (parameters.encodings.size() == 1 &&
      !parameters.encodings[0].rid.empty()) {
    RTC_LOG(LS_INFO) << "Removing RID: " << parameters.encodings[0].rid << ".";
    parameters.encodings[0].rid.clear();
  }

  // If RIDs were not provided, they are generated for simulcast scenario.
  if (parameters.encodings.size() > 1 && num_rids == 0) {
    rtc::UniqueStringGenerator rid_generator;
    for (RtpEncodingParameters& encoding : parameters.encodings) {
      encoding.rid = rid_generator();
    }
  }

  if (UnimplementedRtpParameterHasValue(parameters)) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::UNSUPPORTED_PARAMETER,
        "Attempted to set an unimplemented parameter of RtpParameters.");
  }

  auto result = cricket::CheckRtpParametersValues(parameters);
  if (!result.ok()) {
    LOG_AND_RETURN_ERROR(result.type(), result.message());
  }

  RTC_LOG(LS_INFO) << "Adding " << cricket::MediaTypeToString(media_type)
                   << " transceiver in response to a call to AddTransceiver.";
  // Set the sender ID equal to the track ID if the track is specified unless
  // that sender ID is already in use.
  std::string sender_id =
      (track && !FindSenderById(track->id()) ? track->id()
                                             : rtc::CreateRandomUuid());
  auto sender = CreateSender(media_type, sender_id, track, init.stream_ids,
                             parameters.encodings);
  auto receiver = CreateReceiver(media_type, rtc::CreateRandomUuid());
  auto transceiver = CreateAndAddTransceiver(sender, receiver);
  transceiver->internal()->set_direction(init.direction);

  if (update_negotiation_needed) {
    sdp_handler_.UpdateNegotiationNeeded();
  }

  return rtc::scoped_refptr<RtpTransceiverInterface>(transceiver);
}

rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
PeerConnection::CreateSender(
    cricket::MediaType media_type,
    const std::string& id,
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids,
    const std::vector<RtpEncodingParameters>& send_encodings) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender;
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    RTC_DCHECK(!track ||
               (track->kind() == MediaStreamTrackInterface::kAudioKind));
    sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        AudioRtpSender::Create(worker_thread(), id, stats_.get(), this));
    NoteUsageEvent(UsageEvent::AUDIO_ADDED);
  } else {
    RTC_DCHECK_EQ(media_type, cricket::MEDIA_TYPE_VIDEO);
    RTC_DCHECK(!track ||
               (track->kind() == MediaStreamTrackInterface::kVideoKind));
    sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), VideoRtpSender::Create(worker_thread(), id, this));
    NoteUsageEvent(UsageEvent::VIDEO_ADDED);
  }
  bool set_track_succeeded = sender->SetTrack(track);
  RTC_DCHECK(set_track_succeeded);
  sender->internal()->set_stream_ids(stream_ids);
  sender->internal()->set_init_send_encodings(send_encodings);
  return sender;
}

rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
PeerConnection::CreateReceiver(cricket::MediaType media_type,
                               const std::string& receiver_id) {
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
      receiver;
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
        signaling_thread(), new AudioRtpReceiver(worker_thread(), receiver_id,
                                                 std::vector<std::string>({})));
    NoteUsageEvent(UsageEvent::AUDIO_ADDED);
  } else {
    RTC_DCHECK_EQ(media_type, cricket::MEDIA_TYPE_VIDEO);
    receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
        signaling_thread(), new VideoRtpReceiver(worker_thread(), receiver_id,
                                                 std::vector<std::string>({})));
    NoteUsageEvent(UsageEvent::VIDEO_ADDED);
  }
  return receiver;
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::CreateAndAddTransceiver(
    rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
    rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
        receiver) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Ensure that the new sender does not have an ID that is already in use by
  // another sender.
  // Allow receiver IDs to conflict since those come from remote SDP (which
  // could be invalid, but should not cause a crash).
  RTC_DCHECK(!FindSenderById(sender->id()));
  auto transceiver = RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
      signaling_thread(),
      new RtpTransceiver(
          sender, receiver, channel_manager(),
          sender->media_type() == cricket::MEDIA_TYPE_AUDIO
              ? channel_manager()->GetSupportedAudioRtpHeaderExtensions()
              : channel_manager()->GetSupportedVideoRtpHeaderExtensions()));
  transceivers_.Add(transceiver);
  transceiver->internal()->SignalNegotiationNeeded.connect(
      this, &PeerConnection::OnNegotiationNeeded);
  return transceiver;
}

void PeerConnection::OnNegotiationNeeded() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(!IsClosed());
  sdp_handler_.UpdateNegotiationNeeded();
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnection::CreateSender(
    const std::string& kind,
    const std::string& stream_id) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(!IsUnifiedPlan()) << "CreateSender is not available with Unified "
                                 "Plan SdpSemantics. Please use AddTransceiver "
                                 "instead.";
  TRACE_EVENT0("webrtc", "PeerConnection::CreateSender");
  if (IsClosed()) {
    return nullptr;
  }

  // Internally we need to have one stream with Plan B semantics, so we
  // generate a random stream ID if not specified.
  std::vector<std::string> stream_ids;
  if (stream_id.empty()) {
    stream_ids.push_back(rtc::CreateRandomUuid());
    RTC_LOG(LS_INFO)
        << "No stream_id specified for sender. Generated stream ID: "
        << stream_ids[0];
  } else {
    stream_ids.push_back(stream_id);
  }

  // TODO(steveanton): Move construction of the RtpSenders to RtpTransceiver.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender;
  if (kind == MediaStreamTrackInterface::kAudioKind) {
    auto audio_sender = AudioRtpSender::Create(
        worker_thread(), rtc::CreateRandomUuid(), stats_.get(), this);
    audio_sender->SetMediaChannel(voice_media_channel());
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), audio_sender);
    GetAudioTransceiver()->internal()->AddSender(new_sender);
  } else if (kind == MediaStreamTrackInterface::kVideoKind) {
    auto video_sender =
        VideoRtpSender::Create(worker_thread(), rtc::CreateRandomUuid(), this);
    video_sender->SetMediaChannel(video_media_channel());
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), video_sender);
    GetVideoTransceiver()->internal()->AddSender(new_sender);
  } else {
    RTC_LOG(LS_ERROR) << "CreateSender called with invalid kind: " << kind;
    return nullptr;
  }
  new_sender->internal()->set_stream_ids(stream_ids);

  return new_sender;
}

std::vector<rtc::scoped_refptr<RtpSenderInterface>> PeerConnection::GetSenders()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<RtpSenderInterface>> ret;
  for (const auto& sender : GetSendersInternal()) {
    ret.push_back(sender);
  }
  return ret;
}

std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
PeerConnection::GetSendersInternal() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
      all_senders;
  for (const auto& transceiver : transceivers_.List()) {
    if (IsUnifiedPlan() && transceiver->internal()->stopped())
      continue;

    auto senders = transceiver->internal()->senders();
    all_senders.insert(all_senders.end(), senders.begin(), senders.end());
  }
  return all_senders;
}

std::vector<rtc::scoped_refptr<RtpReceiverInterface>>
PeerConnection::GetReceivers() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> ret;
  for (const auto& receiver : GetReceiversInternal()) {
    ret.push_back(receiver);
  }
  return ret;
}

std::vector<
    rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
PeerConnection::GetReceiversInternal() const {
  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
      all_receivers;
  for (const auto& transceiver : transceivers_.List()) {
    if (IsUnifiedPlan() && transceiver->internal()->stopped())
      continue;

    auto receivers = transceiver->internal()->receivers();
    all_receivers.insert(all_receivers.end(), receivers.begin(),
                         receivers.end());
  }
  return all_receivers;
}

std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnection::GetTransceivers() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(IsUnifiedPlan())
      << "GetTransceivers is only supported with Unified Plan SdpSemantics.";
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> all_transceivers;
  for (const auto& transceiver : transceivers_.List()) {
    all_transceivers.push_back(transceiver);
  }
  return all_transceivers;
}

bool PeerConnection::GetStats(StatsObserver* observer,
                              MediaStreamTrackInterface* track,
                              StatsOutputLevel level) {
  TRACE_EVENT0("webrtc", "PeerConnection::GetStats");
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!observer) {
    RTC_LOG(LS_ERROR) << "GetStats - observer is NULL.";
    return false;
  }

  stats_->UpdateStats(level);
  // The StatsCollector is used to tell if a track is valid because it may
  // remember tracks that the PeerConnection previously removed.
  if (track && !stats_->IsValidTrack(track->id())) {
    RTC_LOG(LS_WARNING) << "GetStats is called with an invalid track: "
                        << track->id();
    return false;
  }
  signaling_thread()->Post(RTC_FROM_HERE, this, MSG_GETSTATS,
                           new GetStatsMsg(observer, track));
  return true;
}

void PeerConnection::GetStats(RTCStatsCollectorCallback* callback) {
  TRACE_EVENT0("webrtc", "PeerConnection::GetStats");
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(stats_collector_);
  RTC_DCHECK(callback);
  stats_collector_->GetStatsReport(callback);
}

void PeerConnection::GetStats(
    rtc::scoped_refptr<RtpSenderInterface> selector,
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback) {
  TRACE_EVENT0("webrtc", "PeerConnection::GetStats");
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(callback);
  RTC_DCHECK(stats_collector_);
  rtc::scoped_refptr<RtpSenderInternal> internal_sender;
  if (selector) {
    for (const auto& proxy_transceiver : transceivers_.List()) {
      for (const auto& proxy_sender :
           proxy_transceiver->internal()->senders()) {
        if (proxy_sender == selector) {
          internal_sender = proxy_sender->internal();
          break;
        }
      }
      if (internal_sender)
        break;
    }
  }
  // If there is no |internal_sender| then |selector| is either null or does not
  // belong to the PeerConnection (in Plan B, senders can be removed from the
  // PeerConnection). This means that "all the stats objects representing the
  // selector" is an empty set. Invoking GetStatsReport() with a null selector
  // produces an empty stats report.
  stats_collector_->GetStatsReport(internal_sender, callback);
}

void PeerConnection::GetStats(
    rtc::scoped_refptr<RtpReceiverInterface> selector,
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback) {
  TRACE_EVENT0("webrtc", "PeerConnection::GetStats");
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(callback);
  RTC_DCHECK(stats_collector_);
  rtc::scoped_refptr<RtpReceiverInternal> internal_receiver;
  if (selector) {
    for (const auto& proxy_transceiver : transceivers_.List()) {
      for (const auto& proxy_receiver :
           proxy_transceiver->internal()->receivers()) {
        if (proxy_receiver == selector) {
          internal_receiver = proxy_receiver->internal();
          break;
        }
      }
      if (internal_receiver)
        break;
    }
  }
  // If there is no |internal_receiver| then |selector| is either null or does
  // not belong to the PeerConnection (in Plan B, receivers can be removed from
  // the PeerConnection). This means that "all the stats objects representing
  // the selector" is an empty set. Invoking GetStatsReport() with a null
  // selector produces an empty stats report.
  stats_collector_->GetStatsReport(internal_receiver, callback);
}

PeerConnectionInterface::SignalingState PeerConnection::signaling_state() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.signaling_state();
}

PeerConnectionInterface::IceConnectionState
PeerConnection::ice_connection_state() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return ice_connection_state_;
}

PeerConnectionInterface::IceConnectionState
PeerConnection::standardized_ice_connection_state() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return standardized_ice_connection_state_;
}

PeerConnectionInterface::PeerConnectionState
PeerConnection::peer_connection_state() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return connection_state_;
}

PeerConnectionInterface::IceGatheringState
PeerConnection::ice_gathering_state() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return ice_gathering_state_;
}

absl::optional<bool> PeerConnection::can_trickle_ice_candidates() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  const SessionDescriptionInterface* description = current_remote_description();
  if (!description) {
    description = pending_remote_description();
  }
  if (!description) {
    return absl::nullopt;
  }
  // TODO(bugs.webrtc.org/7443): Change to retrieve from session-level option.
  if (description->description()->transport_infos().size() < 1) {
    return absl::nullopt;
  }
  return description->description()->transport_infos()[0].description.HasOption(
      "trickle");
}

rtc::scoped_refptr<DataChannelInterface> PeerConnection::CreateDataChannel(
    const std::string& label,
    const DataChannelInit* config) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "PeerConnection::CreateDataChannel");

  bool first_datachannel = !data_channel_controller_.HasDataChannels();

  std::unique_ptr<InternalDataChannelInit> internal_config;
  if (config) {
    internal_config.reset(new InternalDataChannelInit(*config));
  }
  rtc::scoped_refptr<DataChannelInterface> channel(
      data_channel_controller_.InternalCreateDataChannelWithProxy(
          label, internal_config.get()));
  if (!channel.get()) {
    return nullptr;
  }

  // Trigger the onRenegotiationNeeded event for every new RTP DataChannel, or
  // the first SCTP DataChannel.
  if (data_channel_type() == cricket::DCT_RTP || first_datachannel) {
    sdp_handler_.UpdateNegotiationNeeded();
  }
  NoteUsageEvent(UsageEvent::DATA_ADDED);
  return channel;
}

void PeerConnection::RestartIce() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sdp_handler_.RestartIce();
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const RTCOfferAnswerOptions& options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sdp_handler_.CreateOffer(observer, options);
}

void PeerConnection::CreateAnswer(CreateSessionDescriptionObserver* observer,
                                  const RTCOfferAnswerOptions& options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sdp_handler_.CreateAnswer(observer, options);
}

RTCError PeerConnection::HandleLegacyOfferOptions(
    const RTCOfferAnswerOptions& options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());

  if (options.offer_to_receive_audio == 0) {
    RemoveRecvDirectionFromReceivingTransceiversOfType(
        cricket::MEDIA_TYPE_AUDIO);
  } else if (options.offer_to_receive_audio == 1) {
    AddUpToOneReceivingTransceiverOfType(cricket::MEDIA_TYPE_AUDIO);
  } else if (options.offer_to_receive_audio > 1) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_PARAMETER,
                         "offer_to_receive_audio > 1 is not supported.");
  }

  if (options.offer_to_receive_video == 0) {
    RemoveRecvDirectionFromReceivingTransceiversOfType(
        cricket::MEDIA_TYPE_VIDEO);
  } else if (options.offer_to_receive_video == 1) {
    AddUpToOneReceivingTransceiverOfType(cricket::MEDIA_TYPE_VIDEO);
  } else if (options.offer_to_receive_video > 1) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_PARAMETER,
                         "offer_to_receive_video > 1 is not supported.");
  }

  return RTCError::OK();
}

void PeerConnection::RemoveRecvDirectionFromReceivingTransceiversOfType(
    cricket::MediaType media_type) {
  for (const auto& transceiver : GetReceivingTransceiversOfType(media_type)) {
    RtpTransceiverDirection new_direction =
        RtpTransceiverDirectionWithRecvSet(transceiver->direction(), false);
    if (new_direction != transceiver->direction()) {
      RTC_LOG(LS_INFO) << "Changing " << cricket::MediaTypeToString(media_type)
                       << " transceiver (MID="
                       << transceiver->mid().value_or("<not set>") << ") from "
                       << RtpTransceiverDirectionToString(
                              transceiver->direction())
                       << " to "
                       << RtpTransceiverDirectionToString(new_direction)
                       << " since CreateOffer specified offer_to_receive=0";
      transceiver->internal()->set_direction(new_direction);
    }
  }
}

void PeerConnection::AddUpToOneReceivingTransceiverOfType(
    cricket::MediaType media_type) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (GetReceivingTransceiversOfType(media_type).empty()) {
    RTC_LOG(LS_INFO)
        << "Adding one recvonly " << cricket::MediaTypeToString(media_type)
        << " transceiver since CreateOffer specified offer_to_receive=1";
    RtpTransceiverInit init;
    init.direction = RtpTransceiverDirection::kRecvOnly;
    AddTransceiver(media_type, nullptr, init,
                   /*update_negotiation_needed=*/false);
  }
}

std::vector<rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
PeerConnection::GetReceivingTransceiversOfType(cricket::MediaType media_type) {
  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
      receiving_transceivers;
  for (const auto& transceiver : transceivers_.List()) {
    if (!transceiver->stopped() && transceiver->media_type() == media_type &&
        RtpTransceiverDirectionHasRecv(transceiver->direction())) {
      receiving_transceivers.push_back(transceiver);
    }
  }
  return receiving_transceivers;
}

void PeerConnection::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc_ptr) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sdp_handler_.SetLocalDescription(observer, desc_ptr);
}

void PeerConnection::SetLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sdp_handler_.SetLocalDescription(std::move(desc), observer);
}

void PeerConnection::SetLocalDescription(
    SetSessionDescriptionObserver* observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sdp_handler_.SetLocalDescription(observer);
}

void PeerConnection::SetLocalDescription(
    rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sdp_handler_.SetLocalDescription(observer);
}

void PeerConnection::RemoveStoppedTransceivers() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // 3.2.10.1: For each transceiver in the connection's set of transceivers
  //           run the following steps:
  if (!IsUnifiedPlan())
    return;
  // Traverse a copy of the transceiver list.
  auto transceiver_list = transceivers_.List();
  for (auto transceiver : transceiver_list) {
    // 3.2.10.1.1: If transceiver is stopped, associated with an m= section
    //             and the associated m= section is rejected in
    //             connection.[[CurrentLocalDescription]] or
    //             connection.[[CurrentRemoteDescription]], remove the
    //             transceiver from the connection's set of transceivers.
    if (!transceiver->stopped()) {
      continue;
    }
    const ContentInfo* local_content =
        sdp_handler_.FindMediaSectionForTransceiver(transceiver,
                                                    local_description());
    const ContentInfo* remote_content =
        sdp_handler_.FindMediaSectionForTransceiver(transceiver,
                                                    remote_description());
    if ((local_content && local_content->rejected) ||
        (remote_content && remote_content->rejected)) {
      RTC_LOG(LS_INFO) << "Dissociating transceiver"
                       << " since the media section is being recycled.";
      transceiver->internal()->set_mid(absl::nullopt);
      transceiver->internal()->set_mline_index(absl::nullopt);
      transceivers_.Remove(transceiver);
      continue;
    }
    if (!local_content && !remote_content) {
      // TODO(bugs.webrtc.org/11973): Consider if this should be removed already
      // See https://github.com/w3c/webrtc-pc/issues/2576
      RTC_LOG(LS_INFO)
          << "Dropping stopped transceiver that was never associated";
      transceivers_.Remove(transceiver);
      continue;
    }
  }
}

void PeerConnection::SetRemoteDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc_ptr) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sdp_handler_.SetRemoteDescription(observer, desc_ptr);
}

void PeerConnection::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sdp_handler_.SetRemoteDescription(std::move(desc), observer);
}

void PeerConnection::ProcessRemovalOfRemoteTrack(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver,
    std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>* remove_list,
    std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams) {
  RTC_DCHECK(transceiver->mid());
  RTC_LOG(LS_INFO) << "Processing the removal of a track for MID="
                   << *transceiver->mid();
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> previous_streams =
      transceiver->internal()->receiver_internal()->streams();
  // This will remove the remote track from the streams.
  transceiver->internal()->receiver_internal()->set_stream_ids({});
  remove_list->push_back(transceiver);
  RemoveRemoteStreamsIfEmpty(previous_streams, removed_streams);
}

void PeerConnection::RemoveRemoteStreamsIfEmpty(
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& remote_streams,
    std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // TODO(https://crbug.com/webrtc/9480): When we use stream IDs instead of
  // streams, see if the stream was removed by checking if this was the last
  // receiver with that stream ID.
  for (const auto& remote_stream : remote_streams) {
    if (remote_stream->GetAudioTracks().empty() &&
        remote_stream->GetVideoTracks().empty()) {
      remote_streams_->RemoveStream(remote_stream);
      removed_streams->push_back(remote_stream);
    }
  }
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetAssociatedTransceiver(const std::string& mid) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());
  return transceivers_.FindByMid(mid);
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetTransceiverByMLineIndex(size_t mline_index) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());
  return transceivers_.FindByMLineIndex(mline_index);
}

PeerConnectionInterface::RTCConfiguration PeerConnection::GetConfiguration() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return configuration_;
}

RTCError PeerConnection::SetConfiguration(
    const RTCConfiguration& configuration) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "PeerConnection::SetConfiguration");
  if (IsClosed()) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_STATE,
                         "SetConfiguration: PeerConnection is closed.");
  }

  // According to JSEP, after setLocalDescription, changing the candidate pool
  // size is not allowed, and changing the set of ICE servers will not result
  // in new candidates being gathered.
  if (local_description() && configuration.ice_candidate_pool_size !=
                                 configuration_.ice_candidate_pool_size) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_MODIFICATION,
                         "Can't change candidate pool size after calling "
                         "SetLocalDescription.");
  }

  if (local_description() &&
      configuration.crypto_options != configuration_.crypto_options) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_MODIFICATION,
                         "Can't change crypto_options after calling "
                         "SetLocalDescription.");
  }

  // The simplest (and most future-compatible) way to tell if the config was
  // modified in an invalid way is to copy each property we do support
  // modifying, then use operator==. There are far more properties we don't
  // support modifying than those we do, and more could be added.
  RTCConfiguration modified_config = configuration_;
  modified_config.servers = configuration.servers;
  modified_config.type = configuration.type;
  modified_config.ice_candidate_pool_size =
      configuration.ice_candidate_pool_size;
  modified_config.prune_turn_ports = configuration.prune_turn_ports;
  modified_config.turn_port_prune_policy = configuration.turn_port_prune_policy;
  modified_config.surface_ice_candidates_on_ice_transport_type_changed =
      configuration.surface_ice_candidates_on_ice_transport_type_changed;
  modified_config.ice_check_min_interval = configuration.ice_check_min_interval;
  modified_config.ice_check_interval_strong_connectivity =
      configuration.ice_check_interval_strong_connectivity;
  modified_config.ice_check_interval_weak_connectivity =
      configuration.ice_check_interval_weak_connectivity;
  modified_config.ice_unwritable_timeout = configuration.ice_unwritable_timeout;
  modified_config.ice_unwritable_min_checks =
      configuration.ice_unwritable_min_checks;
  modified_config.ice_inactive_timeout = configuration.ice_inactive_timeout;
  modified_config.stun_candidate_keepalive_interval =
      configuration.stun_candidate_keepalive_interval;
  modified_config.turn_customizer = configuration.turn_customizer;
  modified_config.network_preference = configuration.network_preference;
  modified_config.active_reset_srtp_params =
      configuration.active_reset_srtp_params;
  modified_config.turn_logging_id = configuration.turn_logging_id;
  modified_config.allow_codec_switching = configuration.allow_codec_switching;
  if (configuration != modified_config) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_MODIFICATION,
                         "Modifying the configuration in an unsupported way.");
  }

  // Validate the modified configuration.
  RTCError validate_error = ValidateConfiguration(modified_config);
  if (!validate_error.ok()) {
    return validate_error;
  }

  // Note that this isn't possible through chromium, since it's an unsigned
  // short in WebIDL.
  if (configuration.ice_candidate_pool_size < 0 ||
      configuration.ice_candidate_pool_size > static_cast<int>(UINT16_MAX)) {
    return RTCError(RTCErrorType::INVALID_RANGE);
  }

  // Parse ICE servers before hopping to network thread.
  cricket::ServerAddresses stun_servers;
  std::vector<cricket::RelayServerConfig> turn_servers;
  RTCErrorType parse_error =
      ParseIceServers(configuration.servers, &stun_servers, &turn_servers);
  if (parse_error != RTCErrorType::NONE) {
    return RTCError(parse_error);
  }
  // Add the turn logging id to all turn servers
  for (cricket::RelayServerConfig& turn_server : turn_servers) {
    turn_server.turn_logging_id = configuration.turn_logging_id;
  }

  // Note if STUN or TURN servers were supplied.
  if (!stun_servers.empty()) {
    NoteUsageEvent(UsageEvent::STUN_SERVER_ADDED);
  }
  if (!turn_servers.empty()) {
    NoteUsageEvent(UsageEvent::TURN_SERVER_ADDED);
  }

  // In theory this shouldn't fail.
  if (!network_thread()->Invoke<bool>(
          RTC_FROM_HERE,
          rtc::Bind(&PeerConnection::ReconfigurePortAllocator_n, this,
                    stun_servers, turn_servers, modified_config.type,
                    modified_config.ice_candidate_pool_size,
                    modified_config.GetTurnPortPrunePolicy(),
                    modified_config.turn_customizer,
                    modified_config.stun_candidate_keepalive_interval,
                    static_cast<bool>(local_description())))) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to apply configuration to PortAllocator.");
  }

  // As described in JSEP, calling setConfiguration with new ICE servers or
  // candidate policy must set a "needs-ice-restart" bit so that the next offer
  // triggers an ICE restart which will pick up the changes.
  if (modified_config.servers != configuration_.servers ||
      NeedIceRestart(
          configuration_.surface_ice_candidates_on_ice_transport_type_changed,
          configuration_.type, modified_config.type) ||
      modified_config.GetTurnPortPrunePolicy() !=
          configuration_.GetTurnPortPrunePolicy()) {
    transport_controller_->SetNeedsIceRestartFlag();
  }

  transport_controller_->SetIceConfig(ParseIceConfig(modified_config));

  if (configuration_.active_reset_srtp_params !=
      modified_config.active_reset_srtp_params) {
    transport_controller_->SetActiveResetSrtpParams(
        modified_config.active_reset_srtp_params);
  }

  if (modified_config.allow_codec_switching.has_value()) {
    std::vector<cricket::VideoMediaChannel*> channels;
    for (const auto& transceiver : transceivers_.List()) {
      if (transceiver->media_type() != cricket::MEDIA_TYPE_VIDEO)
        continue;

      auto* video_channel = static_cast<cricket::VideoChannel*>(
          transceiver->internal()->channel());
      if (video_channel)
        channels.push_back(video_channel->media_channel());
    }

    worker_thread()->Invoke<void>(
        RTC_FROM_HERE,
        [channels = std::move(channels),
         allow_codec_switching = *modified_config.allow_codec_switching]() {
          for (auto* ch : channels)
            ch->SetVideoCodecSwitchingEnabled(allow_codec_switching);
        });
  }

  configuration_ = modified_config;
  return RTCError::OK();
}

bool PeerConnection::AddIceCandidate(
    const IceCandidateInterface* ice_candidate) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.AddIceCandidate(ice_candidate);
}

void PeerConnection::AddIceCandidate(
    std::unique_ptr<IceCandidateInterface> candidate,
    std::function<void(RTCError)> callback) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sdp_handler_.AddIceCandidate(std::move(candidate), callback);
}

bool PeerConnection::RemoveIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveIceCandidates");
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.RemoveIceCandidates(candidates);
}

RTCError PeerConnection::SetBitrate(const BitrateSettings& bitrate) {
  if (!worker_thread()->IsCurrent()) {
    return worker_thread()->Invoke<RTCError>(
        RTC_FROM_HERE, [&]() { return SetBitrate(bitrate); });
  }
  RTC_DCHECK_RUN_ON(worker_thread());

  const bool has_min = bitrate.min_bitrate_bps.has_value();
  const bool has_start = bitrate.start_bitrate_bps.has_value();
  const bool has_max = bitrate.max_bitrate_bps.has_value();
  if (has_min && *bitrate.min_bitrate_bps < 0) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "min_bitrate_bps <= 0");
  }
  if (has_start) {
    if (has_min && *bitrate.start_bitrate_bps < *bitrate.min_bitrate_bps) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "start_bitrate_bps < min_bitrate_bps");
    } else if (*bitrate.start_bitrate_bps < 0) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "curent_bitrate_bps < 0");
    }
  }
  if (has_max) {
    if (has_start && *bitrate.max_bitrate_bps < *bitrate.start_bitrate_bps) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max_bitrate_bps < start_bitrate_bps");
    } else if (has_min && *bitrate.max_bitrate_bps < *bitrate.min_bitrate_bps) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max_bitrate_bps < min_bitrate_bps");
    } else if (*bitrate.max_bitrate_bps < 0) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max_bitrate_bps < 0");
    }
  }

  RTC_DCHECK(call_.get());
  call_->SetClientBitratePreferences(bitrate);

  return RTCError::OK();
}

void PeerConnection::SetAudioPlayout(bool playout) {
  if (!worker_thread()->IsCurrent()) {
    worker_thread()->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&PeerConnection::SetAudioPlayout, this, playout));
    return;
  }
  auto audio_state =
      factory_->channel_manager()->media_engine()->voice().GetAudioState();
  audio_state->SetPlayout(playout);
}

void PeerConnection::SetAudioRecording(bool recording) {
  if (!worker_thread()->IsCurrent()) {
    worker_thread()->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&PeerConnection::SetAudioRecording, this, recording));
    return;
  }
  auto audio_state =
      factory_->channel_manager()->media_engine()->voice().GetAudioState();
  audio_state->SetRecording(recording);
}

std::unique_ptr<rtc::SSLCertificate>
PeerConnection::GetRemoteAudioSSLCertificate() {
  std::unique_ptr<rtc::SSLCertChain> chain = GetRemoteAudioSSLCertChain();
  if (!chain || !chain->GetSize()) {
    return nullptr;
  }
  return chain->Get(0).Clone();
}

std::unique_ptr<rtc::SSLCertChain>
PeerConnection::GetRemoteAudioSSLCertChain() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  auto audio_transceiver = GetFirstAudioTransceiver();
  if (!audio_transceiver || !audio_transceiver->internal()->channel()) {
    return nullptr;
  }
  return transport_controller_->GetRemoteSSLCertChain(
      audio_transceiver->internal()->channel()->transport_name());
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetFirstAudioTransceiver() const {
  for (auto transceiver : transceivers_.List()) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      return transceiver;
    }
  }
  return nullptr;
}

void PeerConnection::AddAdaptationResource(
    rtc::scoped_refptr<Resource> resource) {
  if (!worker_thread()->IsCurrent()) {
    return worker_thread()->Invoke<void>(RTC_FROM_HERE, [this, resource]() {
      return AddAdaptationResource(resource);
    });
  }
  RTC_DCHECK_RUN_ON(worker_thread());
  if (!call_) {
    // The PeerConnection has been closed.
    return;
  }
  call_->AddAdaptationResource(resource);
}

bool PeerConnection::StartRtcEventLog(std::unique_ptr<RtcEventLogOutput> output,
                                      int64_t output_period_ms) {
  return worker_thread()->Invoke<bool>(
      RTC_FROM_HERE,
      [this, output = std::move(output), output_period_ms]() mutable {
        return StartRtcEventLog_w(std::move(output), output_period_ms);
      });
}

bool PeerConnection::StartRtcEventLog(
    std::unique_ptr<RtcEventLogOutput> output) {
  int64_t output_period_ms = webrtc::RtcEventLog::kImmediateOutput;
  if (absl::StartsWith(factory_->trials().Lookup("WebRTC-RtcEventLogNewFormat"),
                       "Enabled")) {
    output_period_ms = 5000;
  }
  return StartRtcEventLog(std::move(output), output_period_ms);
}

void PeerConnection::StopRtcEventLog() {
  worker_thread()->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&PeerConnection::StopRtcEventLog_w, this));
}

rtc::scoped_refptr<DtlsTransportInterface>
PeerConnection::LookupDtlsTransportByMid(const std::string& mid) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return transport_controller_->LookupDtlsTransportByMid(mid);
}

rtc::scoped_refptr<DtlsTransport>
PeerConnection::LookupDtlsTransportByMidInternal(const std::string& mid) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return transport_controller_->LookupDtlsTransportByMid(mid);
}

rtc::scoped_refptr<SctpTransportInterface> PeerConnection::GetSctpTransport()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!sctp_mid_s_) {
    return nullptr;
  }
  return transport_controller_->GetSctpTransport(*sctp_mid_s_);
}

const SessionDescriptionInterface* PeerConnection::local_description() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.local_description();
}

const SessionDescriptionInterface* PeerConnection::remote_description() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.remote_description();
}

const SessionDescriptionInterface* PeerConnection::current_local_description()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.current_local_description();
}

const SessionDescriptionInterface* PeerConnection::current_remote_description()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.current_remote_description();
}

const SessionDescriptionInterface* PeerConnection::pending_local_description()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.pending_local_description();
}

const SessionDescriptionInterface* PeerConnection::pending_remote_description()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.pending_remote_description();
}

void PeerConnection::Close() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "PeerConnection::Close");

  if (IsClosed()) {
    return;
  }
  // Update stats here so that we have the most recent stats for tracks and
  // streams before the channels are closed.
  stats_->UpdateStats(kStatsOutputLevelStandard);

  ice_connection_state_ = PeerConnectionInterface::kIceConnectionClosed;
  Observer()->OnIceConnectionChange(ice_connection_state_);
  standardized_ice_connection_state_ =
      PeerConnectionInterface::IceConnectionState::kIceConnectionClosed;
  connection_state_ = PeerConnectionInterface::PeerConnectionState::kClosed;
  Observer()->OnConnectionChange(connection_state_);

  sdp_handler_.Close();

  NoteUsageEvent(UsageEvent::CLOSE_CALLED);

  for (const auto& transceiver : transceivers_.List()) {
    transceiver->internal()->SetPeerConnectionClosed();
    if (!transceiver->stopped())
      transceiver->StopInternal();
  }

  // Ensure that all asynchronous stats requests are completed before destroying
  // the transport controller below.
  if (stats_collector_) {
    stats_collector_->WaitForPendingRequest();
  }

  // Don't destroy BaseChannels until after stats has been cleaned up so that
  // the last stats request can still read from the channels.
  DestroyAllChannels();

  // The event log is used in the transport controller, which must be outlived
  // by the former. CreateOffer by the peer connection is implemented
  // asynchronously and if the peer connection is closed without resetting the
  // WebRTC session description factory, the session description factory would
  // call the transport controller.
  sdp_handler_.ResetSessionDescFactory();
  transport_controller_.reset();

  network_thread()->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                               port_allocator_.get()));

  worker_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
    RTC_DCHECK_RUN_ON(worker_thread());
    call_safety_.reset();
    call_.reset();
    // The event log must outlive call (and any other object that uses it).
    event_log_.reset();
  });
  ReportUsagePattern();
  // The .h file says that observer can be discarded after close() returns.
  // Make sure this is true.
  observer_ = nullptr;
}

void PeerConnection::OnMessage(rtc::Message* msg) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  switch (msg->message_id) {
    case MSG_SET_SESSIONDESCRIPTION_SUCCESS: {
      SetSessionDescriptionMsg* param =
          static_cast<SetSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnSuccess();
      delete param;
      break;
    }
    case MSG_SET_SESSIONDESCRIPTION_FAILED: {
      SetSessionDescriptionMsg* param =
          static_cast<SetSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnFailure(std::move(param->error));
      delete param;
      break;
    }
    case MSG_CREATE_SESSIONDESCRIPTION_FAILED: {
      CreateSessionDescriptionMsg* param =
          static_cast<CreateSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnFailure(std::move(param->error));
      delete param;
      break;
    }
    case MSG_GETSTATS: {
      GetStatsMsg* param = static_cast<GetStatsMsg*>(msg->pdata);
      StatsReports reports;
      stats_->GetStats(param->track, &reports);
      param->observer->OnComplete(reports);
      delete param;
      break;
    }
    case MSG_REPORT_USAGE_PATTERN: {
      ReportUsagePattern();
      break;
    }
    default:
      RTC_NOTREACHED() << "Not implemented";
      break;
  }
}

cricket::VoiceMediaChannel* PeerConnection::voice_media_channel() const {
  RTC_DCHECK(!IsUnifiedPlan());
  auto* voice_channel = static_cast<cricket::VoiceChannel*>(
      GetAudioTransceiver()->internal()->channel());
  if (voice_channel) {
    return voice_channel->media_channel();
  } else {
    return nullptr;
  }
}

cricket::VideoMediaChannel* PeerConnection::video_media_channel() const {
  RTC_DCHECK(!IsUnifiedPlan());
  auto* video_channel = static_cast<cricket::VideoChannel*>(
      GetVideoTransceiver()->internal()->channel());
  if (video_channel) {
    return video_channel->media_channel();
  } else {
    return nullptr;
  }
}

void PeerConnection::CreateAudioReceiver(
    MediaStreamInterface* stream,
    const RtpSenderInfo& remote_sender_info) {
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams;
  streams.push_back(rtc::scoped_refptr<MediaStreamInterface>(stream));
  // TODO(https://crbug.com/webrtc/9480): When we remove remote_streams(), use
  // the constructor taking stream IDs instead.
  auto* audio_receiver = new AudioRtpReceiver(
      worker_thread(), remote_sender_info.sender_id, streams);
  audio_receiver->SetMediaChannel(voice_media_channel());
  if (remote_sender_info.sender_id == kDefaultAudioSenderId) {
    audio_receiver->SetupUnsignaledMediaChannel();
  } else {
    audio_receiver->SetupMediaChannel(remote_sender_info.first_ssrc);
  }
  auto receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
      signaling_thread(), audio_receiver);
  GetAudioTransceiver()->internal()->AddReceiver(receiver);
  Observer()->OnAddTrack(receiver, streams);
  NoteUsageEvent(UsageEvent::AUDIO_ADDED);
}

void PeerConnection::CreateVideoReceiver(
    MediaStreamInterface* stream,
    const RtpSenderInfo& remote_sender_info) {
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams;
  streams.push_back(rtc::scoped_refptr<MediaStreamInterface>(stream));
  // TODO(https://crbug.com/webrtc/9480): When we remove remote_streams(), use
  // the constructor taking stream IDs instead.
  auto* video_receiver = new VideoRtpReceiver(
      worker_thread(), remote_sender_info.sender_id, streams);
  video_receiver->SetMediaChannel(video_media_channel());
  if (remote_sender_info.sender_id == kDefaultVideoSenderId) {
    video_receiver->SetupUnsignaledMediaChannel();
  } else {
    video_receiver->SetupMediaChannel(remote_sender_info.first_ssrc);
  }
  auto receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
      signaling_thread(), video_receiver);
  GetVideoTransceiver()->internal()->AddReceiver(receiver);
  Observer()->OnAddTrack(receiver, streams);
  NoteUsageEvent(UsageEvent::VIDEO_ADDED);
}

// TODO(deadbeef): Keep RtpReceivers around even if track goes away in remote
// description.
rtc::scoped_refptr<RtpReceiverInterface> PeerConnection::RemoveAndStopReceiver(
    const RtpSenderInfo& remote_sender_info) {
  auto receiver = FindReceiverById(remote_sender_info.sender_id);
  if (!receiver) {
    RTC_LOG(LS_WARNING) << "RtpReceiver for track with id "
                        << remote_sender_info.sender_id << " doesn't exist.";
    return nullptr;
  }
  if (receiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
    GetAudioTransceiver()->internal()->RemoveReceiver(receiver);
  } else {
    GetVideoTransceiver()->internal()->RemoveReceiver(receiver);
  }
  return receiver;
}

void PeerConnection::AddAudioTrack(AudioTrackInterface* track,
                                   MediaStreamInterface* stream) {
  RTC_DCHECK(!IsClosed());
  RTC_DCHECK(track);
  RTC_DCHECK(stream);
  auto sender = FindSenderForTrack(track);
  if (sender) {
    // We already have a sender for this track, so just change the stream_id
    // so that it's correct in the next call to CreateOffer.
    sender->internal()->set_stream_ids({stream->id()});
    return;
  }

  // Normal case; we've never seen this track before.
  auto new_sender = CreateSender(cricket::MEDIA_TYPE_AUDIO, track->id(), track,
                                 {stream->id()}, {});
  new_sender->internal()->SetMediaChannel(voice_media_channel());
  GetAudioTransceiver()->internal()->AddSender(new_sender);
  // If the sender has already been configured in SDP, we call SetSsrc,
  // which will connect the sender to the underlying transport. This can
  // occur if a local session description that contains the ID of the sender
  // is set before AddStream is called. It can also occur if the local
  // session description is not changed and RemoveStream is called, and
  // later AddStream is called again with the same stream.
  const RtpSenderInfo* sender_info =
      FindSenderInfo(local_audio_sender_infos_, stream->id(), track->id());
  if (sender_info) {
    new_sender->internal()->SetSsrc(sender_info->first_ssrc);
  }
}

// TODO(deadbeef): Don't destroy RtpSenders here; they should be kept around
// indefinitely, when we have unified plan SDP.
void PeerConnection::RemoveAudioTrack(AudioTrackInterface* track,
                                      MediaStreamInterface* stream) {
  RTC_DCHECK(!IsClosed());
  auto sender = FindSenderForTrack(track);
  if (!sender) {
    RTC_LOG(LS_WARNING) << "RtpSender for track with id " << track->id()
                        << " doesn't exist.";
    return;
  }
  GetAudioTransceiver()->internal()->RemoveSender(sender);
}

void PeerConnection::AddVideoTrack(VideoTrackInterface* track,
                                   MediaStreamInterface* stream) {
  RTC_DCHECK(!IsClosed());
  RTC_DCHECK(track);
  RTC_DCHECK(stream);
  auto sender = FindSenderForTrack(track);
  if (sender) {
    // We already have a sender for this track, so just change the stream_id
    // so that it's correct in the next call to CreateOffer.
    sender->internal()->set_stream_ids({stream->id()});
    return;
  }

  // Normal case; we've never seen this track before.
  auto new_sender = CreateSender(cricket::MEDIA_TYPE_VIDEO, track->id(), track,
                                 {stream->id()}, {});
  new_sender->internal()->SetMediaChannel(video_media_channel());
  GetVideoTransceiver()->internal()->AddSender(new_sender);
  const RtpSenderInfo* sender_info =
      FindSenderInfo(local_video_sender_infos_, stream->id(), track->id());
  if (sender_info) {
    new_sender->internal()->SetSsrc(sender_info->first_ssrc);
  }
}

void PeerConnection::RemoveVideoTrack(VideoTrackInterface* track,
                                      MediaStreamInterface* stream) {
  RTC_DCHECK(!IsClosed());
  auto sender = FindSenderForTrack(track);
  if (!sender) {
    RTC_LOG(LS_WARNING) << "RtpSender for track with id " << track->id()
                        << " doesn't exist.";
    return;
  }
  GetVideoTransceiver()->internal()->RemoveSender(sender);
}

void PeerConnection::SetIceConnectionState(IceConnectionState new_state) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (ice_connection_state_ == new_state) {
    return;
  }

  // After transitioning to "closed", ignore any additional states from
  // TransportController (such as "disconnected").
  if (IsClosed()) {
    return;
  }

  RTC_LOG(LS_INFO) << "Changing IceConnectionState " << ice_connection_state_
                   << " => " << new_state;
  RTC_DCHECK(ice_connection_state_ !=
             PeerConnectionInterface::kIceConnectionClosed);

  ice_connection_state_ = new_state;
  Observer()->OnIceConnectionChange(ice_connection_state_);
}

void PeerConnection::SetStandardizedIceConnectionState(
    PeerConnectionInterface::IceConnectionState new_state) {
  if (standardized_ice_connection_state_ == new_state) {
    return;
  }

  if (IsClosed()) {
    return;
  }

  RTC_LOG(LS_INFO) << "Changing standardized IceConnectionState "
                   << standardized_ice_connection_state_ << " => " << new_state;

  standardized_ice_connection_state_ = new_state;
  Observer()->OnStandardizedIceConnectionChange(new_state);
}

void PeerConnection::SetConnectionState(
    PeerConnectionInterface::PeerConnectionState new_state) {
  if (connection_state_ == new_state)
    return;
  if (IsClosed())
    return;
  connection_state_ = new_state;
  Observer()->OnConnectionChange(new_state);
}

void PeerConnection::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  if (IsClosed()) {
    return;
  }
  ice_gathering_state_ = new_state;
  Observer()->OnIceGatheringChange(ice_gathering_state_);
}

void PeerConnection::OnIceCandidate(
    std::unique_ptr<IceCandidateInterface> candidate) {
  if (IsClosed()) {
    return;
  }
  ReportIceCandidateCollected(candidate->candidate());
  Observer()->OnIceCandidate(candidate.get());
}

void PeerConnection::OnIceCandidateError(const std::string& address,
                                         int port,
                                         const std::string& url,
                                         int error_code,
                                         const std::string& error_text) {
  if (IsClosed()) {
    return;
  }
  Observer()->OnIceCandidateError(address, port, url, error_code, error_text);
  // Leftover not to break wpt test during migration to the new API.
  Observer()->OnIceCandidateError(address + ":", url, error_code, error_text);
}

void PeerConnection::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  if (IsClosed()) {
    return;
  }
  Observer()->OnIceCandidatesRemoved(candidates);
}

void PeerConnection::OnSelectedCandidatePairChanged(
    const cricket::CandidatePairChangeEvent& event) {
  if (IsClosed()) {
    return;
  }

  if (event.selected_candidate_pair.local_candidate().type() ==
          LOCAL_PORT_TYPE &&
      event.selected_candidate_pair.remote_candidate().type() ==
          LOCAL_PORT_TYPE) {
    NoteUsageEvent(UsageEvent::DIRECT_CONNECTION_SELECTED);
  }

  Observer()->OnIceSelectedCandidatePairChanged(event);
}

void PeerConnection::OnAudioTrackAdded(AudioTrackInterface* track,
                                       MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  AddAudioTrack(track, stream);
  sdp_handler_.UpdateNegotiationNeeded();
}

void PeerConnection::OnAudioTrackRemoved(AudioTrackInterface* track,
                                         MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  RemoveAudioTrack(track, stream);
  sdp_handler_.UpdateNegotiationNeeded();
}

void PeerConnection::OnVideoTrackAdded(VideoTrackInterface* track,
                                       MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  AddVideoTrack(track, stream);
  sdp_handler_.UpdateNegotiationNeeded();
}

void PeerConnection::OnVideoTrackRemoved(VideoTrackInterface* track,
                                         MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  RemoveVideoTrack(track, stream);
  sdp_handler_.UpdateNegotiationNeeded();
}

void PeerConnection::PostSetSessionDescriptionSuccess(
    SetSessionDescriptionObserver* observer) {
  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);
}

void PeerConnection::PostSetSessionDescriptionFailure(
    SetSessionDescriptionObserver* observer,
    RTCError&& error) {
  RTC_DCHECK(!error.ok());
  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  msg->error = std::move(error);
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_SET_SESSIONDESCRIPTION_FAILED, msg);
}

void PeerConnection::PostCreateSessionDescriptionFailure(
    CreateSessionDescriptionObserver* observer,
    RTCError error) {
  RTC_DCHECK(!error.ok());
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->error = std::move(error);
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_CREATE_SESSIONDESCRIPTION_FAILED, msg);
}


void PeerConnection::GenerateMediaDescriptionOptions(
    const SessionDescriptionInterface* session_desc,
    RtpTransceiverDirection audio_direction,
    RtpTransceiverDirection video_direction,
    absl::optional<size_t>* audio_index,
    absl::optional<size_t>* video_index,
    absl::optional<size_t>* data_index,
    cricket::MediaSessionOptions* session_options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (const cricket::ContentInfo& content :
       session_desc->description()->contents()) {
    if (IsAudioContent(&content)) {
      // If we already have an audio m= section, reject this extra one.
      if (*audio_index) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_AUDIO, content.name,
                RtpTransceiverDirection::kInactive, /*stopped=*/true));
      } else {
        bool stopped = (audio_direction == RtpTransceiverDirection::kInactive);
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(cricket::MEDIA_TYPE_AUDIO,
                                             content.name, audio_direction,
                                             stopped));
        *audio_index = session_options->media_description_options.size() - 1;
      }
      session_options->media_description_options.back().header_extensions =
          channel_manager()->GetSupportedAudioRtpHeaderExtensions();
    } else if (IsVideoContent(&content)) {
      // If we already have an video m= section, reject this extra one.
      if (*video_index) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_VIDEO, content.name,
                RtpTransceiverDirection::kInactive, /*stopped=*/true));
      } else {
        bool stopped = (video_direction == RtpTransceiverDirection::kInactive);
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(cricket::MEDIA_TYPE_VIDEO,
                                             content.name, video_direction,
                                             stopped));
        *video_index = session_options->media_description_options.size() - 1;
      }
      session_options->media_description_options.back().header_extensions =
          channel_manager()->GetSupportedVideoRtpHeaderExtensions();
    } else {
      RTC_DCHECK(IsDataContent(&content));
      // If we already have an data m= section, reject this extra one.
      if (*data_index) {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForRejectedData(content.name));
      } else {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForActiveData(content.name));
        *data_index = session_options->media_description_options.size() - 1;
      }
    }
  }
}

cricket::MediaDescriptionOptions
PeerConnection::GetMediaDescriptionOptionsForActiveData(
    const std::string& mid) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Direction for data sections is meaningless, but legacy endpoints might
  // expect sendrecv.
  cricket::MediaDescriptionOptions options(cricket::MEDIA_TYPE_DATA, mid,
                                           RtpTransceiverDirection::kSendRecv,
                                           /*stopped=*/false);
  AddRtpDataChannelOptions(*data_channel_controller_.rtp_data_channels(),
                           &options);
  return options;
}

cricket::MediaDescriptionOptions
PeerConnection::GetMediaDescriptionOptionsForRejectedData(
    const std::string& mid) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  cricket::MediaDescriptionOptions options(cricket::MEDIA_TYPE_DATA, mid,
                                           RtpTransceiverDirection::kInactive,
                                           /*stopped=*/true);
  AddRtpDataChannelOptions(*data_channel_controller_.rtp_data_channels(),
                           &options);
  return options;
}

absl::optional<std::string> PeerConnection::GetDataMid() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  switch (data_channel_type()) {
    case cricket::DCT_RTP:
      if (!data_channel_controller_.rtp_data_channel()) {
        return absl::nullopt;
      }
      return data_channel_controller_.rtp_data_channel()->content_name();
    case cricket::DCT_SCTP:
      return sctp_mid_s_;
    default:
      return absl::nullopt;
  }
}

void PeerConnection::RemoveSenders(cricket::MediaType media_type) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  UpdateLocalSenders(std::vector<cricket::StreamParams>(), media_type);
  UpdateRemoteSendersList(std::vector<cricket::StreamParams>(), false,
                          media_type, nullptr);
}

void PeerConnection::UpdateRemoteSendersList(
    const cricket::StreamParamsVec& streams,
    bool default_sender_needed,
    cricket::MediaType media_type,
    StreamCollection* new_streams) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(!IsUnifiedPlan());

  std::vector<RtpSenderInfo>* current_senders =
      GetRemoteSenderInfos(media_type);

  // Find removed senders. I.e., senders where the sender id or ssrc don't match
  // the new StreamParam.
  for (auto sender_it = current_senders->begin();
       sender_it != current_senders->end();
       /* incremented manually */) {
    const RtpSenderInfo& info = *sender_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.first_ssrc);
    std::string params_stream_id;
    if (params) {
      params_stream_id =
          (!params->first_stream_id().empty() ? params->first_stream_id()
                                              : kDefaultStreamId);
    }
    bool sender_exists = params && params->id == info.sender_id &&
                         params_stream_id == info.stream_id;
    // If this is a default track, and we still need it, don't remove it.
    if ((info.stream_id == kDefaultStreamId && default_sender_needed) ||
        sender_exists) {
      ++sender_it;
    } else {
      OnRemoteSenderRemoved(info, media_type);
      sender_it = current_senders->erase(sender_it);
    }
  }

  // Find new and active senders.
  for (const cricket::StreamParams& params : streams) {
    if (!params.has_ssrcs()) {
      // The remote endpoint has streams, but didn't signal ssrcs. For an active
      // sender, this means it is coming from a Unified Plan endpoint,so we just
      // create a default.
      default_sender_needed = true;
      break;
    }

    // |params.id| is the sender id and the stream id uses the first of
    // |params.stream_ids|. The remote description could come from a Unified
    // Plan endpoint, with multiple or no stream_ids() signaled. Since this is
    // not supported in Plan B, we just take the first here and create the
    // default stream ID if none is specified.
    const std::string& stream_id =
        (!params.first_stream_id().empty() ? params.first_stream_id()
                                           : kDefaultStreamId);
    const std::string& sender_id = params.id;
    uint32_t ssrc = params.first_ssrc();

    rtc::scoped_refptr<MediaStreamInterface> stream =
        remote_streams_->find(stream_id);
    if (!stream) {
      // This is a new MediaStream. Create a new remote MediaStream.
      stream = MediaStreamProxy::Create(rtc::Thread::Current(),
                                        MediaStream::Create(stream_id));
      remote_streams_->AddStream(stream);
      new_streams->AddStream(stream);
    }

    const RtpSenderInfo* sender_info =
        FindSenderInfo(*current_senders, stream_id, sender_id);
    if (!sender_info) {
      current_senders->push_back(RtpSenderInfo(stream_id, sender_id, ssrc));
      OnRemoteSenderAdded(current_senders->back(), media_type);
    }
  }

  // Add default sender if necessary.
  if (default_sender_needed) {
    rtc::scoped_refptr<MediaStreamInterface> default_stream =
        remote_streams_->find(kDefaultStreamId);
    if (!default_stream) {
      // Create the new default MediaStream.
      default_stream = MediaStreamProxy::Create(
          rtc::Thread::Current(), MediaStream::Create(kDefaultStreamId));
      remote_streams_->AddStream(default_stream);
      new_streams->AddStream(default_stream);
    }
    std::string default_sender_id = (media_type == cricket::MEDIA_TYPE_AUDIO)
                                        ? kDefaultAudioSenderId
                                        : kDefaultVideoSenderId;
    const RtpSenderInfo* default_sender_info =
        FindSenderInfo(*current_senders, kDefaultStreamId, default_sender_id);
    if (!default_sender_info) {
      current_senders->push_back(
          RtpSenderInfo(kDefaultStreamId, default_sender_id, /*ssrc=*/0));
      OnRemoteSenderAdded(current_senders->back(), media_type);
    }
  }
}

void PeerConnection::OnRemoteSenderAdded(const RtpSenderInfo& sender_info,
                                         cricket::MediaType media_type) {
  RTC_LOG(LS_INFO) << "Creating " << cricket::MediaTypeToString(media_type)
                   << " receiver for track_id=" << sender_info.sender_id
                   << " and stream_id=" << sender_info.stream_id;

  MediaStreamInterface* stream = remote_streams_->find(sender_info.stream_id);
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    CreateAudioReceiver(stream, sender_info);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    CreateVideoReceiver(stream, sender_info);
  } else {
    RTC_NOTREACHED() << "Invalid media type";
  }
}

void PeerConnection::OnRemoteSenderRemoved(const RtpSenderInfo& sender_info,
                                           cricket::MediaType media_type) {
  RTC_LOG(LS_INFO) << "Removing " << cricket::MediaTypeToString(media_type)
                   << " receiver for track_id=" << sender_info.sender_id
                   << " and stream_id=" << sender_info.stream_id;

  MediaStreamInterface* stream = remote_streams_->find(sender_info.stream_id);

  rtc::scoped_refptr<RtpReceiverInterface> receiver;
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    // When the MediaEngine audio channel is destroyed, the RemoteAudioSource
    // will be notified which will end the AudioRtpReceiver::track().
    receiver = RemoveAndStopReceiver(sender_info);
    rtc::scoped_refptr<AudioTrackInterface> audio_track =
        stream->FindAudioTrack(sender_info.sender_id);
    if (audio_track) {
      stream->RemoveTrack(audio_track);
    }
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    // Stopping or destroying a VideoRtpReceiver will end the
    // VideoRtpReceiver::track().
    receiver = RemoveAndStopReceiver(sender_info);
    rtc::scoped_refptr<VideoTrackInterface> video_track =
        stream->FindVideoTrack(sender_info.sender_id);
    if (video_track) {
      // There's no guarantee the track is still available, e.g. the track may
      // have been removed from the stream by an application.
      stream->RemoveTrack(video_track);
    }
  } else {
    RTC_NOTREACHED() << "Invalid media type";
  }
  if (receiver) {
    Observer()->OnRemoveTrack(receiver);
  }
}

void PeerConnection::UpdateEndedRemoteMediaStreams() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams_to_remove;
  for (size_t i = 0; i < remote_streams_->count(); ++i) {
    MediaStreamInterface* stream = remote_streams_->at(i);
    if (stream->GetAudioTracks().empty() && stream->GetVideoTracks().empty()) {
      streams_to_remove.push_back(stream);
    }
  }

  for (auto& stream : streams_to_remove) {
    remote_streams_->RemoveStream(stream);
    Observer()->OnRemoveStream(std::move(stream));
  }
}

void PeerConnection::UpdateLocalSenders(
    const std::vector<cricket::StreamParams>& streams,
    cricket::MediaType media_type) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<RtpSenderInfo>* current_senders = GetLocalSenderInfos(media_type);

  // Find removed tracks. I.e., tracks where the track id, stream id or ssrc
  // don't match the new StreamParam.
  for (auto sender_it = current_senders->begin();
       sender_it != current_senders->end();
       /* incremented manually */) {
    const RtpSenderInfo& info = *sender_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.first_ssrc);
    if (!params || params->id != info.sender_id ||
        params->first_stream_id() != info.stream_id) {
      OnLocalSenderRemoved(info, media_type);
      sender_it = current_senders->erase(sender_it);
    } else {
      ++sender_it;
    }
  }

  // Find new and active senders.
  for (const cricket::StreamParams& params : streams) {
    // The sync_label is the MediaStream label and the |stream.id| is the
    // sender id.
    const std::string& stream_id = params.first_stream_id();
    const std::string& sender_id = params.id;
    uint32_t ssrc = params.first_ssrc();
    const RtpSenderInfo* sender_info =
        FindSenderInfo(*current_senders, stream_id, sender_id);
    if (!sender_info) {
      current_senders->push_back(RtpSenderInfo(stream_id, sender_id, ssrc));
      OnLocalSenderAdded(current_senders->back(), media_type);
    }
  }
}

void PeerConnection::OnLocalSenderAdded(const RtpSenderInfo& sender_info,
                                        cricket::MediaType media_type) {
  RTC_DCHECK(!IsUnifiedPlan());
  auto sender = FindSenderById(sender_info.sender_id);
  if (!sender) {
    RTC_LOG(LS_WARNING) << "An unknown RtpSender with id "
                        << sender_info.sender_id
                        << " has been configured in the local description.";
    return;
  }

  if (sender->media_type() != media_type) {
    RTC_LOG(LS_WARNING) << "An RtpSender has been configured in the local"
                           " description with an unexpected media type.";
    return;
  }

  sender->internal()->set_stream_ids({sender_info.stream_id});
  sender->internal()->SetSsrc(sender_info.first_ssrc);
}

void PeerConnection::OnLocalSenderRemoved(const RtpSenderInfo& sender_info,
                                          cricket::MediaType media_type) {
  auto sender = FindSenderById(sender_info.sender_id);
  if (!sender) {
    // This is the normal case. I.e., RemoveStream has been called and the
    // SessionDescriptions has been renegotiated.
    return;
  }

  // A sender has been removed from the SessionDescription but it's still
  // associated with the PeerConnection. This only occurs if the SDP doesn't
  // match with the calls to CreateSender, AddStream and RemoveStream.
  if (sender->media_type() != media_type) {
    RTC_LOG(LS_WARNING) << "An RtpSender has been configured in the local"
                           " description with an unexpected media type.";
    return;
  }

  sender->internal()->SetSsrc(0);
}

void PeerConnection::OnSctpDataChannelClosed(DataChannelInterface* channel) {
  // Since data_channel_controller doesn't do signals, this
  // signal is relayed here.
  data_channel_controller_.OnSctpDataChannelClosed(
      static_cast<SctpDataChannel*>(channel));
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetAudioTransceiver() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // This method only works with Plan B SDP, where there is a single
  // audio/video transceiver.
  RTC_DCHECK(!IsUnifiedPlan());
  for (auto transceiver : transceivers_.List()) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      return transceiver;
    }
  }
  RTC_NOTREACHED();
  return nullptr;
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetVideoTransceiver() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // This method only works with Plan B SDP, where there is a single
  // audio/video transceiver.
  RTC_DCHECK(!IsUnifiedPlan());
  for (auto transceiver : transceivers_.List()) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      return transceiver;
    }
  }
  RTC_NOTREACHED();
  return nullptr;
}

rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
PeerConnection::FindSenderForTrack(MediaStreamTrackInterface* track) const {
  for (const auto& transceiver : transceivers_.List()) {
    for (auto sender : transceiver->internal()->senders()) {
      if (sender->track() == track) {
        return sender;
      }
    }
  }
  return nullptr;
}

rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
PeerConnection::FindSenderById(const std::string& sender_id) const {
  for (const auto& transceiver : transceivers_.List()) {
    for (auto sender : transceiver->internal()->senders()) {
      if (sender->id() == sender_id) {
        return sender;
      }
    }
  }
  return nullptr;
}

rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
PeerConnection::FindReceiverById(const std::string& receiver_id) const {
  for (const auto& transceiver : transceivers_.List()) {
    for (auto receiver : transceiver->internal()->receivers()) {
      if (receiver->id() == receiver_id) {
        return receiver;
      }
    }
  }
  return nullptr;
}

std::vector<PeerConnection::RtpSenderInfo>*
PeerConnection::GetRemoteSenderInfos(cricket::MediaType media_type) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
  return (media_type == cricket::MEDIA_TYPE_AUDIO)
             ? &remote_audio_sender_infos_
             : &remote_video_sender_infos_;
}

std::vector<PeerConnection::RtpSenderInfo>* PeerConnection::GetLocalSenderInfos(
    cricket::MediaType media_type) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
  return (media_type == cricket::MEDIA_TYPE_AUDIO) ? &local_audio_sender_infos_
                                                   : &local_video_sender_infos_;
}

const PeerConnection::RtpSenderInfo* PeerConnection::FindSenderInfo(
    const std::vector<PeerConnection::RtpSenderInfo>& infos,
    const std::string& stream_id,
    const std::string sender_id) const {
  for (const RtpSenderInfo& sender_info : infos) {
    if (sender_info.stream_id == stream_id &&
        sender_info.sender_id == sender_id) {
      return &sender_info;
    }
  }
  return nullptr;
}

SctpDataChannel* PeerConnection::FindDataChannelBySid(int sid) const {
  return data_channel_controller_.FindDataChannelBySid(sid);
}

PeerConnection::InitializePortAllocatorResult
PeerConnection::InitializePortAllocator_n(
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    const RTCConfiguration& configuration) {
  RTC_DCHECK_RUN_ON(network_thread());

  port_allocator_->Initialize();
  // To handle both internal and externally created port allocator, we will
  // enable BUNDLE here.
  int port_allocator_flags = port_allocator_->flags();
  port_allocator_flags |= cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                          cricket::PORTALLOCATOR_ENABLE_IPV6 |
                          cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI;
  // If the disable-IPv6 flag was specified, we'll not override it
  // by experiment.
  if (configuration.disable_ipv6) {
    port_allocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  } else if (absl::StartsWith(factory_->trials().Lookup("WebRTC-IPv6Default"),
                              "Disabled")) {
    port_allocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  }
  if (configuration.disable_ipv6_on_wifi) {
    port_allocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI);
    RTC_LOG(LS_INFO) << "IPv6 candidates on Wi-Fi are disabled.";
  }

  if (configuration.tcp_candidate_policy == kTcpCandidatePolicyDisabled) {
    port_allocator_flags |= cricket::PORTALLOCATOR_DISABLE_TCP;
    RTC_LOG(LS_INFO) << "TCP candidates are disabled.";
  }

  if (configuration.candidate_network_policy ==
      kCandidateNetworkPolicyLowCost) {
    port_allocator_flags |= cricket::PORTALLOCATOR_DISABLE_COSTLY_NETWORKS;
    RTC_LOG(LS_INFO) << "Do not gather candidates on high-cost networks";
  }

  if (configuration.disable_link_local_networks) {
    port_allocator_flags |= cricket::PORTALLOCATOR_DISABLE_LINK_LOCAL_NETWORKS;
    RTC_LOG(LS_INFO) << "Disable candidates on link-local network interfaces.";
  }

  port_allocator_->set_flags(port_allocator_flags);
  // No step delay is used while allocating ports.
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);
  port_allocator_->SetCandidateFilter(
      ConvertIceTransportTypeToCandidateFilter(configuration.type));
  port_allocator_->set_max_ipv6_networks(configuration.max_ipv6_networks);

  auto turn_servers_copy = turn_servers;
  for (auto& turn_server : turn_servers_copy) {
    turn_server.tls_cert_verifier = tls_cert_verifier_.get();
  }
  // Call this last since it may create pooled allocator sessions using the
  // properties set above.
  port_allocator_->SetConfiguration(
      stun_servers, std::move(turn_servers_copy),
      configuration.ice_candidate_pool_size,
      configuration.GetTurnPortPrunePolicy(), configuration.turn_customizer,
      configuration.stun_candidate_keepalive_interval);

  InitializePortAllocatorResult res;
  res.enable_ipv6 = port_allocator_flags & cricket::PORTALLOCATOR_ENABLE_IPV6;
  return res;
}

bool PeerConnection::ReconfigurePortAllocator_n(
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    IceTransportsType type,
    int candidate_pool_size,
    PortPrunePolicy turn_port_prune_policy,
    webrtc::TurnCustomizer* turn_customizer,
    absl::optional<int> stun_candidate_keepalive_interval,
    bool have_local_description) {
  port_allocator_->SetCandidateFilter(
      ConvertIceTransportTypeToCandidateFilter(type));
  // According to JSEP, after setLocalDescription, changing the candidate pool
  // size is not allowed, and changing the set of ICE servers will not result
  // in new candidates being gathered.
  if (have_local_description) {
    port_allocator_->FreezeCandidatePool();
  }
  // Add the custom tls turn servers if they exist.
  auto turn_servers_copy = turn_servers;
  for (auto& turn_server : turn_servers_copy) {
    turn_server.tls_cert_verifier = tls_cert_verifier_.get();
  }
  // Call this last since it may create pooled allocator sessions using the
  // candidate filter set above.
  return port_allocator_->SetConfiguration(
      stun_servers, std::move(turn_servers_copy), candidate_pool_size,
      turn_port_prune_policy, turn_customizer,
      stun_candidate_keepalive_interval);
}

cricket::ChannelManager* PeerConnection::channel_manager() const {
  return factory_->channel_manager();
}

bool PeerConnection::StartRtcEventLog_w(
    std::unique_ptr<RtcEventLogOutput> output,
    int64_t output_period_ms) {
  RTC_DCHECK_RUN_ON(worker_thread());
  if (!event_log_) {
    return false;
  }
  return event_log_->StartLogging(std::move(output), output_period_ms);
}

void PeerConnection::StopRtcEventLog_w() {
  RTC_DCHECK_RUN_ON(worker_thread());
  if (event_log_) {
    event_log_->StopLogging();
  }
}

cricket::ChannelInterface* PeerConnection::GetChannel(
    const std::string& content_name) {
  for (const auto& transceiver : transceivers_.List()) {
    cricket::ChannelInterface* channel = transceiver->internal()->channel();
    if (channel && channel->content_name() == content_name) {
      return channel;
    }
  }
  if (rtp_data_channel() &&
      rtp_data_channel()->content_name() == content_name) {
    return rtp_data_channel();
  }
  return nullptr;
}

bool PeerConnection::GetSctpSslRole(rtc::SSLRole* role) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!local_description() || !remote_description()) {
    RTC_LOG(LS_VERBOSE)
        << "Local and Remote descriptions must be applied to get the "
           "SSL Role of the SCTP transport.";
    return false;
  }
  if (!data_channel_controller_.data_channel_transport()) {
    RTC_LOG(LS_INFO) << "Non-rejected SCTP m= section is needed to get the "
                        "SSL Role of the SCTP transport.";
    return false;
  }

  absl::optional<rtc::SSLRole> dtls_role;
  if (sctp_mid_s_) {
    dtls_role = transport_controller_->GetDtlsRole(*sctp_mid_s_);
    if (!dtls_role && sdp_handler_.is_caller().has_value()) {
      dtls_role = *sdp_handler_.is_caller() ? rtc::SSL_SERVER : rtc::SSL_CLIENT;
    }
    *role = *dtls_role;
    return true;
  }
  return false;
}

bool PeerConnection::GetSslRole(const std::string& content_name,
                                rtc::SSLRole* role) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!local_description() || !remote_description()) {
    RTC_LOG(LS_INFO)
        << "Local and Remote descriptions must be applied to get the "
           "SSL Role of the session.";
    return false;
  }

  auto dtls_role = transport_controller_->GetDtlsRole(content_name);
  if (dtls_role) {
    *role = *dtls_role;
    return true;
  }
  return false;
}

void PeerConnection::SetSessionError(SessionError error,
                                     const std::string& error_desc) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (error != session_error_) {
    session_error_ = error;
    session_error_desc_ = error_desc;
  }
}

bool PeerConnection::UpdatePayloadTypeDemuxingState(
    cricket::ContentSource source) {
  // We may need to delete any created default streams and disable creation of
  // new ones on the basis of payload type. This is needed to avoid SSRC
  // collisions in Call's RtpDemuxer, in the case that a transceiver has
  // created a default stream, and then some other channel gets the SSRC
  // signaled in the corresponding Unified Plan "m=" section. Specifically, we
  // need to disable payload type based demuxing when two bundled "m=" sections
  // are using the same payload type(s). For more context see:
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=11477
  const SessionDescriptionInterface* sdesc =
      (source == cricket::CS_LOCAL ? local_description()
                                   : remote_description());
  const cricket::ContentGroup* bundle_group =
      sdesc->description()->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  std::set<int> audio_payload_types;
  std::set<int> video_payload_types;
  bool pt_demuxing_enabled_audio = true;
  bool pt_demuxing_enabled_video = true;
  for (auto& content_info : sdesc->description()->contents()) {
    // If this m= section isn't bundled, it's safe to demux by payload type
    // since other m= sections using the same payload type will also be using
    // different transports.
    if (!bundle_group || !bundle_group->HasContentName(content_info.name)) {
      continue;
    }
    if (content_info.rejected ||
        (source == cricket::ContentSource::CS_LOCAL &&
         !RtpTransceiverDirectionHasRecv(
             content_info.media_description()->direction())) ||
        (source == cricket::ContentSource::CS_REMOTE &&
         !RtpTransceiverDirectionHasSend(
             content_info.media_description()->direction()))) {
      // Ignore transceivers that are not receiving.
      continue;
    }
    switch (content_info.media_description()->type()) {
      case cricket::MediaType::MEDIA_TYPE_AUDIO: {
        const cricket::AudioContentDescription* audio_desc =
            content_info.media_description()->as_audio();
        for (const cricket::AudioCodec& audio : audio_desc->codecs()) {
          if (audio_payload_types.count(audio.id)) {
            // Two m= sections are using the same payload type, thus demuxing
            // by payload type is not possible.
            pt_demuxing_enabled_audio = false;
          }
          audio_payload_types.insert(audio.id);
        }
        break;
      }
      case cricket::MediaType::MEDIA_TYPE_VIDEO: {
        const cricket::VideoContentDescription* video_desc =
            content_info.media_description()->as_video();
        for (const cricket::VideoCodec& video : video_desc->codecs()) {
          if (video_payload_types.count(video.id)) {
            // Two m= sections are using the same payload type, thus demuxing
            // by payload type is not possible.
            pt_demuxing_enabled_video = false;
          }
          video_payload_types.insert(video.id);
        }
        break;
      }
      default:
        // Ignore data channels.
        continue;
    }
  }

  // Gather all updates ahead of time so that all channels can be updated in a
  // single Invoke; necessary due to thread guards.
  std::vector<std::pair<RtpTransceiverDirection, cricket::ChannelInterface*>>
      channels_to_update;
  for (const auto& transceiver : transceivers_.List()) {
    cricket::ChannelInterface* channel = transceiver->internal()->channel();
    const ContentInfo* content =
        sdp_handler_.FindMediaSectionForTransceiver(transceiver, sdesc);
    if (!channel || !content) {
      continue;
    }
    RtpTransceiverDirection local_direction =
        content->media_description()->direction();
    if (source == cricket::CS_REMOTE) {
      local_direction = RtpTransceiverDirectionReversed(local_direction);
    }
    channels_to_update.emplace_back(local_direction,
                                    transceiver->internal()->channel());
  }

  if (channels_to_update.empty()) {
    return true;
  }
  return worker_thread()->Invoke<bool>(
      RTC_FROM_HERE, [&channels_to_update, bundle_group,
                      pt_demuxing_enabled_audio, pt_demuxing_enabled_video]() {
        for (const auto& it : channels_to_update) {
          RtpTransceiverDirection local_direction = it.first;
          cricket::ChannelInterface* channel = it.second;
          cricket::MediaType media_type = channel->media_type();
          bool in_bundle_group = (bundle_group && bundle_group->HasContentName(
                                                      channel->content_name()));
          if (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO) {
            if (!channel->SetPayloadTypeDemuxingEnabled(
                    (!in_bundle_group || pt_demuxing_enabled_audio) &&
                    RtpTransceiverDirectionHasRecv(local_direction))) {
              return false;
            }
          } else if (media_type == cricket::MediaType::MEDIA_TYPE_VIDEO) {
            if (!channel->SetPayloadTypeDemuxingEnabled(
                    (!in_bundle_group || pt_demuxing_enabled_video) &&
                    RtpTransceiverDirectionHasRecv(local_direction))) {
              return false;
            }
          }
        }
        return true;
      });
}

RTCError PeerConnection::PushdownMediaDescription(
    SdpType type,
    cricket::ContentSource source) {
  const SessionDescriptionInterface* sdesc =
      (source == cricket::CS_LOCAL ? local_description()
                                   : remote_description());
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(sdesc);

  if (!UpdatePayloadTypeDemuxingState(source)) {
    // Note that this is never expected to fail, since RtpDemuxer doesn't return
    // an error when changing payload type demux criteria, which is all this
    // does.
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to update payload type demuxing state.");
  }

  // Push down the new SDP media section for each audio/video transceiver.
  for (const auto& transceiver : transceivers_.List()) {
    const ContentInfo* content_info =
        sdp_handler_.FindMediaSectionForTransceiver(transceiver, sdesc);
    cricket::ChannelInterface* channel = transceiver->internal()->channel();
    if (!channel || !content_info || content_info->rejected) {
      continue;
    }
    const MediaContentDescription* content_desc =
        content_info->media_description();
    if (!content_desc) {
      continue;
    }
    std::string error;
    bool success = (source == cricket::CS_LOCAL)
                       ? channel->SetLocalContent(content_desc, type, &error)
                       : channel->SetRemoteContent(content_desc, type, &error);
    if (!success) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, error);
    }
  }

  // If using the RtpDataChannel, push down the new SDP section for it too.
  if (data_channel_controller_.rtp_data_channel()) {
    const ContentInfo* data_content =
        cricket::GetFirstDataContent(sdesc->description());
    if (data_content && !data_content->rejected) {
      const MediaContentDescription* data_desc =
          data_content->media_description();
      if (data_desc) {
        std::string error;
        bool success =
            (source == cricket::CS_LOCAL)
                ? data_channel_controller_.rtp_data_channel()->SetLocalContent(
                      data_desc, type, &error)
                : data_channel_controller_.rtp_data_channel()->SetRemoteContent(
                      data_desc, type, &error);
        if (!success) {
          LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, error);
        }
      }
    }
  }

  // Need complete offer/answer with an SCTP m= section before starting SCTP,
  // according to https://tools.ietf.org/html/draft-ietf-mmusic-sctp-sdp-19
  if (sctp_mid_s_ && local_description() && remote_description()) {
    rtc::scoped_refptr<SctpTransport> sctp_transport =
        transport_controller_->GetSctpTransport(*sctp_mid_s_);
    auto local_sctp_description = cricket::GetFirstSctpDataContentDescription(
        local_description()->description());
    auto remote_sctp_description = cricket::GetFirstSctpDataContentDescription(
        remote_description()->description());
    if (sctp_transport && local_sctp_description && remote_sctp_description) {
      int max_message_size;
      // A remote max message size of zero means "any size supported".
      // We configure the connection with our own max message size.
      if (remote_sctp_description->max_message_size() == 0) {
        max_message_size = local_sctp_description->max_message_size();
      } else {
        max_message_size =
            std::min(local_sctp_description->max_message_size(),
                     remote_sctp_description->max_message_size());
      }
      sctp_transport->Start(local_sctp_description->port(),
                            remote_sctp_description->port(), max_message_size);
    }
  }

  return RTCError::OK();
}

RTCError PeerConnection::PushdownTransportDescription(
    cricket::ContentSource source,
    SdpType type) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  if (source == cricket::CS_LOCAL) {
    const SessionDescriptionInterface* sdesc = local_description();
    RTC_DCHECK(sdesc);
    return transport_controller_->SetLocalDescription(type,
                                                      sdesc->description());
  } else {
    const SessionDescriptionInterface* sdesc = remote_description();
    RTC_DCHECK(sdesc);
    return transport_controller_->SetRemoteDescription(type,
                                                       sdesc->description());
  }
}

bool PeerConnection::GetTransportDescription(
    const SessionDescription* description,
    const std::string& content_name,
    cricket::TransportDescription* tdesc) {
  if (!description || !tdesc) {
    return false;
  }
  const TransportInfo* transport_info =
      description->GetTransportInfoByName(content_name);
  if (!transport_info) {
    return false;
  }
  *tdesc = transport_info->description;
  return true;
}

cricket::IceConfig PeerConnection::ParseIceConfig(
    const PeerConnectionInterface::RTCConfiguration& config) const {
  cricket::ContinualGatheringPolicy gathering_policy;
  switch (config.continual_gathering_policy) {
    case PeerConnectionInterface::GATHER_ONCE:
      gathering_policy = cricket::GATHER_ONCE;
      break;
    case PeerConnectionInterface::GATHER_CONTINUALLY:
      gathering_policy = cricket::GATHER_CONTINUALLY;
      break;
    default:
      RTC_NOTREACHED();
      gathering_policy = cricket::GATHER_ONCE;
  }

  cricket::IceConfig ice_config;
  ice_config.receiving_timeout = RTCConfigurationToIceConfigOptionalInt(
      config.ice_connection_receiving_timeout);
  ice_config.prioritize_most_likely_candidate_pairs =
      config.prioritize_most_likely_ice_candidate_pairs;
  ice_config.backup_connection_ping_interval =
      RTCConfigurationToIceConfigOptionalInt(
          config.ice_backup_candidate_pair_ping_interval);
  ice_config.continual_gathering_policy = gathering_policy;
  ice_config.presume_writable_when_fully_relayed =
      config.presume_writable_when_fully_relayed;
  ice_config.surface_ice_candidates_on_ice_transport_type_changed =
      config.surface_ice_candidates_on_ice_transport_type_changed;
  ice_config.ice_check_interval_strong_connectivity =
      config.ice_check_interval_strong_connectivity;
  ice_config.ice_check_interval_weak_connectivity =
      config.ice_check_interval_weak_connectivity;
  ice_config.ice_check_min_interval = config.ice_check_min_interval;
  ice_config.ice_unwritable_timeout = config.ice_unwritable_timeout;
  ice_config.ice_unwritable_min_checks = config.ice_unwritable_min_checks;
  ice_config.ice_inactive_timeout = config.ice_inactive_timeout;
  ice_config.stun_keepalive_interval = config.stun_candidate_keepalive_interval;
  ice_config.network_preference = config.network_preference;
  return ice_config;
}

std::vector<DataChannelStats> PeerConnection::GetDataChannelStats() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return data_channel_controller_.GetDataChannelStats();
}

absl::optional<std::string> PeerConnection::sctp_transport_name() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (sctp_mid_s_ && transport_controller_) {
    auto dtls_transport = transport_controller_->GetDtlsTransport(*sctp_mid_s_);
    if (dtls_transport) {
      return dtls_transport->transport_name();
    }
    return absl::optional<std::string>();
  }
  return absl::optional<std::string>();
}

cricket::CandidateStatsList PeerConnection::GetPooledCandidateStats() const {
  cricket::CandidateStatsList candidate_states_list;
  network_thread()->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&cricket::PortAllocator::GetCandidateStatsFromPooledSessions,
                port_allocator_.get(), &candidate_states_list));
  return candidate_states_list;
}

std::map<std::string, std::string> PeerConnection::GetTransportNamesByMid()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::map<std::string, std::string> transport_names_by_mid;
  for (const auto& transceiver : transceivers_.List()) {
    cricket::ChannelInterface* channel = transceiver->internal()->channel();
    if (channel) {
      transport_names_by_mid[channel->content_name()] =
          channel->transport_name();
    }
  }
  if (data_channel_controller_.rtp_data_channel()) {
    transport_names_by_mid[data_channel_controller_.rtp_data_channel()
                               ->content_name()] =
        data_channel_controller_.rtp_data_channel()->transport_name();
  }
  if (data_channel_controller_.data_channel_transport()) {
    absl::optional<std::string> transport_name = sctp_transport_name();
    RTC_DCHECK(transport_name);
    transport_names_by_mid[*sctp_mid_s_] = *transport_name;
  }
  return transport_names_by_mid;
}

std::map<std::string, cricket::TransportStats>
PeerConnection::GetTransportStatsByNames(
    const std::set<std::string>& transport_names) {
  if (!network_thread()->IsCurrent()) {
    return network_thread()
        ->Invoke<std::map<std::string, cricket::TransportStats>>(
            RTC_FROM_HERE,
            [&] { return GetTransportStatsByNames(transport_names); });
  }
  RTC_DCHECK_RUN_ON(network_thread());
  std::map<std::string, cricket::TransportStats> transport_stats_by_name;
  for (const std::string& transport_name : transport_names) {
    cricket::TransportStats transport_stats;
    bool success =
        transport_controller_->GetStats(transport_name, &transport_stats);
    if (success) {
      transport_stats_by_name[transport_name] = std::move(transport_stats);
    } else {
      RTC_LOG(LS_ERROR) << "Failed to get transport stats for transport_name="
                        << transport_name;
    }
  }
  return transport_stats_by_name;
}

bool PeerConnection::GetLocalCertificate(
    const std::string& transport_name,
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
  if (!certificate) {
    return false;
  }
  *certificate = transport_controller_->GetLocalCertificate(transport_name);
  return *certificate != nullptr;
}

std::unique_ptr<rtc::SSLCertChain> PeerConnection::GetRemoteSSLCertChain(
    const std::string& transport_name) {
  return transport_controller_->GetRemoteSSLCertChain(transport_name);
}

cricket::DataChannelType PeerConnection::data_channel_type() const {
  return data_channel_controller_.data_channel_type();
}

bool PeerConnection::IceRestartPending(const std::string& content_name) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.IceRestartPending(content_name);
}

bool PeerConnection::NeedsIceRestart(const std::string& content_name) const {
  return transport_controller_->NeedsIceRestart(content_name);
}

void PeerConnection::OnCertificateReady(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  transport_controller_->SetLocalCertificate(certificate);
}

void PeerConnection::OnTransportControllerConnectionState(
    cricket::IceConnectionState state) {
  switch (state) {
    case cricket::kIceConnectionConnecting:
      // If the current state is Connected or Completed, then there were
      // writable channels but now there are not, so the next state must
      // be Disconnected.
      // kIceConnectionConnecting is currently used as the default,
      // un-connected state by the TransportController, so its only use is
      // detecting disconnections.
      if (ice_connection_state_ ==
              PeerConnectionInterface::kIceConnectionConnected ||
          ice_connection_state_ ==
              PeerConnectionInterface::kIceConnectionCompleted) {
        SetIceConnectionState(
            PeerConnectionInterface::kIceConnectionDisconnected);
      }
      break;
    case cricket::kIceConnectionFailed:
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionFailed);
      break;
    case cricket::kIceConnectionConnected:
      RTC_LOG(LS_INFO) << "Changing to ICE connected state because "
                          "all transports are writable.";
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionConnected);
      NoteUsageEvent(UsageEvent::ICE_STATE_CONNECTED);
      break;
    case cricket::kIceConnectionCompleted:
      RTC_LOG(LS_INFO) << "Changing to ICE completed state because "
                          "all transports are complete.";
      if (ice_connection_state_ !=
          PeerConnectionInterface::kIceConnectionConnected) {
        // If jumping directly from "checking" to "connected",
        // signal "connected" first.
        SetIceConnectionState(PeerConnectionInterface::kIceConnectionConnected);
      }
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionCompleted);
      NoteUsageEvent(UsageEvent::ICE_STATE_CONNECTED);
      ReportTransportStats();
      break;
    default:
      RTC_NOTREACHED();
  }
}

void PeerConnection::OnTransportControllerCandidatesGathered(
    const std::string& transport_name,
    const cricket::Candidates& candidates) {
  int sdp_mline_index;
  if (!GetLocalCandidateMediaIndex(transport_name, &sdp_mline_index)) {
    RTC_LOG(LS_ERROR)
        << "OnTransportControllerCandidatesGathered: content name "
        << transport_name << " not found";
    return;
  }

  for (cricket::Candidates::const_iterator citer = candidates.begin();
       citer != candidates.end(); ++citer) {
    // Use transport_name as the candidate media id.
    std::unique_ptr<JsepIceCandidate> candidate(
        new JsepIceCandidate(transport_name, sdp_mline_index, *citer));
    sdp_handler_.AddLocalIceCandidate(candidate.get());
    OnIceCandidate(std::move(candidate));
  }
}

void PeerConnection::OnTransportControllerCandidateError(
    const cricket::IceCandidateErrorEvent& event) {
  OnIceCandidateError(event.address, event.port, event.url, event.error_code,
                      event.error_text);
}

void PeerConnection::OnTransportControllerCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  // Sanity check.
  for (const cricket::Candidate& candidate : candidates) {
    if (candidate.transport_name().empty()) {
      RTC_LOG(LS_ERROR) << "OnTransportControllerCandidatesRemoved: "
                           "empty content name in candidate "
                        << candidate.ToString();
      return;
    }
  }
  sdp_handler_.RemoveLocalIceCandidates(candidates);
  OnIceCandidatesRemoved(candidates);
}

void PeerConnection::OnTransportControllerCandidateChanged(
    const cricket::CandidatePairChangeEvent& event) {
  OnSelectedCandidatePairChanged(event);
}

void PeerConnection::OnTransportControllerDtlsHandshakeError(
    rtc::SSLHandshakeError error) {
  RTC_HISTOGRAM_ENUMERATION(
      "WebRTC.PeerConnection.DtlsHandshakeError", static_cast<int>(error),
      static_cast<int>(rtc::SSLHandshakeError::MAX_VALUE));
}

void PeerConnection::EnableSending() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (const auto& transceiver : transceivers_.List()) {
    cricket::ChannelInterface* channel = transceiver->internal()->channel();
    if (channel && !channel->enabled()) {
      channel->Enable(true);
    }
  }

  if (data_channel_controller_.rtp_data_channel() &&
      !data_channel_controller_.rtp_data_channel()->enabled()) {
    data_channel_controller_.rtp_data_channel()->Enable(true);
  }
}

// Returns the media index for a local ice candidate given the content name.
bool PeerConnection::GetLocalCandidateMediaIndex(
    const std::string& content_name,
    int* sdp_mline_index) {
  if (!local_description() || !sdp_mline_index) {
    return false;
  }

  bool content_found = false;
  const ContentInfos& contents = local_description()->description()->contents();
  for (size_t index = 0; index < contents.size(); ++index) {
    if (contents[index].name == content_name) {
      *sdp_mline_index = static_cast<int>(index);
      content_found = true;
      break;
    }
  }
  return content_found;
}

bool PeerConnection::UseCandidatesInSessionDescription(
    const SessionDescriptionInterface* remote_desc) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!remote_desc) {
    return true;
  }
  bool ret = true;

  for (size_t m = 0; m < remote_desc->number_of_mediasections(); ++m) {
    const IceCandidateCollection* candidates = remote_desc->candidates(m);
    for (size_t n = 0; n < candidates->count(); ++n) {
      const IceCandidateInterface* candidate = candidates->at(n);
      bool valid = false;
      if (!ReadyToUseRemoteCandidate(candidate, remote_desc, &valid)) {
        if (valid) {
          RTC_LOG(LS_INFO)
              << "UseCandidatesInSessionDescription: Not ready to use "
                 "candidate.";
        }
        continue;
      }
      ret = UseCandidate(candidate);
      if (!ret) {
        break;
      }
    }
  }
  return ret;
}

bool PeerConnection::UseCandidate(const IceCandidateInterface* candidate) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTCErrorOr<const cricket::ContentInfo*> result =
      FindContentInfo(remote_description(), candidate);
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "UseCandidate: Invalid candidate. "
                      << result.error().message();
    return false;
  }
  std::vector<cricket::Candidate> candidates;
  candidates.push_back(candidate->candidate());
  // Invoking BaseSession method to handle remote candidates.
  RTCError error = transport_controller_->AddRemoteCandidates(
      result.value()->name, candidates);
  if (error.ok()) {
    ReportRemoteIceCandidateAdded(candidate->candidate());
    // Candidates successfully submitted for checking.
    if (ice_connection_state_ == PeerConnectionInterface::kIceConnectionNew ||
        ice_connection_state_ ==
            PeerConnectionInterface::kIceConnectionDisconnected) {
      // If state is New, then the session has just gotten its first remote ICE
      // candidates, so go to Checking.
      // If state is Disconnected, the session is re-using old candidates or
      // receiving additional ones, so go to Checking.
      // If state is Connected, stay Connected.
      // TODO(bemasc): If state is Connected, and the new candidates are for a
      // newly added transport, then the state actually _should_ move to
      // checking.  Add a way to distinguish that case.
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionChecking);
    }
    // TODO(bemasc): If state is Completed, go back to Connected.
  } else {
    RTC_LOG(LS_WARNING) << error.message();
  }
  return true;
}

RTCErrorOr<const cricket::ContentInfo*> PeerConnection::FindContentInfo(
    const SessionDescriptionInterface* description,
    const IceCandidateInterface* candidate) {
  if (candidate->sdp_mline_index() >= 0) {
    size_t mediacontent_index =
        static_cast<size_t>(candidate->sdp_mline_index());
    size_t content_size = description->description()->contents().size();
    if (mediacontent_index < content_size) {
      return &description->description()->contents()[mediacontent_index];
    } else {
      return RTCError(RTCErrorType::INVALID_RANGE,
                      "Media line index (" +
                          rtc::ToString(candidate->sdp_mline_index()) +
                          ") out of range (number of mlines: " +
                          rtc::ToString(content_size) + ").");
    }
  } else if (!candidate->sdp_mid().empty()) {
    auto& contents = description->description()->contents();
    auto it = absl::c_find_if(
        contents, [candidate](const cricket::ContentInfo& content_info) {
          return content_info.mid() == candidate->sdp_mid();
        });
    if (it == contents.end()) {
      return RTCError(
          RTCErrorType::INVALID_PARAMETER,
          "Mid " + candidate->sdp_mid() +
              " specified but no media section with that mid found.");
    } else {
      return &*it;
    }
  }

  return RTCError(RTCErrorType::INVALID_PARAMETER,
                  "Neither sdp_mline_index nor sdp_mid specified.");
}

void PeerConnection::RemoveUnusedChannels(const SessionDescription* desc) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Destroy video channel first since it may have a pointer to the
  // voice channel.
  const cricket::ContentInfo* video_info = cricket::GetFirstVideoContent(desc);
  if (!video_info || video_info->rejected) {
    DestroyTransceiverChannel(GetVideoTransceiver());
  }

  const cricket::ContentInfo* audio_info = cricket::GetFirstAudioContent(desc);
  if (!audio_info || audio_info->rejected) {
    DestroyTransceiverChannel(GetAudioTransceiver());
  }

  const cricket::ContentInfo* data_info = cricket::GetFirstDataContent(desc);
  if (!data_info || data_info->rejected) {
    DestroyDataChannelTransport();
  }
}

RTCError PeerConnection::CreateChannels(const SessionDescription& desc) {
  // Creating the media channels. Transports should already have been created
  // at this point.
  RTC_DCHECK_RUN_ON(signaling_thread());
  const cricket::ContentInfo* voice = cricket::GetFirstAudioContent(&desc);
  if (voice && !voice->rejected &&
      !GetAudioTransceiver()->internal()->channel()) {
    cricket::VoiceChannel* voice_channel = CreateVoiceChannel(voice->name);
    if (!voice_channel) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                           "Failed to create voice channel.");
    }
    GetAudioTransceiver()->internal()->SetChannel(voice_channel);
  }

  const cricket::ContentInfo* video = cricket::GetFirstVideoContent(&desc);
  if (video && !video->rejected &&
      !GetVideoTransceiver()->internal()->channel()) {
    cricket::VideoChannel* video_channel = CreateVideoChannel(video->name);
    if (!video_channel) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                           "Failed to create video channel.");
    }
    GetVideoTransceiver()->internal()->SetChannel(video_channel);
  }

  const cricket::ContentInfo* data = cricket::GetFirstDataContent(&desc);
  if (data_channel_type() != cricket::DCT_NONE && data && !data->rejected &&
      !data_channel_controller_.rtp_data_channel() &&
      !data_channel_controller_.data_channel_transport()) {
    if (!CreateDataChannel(data->name)) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                           "Failed to create data channel.");
    }
  }

  return RTCError::OK();
}

// TODO(steveanton): Perhaps this should be managed by the RtpTransceiver.
cricket::VoiceChannel* PeerConnection::CreateVoiceChannel(
    const std::string& mid) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RtpTransportInternal* rtp_transport = GetRtpTransport(mid);

  // TODO(bugs.webrtc.org/11992): CreateVoiceChannel internally switches to the
  // worker thread. We shouldn't be using the |call_ptr_| hack here but simply
  // be on the worker thread and use |call_| (update upstream code).
  cricket::VoiceChannel* voice_channel = channel_manager()->CreateVoiceChannel(
      call_ptr_, configuration_.media_config, rtp_transport, signaling_thread(),
      mid, SrtpRequired(), GetCryptoOptions(), &ssrc_generator_,
      audio_options_);
  if (!voice_channel) {
    return nullptr;
  }
  voice_channel->SignalSentPacket().connect(this,
                                            &PeerConnection::OnSentPacket_w);
  voice_channel->SetRtpTransport(rtp_transport);

  return voice_channel;
}

// TODO(steveanton): Perhaps this should be managed by the RtpTransceiver.
cricket::VideoChannel* PeerConnection::CreateVideoChannel(
    const std::string& mid) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RtpTransportInternal* rtp_transport = GetRtpTransport(mid);

  // TODO(bugs.webrtc.org/11992): CreateVideoChannel internally switches to the
  // worker thread. We shouldn't be using the |call_ptr_| hack here but simply
  // be on the worker thread and use |call_| (update upstream code).
  cricket::VideoChannel* video_channel = channel_manager()->CreateVideoChannel(
      call_ptr_, configuration_.media_config, rtp_transport, signaling_thread(),
      mid, SrtpRequired(), GetCryptoOptions(), &ssrc_generator_, video_options_,
      video_bitrate_allocator_factory_.get());
  if (!video_channel) {
    return nullptr;
  }
  video_channel->SignalSentPacket().connect(this,
                                            &PeerConnection::OnSentPacket_w);
  video_channel->SetRtpTransport(rtp_transport);

  return video_channel;
}

bool PeerConnection::CreateDataChannel(const std::string& mid) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  switch (data_channel_type()) {
    case cricket::DCT_SCTP:
      if (network_thread()->Invoke<bool>(
              RTC_FROM_HERE,
              rtc::Bind(&PeerConnection::SetupDataChannelTransport_n, this,
                        mid))) {
        sctp_mid_s_ = mid;
      } else {
        return false;
      }
      return true;
    case cricket::DCT_RTP:
    default:
      RtpTransportInternal* rtp_transport = GetRtpTransport(mid);
      // TODO(bugs.webrtc.org/9987): set_rtp_data_channel() should be called on
      // the network thread like set_data_channel_transport is.
      data_channel_controller_.set_rtp_data_channel(
          channel_manager()->CreateRtpDataChannel(
              configuration_.media_config, rtp_transport, signaling_thread(),
              mid, SrtpRequired(), GetCryptoOptions(), &ssrc_generator_));
      if (!data_channel_controller_.rtp_data_channel()) {
        return false;
      }
      data_channel_controller_.rtp_data_channel()->SignalSentPacket().connect(
          this, &PeerConnection::OnSentPacket_w);
      data_channel_controller_.rtp_data_channel()->SetRtpTransport(
          rtp_transport);
      sdp_handler_.SetHavePendingRtpDataChannel();
      return true;
  }
  return false;
}

Call::Stats PeerConnection::GetCallStats() {
  if (!worker_thread()->IsCurrent()) {
    return worker_thread()->Invoke<Call::Stats>(
        RTC_FROM_HERE, rtc::Bind(&PeerConnection::GetCallStats, this));
  }
  RTC_DCHECK_RUN_ON(worker_thread());
  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;
  if (call_) {
    return call_->GetStats();
  } else {
    return Call::Stats();
  }
}

bool PeerConnection::SetupDataChannelTransport_n(const std::string& mid) {
  DataChannelTransportInterface* transport =
      transport_controller_->GetDataChannelTransport(mid);
  if (!transport) {
    RTC_LOG(LS_ERROR)
        << "Data channel transport is not available for data channels, mid="
        << mid;
    return false;
  }
  RTC_LOG(LS_INFO) << "Setting up data channel transport for mid=" << mid;

  data_channel_controller_.set_data_channel_transport(transport);
  data_channel_controller_.SetupDataChannelTransport_n();
  sctp_mid_n_ = mid;

  // Note: setting the data sink and checking initial state must be done last,
  // after setting up the data channel.  Setting the data sink may trigger
  // callbacks to PeerConnection which require the transport to be completely
  // set up (eg. OnReadyToSend()).
  transport->SetDataSink(&data_channel_controller_);
  return true;
}

void PeerConnection::TeardownDataChannelTransport_n() {
  if (!sctp_mid_n_ && !data_channel_controller_.data_channel_transport()) {
    return;
  }
  RTC_LOG(LS_INFO) << "Tearing down data channel transport for mid="
                   << *sctp_mid_n_;

  // |sctp_mid_| may still be active through an SCTP transport.  If not, unset
  // it.
  sctp_mid_n_.reset();
  data_channel_controller_.TeardownDataChannelTransport_n();
}

// Returns false if bundle is enabled and rtcp_mux is disabled.
bool PeerConnection::ValidateBundleSettings(const SessionDescription* desc) {
  bool bundle_enabled = desc->HasGroup(cricket::GROUP_TYPE_BUNDLE);
  if (!bundle_enabled)
    return true;

  const cricket::ContentGroup* bundle_group =
      desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  RTC_DCHECK(bundle_group != NULL);

  const cricket::ContentInfos& contents = desc->contents();
  for (cricket::ContentInfos::const_iterator citer = contents.begin();
       citer != contents.end(); ++citer) {
    const cricket::ContentInfo* content = (&*citer);
    RTC_DCHECK(content != NULL);
    if (bundle_group->HasContentName(content->name) && !content->rejected &&
        content->type == MediaProtocolType::kRtp) {
      if (!HasRtcpMuxEnabled(content))
        return false;
    }
  }
  // RTCP-MUX is enabled in all the contents.
  return true;
}

bool PeerConnection::HasRtcpMuxEnabled(const cricket::ContentInfo* content) {
  return content->media_description()->rtcp_mux();
}

const char* PeerConnection::SessionErrorToString(SessionError error) const {
  switch (error) {
    case SessionError::kNone:
      return "ERROR_NONE";
    case SessionError::kContent:
      return "ERROR_CONTENT";
    case SessionError::kTransport:
      return "ERROR_TRANSPORT";
  }
  RTC_NOTREACHED();
  return "";
}

std::string PeerConnection::GetSessionErrorMsg() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  rtc::StringBuilder desc;
  desc << kSessionError << SessionErrorToString(session_error()) << ". ";
  desc << kSessionErrorDesc << session_error_desc() << ".";
  return desc.Release();
}

void PeerConnection::ReportSdpFormatReceived(
    const SessionDescriptionInterface& remote_offer) {
  int num_audio_mlines = 0;
  int num_video_mlines = 0;
  int num_audio_tracks = 0;
  int num_video_tracks = 0;
  for (const ContentInfo& content : remote_offer.description()->contents()) {
    cricket::MediaType media_type = content.media_description()->type();
    int num_tracks = std::max(
        1, static_cast<int>(content.media_description()->streams().size()));
    if (media_type == cricket::MEDIA_TYPE_AUDIO) {
      num_audio_mlines += 1;
      num_audio_tracks += num_tracks;
    } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
      num_video_mlines += 1;
      num_video_tracks += num_tracks;
    }
  }
  SdpFormatReceived format = kSdpFormatReceivedNoTracks;
  if (num_audio_mlines > 1 || num_video_mlines > 1) {
    format = kSdpFormatReceivedComplexUnifiedPlan;
  } else if (num_audio_tracks > 1 || num_video_tracks > 1) {
    format = kSdpFormatReceivedComplexPlanB;
  } else if (num_audio_tracks > 0 || num_video_tracks > 0) {
    format = kSdpFormatReceivedSimple;
  }
  RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.SdpFormatReceived", format,
                            kSdpFormatReceivedMax);
}

void PeerConnection::ReportIceCandidateCollected(
    const cricket::Candidate& candidate) {
  NoteUsageEvent(UsageEvent::CANDIDATE_COLLECTED);
  if (candidate.address().IsPrivateIP()) {
    NoteUsageEvent(UsageEvent::PRIVATE_CANDIDATE_COLLECTED);
  }
  if (candidate.address().IsUnresolvedIP()) {
    NoteUsageEvent(UsageEvent::MDNS_CANDIDATE_COLLECTED);
  }
  if (candidate.address().family() == AF_INET6) {
    NoteUsageEvent(UsageEvent::IPV6_CANDIDATE_COLLECTED);
  }
}

void PeerConnection::ReportRemoteIceCandidateAdded(
    const cricket::Candidate& candidate) {
  NoteUsageEvent(UsageEvent::REMOTE_CANDIDATE_ADDED);
  if (candidate.address().IsPrivateIP()) {
    NoteUsageEvent(UsageEvent::REMOTE_PRIVATE_CANDIDATE_ADDED);
  }
  if (candidate.address().IsUnresolvedIP()) {
    NoteUsageEvent(UsageEvent::REMOTE_MDNS_CANDIDATE_ADDED);
  }
  if (candidate.address().family() == AF_INET6) {
    NoteUsageEvent(UsageEvent::REMOTE_IPV6_CANDIDATE_ADDED);
  }
}

void PeerConnection::NoteUsageEvent(UsageEvent event) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  usage_event_accumulator_ |= static_cast<int>(event);
}

void PeerConnection::ReportUsagePattern() const {
  RTC_DLOG(LS_INFO) << "Usage signature is " << usage_event_accumulator_;
  RTC_HISTOGRAM_ENUMERATION_SPARSE("WebRTC.PeerConnection.UsagePattern",
                                   usage_event_accumulator_,
                                   static_cast<int>(UsageEvent::MAX_VALUE));
  const int bad_bits =
      static_cast<int>(UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED) |
      static_cast<int>(UsageEvent::CANDIDATE_COLLECTED);
  const int good_bits =
      static_cast<int>(UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED) |
      static_cast<int>(UsageEvent::REMOTE_CANDIDATE_ADDED) |
      static_cast<int>(UsageEvent::ICE_STATE_CONNECTED);
  if ((usage_event_accumulator_ & bad_bits) == bad_bits &&
      (usage_event_accumulator_ & good_bits) == 0) {
    // If called after close(), we can't report, because observer may have
    // been deallocated, and therefore pointer is null. Write to log instead.
    if (observer_) {
      Observer()->OnInterestingUsage(usage_event_accumulator_);
    } else {
      RTC_LOG(LS_INFO) << "Interesting usage signature "
                       << usage_event_accumulator_
                       << " observed after observer shutdown";
    }
  }
}

void PeerConnection::ReportNegotiatedSdpSemantics(
    const SessionDescriptionInterface& answer) {
  SdpSemanticNegotiated semantics_negotiated;
  switch (answer.description()->msid_signaling()) {
    case 0:
      semantics_negotiated = kSdpSemanticNegotiatedNone;
      break;
    case cricket::kMsidSignalingMediaSection:
      semantics_negotiated = kSdpSemanticNegotiatedUnifiedPlan;
      break;
    case cricket::kMsidSignalingSsrcAttribute:
      semantics_negotiated = kSdpSemanticNegotiatedPlanB;
      break;
    case cricket::kMsidSignalingMediaSection |
        cricket::kMsidSignalingSsrcAttribute:
      semantics_negotiated = kSdpSemanticNegotiatedMixed;
      break;
    default:
      RTC_NOTREACHED();
  }
  RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.SdpSemanticNegotiated",
                            semantics_negotiated, kSdpSemanticNegotiatedMax);
}

// We need to check the local/remote description for the Transport instead of
// the session, because a new Transport added during renegotiation may have
// them unset while the session has them set from the previous negotiation.
// Not doing so may trigger the auto generation of transport description and
// mess up DTLS identity information, ICE credential, etc.
bool PeerConnection::ReadyToUseRemoteCandidate(
    const IceCandidateInterface* candidate,
    const SessionDescriptionInterface* remote_desc,
    bool* valid) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  *valid = true;

  const SessionDescriptionInterface* current_remote_desc =
      remote_desc ? remote_desc : remote_description();

  if (!current_remote_desc) {
    return false;
  }

  RTCErrorOr<const cricket::ContentInfo*> result =
      FindContentInfo(current_remote_desc, candidate);
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "ReadyToUseRemoteCandidate: Invalid candidate. "
                      << result.error().message();

    *valid = false;
    return false;
  }

  std::string transport_name = GetTransportName(result.value()->name);
  return !transport_name.empty();
}

bool PeerConnection::SrtpRequired() const {
  return (dtls_enabled_ ||
          sdp_handler_.webrtc_session_desc_factory()->SdesPolicy() ==
              cricket::SEC_REQUIRED);
}

void PeerConnection::OnTransportControllerGatheringState(
    cricket::IceGatheringState state) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (state == cricket::kIceGatheringGathering) {
    OnIceGatheringChange(PeerConnectionInterface::kIceGatheringGathering);
  } else if (state == cricket::kIceGatheringComplete) {
    OnIceGatheringChange(PeerConnectionInterface::kIceGatheringComplete);
  } else if (state == cricket::kIceGatheringNew) {
    OnIceGatheringChange(PeerConnectionInterface::kIceGatheringNew);
  } else {
    RTC_LOG(LS_ERROR) << "Unknown state received: " << state;
    RTC_NOTREACHED();
  }
}

void PeerConnection::ReportTransportStats() {
  std::map<std::string, std::set<cricket::MediaType>>
      media_types_by_transport_name;
  for (const auto& transceiver : transceivers_.List()) {
    if (transceiver->internal()->channel()) {
      const std::string& transport_name =
          transceiver->internal()->channel()->transport_name();
      media_types_by_transport_name[transport_name].insert(
          transceiver->media_type());
    }
  }
  if (rtp_data_channel()) {
    media_types_by_transport_name[rtp_data_channel()->transport_name()].insert(
        cricket::MEDIA_TYPE_DATA);
  }

  absl::optional<std::string> transport_name = sctp_transport_name();
  if (transport_name) {
    media_types_by_transport_name[*transport_name].insert(
        cricket::MEDIA_TYPE_DATA);
  }

  for (const auto& entry : media_types_by_transport_name) {
    const std::string& transport_name = entry.first;
    const std::set<cricket::MediaType> media_types = entry.second;
    cricket::TransportStats stats;
    if (transport_controller_->GetStats(transport_name, &stats)) {
      ReportBestConnectionState(stats);
      ReportNegotiatedCiphers(stats, media_types);
    }
  }
}
// Walk through the ConnectionInfos to gather best connection usage
// for IPv4 and IPv6.
void PeerConnection::ReportBestConnectionState(
    const cricket::TransportStats& stats) {
  for (const cricket::TransportChannelStats& channel_stats :
       stats.channel_stats) {
    for (const cricket::ConnectionInfo& connection_info :
         channel_stats.ice_transport_stats.connection_infos) {
      if (!connection_info.best_connection) {
        continue;
      }

      const cricket::Candidate& local = connection_info.local_candidate;
      const cricket::Candidate& remote = connection_info.remote_candidate;

      // Increment the counter for IceCandidatePairType.
      if (local.protocol() == cricket::TCP_PROTOCOL_NAME ||
          (local.type() == RELAY_PORT_TYPE &&
           local.relay_protocol() == cricket::TCP_PROTOCOL_NAME)) {
        RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.CandidatePairType_TCP",
                                  GetIceCandidatePairCounter(local, remote),
                                  kIceCandidatePairMax);
      } else if (local.protocol() == cricket::UDP_PROTOCOL_NAME) {
        RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.CandidatePairType_UDP",
                                  GetIceCandidatePairCounter(local, remote),
                                  kIceCandidatePairMax);
      } else {
        RTC_CHECK(0);
      }

      // Increment the counter for IP type.
      if (local.address().family() == AF_INET) {
        RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.IPMetrics",
                                  kBestConnections_IPv4,
                                  kPeerConnectionAddressFamilyCounter_Max);
      } else if (local.address().family() == AF_INET6) {
        RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.IPMetrics",
                                  kBestConnections_IPv6,
                                  kPeerConnectionAddressFamilyCounter_Max);
      } else {
        RTC_CHECK(!local.address().hostname().empty() &&
                  local.address().IsUnresolvedIP());
      }

      return;
    }
  }
}

void PeerConnection::ReportNegotiatedCiphers(
    const cricket::TransportStats& stats,
    const std::set<cricket::MediaType>& media_types) {
  if (!dtls_enabled_ || stats.channel_stats.empty()) {
    return;
  }

  int srtp_crypto_suite = stats.channel_stats[0].srtp_crypto_suite;
  int ssl_cipher_suite = stats.channel_stats[0].ssl_cipher_suite;
  if (srtp_crypto_suite == rtc::SRTP_INVALID_CRYPTO_SUITE &&
      ssl_cipher_suite == rtc::TLS_NULL_WITH_NULL_NULL) {
    return;
  }

  if (srtp_crypto_suite != rtc::SRTP_INVALID_CRYPTO_SUITE) {
    for (cricket::MediaType media_type : media_types) {
      switch (media_type) {
        case cricket::MEDIA_TYPE_AUDIO:
          RTC_HISTOGRAM_ENUMERATION_SPARSE(
              "WebRTC.PeerConnection.SrtpCryptoSuite.Audio", srtp_crypto_suite,
              rtc::SRTP_CRYPTO_SUITE_MAX_VALUE);
          break;
        case cricket::MEDIA_TYPE_VIDEO:
          RTC_HISTOGRAM_ENUMERATION_SPARSE(
              "WebRTC.PeerConnection.SrtpCryptoSuite.Video", srtp_crypto_suite,
              rtc::SRTP_CRYPTO_SUITE_MAX_VALUE);
          break;
        case cricket::MEDIA_TYPE_DATA:
          RTC_HISTOGRAM_ENUMERATION_SPARSE(
              "WebRTC.PeerConnection.SrtpCryptoSuite.Data", srtp_crypto_suite,
              rtc::SRTP_CRYPTO_SUITE_MAX_VALUE);
          break;
        default:
          RTC_NOTREACHED();
          continue;
      }
    }
  }

  if (ssl_cipher_suite != rtc::TLS_NULL_WITH_NULL_NULL) {
    for (cricket::MediaType media_type : media_types) {
      switch (media_type) {
        case cricket::MEDIA_TYPE_AUDIO:
          RTC_HISTOGRAM_ENUMERATION_SPARSE(
              "WebRTC.PeerConnection.SslCipherSuite.Audio", ssl_cipher_suite,
              rtc::SSL_CIPHER_SUITE_MAX_VALUE);
          break;
        case cricket::MEDIA_TYPE_VIDEO:
          RTC_HISTOGRAM_ENUMERATION_SPARSE(
              "WebRTC.PeerConnection.SslCipherSuite.Video", ssl_cipher_suite,
              rtc::SSL_CIPHER_SUITE_MAX_VALUE);
          break;
        case cricket::MEDIA_TYPE_DATA:
          RTC_HISTOGRAM_ENUMERATION_SPARSE(
              "WebRTC.PeerConnection.SslCipherSuite.Data", ssl_cipher_suite,
              rtc::SSL_CIPHER_SUITE_MAX_VALUE);
          break;
        default:
          RTC_NOTREACHED();
          continue;
      }
    }
  }
}

void PeerConnection::OnSentPacket_w(const rtc::SentPacket& sent_packet) {
  RTC_DCHECK_RUN_ON(worker_thread());
  RTC_DCHECK(call_);
  call_->OnSentPacket(sent_packet);
}

const std::string PeerConnection::GetTransportName(
    const std::string& content_name) {
  cricket::ChannelInterface* channel = GetChannel(content_name);
  if (channel) {
    return channel->transport_name();
  }
  if (data_channel_controller_.data_channel_transport()) {
    RTC_DCHECK(sctp_mid_s_);
    if (content_name == *sctp_mid_s_) {
      return *sctp_transport_name();
    }
  }
  // Return an empty string if failed to retrieve the transport name.
  return "";
}

void PeerConnection::DestroyTransceiverChannel(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver) {
  RTC_DCHECK(transceiver);

  cricket::ChannelInterface* channel = transceiver->internal()->channel();
  if (channel) {
    transceiver->internal()->SetChannel(nullptr);
    DestroyChannelInterface(channel);
  }
}

void PeerConnection::DestroyDataChannelTransport() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (data_channel_controller_.rtp_data_channel()) {
    data_channel_controller_.OnTransportChannelClosed();
    DestroyChannelInterface(data_channel_controller_.rtp_data_channel());
    data_channel_controller_.set_rtp_data_channel(nullptr);
  }

  // Note: Cannot use rtc::Bind to create a functor to invoke because it will
  // grab a reference to this PeerConnection. If this is called from the
  // PeerConnection destructor, the RefCountedObject vtable will have already
  // been destroyed (since it is a subclass of PeerConnection) and using
  // rtc::Bind will cause "Pure virtual function called" error to appear.

  if (sctp_mid_s_) {
    data_channel_controller_.OnTransportChannelClosed();
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
      RTC_DCHECK_RUN_ON(network_thread());
      TeardownDataChannelTransport_n();
    });
    sctp_mid_s_.reset();
  }
}

void PeerConnection::DestroyChannelInterface(
    cricket::ChannelInterface* channel) {
  // TODO(bugs.webrtc.org/11992): All the below methods should be called on the
  // worker thread. (they switch internally anyway). Change
  // DestroyChannelInterface to either be called on the worker thread, or do
  // this asynchronously on the worker.
  RTC_DCHECK(channel);
  switch (channel->media_type()) {
    case cricket::MEDIA_TYPE_AUDIO:
      channel_manager()->DestroyVoiceChannel(
          static_cast<cricket::VoiceChannel*>(channel));
      break;
    case cricket::MEDIA_TYPE_VIDEO:
      channel_manager()->DestroyVideoChannel(
          static_cast<cricket::VideoChannel*>(channel));
      break;
    case cricket::MEDIA_TYPE_DATA:
      channel_manager()->DestroyRtpDataChannel(
          static_cast<cricket::RtpDataChannel*>(channel));
      break;
    default:
      RTC_NOTREACHED() << "Unknown media type: " << channel->media_type();
      break;
  }
}

bool PeerConnection::OnTransportChanged(
    const std::string& mid,
    RtpTransportInternal* rtp_transport,
    rtc::scoped_refptr<DtlsTransport> dtls_transport,
    DataChannelTransportInterface* data_channel_transport) {
  RTC_DCHECK_RUN_ON(network_thread());
  bool ret = true;
  auto base_channel = GetChannel(mid);
  if (base_channel) {
    ret = base_channel->SetRtpTransport(rtp_transport);
  }
  if (mid == sctp_mid_n_) {
    data_channel_controller_.OnTransportChanged(data_channel_transport);
  }
  return ret;
}

void PeerConnection::OnSetStreams() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (IsUnifiedPlan())
    sdp_handler_.UpdateNegotiationNeeded();
}

PeerConnectionObserver* PeerConnection::Observer() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(observer_);
  return observer_;
}

CryptoOptions PeerConnection::GetCryptoOptions() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // TODO(bugs.webrtc.org/9891) - Remove PeerConnectionFactory::CryptoOptions
  // after it has been removed.
  return configuration_.crypto_options.has_value()
             ? *configuration_.crypto_options
             : factory_->options().crypto_options;
}

void PeerConnection::ClearStatsCache() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (stats_collector_) {
    stats_collector_->ClearCachedStatsReport();
  }
}

void PeerConnection::RequestUsagePatternReportForTesting() {
  signaling_thread()->Post(RTC_FROM_HERE, this, MSG_REPORT_USAGE_PATTERN,
                           nullptr);
}

bool PeerConnection::ShouldFireNegotiationNeededEvent(uint32_t event_id) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return sdp_handler_.ShouldFireNegotiationNeededEvent(event_id);
}

std::function<void(const rtc::CopyOnWriteBuffer& packet,
                   int64_t packet_time_us)>
PeerConnection::InitializeRtcpCallback() {
  RTC_DCHECK_RUN_ON(signaling_thread());

  auto flag =
      worker_thread()->Invoke<rtc::scoped_refptr<PendingTaskSafetyFlag>>(
          RTC_FROM_HERE, [this] {
            RTC_DCHECK_RUN_ON(worker_thread());
            if (!call_)
              return rtc::scoped_refptr<PendingTaskSafetyFlag>();
            if (!call_safety_)
              call_safety_.reset(new ScopedTaskSafety());
            return call_safety_->flag();
          });

  if (!flag)
    return [](const rtc::CopyOnWriteBuffer&, int64_t) {};

  return [this, flag = std::move(flag)](const rtc::CopyOnWriteBuffer& packet,
                                        int64_t packet_time_us) {
    RTC_DCHECK_RUN_ON(network_thread());
    // TODO(bugs.webrtc.org/11993): We should actually be delivering this call
    // directly to the Call class somehow directly on the network thread and not
    // incur this hop here. The DeliverPacket() method will eventually just have
    // to hop back over to the network thread.
    worker_thread()->PostTask(ToQueuedTask(flag, [this, packet,
                                                  packet_time_us] {
      RTC_DCHECK_RUN_ON(worker_thread());
      call_->Receiver()->DeliverPacket(MediaType::ANY, packet, packet_time_us);
    }));
  };
}

}  // namespace webrtc
