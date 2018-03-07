/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/peerconnection.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "api/jsepicecandidate.h"
#include "api/jsepsessiondescription.h"
#include "api/mediaconstraintsinterface.h"
#include "api/mediastreamproxy.h"
#include "api/mediastreamtrackproxy.h"
#include "call/call.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "media/sctp/sctptransport.h"
#include "pc/audiotrack.h"
#include "pc/channel.h"
#include "pc/channelmanager.h"
#include "pc/dtmfsender.h"
#include "pc/mediastream.h"
#include "pc/mediastreamobserver.h"
#include "pc/remoteaudiosource.h"
#include "pc/rtpmediautils.h"
#include "pc/rtpreceiver.h"
#include "pc/rtpsender.h"
#include "pc/sctputils.h"
#include "pc/streamcollection.h"
#include "pc/videocapturertracksource.h"
#include "pc/videotrack.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

using cricket::ContentInfo;
using cricket::ContentInfos;
using cricket::MediaContentDescription;
using cricket::SessionDescription;
using cricket::TransportInfo;

using cricket::LOCAL_PORT_TYPE;
using cricket::STUN_PORT_TYPE;
using cricket::RELAY_PORT_TYPE;
using cricket::PRFLX_PORT_TYPE;

namespace webrtc {

// Error messages
const char kBundleWithoutRtcpMux[] =
    "rtcp-mux must be enabled when BUNDLE "
    "is enabled.";
const char kCreateChannelFailed[] = "Failed to create channels.";
const char kInvalidCandidates[] = "Description contains invalid candidates.";
const char kInvalidSdp[] = "Invalid session description.";
const char kMlineMismatchInAnswer[] =
    "The order of m-lines in answer doesn't match order in offer. Rejecting "
    "answer.";
const char kMlineMismatchInSubsequentOffer[] =
    "The order of m-lines in subsequent offer doesn't match order from "
    "previous offer/answer.";
const char kPushDownTDFailed[] = "Failed to push down transport description:";
const char kSdpWithoutDtlsFingerprint[] =
    "Called with SDP without DTLS fingerprint.";
const char kSdpWithoutSdesCrypto[] = "Called with SDP without SDES crypto.";
const char kSdpWithoutIceUfragPwd[] =
    "Called with SDP without ice-ufrag and ice-pwd.";
const char kSessionError[] = "Session error code: ";
const char kSessionErrorDesc[] = "Session error description: ";
const char kDtlsSrtpSetupFailureRtp[] =
    "Couldn't set up DTLS-SRTP on RTP channel.";
const char kDtlsSrtpSetupFailureRtcp[] =
    "Couldn't set up DTLS-SRTP on RTCP channel.";
const char kEnableBundleFailed[] = "Failed to enable BUNDLE.";

namespace {

static const char kDefaultStreamLabel[] = "default";
static const char kDefaultAudioSenderId[] = "defaulta0";
static const char kDefaultVideoSenderId[] = "defaultv0";

// The length of RTCP CNAMEs.
static const int kRtcpCnameLength = 16;

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

// Check if we can send |new_stream| on a PeerConnection.
bool CanAddLocalMediaStream(webrtc::StreamCollectionInterface* current_streams,
                            webrtc::MediaStreamInterface* new_stream) {
  if (!new_stream || !current_streams) {
    return false;
  }
  if (current_streams->find(new_stream->label()) != nullptr) {
    RTC_LOG(LS_ERROR) << "MediaStream with label " << new_stream->label()
                      << " is already added.";
    return false;
  }
  return true;
}

// If the direction is "recvonly" or "inactive", treat the description
// as containing no streams.
// See: https://code.google.com/p/webrtc/issues/detail?id=5054
std::vector<cricket::StreamParams> GetActiveStreams(
    const cricket::MediaContentDescription* desc) {
  return RtpTransceiverDirectionHasSend(desc->direction())
             ? desc->streams()
             : std::vector<cricket::StreamParams>();
}

bool IsValidOfferToReceiveMedia(int value) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions Options;
  return (value >= Options::kUndefined) &&
         (value <= Options::kMaxOfferToReceiveMedia);
}

// Add options to |[audio/video]_media_description_options| from |senders|.
void AddRtpSenderOptions(
    const std::vector<rtc::scoped_refptr<
        RtpSenderProxyWithInternal<RtpSenderInternal>>>& senders,
    cricket::MediaDescriptionOptions* audio_media_description_options,
    cricket::MediaDescriptionOptions* video_media_description_options) {
  for (const auto& sender : senders) {
    if (sender->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      if (audio_media_description_options) {
        audio_media_description_options->AddAudioSender(
            sender->id(), sender->internal()->stream_ids());
      }
    } else {
      RTC_DCHECK(sender->media_type() == cricket::MEDIA_TYPE_VIDEO);
      if (video_media_description_options) {
        video_media_description_options->AddVideoSender(
            sender->id(), sender->internal()->stream_ids(), 1);
      }
    }
  }
}

// Add options to |session_options| from |rtp_data_channels|.
void AddRtpDataChannelOptions(
    const std::map<std::string, rtc::scoped_refptr<DataChannel>>&
        rtp_data_channels,
    cricket::MediaDescriptionOptions* data_media_description_options) {
  if (!data_media_description_options) {
    return;
  }
  // Check for data channels.
  for (const auto& kv : rtp_data_channels) {
    const DataChannel* channel = kv.second;
    if (channel->state() == DataChannel::kConnecting ||
        channel->state() == DataChannel::kOpen) {
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

// Helper to set an error and return from a method.
bool SafeSetError(webrtc::RTCErrorType type, webrtc::RTCError* error) {
  if (error) {
    error->set_type(type);
  }
  return type == webrtc::RTCErrorType::NONE;
}

bool SafeSetError(webrtc::RTCError error, webrtc::RTCError* error_out) {
  if (error_out) {
    *error_out = std::move(error);
  }
  return error.ok();
}

std::string GetSignalingStateString(
    PeerConnectionInterface::SignalingState state) {
  switch (state) {
    case PeerConnectionInterface::kStable:
      return "kStable";
    case PeerConnectionInterface::kHaveLocalOffer:
      return "kHaveLocalOffer";
    case PeerConnectionInterface::kHaveLocalPrAnswer:
      return "kHavePrAnswer";
    case PeerConnectionInterface::kHaveRemoteOffer:
      return "kHaveRemoteOffer";
    case PeerConnectionInterface::kHaveRemotePrAnswer:
      return "kHaveRemotePrAnswer";
    case PeerConnectionInterface::kClosed:
      return "kClosed";
  }
  RTC_NOTREACHED();
  return "";
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
    bool local_private = IPIsPrivate(local.address().ipaddr());
    bool remote_private = IPIsPrivate(remote.address().ipaddr());
    if (local_private) {
      if (remote_private) {
        return kIceCandidatePairHostPrivateHostPrivate;
      } else {
        return kIceCandidatePairHostPrivateHostPublic;
      }
    } else {
      if (remote_private) {
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

// Verify that the order of media sections in |new_desc| matches
// |existing_desc|. The number of m= sections in |new_desc| should be no less
// than |existing_desc|.
bool MediaSectionsInSameOrder(const SessionDescription* existing_desc,
                              const SessionDescription* new_desc) {
  if (!existing_desc || !new_desc) {
    return false;
  }

  if (existing_desc->contents().size() > new_desc->contents().size()) {
    return false;
  }

  for (size_t i = 0; i < existing_desc->contents().size(); ++i) {
    if (new_desc->contents()[i].name != existing_desc->contents()[i].name) {
      return false;
    }
    const MediaContentDescription* new_desc_mdesc =
        static_cast<const MediaContentDescription*>(
            new_desc->contents()[i].description);
    const MediaContentDescription* existing_desc_mdesc =
        static_cast<const MediaContentDescription*>(
            existing_desc->contents()[i].description);
    if (new_desc_mdesc->type() != existing_desc_mdesc->type()) {
      return false;
    }
  }
  return true;
}

bool MediaSectionsHaveSameCount(const SessionDescription* desc1,
                                const SessionDescription* desc2) {
  if (!desc1 || !desc2) {
    return false;
  }
  return desc1->contents().size() == desc2->contents().size();
}

// Checks that each non-rejected content has SDES crypto keys or a DTLS
// fingerprint, unless it's in a BUNDLE group, in which case only the
// BUNDLE-tag section (first media section/description in the BUNDLE group)
// needs a ufrag and pwd. Mismatches, such as replying with a DTLS fingerprint
// to SDES keys, will be caught in JsepTransport negotiation, and backstopped
// by Channel's |srtp_required| check.
bool VerifyCrypto(const SessionDescription* desc,
                  bool dtls_enabled,
                  std::string* error) {
  const cricket::ContentGroup* bundle =
      desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  const ContentInfos& contents = desc->contents();
  for (size_t index = 0; index < contents.size(); ++index) {
    const ContentInfo* cinfo = &contents[index];
    if (cinfo->rejected) {
      continue;
    }
    if (bundle && bundle->HasContentName(cinfo->name) &&
        cinfo->name != *(bundle->FirstContentName())) {
      // This isn't the first media section in the BUNDLE group, so it's not
      // required to have crypto attributes, since only the crypto attributes
      // from the first section actually get used.
      continue;
    }

    // If the content isn't rejected or bundled into another m= section, crypto
    // must be present.
    const MediaContentDescription* media =
        static_cast<const MediaContentDescription*>(cinfo->description);
    const TransportInfo* tinfo = desc->GetTransportInfoByName(cinfo->name);
    if (!media || !tinfo) {
      // Something is not right.
      RTC_LOG(LS_ERROR) << kInvalidSdp;
      *error = kInvalidSdp;
      return false;
    }
    if (dtls_enabled) {
      if (!tinfo->description.identity_fingerprint) {
        RTC_LOG(LS_WARNING)
            << "Session description must have DTLS fingerprint if "
               "DTLS enabled.";
        *error = kSdpWithoutDtlsFingerprint;
        return false;
      }
    } else {
      if (media->cryptos().empty()) {
        RTC_LOG(LS_WARNING)
            << "Session description must have SDES when DTLS disabled.";
        *error = kSdpWithoutSdesCrypto;
        return false;
      }
    }
  }

  return true;
}

// Checks that each non-rejected content has ice-ufrag and ice-pwd set, unless
// it's in a BUNDLE group, in which case only the BUNDLE-tag section (first
// media section/description in the BUNDLE group) needs a ufrag and pwd.
bool VerifyIceUfragPwdPresent(const SessionDescription* desc) {
  const cricket::ContentGroup* bundle =
      desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  const ContentInfos& contents = desc->contents();
  for (size_t index = 0; index < contents.size(); ++index) {
    const ContentInfo* cinfo = &contents[index];
    if (cinfo->rejected) {
      continue;
    }
    if (bundle && bundle->HasContentName(cinfo->name) &&
        cinfo->name != *(bundle->FirstContentName())) {
      // This isn't the first media section in the BUNDLE group, so it's not
      // required to have ufrag/password, since only the ufrag/password from
      // the first section actually get used.
      continue;
    }

    // If the content isn't rejected or bundled into another m= section,
    // ice-ufrag and ice-pwd must be present.
    const TransportInfo* tinfo = desc->GetTransportInfoByName(cinfo->name);
    if (!tinfo) {
      // Something is not right.
      RTC_LOG(LS_ERROR) << kInvalidSdp;
      return false;
    }
    if (tinfo->description.ice_ufrag.empty() ||
        tinfo->description.ice_pwd.empty()) {
      RTC_LOG(LS_ERROR) << "Session description must have ice ufrag and pwd.";
      return false;
    }
  }
  return true;
}

bool GetTrackIdBySsrc(const SessionDescription* session_description,
                      uint32_t ssrc,
                      std::string* track_id) {
  RTC_DCHECK(track_id != NULL);

  const cricket::ContentInfo* audio_info =
      cricket::GetFirstAudioContent(session_description);
  if (audio_info) {
    const cricket::MediaContentDescription* audio_content =
        static_cast<const cricket::MediaContentDescription*>(
            audio_info->description);

    const auto* found =
        cricket::GetStreamBySsrc(audio_content->streams(), ssrc);
    if (found) {
      *track_id = found->id;
      return true;
    }
  }

  const cricket::ContentInfo* video_info =
      cricket::GetFirstVideoContent(session_description);
  if (video_info) {
    const cricket::MediaContentDescription* video_content =
        static_cast<const cricket::MediaContentDescription*>(
            video_info->description);

    const auto* found =
        cricket::GetStreamBySsrc(video_content->streams(), ssrc);
    if (found) {
      *track_id = found->id;
      return true;
    }
  }
  return false;
}

// Get the SCTP port out of a SessionDescription.
// Return -1 if not found.
int GetSctpPort(const SessionDescription* session_description) {
  const ContentInfo* content_info = GetFirstDataContent(session_description);
  RTC_DCHECK(content_info);
  if (!content_info) {
    return -1;
  }
  const cricket::DataContentDescription* data =
      static_cast<const cricket::DataContentDescription*>(
          (content_info->description));
  std::string value;
  cricket::DataCodec match_pattern(cricket::kGoogleSctpDataCodecPlType,
                                   cricket::kGoogleSctpDataCodecName);
  for (const cricket::DataCodec& codec : data->codecs()) {
    if (!codec.Matches(match_pattern)) {
      continue;
    }
    if (codec.GetParam(cricket::kCodecParamPort, &value)) {
      return rtc::FromString<int>(value);
    }
  }
  return -1;
}

bool BadSdp(const std::string& source,
            const std::string& type,
            const std::string& reason,
            std::string* err_desc) {
  std::ostringstream desc;
  desc << "Failed to set " << source;
  if (!type.empty()) {
    desc << " " << type;
  }
  desc << " sdp: " << reason;

  if (err_desc) {
    *err_desc = desc.str();
  }
  RTC_LOG(LS_ERROR) << desc.str();
  return false;
}

bool BadSdp(cricket::ContentSource source,
            const std::string& type,
            const std::string& reason,
            std::string* err_desc) {
  if (source == cricket::CS_LOCAL) {
    return BadSdp("local", type, reason, err_desc);
  } else {
    return BadSdp("remote", type, reason, err_desc);
  }
}

bool BadLocalSdp(const std::string& type,
                 const std::string& reason,
                 std::string* err_desc) {
  return BadSdp(cricket::CS_LOCAL, type, reason, err_desc);
}

bool BadRemoteSdp(const std::string& type,
                  const std::string& reason,
                  std::string* err_desc) {
  return BadSdp(cricket::CS_REMOTE, type, reason, err_desc);
}

bool BadOfferSdp(cricket::ContentSource source,
                 const std::string& reason,
                 std::string* err_desc) {
  return BadSdp(source, SessionDescriptionInterface::kOffer, reason, err_desc);
}

bool BadPranswerSdp(cricket::ContentSource source,
                    const std::string& reason,
                    std::string* err_desc) {
  return BadSdp(source, SessionDescriptionInterface::kPrAnswer, reason,
                err_desc);
}

bool BadAnswerSdp(cricket::ContentSource source,
                  const std::string& reason,
                  std::string* err_desc) {
  return BadSdp(source, SessionDescriptionInterface::kAnswer, reason, err_desc);
}

std::string BadStateErrMsg(PeerConnectionInterface::SignalingState state) {
  std::ostringstream desc;
  desc << "Called in wrong state: " << GetSignalingStateString(state);
  return desc.str();
}

#define GET_STRING_OF_ERROR_CODE(err) \
  case webrtc::PeerConnection::err:   \
    result = #err;                    \
    break;

std::string GetErrorCodeString(webrtc::PeerConnection::Error err) {
  std::string result;
  switch (err) {
    GET_STRING_OF_ERROR_CODE(ERROR_NONE)
    GET_STRING_OF_ERROR_CODE(ERROR_CONTENT)
    GET_STRING_OF_ERROR_CODE(ERROR_TRANSPORT)
    default:
      RTC_NOTREACHED();
      break;
  }
  return result;
}

std::string MakeErrorString(const std::string& error, const std::string& desc) {
  std::ostringstream ret;
  ret << error << " " << desc;
  return ret.str();
}

std::string MakeTdErrorString(const std::string& desc) {
  return MakeErrorString(kPushDownTDFailed, desc);
}

// Returns true if |new_desc| requests an ICE restart (i.e., new ufrag/pwd).
bool CheckForRemoteIceRestart(const SessionDescriptionInterface* old_desc,
                              const SessionDescriptionInterface* new_desc,
                              const std::string& content_name) {
  if (!old_desc) {
    return false;
  }
  const SessionDescription* new_sd = new_desc->description();
  const SessionDescription* old_sd = old_desc->description();
  const ContentInfo* cinfo = new_sd->GetContentByName(content_name);
  if (!cinfo || cinfo->rejected) {
    return false;
  }
  // If the content isn't rejected, check if ufrag and password has changed.
  const cricket::TransportDescription* new_transport_desc =
      new_sd->GetTransportDescriptionByName(content_name);
  const cricket::TransportDescription* old_transport_desc =
      old_sd->GetTransportDescriptionByName(content_name);
  if (!new_transport_desc || !old_transport_desc) {
    // No transport description exists. This is not an ICE restart.
    return false;
  }
  if (cricket::IceCredentialsChanged(
          old_transport_desc->ice_ufrag, old_transport_desc->ice_pwd,
          new_transport_desc->ice_ufrag, new_transport_desc->ice_pwd)) {
    RTC_LOG(LS_INFO) << "Remote peer requests ICE restart for " << content_name
                     << ".";
    return true;
  }
  return false;
}

}  // namespace

// Upon completion, posts a task to execute the callback of the
// SetSessionDescriptionObserver asynchronously on the same thread. At this
// point, the state of the peer connection might no longer reflect the effects
// of the SetRemoteDescription operation, as the peer connection could have been
// modified during the post.
// TODO(hbos): Remove this class once we remove the version of
// PeerConnectionInterface::SetRemoteDescription() that takes a
// SetSessionDescriptionObserver as an argument.
class PeerConnection::SetRemoteDescriptionObserverAdapter
    : public rtc::RefCountedObject<SetRemoteDescriptionObserverInterface> {
 public:
  SetRemoteDescriptionObserverAdapter(
      rtc::scoped_refptr<PeerConnection> pc,
      rtc::scoped_refptr<SetSessionDescriptionObserver> wrapper)
      : pc_(std::move(pc)), wrapper_(std::move(wrapper)) {}

  // SetRemoteDescriptionObserverInterface implementation.
  void OnSetRemoteDescriptionComplete(RTCError error) override {
    if (error.ok())
      pc_->PostSetSessionDescriptionSuccess(wrapper_);
    else
      pc_->PostSetSessionDescriptionFailure(wrapper_, error.message());
  }

 private:
  rtc::scoped_refptr<PeerConnection> pc_;
  rtc::scoped_refptr<SetSessionDescriptionObserver> wrapper_;
};

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
    bool enable_rtp_data_channel;
    rtc::Optional<int> screencast_min_bitrate;
    rtc::Optional<bool> combined_audio_video_bwe;
    rtc::Optional<bool> enable_dtls_srtp;
    TcpCandidatePolicy tcp_candidate_policy;
    CandidateNetworkPolicy candidate_network_policy;
    int audio_jitter_buffer_max_packets;
    bool audio_jitter_buffer_fast_accelerate;
    int ice_connection_receiving_timeout;
    int ice_backup_candidate_pair_ping_interval;
    ContinualGatheringPolicy continual_gathering_policy;
    bool prioritize_most_likely_ice_candidate_pairs;
    struct cricket::MediaConfig media_config;
    bool prune_turn_ports;
    bool presume_writable_when_fully_relayed;
    bool enable_ice_renomination;
    bool redetermine_role_on_ice_restart;
    rtc::Optional<int> ice_check_min_interval;
    rtc::Optional<rtc::IntervalRange> ice_regather_interval_range;
    webrtc::TurnCustomizer* turn_customizer;
    SdpSemantics sdp_semantics;
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
         disable_ipv6_on_wifi == o.disable_ipv6_on_wifi &&
         max_ipv6_networks == o.max_ipv6_networks &&
         enable_rtp_data_channel == o.enable_rtp_data_channel &&
         screencast_min_bitrate == o.screencast_min_bitrate &&
         combined_audio_video_bwe == o.combined_audio_video_bwe &&
         enable_dtls_srtp == o.enable_dtls_srtp &&
         ice_candidate_pool_size == o.ice_candidate_pool_size &&
         prune_turn_ports == o.prune_turn_ports &&
         presume_writable_when_fully_relayed ==
             o.presume_writable_when_fully_relayed &&
         enable_ice_renomination == o.enable_ice_renomination &&
         redetermine_role_on_ice_restart == o.redetermine_role_on_ice_restart &&
         ice_check_min_interval == o.ice_check_min_interval &&
         ice_regather_interval_range == o.ice_regather_interval_range &&
         turn_customizer == o.turn_customizer &&
         sdp_semantics == o.sdp_semantics;
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

bool ValidateOfferAnswerOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options) {
  return IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_audio) &&
         IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_video);
}

// From |rtc_options|, fill parts of |session_options| shared by all generated
// m= sections (in other words, nothing that involves a map/array).
void ExtractSharedMediaSessionOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  session_options->vad_enabled = rtc_options.voice_activity_detection;
  session_options->bundle_enabled = rtc_options.use_rtp_mux;
}

bool ConvertConstraintsToOfferAnswerOptions(
    const MediaConstraintsInterface* constraints,
    PeerConnectionInterface::RTCOfferAnswerOptions* offer_answer_options) {
  if (!constraints) {
    return true;
  }

  bool value = false;
  size_t mandatory_constraints_satisfied = 0;

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kOfferToReceiveAudio, &value,
                     &mandatory_constraints_satisfied)) {
    offer_answer_options->offer_to_receive_audio =
        value ? PeerConnectionInterface::RTCOfferAnswerOptions::
                    kOfferToReceiveMediaTrue
              : 0;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kOfferToReceiveVideo, &value,
                     &mandatory_constraints_satisfied)) {
    offer_answer_options->offer_to_receive_video =
        value ? PeerConnectionInterface::RTCOfferAnswerOptions::
                    kOfferToReceiveMediaTrue
              : 0;
  }
  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kVoiceActivityDetection, &value,
                     &mandatory_constraints_satisfied)) {
    offer_answer_options->voice_activity_detection = value;
  }
  if (FindConstraint(constraints, MediaConstraintsInterface::kUseRtpMux, &value,
                     &mandatory_constraints_satisfied)) {
    offer_answer_options->use_rtp_mux = value;
  }
  if (FindConstraint(constraints, MediaConstraintsInterface::kIceRestart,
                     &value, &mandatory_constraints_satisfied)) {
    offer_answer_options->ice_restart = value;
  }

  return mandatory_constraints_satisfied == constraints->GetMandatory().size();
}

PeerConnection::PeerConnection(PeerConnectionFactory* factory,
                               std::unique_ptr<RtcEventLog> event_log,
                               std::unique_ptr<Call> call)
    : factory_(factory),
      event_log_(std::move(event_log)),
      rtcp_cname_(GenerateRtcpCname()),
      local_streams_(StreamCollection::Create()),
      remote_streams_(StreamCollection::Create()),
      call_(std::move(call)) {}

PeerConnection::~PeerConnection() {
  TRACE_EVENT0("webrtc", "PeerConnection::~PeerConnection");
  RTC_DCHECK_RUN_ON(signaling_thread());

  // Need to detach RTP transceivers from PeerConnection, since it's about to be
  // destroyed.
  for (auto transceiver : transceivers_) {
    transceiver->Stop();
  }

  // Destroy stats_ because it depends on session_.
  stats_.reset(nullptr);
  if (stats_collector_) {
    stats_collector_->WaitForPendingRequest();
    stats_collector_ = nullptr;
  }

  // Destroy video channels first since they may have a pointer to a voice
  // channel.
  for (auto transceiver : transceivers_) {
    if (transceiver->internal()->media_type() == cricket::MEDIA_TYPE_VIDEO &&
        transceiver->internal()->channel()) {
      DestroyVideoChannel(static_cast<cricket::VideoChannel*>(
          transceiver->internal()->channel()));
    }
  }
  for (auto transceiver : transceivers_) {
    if (transceiver->internal()->media_type() == cricket::MEDIA_TYPE_AUDIO &&
        transceiver->internal()->channel()) {
      DestroyVoiceChannel(static_cast<cricket::VoiceChannel*>(
          transceiver->internal()->channel()));
    }
  }
  if (rtp_data_channel_) {
    DestroyDataChannel();
  }

  // Note: Cannot use rtc::Bind to create a functor to invoke because it will
  // grab a reference to this PeerConnection. The RefCountedObject vtable will
  // have already been destroyed (since it is a subclass of PeerConnection) and
  // using rtc::Bind will cause "Pure virtual function called" error to appear.

  if (sctp_transport_) {
    OnDataChannelDestroyed();
    network_thread()->Invoke<void>(RTC_FROM_HERE,
                                   [this] { DestroySctpTransport_n(); });
  }

  RTC_LOG(LS_INFO) << "Session: " << session_id() << " is destroyed.";

  webrtc_session_desc_factory_.reset();
  sctp_invoker_.reset();
  sctp_factory_.reset();
  transport_controller_.reset();

  // port_allocator_ lives on the network thread and should be destroyed there.
  network_thread()->Invoke<void>(RTC_FROM_HERE,
                                 [this] { port_allocator_.reset(); });
  // call_ and event_log_ must be destroyed on the worker thread.
  worker_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
    call_.reset();
    event_log_.reset();
  });
}

bool PeerConnection::Initialize(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    std::unique_ptr<cricket::PortAllocator> allocator,
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
    PeerConnectionObserver* observer) {
  TRACE_EVENT0("webrtc", "PeerConnection::Initialize");

  RTCError config_error = ValidateConfiguration(configuration);
  if (!config_error.ok()) {
    RTC_LOG(LS_ERROR) << "Invalid configuration: " << config_error.message();
    return false;
  }

  if (!allocator) {
    RTC_LOG(LS_ERROR)
        << "PeerConnection initialized without a PortAllocator? "
        << "This shouldn't happen if using PeerConnectionFactory.";
    return false;
  }

  if (!observer) {
    // TODO(deadbeef): Why do we do this?
    RTC_LOG(LS_ERROR) << "PeerConnection initialized without a "
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

  // RFC 3264: The numeric value of the session id and version in the
  // o line MUST be representable with a "64 bit signed integer".
  // Due to this constraint session id |session_id_| is max limited to
  // LLONG_MAX.
  session_id_ = rtc::ToString(rtc::CreateRandomId64() & LLONG_MAX);
  transport_controller_.reset(factory_->CreateTransportController(
      port_allocator_.get(), configuration.redetermine_role_on_ice_restart));
  transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLED);
  transport_controller_->SignalConnectionState.connect(
      this, &PeerConnection::OnTransportControllerConnectionState);
  transport_controller_->SignalGatheringState.connect(
      this, &PeerConnection::OnTransportControllerGatheringState);
  transport_controller_->SignalCandidatesGathered.connect(
      this, &PeerConnection::OnTransportControllerCandidatesGathered);
  transport_controller_->SignalCandidatesRemoved.connect(
      this, &PeerConnection::OnTransportControllerCandidatesRemoved);
  transport_controller_->SignalDtlsHandshakeError.connect(
      this, &PeerConnection::OnTransportControllerDtlsHandshakeError);

  sctp_factory_ = factory_->CreateSctpTransportInternalFactory();

  stats_.reset(new StatsCollector(this));
  stats_collector_ = RTCStatsCollector::Create(this);

  configuration_ = configuration;

  const PeerConnectionFactoryInterface::Options& options = factory_->options();

  transport_controller_->SetSslMaxProtocolVersion(options.ssl_max_version);

  // Obtain a certificate from RTCConfiguration if any were provided (optional).
  rtc::scoped_refptr<rtc::RTCCertificate> certificate;
  if (!configuration.certificates.empty()) {
    // TODO(hbos,torbjorng): Decide on certificate-selection strategy instead of
    // just picking the first one. The decision should be made based on the DTLS
    // handshake. The DTLS negotiations need to know about all certificates.
    certificate = configuration.certificates[0];
  }

  transport_controller_->SetIceConfig(ParseIceConfig(configuration));

  if (options.disable_encryption) {
    dtls_enabled_ = false;
  } else {
    // Enable DTLS by default if we have an identity store or a certificate.
    dtls_enabled_ = (cert_generator || certificate);
    // |configuration| can override the default |dtls_enabled_| value.
    if (configuration.enable_dtls_srtp) {
      dtls_enabled_ = *(configuration.enable_dtls_srtp);
    }
  }

  // Enable creation of RTP data channels if the kEnableRtpDataChannels is set.
  // It takes precendence over the disable_sctp_data_channels
  // PeerConnectionFactoryInterface::Options.
  if (configuration.enable_rtp_data_channel) {
    data_channel_type_ = cricket::DCT_RTP;
  } else {
    // DTLS has to be enabled to use SCTP.
    if (!options.disable_sctp_data_channels && dtls_enabled_) {
      data_channel_type_ = cricket::DCT_SCTP;
    }
  }

  video_options_.screencast_min_bitrate_kbps =
      configuration.screencast_min_bitrate;
  audio_options_.combined_audio_video_bwe =
      configuration.combined_audio_video_bwe;

  audio_options_.audio_jitter_buffer_max_packets =
      configuration.audio_jitter_buffer_max_packets;

  audio_options_.audio_jitter_buffer_fast_accelerate =
      configuration.audio_jitter_buffer_fast_accelerate;

  // Whether the certificate generator/certificate is null or not determines
  // what PeerConnectionDescriptionFactory will do, so make sure that we give it
  // the right instructions by clearing the variables if needed.
  if (!dtls_enabled_) {
    cert_generator.reset();
    certificate = nullptr;
  } else if (certificate) {
    // Favor generated certificate over the certificate generator.
    cert_generator.reset();
  }

  webrtc_session_desc_factory_.reset(new WebRtcSessionDescriptionFactory(
      signaling_thread(), channel_manager(), this, session_id(),
      std::move(cert_generator), certificate));
  webrtc_session_desc_factory_->SignalCertificateReady.connect(
      this, &PeerConnection::OnCertificateReady);

  if (options.disable_encryption) {
    webrtc_session_desc_factory_->SetSdesPolicy(cricket::SEC_DISABLED);
  }

  webrtc_session_desc_factory_->set_enable_encrypted_rtp_header_extensions(
      options.crypto_options.enable_encrypted_rtp_header_extensions);

  // Add default audio/video transceivers for Plan B SDP.
  if (!IsUnifiedPlan()) {
    transceivers_.push_back(
        RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
            signaling_thread(), new RtpTransceiver(cricket::MEDIA_TYPE_AUDIO)));
    transceivers_.push_back(
        RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
            signaling_thread(), new RtpTransceiver(cricket::MEDIA_TYPE_VIDEO)));
  }

  return true;
}

RTCError PeerConnection::ValidateConfiguration(
    const RTCConfiguration& config) const {
  if (config.ice_regather_interval_range &&
      config.continual_gathering_policy == GATHER_ONCE) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "ice_regather_interval_range specified but continual "
                    "gathering policy is GATHER_ONCE");
  }
  return RTCError::OK();
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
    AddAudioTrack(track.get(), local_stream);
  }
  for (const auto& track : local_stream->GetVideoTracks()) {
    AddVideoTrack(track.get(), local_stream);
  }

  stats_->AddStream(local_stream);
  observer_->OnRenegotiationNeeded();
  return true;
}

void PeerConnection::RemoveStream(MediaStreamInterface* local_stream) {
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
    RTC_LOG(LS_ERROR)
        << "Adding a track with two streams is not currently supported.";
    return nullptr;
  }
  if (FindSenderForTrack(track)) {
    RTC_LOG(LS_ERROR) << "Sender for track " << track->id()
                      << " already exists.";
    return nullptr;
  }

  // TODO(deadbeef): Support adding a track to multiple streams.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender;
  if (track->kind() == MediaStreamTrackInterface::kAudioKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        new AudioRtpSender(static_cast<AudioTrackInterface*>(track),
                           voice_channel(), stats_.get()));
    GetAudioTransceiver()->internal()->AddSender(new_sender);
    if (!streams.empty()) {
      new_sender->internal()->set_stream_id(streams[0]->label());
    }
    const RtpSenderInfo* sender_info =
        FindSenderInfo(local_audio_sender_infos_,
                       new_sender->internal()->stream_id(), track->id());
    if (sender_info) {
      new_sender->internal()->SetSsrc(sender_info->first_ssrc);
    }
  } else if (track->kind() == MediaStreamTrackInterface::kVideoKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        new VideoRtpSender(static_cast<VideoTrackInterface*>(track),
                           video_channel()));
    GetVideoTransceiver()->internal()->AddSender(new_sender);
    if (!streams.empty()) {
      new_sender->internal()->set_stream_id(streams[0]->label());
    }
    const RtpSenderInfo* sender_info =
        FindSenderInfo(local_video_sender_infos_,
                       new_sender->internal()->stream_id(), track->id());
    if (sender_info) {
      new_sender->internal()->SetSsrc(sender_info->first_ssrc);
    }
  } else {
    RTC_LOG(LS_ERROR) << "CreateSender called with invalid kind: "
                      << track->kind();
    return rtc::scoped_refptr<RtpSenderInterface>();
  }

  observer_->OnRenegotiationNeeded();
  return new_sender;
}

bool PeerConnection::RemoveTrack(RtpSenderInterface* sender) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveTrack");
  if (IsClosed()) {
    return false;
  }

  bool removed;
  if (sender->media_type() == cricket::MEDIA_TYPE_AUDIO) {
    removed = GetAudioTransceiver()->internal()->RemoveSender(sender);
  } else {
    RTC_DCHECK_EQ(cricket::MEDIA_TYPE_VIDEO, sender->media_type());
    removed = GetVideoTransceiver()->internal()->RemoveSender(sender);
  }
  if (!removed) {
    RTC_LOG(LS_ERROR) << "Couldn't find sender " << sender->id()
                      << " to remove.";
    return false;
  }

  observer_->OnRenegotiationNeeded();
  return true;
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
  if (!IsUnifiedPlan()) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INTERNAL_ERROR,
        "AddTransceiver only supported when Unified Plan is enabled.");
  }
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
  if (!IsUnifiedPlan()) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INTERNAL_ERROR,
        "AddTransceiver only supported when Unified Plan is enabled.");
  }
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
    const RtpTransceiverInit& init) {
  RTC_DCHECK((media_type == cricket::MEDIA_TYPE_AUDIO ||
              media_type == cricket::MEDIA_TYPE_VIDEO));
  if (track) {
    RTC_DCHECK_EQ(media_type,
                  (track->kind() == MediaStreamTrackInterface::kAudioKind
                       ? cricket::MEDIA_TYPE_AUDIO
                       : cricket::MEDIA_TYPE_VIDEO));
  }

  // TODO(bugs.webrtc.org/7600): Verify init.

  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender;
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
      receiver;
  std::string receiver_id = rtc::CreateRandomUuid();
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), new AudioRtpSender(nullptr, stats_.get()));
    receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
        signaling_thread(), new AudioRtpReceiver(receiver_id, {}, 0, nullptr));
  } else {
    RTC_DCHECK_EQ(cricket::MEDIA_TYPE_VIDEO, media_type);
    sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), new VideoRtpSender(nullptr));
    receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
        signaling_thread(),
        new VideoRtpReceiver(receiver_id, {}, worker_thread(), 0, nullptr));
  }
  // TODO(bugs.webrtc.org/7600): Initializing the sender/receiver with a null
  // channel prevents users from calling SetParameters on them, which is needed
  // to be in compliance with the spec.

  if (track) {
    sender->SetTrack(track);
  }

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
      transceiver = RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
          signaling_thread(), new RtpTransceiver(sender, receiver));
  transceiver->SetDirection(init.direction);

  transceivers_.push_back(transceiver);

  observer_->OnRenegotiationNeeded();

  return rtc::scoped_refptr<RtpTransceiverInterface>(transceiver);
}

rtc::scoped_refptr<DtmfSenderInterface> PeerConnection::CreateDtmfSender(
    AudioTrackInterface* track) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateDtmfSender");
  if (IsClosed()) {
    return nullptr;
  }
  if (!track) {
    RTC_LOG(LS_ERROR) << "CreateDtmfSender - track is NULL.";
    return nullptr;
  }
  auto track_sender = FindSenderForTrack(track);
  if (!track_sender) {
    RTC_LOG(LS_ERROR) << "CreateDtmfSender called with a non-added track.";
    return nullptr;
  }

  return track_sender->GetDtmfSender();
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnection::CreateSender(
    const std::string& kind,
    const std::string& stream_id) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateSender");
  if (IsClosed()) {
    return nullptr;
  }

  // TODO(steveanton): Move construction of the RtpSenders to RtpTransceiver.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender;
  if (kind == MediaStreamTrackInterface::kAudioKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), new AudioRtpSender(voice_channel(), stats_.get()));
    GetAudioTransceiver()->internal()->AddSender(new_sender);
  } else if (kind == MediaStreamTrackInterface::kVideoKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), new VideoRtpSender(video_channel()));
    GetVideoTransceiver()->internal()->AddSender(new_sender);
  } else {
    RTC_LOG(LS_ERROR) << "CreateSender called with invalid kind: " << kind;
    return nullptr;
  }

  if (!stream_id.empty()) {
    new_sender->internal()->set_stream_id(stream_id);
  }

  return new_sender;
}

std::vector<rtc::scoped_refptr<RtpSenderInterface>> PeerConnection::GetSenders()
    const {
  std::vector<rtc::scoped_refptr<RtpSenderInterface>> ret;
  for (auto sender : GetSendersInternal()) {
    ret.push_back(sender);
  }
  return ret;
}

std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
PeerConnection::GetSendersInternal() const {
  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
      all_senders;
  for (auto transceiver : transceivers_) {
    auto senders = transceiver->internal()->senders();
    all_senders.insert(all_senders.end(), senders.begin(), senders.end());
  }
  return all_senders;
}

std::vector<rtc::scoped_refptr<RtpReceiverInterface>>
PeerConnection::GetReceivers() const {
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
  for (auto transceiver : transceivers_) {
    auto receivers = transceiver->internal()->receivers();
    all_receivers.insert(all_receivers.end(), receivers.begin(),
                         receivers.end());
  }
  return all_receivers;
}

std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnection::GetTransceivers() const {
  RTC_DCHECK(IsUnifiedPlan());
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> all_transceivers;
  for (auto transceiver : transceivers_) {
    all_transceivers.push_back(transceiver);
  }
  return all_transceivers;
}

bool PeerConnection::GetStats(StatsObserver* observer,
                              MediaStreamTrackInterface* track,
                              StatsOutputLevel level) {
  TRACE_EVENT0("webrtc", "PeerConnection::GetStats");
  RTC_DCHECK(signaling_thread()->IsCurrent());
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
  if (data_channel_type() == cricket::DCT_RTP || first_datachannel) {
    observer_->OnRenegotiationNeeded();
  }

  return DataChannelProxy::Create(signaling_thread(), channel.get());
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const MediaConstraintsInterface* constraints) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateOffer");

  PeerConnectionInterface::RTCOfferAnswerOptions offer_answer_options;
  // Always create an offer even if |ConvertConstraintsToOfferAnswerOptions|
  // returns false for now. Because |ConvertConstraintsToOfferAnswerOptions|
  // compares the mandatory fields parsed with the mandatory fields added in the
  // |constraints| and some downstream applications might create offers with
  // mandatory fields which would not be parsed in the helper method. For
  // example, in Chromium/remoting, |kEnableDtlsSrtp| is added to the
  // |constraints| as a mandatory field but it is not parsed.
  ConvertConstraintsToOfferAnswerOptions(constraints, &offer_answer_options);

  CreateOffer(observer, offer_answer_options);
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const RTCOfferAnswerOptions& options) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateOffer");

  if (!observer) {
    RTC_LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }

  if (IsClosed()) {
    std::string error = "CreateOffer called when PeerConnection is closed.";
    RTC_LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  if (!ValidateOfferAnswerOptions(options)) {
    std::string error = "CreateOffer called with invalid options.";
    RTC_LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForOffer(options, &session_options);
  webrtc_session_desc_factory_->CreateOffer(observer, options, session_options);
}

void PeerConnection::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateAnswer");

  if (!observer) {
    RTC_LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }

  PeerConnectionInterface::RTCOfferAnswerOptions offer_answer_options;
  if (!ConvertConstraintsToOfferAnswerOptions(constraints,
                                              &offer_answer_options)) {
    std::string error = "CreateAnswer called with invalid constraints.";
    RTC_LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  CreateAnswer(observer, offer_answer_options);
}

void PeerConnection::CreateAnswer(CreateSessionDescriptionObserver* observer,
                                  const RTCOfferAnswerOptions& options) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateAnswer");
  if (!observer) {
    RTC_LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }

  if (IsClosed()) {
    std::string error = "CreateAnswer called when PeerConnection is closed.";
    RTC_LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  if (remote_description() &&
      remote_description()->type() != SessionDescriptionInterface::kOffer) {
    std::string error = "CreateAnswer called without remote offer.";
    RTC_LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForAnswer(options, &session_options);

  webrtc_session_desc_factory_->CreateAnswer(observer, session_options);
}

void PeerConnection::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetLocalDescription");
  if (!observer) {
    RTC_LOG(LS_ERROR) << "SetLocalDescription - observer is NULL.";
    return;
  }
  if (!desc) {
    PostSetSessionDescriptionFailure(observer, "SessionDescription is NULL.");
    return;
  }

  // Takes the ownership of |desc| regardless of the result.
  std::unique_ptr<SessionDescriptionInterface> desc_temp(desc);

  if (IsClosed()) {
    std::string error = "Failed to set local " + desc_temp->type() +
                        " sdp: Called in wrong state: STATE_CLOSED";
    RTC_LOG(LS_ERROR) << error;
    PostSetSessionDescriptionFailure(observer, error);
    return;
  }

  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_->UpdateStats(kStatsOutputLevelStandard);
  std::string error;
  // Takes the ownership of |desc_temp|. On success, local_description() is
  // updated to reflect the description that was passed in.
  if (!SetCurrentOrPendingLocalDescription(std::move(desc_temp), &error)) {
    PostSetSessionDescriptionFailure(observer, error);
    return;
  }
  RTC_DCHECK(local_description());

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (data_channel_type() == cricket::DCT_SCTP && GetSctpSslRole(&role)) {
    AllocateSctpSids(role);
  }

  // Update state and SSRC of local MediaStreams and DataChannels based on the
  // local session description.
  const cricket::ContentInfo* audio_content =
      GetFirstAudioContent(local_description()->description());
  if (audio_content) {
    if (audio_content->rejected) {
      RemoveSenders(cricket::MEDIA_TYPE_AUDIO);
    } else {
      const cricket::AudioContentDescription* audio_desc =
          static_cast<const cricket::AudioContentDescription*>(
              audio_content->description);
      UpdateLocalSenders(audio_desc->streams(), audio_desc->type());
    }
  }

  const cricket::ContentInfo* video_content =
      GetFirstVideoContent(local_description()->description());
  if (video_content) {
    if (video_content->rejected) {
      RemoveSenders(cricket::MEDIA_TYPE_VIDEO);
    } else {
      const cricket::VideoContentDescription* video_desc =
          static_cast<const cricket::VideoContentDescription*>(
              video_content->description);
      UpdateLocalSenders(video_desc->streams(), video_desc->type());
    }
  }

  const cricket::ContentInfo* data_content =
      GetFirstDataContent(local_description()->description());
  if (data_content) {
    const cricket::DataContentDescription* data_desc =
        static_cast<const cricket::DataContentDescription*>(
            data_content->description);
    if (rtc::starts_with(data_desc->protocol().data(),
                         cricket::kMediaProtocolRtpPrefix)) {
      UpdateLocalRtpDataChannels(data_desc->streams());
    }
  }

  PostSetSessionDescriptionSuccess(observer);

  // According to JSEP, after setLocalDescription, changing the candidate pool
  // size is not allowed, and changing the set of ICE servers will not result
  // in new candidates being gathered.
  port_allocator_->FreezeCandidatePool();

  // MaybeStartGathering needs to be called after posting
  // MSG_SET_SESSIONDESCRIPTION_SUCCESS, so that we don't signal any candidates
  // before signaling that SetLocalDescription completed.
  transport_controller_->MaybeStartGathering();

  if (local_description()->type() == SessionDescriptionInterface::kAnswer) {
    // TODO(deadbeef): We already had to hop to the network thread for
    // MaybeStartGathering...
    network_thread()->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                  port_allocator_.get()));
  }
}

void PeerConnection::SetRemoteDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  SetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface>(desc),
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface>(
          new SetRemoteDescriptionObserverAdapter(this, observer)));
}

void PeerConnection::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetRemoteDescription");
  if (!observer) {
    RTC_LOG(LS_ERROR) << "SetRemoteDescription - observer is NULL.";
    return;
  }
  if (!desc) {
    observer->OnSetRemoteDescriptionComplete(RTCError(
        RTCErrorType::UNSUPPORTED_PARAMETER, "SessionDescription is NULL."));
    return;
  }

  if (IsClosed()) {
    std::string error = "Failed to set remote " + desc->type() +
                        " sdp: Called in wrong state: STATE_CLOSED";
    RTC_LOG(LS_ERROR) << error;
    observer->OnSetRemoteDescriptionComplete(
        RTCError(RTCErrorType::INVALID_STATE, std::move(error)));
    return;
  }

  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_->UpdateStats(kStatsOutputLevelStandard);
  std::string error;
  // Takes the ownership of |desc|. On success, remote_description() is updated
  // to reflect the description that was passed in.
  if (!SetCurrentOrPendingRemoteDescription(std::move(desc), &error)) {
    observer->OnSetRemoteDescriptionComplete(
        RTCError(RTCErrorType::UNSUPPORTED_PARAMETER, std::move(error)));
    return;
  }
  RTC_DCHECK(remote_description());

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (data_channel_type() == cricket::DCT_SCTP && GetSctpSslRole(&role)) {
    AllocateSctpSids(role);
  }

  const cricket::ContentInfo* audio_content =
      GetFirstAudioContent(remote_description()->description());
  const cricket::ContentInfo* video_content =
      GetFirstVideoContent(remote_description()->description());
  const cricket::AudioContentDescription* audio_desc =
      GetFirstAudioContentDescription(remote_description()->description());
  const cricket::VideoContentDescription* video_desc =
      GetFirstVideoContentDescription(remote_description()->description());
  const cricket::DataContentDescription* data_desc =
      GetFirstDataContentDescription(remote_description()->description());

  // Check if the descriptions include streams, just in case the peer supports
  // MSID, but doesn't indicate so with "a=msid-semantic".
  if (remote_description()->description()->msid_supported() ||
      (audio_desc && !audio_desc->streams().empty()) ||
      (video_desc && !video_desc->streams().empty())) {
    remote_peer_supports_msid_ = true;
  }

  // We wait to signal new streams until we finish processing the description,
  // since only at that point will new streams have all their tracks.
  rtc::scoped_refptr<StreamCollection> new_streams(StreamCollection::Create());

  // TODO(steveanton): When removing RTP senders/receivers in response to a
  // rejected media section, there is some cleanup logic that expects the voice/
  // video channel to still be set. But in this method the voice/video channel
  // would have been destroyed by the SetRemoteDescription caller above so the
  // cleanup that relies on them fails to run. The RemoveSenders calls should be
  // moved to right before the DestroyChannel calls to fix this.

  // Find all audio rtp streams and create corresponding remote AudioTracks
  // and MediaStreams.
  if (audio_content) {
    if (audio_content->rejected) {
      RemoveSenders(cricket::MEDIA_TYPE_AUDIO);
    } else {
      bool default_audio_track_needed =
          !remote_peer_supports_msid_ &&
          RtpTransceiverDirectionHasSend(audio_desc->direction());
      UpdateRemoteSendersList(GetActiveStreams(audio_desc),
                              default_audio_track_needed, audio_desc->type(),
                              new_streams);
    }
  }

  // Find all video rtp streams and create corresponding remote VideoTracks
  // and MediaStreams.
  if (video_content) {
    if (video_content->rejected) {
      RemoveSenders(cricket::MEDIA_TYPE_VIDEO);
    } else {
      bool default_video_track_needed =
          !remote_peer_supports_msid_ &&
          RtpTransceiverDirectionHasSend(video_desc->direction());
      UpdateRemoteSendersList(GetActiveStreams(video_desc),
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
    observer_->OnAddStream(
        rtc::scoped_refptr<MediaStreamInterface>(new_stream));
  }

  UpdateEndedRemoteMediaStreams();

  observer->OnSetRemoteDescriptionComplete(RTCError::OK());

  if (remote_description()->type() == SessionDescriptionInterface::kAnswer) {
    // TODO(deadbeef): We already had to hop to the network thread for
    // MaybeStartGathering...
    network_thread()->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                  port_allocator_.get()));
  }
}

PeerConnectionInterface::RTCConfiguration PeerConnection::GetConfiguration() {
  return configuration_;
}

bool PeerConnection::SetConfiguration(const RTCConfiguration& configuration,
                                      RTCError* error) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetConfiguration");

  if (local_description() && configuration.ice_candidate_pool_size !=
                                 configuration_.ice_candidate_pool_size) {
    RTC_LOG(LS_ERROR) << "Can't change candidate pool size after calling "
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
  modified_config.turn_customizer = configuration.turn_customizer;
  if (configuration != modified_config) {
    RTC_LOG(LS_ERROR) << "Modifying the configuration in an unsupported way.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
  }

  // Validate the modified configuration.
  RTCError validate_error = ValidateConfiguration(modified_config);
  if (!validate_error.ok()) {
    return SafeSetError(std::move(validate_error), error);
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
                    modified_config.prune_turn_ports,
                    modified_config.turn_customizer))) {
    RTC_LOG(LS_ERROR) << "Failed to apply configuration to PortAllocator.";
    return SafeSetError(RTCErrorType::INTERNAL_ERROR, error);
  }

  // As described in JSEP, calling setConfiguration with new ICE servers or
  // candidate policy must set a "needs-ice-restart" bit so that the next offer
  // triggers an ICE restart which will pick up the changes.
  if (modified_config.servers != configuration_.servers ||
      modified_config.type != configuration_.type ||
      modified_config.prune_turn_ports != configuration_.prune_turn_ports) {
    transport_controller_->SetNeedsIceRestartFlag();
  }

  if (modified_config.ice_check_min_interval !=
      configuration_.ice_check_min_interval) {
    transport_controller_->SetIceConfig(ParseIceConfig(modified_config));
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

  if (!remote_description()) {
    RTC_LOG(LS_ERROR) << "ProcessIceMessage: ICE candidates can't be added "
                      << "without any remote session description.";
    return false;
  }

  if (!ice_candidate) {
    RTC_LOG(LS_ERROR) << "ProcessIceMessage: Candidate is NULL.";
    return false;
  }

  bool valid = false;
  bool ready = ReadyToUseRemoteCandidate(ice_candidate, nullptr, &valid);
  if (!valid) {
    return false;
  }

  // Add this candidate to the remote session description.
  if (!mutable_remote_description()->AddCandidate(ice_candidate)) {
    RTC_LOG(LS_ERROR) << "ProcessIceMessage: Candidate cannot be used.";
    return false;
  }

  if (ready) {
    return UseCandidate(ice_candidate);
  } else {
    RTC_LOG(LS_INFO) << "ProcessIceMessage: Not ready to use candidate.";
    return true;
  }
}

bool PeerConnection::RemoveIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveIceCandidates");
  if (!remote_description()) {
    RTC_LOG(LS_ERROR) << "RemoveRemoteIceCandidates: ICE candidates can't be "
                      << "removed without any remote session description.";
    return false;
  }

  if (candidates.empty()) {
    RTC_LOG(LS_ERROR) << "RemoveRemoteIceCandidates: candidates are empty.";
    return false;
  }

  size_t number_removed =
      mutable_remote_description()->RemoveCandidates(candidates);
  if (number_removed != candidates.size()) {
    RTC_LOG(LS_ERROR)
        << "RemoveRemoteIceCandidates: Failed to remove candidates. "
        << "Requested " << candidates.size() << " but only " << number_removed
        << " are removed.";
  }

  // Remove the candidates from the transport controller.
  std::string error;
  bool res = transport_controller_->RemoveRemoteCandidates(candidates, &error);
  if (!res && !error.empty()) {
    RTC_LOG(LS_ERROR) << "Error when removing remote candidates: " << error;
  }
  return true;
}

void PeerConnection::RegisterUMAObserver(UMAObserver* observer) {
  TRACE_EVENT0("webrtc", "PeerConnection::RegisterUmaObserver");
  uma_observer_ = observer;

  if (transport_controller()) {
    transport_controller()->SetMetricsObserver(uma_observer_);
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

RTCError PeerConnection::SetBitrate(const BitrateParameters& bitrate) {
  if (!worker_thread()->IsCurrent()) {
    return worker_thread()->Invoke<RTCError>(
        RTC_FROM_HERE, rtc::Bind(&PeerConnection::SetBitrate, this, bitrate));
  }

  const bool has_min = static_cast<bool>(bitrate.min_bitrate_bps);
  const bool has_current = static_cast<bool>(bitrate.current_bitrate_bps);
  const bool has_max = static_cast<bool>(bitrate.max_bitrate_bps);
  if (has_min && *bitrate.min_bitrate_bps < 0) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "min_bitrate_bps <= 0");
  }
  if (has_current) {
    if (has_min && *bitrate.current_bitrate_bps < *bitrate.min_bitrate_bps) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "current_bitrate_bps < min_bitrate_bps");
    } else if (*bitrate.current_bitrate_bps < 0) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "curent_bitrate_bps < 0");
    }
  }
  if (has_max) {
    if (has_current &&
        *bitrate.max_bitrate_bps < *bitrate.current_bitrate_bps) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max_bitrate_bps < current_bitrate_bps");
    } else if (has_min && *bitrate.max_bitrate_bps < *bitrate.min_bitrate_bps) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max_bitrate_bps < min_bitrate_bps");
    } else if (*bitrate.max_bitrate_bps < 0) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max_bitrate_bps < 0");
    }
  }

  Call::Config::BitrateConfigMask mask;
  mask.min_bitrate_bps = bitrate.min_bitrate_bps;
  mask.start_bitrate_bps = bitrate.current_bitrate_bps;
  mask.max_bitrate_bps = bitrate.max_bitrate_bps;

  RTC_DCHECK(call_.get());
  call_->SetBitrateConfigMask(mask);

  return RTCError::OK();
}

void PeerConnection::SetBitrateAllocationStrategy(
    std::unique_ptr<rtc::BitrateAllocationStrategy>
        bitrate_allocation_strategy) {
  rtc::Thread* worker_thread = factory_->worker_thread();
  if (!worker_thread->IsCurrent()) {
    rtc::BitrateAllocationStrategy* strategy_raw =
        bitrate_allocation_strategy.release();
    auto functor = [this, strategy_raw]() {
      call_->SetBitrateAllocationStrategy(
          rtc::WrapUnique<rtc::BitrateAllocationStrategy>(strategy_raw));
    };
    worker_thread->Invoke<void>(RTC_FROM_HERE, functor);
    return;
  }
  RTC_DCHECK(call_.get());
  call_->SetBitrateAllocationStrategy(std::move(bitrate_allocation_strategy));
}

void PeerConnection::SetAudioPlayout(bool playout) {
  if (!worker_thread()->IsCurrent()) {
    worker_thread()->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&PeerConnection::SetAudioPlayout, this, playout));
    return;
  }
  auto audio_state =
      factory_->channel_manager()->media_engine()->GetAudioState();
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
      factory_->channel_manager()->media_engine()->GetAudioState();
  audio_state->SetRecording(recording);
}

std::unique_ptr<rtc::SSLCertificate>
PeerConnection::GetRemoteAudioSSLCertificate() {
  if (!voice_channel()) {
    return nullptr;
  }
  return GetRemoteSSLCertificate(voice_channel()->transport_name());
}

bool PeerConnection::StartRtcEventLog(rtc::PlatformFile file,
                                      int64_t max_size_bytes) {
  // TODO(eladalon): It would be better to not allow negative values into PC.
  const size_t max_size = (max_size_bytes < 0)
                              ? RtcEventLog::kUnlimitedOutput
                              : rtc::saturated_cast<size_t>(max_size_bytes);
  return StartRtcEventLog(
      rtc::MakeUnique<RtcEventLogOutputFile>(file, max_size),
      webrtc::RtcEventLog::kImmediateOutput);
}

bool PeerConnection::StartRtcEventLog(std::unique_ptr<RtcEventLogOutput> output,
                                      int64_t output_period_ms) {
  // TODO(eladalon): In C++14, this can be done with a lambda.
  struct Functor {
    bool operator()() {
      return pc->StartRtcEventLog_w(std::move(output), output_period_ms);
    }
    PeerConnection* const pc;
    std::unique_ptr<RtcEventLogOutput> output;
    const int64_t output_period_ms;
  };
  return worker_thread()->Invoke<bool>(
      RTC_FROM_HERE, Functor{this, std::move(output), output_period_ms});
}

void PeerConnection::StopRtcEventLog() {
  worker_thread()->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&PeerConnection::StopRtcEventLog_w, this));
}

const SessionDescriptionInterface* PeerConnection::local_description() const {
  return pending_local_description_ ? pending_local_description_.get()
                                    : current_local_description_.get();
}

const SessionDescriptionInterface* PeerConnection::remote_description() const {
  return pending_remote_description_ ? pending_remote_description_.get()
                                     : current_remote_description_.get();
}

const SessionDescriptionInterface* PeerConnection::current_local_description()
    const {
  return current_local_description_.get();
}

const SessionDescriptionInterface* PeerConnection::current_remote_description()
    const {
  return current_remote_description_.get();
}

const SessionDescriptionInterface* PeerConnection::pending_local_description()
    const {
  return pending_local_description_.get();
}

const SessionDescriptionInterface* PeerConnection::pending_remote_description()
    const {
  return pending_remote_description_.get();
}

void PeerConnection::Close() {
  TRACE_EVENT0("webrtc", "PeerConnection::Close");
  // Update stats here so that we have the most recent stats for tracks and
  // streams before the channels are closed.
  stats_->UpdateStats(kStatsOutputLevelStandard);

  ChangeSignalingState(PeerConnectionInterface::kClosed);
  RemoveUnusedChannels(nullptr);
  RTC_DCHECK(!GetAudioTransceiver()->internal()->channel());
  RTC_DCHECK(!GetVideoTransceiver()->internal()->channel());
  RTC_DCHECK(!rtp_data_channel_);
  RTC_DCHECK(!sctp_transport_);

  network_thread()->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                port_allocator_.get()));

  worker_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
    call_.reset();
    // The event log must outlive call (and any other object that uses it).
    event_log_.reset();
  });
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

void PeerConnection::CreateAudioReceiver(
    MediaStreamInterface* stream,
    const RtpSenderInfo& remote_sender_info) {
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams;
  streams.push_back(rtc::scoped_refptr<MediaStreamInterface>(stream));
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
      receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
          signaling_thread(),
          new AudioRtpReceiver(remote_sender_info.sender_id, streams,
                               remote_sender_info.first_ssrc, voice_channel()));
  stream->AddTrack(
      static_cast<AudioTrackInterface*>(receiver->internal()->track().get()));
  GetAudioTransceiver()->internal()->AddReceiver(receiver);
  observer_->OnAddTrack(receiver, std::move(streams));
}

void PeerConnection::CreateVideoReceiver(
    MediaStreamInterface* stream,
    const RtpSenderInfo& remote_sender_info) {
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams;
  streams.push_back(rtc::scoped_refptr<MediaStreamInterface>(stream));
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
      receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
          signaling_thread(),
          new VideoRtpReceiver(remote_sender_info.sender_id, streams,
                               worker_thread(), remote_sender_info.first_ssrc,
                               video_channel()));
  stream->AddTrack(
      static_cast<VideoTrackInterface*>(receiver->internal()->track().get()));
  GetVideoTransceiver()->internal()->AddReceiver(receiver);
  observer_->OnAddTrack(receiver, std::move(streams));
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
  auto sender = FindSenderForTrack(track);
  if (sender) {
    // We already have a sender for this track, so just change the stream_id
    // so that it's correct in the next call to CreateOffer.
    sender->internal()->set_stream_id(stream->label());
    return;
  }

  // Normal case; we've never seen this track before.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender =
      RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
          signaling_thread(),
          new AudioRtpSender(track, {stream->label()}, voice_channel(),
                             stats_.get()));
  GetAudioTransceiver()->internal()->AddSender(new_sender);
  // If the sender has already been configured in SDP, we call SetSsrc,
  // which will connect the sender to the underlying transport. This can
  // occur if a local session description that contains the ID of the sender
  // is set before AddStream is called. It can also occur if the local
  // session description is not changed and RemoveStream is called, and
  // later AddStream is called again with the same stream.
  const RtpSenderInfo* sender_info =
      FindSenderInfo(local_audio_sender_infos_, stream->label(), track->id());
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
  auto sender = FindSenderForTrack(track);
  if (sender) {
    // We already have a sender for this track, so just change the stream_id
    // so that it's correct in the next call to CreateOffer.
    sender->internal()->set_stream_id(stream->label());
    return;
  }

  // Normal case; we've never seen this track before.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender =
      RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
          signaling_thread(),
          new VideoRtpSender(track, {stream->label()}, video_channel()));
  GetVideoTransceiver()->internal()->AddSender(new_sender);
  const RtpSenderInfo* sender_info =
      FindSenderInfo(local_video_sender_infos_, stream->label(), track->id());
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
  RTC_DCHECK(signaling_thread()->IsCurrent());
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

void PeerConnection::OnIceCandidate(
    std::unique_ptr<IceCandidateInterface> candidate) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  observer_->OnIceCandidate(candidate.get());
}

void PeerConnection::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  observer_->OnIceCandidatesRemoved(candidates);
}

void PeerConnection::ChangeSignalingState(
    PeerConnectionInterface::SignalingState signaling_state) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (signaling_state_ == signaling_state) {
    return;
  }
  RTC_LOG(LS_INFO) << "Session: " << session_id() << " Old state: "
                   << GetSignalingStateString(signaling_state_)
                   << " New state: "
                   << GetSignalingStateString(signaling_state);
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
  AddAudioTrack(track, stream);
  observer_->OnRenegotiationNeeded();
}

void PeerConnection::OnAudioTrackRemoved(AudioTrackInterface* track,
                                         MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  RemoveAudioTrack(track, stream);
  observer_->OnRenegotiationNeeded();
}

void PeerConnection::OnVideoTrackAdded(VideoTrackInterface* track,
                                       MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  AddVideoTrack(track, stream);
  observer_->OnRenegotiationNeeded();
}

void PeerConnection::OnVideoTrackRemoved(VideoTrackInterface* track,
                                         MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  RemoveVideoTrack(track, stream);
  observer_->OnRenegotiationNeeded();
}

void PeerConnection::PostSetSessionDescriptionSuccess(
    SetSessionDescriptionObserver* observer) {
  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);
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

void PeerConnection::GetOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  ExtractSharedMediaSessionOptions(rtc_options, session_options);

  // Figure out transceiver directional preferences.
  bool send_audio = HasRtpSender(cricket::MEDIA_TYPE_AUDIO);
  bool send_video = HasRtpSender(cricket::MEDIA_TYPE_VIDEO);

  // By default, generate sendrecv/recvonly m= sections.
  bool recv_audio = true;
  bool recv_video = true;

  // By default, only offer a new m= section if we have media to send with it.
  bool offer_new_audio_description = send_audio;
  bool offer_new_video_description = send_video;
  bool offer_new_data_description = HasDataChannels();

  // The "offer_to_receive_X" options allow those defaults to be overridden.
  if (rtc_options.offer_to_receive_audio != RTCOfferAnswerOptions::kUndefined) {
    recv_audio = (rtc_options.offer_to_receive_audio > 0);
    offer_new_audio_description =
        offer_new_audio_description || (rtc_options.offer_to_receive_audio > 0);
  }
  if (rtc_options.offer_to_receive_video != RTCOfferAnswerOptions::kUndefined) {
    recv_video = (rtc_options.offer_to_receive_video > 0);
    offer_new_video_description =
        offer_new_video_description || (rtc_options.offer_to_receive_video > 0);
  }

  rtc::Optional<size_t> audio_index;
  rtc::Optional<size_t> video_index;
  rtc::Optional<size_t> data_index;
  // If a current description exists, generate m= sections in the same order,
  // using the first audio/video/data section that appears and rejecting
  // extraneous ones.
  if (local_description()) {
    GenerateMediaDescriptionOptions(
        local_description(),
        RtpTransceiverDirectionFromSendRecv(send_audio, recv_audio),
        RtpTransceiverDirectionFromSendRecv(send_video, recv_video),
        &audio_index, &video_index, &data_index, session_options);
  }

  // Add audio/video/data m= sections to the end if needed.
  if (!audio_index && offer_new_audio_description) {
    session_options->media_description_options.push_back(
        cricket::MediaDescriptionOptions(
            cricket::MEDIA_TYPE_AUDIO, cricket::CN_AUDIO,
            RtpTransceiverDirectionFromSendRecv(send_audio, recv_audio),
            false));
    audio_index = session_options->media_description_options.size() - 1;
  }
  if (!video_index && offer_new_video_description) {
    session_options->media_description_options.push_back(
        cricket::MediaDescriptionOptions(
            cricket::MEDIA_TYPE_VIDEO, cricket::CN_VIDEO,
            RtpTransceiverDirectionFromSendRecv(send_video, recv_video),
            false));
    video_index = session_options->media_description_options.size() - 1;
  }
  if (!data_index && offer_new_data_description) {
    session_options->media_description_options.push_back(
        cricket::MediaDescriptionOptions(
            cricket::MEDIA_TYPE_DATA, cricket::CN_DATA,
            RtpTransceiverDirection::kSendRecv, false));
    data_index = session_options->media_description_options.size() - 1;
  }

  cricket::MediaDescriptionOptions* audio_media_description_options =
      !audio_index ? nullptr
                   : &session_options->media_description_options[*audio_index];
  cricket::MediaDescriptionOptions* video_media_description_options =
      !video_index ? nullptr
                   : &session_options->media_description_options[*video_index];
  cricket::MediaDescriptionOptions* data_media_description_options =
      !data_index ? nullptr
                  : &session_options->media_description_options[*data_index];

  // Apply ICE restart flag and renomination flag.
  for (auto& options : session_options->media_description_options) {
    options.transport_options.ice_restart = rtc_options.ice_restart;
    options.transport_options.enable_ice_renomination =
        configuration_.enable_ice_renomination;
  }

  AddRtpSenderOptions(GetSendersInternal(), audio_media_description_options,
                      video_media_description_options);
  AddRtpDataChannelOptions(rtp_data_channels_, data_media_description_options);

  // Intentionally unset the data channel type for RTP data channel with the
  // second condition. Otherwise the RTP data channels would be successfully
  // negotiated by default and the unit tests in WebRtcDataBrowserTest will fail
  // when building with chromium. We want to leave RTP data channels broken, so
  // people won't try to use them.
  if (!rtp_data_channels_.empty() || data_channel_type() != cricket::DCT_RTP) {
    session_options->data_channel_type = data_channel_type();
  }

  session_options->rtcp_cname = rtcp_cname_;
  session_options->crypto_options = factory_->options().crypto_options;
}

void PeerConnection::GetOptionsForAnswer(
    const RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  ExtractSharedMediaSessionOptions(rtc_options, session_options);

  // Figure out transceiver directional preferences.
  bool send_audio = HasRtpSender(cricket::MEDIA_TYPE_AUDIO);
  bool send_video = HasRtpSender(cricket::MEDIA_TYPE_VIDEO);

  // By default, generate sendrecv/recvonly m= sections. The direction is also
  // restricted by the direction in the offer.
  bool recv_audio = true;
  bool recv_video = true;

  // The "offer_to_receive_X" options allow those defaults to be overridden.
  if (rtc_options.offer_to_receive_audio != RTCOfferAnswerOptions::kUndefined) {
    recv_audio = (rtc_options.offer_to_receive_audio > 0);
  }
  if (rtc_options.offer_to_receive_video != RTCOfferAnswerOptions::kUndefined) {
    recv_video = (rtc_options.offer_to_receive_video > 0);
  }

  rtc::Optional<size_t> audio_index;
  rtc::Optional<size_t> video_index;
  rtc::Optional<size_t> data_index;
  if (remote_description()) {
    // The pending remote description should be an offer.
    RTC_DCHECK(remote_description()->type() ==
               SessionDescriptionInterface::kOffer);
    // Generate m= sections that match those in the offer.
    // Note that mediasession.cc will handle intersection our preferred
    // direction with the offered direction.
    GenerateMediaDescriptionOptions(
        remote_description(),
        RtpTransceiverDirectionFromSendRecv(send_audio, recv_audio),
        RtpTransceiverDirectionFromSendRecv(send_video, recv_video),
        &audio_index, &video_index, &data_index, session_options);
  }

  cricket::MediaDescriptionOptions* audio_media_description_options =
      !audio_index ? nullptr
                   : &session_options->media_description_options[*audio_index];
  cricket::MediaDescriptionOptions* video_media_description_options =
      !video_index ? nullptr
                   : &session_options->media_description_options[*video_index];
  cricket::MediaDescriptionOptions* data_media_description_options =
      !data_index ? nullptr
                  : &session_options->media_description_options[*data_index];

  // Apply ICE renomination flag.
  for (auto& options : session_options->media_description_options) {
    options.transport_options.enable_ice_renomination =
        configuration_.enable_ice_renomination;
  }

  AddRtpSenderOptions(GetSendersInternal(), audio_media_description_options,
                      video_media_description_options);
  AddRtpDataChannelOptions(rtp_data_channels_, data_media_description_options);

  // Intentionally unset the data channel type for RTP data channel. Otherwise
  // the RTP data channels would be successfully negotiated by default and the
  // unit tests in WebRtcDataBrowserTest will fail when building with chromium.
  // We want to leave RTP data channels broken, so people won't try to use them.
  if (!rtp_data_channels_.empty() || data_channel_type() != cricket::DCT_RTP) {
    session_options->data_channel_type = data_channel_type();
  }

  session_options->rtcp_cname = rtcp_cname_;
  session_options->crypto_options = factory_->options().crypto_options;
}

void PeerConnection::GenerateMediaDescriptionOptions(
    const SessionDescriptionInterface* session_desc,
    RtpTransceiverDirection audio_direction,
    RtpTransceiverDirection video_direction,
    rtc::Optional<size_t>* audio_index,
    rtc::Optional<size_t>* video_index,
    rtc::Optional<size_t>* data_index,
    cricket::MediaSessionOptions* session_options) {
  for (const cricket::ContentInfo& content :
       session_desc->description()->contents()) {
    if (IsAudioContent(&content)) {
      // If we already have an audio m= section, reject this extra one.
      if (*audio_index) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_AUDIO, content.name,
                RtpTransceiverDirection::kInactive, true));
      } else {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_AUDIO, content.name, audio_direction,
                audio_direction == RtpTransceiverDirection::kInactive));
        *audio_index = session_options->media_description_options.size() - 1;
      }
    } else if (IsVideoContent(&content)) {
      // If we already have an video m= section, reject this extra one.
      if (*video_index) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_VIDEO, content.name,
                RtpTransceiverDirection::kInactive, true));
      } else {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_VIDEO, content.name, video_direction,
                video_direction == RtpTransceiverDirection::kInactive));
        *video_index = session_options->media_description_options.size() - 1;
      }
    } else {
      RTC_DCHECK(IsDataContent(&content));
      // If we already have an data m= section, reject this extra one.
      if (*data_index) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_DATA, content.name,
                RtpTransceiverDirection::kInactive, true));
      } else {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_DATA, content.name,
                // Direction for data sections is meaningless, but legacy
                // endpoints might expect sendrecv.
                RtpTransceiverDirection::kSendRecv, false));
        *data_index = session_options->media_description_options.size() - 1;
      }
    }
  }
}

void PeerConnection::RemoveSenders(cricket::MediaType media_type) {
  UpdateLocalSenders(std::vector<cricket::StreamParams>(), media_type);
  UpdateRemoteSendersList(std::vector<cricket::StreamParams>(), false,
                          media_type, nullptr);
}

void PeerConnection::UpdateRemoteSendersList(
    const cricket::StreamParamsVec& streams,
    bool default_sender_needed,
    cricket::MediaType media_type,
    StreamCollection* new_streams) {
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
    bool sender_exists = params && params->id == info.sender_id;
    // If this is a default track, and we still need it, don't remove it.
    if ((info.stream_label == kDefaultStreamLabel && default_sender_needed) ||
        sender_exists) {
      ++sender_it;
    } else {
      OnRemoteSenderRemoved(info, media_type);
      sender_it = current_senders->erase(sender_it);
    }
  }

  // Find new and active senders.
  for (const cricket::StreamParams& params : streams) {
    // The sync_label is the MediaStream label and the |stream.id| is the
    // sender id.
    const std::string& stream_label = params.sync_label;
    const std::string& sender_id = params.id;
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

    const RtpSenderInfo* sender_info =
        FindSenderInfo(*current_senders, stream_label, sender_id);
    if (!sender_info) {
      current_senders->push_back(RtpSenderInfo(stream_label, sender_id, ssrc));
      OnRemoteSenderAdded(current_senders->back(), media_type);
    }
  }

  // Add default sender if necessary.
  if (default_sender_needed) {
    rtc::scoped_refptr<MediaStreamInterface> default_stream =
        remote_streams_->find(kDefaultStreamLabel);
    if (!default_stream) {
      // Create the new default MediaStream.
      default_stream = MediaStreamProxy::Create(
          rtc::Thread::Current(), MediaStream::Create(kDefaultStreamLabel));
      remote_streams_->AddStream(default_stream);
      new_streams->AddStream(default_stream);
    }
    std::string default_sender_id = (media_type == cricket::MEDIA_TYPE_AUDIO)
                                        ? kDefaultAudioSenderId
                                        : kDefaultVideoSenderId;
    const RtpSenderInfo* default_sender_info = FindSenderInfo(
        *current_senders, kDefaultStreamLabel, default_sender_id);
    if (!default_sender_info) {
      current_senders->push_back(
          RtpSenderInfo(kDefaultStreamLabel, default_sender_id, 0));
      OnRemoteSenderAdded(current_senders->back(), media_type);
    }
  }
}

void PeerConnection::OnRemoteSenderAdded(const RtpSenderInfo& sender_info,
                                         cricket::MediaType media_type) {
  MediaStreamInterface* stream =
      remote_streams_->find(sender_info.stream_label);

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
  MediaStreamInterface* stream =
      remote_streams_->find(sender_info.stream_label);

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
    observer_->OnRemoveTrack(receiver);
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
    observer_->OnRemoveStream(std::move(stream));
  }
}

void PeerConnection::UpdateLocalSenders(
    const std::vector<cricket::StreamParams>& streams,
    cricket::MediaType media_type) {
  std::vector<RtpSenderInfo>* current_senders = GetLocalSenderInfos(media_type);

  // Find removed tracks. I.e., tracks where the track id, stream label or ssrc
  // don't match the new StreamParam.
  for (auto sender_it = current_senders->begin();
       sender_it != current_senders->end();
       /* incremented manually */) {
    const RtpSenderInfo& info = *sender_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.first_ssrc);
    if (!params || params->id != info.sender_id ||
        params->sync_label != info.stream_label) {
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
    const std::string& stream_label = params.sync_label;
    const std::string& sender_id = params.id;
    uint32_t ssrc = params.first_ssrc();
    const RtpSenderInfo* sender_info =
        FindSenderInfo(*current_senders, stream_label, sender_id);
    if (!sender_info) {
      current_senders->push_back(RtpSenderInfo(stream_label, sender_id, ssrc));
      OnLocalSenderAdded(current_senders->back(), media_type);
    }
  }
}

void PeerConnection::OnLocalSenderAdded(const RtpSenderInfo& sender_info,
                                        cricket::MediaType media_type) {
  auto sender = FindSenderById(sender_info.sender_id);
  if (!sender) {
    RTC_LOG(LS_WARNING) << "An unknown RtpSender with id "
                        << sender_info.sender_id
                        << " has been configured in the local description.";
    return;
  }

  if (sender->media_type() != media_type) {
    RTC_LOG(LS_WARNING) << "An RtpSender has been configured in the local"
                        << " description with an unexpected media type.";
    return;
  }

  sender->internal()->set_stream_id(sender_info.stream_label);
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
                        << " description with an unexpected media type.";
    return;
  }

  sender->internal()->SetSsrc(0);
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
      RTC_LOG(LS_ERROR) << "channel label not found";
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
    RTC_LOG(LS_WARNING) << "Remote peer requested a DataChannel but"
                        << "CreateDataChannel failed.";
    return;
  }
  channel->SetReceiveSsrc(remote_ssrc);
  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  observer_->OnDataChannel(std::move(proxy_channel));
}

rtc::scoped_refptr<DataChannel> PeerConnection::InternalCreateDataChannel(
    const std::string& label,
    const InternalDataChannelInit* config) {
  if (IsClosed()) {
    return nullptr;
  }
  if (data_channel_type() == cricket::DCT_NONE) {
    RTC_LOG(LS_ERROR)
        << "InternalCreateDataChannel: Data is not supported in this call.";
    return nullptr;
  }
  InternalDataChannelInit new_config =
      config ? (*config) : InternalDataChannelInit();
  if (data_channel_type() == cricket::DCT_SCTP) {
    if (new_config.id < 0) {
      rtc::SSLRole role;
      if ((GetSctpSslRole(&role)) &&
          !sid_allocator_.AllocateSid(role, &new_config.id)) {
        RTC_LOG(LS_ERROR)
            << "No id can be allocated for the SCTP data channel.";
        return nullptr;
      }
    } else if (!sid_allocator_.ReserveSid(new_config.id)) {
      RTC_LOG(LS_ERROR) << "Failed to create a SCTP data channel "
                        << "because the id is already in use or out of range.";
      return nullptr;
    }
  }

  rtc::scoped_refptr<DataChannel> channel(
      DataChannel::Create(this, data_channel_type(), label, new_config));
  if (!channel) {
    sid_allocator_.ReleaseSid(new_config.id);
    return nullptr;
  }

  if (channel->data_channel_type() == cricket::DCT_RTP) {
    if (rtp_data_channels_.find(channel->label()) != rtp_data_channels_.end()) {
      RTC_LOG(LS_ERROR) << "DataChannel with label " << channel->label()
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
  return !rtp_data_channels_.empty() || !sctp_data_channels_.empty();
}

void PeerConnection::AllocateSctpSids(rtc::SSLRole role) {
  for (const auto& channel : sctp_data_channels_) {
    if (channel->id() < 0) {
      int sid;
      if (!sid_allocator_.AllocateSid(role, &sid)) {
        RTC_LOG(LS_ERROR) << "Failed to allocate SCTP sid.";
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
    RTC_LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message.";
    return;
  }

  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  observer_->OnDataChannel(std::move(proxy_channel));
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetAudioTransceiver() const {
  // This method only works with Plan B SDP, where there is a single
  // audio/video transceiver.
  RTC_DCHECK(!IsUnifiedPlan());
  for (auto transceiver : transceivers_) {
    if (transceiver->internal()->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      return transceiver;
    }
  }
  RTC_NOTREACHED();
  return nullptr;
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetVideoTransceiver() const {
  // This method only works with Plan B SDP, where there is a single
  // audio/video transceiver.
  RTC_DCHECK(!IsUnifiedPlan());
  for (auto transceiver : transceivers_) {
    if (transceiver->internal()->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      return transceiver;
    }
  }
  RTC_NOTREACHED();
  return nullptr;
}

// TODO(bugs.webrtc.org/7600): Remove this when multiple transceivers with
// individual transceiver directions are supported.
bool PeerConnection::HasRtpSender(cricket::MediaType type) const {
  switch (type) {
    case cricket::MEDIA_TYPE_AUDIO:
      return !GetAudioTransceiver()->internal()->senders().empty();
    case cricket::MEDIA_TYPE_VIDEO:
      return !GetVideoTransceiver()->internal()->senders().empty();
    case cricket::MEDIA_TYPE_DATA:
      return false;
  }
  RTC_NOTREACHED();
  return false;
}

rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
PeerConnection::FindSenderForTrack(MediaStreamTrackInterface* track) const {
  for (auto transceiver : transceivers_) {
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
  for (auto transceiver : transceivers_) {
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
  for (auto transceiver : transceivers_) {
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
    const std::string& stream_label,
    const std::string sender_id) const {
  for (const RtpSenderInfo& sender_info : infos) {
    if (sender_info.stream_label == stream_label &&
        sender_info.sender_id == sender_id) {
      return &sender_info;
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
                         cricket::PORTALLOCATOR_ENABLE_IPV6 |
                         cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI;
  // If the disable-IPv6 flag was specified, we'll not override it
  // by experiment.
  if (configuration.disable_ipv6) {
    portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  } else if (webrtc::field_trial::FindFullName("WebRTC-IPv6Default")
                 .find("Disabled") == 0) {
    portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  }

  if (configuration.disable_ipv6_on_wifi) {
    portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI);
    RTC_LOG(LS_INFO) << "IPv6 candidates on Wi-Fi are disabled.";
  }

  if (configuration.tcp_candidate_policy == kTcpCandidatePolicyDisabled) {
    portallocator_flags |= cricket::PORTALLOCATOR_DISABLE_TCP;
    RTC_LOG(LS_INFO) << "TCP candidates are disabled.";
  }

  if (configuration.candidate_network_policy ==
      kCandidateNetworkPolicyLowCost) {
    portallocator_flags |= cricket::PORTALLOCATOR_DISABLE_COSTLY_NETWORKS;
    RTC_LOG(LS_INFO) << "Do not gather candidates on high-cost networks";
  }

  port_allocator_->set_flags(portallocator_flags);
  // No step delay is used while allocating ports.
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);
  port_allocator_->set_candidate_filter(
      ConvertIceTransportTypeToCandidateFilter(configuration.type));
  port_allocator_->set_max_ipv6_networks(configuration.max_ipv6_networks);

  // Call this last since it may create pooled allocator sessions using the
  // properties set above.
  port_allocator_->SetConfiguration(stun_servers, turn_servers,
                                    configuration.ice_candidate_pool_size,
                                    configuration.prune_turn_ports,
                                    configuration.turn_customizer);
  return true;
}

bool PeerConnection::ReconfigurePortAllocator_n(
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    IceTransportsType type,
    int candidate_pool_size,
    bool prune_turn_ports,
    webrtc::TurnCustomizer* turn_customizer) {
  port_allocator_->set_candidate_filter(
      ConvertIceTransportTypeToCandidateFilter(type));
  // Call this last since it may create pooled allocator sessions using the
  // candidate filter set above.
  return port_allocator_->SetConfiguration(
      stun_servers, turn_servers, candidate_pool_size, prune_turn_ports,
      turn_customizer);
}

cricket::ChannelManager* PeerConnection::channel_manager() const {
  return factory_->channel_manager();
}

MetricsObserverInterface* PeerConnection::metrics_observer() const {
  return uma_observer_;
}

bool PeerConnection::StartRtcEventLog_w(
    std::unique_ptr<RtcEventLogOutput> output,
    int64_t output_period_ms) {
  if (!event_log_) {
    return false;
  }
  return event_log_->StartLogging(std::move(output), output_period_ms);
}

void PeerConnection::StopRtcEventLog_w() {
  if (event_log_) {
    event_log_->StopLogging();
  }
}

cricket::BaseChannel* PeerConnection::GetChannel(
    const std::string& content_name) {
  if (voice_channel() && voice_channel()->content_name() == content_name) {
    return voice_channel();
  }
  if (video_channel() && video_channel()->content_name() == content_name) {
    return video_channel();
  }
  if (rtp_data_channel() &&
      rtp_data_channel()->content_name() == content_name) {
    return rtp_data_channel();
  }
  return nullptr;
}

bool PeerConnection::GetSctpSslRole(rtc::SSLRole* role) {
  if (!local_description() || !remote_description()) {
    RTC_LOG(LS_INFO)
        << "Local and Remote descriptions must be applied to get the "
        << "SSL Role of the SCTP transport.";
    return false;
  }
  if (!sctp_transport_) {
    RTC_LOG(LS_INFO) << "Non-rejected SCTP m= section is needed to get the "
                     << "SSL Role of the SCTP transport.";
    return false;
  }

  return transport_controller_->GetSslRole(*sctp_transport_name_, role);
}

bool PeerConnection::GetSslRole(const std::string& content_name,
                                rtc::SSLRole* role) {
  if (!local_description() || !remote_description()) {
    RTC_LOG(LS_INFO)
        << "Local and Remote descriptions must be applied to get the "
        << "SSL Role of the session.";
    return false;
  }

  return transport_controller_->GetSslRole(GetTransportName(content_name),
                                           role);
}

bool PeerConnection::SetCurrentOrPendingLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    std::string* err_desc) {
  RTC_DCHECK(signaling_thread()->IsCurrent());

  // Validate SDP.
  if (!ValidateSessionDescription(desc.get(), cricket::CS_LOCAL, err_desc)) {
    return false;
  }

  // Update the initial_offerer flag if this session is the initial_offerer.
  Action action = GetAction(desc->type());
  if (!initial_offerer_.has_value()) {
    initial_offerer_.emplace(action == kOffer);
    if (*initial_offerer_) {
      transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLING);
    } else {
      transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLED);
    }
  }

  if (action == kAnswer) {
    current_local_description_ = std::move(desc);
    pending_local_description_ = nullptr;
    current_remote_description_ = std::move(pending_remote_description_);
  } else {
    pending_local_description_ = std::move(desc);
  }

  // Transport and Media channels will be created only when offer is set.
  if (action == kOffer && !CreateChannels(local_description()->description())) {
    // TODO(mallinath) - Handle CreateChannel failure, as new local description
    // is applied. Restore back to old description.
    return BadLocalSdp(local_description()->type(), kCreateChannelFailed,
                       err_desc);
  }

  // Remove unused channels if MediaContentDescription is rejected.
  RemoveUnusedChannels(local_description()->description());

  if (!UpdateSessionState(action, cricket::CS_LOCAL, err_desc)) {
    return false;
  }
  if (remote_description()) {
    // Now that we have a local description, we can push down remote candidates.
    UseCandidatesInSessionDescription(remote_description());
  }

  pending_ice_restarts_.clear();
  if (error() != ERROR_NONE) {
    return BadLocalSdp(local_description()->type(), GetSessionErrorMsg(),
                       err_desc);
  }
  return true;
}

bool PeerConnection::SetCurrentOrPendingRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    std::string* err_desc) {
  RTC_DCHECK(signaling_thread()->IsCurrent());

  // Validate SDP.
  if (!ValidateSessionDescription(desc.get(), cricket::CS_REMOTE, err_desc)) {
    return false;
  }

  // Hold this pointer so candidates can be copied to it later in the method.
  SessionDescriptionInterface* desc_ptr = desc.get();

  const SessionDescriptionInterface* old_remote_description =
      remote_description();
  // Grab ownership of the description being replaced for the remainder of this
  // method, since it's used below as |old_remote_description|.
  std::unique_ptr<SessionDescriptionInterface> replaced_remote_description;
  Action action = GetAction(desc->type());
  if (action == kAnswer) {
    replaced_remote_description = pending_remote_description_
                                      ? std::move(pending_remote_description_)
                                      : std::move(current_remote_description_);
    current_remote_description_ = std::move(desc);
    pending_remote_description_ = nullptr;
    current_local_description_ = std::move(pending_local_description_);
  } else {
    replaced_remote_description = std::move(pending_remote_description_);
    pending_remote_description_ = std::move(desc);
  }

  // Transport and Media channels will be created only when offer is set.
  if (action == kOffer &&
      !CreateChannels(remote_description()->description())) {
    // TODO(mallinath) - Handle CreateChannel failure, as new local description
    // is applied. Restore back to old description.
    return BadRemoteSdp(remote_description()->type(), kCreateChannelFailed,
                        err_desc);
  }

  // Remove unused channels if MediaContentDescription is rejected.
  RemoveUnusedChannels(remote_description()->description());

  // NOTE: Candidates allocation will be initiated only when SetLocalDescription
  // is called.
  if (!UpdateSessionState(action, cricket::CS_REMOTE, err_desc)) {
    return false;
  }

  if (local_description() &&
      !UseCandidatesInSessionDescription(remote_description())) {
    return BadRemoteSdp(remote_description()->type(), kInvalidCandidates,
                        err_desc);
  }

  if (old_remote_description) {
    for (const cricket::ContentInfo& content :
         old_remote_description->description()->contents()) {
      // Check if this new SessionDescription contains new ICE ufrag and
      // password that indicates the remote peer requests an ICE restart.
      // TODO(deadbeef): When we start storing both the current and pending
      // remote description, this should reset pending_ice_restarts and compare
      // against the current description.
      if (CheckForRemoteIceRestart(old_remote_description, remote_description(),
                                   content.name)) {
        if (action == kOffer) {
          pending_ice_restarts_.insert(content.name);
        }
      } else {
        // We retain all received candidates only if ICE is not restarted.
        // When ICE is restarted, all previous candidates belong to an old
        // generation and should not be kept.
        // TODO(deadbeef): This goes against the W3C spec which says the remote
        // description should only contain candidates from the last set remote
        // description plus any candidates added since then. We should remove
        // this once we're sure it won't break anything.
        WebRtcSessionDescriptionFactory::CopyCandidatesFromSessionDescription(
            old_remote_description, content.name, desc_ptr);
      }
    }
  }

  if (error() != ERROR_NONE) {
    return BadRemoteSdp(remote_description()->type(), GetSessionErrorMsg(),
                        err_desc);
  }

  // Set the the ICE connection state to connecting since the connection may
  // become writable with peer reflexive candidates before any remote candidate
  // is signaled.
  // TODO(pthatcher): This is a short-term solution for crbug/446908. A real fix
  // is to have a new signal the indicates a change in checking state from the
  // transport and expose a new checking() member from transport that can be
  // read to determine the current checking state. The existing SignalConnecting
  // actually means "gathering candidates", so cannot be be used here.
  if (remote_description()->type() != SessionDescriptionInterface::kOffer &&
      ice_connection_state() == PeerConnectionInterface::kIceConnectionNew) {
    SetIceConnectionState(PeerConnectionInterface::kIceConnectionChecking);
  }
  return true;
}

// TODO(steveanton): Eventually it'd be nice to store the channels as a single
// vector of BaseChannel pointers instead of separate voice and video channel
// vectors. At that point, this will become a simple getter.
std::vector<cricket::BaseChannel*> PeerConnection::Channels() const {
  std::vector<cricket::BaseChannel*> channels;
  if (voice_channel()) {
    channels.push_back(voice_channel());
  }
  if (video_channel()) {
    channels.push_back(video_channel());
  }
  if (rtp_data_channel_) {
    channels.push_back(rtp_data_channel_);
  }
  return channels;
}

void PeerConnection::SetError(Error error, const std::string& error_desc) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (error != error_) {
    error_ = error;
    error_desc_ = error_desc;
  }
}

bool PeerConnection::UpdateSessionState(Action action,
                                        cricket::ContentSource source,
                                        std::string* err_desc) {
  RTC_DCHECK(signaling_thread()->IsCurrent());

  // If there's already a pending error then no state transition should happen.
  // But all call-sites should be verifying this before calling us!
  RTC_DCHECK(error() == ERROR_NONE);
  std::string td_err;
  if (action == kOffer) {
    if (!PushdownTransportDescription(source, cricket::CA_OFFER, &td_err)) {
      return BadOfferSdp(source, MakeTdErrorString(td_err), err_desc);
    }
    ChangeSignalingState(source == cricket::CS_LOCAL
                             ? PeerConnectionInterface::kHaveLocalOffer
                             : PeerConnectionInterface::kHaveRemoteOffer);
    if (!PushdownMediaDescription(cricket::CA_OFFER, source, err_desc)) {
      SetError(ERROR_CONTENT, *err_desc);
    }
    if (error() != ERROR_NONE) {
      return BadOfferSdp(source, GetSessionErrorMsg(), err_desc);
    }
  } else if (action == kPrAnswer) {
    if (!PushdownTransportDescription(source, cricket::CA_PRANSWER, &td_err)) {
      return BadPranswerSdp(source, MakeTdErrorString(td_err), err_desc);
    }
    EnableChannels();
    ChangeSignalingState(source == cricket::CS_LOCAL
                             ? PeerConnectionInterface::kHaveLocalPrAnswer
                             : PeerConnectionInterface::kHaveRemotePrAnswer);
    if (!PushdownMediaDescription(cricket::CA_PRANSWER, source, err_desc)) {
      SetError(ERROR_CONTENT, *err_desc);
    }
    if (error() != ERROR_NONE) {
      return BadPranswerSdp(source, GetSessionErrorMsg(), err_desc);
    }
  } else if (action == kAnswer) {
    const cricket::ContentGroup* local_bundle =
        local_description()->description()->GetGroupByName(
            cricket::GROUP_TYPE_BUNDLE);
    const cricket::ContentGroup* remote_bundle =
        remote_description()->description()->GetGroupByName(
            cricket::GROUP_TYPE_BUNDLE);
    if (local_bundle && remote_bundle) {
      // The answerer decides the transport to bundle on.
      const cricket::ContentGroup* answer_bundle =
          (source == cricket::CS_LOCAL ? local_bundle : remote_bundle);
      if (!EnableBundle(*answer_bundle)) {
        RTC_LOG(LS_WARNING) << "Failed to enable BUNDLE.";
        return BadAnswerSdp(source, kEnableBundleFailed, err_desc);
      }
    }
    // Only push down the transport description after enabling BUNDLE; we don't
    // want to push down a description on a transport about to be destroyed.
    if (!PushdownTransportDescription(source, cricket::CA_ANSWER, &td_err)) {
      return BadAnswerSdp(source, MakeTdErrorString(td_err), err_desc);
    }
    EnableChannels();
    ChangeSignalingState(PeerConnectionInterface::kStable);
    if (!PushdownMediaDescription(cricket::CA_ANSWER, source, err_desc)) {
      SetError(ERROR_CONTENT, *err_desc);
    }
    if (error() != ERROR_NONE) {
      return BadAnswerSdp(source, GetSessionErrorMsg(), err_desc);
    }
  }
  return true;
}

PeerConnection::Action PeerConnection::GetAction(const std::string& type) {
  if (type == SessionDescriptionInterface::kOffer) {
    return PeerConnection::kOffer;
  } else if (type == SessionDescriptionInterface::kPrAnswer) {
    return PeerConnection::kPrAnswer;
  } else if (type == SessionDescriptionInterface::kAnswer) {
    return PeerConnection::kAnswer;
  }
  RTC_NOTREACHED() << "unknown action type";
  return PeerConnection::kOffer;
}

bool PeerConnection::PushdownMediaDescription(cricket::ContentAction action,
                                              cricket::ContentSource source,
                                              std::string* err) {
  const SessionDescription* sdesc =
      (source == cricket::CS_LOCAL ? local_description() : remote_description())
          ->description();
  RTC_DCHECK(sdesc);
  bool all_success = true;
  for (auto* channel : Channels()) {
    // TODO(steveanton): Add support for multiple channels of the same type.
    const ContentInfo* content_info =
        cricket::GetFirstMediaContent(sdesc->contents(), channel->media_type());
    if (!content_info) {
      continue;
    }
    const MediaContentDescription* content_desc =
        static_cast<const MediaContentDescription*>(content_info->description);
    if (content_desc && !content_info->rejected) {
      bool success = (source == cricket::CS_LOCAL)
                         ? channel->SetLocalContent(content_desc, action, err)
                         : channel->SetRemoteContent(content_desc, action, err);
      if (!success) {
        all_success = false;
        break;
      }
    }
  }
  // Need complete offer/answer with an SCTP m= section before starting SCTP,
  // according to https://tools.ietf.org/html/draft-ietf-mmusic-sctp-sdp-19
  if (sctp_transport_ && local_description() && remote_description() &&
      cricket::GetFirstDataContent(local_description()->description()) &&
      cricket::GetFirstDataContent(remote_description()->description())) {
    all_success &= network_thread()->Invoke<bool>(
        RTC_FROM_HERE,
        rtc::Bind(&PeerConnection::PushdownSctpParameters_n, this, source));
  }
  return all_success;
}

bool PeerConnection::PushdownSctpParameters_n(cricket::ContentSource source) {
  RTC_DCHECK(network_thread()->IsCurrent());
  RTC_DCHECK(local_description());
  RTC_DCHECK(remote_description());
  // Apply the SCTP port (which is hidden inside a DataCodec structure...)
  // When we support "max-message-size", that would also be pushed down here.
  return sctp_transport_->Start(
      GetSctpPort(local_description()->description()),
      GetSctpPort(remote_description()->description()));
}

bool PeerConnection::PushdownTransportDescription(cricket::ContentSource source,
                                                  cricket::ContentAction action,
                                                  std::string* error_desc) {
  RTC_DCHECK(signaling_thread()->IsCurrent());

  if (source == cricket::CS_LOCAL) {
    return PushdownLocalTransportDescription(local_description()->description(),
                                             action, error_desc);
  }
  return PushdownRemoteTransportDescription(remote_description()->description(),
                                            action, error_desc);
}

bool PeerConnection::PushdownLocalTransportDescription(
    const SessionDescription* sdesc,
    cricket::ContentAction action,
    std::string* err) {
  RTC_DCHECK(signaling_thread()->IsCurrent());

  if (!sdesc) {
    return false;
  }

  for (const TransportInfo& tinfo : sdesc->transport_infos()) {
    if (!transport_controller_->SetLocalTransportDescription(
            tinfo.content_name, tinfo.description, action, err)) {
      return false;
    }
  }

  return true;
}

bool PeerConnection::PushdownRemoteTransportDescription(
    const SessionDescription* sdesc,
    cricket::ContentAction action,
    std::string* err) {
  RTC_DCHECK(signaling_thread()->IsCurrent());

  if (!sdesc) {
    return false;
  }

  for (const TransportInfo& tinfo : sdesc->transport_infos()) {
    if (!transport_controller_->SetRemoteTransportDescription(
            tinfo.content_name, tinfo.description, action, err)) {
      return false;
    }
  }

  return true;
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

bool PeerConnection::EnableBundle(const cricket::ContentGroup& bundle) {
  const std::string* first_content_name = bundle.FirstContentName();
  if (!first_content_name) {
    RTC_LOG(LS_WARNING) << "Tried to BUNDLE with no contents.";
    return false;
  }
  const std::string& transport_name = *first_content_name;

  auto maybe_set_transport = [this, bundle,
                              transport_name](cricket::BaseChannel* ch) {
    if (!ch || !bundle.HasContentName(ch->content_name())) {
      return true;
    }

    std::string old_transport_name = ch->transport_name();
    if (old_transport_name == transport_name) {
      RTC_LOG(LS_INFO) << "BUNDLE already enabled for " << ch->content_name()
                       << " on " << transport_name << ".";
      return true;
    }

    cricket::DtlsTransportInternal* rtp_dtls_transport =
        transport_controller_->CreateDtlsTransport(
            transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
    bool need_rtcp = (ch->rtcp_dtls_transport() != nullptr);
    cricket::DtlsTransportInternal* rtcp_dtls_transport = nullptr;
    if (need_rtcp) {
      rtcp_dtls_transport = transport_controller_->CreateDtlsTransport(
          transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
    }

    ch->SetTransports(rtp_dtls_transport, rtcp_dtls_transport);
    RTC_LOG(LS_INFO) << "Enabled BUNDLE for " << ch->content_name() << " on "
                     << transport_name << ".";
    transport_controller_->DestroyDtlsTransport(
        old_transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
    // If the channel needs rtcp, it means that the channel used to have a
    // rtcp transport which needs to be deleted now.
    if (need_rtcp) {
      transport_controller_->DestroyDtlsTransport(
          old_transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
    }
    return true;
  };

  if (!maybe_set_transport(voice_channel()) ||
      !maybe_set_transport(video_channel()) ||
      !maybe_set_transport(rtp_data_channel())) {
    return false;
  }
  // For SCTP, transport creation/deletion happens here instead of in the
  // object itself.
  if (sctp_transport_) {
    RTC_DCHECK(sctp_transport_name_);
    RTC_DCHECK(sctp_content_name_);
    if (transport_name != *sctp_transport_name_ &&
        bundle.HasContentName(*sctp_content_name_)) {
      network_thread()->Invoke<void>(
          RTC_FROM_HERE, rtc::Bind(&PeerConnection::ChangeSctpTransport_n, this,
                                   transport_name));
    }
  }

  return true;
}

cricket::IceConfig PeerConnection::ParseIceConfig(
    const PeerConnectionInterface::RTCConfiguration& config) const {
  cricket::ContinualGatheringPolicy gathering_policy;
  // TODO(honghaiz): Add the third continual gathering policy in
  // PeerConnectionInterface and map it to GATHER_CONTINUALLY_AND_RECOVER.
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
  ice_config.receiving_timeout = config.ice_connection_receiving_timeout;
  ice_config.prioritize_most_likely_candidate_pairs =
      config.prioritize_most_likely_ice_candidate_pairs;
  ice_config.backup_connection_ping_interval =
      config.ice_backup_candidate_pair_ping_interval;
  ice_config.continual_gathering_policy = gathering_policy;
  ice_config.presume_writable_when_fully_relayed =
      config.presume_writable_when_fully_relayed;
  ice_config.ice_check_min_interval = config.ice_check_min_interval;
  ice_config.regather_all_networks_interval_range =
      config.ice_regather_interval_range;
  return ice_config;
}

bool PeerConnection::GetLocalTrackIdBySsrc(uint32_t ssrc,
                                           std::string* track_id) {
  if (!local_description()) {
    return false;
  }
  return webrtc::GetTrackIdBySsrc(local_description()->description(), ssrc,
                                  track_id);
}

bool PeerConnection::GetRemoteTrackIdBySsrc(uint32_t ssrc,
                                            std::string* track_id) {
  if (!remote_description()) {
    return false;
  }
  return webrtc::GetTrackIdBySsrc(remote_description()->description(), ssrc,
                                  track_id);
}

bool PeerConnection::SendData(const cricket::SendDataParams& params,
                              const rtc::CopyOnWriteBuffer& payload,
                              cricket::SendDataResult* result) {
  if (!rtp_data_channel_ && !sctp_transport_) {
    RTC_LOG(LS_ERROR) << "SendData called when rtp_data_channel_ "
                      << "and sctp_transport_ are NULL.";
    return false;
  }
  return rtp_data_channel_
             ? rtp_data_channel_->SendData(params, payload, result)
             : network_thread()->Invoke<bool>(
                   RTC_FROM_HERE,
                   Bind(&cricket::SctpTransportInternal::SendData,
                        sctp_transport_.get(), params, payload, result));
}

bool PeerConnection::ConnectDataChannel(DataChannel* webrtc_data_channel) {
  if (!rtp_data_channel_ && !sctp_transport_) {
    // Don't log an error here, because DataChannels are expected to call
    // ConnectDataChannel in this state. It's the only way to initially tell
    // whether or not the underlying transport is ready.
    return false;
  }
  if (rtp_data_channel_) {
    rtp_data_channel_->SignalReadyToSendData.connect(
        webrtc_data_channel, &DataChannel::OnChannelReady);
    rtp_data_channel_->SignalDataReceived.connect(webrtc_data_channel,
                                                  &DataChannel::OnDataReceived);
  } else {
    SignalSctpReadyToSendData.connect(webrtc_data_channel,
                                      &DataChannel::OnChannelReady);
    SignalSctpDataReceived.connect(webrtc_data_channel,
                                   &DataChannel::OnDataReceived);
    SignalSctpStreamClosedRemotely.connect(
        webrtc_data_channel, &DataChannel::OnStreamClosedRemotely);
  }
  return true;
}

void PeerConnection::DisconnectDataChannel(DataChannel* webrtc_data_channel) {
  if (!rtp_data_channel_ && !sctp_transport_) {
    RTC_LOG(LS_ERROR)
        << "DisconnectDataChannel called when rtp_data_channel_ and "
           "sctp_transport_ are NULL.";
    return;
  }
  if (rtp_data_channel_) {
    rtp_data_channel_->SignalReadyToSendData.disconnect(webrtc_data_channel);
    rtp_data_channel_->SignalDataReceived.disconnect(webrtc_data_channel);
  } else {
    SignalSctpReadyToSendData.disconnect(webrtc_data_channel);
    SignalSctpDataReceived.disconnect(webrtc_data_channel);
    SignalSctpStreamClosedRemotely.disconnect(webrtc_data_channel);
  }
}

void PeerConnection::AddSctpDataStream(int sid) {
  if (!sctp_transport_) {
    RTC_LOG(LS_ERROR)
        << "AddSctpDataStream called when sctp_transport_ is NULL.";
    return;
  }
  network_thread()->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&cricket::SctpTransportInternal::OpenStream,
                               sctp_transport_.get(), sid));
}

void PeerConnection::RemoveSctpDataStream(int sid) {
  if (!sctp_transport_) {
    RTC_LOG(LS_ERROR) << "RemoveSctpDataStream called when sctp_transport_ is "
                      << "NULL.";
    return;
  }
  network_thread()->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&cricket::SctpTransportInternal::ResetStream,
                               sctp_transport_.get(), sid));
}

bool PeerConnection::ReadyToSendData() const {
  return (rtp_data_channel_ && rtp_data_channel_->ready_to_send_data()) ||
         sctp_ready_to_send_data_;
}

std::unique_ptr<SessionStats> PeerConnection::GetSessionStats_s() {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  ChannelNamePairs channel_name_pairs;
  if (voice_channel()) {
    channel_name_pairs.voice = ChannelNamePair(
        voice_channel()->content_name(), voice_channel()->transport_name());
  }
  if (video_channel()) {
    channel_name_pairs.video = ChannelNamePair(
        video_channel()->content_name(), video_channel()->transport_name());
  }
  if (rtp_data_channel()) {
    channel_name_pairs.data =
        ChannelNamePair(rtp_data_channel()->content_name(),
                        rtp_data_channel()->transport_name());
  }
  if (sctp_transport_) {
    RTC_DCHECK(sctp_content_name_);
    RTC_DCHECK(sctp_transport_name_);
    channel_name_pairs.data =
        ChannelNamePair(*sctp_content_name_, *sctp_transport_name_);
  }
  return GetSessionStats(channel_name_pairs);
}

std::unique_ptr<SessionStats> PeerConnection::GetSessionStats(
    const ChannelNamePairs& channel_name_pairs) {
  if (network_thread()->IsCurrent()) {
    return GetSessionStats_n(channel_name_pairs);
  }
  return network_thread()->Invoke<std::unique_ptr<SessionStats>>(
      RTC_FROM_HERE,
      rtc::Bind(&PeerConnection::GetSessionStats_n, this, channel_name_pairs));
}

bool PeerConnection::GetLocalCertificate(
    const std::string& transport_name,
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
  return transport_controller_->GetLocalCertificate(transport_name,
                                                    certificate);
}

std::unique_ptr<rtc::SSLCertificate> PeerConnection::GetRemoteSSLCertificate(
    const std::string& transport_name) {
  return transport_controller_->GetRemoteSSLCertificate(transport_name);
}

cricket::DataChannelType PeerConnection::data_channel_type() const {
  return data_channel_type_;
}

bool PeerConnection::IceRestartPending(const std::string& content_name) const {
  return pending_ice_restarts_.find(content_name) !=
         pending_ice_restarts_.end();
}

bool PeerConnection::NeedsIceRestart(const std::string& content_name) const {
  return transport_controller_->NeedsIceRestart(content_name);
}

void PeerConnection::OnCertificateReady(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  transport_controller_->SetLocalCertificate(certificate);
}

void PeerConnection::OnDtlsSrtpSetupFailure(cricket::BaseChannel*, bool rtcp) {
  SetError(ERROR_TRANSPORT,
           rtcp ? kDtlsSrtpSetupFailureRtcp : kDtlsSrtpSetupFailureRtp);
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
                       << "all transports are writable.";
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionConnected);
      break;
    case cricket::kIceConnectionCompleted:
      RTC_LOG(LS_INFO) << "Changing to ICE completed state because "
                       << "all transports are complete.";
      if (ice_connection_state_ !=
          PeerConnectionInterface::kIceConnectionConnected) {
        // If jumping directly from "checking" to "connected",
        // signal "connected" first.
        SetIceConnectionState(PeerConnectionInterface::kIceConnectionConnected);
      }
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionCompleted);
      if (metrics_observer()) {
        ReportTransportStats();
      }
      break;
    default:
      RTC_NOTREACHED();
  }
}

void PeerConnection::OnTransportControllerCandidatesGathered(
    const std::string& transport_name,
    const cricket::Candidates& candidates) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
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
    if (local_description()) {
      mutable_local_description()->AddCandidate(candidate.get());
    }
    OnIceCandidate(std::move(candidate));
  }
}

void PeerConnection::OnTransportControllerCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  // Sanity check.
  for (const cricket::Candidate& candidate : candidates) {
    if (candidate.transport_name().empty()) {
      RTC_LOG(LS_ERROR) << "OnTransportControllerCandidatesRemoved: "
                        << "empty content name in candidate "
                        << candidate.ToString();
      return;
    }
  }

  if (local_description()) {
    mutable_local_description()->RemoveCandidates(candidates);
  }
  OnIceCandidatesRemoved(candidates);
}

void PeerConnection::OnTransportControllerDtlsHandshakeError(
    rtc::SSLHandshakeError error) {
  if (metrics_observer()) {
    metrics_observer()->IncrementEnumCounter(
        webrtc::kEnumCounterDtlsHandshakeError, static_cast<int>(error),
        static_cast<int>(rtc::SSLHandshakeError::MAX_VALUE));
  }
}

// Enabling voice and video (and RTP data) channels.
void PeerConnection::EnableChannels() {
  if (voice_channel() && !voice_channel()->enabled()) {
    voice_channel()->Enable(true);
  }

  if (video_channel() && !video_channel()->enabled()) {
    video_channel()->Enable(true);
  }

  if (rtp_data_channel_ && !rtp_data_channel_->enabled()) {
    rtp_data_channel_->Enable(true);
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
              << "candidate.";
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
  size_t mediacontent_index = static_cast<size_t>(candidate->sdp_mline_index());
  size_t remote_content_size =
      remote_description()->description()->contents().size();
  if (mediacontent_index >= remote_content_size) {
    RTC_LOG(LS_ERROR) << "UseCandidate: Invalid candidate media index.";
    return false;
  }

  cricket::ContentInfo content =
      remote_description()->description()->contents()[mediacontent_index];
  std::vector<cricket::Candidate> candidates;
  candidates.push_back(candidate->candidate());
  // Invoking BaseSession method to handle remote candidates.
  std::string error;
  if (transport_controller_->AddRemoteCandidates(content.name, candidates,
                                                 &error)) {
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
    if (!error.empty()) {
      RTC_LOG(LS_WARNING) << error;
    }
  }
  return true;
}

void PeerConnection::RemoveUnusedChannels(const SessionDescription* desc) {
  // TODO(steveanton): Add support for multiple audio/video channels.
  // Destroy video channel first since it may have a pointer to the
  // voice channel.
  const cricket::ContentInfo* video_info = cricket::GetFirstVideoContent(desc);
  if ((!video_info || video_info->rejected) && video_channel()) {
    DestroyVideoChannel(video_channel());
  }

  const cricket::ContentInfo* voice_info = cricket::GetFirstAudioContent(desc);
  if ((!voice_info || voice_info->rejected) && voice_channel()) {
    DestroyVoiceChannel(voice_channel());
  }

  const cricket::ContentInfo* data_info = cricket::GetFirstDataContent(desc);
  if (!data_info || data_info->rejected) {
    if (rtp_data_channel_) {
      DestroyDataChannel();
    }
    if (sctp_transport_) {
      OnDataChannelDestroyed();
      network_thread()->Invoke<void>(
          RTC_FROM_HERE,
          rtc::Bind(&PeerConnection::DestroySctpTransport_n, this));
    }
  }
}

// Returns the name of the transport channel when BUNDLE is enabled, or nullptr
// if the channel is not part of any bundle.
const std::string* PeerConnection::GetBundleTransportName(
    const cricket::ContentInfo* content,
    const cricket::ContentGroup* bundle) {
  if (!bundle) {
    return nullptr;
  }
  const std::string* first_content_name = bundle->FirstContentName();
  if (!first_content_name) {
    RTC_LOG(LS_WARNING) << "Tried to BUNDLE with no contents.";
    return nullptr;
  }
  if (!bundle->HasContentName(content->name)) {
    RTC_LOG(LS_WARNING) << content->name << " is not part of any bundle group";
    return nullptr;
  }
  RTC_LOG(LS_INFO) << "Bundling " << content->name << " on "
                   << *first_content_name;
  return first_content_name;
}

bool PeerConnection::CreateChannels(const SessionDescription* desc) {
  // TODO(steveanton): Add support for multiple audio/video channels.
  const cricket::ContentGroup* bundle_group = nullptr;
  if (configuration_.bundle_policy ==
      PeerConnectionInterface::kBundlePolicyMaxBundle) {
    bundle_group = desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
    if (!bundle_group) {
      RTC_LOG(LS_WARNING) << "max-bundle specified without BUNDLE specified";
      return false;
    }
  }
  // Creating the media channels and transport proxies.
  const cricket::ContentInfo* voice = cricket::GetFirstAudioContent(desc);
  if (voice && !voice->rejected && !voice_channel()) {
    if (!CreateVoiceChannel(voice,
                            GetBundleTransportName(voice, bundle_group))) {
      RTC_LOG(LS_ERROR) << "Failed to create voice channel.";
      return false;
    }
  }

  const cricket::ContentInfo* video = cricket::GetFirstVideoContent(desc);
  if (video && !video->rejected && !video_channel()) {
    if (!CreateVideoChannel(video,
                            GetBundleTransportName(video, bundle_group))) {
      RTC_LOG(LS_ERROR) << "Failed to create video channel.";
      return false;
    }
  }

  const cricket::ContentInfo* data = cricket::GetFirstDataContent(desc);
  if (data_channel_type_ != cricket::DCT_NONE && data && !data->rejected &&
      !rtp_data_channel_ && !sctp_transport_) {
    if (!CreateDataChannel(data, GetBundleTransportName(data, bundle_group))) {
      RTC_LOG(LS_ERROR) << "Failed to create data channel.";
      return false;
    }
  }

  return true;
}

// TODO(steveanton): Perhaps this should be managed by the RtpTransceiver.
bool PeerConnection::CreateVoiceChannel(const cricket::ContentInfo* content,
                                        const std::string* bundle_transport) {
  // TODO(steveanton): Check to see if it's safe to create multiple voice
  // channels.
  RTC_DCHECK(!voice_channel());

  std::string transport_name =
      bundle_transport ? *bundle_transport : content->name;

  cricket::DtlsTransportInternal* rtp_dtls_transport =
      transport_controller_->CreateDtlsTransport(
          transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  cricket::DtlsTransportInternal* rtcp_dtls_transport = nullptr;
  if (configuration_.rtcp_mux_policy !=
      PeerConnectionInterface::kRtcpMuxPolicyRequire) {
    rtcp_dtls_transport = transport_controller_->CreateDtlsTransport(
        transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
  }

  cricket::VoiceChannel* voice_channel = channel_manager()->CreateVoiceChannel(
      call_.get(), configuration_.media_config, rtp_dtls_transport,
      rtcp_dtls_transport, transport_controller_->signaling_thread(),
      content->name, SrtpRequired(), audio_options_);
  if (!voice_channel) {
    transport_controller_->DestroyDtlsTransport(
        transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
    if (rtcp_dtls_transport) {
      transport_controller_->DestroyDtlsTransport(
          transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
    }
    return false;
  }
  voice_channel->SignalRtcpMuxFullyActive.connect(
      this, &PeerConnection::DestroyRtcpTransport_n);
  voice_channel->SignalDtlsSrtpSetupFailure.connect(
      this, &PeerConnection::OnDtlsSrtpSetupFailure);
  voice_channel->SignalSentPacket.connect(this,
                                          &PeerConnection::OnSentPacket_w);

  GetAudioTransceiver()->internal()->SetChannel(voice_channel);

  return true;
}

// TODO(steveanton): Perhaps this should be managed by the RtpTransceiver.
bool PeerConnection::CreateVideoChannel(const cricket::ContentInfo* content,
                                        const std::string* bundle_transport) {
  // TODO(steveanton): Check to see if it's safe to create multiple video
  // channels.
  RTC_DCHECK(!video_channel());

  std::string transport_name =
      bundle_transport ? *bundle_transport : content->name;

  cricket::DtlsTransportInternal* rtp_dtls_transport =
      transport_controller_->CreateDtlsTransport(
          transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  cricket::DtlsTransportInternal* rtcp_dtls_transport = nullptr;
  if (configuration_.rtcp_mux_policy !=
      PeerConnectionInterface::kRtcpMuxPolicyRequire) {
    rtcp_dtls_transport = transport_controller_->CreateDtlsTransport(
        transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
  }

  cricket::VideoChannel* video_channel = channel_manager()->CreateVideoChannel(
      call_.get(), configuration_.media_config, rtp_dtls_transport,
      rtcp_dtls_transport, transport_controller_->signaling_thread(),
      content->name, SrtpRequired(), video_options_);

  if (!video_channel) {
    transport_controller_->DestroyDtlsTransport(
        transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
    if (rtcp_dtls_transport) {
      transport_controller_->DestroyDtlsTransport(
          transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
    }
    return false;
  }
  video_channel->SignalRtcpMuxFullyActive.connect(
      this, &PeerConnection::DestroyRtcpTransport_n);
  video_channel->SignalDtlsSrtpSetupFailure.connect(
      this, &PeerConnection::OnDtlsSrtpSetupFailure);
  video_channel->SignalSentPacket.connect(this,
                                          &PeerConnection::OnSentPacket_w);

  GetVideoTransceiver()->internal()->SetChannel(video_channel);

  return true;
}

bool PeerConnection::CreateDataChannel(const cricket::ContentInfo* content,
                                       const std::string* bundle_transport) {
  const std::string transport_name =
      bundle_transport ? *bundle_transport : content->name;
  bool sctp = (data_channel_type_ == cricket::DCT_SCTP);
  if (sctp) {
    if (!sctp_factory_) {
      RTC_LOG(LS_ERROR)
          << "Trying to create SCTP transport, but didn't compile with "
             "SCTP support (HAVE_SCTP)";
      return false;
    }
    if (!network_thread()->Invoke<bool>(
            RTC_FROM_HERE, rtc::Bind(&PeerConnection::CreateSctpTransport_n,
                                     this, content->name, transport_name))) {
      return false;
    }
  } else {
    std::string transport_name =
        bundle_transport ? *bundle_transport : content->name;
    cricket::DtlsTransportInternal* rtp_dtls_transport =
        transport_controller_->CreateDtlsTransport(
            transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
    cricket::DtlsTransportInternal* rtcp_dtls_transport = nullptr;
    if (configuration_.rtcp_mux_policy !=
        PeerConnectionInterface::kRtcpMuxPolicyRequire) {
      rtcp_dtls_transport = transport_controller_->CreateDtlsTransport(
          transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
    }

    rtp_data_channel_ = channel_manager()->CreateRtpDataChannel(
        configuration_.media_config, rtp_dtls_transport, rtcp_dtls_transport,
        transport_controller_->signaling_thread(), content->name,
        SrtpRequired());

    if (!rtp_data_channel_) {
      transport_controller_->DestroyDtlsTransport(
          transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
      if (rtcp_dtls_transport) {
        transport_controller_->DestroyDtlsTransport(
            transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
      }
      return false;
    }

    rtp_data_channel_->SignalRtcpMuxFullyActive.connect(
        this, &PeerConnection::DestroyRtcpTransport_n);
    rtp_data_channel_->SignalDtlsSrtpSetupFailure.connect(
        this, &PeerConnection::OnDtlsSrtpSetupFailure);
    rtp_data_channel_->SignalSentPacket.connect(
        this, &PeerConnection::OnSentPacket_w);
  }

  for (const auto& channel : sctp_data_channels_) {
    channel->OnTransportChannelCreated();
  }

  return true;
}

Call::Stats PeerConnection::GetCallStats() {
  if (!worker_thread()->IsCurrent()) {
    return worker_thread()->Invoke<Call::Stats>(
        RTC_FROM_HERE, rtc::Bind(&PeerConnection::GetCallStats, this));
  }
  if (call_) {
    return call_->GetStats();
  } else {
    return Call::Stats();
  }
}

std::unique_ptr<SessionStats> PeerConnection::GetSessionStats_n(
    const ChannelNamePairs& channel_name_pairs) {
  RTC_DCHECK(network_thread()->IsCurrent());
  std::unique_ptr<SessionStats> session_stats(new SessionStats());
  for (const auto channel_name_pair :
       {&channel_name_pairs.voice, &channel_name_pairs.video,
        &channel_name_pairs.data}) {
    if (*channel_name_pair) {
      cricket::TransportStats transport_stats;
      if (!transport_controller_->GetStats((*channel_name_pair)->transport_name,
                                           &transport_stats)) {
        return nullptr;
      }
      session_stats->proxy_to_transport[(*channel_name_pair)->content_name] =
          (*channel_name_pair)->transport_name;
      session_stats->transport_stats[(*channel_name_pair)->transport_name] =
          std::move(transport_stats);
    }
  }
  return session_stats;
}

bool PeerConnection::CreateSctpTransport_n(const std::string& content_name,
                                           const std::string& transport_name) {
  RTC_DCHECK(network_thread()->IsCurrent());
  RTC_DCHECK(sctp_factory_);
  cricket::DtlsTransportInternal* tc =
      transport_controller_->CreateDtlsTransport_n(
          transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  sctp_transport_ = sctp_factory_->CreateSctpTransport(tc);
  RTC_DCHECK(sctp_transport_);
  sctp_invoker_.reset(new rtc::AsyncInvoker());
  sctp_transport_->SignalReadyToSendData.connect(
      this, &PeerConnection::OnSctpTransportReadyToSendData_n);
  sctp_transport_->SignalDataReceived.connect(
      this, &PeerConnection::OnSctpTransportDataReceived_n);
  sctp_transport_->SignalStreamClosedRemotely.connect(
      this, &PeerConnection::OnSctpStreamClosedRemotely_n);
  sctp_transport_name_ = transport_name;
  sctp_content_name_ = content_name;
  return true;
}

void PeerConnection::ChangeSctpTransport_n(const std::string& transport_name) {
  RTC_DCHECK(network_thread()->IsCurrent());
  RTC_DCHECK(sctp_transport_);
  RTC_DCHECK(sctp_transport_name_);
  std::string old_sctp_transport_name = *sctp_transport_name_;
  sctp_transport_name_ = transport_name;
  cricket::DtlsTransportInternal* tc =
      transport_controller_->CreateDtlsTransport_n(
          transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  sctp_transport_->SetTransportChannel(tc);
  transport_controller_->DestroyDtlsTransport_n(
      old_sctp_transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
}

void PeerConnection::DestroySctpTransport_n() {
  RTC_DCHECK(network_thread()->IsCurrent());
  sctp_transport_.reset(nullptr);
  sctp_content_name_.reset();
  sctp_transport_name_.reset();
  sctp_invoker_.reset(nullptr);
  sctp_ready_to_send_data_ = false;
}

void PeerConnection::OnSctpTransportReadyToSendData_n() {
  RTC_DCHECK(data_channel_type_ == cricket::DCT_SCTP);
  RTC_DCHECK(network_thread()->IsCurrent());
  // Note: Cannot use rtc::Bind here because it will grab a reference to
  // PeerConnection and potentially cause PeerConnection to live longer than
  // expected. It is safe not to grab a reference since the sctp_invoker_ will
  // be destroyed before PeerConnection is destroyed, and at that point all
  // pending tasks will be cleared.
  sctp_invoker_->AsyncInvoke<void>(RTC_FROM_HERE, signaling_thread(), [this] {
    OnSctpTransportReadyToSendData_s(true);
  });
}

void PeerConnection::OnSctpTransportReadyToSendData_s(bool ready) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  sctp_ready_to_send_data_ = ready;
  SignalSctpReadyToSendData(ready);
}

void PeerConnection::OnSctpTransportDataReceived_n(
    const cricket::ReceiveDataParams& params,
    const rtc::CopyOnWriteBuffer& payload) {
  RTC_DCHECK(data_channel_type_ == cricket::DCT_SCTP);
  RTC_DCHECK(network_thread()->IsCurrent());
  // Note: Cannot use rtc::Bind here because it will grab a reference to
  // PeerConnection and potentially cause PeerConnection to live longer than
  // expected. It is safe not to grab a reference since the sctp_invoker_ will
  // be destroyed before PeerConnection is destroyed, and at that point all
  // pending tasks will be cleared.
  sctp_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, params, payload] {
        OnSctpTransportDataReceived_s(params, payload);
      });
}

void PeerConnection::OnSctpTransportDataReceived_s(
    const cricket::ReceiveDataParams& params,
    const rtc::CopyOnWriteBuffer& payload) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (params.type == cricket::DMT_CONTROL && IsOpenMessage(payload)) {
    // Received OPEN message; parse and signal that a new data channel should
    // be created.
    std::string label;
    InternalDataChannelInit config;
    config.id = params.ssrc;
    if (!ParseDataChannelOpenMessage(payload, &label, &config)) {
      RTC_LOG(LS_WARNING) << "Failed to parse the OPEN message for sid "
                          << params.ssrc;
      return;
    }
    config.open_handshake_role = InternalDataChannelInit::kAcker;
    OnDataChannelOpenMessage(label, config);
  } else {
    // Otherwise just forward the signal.
    SignalSctpDataReceived(params, payload);
  }
}

void PeerConnection::OnSctpStreamClosedRemotely_n(int sid) {
  RTC_DCHECK(data_channel_type_ == cricket::DCT_SCTP);
  RTC_DCHECK(network_thread()->IsCurrent());
  sctp_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(),
      rtc::Bind(&sigslot::signal1<int>::operator(),
                &SignalSctpStreamClosedRemotely, sid));
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
        content->type == cricket::NS_JINGLE_RTP) {
      if (!HasRtcpMuxEnabled(content))
        return false;
    }
  }
  // RTCP-MUX is enabled in all the contents.
  return true;
}

bool PeerConnection::HasRtcpMuxEnabled(const cricket::ContentInfo* content) {
  const cricket::MediaContentDescription* description =
      static_cast<cricket::MediaContentDescription*>(content->description);
  return description->rtcp_mux();
}

bool PeerConnection::ValidateSessionDescription(
    const SessionDescriptionInterface* sdesc,
    cricket::ContentSource source,
    std::string* err_desc) {
  std::string type;
  if (error() != ERROR_NONE) {
    return BadSdp(source, type, GetSessionErrorMsg(), err_desc);
  }

  if (!sdesc || !sdesc->description()) {
    return BadSdp(source, type, kInvalidSdp, err_desc);
  }

  type = sdesc->type();
  Action action = GetAction(sdesc->type());
  if (source == cricket::CS_LOCAL) {
    if (!ExpectSetLocalDescription(action))
      return BadLocalSdp(type, BadStateErrMsg(signaling_state()), err_desc);
  } else {
    if (!ExpectSetRemoteDescription(action))
      return BadRemoteSdp(type, BadStateErrMsg(signaling_state()), err_desc);
  }

  // Verify crypto settings.
  std::string crypto_error;
  if ((webrtc_session_desc_factory_->SdesPolicy() == cricket::SEC_REQUIRED ||
       dtls_enabled_) &&
      !VerifyCrypto(sdesc->description(), dtls_enabled_, &crypto_error)) {
    return BadSdp(source, type, crypto_error, err_desc);
  }

  // Verify ice-ufrag and ice-pwd.
  if (!VerifyIceUfragPwdPresent(sdesc->description())) {
    return BadSdp(source, type, kSdpWithoutIceUfragPwd, err_desc);
  }

  if (!ValidateBundleSettings(sdesc->description())) {
    return BadSdp(source, type, kBundleWithoutRtcpMux, err_desc);
  }

  // TODO(skvlad): When the local rtcp-mux policy is Require, reject any
  // m-lines that do not rtcp-mux enabled.

  // Verify m-lines in Answer when compared against Offer.
  if (action == kAnswer || action == kPrAnswer) {
    const cricket::SessionDescription* offer_desc =
        (source == cricket::CS_LOCAL) ? remote_description()->description()
                                      : local_description()->description();
    if (!MediaSectionsHaveSameCount(offer_desc, sdesc->description()) ||
        !MediaSectionsInSameOrder(offer_desc, sdesc->description())) {
      return BadAnswerSdp(source, kMlineMismatchInAnswer, err_desc);
    }
  } else {
    const cricket::SessionDescription* current_desc = nullptr;
    if (source == cricket::CS_LOCAL && local_description()) {
      current_desc = local_description()->description();
    } else if (source == cricket::CS_REMOTE && remote_description()) {
      current_desc = remote_description()->description();
    }
    // The re-offers should respect the order of m= sections in current
    // description. See RFC3264 Section 8 paragraph 4 for more details.
    if (current_desc &&
        !MediaSectionsInSameOrder(current_desc, sdesc->description())) {
      return BadOfferSdp(source, kMlineMismatchInSubsequentOffer, err_desc);
    }
  }

  return true;
}

bool PeerConnection::ExpectSetLocalDescription(Action action) {
  PeerConnectionInterface::SignalingState state = signaling_state();
  if (action == kOffer) {
    return (state == PeerConnectionInterface::kStable) ||
           (state == PeerConnectionInterface::kHaveLocalOffer);
  } else {  // Answer or PrAnswer
    return (state == PeerConnectionInterface::kHaveRemoteOffer) ||
           (state == PeerConnectionInterface::kHaveLocalPrAnswer);
  }
}

bool PeerConnection::ExpectSetRemoteDescription(Action action) {
  PeerConnectionInterface::SignalingState state = signaling_state();
  if (action == kOffer) {
    return (state == PeerConnectionInterface::kStable) ||
           (state == PeerConnectionInterface::kHaveRemoteOffer);
  } else {  // Answer or PrAnswer.
    return (state == PeerConnectionInterface::kHaveLocalOffer) ||
           (state == PeerConnectionInterface::kHaveRemotePrAnswer);
  }
}

std::string PeerConnection::GetSessionErrorMsg() {
  std::ostringstream desc;
  desc << kSessionError << GetErrorCodeString(error()) << ". ";
  desc << kSessionErrorDesc << error_desc() << ".";
  return desc.str();
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
  *valid = true;

  const SessionDescriptionInterface* current_remote_desc =
      remote_desc ? remote_desc : remote_description();

  if (!current_remote_desc) {
    return false;
  }

  size_t mediacontent_index = static_cast<size_t>(candidate->sdp_mline_index());
  size_t remote_content_size =
      current_remote_desc->description()->contents().size();
  if (mediacontent_index >= remote_content_size) {
    RTC_LOG(LS_ERROR)
        << "ReadyToUseRemoteCandidate: Invalid candidate media index "
        << mediacontent_index;

    *valid = false;
    return false;
  }

  cricket::ContentInfo content =
      current_remote_desc->description()->contents()[mediacontent_index];

  const std::string transport_name = GetTransportName(content.name);
  if (transport_name.empty()) {
    return false;
  }
  return transport_controller_->ReadyForRemoteCandidates(transport_name);
}

bool PeerConnection::SrtpRequired() const {
  return dtls_enabled_ ||
         webrtc_session_desc_factory_->SdesPolicy() == cricket::SEC_REQUIRED;
}

void PeerConnection::OnTransportControllerGatheringState(
    cricket::IceGatheringState state) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (state == cricket::kIceGatheringGathering) {
    OnIceGatheringChange(PeerConnectionInterface::kIceGatheringGathering);
  } else if (state == cricket::kIceGatheringComplete) {
    OnIceGatheringChange(PeerConnectionInterface::kIceGatheringComplete);
  }
}

void PeerConnection::ReportTransportStats() {
  // Use a set so we don't report the same stats twice if two channels share
  // a transport.
  std::set<std::string> transport_names;
  if (voice_channel()) {
    transport_names.insert(voice_channel()->transport_name());
  }
  if (video_channel()) {
    transport_names.insert(video_channel()->transport_name());
  }
  if (rtp_data_channel()) {
    transport_names.insert(rtp_data_channel()->transport_name());
  }
  if (sctp_transport_name_) {
    transport_names.insert(*sctp_transport_name_);
  }
  for (const auto& name : transport_names) {
    cricket::TransportStats stats;
    if (transport_controller_->GetStats(name, &stats)) {
      ReportBestConnectionState(stats);
      ReportNegotiatedCiphers(stats);
    }
  }
}
// Walk through the ConnectionInfos to gather best connection usage
// for IPv4 and IPv6.
void PeerConnection::ReportBestConnectionState(
    const cricket::TransportStats& stats) {
  RTC_DCHECK(metrics_observer());
  for (cricket::TransportChannelStatsList::const_iterator it =
           stats.channel_stats.begin();
       it != stats.channel_stats.end(); ++it) {
    for (cricket::ConnectionInfos::const_iterator it_info =
             it->connection_infos.begin();
         it_info != it->connection_infos.end(); ++it_info) {
      if (!it_info->best_connection) {
        continue;
      }

      PeerConnectionEnumCounterType type = kPeerConnectionEnumCounterMax;
      const cricket::Candidate& local = it_info->local_candidate;
      const cricket::Candidate& remote = it_info->remote_candidate;

      // Increment the counter for IceCandidatePairType.
      if (local.protocol() == cricket::TCP_PROTOCOL_NAME ||
          (local.type() == RELAY_PORT_TYPE &&
           local.relay_protocol() == cricket::TCP_PROTOCOL_NAME)) {
        type = kEnumCounterIceCandidatePairTypeTcp;
      } else if (local.protocol() == cricket::UDP_PROTOCOL_NAME) {
        type = kEnumCounterIceCandidatePairTypeUdp;
      } else {
        RTC_CHECK(0);
      }
      metrics_observer()->IncrementEnumCounter(
          type, GetIceCandidatePairCounter(local, remote),
          kIceCandidatePairMax);

      // Increment the counter for IP type.
      if (local.address().family() == AF_INET) {
        metrics_observer()->IncrementEnumCounter(
            kEnumCounterAddressFamily, kBestConnections_IPv4,
            kPeerConnectionAddressFamilyCounter_Max);

      } else if (local.address().family() == AF_INET6) {
        metrics_observer()->IncrementEnumCounter(
            kEnumCounterAddressFamily, kBestConnections_IPv6,
            kPeerConnectionAddressFamilyCounter_Max);
      } else {
        RTC_CHECK(0);
      }

      return;
    }
  }
}

void PeerConnection::ReportNegotiatedCiphers(
    const cricket::TransportStats& stats) {
  RTC_DCHECK(metrics_observer());
  if (!dtls_enabled_ || stats.channel_stats.empty()) {
    return;
  }

  int srtp_crypto_suite = stats.channel_stats[0].srtp_crypto_suite;
  int ssl_cipher_suite = stats.channel_stats[0].ssl_cipher_suite;
  if (srtp_crypto_suite == rtc::SRTP_INVALID_CRYPTO_SUITE &&
      ssl_cipher_suite == rtc::TLS_NULL_WITH_NULL_NULL) {
    return;
  }

  PeerConnectionEnumCounterType srtp_counter_type;
  PeerConnectionEnumCounterType ssl_counter_type;
  if (stats.transport_name == cricket::CN_AUDIO) {
    srtp_counter_type = kEnumCounterAudioSrtpCipher;
    ssl_counter_type = kEnumCounterAudioSslCipher;
  } else if (stats.transport_name == cricket::CN_VIDEO) {
    srtp_counter_type = kEnumCounterVideoSrtpCipher;
    ssl_counter_type = kEnumCounterVideoSslCipher;
  } else if (stats.transport_name == cricket::CN_DATA) {
    srtp_counter_type = kEnumCounterDataSrtpCipher;
    ssl_counter_type = kEnumCounterDataSslCipher;
  } else {
    RTC_NOTREACHED();
    return;
  }

  if (srtp_crypto_suite != rtc::SRTP_INVALID_CRYPTO_SUITE) {
    metrics_observer()->IncrementSparseEnumCounter(srtp_counter_type,
                                                   srtp_crypto_suite);
  }
  if (ssl_cipher_suite != rtc::TLS_NULL_WITH_NULL_NULL) {
    metrics_observer()->IncrementSparseEnumCounter(ssl_counter_type,
                                                   ssl_cipher_suite);
  }
}

void PeerConnection::OnSentPacket_w(const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(worker_thread()->IsCurrent());
  RTC_DCHECK(call_);
  call_->OnSentPacket(sent_packet);
}

const std::string PeerConnection::GetTransportName(
    const std::string& content_name) {
  cricket::BaseChannel* channel = GetChannel(content_name);
  if (!channel) {
    if (sctp_transport_) {
      RTC_DCHECK(sctp_content_name_);
      RTC_DCHECK(sctp_transport_name_);
      if (content_name == *sctp_content_name_) {
        return *sctp_transport_name_;
      }
    }
    // Return an empty string if failed to retrieve the transport name.
    return "";
  }
  return channel->transport_name();
}

void PeerConnection::DestroyRtcpTransport_n(const std::string& transport_name) {
  RTC_DCHECK(network_thread()->IsCurrent());
  transport_controller_->DestroyDtlsTransport_n(
      transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
}

// TODO(steveanton): Perhaps this should be managed by the RtpTransceiver.
void PeerConnection::DestroyVideoChannel(cricket::VideoChannel* video_channel) {
  RTC_DCHECK(video_channel);
  RTC_DCHECK(video_channel->rtp_dtls_transport());
  RTC_DCHECK_EQ(GetVideoTransceiver()->internal()->channel(), video_channel);
  GetVideoTransceiver()->internal()->SetChannel(nullptr);
  const std::string transport_name =
      video_channel->rtp_dtls_transport()->transport_name();
  const bool need_to_delete_rtcp =
      (video_channel->rtcp_dtls_transport() != nullptr);
  // The above need to be cached before destroying the video channel so that we
  // do not access uninitialized memory.
  channel_manager()->DestroyVideoChannel(video_channel);
  transport_controller_->DestroyDtlsTransport(
      transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  if (need_to_delete_rtcp) {
    transport_controller_->DestroyDtlsTransport(
        transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
  }
}

// TODO(steveanton): Perhaps this should be managed by the RtpTransceiver.
void PeerConnection::DestroyVoiceChannel(cricket::VoiceChannel* voice_channel) {
  RTC_DCHECK(voice_channel);
  RTC_DCHECK(voice_channel->rtp_dtls_transport());
  RTC_DCHECK_EQ(GetAudioTransceiver()->internal()->channel(), voice_channel);
  GetAudioTransceiver()->internal()->SetChannel(nullptr);
  const std::string transport_name =
      voice_channel->rtp_dtls_transport()->transport_name();
  const bool need_to_delete_rtcp =
      (voice_channel->rtcp_dtls_transport() != nullptr);
  // The above need to be cached before destroying the video channel so that we
  // do not access uninitialized memory.
  channel_manager()->DestroyVoiceChannel(voice_channel);
  transport_controller_->DestroyDtlsTransport(
      transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  if (need_to_delete_rtcp) {
    transport_controller_->DestroyDtlsTransport(
        transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
  }
}

void PeerConnection::DestroyDataChannel() {
  OnDataChannelDestroyed();
  RTC_DCHECK(rtp_data_channel_->rtp_dtls_transport());
  std::string transport_name;
  transport_name = rtp_data_channel_->rtp_dtls_transport()->transport_name();
  bool need_to_delete_rtcp =
      (rtp_data_channel_->rtcp_dtls_transport() != nullptr);
  channel_manager()->DestroyRtpDataChannel(rtp_data_channel_);
  rtp_data_channel_ = nullptr;
  transport_controller_->DestroyDtlsTransport(
      transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  if (need_to_delete_rtcp) {
    transport_controller_->DestroyDtlsTransport(
        transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
  }
}

}  // namespace webrtc
