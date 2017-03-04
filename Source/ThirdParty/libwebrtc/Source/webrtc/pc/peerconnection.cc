/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/peerconnection.h"

#include <algorithm>
#include <cctype>  // for isdigit
#include <utility>
#include <vector>

#include "webrtc/api/jsepicecandidate.h"
#include "webrtc/api/jsepsessiondescription.h"
#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/api/mediastreamproxy.h"
#include "webrtc/api/mediastreamtrackproxy.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/call/call.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/media/sctp/sctptransport.h"
#include "webrtc/pc/audiotrack.h"
#include "webrtc/pc/channelmanager.h"
#include "webrtc/pc/dtmfsender.h"
#include "webrtc/pc/mediastream.h"
#include "webrtc/pc/mediastreamobserver.h"
#include "webrtc/pc/remoteaudiosource.h"
#include "webrtc/pc/rtpreceiver.h"
#include "webrtc/pc/rtpsender.h"
#include "webrtc/pc/streamcollection.h"
#include "webrtc/pc/videocapturertracksource.h"
#include "webrtc/pc/videotrack.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace {

using webrtc::DataChannel;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;
using webrtc::RTCError;
using webrtc::RTCErrorType;
using webrtc::RtpSenderInternal;
using webrtc::RtpSenderInterface;
using webrtc::RtpSenderProxy;
using webrtc::RtpSenderProxyWithInternal;
using webrtc::StreamCollection;

static const char kDefaultStreamLabel[] = "default";
static const char kDefaultAudioTrackLabel[] = "defaulta0";
static const char kDefaultVideoTrackLabel[] = "defaultv0";

// The min number of tokens must present in Turn host uri.
// e.g. user@turn.example.org
static const size_t kTurnHostTokensNum = 2;
// Number of tokens must be preset when TURN uri has transport param.
static const size_t kTurnTransportTokensNum = 2;
// The default stun port.
static const int kDefaultStunPort = 3478;
static const int kDefaultStunTlsPort = 5349;
static const char kTransport[] = "transport";

// NOTE: Must be in the same order as the ServiceType enum.
static const char* kValidIceServiceTypes[] = {"stun", "stuns", "turn", "turns"};

// The length of RTCP CNAMEs.
static const int kRtcpCnameLength = 16;

// NOTE: A loop below assumes that the first value of this enum is 0 and all
// other values are incremental.
enum ServiceType {
  STUN = 0,  // Indicates a STUN server.
  STUNS,     // Indicates a STUN server used with a TLS session.
  TURN,      // Indicates a TURN server
  TURNS,     // Indicates a TURN server used with a TLS session.
  INVALID,   // Unknown.
};
static_assert(INVALID == arraysize(kValidIceServiceTypes),
              "kValidIceServiceTypes must have as many strings as ServiceType "
              "has values.");

enum {
  MSG_SET_SESSIONDESCRIPTION_SUCCESS = 0,
  MSG_SET_SESSIONDESCRIPTION_FAILED,
  MSG_CREATE_SESSIONDESCRIPTION_FAILED,
  MSG_GETSTATS,
  MSG_FREE_DATACHANNELS,
};

struct SetSessionDescriptionMsg : public rtc::MessageData {
  explicit SetSessionDescriptionMsg(
      webrtc::SetSessionDescriptionObserver* observer)
      : observer(observer) {
  }

  rtc::scoped_refptr<webrtc::SetSessionDescriptionObserver> observer;
  std::string error;
};

struct CreateSessionDescriptionMsg : public rtc::MessageData {
  explicit CreateSessionDescriptionMsg(
      webrtc::CreateSessionDescriptionObserver* observer)
      : observer(observer) {}

  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserver> observer;
  std::string error;
};

struct GetStatsMsg : public rtc::MessageData {
  GetStatsMsg(webrtc::StatsObserver* observer,
              webrtc::MediaStreamTrackInterface* track)
      : observer(observer), track(track) {
  }
  rtc::scoped_refptr<webrtc::StatsObserver> observer;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track;
};

// |in_str| should be of format
// stunURI       = scheme ":" stun-host [ ":" stun-port ]
// scheme        = "stun" / "stuns"
// stun-host     = IP-literal / IPv4address / reg-name
// stun-port     = *DIGIT
//
// draft-petithuguenin-behave-turn-uris-01
// turnURI       = scheme ":" turn-host [ ":" turn-port ]
// turn-host     = username@IP-literal / IPv4address / reg-name
bool GetServiceTypeAndHostnameFromUri(const std::string& in_str,
                                      ServiceType* service_type,
                                      std::string* hostname) {
  const std::string::size_type colonpos = in_str.find(':');
  if (colonpos == std::string::npos) {
    LOG(LS_WARNING) << "Missing ':' in ICE URI: " << in_str;
    return false;
  }
  if ((colonpos + 1) == in_str.length()) {
    LOG(LS_WARNING) << "Empty hostname in ICE URI: " << in_str;
    return false;
  }
  *service_type = INVALID;
  for (size_t i = 0; i < arraysize(kValidIceServiceTypes); ++i) {
    if (in_str.compare(0, colonpos, kValidIceServiceTypes[i]) == 0) {
      *service_type = static_cast<ServiceType>(i);
      break;
    }
  }
  if (*service_type == INVALID) {
    return false;
  }
  *hostname = in_str.substr(colonpos + 1, std::string::npos);
  return true;
}

bool ParsePort(const std::string& in_str, int* port) {
  // Make sure port only contains digits. FromString doesn't check this.
  for (const char& c : in_str) {
    if (!std::isdigit(c)) {
      return false;
    }
  }
  return rtc::FromString(in_str, port);
}

// This method parses IPv6 and IPv4 literal strings, along with hostnames in
// standard hostname:port format.
// Consider following formats as correct.
// |hostname:port|, |[IPV6 address]:port|, |IPv4 address|:port,
// |hostname|, |[IPv6 address]|, |IPv4 address|.
bool ParseHostnameAndPortFromString(const std::string& in_str,
                                    std::string* host,
                                    int* port) {
  RTC_DCHECK(host->empty());
  if (in_str.at(0) == '[') {
    std::string::size_type closebracket = in_str.rfind(']');
    if (closebracket != std::string::npos) {
      std::string::size_type colonpos = in_str.find(':', closebracket);
      if (std::string::npos != colonpos) {
        if (!ParsePort(in_str.substr(closebracket + 2, std::string::npos),
                       port)) {
          return false;
        }
      }
      *host = in_str.substr(1, closebracket - 1);
    } else {
      return false;
    }
  } else {
    std::string::size_type colonpos = in_str.find(':');
    if (std::string::npos != colonpos) {
      if (!ParsePort(in_str.substr(colonpos + 1, std::string::npos), port)) {
        return false;
      }
      *host = in_str.substr(0, colonpos);
    } else {
      *host = in_str;
    }
  }
  return !host->empty();
}

// Adds a STUN or TURN server to the appropriate list,
// by parsing |url| and using the username/password in |server|.
RTCErrorType ParseIceServerUrl(
    const PeerConnectionInterface::IceServer& server,
    const std::string& url,
    cricket::ServerAddresses* stun_servers,
    std::vector<cricket::RelayServerConfig>* turn_servers) {
  // draft-nandakumar-rtcweb-stun-uri-01
  // stunURI       = scheme ":" stun-host [ ":" stun-port ]
  // scheme        = "stun" / "stuns"
  // stun-host     = IP-literal / IPv4address / reg-name
  // stun-port     = *DIGIT

  // draft-petithuguenin-behave-turn-uris-01
  // turnURI       = scheme ":" turn-host [ ":" turn-port ]
  //                 [ "?transport=" transport ]
  // scheme        = "turn" / "turns"
  // transport     = "udp" / "tcp" / transport-ext
  // transport-ext = 1*unreserved
  // turn-host     = IP-literal / IPv4address / reg-name
  // turn-port     = *DIGIT
  RTC_DCHECK(stun_servers != nullptr);
  RTC_DCHECK(turn_servers != nullptr);
  std::vector<std::string> tokens;
  cricket::ProtocolType turn_transport_type = cricket::PROTO_UDP;
  RTC_DCHECK(!url.empty());
  rtc::tokenize_with_empty_tokens(url, '?', &tokens);
  std::string uri_without_transport = tokens[0];
  // Let's look into transport= param, if it exists.
  if (tokens.size() == kTurnTransportTokensNum) {  // ?transport= is present.
    std::string uri_transport_param = tokens[1];
    rtc::tokenize_with_empty_tokens(uri_transport_param, '=', &tokens);
    if (tokens[0] != kTransport) {
      LOG(LS_WARNING) << "Invalid transport parameter key.";
      return RTCErrorType::SYNTAX_ERROR;
    }
    if (tokens.size() < 2) {
      LOG(LS_WARNING) << "Transport parameter missing value.";
      return RTCErrorType::SYNTAX_ERROR;
    }
    if (!cricket::StringToProto(tokens[1].c_str(), &turn_transport_type) ||
        (turn_transport_type != cricket::PROTO_UDP &&
         turn_transport_type != cricket::PROTO_TCP)) {
      LOG(LS_WARNING) << "Transport parameter should always be udp or tcp.";
      return RTCErrorType::SYNTAX_ERROR;
    }
  }

  std::string hoststring;
  ServiceType service_type;
  if (!GetServiceTypeAndHostnameFromUri(uri_without_transport,
                                       &service_type,
                                       &hoststring)) {
    LOG(LS_WARNING) << "Invalid transport parameter in ICE URI: " << url;
    return RTCErrorType::SYNTAX_ERROR;
  }

  // GetServiceTypeAndHostnameFromUri should never give an empty hoststring
  RTC_DCHECK(!hoststring.empty());

  // Let's break hostname.
  tokens.clear();
  rtc::tokenize_with_empty_tokens(hoststring, '@', &tokens);

  std::string username(server.username);
  if (tokens.size() > kTurnHostTokensNum) {
    LOG(LS_WARNING) << "Invalid user@hostname format: " << hoststring;
    return RTCErrorType::SYNTAX_ERROR;
  }
  if (tokens.size() == kTurnHostTokensNum) {
    if (tokens[0].empty() || tokens[1].empty()) {
      LOG(LS_WARNING) << "Invalid user@hostname format: " << hoststring;
      return RTCErrorType::SYNTAX_ERROR;
    }
    username.assign(rtc::s_url_decode(tokens[0]));
    hoststring = tokens[1];
  } else {
    hoststring = tokens[0];
  }

  int port = kDefaultStunPort;
  if (service_type == TURNS) {
    port = kDefaultStunTlsPort;
    turn_transport_type = cricket::PROTO_TLS;
  }

  std::string address;
  if (!ParseHostnameAndPortFromString(hoststring, &address, &port)) {
    LOG(WARNING) << "Invalid hostname format: " << uri_without_transport;
    return RTCErrorType::SYNTAX_ERROR;
  }

  if (port <= 0 || port > 0xffff) {
    LOG(WARNING) << "Invalid port: " << port;
    return RTCErrorType::SYNTAX_ERROR;
  }

  switch (service_type) {
    case STUN:
    case STUNS:
      stun_servers->insert(rtc::SocketAddress(address, port));
      break;
    case TURN:
    case TURNS: {
      if (username.empty() || server.password.empty()) {
        // The WebRTC spec requires throwing an InvalidAccessError when username
        // or credential are ommitted; this is the native equivalent.
        return RTCErrorType::INVALID_PARAMETER;
      }
      cricket::RelayServerConfig config = cricket::RelayServerConfig(
          address, port, username, server.password, turn_transport_type);
      if (server.tls_cert_policy ==
          PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck) {
        config.tls_cert_policy =
            cricket::TlsCertPolicy::TLS_CERT_POLICY_INSECURE_NO_CHECK;
      }
      turn_servers->push_back(config);
      break;
    }
    default:
      // We shouldn't get to this point with an invalid service_type, we should
      // have returned an error already.
      RTC_NOTREACHED() << "Unexpected service type";
      return RTCErrorType::INTERNAL_ERROR;
  }
  return RTCErrorType::NONE;
}

// Check if we can send |new_stream| on a PeerConnection.
bool CanAddLocalMediaStream(webrtc::StreamCollectionInterface* current_streams,
                            webrtc::MediaStreamInterface* new_stream) {
  if (!new_stream || !current_streams) {
    return false;
  }
  if (current_streams->find(new_stream->label()) != nullptr) {
    LOG(LS_ERROR) << "MediaStream with label " << new_stream->label()
                  << " is already added.";
    return false;
  }
  return true;
}

bool MediaContentDirectionHasSend(cricket::MediaContentDirection dir) {
  return dir == cricket::MD_SENDONLY || dir == cricket::MD_SENDRECV;
}

// If the direction is "recvonly" or "inactive", treat the description
// as containing no streams.
// See: https://code.google.com/p/webrtc/issues/detail?id=5054
std::vector<cricket::StreamParams> GetActiveStreams(
    const cricket::MediaContentDescription* desc) {
  return MediaContentDirectionHasSend(desc->direction())
             ? desc->streams()
             : std::vector<cricket::StreamParams>();
}

bool IsValidOfferToReceiveMedia(int value) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions Options;
  return (value >= Options::kUndefined) &&
         (value <= Options::kMaxOfferToReceiveMedia);
}

// Add the stream and RTP data channel info to |session_options|.
void AddSendStreams(
    cricket::MediaSessionOptions* session_options,
    const std::vector<rtc::scoped_refptr<
        RtpSenderProxyWithInternal<RtpSenderInternal>>>& senders,
    const std::map<std::string, rtc::scoped_refptr<DataChannel>>&
        rtp_data_channels) {
  session_options->streams.clear();
  for (const auto& sender : senders) {
    session_options->AddSendStream(sender->media_type(), sender->id(),
                                   sender->internal()->stream_id());
  }

  // Check for data channels.
  for (const auto& kv : rtp_data_channels) {
    const DataChannel* channel = kv.second;
    if (channel->state() == DataChannel::kConnecting ||
        channel->state() == DataChannel::kOpen) {
      // |streamid| and |sync_label| are both set to the DataChannel label
      // here so they can be signaled the same way as MediaStreams and Tracks.
      // For MediaStreams, the sync_label is the MediaStream label and the
      // track label is the same as |streamid|.
      const std::string& streamid = channel->label();
      const std::string& sync_label = channel->label();
      session_options->AddSendStream(cricket::MEDIA_TYPE_DATA, streamid,
                                     sync_label);
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

// Helper method to set a voice/video channel on all applicable senders
// and receivers when one is created/destroyed by WebRtcSession.
//
// Used by On(Voice|Video)Channel(Created|Destroyed)
template <class SENDER,
          class RECEIVER,
          class CHANNEL,
          class SENDERS,
          class RECEIVERS>
void SetChannelOnSendersAndReceivers(CHANNEL* channel,
                                     SENDERS& senders,
                                     RECEIVERS& receivers,
                                     cricket::MediaType media_type) {
  for (auto& sender : senders) {
    if (sender->media_type() == media_type) {
      static_cast<SENDER*>(sender->internal())->SetChannel(channel);
    }
  }
  for (auto& receiver : receivers) {
    if (receiver->media_type() == media_type) {
      if (!channel) {
        receiver->internal()->Stop();
      }
      static_cast<RECEIVER*>(receiver->internal())->SetChannel(channel);
    }
  }
}

// Helper to set an error and return from a method.
bool SafeSetError(webrtc::RTCErrorType type, webrtc::RTCError* error) {
  if (error) {
    error->set_type(type);
  }
  return type == webrtc::RTCErrorType::NONE;
}

}  // namespace

namespace webrtc {

bool PeerConnectionInterface::RTCConfiguration::operator==(
    const PeerConnectionInterface::RTCConfiguration& o) const {
  // This static_assert prevents us from accidentally breaking operator==.
  struct stuff_being_tested_for_equality {
    IceTransportsType type;
    IceServers servers;
    BundlePolicy bundle_policy;
    RtcpMuxPolicy rtcp_mux_policy;
    TcpCandidatePolicy tcp_candidate_policy;
    CandidateNetworkPolicy candidate_network_policy;
    int audio_jitter_buffer_max_packets;
    bool audio_jitter_buffer_fast_accelerate;
    int ice_connection_receiving_timeout;
    int ice_backup_candidate_pair_ping_interval;
    ContinualGatheringPolicy continual_gathering_policy;
    std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates;
    bool prioritize_most_likely_ice_candidate_pairs;
    struct cricket::MediaConfig media_config;
    bool disable_ipv6;
    bool enable_rtp_data_channel;
    bool enable_quic;
    rtc::Optional<int> screencast_min_bitrate;
    rtc::Optional<bool> combined_audio_video_bwe;
    rtc::Optional<bool> enable_dtls_srtp;
    int ice_candidate_pool_size;
    bool prune_turn_ports;
    bool presume_writable_when_fully_relayed;
    bool enable_ice_renomination;
    bool redetermine_role_on_ice_restart;
    rtc::Optional<int> ice_check_min_interval;
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
         ice_connection_receiving_timeout ==
             o.ice_connection_receiving_timeout &&
         ice_backup_candidate_pair_ping_interval ==
             o.ice_backup_candidate_pair_ping_interval &&
         continual_gathering_policy == o.continual_gathering_policy &&
         certificates == o.certificates &&
         prioritize_most_likely_ice_candidate_pairs ==
             o.prioritize_most_likely_ice_candidate_pairs &&
         media_config == o.media_config && disable_ipv6 == o.disable_ipv6 &&
         enable_rtp_data_channel == o.enable_rtp_data_channel &&
         enable_quic == o.enable_quic &&
         screencast_min_bitrate == o.screencast_min_bitrate &&
         combined_audio_video_bwe == o.combined_audio_video_bwe &&
         enable_dtls_srtp == o.enable_dtls_srtp &&
         ice_candidate_pool_size == o.ice_candidate_pool_size &&
         prune_turn_ports == o.prune_turn_ports &&
         presume_writable_when_fully_relayed ==
             o.presume_writable_when_fully_relayed &&
         enable_ice_renomination == o.enable_ice_renomination &&
         redetermine_role_on_ice_restart == o.redetermine_role_on_ice_restart &&
         ice_check_min_interval == o.ice_check_min_interval;
}

bool PeerConnectionInterface::RTCConfiguration::operator!=(
    const PeerConnectionInterface::RTCConfiguration& o) const {
  return !(*this == o);
}

// Generate a RTCP CNAME when a PeerConnection is created.
std::string GenerateRtcpCname() {
  std::string cname;
  if (!rtc::CreateRandomString(kRtcpCnameLength, &cname)) {
    LOG(LS_ERROR) << "Failed to generate CNAME.";
    RTC_NOTREACHED();
  }
  return cname;
}

bool ExtractMediaSessionOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    bool is_offer,
    cricket::MediaSessionOptions* session_options) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions RTCOfferAnswerOptions;
  if (!IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_audio) ||
      !IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_video)) {
    return false;
  }

  // If constraints don't prevent us, we always accept video.
  if (rtc_options.offer_to_receive_audio != RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_audio = (rtc_options.offer_to_receive_audio > 0);
  } else {
    session_options->recv_audio = true;
  }
  // For offers, we only offer video if we have it or it's forced by options.
  // For answers, we will always accept video (if offered).
  if (rtc_options.offer_to_receive_video != RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_video = (rtc_options.offer_to_receive_video > 0);
  } else if (is_offer) {
    session_options->recv_video = false;
  } else {
    session_options->recv_video = true;
  }

  session_options->vad_enabled = rtc_options.voice_activity_detection;
  session_options->bundle_enabled = rtc_options.use_rtp_mux;
  for (auto& kv : session_options->transport_options) {
    kv.second.ice_restart = rtc_options.ice_restart;
  }

  return true;
}

bool ParseConstraintsForAnswer(const MediaConstraintsInterface* constraints,
                               cricket::MediaSessionOptions* session_options) {
  bool value = false;
  size_t mandatory_constraints_satisfied = 0;

  // kOfferToReceiveAudio defaults to true according to spec.
  if (!FindConstraint(constraints,
                      MediaConstraintsInterface::kOfferToReceiveAudio, &value,
                      &mandatory_constraints_satisfied) ||
      value) {
    session_options->recv_audio = true;
  }

  // kOfferToReceiveVideo defaults to false according to spec. But
  // if it is an answer and video is offered, we should still accept video
  // per default.
  value = false;
  if (!FindConstraint(constraints,
                      MediaConstraintsInterface::kOfferToReceiveVideo, &value,
                      &mandatory_constraints_satisfied) ||
      value) {
    session_options->recv_video = true;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kVoiceActivityDetection, &value,
                     &mandatory_constraints_satisfied)) {
    session_options->vad_enabled = value;
  }

  if (FindConstraint(constraints, MediaConstraintsInterface::kUseRtpMux, &value,
                     &mandatory_constraints_satisfied)) {
    session_options->bundle_enabled = value;
  } else {
    // kUseRtpMux defaults to true according to spec.
    session_options->bundle_enabled = true;
  }

  bool ice_restart = false;
  if (FindConstraint(constraints, MediaConstraintsInterface::kIceRestart,
                     &value, &mandatory_constraints_satisfied)) {
    // kIceRestart defaults to false according to spec.
    ice_restart = true;
  }
  for (auto& kv : session_options->transport_options) {
    kv.second.ice_restart = ice_restart;
  }

  if (!constraints) {
    return true;
  }
  return mandatory_constraints_satisfied == constraints->GetMandatory().size();
}

RTCErrorType ParseIceServers(
    const PeerConnectionInterface::IceServers& servers,
    cricket::ServerAddresses* stun_servers,
    std::vector<cricket::RelayServerConfig>* turn_servers) {
  for (const webrtc::PeerConnectionInterface::IceServer& server : servers) {
    if (!server.urls.empty()) {
      for (const std::string& url : server.urls) {
        if (url.empty()) {
          LOG(LS_ERROR) << "Empty uri.";
          return RTCErrorType::SYNTAX_ERROR;
        }
        RTCErrorType err =
            ParseIceServerUrl(server, url, stun_servers, turn_servers);
        if (err != RTCErrorType::NONE) {
          return err;
        }
      }
    } else if (!server.uri.empty()) {
      // Fallback to old .uri if new .urls isn't present.
      RTCErrorType err =
          ParseIceServerUrl(server, server.uri, stun_servers, turn_servers);
      if (err != RTCErrorType::NONE) {
        return err;
      }
    } else {
      LOG(LS_ERROR) << "Empty uri.";
      return RTCErrorType::SYNTAX_ERROR;
    }
  }
  // Candidates must have unique priorities, so that connectivity checks
  // are performed in a well-defined order.
  int priority = static_cast<int>(turn_servers->size() - 1);
  for (cricket::RelayServerConfig& turn_server : *turn_servers) {
    // First in the list gets highest priority.
    turn_server.priority = priority--;
  }
  return RTCErrorType::NONE;
}

PeerConnection::PeerConnection(PeerConnectionFactory* factory)
    : factory_(factory),
      observer_(NULL),
      uma_observer_(NULL),
      signaling_state_(kStable),
      ice_connection_state_(kIceConnectionNew),
      ice_gathering_state_(kIceGatheringNew),
      event_log_(RtcEventLog::Create()),
      rtcp_cname_(GenerateRtcpCname()),
      local_streams_(StreamCollection::Create()),
      remote_streams_(StreamCollection::Create()) {}

PeerConnection::~PeerConnection() {
  TRACE_EVENT0("webrtc", "PeerConnection::~PeerConnection");
  RTC_DCHECK(signaling_thread()->IsCurrent());
  // Need to detach RTP senders/receivers from WebRtcSession,
  // since it's about to be destroyed.
  for (const auto& sender : senders_) {
    sender->internal()->Stop();
  }
  for (const auto& receiver : receivers_) {
    receiver->internal()->Stop();
  }
  // Destroy stats_ because it depends on session_.
  stats_.reset(nullptr);
  if (stats_collector_) {
    stats_collector_->WaitForPendingRequest();
    stats_collector_ = nullptr;
  }
  // Now destroy session_ before destroying other members,
  // because its destruction fires signals (such as VoiceChannelDestroyed)
  // which will trigger some final actions in PeerConnection...
  session_.reset(nullptr);
  // port_allocator_ lives on the network thread and should be destroyed there.
  network_thread()->Invoke<void>(RTC_FROM_HERE,
                                 [this] { port_allocator_.reset(nullptr); });
}

bool PeerConnection::Initialize(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    std::unique_ptr<cricket::PortAllocator> allocator,
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
    PeerConnectionObserver* observer) {
  TRACE_EVENT0("webrtc", "PeerConnection::Initialize");
  if (!allocator) {
    LOG(LS_ERROR) << "PeerConnection initialized without a PortAllocator? "
                  << "This shouldn't happen if using PeerConnectionFactory.";
    return false;
  }
  if (!observer) {
    // TODO(deadbeef): Why do we do this?
    LOG(LS_ERROR) << "PeerConnection initialized without a "
                  << "PeerConnectionObserver";
    return false;
  }
  observer_ = observer;
  port_allocator_ = std::move(allocator);

  // The port allocator lives on the network thread and should be initialized
  // there.
  if (!network_thread()->Invoke<bool>(
          RTC_FROM_HERE, rtc::Bind(&PeerConnection::InitializePortAllocator_n,
                                   this, configuration))) {
    return false;
  }

  media_controller_.reset(factory_->CreateMediaController(
      configuration.media_config, event_log_.get()));

  session_.reset(new WebRtcSession(
      media_controller_.get(), factory_->network_thread(),
      factory_->worker_thread(), factory_->signaling_thread(),
      port_allocator_.get(),
      std::unique_ptr<cricket::TransportController>(
          factory_->CreateTransportController(
              port_allocator_.get(),
              configuration.redetermine_role_on_ice_restart)),
#ifdef HAVE_SCTP
      std::unique_ptr<cricket::SctpTransportInternalFactory>(
          new cricket::SctpTransportFactory(factory_->network_thread()))
#else
      nullptr
#endif
          ));

  stats_.reset(new StatsCollector(this));
  stats_collector_ = RTCStatsCollector::Create(this);

  // Initialize the WebRtcSession. It creates transport channels etc.
  if (!session_->Initialize(factory_->options(), std::move(cert_generator),
                            configuration)) {
    return false;
  }

  // Register PeerConnection as receiver of local ice candidates.
  // All the callbacks will be posted to the application from PeerConnection.
  session_->RegisterIceObserver(this);
  session_->SignalState.connect(this, &PeerConnection::OnSessionStateChange);
  session_->SignalVoiceChannelCreated.connect(
      this, &PeerConnection::OnVoiceChannelCreated);
  session_->SignalVoiceChannelDestroyed.connect(
      this, &PeerConnection::OnVoiceChannelDestroyed);
  session_->SignalVideoChannelCreated.connect(
      this, &PeerConnection::OnVideoChannelCreated);
  session_->SignalVideoChannelDestroyed.connect(
      this, &PeerConnection::OnVideoChannelDestroyed);
  session_->SignalDataChannelCreated.connect(
      this, &PeerConnection::OnDataChannelCreated);
  session_->SignalDataChannelDestroyed.connect(
      this, &PeerConnection::OnDataChannelDestroyed);
  session_->SignalDataChannelOpenMessage.connect(
      this, &PeerConnection::OnDataChannelOpenMessage);

  configuration_ = configuration;
  return true;
}

rtc::scoped_refptr<StreamCollectionInterface>
PeerConnection::local_streams() {
  return local_streams_;
}

rtc::scoped_refptr<StreamCollectionInterface>
PeerConnection::remote_streams() {
  return remote_streams_;
}

bool PeerConnection::AddStream(MediaStreamInterface* local_stream) {
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
    OnAudioTrackAdded(track.get(), local_stream);
  }
  for (const auto& track : local_stream->GetVideoTracks()) {
    OnVideoTrackAdded(track.get(), local_stream);
  }

  stats_->AddStream(local_stream);
  observer_->OnRenegotiationNeeded();
  return true;
}

void PeerConnection::RemoveStream(MediaStreamInterface* local_stream) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveStream");
  for (const auto& track : local_stream->GetAudioTracks()) {
    OnAudioTrackRemoved(track.get(), local_stream);
  }
  for (const auto& track : local_stream->GetVideoTracks()) {
    OnVideoTrackRemoved(track.get(), local_stream);
  }

  local_streams_->RemoveStream(local_stream);
  stream_observers_.erase(
      std::remove_if(
          stream_observers_.begin(), stream_observers_.end(),
          [local_stream](const std::unique_ptr<MediaStreamObserver>& observer) {
            return observer->stream()->label().compare(local_stream->label()) ==
                   0;
          }),
      stream_observers_.end());

  if (IsClosed()) {
    return;
  }
  observer_->OnRenegotiationNeeded();
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnection::AddTrack(
    MediaStreamTrackInterface* track,
    std::vector<MediaStreamInterface*> streams) {
  TRACE_EVENT0("webrtc", "PeerConnection::AddTrack");
  if (IsClosed()) {
    return nullptr;
  }
  if (streams.size() >= 2) {
    LOG(LS_ERROR)
        << "Adding a track with two streams is not currently supported.";
    return nullptr;
  }
  // TODO(deadbeef): Support adding a track to two different senders.
  if (FindSenderForTrack(track) != senders_.end()) {
    LOG(LS_ERROR) << "Sender for track " << track->id() << " already exists.";
    return nullptr;
  }

  // TODO(deadbeef): Support adding a track to multiple streams.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender;
  if (track->kind() == MediaStreamTrackInterface::kAudioKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        new AudioRtpSender(static_cast<AudioTrackInterface*>(track),
                           session_->voice_channel(), stats_.get()));
    if (!streams.empty()) {
      new_sender->internal()->set_stream_id(streams[0]->label());
    }
    const TrackInfo* track_info = FindTrackInfo(
        local_audio_tracks_, new_sender->internal()->stream_id(), track->id());
    if (track_info) {
      new_sender->internal()->SetSsrc(track_info->ssrc);
    }
  } else if (track->kind() == MediaStreamTrackInterface::kVideoKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        new VideoRtpSender(static_cast<VideoTrackInterface*>(track),
                           session_->video_channel()));
    if (!streams.empty()) {
      new_sender->internal()->set_stream_id(streams[0]->label());
    }
    const TrackInfo* track_info = FindTrackInfo(
        local_video_tracks_, new_sender->internal()->stream_id(), track->id());
    if (track_info) {
      new_sender->internal()->SetSsrc(track_info->ssrc);
    }
  } else {
    LOG(LS_ERROR) << "CreateSender called with invalid kind: " << track->kind();
    return rtc::scoped_refptr<RtpSenderInterface>();
  }

  senders_.push_back(new_sender);
  observer_->OnRenegotiationNeeded();
  return new_sender;
}

bool PeerConnection::RemoveTrack(RtpSenderInterface* sender) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveTrack");
  if (IsClosed()) {
    return false;
  }

  auto it = std::find(senders_.begin(), senders_.end(), sender);
  if (it == senders_.end()) {
    LOG(LS_ERROR) << "Couldn't find sender " << sender->id() << " to remove.";
    return false;
  }
  (*it)->internal()->Stop();
  senders_.erase(it);

  observer_->OnRenegotiationNeeded();
  return true;
}

rtc::scoped_refptr<DtmfSenderInterface> PeerConnection::CreateDtmfSender(
    AudioTrackInterface* track) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateDtmfSender");
  if (IsClosed()) {
    return nullptr;
  }
  if (!track) {
    LOG(LS_ERROR) << "CreateDtmfSender - track is NULL.";
    return nullptr;
  }
  auto it = FindSenderForTrack(track);
  if (it == senders_.end()) {
    LOG(LS_ERROR) << "CreateDtmfSender called with a non-added track.";
    return nullptr;
  }

  return (*it)->GetDtmfSender();
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnection::CreateSender(
    const std::string& kind,
    const std::string& stream_id) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateSender");
  if (IsClosed()) {
    return nullptr;
  }
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender;
  if (kind == MediaStreamTrackInterface::kAudioKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        new AudioRtpSender(session_->voice_channel(), stats_.get()));
  } else if (kind == MediaStreamTrackInterface::kVideoKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), new VideoRtpSender(session_->video_channel()));
  } else {
    LOG(LS_ERROR) << "CreateSender called with invalid kind: " << kind;
    return new_sender;
  }
  if (!stream_id.empty()) {
    new_sender->internal()->set_stream_id(stream_id);
  }
  senders_.push_back(new_sender);
  return new_sender;
}

std::vector<rtc::scoped_refptr<RtpSenderInterface>> PeerConnection::GetSenders()
    const {
  std::vector<rtc::scoped_refptr<RtpSenderInterface>> ret;
  for (const auto& sender : senders_) {
    ret.push_back(sender.get());
  }
  return ret;
}

std::vector<rtc::scoped_refptr<RtpReceiverInterface>>
PeerConnection::GetReceivers() const {
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> ret;
  for (const auto& receiver : receivers_) {
    ret.push_back(receiver.get());
  }
  return ret;
}

bool PeerConnection::GetStats(StatsObserver* observer,
                              MediaStreamTrackInterface* track,
                              StatsOutputLevel level) {
  TRACE_EVENT0("webrtc", "PeerConnection::GetStats");
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (!observer) {
    LOG(LS_ERROR) << "GetStats - observer is NULL.";
    return false;
  }

  stats_->UpdateStats(level);
  // The StatsCollector is used to tell if a track is valid because it may
  // remember tracks that the PeerConnection previously removed.
  if (track && !stats_->IsValidTrack(track->id())) {
    LOG(LS_WARNING) << "GetStats is called with an invalid track: "
                    << track->id();
    return false;
  }
  signaling_thread()->Post(RTC_FROM_HERE, this, MSG_GETSTATS,
                           new GetStatsMsg(observer, track));
  return true;
}

void PeerConnection::GetStats(RTCStatsCollectorCallback* callback) {
  RTC_DCHECK(stats_collector_);
  stats_collector_->GetStatsReport(callback);
}

PeerConnectionInterface::SignalingState PeerConnection::signaling_state() {
  return signaling_state_;
}

PeerConnectionInterface::IceConnectionState
PeerConnection::ice_connection_state() {
  return ice_connection_state_;
}

PeerConnectionInterface::IceGatheringState
PeerConnection::ice_gathering_state() {
  return ice_gathering_state_;
}

rtc::scoped_refptr<DataChannelInterface>
PeerConnection::CreateDataChannel(
    const std::string& label,
    const DataChannelInit* config) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateDataChannel");
#ifdef HAVE_QUIC
  if (session_->data_channel_type() == cricket::DCT_QUIC) {
    // TODO(zhihuang): Handle case when config is NULL.
    if (!config) {
      LOG(LS_ERROR) << "Missing config for QUIC data channel.";
      return nullptr;
    }
    // TODO(zhihuang): Allow unreliable or ordered QUIC data channels.
    if (!config->reliable || config->ordered) {
      LOG(LS_ERROR) << "QUIC data channel does not implement unreliable or "
                       "ordered delivery.";
      return nullptr;
    }
    return session_->quic_data_transport()->CreateDataChannel(label, config);
  }
#endif  // HAVE_QUIC

  bool first_datachannel = !HasDataChannels();

  std::unique_ptr<InternalDataChannelInit> internal_config;
  if (config) {
    internal_config.reset(new InternalDataChannelInit(*config));
  }
  rtc::scoped_refptr<DataChannelInterface> channel(
      InternalCreateDataChannel(label, internal_config.get()));
  if (!channel.get()) {
    return nullptr;
  }

  // Trigger the onRenegotiationNeeded event for every new RTP DataChannel, or
  // the first SCTP DataChannel.
  if (session_->data_channel_type() == cricket::DCT_RTP || first_datachannel) {
    observer_->OnRenegotiationNeeded();
  }

  return DataChannelProxy::Create(signaling_thread(), channel.get());
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const MediaConstraintsInterface* constraints) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateOffer");
  if (!observer) {
    LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }
  RTCOfferAnswerOptions options;

  bool value;
  size_t mandatory_constraints = 0;

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kOfferToReceiveAudio,
                     &value,
                     &mandatory_constraints)) {
    options.offer_to_receive_audio =
        value ? RTCOfferAnswerOptions::kOfferToReceiveMediaTrue : 0;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kOfferToReceiveVideo,
                     &value,
                     &mandatory_constraints)) {
    options.offer_to_receive_video =
        value ? RTCOfferAnswerOptions::kOfferToReceiveMediaTrue : 0;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kVoiceActivityDetection,
                     &value,
                     &mandatory_constraints)) {
    options.voice_activity_detection = value;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kIceRestart,
                     &value,
                     &mandatory_constraints)) {
    options.ice_restart = value;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kUseRtpMux,
                     &value,
                     &mandatory_constraints)) {
    options.use_rtp_mux = value;
  }

  CreateOffer(observer, options);
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const RTCOfferAnswerOptions& options) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateOffer");
  if (!observer) {
    LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }

  cricket::MediaSessionOptions session_options;
  if (!GetOptionsForOffer(options, &session_options)) {
    std::string error = "CreateOffer called with invalid options.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  session_->CreateOffer(observer, options, session_options);
}

void PeerConnection::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateAnswer");
  if (!observer) {
    LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }

  cricket::MediaSessionOptions session_options;
  if (!GetOptionsForAnswer(constraints, &session_options)) {
    std::string error = "CreateAnswer called with invalid constraints.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  session_->CreateAnswer(observer, session_options);
}

void PeerConnection::CreateAnswer(CreateSessionDescriptionObserver* observer,
                                  const RTCOfferAnswerOptions& options) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateAnswer");
  if (!observer) {
    LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }

  cricket::MediaSessionOptions session_options;
  if (!GetOptionsForAnswer(options, &session_options)) {
    std::string error = "CreateAnswer called with invalid options.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  session_->CreateAnswer(observer, session_options);
}

void PeerConnection::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetLocalDescription");
  if (IsClosed()) {
    return;
  }
  if (!observer) {
    LOG(LS_ERROR) << "SetLocalDescription - observer is NULL.";
    return;
  }
  if (!desc) {
    PostSetSessionDescriptionFailure(observer, "SessionDescription is NULL.");
    return;
  }
  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_->UpdateStats(kStatsOutputLevelStandard);
  std::string error;
  if (!session_->SetLocalDescription(desc, &error)) {
    PostSetSessionDescriptionFailure(observer, error);
    return;
  }

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (session_->data_channel_type() == cricket::DCT_SCTP &&
      session_->GetSctpSslRole(&role)) {
    AllocateSctpSids(role);
  }

  // Update state and SSRC of local MediaStreams and DataChannels based on the
  // local session description.
  const cricket::ContentInfo* audio_content =
      GetFirstAudioContent(desc->description());
  if (audio_content) {
    if (audio_content->rejected) {
      RemoveTracks(cricket::MEDIA_TYPE_AUDIO);
    } else {
      const cricket::AudioContentDescription* audio_desc =
          static_cast<const cricket::AudioContentDescription*>(
              audio_content->description);
      UpdateLocalTracks(audio_desc->streams(), audio_desc->type());
    }
  }

  const cricket::ContentInfo* video_content =
      GetFirstVideoContent(desc->description());
  if (video_content) {
    if (video_content->rejected) {
      RemoveTracks(cricket::MEDIA_TYPE_VIDEO);
    } else {
      const cricket::VideoContentDescription* video_desc =
          static_cast<const cricket::VideoContentDescription*>(
              video_content->description);
      UpdateLocalTracks(video_desc->streams(), video_desc->type());
    }
  }

  const cricket::ContentInfo* data_content =
      GetFirstDataContent(desc->description());
  if (data_content) {
    const cricket::DataContentDescription* data_desc =
        static_cast<const cricket::DataContentDescription*>(
            data_content->description);
    if (rtc::starts_with(data_desc->protocol().data(),
                         cricket::kMediaProtocolRtpPrefix)) {
      UpdateLocalRtpDataChannels(data_desc->streams());
    }
  }

  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);

  // MaybeStartGathering needs to be called after posting
  // MSG_SET_SESSIONDESCRIPTION_SUCCESS, so that we don't signal any candidates
  // before signaling that SetLocalDescription completed.
  session_->MaybeStartGathering();
}

void PeerConnection::SetRemoteDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetRemoteDescription");
  if (IsClosed()) {
    return;
  }
  if (!observer) {
    LOG(LS_ERROR) << "SetRemoteDescription - observer is NULL.";
    return;
  }
  if (!desc) {
    PostSetSessionDescriptionFailure(observer, "SessionDescription is NULL.");
    return;
  }
  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_->UpdateStats(kStatsOutputLevelStandard);
  std::string error;
  if (!session_->SetRemoteDescription(desc, &error)) {
    PostSetSessionDescriptionFailure(observer, error);
    return;
  }

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (session_->data_channel_type() == cricket::DCT_SCTP &&
      session_->GetSctpSslRole(&role)) {
    AllocateSctpSids(role);
  }

  const cricket::SessionDescription* remote_desc = desc->description();
  const cricket::ContentInfo* audio_content = GetFirstAudioContent(remote_desc);
  const cricket::ContentInfo* video_content = GetFirstVideoContent(remote_desc);
  const cricket::AudioContentDescription* audio_desc =
      GetFirstAudioContentDescription(remote_desc);
  const cricket::VideoContentDescription* video_desc =
      GetFirstVideoContentDescription(remote_desc);
  const cricket::DataContentDescription* data_desc =
      GetFirstDataContentDescription(remote_desc);

  // Check if the descriptions include streams, just in case the peer supports
  // MSID, but doesn't indicate so with "a=msid-semantic".
  if (remote_desc->msid_supported() ||
      (audio_desc && !audio_desc->streams().empty()) ||
      (video_desc && !video_desc->streams().empty())) {
    remote_peer_supports_msid_ = true;
  }

  // We wait to signal new streams until we finish processing the description,
  // since only at that point will new streams have all their tracks.
  rtc::scoped_refptr<StreamCollection> new_streams(StreamCollection::Create());

  // Find all audio rtp streams and create corresponding remote AudioTracks
  // and MediaStreams.
  if (audio_content) {
    if (audio_content->rejected) {
      RemoveTracks(cricket::MEDIA_TYPE_AUDIO);
    } else {
      bool default_audio_track_needed =
          !remote_peer_supports_msid_ &&
          MediaContentDirectionHasSend(audio_desc->direction());
      UpdateRemoteStreamsList(GetActiveStreams(audio_desc),
                              default_audio_track_needed, audio_desc->type(),
                              new_streams);
    }
  }

  // Find all video rtp streams and create corresponding remote VideoTracks
  // and MediaStreams.
  if (video_content) {
    if (video_content->rejected) {
      RemoveTracks(cricket::MEDIA_TYPE_VIDEO);
    } else {
      bool default_video_track_needed =
          !remote_peer_supports_msid_ &&
          MediaContentDirectionHasSend(video_desc->direction());
      UpdateRemoteStreamsList(GetActiveStreams(video_desc),
                              default_video_track_needed, video_desc->type(),
                              new_streams);
    }
  }

  // Update the DataChannels with the information from the remote peer.
  if (data_desc) {
    if (rtc::starts_with(data_desc->protocol().data(),
                         cricket::kMediaProtocolRtpPrefix)) {
      UpdateRemoteRtpDataChannels(GetActiveStreams(data_desc));
    }
  }

  // Iterate new_streams and notify the observer about new MediaStreams.
  for (size_t i = 0; i < new_streams->count(); ++i) {
    MediaStreamInterface* new_stream = new_streams->at(i);
    stats_->AddStream(new_stream);
    // Call both the raw pointer and scoped_refptr versions of the method
    // for compatibility.
    observer_->OnAddStream(new_stream);
    observer_->OnAddStream(
        rtc::scoped_refptr<MediaStreamInterface>(new_stream));
  }

  UpdateEndedRemoteMediaStreams();

  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);
}

PeerConnectionInterface::RTCConfiguration PeerConnection::GetConfiguration() {
  return configuration_;
}

bool PeerConnection::SetConfiguration(const RTCConfiguration& configuration,
                                      RTCError* error) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetConfiguration");

  if (session_->local_description() &&
      configuration.ice_candidate_pool_size !=
          configuration_.ice_candidate_pool_size) {
    LOG(LS_ERROR) << "Can't change candidate pool size after calling "
                     "SetLocalDescription.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
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
  modified_config.ice_check_min_interval = configuration.ice_check_min_interval;
  if (configuration != modified_config) {
    LOG(LS_ERROR) << "Modifying the configuration in an unsupported way.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
  }

  // Note that this isn't possible through chromium, since it's an unsigned
  // short in WebIDL.
  if (configuration.ice_candidate_pool_size < 0 ||
      configuration.ice_candidate_pool_size > UINT16_MAX) {
    return SafeSetError(RTCErrorType::INVALID_RANGE, error);
  }

  // Parse ICE servers before hopping to network thread.
  cricket::ServerAddresses stun_servers;
  std::vector<cricket::RelayServerConfig> turn_servers;
  RTCErrorType parse_error =
      ParseIceServers(configuration.servers, &stun_servers, &turn_servers);
  if (parse_error != RTCErrorType::NONE) {
    return SafeSetError(parse_error, error);
  }

  // In theory this shouldn't fail.
  if (!network_thread()->Invoke<bool>(
          RTC_FROM_HERE,
          rtc::Bind(&PeerConnection::ReconfigurePortAllocator_n, this,
                    stun_servers, turn_servers, modified_config.type,
                    modified_config.ice_candidate_pool_size,
                    modified_config.prune_turn_ports))) {
    LOG(LS_ERROR) << "Failed to apply configuration to PortAllocator.";
    return SafeSetError(RTCErrorType::INTERNAL_ERROR, error);
  }

  // As described in JSEP, calling setConfiguration with new ICE servers or
  // candidate policy must set a "needs-ice-restart" bit so that the next offer
  // triggers an ICE restart which will pick up the changes.
  if (modified_config.servers != configuration_.servers ||
      modified_config.type != configuration_.type ||
      modified_config.prune_turn_ports != configuration_.prune_turn_ports) {
    session_->SetNeedsIceRestartFlag();
  }

  if (modified_config.ice_check_min_interval !=
      configuration_.ice_check_min_interval) {
    session_->SetIceConfig(session_->ParseIceConfig(modified_config));
  }

  configuration_ = modified_config;
  return SafeSetError(RTCErrorType::NONE, error);
}

bool PeerConnection::AddIceCandidate(
    const IceCandidateInterface* ice_candidate) {
  TRACE_EVENT0("webrtc", "PeerConnection::AddIceCandidate");
  if (IsClosed()) {
    return false;
  }
  return session_->ProcessIceMessage(ice_candidate);
}

bool PeerConnection::RemoveIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveIceCandidates");
  return session_->RemoveRemoteIceCandidates(candidates);
}

void PeerConnection::RegisterUMAObserver(UMAObserver* observer) {
  TRACE_EVENT0("webrtc", "PeerConnection::RegisterUmaObserver");
  uma_observer_ = observer;

  if (session_) {
    session_->set_metrics_observer(uma_observer_);
  }

  // Send information about IPv4/IPv6 status.
  if (uma_observer_) {
    port_allocator_->SetMetricsObserver(uma_observer_);
    if (port_allocator_->flags() & cricket::PORTALLOCATOR_ENABLE_IPV6) {
      uma_observer_->IncrementEnumCounter(
          kEnumCounterAddressFamily, kPeerConnection_IPv6,
          kPeerConnectionAddressFamilyCounter_Max);
    } else {
      uma_observer_->IncrementEnumCounter(
          kEnumCounterAddressFamily, kPeerConnection_IPv4,
          kPeerConnectionAddressFamilyCounter_Max);
    }
  }
}

bool PeerConnection::StartRtcEventLog(rtc::PlatformFile file,
                                      int64_t max_size_bytes) {
  return factory_->worker_thread()->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&PeerConnection::StartRtcEventLog_w, this, file,
                               max_size_bytes));
}

void PeerConnection::StopRtcEventLog() {
  factory_->worker_thread()->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&PeerConnection::StopRtcEventLog_w, this));
}

const SessionDescriptionInterface* PeerConnection::local_description() const {
  return session_->local_description();
}

const SessionDescriptionInterface* PeerConnection::remote_description() const {
  return session_->remote_description();
}

const SessionDescriptionInterface* PeerConnection::current_local_description()
    const {
  return session_->current_local_description();
}

const SessionDescriptionInterface* PeerConnection::current_remote_description()
    const {
  return session_->current_remote_description();
}

const SessionDescriptionInterface* PeerConnection::pending_local_description()
    const {
  return session_->pending_local_description();
}

const SessionDescriptionInterface* PeerConnection::pending_remote_description()
    const {
  return session_->pending_remote_description();
}

void PeerConnection::Close() {
  TRACE_EVENT0("webrtc", "PeerConnection::Close");
  // Update stats here so that we have the most recent stats for tracks and
  // streams before the channels are closed.
  stats_->UpdateStats(kStatsOutputLevelStandard);

  session_->Close();
  event_log_.reset();
}

void PeerConnection::OnSessionStateChange(WebRtcSession* /*session*/,
                                          WebRtcSession::State state) {
  switch (state) {
    case WebRtcSession::STATE_INIT:
      ChangeSignalingState(PeerConnectionInterface::kStable);
      break;
    case WebRtcSession::STATE_SENTOFFER:
      ChangeSignalingState(PeerConnectionInterface::kHaveLocalOffer);
      break;
    case WebRtcSession::STATE_SENTPRANSWER:
      ChangeSignalingState(PeerConnectionInterface::kHaveLocalPrAnswer);
      break;
    case WebRtcSession::STATE_RECEIVEDOFFER:
      ChangeSignalingState(PeerConnectionInterface::kHaveRemoteOffer);
      break;
    case WebRtcSession::STATE_RECEIVEDPRANSWER:
      ChangeSignalingState(PeerConnectionInterface::kHaveRemotePrAnswer);
      break;
    case WebRtcSession::STATE_INPROGRESS:
      ChangeSignalingState(PeerConnectionInterface::kStable);
      break;
    case WebRtcSession::STATE_CLOSED:
      ChangeSignalingState(PeerConnectionInterface::kClosed);
      break;
    default:
      break;
  }
}

void PeerConnection::OnMessage(rtc::Message* msg) {
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
      param->observer->OnFailure(param->error);
      delete param;
      break;
    }
    case MSG_CREATE_SESSIONDESCRIPTION_FAILED: {
      CreateSessionDescriptionMsg* param =
          static_cast<CreateSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnFailure(param->error);
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
    case MSG_FREE_DATACHANNELS: {
      sctp_data_channels_to_free_.clear();
      break;
    }
    default:
      RTC_NOTREACHED() << "Not implemented";
      break;
  }
}

void PeerConnection::CreateAudioReceiver(MediaStreamInterface* stream,
                                         const std::string& track_id,
                                         uint32_t ssrc) {
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
      receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
          signaling_thread(),
          new AudioRtpReceiver(track_id, ssrc, session_->voice_channel()));
  stream->AddTrack(
      static_cast<AudioTrackInterface*>(receiver->internal()->track().get()));
  receivers_.push_back(receiver);
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams;
  streams.push_back(rtc::scoped_refptr<MediaStreamInterface>(stream));
  observer_->OnAddTrack(receiver, streams);
}

void PeerConnection::CreateVideoReceiver(MediaStreamInterface* stream,
                                         const std::string& track_id,
                                         uint32_t ssrc) {
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
      receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
          signaling_thread(),
          new VideoRtpReceiver(track_id, factory_->worker_thread(), ssrc,
                               session_->video_channel()));
  stream->AddTrack(
      static_cast<VideoTrackInterface*>(receiver->internal()->track().get()));
  receivers_.push_back(receiver);
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams;
  streams.push_back(rtc::scoped_refptr<MediaStreamInterface>(stream));
  observer_->OnAddTrack(receiver, streams);
}

// TODO(deadbeef): Keep RtpReceivers around even if track goes away in remote
// description.
void PeerConnection::DestroyReceiver(const std::string& track_id) {
  auto it = FindReceiverForTrack(track_id);
  if (it == receivers_.end()) {
    LOG(LS_WARNING) << "RtpReceiver for track with id " << track_id
                    << " doesn't exist.";
  } else {
    (*it)->internal()->Stop();
    receivers_.erase(it);
  }
}

void PeerConnection::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  // After transitioning to "closed", ignore any additional states from
  // WebRtcSession (such as "disconnected").
  if (IsClosed()) {
    return;
  }
  ice_connection_state_ = new_state;
  observer_->OnIceConnectionChange(ice_connection_state_);
}

void PeerConnection::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  ice_gathering_state_ = new_state;
  observer_->OnIceGatheringChange(ice_gathering_state_);
}

void PeerConnection::OnIceCandidate(const IceCandidateInterface* candidate) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  observer_->OnIceCandidate(candidate);
}

void PeerConnection::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  observer_->OnIceCandidatesRemoved(candidates);
}

void PeerConnection::OnIceConnectionReceivingChange(bool receiving) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  observer_->OnIceConnectionReceivingChange(receiving);
}

void PeerConnection::ChangeSignalingState(
    PeerConnectionInterface::SignalingState signaling_state) {
  signaling_state_ = signaling_state;
  if (signaling_state == kClosed) {
    ice_connection_state_ = kIceConnectionClosed;
    observer_->OnIceConnectionChange(ice_connection_state_);
    if (ice_gathering_state_ != kIceGatheringComplete) {
      ice_gathering_state_ = kIceGatheringComplete;
      observer_->OnIceGatheringChange(ice_gathering_state_);
    }
  }
  observer_->OnSignalingChange(signaling_state_);
}

void PeerConnection::OnAudioTrackAdded(AudioTrackInterface* track,
                                       MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  auto sender = FindSenderForTrack(track);
  if (sender != senders_.end()) {
    // We already have a sender for this track, so just change the stream_id
    // so that it's correct in the next call to CreateOffer.
    (*sender)->internal()->set_stream_id(stream->label());
    return;
  }

  // Normal case; we've never seen this track before.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender =
      RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
          signaling_thread(),
          new AudioRtpSender(track, stream->label(), session_->voice_channel(),
                             stats_.get()));
  senders_.push_back(new_sender);
  // If the sender has already been configured in SDP, we call SetSsrc,
  // which will connect the sender to the underlying transport. This can
  // occur if a local session description that contains the ID of the sender
  // is set before AddStream is called. It can also occur if the local
  // session description is not changed and RemoveStream is called, and
  // later AddStream is called again with the same stream.
  const TrackInfo* track_info =
      FindTrackInfo(local_audio_tracks_, stream->label(), track->id());
  if (track_info) {
    new_sender->internal()->SetSsrc(track_info->ssrc);
  }
}

// TODO(deadbeef): Don't destroy RtpSenders here; they should be kept around
// indefinitely, when we have unified plan SDP.
void PeerConnection::OnAudioTrackRemoved(AudioTrackInterface* track,
                                         MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  auto sender = FindSenderForTrack(track);
  if (sender == senders_.end()) {
    LOG(LS_WARNING) << "RtpSender for track with id " << track->id()
                    << " doesn't exist.";
    return;
  }
  (*sender)->internal()->Stop();
  senders_.erase(sender);
}

void PeerConnection::OnVideoTrackAdded(VideoTrackInterface* track,
                                       MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  auto sender = FindSenderForTrack(track);
  if (sender != senders_.end()) {
    // We already have a sender for this track, so just change the stream_id
    // so that it's correct in the next call to CreateOffer.
    (*sender)->internal()->set_stream_id(stream->label());
    return;
  }

  // Normal case; we've never seen this track before.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender =
      RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
          signaling_thread(), new VideoRtpSender(track, stream->label(),
                                                 session_->video_channel()));
  senders_.push_back(new_sender);
  const TrackInfo* track_info =
      FindTrackInfo(local_video_tracks_, stream->label(), track->id());
  if (track_info) {
    new_sender->internal()->SetSsrc(track_info->ssrc);
  }
}

void PeerConnection::OnVideoTrackRemoved(VideoTrackInterface* track,
                                         MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  auto sender = FindSenderForTrack(track);
  if (sender == senders_.end()) {
    LOG(LS_WARNING) << "RtpSender for track with id " << track->id()
                    << " doesn't exist.";
    return;
  }
  (*sender)->internal()->Stop();
  senders_.erase(sender);
}

void PeerConnection::PostSetSessionDescriptionFailure(
    SetSessionDescriptionObserver* observer,
    const std::string& error) {
  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_SET_SESSIONDESCRIPTION_FAILED, msg);
}

void PeerConnection::PostCreateSessionDescriptionFailure(
    CreateSessionDescriptionObserver* observer,
    const std::string& error) {
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_CREATE_SESSIONDESCRIPTION_FAILED, msg);
}

bool PeerConnection::GetOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  // TODO(deadbeef): Once we have transceivers, enumerate them here instead of
  // ContentInfos.
  if (session_->local_description()) {
    for (const cricket::ContentInfo& content :
         session_->local_description()->description()->contents()) {
      session_options->transport_options[content.name] =
          cricket::TransportOptions();
    }
  }
  session_options->enable_ice_renomination =
      configuration_.enable_ice_renomination;

  if (!ExtractMediaSessionOptions(rtc_options, true, session_options)) {
    return false;
  }

  AddSendStreams(session_options, senders_, rtp_data_channels_);
  // Offer to receive audio/video if the constraint is not set and there are
  // send streams, or we're currently receiving.
  if (rtc_options.offer_to_receive_audio == RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_audio =
        session_options->HasSendMediaStream(cricket::MEDIA_TYPE_AUDIO) ||
        !remote_audio_tracks_.empty();
  }
  if (rtc_options.offer_to_receive_video == RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_video =
        session_options->HasSendMediaStream(cricket::MEDIA_TYPE_VIDEO) ||
        !remote_video_tracks_.empty();
  }

  // Intentionally unset the data channel type for RTP data channel with the
  // second condition. Otherwise the RTP data channels would be successfully
  // negotiated by default and the unit tests in WebRtcDataBrowserTest will fail
  // when building with chromium. We want to leave RTP data channels broken, so
  // people won't try to use them.
  if (HasDataChannels() && session_->data_channel_type() != cricket::DCT_RTP) {
    session_options->data_channel_type = session_->data_channel_type();
  }

  session_options->bundle_enabled =
      session_options->bundle_enabled &&
      (session_options->has_audio() || session_options->has_video() ||
       session_options->has_data());

  session_options->rtcp_cname = rtcp_cname_;
  session_options->crypto_options = factory_->options().crypto_options;
  return true;
}

void PeerConnection::InitializeOptionsForAnswer(
    cricket::MediaSessionOptions* session_options) {
  session_options->recv_audio = false;
  session_options->recv_video = false;
  session_options->enable_ice_renomination =
      configuration_.enable_ice_renomination;
}

void PeerConnection::FinishOptionsForAnswer(
    cricket::MediaSessionOptions* session_options) {
  // TODO(deadbeef): Once we have transceivers, enumerate them here instead of
  // ContentInfos.
  if (session_->remote_description()) {
    // Initialize the transport_options map.
    for (const cricket::ContentInfo& content :
         session_->remote_description()->description()->contents()) {
      session_options->transport_options[content.name] =
          cricket::TransportOptions();
    }
  }
  AddSendStreams(session_options, senders_, rtp_data_channels_);
  // RTP data channel is handled in MediaSessionOptions::AddStream. SCTP streams
  // are not signaled in the SDP so does not go through that path and must be
  // handled here.
  // Intentionally unset the data channel type for RTP data channel. Otherwise
  // the RTP data channels would be successfully negotiated by default and the
  // unit tests in WebRtcDataBrowserTest will fail when building with chromium.
  // We want to leave RTP data channels broken, so people won't try to use them.
  if (session_->data_channel_type() != cricket::DCT_RTP) {
    session_options->data_channel_type = session_->data_channel_type();
  }
  session_options->bundle_enabled =
      session_options->bundle_enabled &&
      (session_options->has_audio() || session_options->has_video() ||
       session_options->has_data());

  session_options->crypto_options = factory_->options().crypto_options;
}

bool PeerConnection::GetOptionsForAnswer(
    const MediaConstraintsInterface* constraints,
    cricket::MediaSessionOptions* session_options) {
  InitializeOptionsForAnswer(session_options);
  if (!ParseConstraintsForAnswer(constraints, session_options)) {
    return false;
  }
  session_options->rtcp_cname = rtcp_cname_;

  FinishOptionsForAnswer(session_options);
  return true;
}

bool PeerConnection::GetOptionsForAnswer(
    const RTCOfferAnswerOptions& options,
    cricket::MediaSessionOptions* session_options) {
  InitializeOptionsForAnswer(session_options);
  if (!ExtractMediaSessionOptions(options, false, session_options)) {
    return false;
  }
  session_options->rtcp_cname = rtcp_cname_;

  FinishOptionsForAnswer(session_options);
  return true;
}

void PeerConnection::RemoveTracks(cricket::MediaType media_type) {
  UpdateLocalTracks(std::vector<cricket::StreamParams>(), media_type);
  UpdateRemoteStreamsList(std::vector<cricket::StreamParams>(), false,
                          media_type, nullptr);
}

void PeerConnection::UpdateRemoteStreamsList(
    const cricket::StreamParamsVec& streams,
    bool default_track_needed,
    cricket::MediaType media_type,
    StreamCollection* new_streams) {
  TrackInfos* current_tracks = GetRemoteTracks(media_type);

  // Find removed tracks. I.e., tracks where the track id or ssrc don't match
  // the new StreamParam.
  auto track_it = current_tracks->begin();
  while (track_it != current_tracks->end()) {
    const TrackInfo& info = *track_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.ssrc);
    bool track_exists = params && params->id == info.track_id;
    // If this is a default track, and we still need it, don't remove it.
    if ((info.stream_label == kDefaultStreamLabel && default_track_needed) ||
        track_exists) {
      ++track_it;
    } else {
      OnRemoteTrackRemoved(info.stream_label, info.track_id, media_type);
      track_it = current_tracks->erase(track_it);
    }
  }

  // Find new and active tracks.
  for (const cricket::StreamParams& params : streams) {
    // The sync_label is the MediaStream label and the |stream.id| is the
    // track id.
    const std::string& stream_label = params.sync_label;
    const std::string& track_id = params.id;
    uint32_t ssrc = params.first_ssrc();

    rtc::scoped_refptr<MediaStreamInterface> stream =
        remote_streams_->find(stream_label);
    if (!stream) {
      // This is a new MediaStream. Create a new remote MediaStream.
      stream = MediaStreamProxy::Create(rtc::Thread::Current(),
                                        MediaStream::Create(stream_label));
      remote_streams_->AddStream(stream);
      new_streams->AddStream(stream);
    }

    const TrackInfo* track_info =
        FindTrackInfo(*current_tracks, stream_label, track_id);
    if (!track_info) {
      current_tracks->push_back(TrackInfo(stream_label, track_id, ssrc));
      OnRemoteTrackSeen(stream_label, track_id, ssrc, media_type);
    }
  }

  // Add default track if necessary.
  if (default_track_needed) {
    rtc::scoped_refptr<MediaStreamInterface> default_stream =
        remote_streams_->find(kDefaultStreamLabel);
    if (!default_stream) {
      // Create the new default MediaStream.
      default_stream = MediaStreamProxy::Create(
          rtc::Thread::Current(), MediaStream::Create(kDefaultStreamLabel));
      remote_streams_->AddStream(default_stream);
      new_streams->AddStream(default_stream);
    }
    std::string default_track_id = (media_type == cricket::MEDIA_TYPE_AUDIO)
                                       ? kDefaultAudioTrackLabel
                                       : kDefaultVideoTrackLabel;
    const TrackInfo* default_track_info =
        FindTrackInfo(*current_tracks, kDefaultStreamLabel, default_track_id);
    if (!default_track_info) {
      current_tracks->push_back(
          TrackInfo(kDefaultStreamLabel, default_track_id, 0));
      OnRemoteTrackSeen(kDefaultStreamLabel, default_track_id, 0, media_type);
    }
  }
}

void PeerConnection::OnRemoteTrackSeen(const std::string& stream_label,
                                       const std::string& track_id,
                                       uint32_t ssrc,
                                       cricket::MediaType media_type) {
  MediaStreamInterface* stream = remote_streams_->find(stream_label);

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    CreateAudioReceiver(stream, track_id, ssrc);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    CreateVideoReceiver(stream, track_id, ssrc);
  } else {
    RTC_NOTREACHED() << "Invalid media type";
  }
}

void PeerConnection::OnRemoteTrackRemoved(const std::string& stream_label,
                                          const std::string& track_id,
                                          cricket::MediaType media_type) {
  MediaStreamInterface* stream = remote_streams_->find(stream_label);

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    // When the MediaEngine audio channel is destroyed, the RemoteAudioSource
    // will be notified which will end the AudioRtpReceiver::track().
    DestroyReceiver(track_id);
    rtc::scoped_refptr<AudioTrackInterface> audio_track =
        stream->FindAudioTrack(track_id);
    if (audio_track) {
      stream->RemoveTrack(audio_track);
    }
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    // Stopping or destroying a VideoRtpReceiver will end the
    // VideoRtpReceiver::track().
    DestroyReceiver(track_id);
    rtc::scoped_refptr<VideoTrackInterface> video_track =
        stream->FindVideoTrack(track_id);
    if (video_track) {
      // There's no guarantee the track is still available, e.g. the track may
      // have been removed from the stream by an application.
      stream->RemoveTrack(video_track);
    }
  } else {
    RTC_NOTREACHED() << "Invalid media type";
  }
}

void PeerConnection::UpdateEndedRemoteMediaStreams() {
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams_to_remove;
  for (size_t i = 0; i < remote_streams_->count(); ++i) {
    MediaStreamInterface* stream = remote_streams_->at(i);
    if (stream->GetAudioTracks().empty() && stream->GetVideoTracks().empty()) {
      streams_to_remove.push_back(stream);
    }
  }

  for (auto& stream : streams_to_remove) {
    remote_streams_->RemoveStream(stream);
    // Call both the raw pointer and scoped_refptr versions of the method
    // for compatibility.
    observer_->OnRemoveStream(stream.get());
    observer_->OnRemoveStream(std::move(stream));
  }
}

void PeerConnection::UpdateLocalTracks(
    const std::vector<cricket::StreamParams>& streams,
    cricket::MediaType media_type) {
  TrackInfos* current_tracks = GetLocalTracks(media_type);

  // Find removed tracks. I.e., tracks where the track id, stream label or ssrc
  // don't match the new StreamParam.
  TrackInfos::iterator track_it = current_tracks->begin();
  while (track_it != current_tracks->end()) {
    const TrackInfo& info = *track_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.ssrc);
    if (!params || params->id != info.track_id ||
        params->sync_label != info.stream_label) {
      OnLocalTrackRemoved(info.stream_label, info.track_id, info.ssrc,
                          media_type);
      track_it = current_tracks->erase(track_it);
    } else {
      ++track_it;
    }
  }

  // Find new and active tracks.
  for (const cricket::StreamParams& params : streams) {
    // The sync_label is the MediaStream label and the |stream.id| is the
    // track id.
    const std::string& stream_label = params.sync_label;
    const std::string& track_id = params.id;
    uint32_t ssrc = params.first_ssrc();
    const TrackInfo* track_info =
        FindTrackInfo(*current_tracks, stream_label, track_id);
    if (!track_info) {
      current_tracks->push_back(TrackInfo(stream_label, track_id, ssrc));
      OnLocalTrackSeen(stream_label, track_id, params.first_ssrc(), media_type);
    }
  }
}

void PeerConnection::OnLocalTrackSeen(const std::string& stream_label,
                                      const std::string& track_id,
                                      uint32_t ssrc,
                                      cricket::MediaType media_type) {
  RtpSenderInternal* sender = FindSenderById(track_id);
  if (!sender) {
    LOG(LS_WARNING) << "An unknown RtpSender with id " << track_id
                    << " has been configured in the local description.";
    return;
  }

  if (sender->media_type() != media_type) {
    LOG(LS_WARNING) << "An RtpSender has been configured in the local"
                    << " description with an unexpected media type.";
    return;
  }

  sender->set_stream_id(stream_label);
  sender->SetSsrc(ssrc);
}

void PeerConnection::OnLocalTrackRemoved(const std::string& stream_label,
                                         const std::string& track_id,
                                         uint32_t ssrc,
                                         cricket::MediaType media_type) {
  RtpSenderInternal* sender = FindSenderById(track_id);
  if (!sender) {
    // This is the normal case. I.e., RemoveStream has been called and the
    // SessionDescriptions has been renegotiated.
    return;
  }

  // A sender has been removed from the SessionDescription but it's still
  // associated with the PeerConnection. This only occurs if the SDP doesn't
  // match with the calls to CreateSender, AddStream and RemoveStream.
  if (sender->media_type() != media_type) {
    LOG(LS_WARNING) << "An RtpSender has been configured in the local"
                    << " description with an unexpected media type.";
    return;
  }

  sender->SetSsrc(0);
}

void PeerConnection::UpdateLocalRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  std::vector<std::string> existing_channels;

  // Find new and active data channels.
  for (const cricket::StreamParams& params : streams) {
    // |it->sync_label| is actually the data channel label. The reason is that
    // we use the same naming of data channels as we do for
    // MediaStreams and Tracks.
    // For MediaStreams, the sync_label is the MediaStream label and the
    // track label is the same as |streamid|.
    const std::string& channel_label = params.sync_label;
    auto data_channel_it = rtp_data_channels_.find(channel_label);
    if (data_channel_it == rtp_data_channels_.end()) {
      LOG(LS_ERROR) << "channel label not found";
      continue;
    }
    // Set the SSRC the data channel should use for sending.
    data_channel_it->second->SetSendSsrc(params.first_ssrc());
    existing_channels.push_back(data_channel_it->first);
  }

  UpdateClosingRtpDataChannels(existing_channels, true);
}

void PeerConnection::UpdateRemoteRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  std::vector<std::string> existing_channels;

  // Find new and active data channels.
  for (const cricket::StreamParams& params : streams) {
    // The data channel label is either the mslabel or the SSRC if the mslabel
    // does not exist. Ex a=ssrc:444330170 mslabel:test1.
    std::string label = params.sync_label.empty()
                            ? rtc::ToString(params.first_ssrc())
                            : params.sync_label;
    auto data_channel_it = rtp_data_channels_.find(label);
    if (data_channel_it == rtp_data_channels_.end()) {
      // This is a new data channel.
      CreateRemoteRtpDataChannel(label, params.first_ssrc());
    } else {
      data_channel_it->second->SetReceiveSsrc(params.first_ssrc());
    }
    existing_channels.push_back(label);
  }

  UpdateClosingRtpDataChannels(existing_channels, false);
}

void PeerConnection::UpdateClosingRtpDataChannels(
    const std::vector<std::string>& active_channels,
    bool is_local_update) {
  auto it = rtp_data_channels_.begin();
  while (it != rtp_data_channels_.end()) {
    DataChannel* data_channel = it->second;
    if (std::find(active_channels.begin(), active_channels.end(),
                  data_channel->label()) != active_channels.end()) {
      ++it;
      continue;
    }

    if (is_local_update) {
      data_channel->SetSendSsrc(0);
    } else {
      data_channel->RemotePeerRequestClose();
    }

    if (data_channel->state() == DataChannel::kClosed) {
      rtp_data_channels_.erase(it);
      it = rtp_data_channels_.begin();
    } else {
      ++it;
    }
  }
}

void PeerConnection::CreateRemoteRtpDataChannel(const std::string& label,
                                                uint32_t remote_ssrc) {
  rtc::scoped_refptr<DataChannel> channel(
      InternalCreateDataChannel(label, nullptr));
  if (!channel.get()) {
    LOG(LS_WARNING) << "Remote peer requested a DataChannel but"
                    << "CreateDataChannel failed.";
    return;
  }
  channel->SetReceiveSsrc(remote_ssrc);
  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  // Call both the raw pointer and scoped_refptr versions of the method
  // for compatibility.
  observer_->OnDataChannel(proxy_channel.get());
  observer_->OnDataChannel(std::move(proxy_channel));
}

rtc::scoped_refptr<DataChannel> PeerConnection::InternalCreateDataChannel(
    const std::string& label,
    const InternalDataChannelInit* config) {
  if (IsClosed()) {
    return nullptr;
  }
  if (session_->data_channel_type() == cricket::DCT_NONE) {
    LOG(LS_ERROR)
        << "InternalCreateDataChannel: Data is not supported in this call.";
    return nullptr;
  }
  InternalDataChannelInit new_config =
      config ? (*config) : InternalDataChannelInit();
  if (session_->data_channel_type() == cricket::DCT_SCTP) {
    if (new_config.id < 0) {
      rtc::SSLRole role;
      if ((session_->GetSctpSslRole(&role)) &&
          !sid_allocator_.AllocateSid(role, &new_config.id)) {
        LOG(LS_ERROR) << "No id can be allocated for the SCTP data channel.";
        return nullptr;
      }
    } else if (!sid_allocator_.ReserveSid(new_config.id)) {
      LOG(LS_ERROR) << "Failed to create a SCTP data channel "
                    << "because the id is already in use or out of range.";
      return nullptr;
    }
  }

  rtc::scoped_refptr<DataChannel> channel(DataChannel::Create(
      session_.get(), session_->data_channel_type(), label, new_config));
  if (!channel) {
    sid_allocator_.ReleaseSid(new_config.id);
    return nullptr;
  }

  if (channel->data_channel_type() == cricket::DCT_RTP) {
    if (rtp_data_channels_.find(channel->label()) != rtp_data_channels_.end()) {
      LOG(LS_ERROR) << "DataChannel with label " << channel->label()
                    << " already exists.";
      return nullptr;
    }
    rtp_data_channels_[channel->label()] = channel;
  } else {
    RTC_DCHECK(channel->data_channel_type() == cricket::DCT_SCTP);
    sctp_data_channels_.push_back(channel);
    channel->SignalClosed.connect(this,
                                  &PeerConnection::OnSctpDataChannelClosed);
  }

  SignalDataChannelCreated(channel.get());
  return channel;
}

bool PeerConnection::HasDataChannels() const {
#ifdef HAVE_QUIC
  return !rtp_data_channels_.empty() || !sctp_data_channels_.empty() ||
         (session_->quic_data_transport() &&
          session_->quic_data_transport()->HasDataChannels());
#else
  return !rtp_data_channels_.empty() || !sctp_data_channels_.empty();
#endif  // HAVE_QUIC
}

void PeerConnection::AllocateSctpSids(rtc::SSLRole role) {
  for (const auto& channel : sctp_data_channels_) {
    if (channel->id() < 0) {
      int sid;
      if (!sid_allocator_.AllocateSid(role, &sid)) {
        LOG(LS_ERROR) << "Failed to allocate SCTP sid.";
        continue;
      }
      channel->SetSctpSid(sid);
    }
  }
}

void PeerConnection::OnSctpDataChannelClosed(DataChannel* channel) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  for (auto it = sctp_data_channels_.begin(); it != sctp_data_channels_.end();
       ++it) {
    if (it->get() == channel) {
      if (channel->id() >= 0) {
        sid_allocator_.ReleaseSid(channel->id());
      }
      // Since this method is triggered by a signal from the DataChannel,
      // we can't free it directly here; we need to free it asynchronously.
      sctp_data_channels_to_free_.push_back(*it);
      sctp_data_channels_.erase(it);
      signaling_thread()->Post(RTC_FROM_HERE, this, MSG_FREE_DATACHANNELS,
                               nullptr);
      return;
    }
  }
}

void PeerConnection::OnVoiceChannelCreated() {
  SetChannelOnSendersAndReceivers<AudioRtpSender, AudioRtpReceiver>(
      session_->voice_channel(), senders_, receivers_,
      cricket::MEDIA_TYPE_AUDIO);
}

void PeerConnection::OnVoiceChannelDestroyed() {
  SetChannelOnSendersAndReceivers<AudioRtpSender, AudioRtpReceiver,
                                  cricket::VoiceChannel>(
      nullptr, senders_, receivers_, cricket::MEDIA_TYPE_AUDIO);
}

void PeerConnection::OnVideoChannelCreated() {
  SetChannelOnSendersAndReceivers<VideoRtpSender, VideoRtpReceiver>(
      session_->video_channel(), senders_, receivers_,
      cricket::MEDIA_TYPE_VIDEO);
}

void PeerConnection::OnVideoChannelDestroyed() {
  SetChannelOnSendersAndReceivers<VideoRtpSender, VideoRtpReceiver,
                                  cricket::VideoChannel>(
      nullptr, senders_, receivers_, cricket::MEDIA_TYPE_VIDEO);
}

void PeerConnection::OnDataChannelCreated() {
  for (const auto& channel : sctp_data_channels_) {
    channel->OnTransportChannelCreated();
  }
}

void PeerConnection::OnDataChannelDestroyed() {
  // Use a temporary copy of the RTP/SCTP DataChannel list because the
  // DataChannel may callback to us and try to modify the list.
  std::map<std::string, rtc::scoped_refptr<DataChannel>> temp_rtp_dcs;
  temp_rtp_dcs.swap(rtp_data_channels_);
  for (const auto& kv : temp_rtp_dcs) {
    kv.second->OnTransportChannelDestroyed();
  }

  std::vector<rtc::scoped_refptr<DataChannel>> temp_sctp_dcs;
  temp_sctp_dcs.swap(sctp_data_channels_);
  for (const auto& channel : temp_sctp_dcs) {
    channel->OnTransportChannelDestroyed();
  }
}

void PeerConnection::OnDataChannelOpenMessage(
    const std::string& label,
    const InternalDataChannelInit& config) {
  rtc::scoped_refptr<DataChannel> channel(
      InternalCreateDataChannel(label, &config));
  if (!channel.get()) {
    LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message.";
    return;
  }

  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  // Call both the raw pointer and scoped_refptr versions of the method
  // for compatibility.
  observer_->OnDataChannel(proxy_channel.get());
  observer_->OnDataChannel(std::move(proxy_channel));
}

RtpSenderInternal* PeerConnection::FindSenderById(const std::string& id) {
  auto it = std::find_if(
      senders_.begin(), senders_.end(),
      [id](const rtc::scoped_refptr<
           RtpSenderProxyWithInternal<RtpSenderInternal>>& sender) {
        return sender->id() == id;
      });
  return it != senders_.end() ? (*it)->internal() : nullptr;
}

std::vector<
    rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>::iterator
PeerConnection::FindSenderForTrack(MediaStreamTrackInterface* track) {
  return std::find_if(
      senders_.begin(), senders_.end(),
      [track](const rtc::scoped_refptr<
              RtpSenderProxyWithInternal<RtpSenderInternal>>& sender) {
        return sender->track() == track;
      });
}

std::vector<rtc::scoped_refptr<
    RtpReceiverProxyWithInternal<RtpReceiverInternal>>>::iterator
PeerConnection::FindReceiverForTrack(const std::string& track_id) {
  return std::find_if(
      receivers_.begin(), receivers_.end(),
      [track_id](const rtc::scoped_refptr<
                 RtpReceiverProxyWithInternal<RtpReceiverInternal>>& receiver) {
        return receiver->id() == track_id;
      });
}

PeerConnection::TrackInfos* PeerConnection::GetRemoteTracks(
    cricket::MediaType media_type) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
  return (media_type == cricket::MEDIA_TYPE_AUDIO) ? &remote_audio_tracks_
                                                   : &remote_video_tracks_;
}

PeerConnection::TrackInfos* PeerConnection::GetLocalTracks(
    cricket::MediaType media_type) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
  return (media_type == cricket::MEDIA_TYPE_AUDIO) ? &local_audio_tracks_
                                                   : &local_video_tracks_;
}

const PeerConnection::TrackInfo* PeerConnection::FindTrackInfo(
    const PeerConnection::TrackInfos& infos,
    const std::string& stream_label,
    const std::string track_id) const {
  for (const TrackInfo& track_info : infos) {
    if (track_info.stream_label == stream_label &&
        track_info.track_id == track_id) {
      return &track_info;
    }
  }
  return nullptr;
}

DataChannel* PeerConnection::FindDataChannelBySid(int sid) const {
  for (const auto& channel : sctp_data_channels_) {
    if (channel->id() == sid) {
      return channel;
    }
  }
  return nullptr;
}

bool PeerConnection::InitializePortAllocator_n(
    const RTCConfiguration& configuration) {
  cricket::ServerAddresses stun_servers;
  std::vector<cricket::RelayServerConfig> turn_servers;
  if (ParseIceServers(configuration.servers, &stun_servers, &turn_servers) !=
      RTCErrorType::NONE) {
    return false;
  }

  port_allocator_->Initialize();

  // To handle both internal and externally created port allocator, we will
  // enable BUNDLE here.
  int portallocator_flags = port_allocator_->flags();
  portallocator_flags |= cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                         cricket::PORTALLOCATOR_ENABLE_IPV6;
  // If the disable-IPv6 flag was specified, we'll not override it
  // by experiment.
  if (configuration.disable_ipv6) {
    portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  } else if (webrtc::field_trial::FindFullName("WebRTC-IPv6Default")
                 .find("Disabled") == 0) {
    portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  }

  if (configuration.tcp_candidate_policy == kTcpCandidatePolicyDisabled) {
    portallocator_flags |= cricket::PORTALLOCATOR_DISABLE_TCP;
    LOG(LS_INFO) << "TCP candidates are disabled.";
  }

  if (configuration.candidate_network_policy ==
      kCandidateNetworkPolicyLowCost) {
    portallocator_flags |= cricket::PORTALLOCATOR_DISABLE_COSTLY_NETWORKS;
    LOG(LS_INFO) << "Do not gather candidates on high-cost networks";
  }

  port_allocator_->set_flags(portallocator_flags);
  // No step delay is used while allocating ports.
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);
  port_allocator_->set_candidate_filter(
      ConvertIceTransportTypeToCandidateFilter(configuration.type));

  // Call this last since it may create pooled allocator sessions using the
  // properties set above.
  port_allocator_->SetConfiguration(stun_servers, turn_servers,
                                    configuration.ice_candidate_pool_size,
                                    configuration.prune_turn_ports);
  return true;
}

bool PeerConnection::ReconfigurePortAllocator_n(
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    IceTransportsType type,
    int candidate_pool_size,
    bool prune_turn_ports) {
  port_allocator_->set_candidate_filter(
      ConvertIceTransportTypeToCandidateFilter(type));
  // Call this last since it may create pooled allocator sessions using the
  // candidate filter set above.
  return port_allocator_->SetConfiguration(
      stun_servers, turn_servers, candidate_pool_size, prune_turn_ports);
}

bool PeerConnection::StartRtcEventLog_w(rtc::PlatformFile file,
                                        int64_t max_size_bytes) {
  if (!event_log_) {
    return false;
  }
  return event_log_->StartLogging(file, max_size_bytes);
}

void PeerConnection::StopRtcEventLog_w() {
  if (event_log_) {
    event_log_->StopLogging();
  }
}
}  // namespace webrtc
