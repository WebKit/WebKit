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
#include <limits>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/jsepicecandidate.h"
#include "api/jsepsessiondescription.h"
#include "api/mediastreamproxy.h"
#include "api/mediastreamtrackproxy.h"
#include "api/umametrics.h"
#include "call/call.h"
#include "logging/rtc_event_log/icelogger.h"
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
#include "pc/sdputils.h"
#include "pc/streamcollection.h"
#include "pc/videocapturertracksource.h"
#include "pc/videotrack.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

using cricket::ContentInfo;
using cricket::ContentInfos;
using cricket::MediaContentDescription;
using cricket::SessionDescription;
using cricket::MediaProtocolType;
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
const char kInvalidCandidates[] = "Description contains invalid candidates.";
const char kInvalidSdp[] = "Invalid session description.";
const char kMlineMismatchInAnswer[] =
    "The order of m-lines in answer doesn't match order in offer. Rejecting "
    "answer.";
const char kMlineMismatchInSubsequentOffer[] =
    "The order of m-lines in subsequent offer doesn't match order from "
    "previous offer/answer.";
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

namespace {

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
  MSG_FREE_DATACHANNELS,
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
    cricket::MediaDescriptionOptions* video_media_description_options,
    int num_sim_layers) {
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
            sender->id(), sender->internal()->stream_ids(),
            num_sim_layers);
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
  bool ok = error.ok();
  if (error_out) {
    *error_out = std::move(error);
  }
  return ok;
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

// Logic to decide if an m= section can be recycled. This means that the new
// m= section is not rejected, but the old local or remote m= section is
// rejected. |old_content_one| and |old_content_two| refer to the m= section
// of the old remote and old local descriptions in no particular order.
// We need to check both the old local and remote because either
// could be the most current from the latest negotation.
bool IsMediaSectionBeingRecycled(SdpType type,
                                 const ContentInfo& content,
                                 const ContentInfo* old_content_one,
                                 const ContentInfo* old_content_two) {
  return type == SdpType::kOffer && !content.rejected &&
         ((old_content_one && old_content_one->rejected) ||
          (old_content_two && old_content_two->rejected));
}

// Verify that the order of media sections in |new_desc| matches
// |current_desc|. The number of m= sections in |new_desc| should be no
// less than |current_desc|. In the case of checking an answer's
// |new_desc|, the |current_desc| is the last offer that was set as the
// local or remote. In the case of checking an offer's |new_desc| we
// check against the local and remote descriptions stored from the last
// negotiation, because either of these could be the most up to date for
// possible rejected m sections. These are the |current_desc| and
// |secondary_current_desc|.
bool MediaSectionsInSameOrder(const SessionDescription& current_desc,
                              const SessionDescription* secondary_current_desc,
                              const SessionDescription& new_desc,
                              const SdpType type) {
  if (current_desc.contents().size() > new_desc.contents().size()) {
    return false;
  }

  for (size_t i = 0; i < current_desc.contents().size(); ++i) {
    const cricket::ContentInfo* secondary_content_info = nullptr;
    if (secondary_current_desc &&
        i < secondary_current_desc->contents().size()) {
      secondary_content_info = &secondary_current_desc->contents()[i];
    }
    if (IsMediaSectionBeingRecycled(type, new_desc.contents()[i],
                                    &current_desc.contents()[i],
                                    secondary_content_info)) {
      // For new offer descriptions, if the media section can be recycled, it's
      // valid for the MID and media type to change.
      continue;
    }
    if (new_desc.contents()[i].name != current_desc.contents()[i].name) {
      return false;
    }
    const MediaContentDescription* new_desc_mdesc =
        new_desc.contents()[i].media_description();
    const MediaContentDescription* current_desc_mdesc =
        current_desc.contents()[i].media_description();
    if (new_desc_mdesc->type() != current_desc_mdesc->type()) {
      return false;
    }
  }
  return true;
}

bool MediaSectionsHaveSameCount(const SessionDescription& desc1,
                                const SessionDescription& desc2) {
  return desc1.contents().size() == desc2.contents().size();
}

void NoteKeyProtocolAndMedia(KeyExchangeProtocolType protocol_type,
                             cricket::MediaType media_type) {
  // Array of structs needed to map {KeyExchangeProtocolType,
  // cricket::MediaType} to KeyExchangeProtocolMedia without using std::map in
  // order to avoid -Wglobal-constructors and -Wexit-time-destructors.
  static constexpr struct {
    KeyExchangeProtocolType protocol_type;
    cricket::MediaType media_type;
    KeyExchangeProtocolMedia protocol_media;
  } kEnumCounterKeyProtocolMediaMap[] = {
      {kEnumCounterKeyProtocolDtls, cricket::MEDIA_TYPE_AUDIO,
       kEnumCounterKeyProtocolMediaTypeDtlsAudio},
      {kEnumCounterKeyProtocolDtls, cricket::MEDIA_TYPE_VIDEO,
       kEnumCounterKeyProtocolMediaTypeDtlsVideo},
      {kEnumCounterKeyProtocolDtls, cricket::MEDIA_TYPE_DATA,
       kEnumCounterKeyProtocolMediaTypeDtlsData},
      {kEnumCounterKeyProtocolSdes, cricket::MEDIA_TYPE_AUDIO,
       kEnumCounterKeyProtocolMediaTypeSdesAudio},
      {kEnumCounterKeyProtocolSdes, cricket::MEDIA_TYPE_VIDEO,
       kEnumCounterKeyProtocolMediaTypeSdesVideo},
      {kEnumCounterKeyProtocolSdes, cricket::MEDIA_TYPE_DATA,
       kEnumCounterKeyProtocolMediaTypeSdesData},
  };

  RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.KeyProtocol", protocol_type,
                            kEnumCounterKeyProtocolMax);

  for (const auto& i : kEnumCounterKeyProtocolMediaMap) {
    if (i.protocol_type == protocol_type && i.media_type == media_type) {
      RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.KeyProtocolByMedia",
                                i.protocol_media,
                                kEnumCounterKeyProtocolMediaTypeMax);
    }
  }
}

void NoteAddIceCandidateResult(int result) {
  RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.AddIceCandidate", result,
                            kAddIceCandidateMax);
}

// Checks that each non-rejected content has SDES crypto keys or a DTLS
// fingerprint, unless it's in a BUNDLE group, in which case only the
// BUNDLE-tag section (first media section/description in the BUNDLE group)
// needs a ufrag and pwd. Mismatches, such as replying with a DTLS fingerprint
// to SDES keys, will be caught in JsepTransport negotiation, and backstopped
// by Channel's |srtp_required| check.
RTCError VerifyCrypto(const SessionDescription* desc, bool dtls_enabled) {
  const cricket::ContentGroup* bundle =
      desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  for (const cricket::ContentInfo& content_info : desc->contents()) {
    if (content_info.rejected) {
      continue;
    }
    // Note what media is used with each crypto protocol, for all sections.
    NoteKeyProtocolAndMedia(dtls_enabled ? webrtc::kEnumCounterKeyProtocolDtls
                                         : webrtc::kEnumCounterKeyProtocolSdes,
                            content_info.media_description()->type());
    const std::string& mid = content_info.name;
    if (bundle && bundle->HasContentName(mid) &&
        mid != *(bundle->FirstContentName())) {
      // This isn't the first media section in the BUNDLE group, so it's not
      // required to have crypto attributes, since only the crypto attributes
      // from the first section actually get used.
      continue;
    }

    // If the content isn't rejected or bundled into another m= section, crypto
    // must be present.
    const MediaContentDescription* media = content_info.media_description();
    const TransportInfo* tinfo = desc->GetTransportInfoByName(mid);
    if (!media || !tinfo) {
      // Something is not right.
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, kInvalidSdp);
    }
    if (dtls_enabled) {
      if (!tinfo->description.identity_fingerprint) {
        RTC_LOG(LS_WARNING)
            << "Session description must have DTLS fingerprint if "
               "DTLS enabled.";
        return RTCError(RTCErrorType::INVALID_PARAMETER,
                        kSdpWithoutDtlsFingerprint);
      }
    } else {
      if (media->cryptos().empty()) {
        RTC_LOG(LS_WARNING)
            << "Session description must have SDES when DTLS disabled.";
        return RTCError(RTCErrorType::INVALID_PARAMETER, kSdpWithoutSdesCrypto);
      }
    }
  }
  return RTCError::OK();
}

// Checks that each non-rejected content has ice-ufrag and ice-pwd set, unless
// it's in a BUNDLE group, in which case only the BUNDLE-tag section (first
// media section/description in the BUNDLE group) needs a ufrag and pwd.
bool VerifyIceUfragPwdPresent(const SessionDescription* desc) {
  const cricket::ContentGroup* bundle =
      desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  for (const cricket::ContentInfo& content_info : desc->contents()) {
    if (content_info.rejected) {
      continue;
    }
    const std::string& mid = content_info.name;
    if (bundle && bundle->HasContentName(mid) &&
        mid != *(bundle->FirstContentName())) {
      // This isn't the first media section in the BUNDLE group, so it's not
      // required to have ufrag/password, since only the ufrag/password from
      // the first section actually get used.
      continue;
    }

    // If the content isn't rejected or bundled into another m= section,
    // ice-ufrag and ice-pwd must be present.
    const TransportInfo* tinfo = desc->GetTransportInfoByName(mid);
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

// Get the SCTP port out of a SessionDescription.
// Return -1 if not found.
int GetSctpPort(const SessionDescription* session_description) {
  const cricket::DataContentDescription* data_desc =
      GetFirstDataContentDescription(session_description);
  RTC_DCHECK(data_desc);
  if (!data_desc) {
    return -1;
  }
  std::string value;
  cricket::DataCodec match_pattern(cricket::kGoogleSctpDataCodecPlType,
                                   cricket::kGoogleSctpDataCodecName);
  for (const cricket::DataCodec& codec : data_desc->codecs()) {
    if (!codec.Matches(match_pattern)) {
      continue;
    }
    if (codec.GetParam(cricket::kCodecParamPort, &value)) {
      return rtc::FromString<int>(value);
    }
  }
  return -1;
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

// Generates a string error message for SetLocalDescription/SetRemoteDescription
// from an RTCError.
std::string GetSetDescriptionErrorMessage(cricket::ContentSource source,
                                          SdpType type,
                                          const RTCError& error) {
  rtc::StringBuilder oss;
  oss << "Failed to set " << (source == cricket::CS_LOCAL ? "local" : "remote")
      << " " << SdpTypeToString(type) << " sdp: " << error.message();
  return oss.Release();
}

std::string GetStreamIdsString(rtc::ArrayView<const std::string> stream_ids) {
  std::string output = "streams=[";
  const char* separator = "";
  for (const auto& stream_id : stream_ids) {
    output.append(separator).append(stream_id);
    separator = ", ";
  }
  output.append("]");
  return output;
}

absl::optional<int> RTCConfigurationToIceConfigOptionalInt(
    int rtc_configuration_parameter) {
  if (rtc_configuration_parameter ==
      webrtc::PeerConnectionInterface::RTCConfiguration::kUndefined) {
    return absl::nullopt;
  }
  return rtc_configuration_parameter;
}

cricket::DataMessageType ToCricketDataMessageType(DataMessageType type) {
  switch (type) {
    case DataMessageType::kText:
      return cricket::DMT_TEXT;
    case DataMessageType::kBinary:
      return cricket::DMT_BINARY;
    case DataMessageType::kControl:
      return cricket::DMT_CONTROL;
    default:
      return cricket::DMT_NONE;
  }
  return cricket::DMT_NONE;
}

DataMessageType ToWebrtcDataMessageType(cricket::DataMessageType type) {
  switch (type) {
    case cricket::DMT_TEXT:
      return DataMessageType::kText;
    case cricket::DMT_BINARY:
      return DataMessageType::kBinary;
    case cricket::DMT_CONTROL:
      return DataMessageType::kControl;
    case cricket::DMT_NONE:
    default:
      RTC_NOTREACHED();
  }
  return DataMessageType::kControl;
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
      pc_->PostSetSessionDescriptionFailure(wrapper_, std::move(error));
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
    int ice_connection_receiving_timeout;
    int ice_backup_candidate_pair_ping_interval;
    ContinualGatheringPolicy continual_gathering_policy;
    bool prioritize_most_likely_ice_candidate_pairs;
    struct cricket::MediaConfig media_config;
    bool prune_turn_ports;
    bool presume_writable_when_fully_relayed;
    bool enable_ice_renomination;
    bool redetermine_role_on_ice_restart;
    absl::optional<int> ice_check_interval_strong_connectivity;
    absl::optional<int> ice_check_interval_weak_connectivity;
    absl::optional<int> ice_check_min_interval;
    absl::optional<int> ice_unwritable_timeout;
    absl::optional<int> ice_unwritable_min_checks;
    absl::optional<int> stun_candidate_keepalive_interval;
    absl::optional<rtc::IntervalRange> ice_regather_interval_range;
    webrtc::TurnCustomizer* turn_customizer;
    SdpSemantics sdp_semantics;
    absl::optional<rtc::AdapterType> network_preference;
    bool active_reset_srtp_params;
    bool use_media_transport;
    bool use_media_transport_for_data_channels;
    absl::optional<CryptoOptions> crypto_options;
    bool offer_extmap_allow_mixed;
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
         presume_writable_when_fully_relayed ==
             o.presume_writable_when_fully_relayed &&
         enable_ice_renomination == o.enable_ice_renomination &&
         redetermine_role_on_ice_restart == o.redetermine_role_on_ice_restart &&
         ice_check_interval_strong_connectivity ==
             o.ice_check_interval_strong_connectivity &&
         ice_check_interval_weak_connectivity ==
             o.ice_check_interval_weak_connectivity &&
         ice_check_min_interval == o.ice_check_min_interval &&
         ice_unwritable_timeout == o.ice_unwritable_timeout &&
         ice_unwritable_min_checks == o.ice_unwritable_min_checks &&
         stun_candidate_keepalive_interval ==
             o.stun_candidate_keepalive_interval &&
         ice_regather_interval_range == o.ice_regather_interval_range &&
         turn_customizer == o.turn_customizer &&
         sdp_semantics == o.sdp_semantics &&
         network_preference == o.network_preference &&
         active_reset_srtp_params == o.active_reset_srtp_params &&
         use_media_transport == o.use_media_transport &&
         use_media_transport_for_data_channels ==
             o.use_media_transport_for_data_channels &&
         crypto_options == o.crypto_options &&
         offer_extmap_allow_mixed == o.offer_extmap_allow_mixed;
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

  // Need to stop transceivers before destroying the stats collector because
  // AudioRtpSender has a reference to the StatsCollector it will update when
  // stopping.
  for (const auto& transceiver : transceivers_) {
    transceiver->Stop();
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

  webrtc_session_desc_factory_.reset();
  sctp_invoker_.reset();
  sctp_factory_.reset();
  media_transport_invoker_.reset();
  transport_controller_.reset();

  // port_allocator_ lives on the network thread and should be destroyed there.
  network_thread()->Invoke<void>(RTC_FROM_HERE,
                                 [this] { port_allocator_.reset(); });
  // call_ and event_log_ must be destroyed on the worker thread.
  worker_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
    call_.reset();
    // The event log must outlive call (and any other object that uses it).
    event_log_.reset();
  });
}

void PeerConnection::DestroyAllChannels() {
  // Destroy video channels first since they may have a pointer to a voice
  // channel.
  for (const auto& transceiver : transceivers_) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      DestroyTransceiverChannel(transceiver);
    }
  }
  for (const auto& transceiver : transceivers_) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      DestroyTransceiverChannel(transceiver);
    }
  }
  DestroyDataChannel();
}

bool PeerConnection::Initialize(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    PeerConnectionDependencies dependencies) {
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
  tls_cert_verifier_ = std::move(dependencies.tls_cert_verifier);

  cricket::ServerAddresses stun_servers;
  std::vector<cricket::RelayServerConfig> turn_servers;

  RTCErrorType parse_error =
      ParseIceServers(configuration.servers, &stun_servers, &turn_servers);
  if (parse_error != RTCErrorType::NONE) {
    return false;
  }

  // The port allocator lives on the network thread and should be initialized
  // there.
  if (!network_thread()->Invoke<bool>(
          RTC_FROM_HERE,
          rtc::Bind(&PeerConnection::InitializePortAllocator_n, this,
                    stun_servers, turn_servers, configuration))) {
    return false;
  }
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
  if (port_allocator_flags_ & cricket::PORTALLOCATOR_ENABLE_IPV6) {
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
  config.event_log = event_log_.get();
#if defined(ENABLE_EXTERNAL_AUTH)
  config.enable_external_auth = true;
#endif
  config.active_reset_srtp_params = configuration.active_reset_srtp_params;

  if (configuration.use_media_transport ||
      configuration.use_media_transport_for_data_channels) {
    if (!factory_->media_transport_factory()) {
      RTC_DCHECK(false)
          << "PeerConnecton is initialized with use_media_transport = true or "
          << "use_media_transport_for_data_channels = true "
          << "but media transport factory is not set in PeerConnectionFactory";
      return false;
    }

    config.media_transport_factory = factory_->media_transport_factory();
  }

  transport_controller_.reset(new JsepTransportController(
      signaling_thread(), network_thread(), port_allocator_.get(),
      async_resolver_factory_.get(), config));
  transport_controller_->SignalIceConnectionState.connect(
      this, &PeerConnection::OnTransportControllerConnectionState);
  transport_controller_->SignalStandardizedIceConnectionState.connect(
      this, &PeerConnection::SetStandardizedIceConnectionState);
  transport_controller_->SignalConnectionState.connect(
      this, &PeerConnection::SetConnectionState);
  transport_controller_->SignalIceGatheringState.connect(
      this, &PeerConnection::OnTransportControllerGatheringState);
  transport_controller_->SignalIceCandidatesGathered.connect(
      this, &PeerConnection::OnTransportControllerCandidatesGathered);
  transport_controller_->SignalIceCandidatesRemoved.connect(
      this, &PeerConnection::OnTransportControllerCandidatesRemoved);
  transport_controller_->SignalDtlsHandshakeError.connect(
      this, &PeerConnection::OnTransportControllerDtlsHandshakeError);

  sctp_factory_ = factory_->CreateSctpTransportInternalFactory();

  stats_.reset(new StatsCollector(this));
  stats_collector_ = RTCStatsCollector::Create(this);

  configuration_ = configuration;

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
    dtls_enabled_ = (dependencies.cert_generator || certificate);
    // |configuration| can override the default |dtls_enabled_| value.
    if (configuration.enable_dtls_srtp) {
      dtls_enabled_ = *(configuration.enable_dtls_srtp);
    }
  }

  if (configuration.use_media_transport_for_data_channels) {
    if (configuration.enable_rtp_data_channel) {
      RTC_LOG(LS_ERROR) << "enable_rtp_data_channel and "
                           "use_media_transport_for_data_channels are "
                           "incompatible and cannot both be set to true";
      return false;
    }
    data_channel_type_ = cricket::DCT_MEDIA_TRANSPORT;
  } else if (configuration.enable_rtp_data_channel) {
    // Enable creation of RTP data channels if the kEnableRtpDataChannels is
    // set. It takes precendence over the disable_sctp_data_channels
    // PeerConnectionFactoryInterface::Options.
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

  audio_options_.audio_jitter_buffer_min_delay_ms =
      configuration.audio_jitter_buffer_min_delay_ms;

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

  webrtc_session_desc_factory_.reset(new WebRtcSessionDescriptionFactory(
      signaling_thread(), channel_manager(), this, session_id(),
      std::move(dependencies.cert_generator), certificate));
  webrtc_session_desc_factory_->SignalCertificateReady.connect(
      this, &PeerConnection::OnCertificateReady);

  if (options.disable_encryption) {
    webrtc_session_desc_factory_->SetSdesPolicy(cricket::SEC_DISABLED);
  }

  webrtc_session_desc_factory_->set_enable_encrypted_rtp_header_extensions(
      GetCryptoOptions().srtp.enable_encrypted_rtp_header_extensions);

  // Add default audio/video transceivers for Plan B SDP.
  if (!IsUnifiedPlan()) {
    transceivers_.push_back(
        RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
            signaling_thread(), new RtpTransceiver(cricket::MEDIA_TYPE_AUDIO)));
    transceivers_.push_back(
        RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
            signaling_thread(), new RtpTransceiver(cricket::MEDIA_TYPE_VIDEO)));
  }
  int delay_ms =
      return_histogram_very_quickly_ ? 0 : REPORT_USAGE_PATTERN_DELAY_MS;
  signaling_thread()->PostDelayed(RTC_FROM_HERE, delay_ms, this,
                                  MSG_REPORT_USAGE_PATTERN, nullptr);
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
  auto result =
      cricket::P2PTransportChannel::ValidateIceConfig(ParseIceConfig(config));
  return result;
}

rtc::scoped_refptr<StreamCollectionInterface> PeerConnection::local_streams() {
  RTC_CHECK(!IsUnifiedPlan()) << "local_streams is not available with Unified "
                                 "Plan SdpSemantics. Please use GetSenders "
                                 "instead.";
  return local_streams_;
}

rtc::scoped_refptr<StreamCollectionInterface> PeerConnection::remote_streams() {
  RTC_CHECK(!IsUnifiedPlan()) << "remote_streams is not available with Unified "
                                 "Plan SdpSemantics. Please use GetReceivers "
                                 "instead.";
  return remote_streams_;
}

bool PeerConnection::AddStream(MediaStreamInterface* local_stream) {
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
  Observer()->OnRenegotiationNeeded();
  return true;
}

void PeerConnection::RemoveStream(MediaStreamInterface* local_stream) {
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
  Observer()->OnRenegotiationNeeded();
}

RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> PeerConnection::AddTrack(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
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
    Observer()->OnRenegotiationNeeded();
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
    if (transceiver->direction() == RtpTransceiverDirection::kRecvOnly) {
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kSendRecv);
    } else if (transceiver->direction() == RtpTransceiverDirection::kInactive) {
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kSendOnly);
    }
    transceiver->sender()->SetTrack(track);
    transceiver->internal()->sender_internal()->set_stream_ids(stream_ids);
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
  for (auto transceiver : transceivers_) {
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
  Observer()->OnRenegotiationNeeded();
  return RTCError::OK();
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::FindTransceiverBySender(
    rtc::scoped_refptr<RtpSenderInterface> sender) {
  for (auto transceiver : transceivers_) {
    if (transceiver->sender() == sender) {
      return transceiver;
    }
  }
  return nullptr;
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
    bool fire_callback) {
  RTC_DCHECK((media_type == cricket::MEDIA_TYPE_AUDIO ||
              media_type == cricket::MEDIA_TYPE_VIDEO));
  if (track) {
    RTC_DCHECK_EQ(media_type,
                  (track->kind() == MediaStreamTrackInterface::kAudioKind
                       ? cricket::MEDIA_TYPE_AUDIO
                       : cricket::MEDIA_TYPE_VIDEO));
  }

  // TODO(bugs.webrtc.org/7600): Verify init.
  if (init.send_encodings.size() > 1) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::UNSUPPORTED_PARAMETER,
        "Attempted to create an encoder with more than 1 encoding parameter.");
  }

  for (const auto& encoding : init.send_encodings) {
    if (encoding.ssrc.has_value()) {
      LOG_AND_RETURN_ERROR(
          RTCErrorType::UNSUPPORTED_PARAMETER,
          "Attempted to set an unimplemented parameter of RtpParameters.");
    }
  }

  RtpParameters parameters;
  parameters.encodings = init.send_encodings;
  if (UnimplementedRtpParameterHasValue(parameters)) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::UNSUPPORTED_PARAMETER,
        "Attempted to set an unimplemented parameter of RtpParameters.");
  }

  RTC_LOG(LS_INFO) << "Adding " << cricket::MediaTypeToString(media_type)
                   << " transceiver in response to a call to AddTransceiver.";
  // Set the sender ID equal to the track ID if the track is specified unless
  // that sender ID is already in use.
  std::string sender_id =
      (track && !FindSenderById(track->id()) ? track->id()
                                             : rtc::CreateRandomUuid());
  auto sender = CreateSender(media_type, sender_id, track, init.stream_ids,
                             init.send_encodings);
  auto receiver = CreateReceiver(media_type, rtc::CreateRandomUuid());
  auto transceiver = CreateAndAddTransceiver(sender, receiver);
  transceiver->internal()->set_direction(init.direction);

  if (fire_callback) {
    Observer()->OnRenegotiationNeeded();
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
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender;
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    RTC_DCHECK(!track ||
               (track->kind() == MediaStreamTrackInterface::kAudioKind));
    sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        new AudioRtpSender(worker_thread(), id, stats_.get()));
    NoteUsageEvent(UsageEvent::AUDIO_ADDED);
  } else {
    RTC_DCHECK_EQ(media_type, cricket::MEDIA_TYPE_VIDEO);
    RTC_DCHECK(!track ||
               (track->kind() == MediaStreamTrackInterface::kVideoKind));
    sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), new VideoRtpSender(worker_thread(), id));
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
  // Ensure that the new sender does not have an ID that is already in use by
  // another sender.
  // Allow receiver IDs to conflict since those come from remote SDP (which
  // could be invalid, but should not cause a crash).
  RTC_DCHECK(!FindSenderById(sender->id()));
  auto transceiver = RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
      signaling_thread(), new RtpTransceiver(sender, receiver));
  transceivers_.push_back(transceiver);
  transceiver->internal()->SignalNegotiationNeeded.connect(
      this, &PeerConnection::OnNegotiationNeeded);
  return transceiver;
}

void PeerConnection::OnNegotiationNeeded() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(!IsClosed());
  Observer()->OnRenegotiationNeeded();
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnection::CreateSender(
    const std::string& kind,
    const std::string& stream_id) {
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
    auto* audio_sender = new AudioRtpSender(
        worker_thread(), rtc::CreateRandomUuid(), stats_.get());
    audio_sender->SetMediaChannel(voice_media_channel());
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), audio_sender);
    GetAudioTransceiver()->internal()->AddSender(new_sender);
  } else if (kind == MediaStreamTrackInterface::kVideoKind) {
    auto* video_sender =
        new VideoRtpSender(worker_thread(), rtc::CreateRandomUuid());
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
  std::vector<rtc::scoped_refptr<RtpSenderInterface>> ret;
  for (const auto& sender : GetSendersInternal()) {
    ret.push_back(sender);
  }
  return ret;
}

std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
PeerConnection::GetSendersInternal() const {
  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
      all_senders;
  for (const auto& transceiver : transceivers_) {
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
  for (const auto& transceiver : transceivers_) {
    auto receivers = transceiver->internal()->receivers();
    all_receivers.insert(all_receivers.end(), receivers.begin(),
                         receivers.end());
  }
  return all_receivers;
}

std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnection::GetTransceivers() const {
  RTC_CHECK(IsUnifiedPlan())
      << "GetTransceivers is only supported with Unified Plan SdpSemantics.";
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> all_transceivers;
  for (const auto& transceiver : transceivers_) {
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
  TRACE_EVENT0("webrtc", "PeerConnection::GetStats");
  RTC_DCHECK(stats_collector_);
  RTC_DCHECK(callback);
  stats_collector_->GetStatsReport(callback);
}

void PeerConnection::GetStats(
    rtc::scoped_refptr<RtpSenderInterface> selector,
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback) {
  TRACE_EVENT0("webrtc", "PeerConnection::GetStats");
  RTC_DCHECK(callback);
  RTC_DCHECK(stats_collector_);
  rtc::scoped_refptr<RtpSenderInternal> internal_sender;
  if (selector) {
    for (const auto& proxy_transceiver : transceivers_) {
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
  RTC_DCHECK(callback);
  RTC_DCHECK(stats_collector_);
  rtc::scoped_refptr<RtpReceiverInternal> internal_receiver;
  if (selector) {
    for (const auto& proxy_transceiver : transceivers_) {
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
  return signaling_state_;
}

PeerConnectionInterface::IceConnectionState
PeerConnection::ice_connection_state() {
  return ice_connection_state_;
}

PeerConnectionInterface::IceConnectionState
PeerConnection::standardized_ice_connection_state() {
  return standardized_ice_connection_state_;
}

PeerConnectionInterface::PeerConnectionState
PeerConnection::peer_connection_state() {
  return connection_state_;
}

PeerConnectionInterface::IceGatheringState
PeerConnection::ice_gathering_state() {
  return ice_gathering_state_;
}

rtc::scoped_refptr<DataChannelInterface> PeerConnection::CreateDataChannel(
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
    Observer()->OnRenegotiationNeeded();
  }
  NoteUsageEvent(UsageEvent::DATA_ADDED);
  return DataChannelProxy::Create(signaling_thread(), channel.get());
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
    PostCreateSessionDescriptionFailure(
        observer, RTCError(RTCErrorType::INVALID_STATE, std::move(error)));
    return;
  }

  if (!ValidateOfferAnswerOptions(options)) {
    std::string error = "CreateOffer called with invalid options.";
    RTC_LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(
        observer, RTCError(RTCErrorType::INVALID_PARAMETER, std::move(error)));
    return;
  }

  // Legacy handling for offer_to_receive_audio and offer_to_receive_video.
  // Specified in WebRTC section 4.4.3.2 "Legacy configuration extensions".
  if (IsUnifiedPlan()) {
    RTCError error = HandleLegacyOfferOptions(options);
    if (!error.ok()) {
      PostCreateSessionDescriptionFailure(observer, std::move(error));
      return;
    }
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForOffer(options, &session_options);
  webrtc_session_desc_factory_->CreateOffer(observer, options, session_options);
}

RTCError PeerConnection::HandleLegacyOfferOptions(
    const RTCOfferAnswerOptions& options) {
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
  if (GetReceivingTransceiversOfType(media_type).empty()) {
    RTC_LOG(LS_INFO)
        << "Adding one recvonly " << cricket::MediaTypeToString(media_type)
        << " transceiver since CreateOffer specified offer_to_receive=1";
    RtpTransceiverInit init;
    init.direction = RtpTransceiverDirection::kRecvOnly;
    AddTransceiver(media_type, nullptr, init, /*fire_callback=*/false);
  }
}

std::vector<rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
PeerConnection::GetReceivingTransceiversOfType(cricket::MediaType media_type) {
  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
      receiving_transceivers;
  for (const auto& transceiver : transceivers_) {
    if (!transceiver->stopped() && transceiver->media_type() == media_type &&
        RtpTransceiverDirectionHasRecv(transceiver->direction())) {
      receiving_transceivers.push_back(transceiver);
    }
  }
  return receiving_transceivers;
}

void PeerConnection::CreateAnswer(CreateSessionDescriptionObserver* observer,
                                  const RTCOfferAnswerOptions& options) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateAnswer");
  if (!observer) {
    RTC_LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }

  if (!(signaling_state_ == kHaveRemoteOffer ||
        signaling_state_ == kHaveLocalPrAnswer)) {
    std::string error =
        "PeerConnection cannot create an answer in a state other than "
        "have-remote-offer or have-local-pranswer.";
    RTC_LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(
        observer, RTCError(RTCErrorType::INVALID_STATE, std::move(error)));
    return;
  }

  // The remote description should be set if we're in the right state.
  RTC_DCHECK(remote_description());

  if (IsUnifiedPlan()) {
    if (options.offer_to_receive_audio != RTCOfferAnswerOptions::kUndefined) {
      RTC_LOG(LS_WARNING) << "CreateAnswer: offer_to_receive_audio is not "
                             "supported with Unified Plan semantics. Use the "
                             "RtpTransceiver API instead.";
    }
    if (options.offer_to_receive_video != RTCOfferAnswerOptions::kUndefined) {
      RTC_LOG(LS_WARNING) << "CreateAnswer: offer_to_receive_video is not "
                             "supported with Unified Plan semantics. Use the "
                             "RtpTransceiver API instead.";
    }
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForAnswer(options, &session_options);

  webrtc_session_desc_factory_->CreateAnswer(observer, session_options);
}

void PeerConnection::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc_ptr) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetLocalDescription");

  // The SetLocalDescription contract is that we take ownership of the session
  // description regardless of the outcome, so wrap it in a unique_ptr right
  // away. Ideally, SetLocalDescription's signature will be changed to take the
  // description as a unique_ptr argument to formalize this agreement.
  std::unique_ptr<SessionDescriptionInterface> desc(desc_ptr);

  if (!observer) {
    RTC_LOG(LS_ERROR) << "SetLocalDescription - observer is NULL.";
    return;
  }

  if (!desc) {
    PostSetSessionDescriptionFailure(
        observer,
        RTCError(RTCErrorType::INTERNAL_ERROR, "SessionDescription is NULL."));
    return;
  }

  // If a session error has occurred the PeerConnection is in a possibly
  // inconsistent state so fail right away.
  if (session_error() != SessionError::kNone) {
    std::string error_message = GetSessionErrorMsg();
    RTC_LOG(LS_ERROR) << "SetLocalDescription: " << error_message;
    PostSetSessionDescriptionFailure(
        observer,
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }

  RTCError error = ValidateSessionDescription(desc.get(), cricket::CS_LOCAL);
  if (!error.ok()) {
    std::string error_message = GetSetDescriptionErrorMessage(
        cricket::CS_LOCAL, desc->GetType(), error);
    RTC_LOG(LS_ERROR) << error_message;
    PostSetSessionDescriptionFailure(
        observer,
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }

  // Grab the description type before moving ownership to ApplyLocalDescription,
  // which may destroy it before returning.
  const SdpType type = desc->GetType();

  error = ApplyLocalDescription(std::move(desc));
  // |desc| may be destroyed at this point.

  if (!error.ok()) {
    // If ApplyLocalDescription fails, the PeerConnection could be in an
    // inconsistent state, so act conservatively here and set the session error
    // so that future calls to SetLocalDescription/SetRemoteDescription fail.
    SetSessionError(SessionError::kContent, error.message());
    std::string error_message =
        GetSetDescriptionErrorMessage(cricket::CS_LOCAL, type, error);
    RTC_LOG(LS_ERROR) << error_message;
    PostSetSessionDescriptionFailure(
        observer,
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }
  RTC_DCHECK(local_description());

  PostSetSessionDescriptionSuccess(observer);

  // MaybeStartGathering needs to be called after posting
  // MSG_SET_SESSIONDESCRIPTION_SUCCESS, so that we don't signal any candidates
  // before signaling that SetLocalDescription completed.
  transport_controller_->MaybeStartGathering();

  if (local_description()->GetType() == SdpType::kAnswer) {
    // TODO(deadbeef): We already had to hop to the network thread for
    // MaybeStartGathering...
    network_thread()->Invoke<void>(
        RTC_FROM_HERE, rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                                 port_allocator_.get()));
    // Make UMA notes about what was agreed to.
    ReportNegotiatedSdpSemantics(*local_description());
  }
  NoteUsageEvent(UsageEvent::SET_LOCAL_DESCRIPTION_CALLED);
}

RTCError PeerConnection::ApplyLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(desc);

  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_->UpdateStats(kStatsOutputLevelStandard);

  // Take a reference to the old local description since it's used below to
  // compare against the new local description. When setting the new local
  // description, grab ownership of the replaced session description in case it
  // is the same as |old_local_description|, to keep it alive for the duration
  // of the method.
  const SessionDescriptionInterface* old_local_description =
      local_description();
  std::unique_ptr<SessionDescriptionInterface> replaced_local_description;
  SdpType type = desc->GetType();
  if (type == SdpType::kAnswer) {
    replaced_local_description = pending_local_description_
                                     ? std::move(pending_local_description_)
                                     : std::move(current_local_description_);
    current_local_description_ = std::move(desc);
    pending_local_description_ = nullptr;
    current_remote_description_ = std::move(pending_remote_description_);
  } else {
    replaced_local_description = std::move(pending_local_description_);
    pending_local_description_ = std::move(desc);
  }
  // The session description to apply now must be accessed by
  // |local_description()|.
  RTC_DCHECK(local_description());

  if (!is_caller_) {
    if (remote_description()) {
      // Remote description was applied first, so this PC is the callee.
      is_caller_ = false;
    } else {
      // Local description is applied first, so this PC is the caller.
      is_caller_ = true;
    }
  }

  RTCError error = PushdownTransportDescription(cricket::CS_LOCAL, type);
  if (!error.ok()) {
    return error;
  }

  if (IsUnifiedPlan()) {
    RTCError error = UpdateTransceiversAndDataChannels(
        cricket::CS_LOCAL, *local_description(), old_local_description,
        remote_description());
    if (!error.ok()) {
      return error;
    }
    std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> remove_list;
    std::vector<rtc::scoped_refptr<MediaStreamInterface>> removed_streams;
    for (const auto& transceiver : transceivers_) {
      const ContentInfo* content =
          FindMediaSectionForTransceiver(transceiver, local_description());
      if (!content) {
        continue;
      }
      const MediaContentDescription* media_desc = content->media_description();
      // 2.2.7.1.6: If description is of type "answer" or "pranswer", then run
      // the following steps:
      if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
        // 2.2.7.1.6.1: If direction is "sendonly" or "inactive", and
        // transceiver's [[FiredDirection]] slot is either "sendrecv" or
        // "recvonly", process the removal of a remote track for the media
        // description, given transceiver, removeList, and muteTracks.
        if (!RtpTransceiverDirectionHasRecv(media_desc->direction()) &&
            (transceiver->internal()->fired_direction() &&
             RtpTransceiverDirectionHasRecv(
                 *transceiver->internal()->fired_direction()))) {
          ProcessRemovalOfRemoteTrack(transceiver, &remove_list,
                                      &removed_streams);
        }
        // 2.2.7.1.6.2: Set transceiver's [[CurrentDirection]] and
        // [[FiredDirection]] slots to direction.
        transceiver->internal()->set_current_direction(media_desc->direction());
        transceiver->internal()->set_fired_direction(media_desc->direction());
      }
    }
    auto observer = Observer();
    for (const auto& transceiver : remove_list) {
      observer->OnRemoveTrack(transceiver->receiver());
    }
    for (const auto& stream : removed_streams) {
      observer->OnRemoveStream(stream);
    }
  } else {
    // Media channels will be created only when offer is set. These may use new
    // transports just created by PushdownTransportDescription.
    if (type == SdpType::kOffer) {
      // TODO(bugs.webrtc.org/4676) - Handle CreateChannel failure, as new local
      // description is applied. Restore back to old description.
      RTCError error = CreateChannels(*local_description()->description());
      if (!error.ok()) {
        return error;
      }
    }
    // Remove unused channels if MediaContentDescription is rejected.
    RemoveUnusedChannels(local_description()->description());
  }

  error = UpdateSessionState(type, cricket::CS_LOCAL,
                             local_description()->description());
  if (!error.ok()) {
    return error;
  }

  if (remote_description()) {
    // Now that we have a local description, we can push down remote candidates.
    UseCandidatesInSessionDescription(remote_description());
  }

  pending_ice_restarts_.clear();
  if (session_error() != SessionError::kNone) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR, GetSessionErrorMsg());
  }

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (DataChannel::IsSctpLike(data_channel_type_) && GetSctpSslRole(&role)) {
    AllocateSctpSids(role);
  }

  if (IsUnifiedPlan()) {
    for (const auto& transceiver : transceivers_) {
      const ContentInfo* content =
          FindMediaSectionForTransceiver(transceiver, local_description());
      if (!content) {
        continue;
      }
      const auto& streams = content->media_description()->streams();
      if (!content->rejected && !streams.empty()) {
        transceiver->internal()->sender_internal()->set_stream_ids(
            streams[0].stream_ids());
        transceiver->internal()->sender_internal()->SetSsrc(
            streams[0].first_ssrc());
      } else {
        // 0 is a special value meaning "this sender has no associated send
        // stream". Need to call this so the sender won't attempt to configure
        // a no longer existing stream and run into DCHECKs in the lower
        // layers.
        transceiver->internal()->sender_internal()->SetSsrc(0);
      }
    }
  } else {
    // Plan B semantics.

    // Update state and SSRC of local MediaStreams and DataChannels based on the
    // local session description.
    const cricket::ContentInfo* audio_content =
        GetFirstAudioContent(local_description()->description());
    if (audio_content) {
      if (audio_content->rejected) {
        RemoveSenders(cricket::MEDIA_TYPE_AUDIO);
      } else {
        const cricket::AudioContentDescription* audio_desc =
            audio_content->media_description()->as_audio();
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
            video_content->media_description()->as_video();
        UpdateLocalSenders(video_desc->streams(), video_desc->type());
      }
    }
  }

  const cricket::ContentInfo* data_content =
      GetFirstDataContent(local_description()->description());
  if (data_content) {
    const cricket::DataContentDescription* data_desc =
        data_content->media_description()->as_data();
    if (rtc::starts_with(data_desc->protocol().data(),
                         cricket::kMediaProtocolRtpPrefix)) {
      UpdateLocalRtpDataChannels(data_desc->streams());
    }
  }

  return RTCError::OK();
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
        RTCErrorType::INVALID_PARAMETER, "SessionDescription is NULL."));
    return;
  }

  // If a session error has occurred the PeerConnection is in a possibly
  // inconsistent state so fail right away.
  if (session_error() != SessionError::kNone) {
    std::string error_message = GetSessionErrorMsg();
    RTC_LOG(LS_ERROR) << "SetRemoteDescription: " << error_message;
    observer->OnSetRemoteDescriptionComplete(
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }

  if (desc->GetType() == SdpType::kOffer) {
    // Report to UMA the format of the received offer.
    ReportSdpFormatReceived(*desc);
  }

  RTCError error = ValidateSessionDescription(desc.get(), cricket::CS_REMOTE);
  if (!error.ok()) {
    std::string error_message = GetSetDescriptionErrorMessage(
        cricket::CS_REMOTE, desc->GetType(), error);
    RTC_LOG(LS_ERROR) << error_message;
    observer->OnSetRemoteDescriptionComplete(
        RTCError(error.type(), std::move(error_message)));
    return;
  }

  // Grab the description type before moving ownership to
  // ApplyRemoteDescription, which may destroy it before returning.
  const SdpType type = desc->GetType();

  error = ApplyRemoteDescription(std::move(desc));
  // |desc| may be destroyed at this point.

  if (!error.ok()) {
    // If ApplyRemoteDescription fails, the PeerConnection could be in an
    // inconsistent state, so act conservatively here and set the session error
    // so that future calls to SetLocalDescription/SetRemoteDescription fail.
    SetSessionError(SessionError::kContent, error.message());
    std::string error_message =
        GetSetDescriptionErrorMessage(cricket::CS_REMOTE, type, error);
    RTC_LOG(LS_ERROR) << error_message;
    observer->OnSetRemoteDescriptionComplete(
        RTCError(error.type(), std::move(error_message)));
    return;
  }
  RTC_DCHECK(remote_description());

  if (type == SdpType::kAnswer) {
    // TODO(deadbeef): We already had to hop to the network thread for
    // MaybeStartGathering...
    network_thread()->Invoke<void>(
        RTC_FROM_HERE, rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                                 port_allocator_.get()));
    // Make UMA notes about what was agreed to.
    ReportNegotiatedSdpSemantics(*remote_description());
  }

  observer->OnSetRemoteDescriptionComplete(RTCError::OK());
  NoteUsageEvent(UsageEvent::SET_REMOTE_DESCRIPTION_CALLED);
}

RTCError PeerConnection::ApplyRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(desc);

  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_->UpdateStats(kStatsOutputLevelStandard);

  // Take a reference to the old remote description since it's used below to
  // compare against the new remote description. When setting the new remote
  // description, grab ownership of the replaced session description in case it
  // is the same as |old_remote_description|, to keep it alive for the duration
  // of the method.
  const SessionDescriptionInterface* old_remote_description =
      remote_description();
  std::unique_ptr<SessionDescriptionInterface> replaced_remote_description;
  SdpType type = desc->GetType();
  if (type == SdpType::kAnswer) {
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
  // The session description to apply now must be accessed by
  // |remote_description()|.
  RTC_DCHECK(remote_description());

  RTCError error = PushdownTransportDescription(cricket::CS_REMOTE, type);
  if (!error.ok()) {
    return error;
  }
  // Transport and Media channels will be created only when offer is set.
  if (IsUnifiedPlan()) {
    RTCError error = UpdateTransceiversAndDataChannels(
        cricket::CS_REMOTE, *remote_description(), local_description(),
        old_remote_description);
    if (!error.ok()) {
      return error;
    }
  } else {
    // Media channels will be created only when offer is set. These may use new
    // transports just created by PushdownTransportDescription.
    if (type == SdpType::kOffer) {
      // TODO(mallinath) - Handle CreateChannel failure, as new local
      // description is applied. Restore back to old description.
      RTCError error = CreateChannels(*remote_description()->description());
      if (!error.ok()) {
        return error;
      }
    }
    // Remove unused channels if MediaContentDescription is rejected.
    RemoveUnusedChannels(remote_description()->description());
  }

  // NOTE: Candidates allocation will be initiated only when
  // SetLocalDescription is called.
  error = UpdateSessionState(type, cricket::CS_REMOTE,
                             remote_description()->description());
  if (!error.ok()) {
    return error;
  }

  if (local_description() &&
      !UseCandidatesInSessionDescription(remote_description())) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, kInvalidCandidates);
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
        if (type == SdpType::kOffer) {
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
            old_remote_description, content.name, mutable_remote_description());
      }
    }
  }

  if (session_error() != SessionError::kNone) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR, GetSessionErrorMsg());
  }

  // Set the the ICE connection state to connecting since the connection may
  // become writable with peer reflexive candidates before any remote candidate
  // is signaled.
  // TODO(pthatcher): This is a short-term solution for crbug/446908. A real fix
  // is to have a new signal the indicates a change in checking state from the
  // transport and expose a new checking() member from transport that can be
  // read to determine the current checking state. The existing SignalConnecting
  // actually means "gathering candidates", so cannot be be used here.
  if (remote_description()->GetType() != SdpType::kOffer &&
      remote_description()->number_of_mediasections() > 0u &&
      ice_connection_state() == PeerConnectionInterface::kIceConnectionNew) {
    SetIceConnectionState(PeerConnectionInterface::kIceConnectionChecking);
  }

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (DataChannel::IsSctpLike(data_channel_type_) && GetSctpSslRole(&role)) {
    AllocateSctpSids(role);
  }

  if (IsUnifiedPlan()) {
    std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
        now_receiving_transceivers;
    std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> remove_list;
    std::vector<rtc::scoped_refptr<MediaStreamInterface>> added_streams;
    std::vector<rtc::scoped_refptr<MediaStreamInterface>> removed_streams;
    for (const auto& transceiver : transceivers_) {
      const ContentInfo* content =
          FindMediaSectionForTransceiver(transceiver, remote_description());
      if (!content) {
        continue;
      }
      const MediaContentDescription* media_desc = content->media_description();
      RtpTransceiverDirection local_direction =
          RtpTransceiverDirectionReversed(media_desc->direction());
      // From the WebRTC specification, steps 2.2.8.5/6 of section 4.4.1.6 "Set
      // the RTCSessionDescription: If direction is sendrecv or recvonly, and
      // transceiver's current direction is neither sendrecv nor recvonly,
      // process the addition of a remote track for the media description.
      std::vector<std::string> stream_ids;
      if (!media_desc->streams().empty()) {
        // The remote description has signaled the stream IDs.
        stream_ids = media_desc->streams()[0].stream_ids();
      }
      if (RtpTransceiverDirectionHasRecv(local_direction) &&
          (!transceiver->fired_direction() ||
           !RtpTransceiverDirectionHasRecv(*transceiver->fired_direction()))) {
        RTC_LOG(LS_INFO) << "Processing the addition of a new track for MID="
                         << content->name << " (added to "
                         << GetStreamIdsString(stream_ids) << ").";

        std::vector<rtc::scoped_refptr<MediaStreamInterface>> media_streams;
        for (const std::string& stream_id : stream_ids) {
          rtc::scoped_refptr<MediaStreamInterface> stream =
              remote_streams_->find(stream_id);
          if (!stream) {
            stream = MediaStreamProxy::Create(rtc::Thread::Current(),
                                              MediaStream::Create(stream_id));
            remote_streams_->AddStream(stream);
            added_streams.push_back(stream);
          }
          media_streams.push_back(stream);
        }
        // Special case: "a=msid" missing, use random stream ID.
        if (media_streams.empty() &&
            !(remote_description()->description()->msid_signaling() &
              cricket::kMsidSignalingMediaSection)) {
          if (!missing_msid_default_stream_) {
            missing_msid_default_stream_ = MediaStreamProxy::Create(
                rtc::Thread::Current(),
                MediaStream::Create(rtc::CreateRandomUuid()));
            added_streams.push_back(missing_msid_default_stream_);
          }
          media_streams.push_back(missing_msid_default_stream_);
        }
        // This will add the remote track to the streams.
        // TODO(hbos): When we remove remote_streams(), use set_stream_ids()
        // instead. https://crbug.com/webrtc/9480
        transceiver->internal()->receiver_internal()->SetStreams(media_streams);
        now_receiving_transceivers.push_back(transceiver);
      }
      // 2.2.8.1.7: If direction is "sendonly" or "inactive", and transceiver's
      // [[FiredDirection]] slot is either "sendrecv" or "recvonly", process the
      // removal of a remote track for the media description, given transceiver,
      // removeList, and muteTracks.
      if (!RtpTransceiverDirectionHasRecv(local_direction) &&
          (transceiver->fired_direction() &&
           RtpTransceiverDirectionHasRecv(*transceiver->fired_direction()))) {
        ProcessRemovalOfRemoteTrack(transceiver, &remove_list,
                                    &removed_streams);
      }
      // 2.2.8.1.8: Set transceiver's [[FiredDirection]] slot to direction.
      transceiver->internal()->set_fired_direction(local_direction);
      // 2.2.8.1.9: If description is of type "answer" or "pranswer", then run
      // the following steps:
      if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
        // 2.2.8.1.9.1: Set transceiver's [[CurrentDirection]] slot to
        // direction.
        transceiver->internal()->set_current_direction(local_direction);
      }
      // 2.2.8.1.10: If the media description is rejected, and transceiver is
      // not already stopped, stop the RTCRtpTransceiver transceiver.
      if (content->rejected && !transceiver->stopped()) {
        RTC_LOG(LS_INFO) << "Stopping transceiver for MID=" << content->name
                         << " since the media section was rejected.";
        transceiver->Stop();
      }
      if (!content->rejected &&
          RtpTransceiverDirectionHasRecv(local_direction)) {
        // Set ssrc to 0 in the case of an unsignalled ssrc.
        uint32_t ssrc = 0;
        if (!media_desc->streams().empty() &&
            media_desc->streams()[0].has_ssrcs()) {
          ssrc = media_desc->streams()[0].first_ssrc();
        }
        transceiver->internal()->receiver_internal()->SetupMediaChannel(ssrc);
      }
    }
    // Once all processing has finished, fire off callbacks.
    auto observer = Observer();
    for (const auto& transceiver : now_receiving_transceivers) {
      stats_->AddTrack(transceiver->receiver()->track());
      observer->OnTrack(transceiver);
      observer->OnAddTrack(transceiver->receiver(),
                           transceiver->receiver()->streams());
    }
    for (const auto& stream : added_streams) {
      observer->OnAddStream(stream);
    }
    for (const auto& transceiver : remove_list) {
      observer->OnRemoveTrack(transceiver->receiver());
    }
    for (const auto& stream : removed_streams) {
      observer->OnRemoveStream(stream);
    }
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

  if (!IsUnifiedPlan()) {
    // TODO(steveanton): When removing RTP senders/receivers in response to a
    // rejected media section, there is some cleanup logic that expects the
    // voice/ video channel to still be set. But in this method the voice/video
    // channel would have been destroyed by the SetRemoteDescription caller
    // above so the cleanup that relies on them fails to run. The RemoveSenders
    // calls should be moved to right before the DestroyChannel calls to fix
    // this.

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
    auto observer = Observer();
    for (size_t i = 0; i < new_streams->count(); ++i) {
      MediaStreamInterface* new_stream = new_streams->at(i);
      stats_->AddStream(new_stream);
      observer->OnAddStream(
          rtc::scoped_refptr<MediaStreamInterface>(new_stream));
    }

    UpdateEndedRemoteMediaStreams();
  }

  return RTCError::OK();
}

void PeerConnection::ProcessRemovalOfRemoteTrack(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver,
    std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>* remove_list,
    std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams) {
  RTC_DCHECK(transceiver->mid());
  RTC_LOG(LS_INFO) << "Processing the removal of a track for MID="
                   << *transceiver->mid();
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> media_streams =
      transceiver->internal()->receiver_internal()->streams();
  // This will remove the remote track from the streams.
  transceiver->internal()->receiver_internal()->set_stream_ids({});
  remove_list->push_back(transceiver);
  // Remove any streams that no longer have tracks.
  // TODO(https://crbug.com/webrtc/9480): When we use stream IDs instead
  // of streams, see if the stream was removed by checking if this was the
  // last receiver with that stream ID.
  for (const auto& stream : media_streams) {
    if (stream->GetAudioTracks().empty() && stream->GetVideoTracks().empty()) {
      remote_streams_->RemoveStream(stream);
      removed_streams->push_back(stream);
    }
  }
}

RTCError PeerConnection::UpdateTransceiversAndDataChannels(
    cricket::ContentSource source,
    const SessionDescriptionInterface& new_session,
    const SessionDescriptionInterface* old_local_description,
    const SessionDescriptionInterface* old_remote_description) {
  RTC_DCHECK(IsUnifiedPlan());

  const cricket::ContentGroup* bundle_group = nullptr;
  if (new_session.GetType() == SdpType::kOffer) {
    auto bundle_group_or_error =
        GetEarlyBundleGroup(*new_session.description());
    if (!bundle_group_or_error.ok()) {
      return bundle_group_or_error.MoveError();
    }
    bundle_group = bundle_group_or_error.MoveValue();
  }

  const ContentInfos& new_contents = new_session.description()->contents();
  for (size_t i = 0; i < new_contents.size(); ++i) {
    const cricket::ContentInfo& new_content = new_contents[i];
    cricket::MediaType media_type = new_content.media_description()->type();
    seen_mids_.insert(new_content.name);
    if (media_type == cricket::MEDIA_TYPE_AUDIO ||
        media_type == cricket::MEDIA_TYPE_VIDEO) {
      const cricket::ContentInfo* old_local_content = nullptr;
      if (old_local_description &&
          i < old_local_description->description()->contents().size()) {
        old_local_content =
            &old_local_description->description()->contents()[i];
      }
      const cricket::ContentInfo* old_remote_content = nullptr;
      if (old_remote_description &&
          i < old_remote_description->description()->contents().size()) {
        old_remote_content =
            &old_remote_description->description()->contents()[i];
      }
      auto transceiver_or_error =
          AssociateTransceiver(source, new_session.GetType(), i, new_content,
                               old_local_content, old_remote_content);
      if (!transceiver_or_error.ok()) {
        return transceiver_or_error.MoveError();
      }
      auto transceiver = transceiver_or_error.MoveValue();
      RTCError error =
          UpdateTransceiverChannel(transceiver, new_content, bundle_group);
      if (!error.ok()) {
        return error;
      }
    } else if (media_type == cricket::MEDIA_TYPE_DATA) {
      if (GetDataMid() && new_content.name != *GetDataMid()) {
        // Ignore all but the first data section.
        RTC_LOG(LS_INFO) << "Ignoring data media section with MID="
                         << new_content.name;
        continue;
      }
      RTCError error = UpdateDataChannel(source, new_content, bundle_group);
      if (!error.ok()) {
        return error;
      }
    } else {
      LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                           "Unknown section type.");
    }
  }

  return RTCError::OK();
}

RTCError PeerConnection::UpdateTransceiverChannel(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver,
    const cricket::ContentInfo& content,
    const cricket::ContentGroup* bundle_group) {
  RTC_DCHECK(IsUnifiedPlan());
  RTC_DCHECK(transceiver);
  cricket::ChannelInterface* channel = transceiver->internal()->channel();
  if (content.rejected) {
    if (channel) {
      transceiver->internal()->SetChannel(nullptr);
      DestroyChannelInterface(channel);
    }
  } else {
    if (!channel) {
      if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
        channel = CreateVoiceChannel(content.name);
      } else {
        RTC_DCHECK_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->media_type());
        channel = CreateVideoChannel(content.name);
      }
      if (!channel) {
        LOG_AND_RETURN_ERROR(
            RTCErrorType::INTERNAL_ERROR,
            "Failed to create channel for mid=" + content.name);
      }
      transceiver->internal()->SetChannel(channel);
    }
  }
  return RTCError::OK();
}

RTCError PeerConnection::UpdateDataChannel(
    cricket::ContentSource source,
    const cricket::ContentInfo& content,
    const cricket::ContentGroup* bundle_group) {
  if (data_channel_type_ == cricket::DCT_NONE) {
    // If data channels are disabled, ignore this media section. CreateAnswer
    // will take care of rejecting it.
    return RTCError::OK();
  }
  if (content.rejected) {
    DestroyDataChannel();
  } else {
    if (!rtp_data_channel_ && !sctp_transport_ && !media_transport_) {
      if (!CreateDataChannel(content.name)) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                             "Failed to create data channel.");
      }
    }
    if (source == cricket::CS_REMOTE) {
      const MediaContentDescription* data_desc = content.media_description();
      if (data_desc && cricket::IsRtpProtocol(data_desc->protocol())) {
        UpdateRemoteRtpDataChannels(GetActiveStreams(data_desc));
      }
    }
  }
  return RTCError::OK();
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
PeerConnection::AssociateTransceiver(cricket::ContentSource source,
                                     SdpType type,
                                     size_t mline_index,
                                     const ContentInfo& content,
                                     const ContentInfo* old_local_content,
                                     const ContentInfo* old_remote_content) {
  RTC_DCHECK(IsUnifiedPlan());
  // If this is an offer then the m= section might be recycled. If the m=
  // section is being recycled (defined as: rejected in the current local or
  // remote description and not rejected in new description), dissociate the
  // currently associated RtpTransceiver by setting its mid property to null,
  // and discard the mapping between the transceiver and its m= section index.
  if (IsMediaSectionBeingRecycled(type, content, old_local_content,
                                  old_remote_content)) {
    // We want to dissociate the transceiver that has the rejected mid.
    const std::string& old_mid =
        (old_local_content && old_local_content->rejected)
            ? old_local_content->name
            : old_remote_content->name;
    auto old_transceiver = GetAssociatedTransceiver(old_mid);
    if (old_transceiver) {
      RTC_LOG(LS_INFO) << "Dissociating transceiver for MID=" << old_mid
                       << " since the media section is being recycled.";
      old_transceiver->internal()->set_mid(absl::nullopt);
      old_transceiver->internal()->set_mline_index(absl::nullopt);
    }
  }
  const MediaContentDescription* media_desc = content.media_description();
  auto transceiver = GetAssociatedTransceiver(content.name);
  if (source == cricket::CS_LOCAL) {
    // Find the RtpTransceiver that corresponds to this m= section, using the
    // mapping between transceivers and m= section indices established when
    // creating the offer.
    if (!transceiver) {
      transceiver = GetTransceiverByMLineIndex(mline_index);
    }
    if (!transceiver) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "Unknown transceiver");
    }
  } else {
    RTC_DCHECK_EQ(source, cricket::CS_REMOTE);
    // If the m= section is sendrecv or recvonly, and there are RtpTransceivers
    // of the same type...
    if (!transceiver &&
        RtpTransceiverDirectionHasRecv(media_desc->direction())) {
      transceiver = FindAvailableTransceiverToReceive(media_desc->type());
    }
    // If no RtpTransceiver was found in the previous step, create one with a
    // recvonly direction.
    if (!transceiver) {
      RTC_LOG(LS_INFO) << "Adding "
                       << cricket::MediaTypeToString(media_desc->type())
                       << " transceiver for MID=" << content.name
                       << " at i=" << mline_index
                       << " in response to the remote description.";
      std::string sender_id = rtc::CreateRandomUuid();
      auto sender =
          CreateSender(media_desc->type(), sender_id, nullptr, {}, {});
      std::string receiver_id;
      if (!media_desc->streams().empty()) {
        receiver_id = media_desc->streams()[0].id;
      } else {
        receiver_id = rtc::CreateRandomUuid();
      }
      auto receiver = CreateReceiver(media_desc->type(), receiver_id);
      transceiver = CreateAndAddTransceiver(sender, receiver);
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kRecvOnly);
    }
  }
  RTC_DCHECK(transceiver);
  if (transceiver->media_type() != media_desc->type()) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Transceiver type does not match media description type.");
  }
  // Associate the found or created RtpTransceiver with the m= section by
  // setting the value of the RtpTransceiver's mid property to the MID of the m=
  // section, and establish a mapping between the transceiver and the index of
  // the m= section.
  transceiver->internal()->set_mid(content.name);
  transceiver->internal()->set_mline_index(mline_index);
  return std::move(transceiver);
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetAssociatedTransceiver(const std::string& mid) const {
  RTC_DCHECK(IsUnifiedPlan());
  for (auto transceiver : transceivers_) {
    if (transceiver->mid() == mid) {
      return transceiver;
    }
  }
  return nullptr;
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetTransceiverByMLineIndex(size_t mline_index) const {
  RTC_DCHECK(IsUnifiedPlan());
  for (auto transceiver : transceivers_) {
    if (transceiver->internal()->mline_index() == mline_index) {
      return transceiver;
    }
  }
  return nullptr;
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::FindAvailableTransceiverToReceive(
    cricket::MediaType media_type) const {
  RTC_DCHECK(IsUnifiedPlan());
  // From JSEP section 5.10 (Applying a Remote Description):
  // If the m= section is sendrecv or recvonly, and there are RtpTransceivers of
  // the same type that were added to the PeerConnection by addTrack and are not
  // associated with any m= section and are not stopped, find the first such
  // RtpTransceiver.
  for (auto transceiver : transceivers_) {
    if (transceiver->media_type() == media_type &&
        transceiver->internal()->created_by_addtrack() && !transceiver->mid() &&
        !transceiver->stopped()) {
      return transceiver;
    }
  }
  return nullptr;
}

const cricket::ContentInfo* PeerConnection::FindMediaSectionForTransceiver(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver,
    const SessionDescriptionInterface* sdesc) const {
  RTC_DCHECK(transceiver);
  RTC_DCHECK(sdesc);
  if (IsUnifiedPlan()) {
    if (!transceiver->internal()->mid()) {
      // This transceiver is not associated with a media section yet.
      return nullptr;
    }
    return sdesc->description()->GetContentByName(
        *transceiver->internal()->mid());
  } else {
    // Plan B only allows at most one audio and one video section, so use the
    // first media section of that type.
    return cricket::GetFirstMediaContent(sdesc->description()->contents(),
                                         transceiver->media_type());
  }
}

PeerConnectionInterface::RTCConfiguration PeerConnection::GetConfiguration() {
  return configuration_;
}

bool PeerConnection::SetConfiguration(const RTCConfiguration& configuration,
                                      RTCError* error) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetConfiguration");
  if (IsClosed()) {
    RTC_LOG(LS_ERROR) << "SetConfiguration: PeerConnection is closed.";
    return SafeSetError(RTCErrorType::INVALID_STATE, error);
  }

  // According to JSEP, after setLocalDescription, changing the candidate pool
  // size is not allowed, and changing the set of ICE servers will not result
  // in new candidates being gathered.
  if (local_description() && configuration.ice_candidate_pool_size !=
                                 configuration_.ice_candidate_pool_size) {
    RTC_LOG(LS_ERROR) << "Can't change candidate pool size after calling "
                         "SetLocalDescription.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
  }

  if (local_description() &&
      configuration.use_media_transport != configuration_.use_media_transport) {
    RTC_LOG(LS_ERROR) << "Can't change media_transport after calling "
                         "SetLocalDescription.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
  }

  if (remote_description() &&
      configuration.use_media_transport != configuration_.use_media_transport) {
    RTC_LOG(LS_ERROR) << "Can't change media_transport after calling "
                         "SetRemoteDescription.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
  }

  if (local_description() &&
      configuration.use_media_transport_for_data_channels !=
          configuration_.use_media_transport_for_data_channels) {
    RTC_LOG(LS_ERROR) << "Can't change media_transport_for_data_channels "
                         "after calling SetLocalDescription.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
  }

  if (remote_description() &&
      configuration.use_media_transport_for_data_channels !=
          configuration_.use_media_transport_for_data_channels) {
    RTC_LOG(LS_ERROR) << "Can't change media_transport_for_data_channels "
                         "after calling SetRemoteDescription.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
  }

  if (local_description() &&
      configuration.crypto_options != configuration_.crypto_options) {
    RTC_LOG(LS_ERROR) << "Can't change crypto_options after calling "
                         "SetLocalDescription.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
  }

  if (configuration.use_media_transport_for_data_channels ||
      configuration.use_media_transport) {
    RTC_CHECK(configuration.bundle_policy == kBundlePolicyMaxBundle)
        << "Media transport requires MaxBundle policy.";
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
  modified_config.ice_check_interval_strong_connectivity =
      configuration.ice_check_interval_strong_connectivity;
  modified_config.ice_check_interval_weak_connectivity =
      configuration.ice_check_interval_weak_connectivity;
  modified_config.ice_unwritable_timeout = configuration.ice_unwritable_timeout;
  modified_config.ice_unwritable_min_checks =
      configuration.ice_unwritable_min_checks;
  modified_config.stun_candidate_keepalive_interval =
      configuration.stun_candidate_keepalive_interval;
  modified_config.turn_customizer = configuration.turn_customizer;
  modified_config.network_preference = configuration.network_preference;
  modified_config.active_reset_srtp_params =
      configuration.active_reset_srtp_params;
  modified_config.use_media_transport = configuration.use_media_transport;
  modified_config.use_media_transport_for_data_channels =
      configuration.use_media_transport_for_data_channels;
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
      configuration.ice_candidate_pool_size > static_cast<int>(UINT16_MAX)) {
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
                    modified_config.prune_turn_ports,
                    modified_config.turn_customizer,
                    modified_config.stun_candidate_keepalive_interval))) {
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

  transport_controller_->SetIceConfig(ParseIceConfig(modified_config));
  transport_controller_->SetMediaTransportFactory(
      modified_config.use_media_transport ||
              modified_config.use_media_transport_for_data_channels
          ? factory_->media_transport_factory()
          : nullptr);

  if (configuration_.active_reset_srtp_params !=
      modified_config.active_reset_srtp_params) {
    transport_controller_->SetActiveResetSrtpParams(
        modified_config.active_reset_srtp_params);
  }

  configuration_ = modified_config;
  return SafeSetError(RTCErrorType::NONE, error);
}

bool PeerConnection::AddIceCandidate(
    const IceCandidateInterface* ice_candidate) {
  TRACE_EVENT0("webrtc", "PeerConnection::AddIceCandidate");
  if (IsClosed()) {
    RTC_LOG(LS_ERROR) << "AddIceCandidate: PeerConnection is closed.";
    NoteAddIceCandidateResult(kAddIceCandidateFailClosed);
    return false;
  }

  if (!remote_description()) {
    RTC_LOG(LS_ERROR) << "AddIceCandidate: ICE candidates can't be added "
                         "without any remote session description.";
    NoteAddIceCandidateResult(kAddIceCandidateFailNoRemoteDescription);
    return false;
  }

  if (!ice_candidate) {
    RTC_LOG(LS_ERROR) << "AddIceCandidate: Candidate is null.";
    NoteAddIceCandidateResult(kAddIceCandidateFailNullCandidate);
    return false;
  }

  bool valid = false;
  bool ready = ReadyToUseRemoteCandidate(ice_candidate, nullptr, &valid);
  if (!valid) {
    NoteAddIceCandidateResult(kAddIceCandidateFailNotValid);
    return false;
  }

  // Add this candidate to the remote session description.
  if (!mutable_remote_description()->AddCandidate(ice_candidate)) {
    RTC_LOG(LS_ERROR) << "AddIceCandidate: Candidate cannot be used.";
    NoteAddIceCandidateResult(kAddIceCandidateFailInAddition);
    return false;
  }

  if (ready) {
    bool result = UseCandidate(ice_candidate);
    if (result) {
      NoteUsageEvent(UsageEvent::REMOTE_CANDIDATE_ADDED);
      NoteAddIceCandidateResult(kAddIceCandidateSuccess);
    } else {
      NoteAddIceCandidateResult(kAddIceCandidateFailNotUsable);
    }
    return result;
  } else {
    RTC_LOG(LS_INFO) << "AddIceCandidate: Not ready to use candidate.";
    NoteAddIceCandidateResult(kAddIceCandidateFailNotReady);
    return true;
  }
}

bool PeerConnection::RemoveIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveIceCandidates");
  if (IsClosed()) {
    RTC_LOG(LS_ERROR) << "RemoveIceCandidates: PeerConnection is closed.";
    return false;
  }

  if (!remote_description()) {
    RTC_LOG(LS_ERROR) << "RemoveIceCandidates: ICE candidates can't be removed "
                         "without any remote session description.";
    return false;
  }

  if (candidates.empty()) {
    RTC_LOG(LS_ERROR) << "RemoveIceCandidates: candidates are empty.";
    return false;
  }

  size_t number_removed =
      mutable_remote_description()->RemoveCandidates(candidates);
  if (number_removed != candidates.size()) {
    RTC_LOG(LS_ERROR)
        << "RemoveIceCandidates: Failed to remove candidates. Requested "
        << candidates.size() << " but only " << number_removed
        << " are removed.";
  }

  // Remove the candidates from the transport controller.
  RTCError error = transport_controller_->RemoveRemoteCandidates(candidates);
  if (!error.ok()) {
    RTC_LOG(LS_ERROR)
        << "RemoveIceCandidates: Error when removing remote candidates: "
        << error.message();
  }
  return true;
}

RTCError PeerConnection::SetBitrate(const BitrateSettings& bitrate) {
  if (!worker_thread()->IsCurrent()) {
    return worker_thread()->Invoke<RTCError>(
        RTC_FROM_HERE, [&]() { return SetBitrate(bitrate); });
  }

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
  call_->GetTransportControllerSend()->SetClientBitratePreferences(bitrate);

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
          absl::WrapUnique<rtc::BitrateAllocationStrategy>(strategy_raw));
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
  auto audio_transceiver = GetFirstAudioTransceiver();
  if (!audio_transceiver || !audio_transceiver->internal()->channel()) {
    return nullptr;
  }
  return transport_controller_->GetRemoteSSLCertChain(
      audio_transceiver->internal()->channel()->transport_name());
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetFirstAudioTransceiver() const {
  for (auto transceiver : transceivers_) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      return transceiver;
    }
  }
  return nullptr;
}

bool PeerConnection::StartRtcEventLog(rtc::PlatformFile file,
                                      int64_t max_size_bytes) {
  // TODO(eladalon): It would be better to not allow negative values into PC.
  const size_t max_size = (max_size_bytes < 0)
                              ? RtcEventLog::kUnlimitedOutput
                              : rtc::saturated_cast<size_t>(max_size_bytes);
  return StartRtcEventLog(
      absl::make_unique<RtcEventLogOutputFile>(file, max_size),
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
  NoteUsageEvent(UsageEvent::CLOSE_CALLED);

  for (const auto& transceiver : transceivers_) {
    transceiver->Stop();
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
  webrtc_session_desc_factory_.reset();
  transport_controller_.reset();

  network_thread()->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                               port_allocator_.get()));

  worker_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
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
    case MSG_FREE_DATACHANNELS: {
      sctp_data_channels_to_free_.clear();
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
  audio_receiver->SetupMediaChannel(remote_sender_info.first_ssrc);
  auto receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
      signaling_thread(), audio_receiver);
  GetAudioTransceiver()->internal()->AddReceiver(receiver);
  Observer()->OnAddTrack(receiver, std::move(streams));
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
  video_receiver->SetupMediaChannel(remote_sender_info.first_ssrc);
  auto receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
      signaling_thread(), video_receiver);
  GetVideoTransceiver()->internal()->AddReceiver(receiver);
  Observer()->OnAddTrack(receiver, std::move(streams));
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
  Observer()->OnIceConnectionChange(ice_connection_state_);
}

void PeerConnection::SetStandardizedIceConnectionState(
    PeerConnectionInterface::IceConnectionState new_state) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (standardized_ice_connection_state_ == new_state)
    return;
  if (IsClosed())
    return;
  standardized_ice_connection_state_ = new_state;
  // TODO(jonasolsson): Pass this value on to OnIceConnectionChange instead of
  // the old one once disconnects are handled properly.
}

void PeerConnection::SetConnectionState(
    PeerConnectionInterface::PeerConnectionState new_state) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (connection_state_ == new_state)
    return;
  if (IsClosed())
    return;
  connection_state_ = new_state;
  Observer()->OnConnectionChange(new_state);
}

void PeerConnection::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  ice_gathering_state_ = new_state;
  Observer()->OnIceGatheringChange(ice_gathering_state_);
}

void PeerConnection::OnIceCandidate(
    std::unique_ptr<IceCandidateInterface> candidate) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  NoteUsageEvent(UsageEvent::CANDIDATE_COLLECTED);
  if (candidate->candidate().type() == LOCAL_PORT_TYPE &&
      candidate->candidate().address().IsPrivateIP()) {
    NoteUsageEvent(UsageEvent::PRIVATE_CANDIDATE_COLLECTED);
  }
  Observer()->OnIceCandidate(candidate.get());
}

void PeerConnection::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  Observer()->OnIceCandidatesRemoved(candidates);
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
    Observer()->OnIceConnectionChange(ice_connection_state_);
    standardized_ice_connection_state_ =
        PeerConnectionInterface::IceConnectionState::kIceConnectionClosed;
    connection_state_ = PeerConnectionInterface::PeerConnectionState::kClosed;
    Observer()->OnConnectionChange(connection_state_);
    if (ice_gathering_state_ != kIceGatheringComplete) {
      ice_gathering_state_ = kIceGatheringComplete;
      Observer()->OnIceGatheringChange(ice_gathering_state_);
    }
  }
  Observer()->OnSignalingChange(signaling_state_);
}

void PeerConnection::OnAudioTrackAdded(AudioTrackInterface* track,
                                       MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  AddAudioTrack(track, stream);
  Observer()->OnRenegotiationNeeded();
}

void PeerConnection::OnAudioTrackRemoved(AudioTrackInterface* track,
                                         MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  RemoveAudioTrack(track, stream);
  Observer()->OnRenegotiationNeeded();
}

void PeerConnection::OnVideoTrackAdded(VideoTrackInterface* track,
                                       MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  AddVideoTrack(track, stream);
  Observer()->OnRenegotiationNeeded();
}

void PeerConnection::OnVideoTrackRemoved(VideoTrackInterface* track,
                                         MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  RemoveVideoTrack(track, stream);
  Observer()->OnRenegotiationNeeded();
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

void PeerConnection::GetOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  ExtractSharedMediaSessionOptions(offer_answer_options, session_options);

  if (IsUnifiedPlan()) {
    GetOptionsForUnifiedPlanOffer(offer_answer_options, session_options);
  } else {
    GetOptionsForPlanBOffer(offer_answer_options, session_options);
  }

  // Intentionally unset the data channel type for RTP data channel with the
  // second condition. Otherwise the RTP data channels would be successfully
  // negotiated by default and the unit tests in WebRtcDataBrowserTest will fail
  // when building with chromium. We want to leave RTP data channels broken, so
  // people won't try to use them.
  if (!rtp_data_channels_.empty() || data_channel_type() != cricket::DCT_RTP) {
    session_options->data_channel_type = data_channel_type();
  }

  // Apply ICE restart flag and renomination flag.
  for (auto& options : session_options->media_description_options) {
    options.transport_options.ice_restart = offer_answer_options.ice_restart;
    options.transport_options.enable_ice_renomination =
        configuration_.enable_ice_renomination;
  }

  session_options->rtcp_cname = rtcp_cname_;
  session_options->crypto_options = GetCryptoOptions();
  session_options->is_unified_plan = IsUnifiedPlan();
  session_options->pooled_ice_credentials =
      network_thread()->Invoke<std::vector<cricket::IceParameters>>(
          RTC_FROM_HERE,
          rtc::Bind(&cricket::PortAllocator::GetPooledIceCredentials,
                    port_allocator_.get()));
  session_options->offer_extmap_allow_mixed =
      configuration_.offer_extmap_allow_mixed;
}

void PeerConnection::GetOptionsForPlanBOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
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
  if (offer_answer_options.offer_to_receive_audio !=
      RTCOfferAnswerOptions::kUndefined) {
    recv_audio = (offer_answer_options.offer_to_receive_audio > 0);
    offer_new_audio_description =
        offer_new_audio_description ||
        (offer_answer_options.offer_to_receive_audio > 0);
  }
  if (offer_answer_options.offer_to_receive_video !=
      RTCOfferAnswerOptions::kUndefined) {
    recv_video = (offer_answer_options.offer_to_receive_video > 0);
    offer_new_video_description =
        offer_new_video_description ||
        (offer_answer_options.offer_to_receive_video > 0);
  }

  absl::optional<size_t> audio_index;
  absl::optional<size_t> video_index;
  absl::optional<size_t> data_index;
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
        GetMediaDescriptionOptionsForActiveData(cricket::CN_DATA));
    data_index = session_options->media_description_options.size() - 1;
  }

  cricket::MediaDescriptionOptions* audio_media_description_options =
      !audio_index ? nullptr
                   : &session_options->media_description_options[*audio_index];
  cricket::MediaDescriptionOptions* video_media_description_options =
      !video_index ? nullptr
                   : &session_options->media_description_options[*video_index];

  AddRtpSenderOptions(GetSendersInternal(), audio_media_description_options,
                      video_media_description_options,
                      offer_answer_options.num_simulcast_layers);
}

// Find a new MID that is not already in |used_mids|, then add it to |used_mids|
// and return a reference to it.
// Generated MIDs should be no more than 3 bytes long to take up less space in
// the RTP packet.
static const std::string& AllocateMid(std::set<std::string>* used_mids) {
  RTC_DCHECK(used_mids);
  // We're boring: just generate MIDs 0, 1, 2, ...
  size_t i = 0;
  std::set<std::string>::iterator it;
  bool inserted;
  do {
    std::string mid = rtc::ToString(i++);
    auto insert_result = used_mids->insert(mid);
    it = insert_result.first;
    inserted = insert_result.second;
  } while (!inserted);
  return *it;
}

static cricket::MediaDescriptionOptions
GetMediaDescriptionOptionsForTransceiver(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver,
    const std::string& mid) {
  cricket::MediaDescriptionOptions media_description_options(
      transceiver->media_type(), mid, transceiver->direction(),
      transceiver->stopped());
  // This behavior is specified in JSEP. The gist is that:
  // 1. The MSID is included if the RtpTransceiver's direction is sendonly or
  //    sendrecv.
  // 2. If the MSID is included, then it must be included in any subsequent
  //    offer/answer exactly the same until the RtpTransceiver is stopped.
  if (!transceiver->stopped() &&
      (RtpTransceiverDirectionHasSend(transceiver->direction()) ||
       transceiver->internal()->has_ever_been_used_to_send())) {
    cricket::SenderOptions sender_options;
    sender_options.track_id = transceiver->sender()->id();
    sender_options.stream_ids = transceiver->sender()->stream_ids();
    int num_send_encoding_layers =
        transceiver->sender()->init_send_encodings().size();
    sender_options.num_sim_layers =
        !num_send_encoding_layers ? 1 : num_send_encoding_layers;
    media_description_options.sender_options.push_back(sender_options);
  }
  return media_description_options;
}

// Returns the ContentInfo at mline index |i|, or null if none exists.
static const ContentInfo* GetContentByIndex(
    const SessionDescriptionInterface* sdesc,
    size_t i) {
  if (!sdesc) {
    return nullptr;
  }
  const ContentInfos& contents = sdesc->description()->contents();
  return (i < contents.size() ? &contents[i] : nullptr);
}

void PeerConnection::GetOptionsForUnifiedPlanOffer(
    const RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  // Rules for generating an offer are dictated by JSEP sections 5.2.1 (Initial
  // Offers) and 5.2.2 (Subsequent Offers).
  RTC_DCHECK_EQ(session_options->media_description_options.size(), 0);
  const ContentInfos& local_contents =
      (local_description() ? local_description()->description()->contents()
                           : ContentInfos());
  const ContentInfos& remote_contents =
      (remote_description() ? remote_description()->description()->contents()
                            : ContentInfos());
  // The mline indices that can be recycled. New transceivers should reuse these
  // slots first.
  std::queue<size_t> recycleable_mline_indices;
  // Track the MIDs used in previous offer/answer exchanges and the current
  // offer so that new, unique MIDs are generated.
  std::set<std::string> used_mids = seen_mids_;
  // First, go through each media section that exists in either the local or
  // remote description and generate a media section in this offer for the
  // associated transceiver. If a media section can be recycled, generate a
  // default, rejected media section here that can be later overwritten.
  for (size_t i = 0;
       i < std::max(local_contents.size(), remote_contents.size()); ++i) {
    // Either |local_content| or |remote_content| is non-null.
    const ContentInfo* local_content =
        (i < local_contents.size() ? &local_contents[i] : nullptr);
    const ContentInfo* current_local_content =
        GetContentByIndex(current_local_description(), i);
    const ContentInfo* remote_content =
        (i < remote_contents.size() ? &remote_contents[i] : nullptr);
    const ContentInfo* current_remote_content =
        GetContentByIndex(current_remote_description(), i);
    bool had_been_rejected =
        (current_local_content && current_local_content->rejected) ||
        (current_remote_content && current_remote_content->rejected);
    const std::string& mid =
        (local_content ? local_content->name : remote_content->name);
    cricket::MediaType media_type =
        (local_content ? local_content->media_description()->type()
                       : remote_content->media_description()->type());
    if (media_type == cricket::MEDIA_TYPE_AUDIO ||
        media_type == cricket::MEDIA_TYPE_VIDEO) {
      auto transceiver = GetAssociatedTransceiver(mid);
      RTC_CHECK(transceiver);
      // A media section is considered eligible for recycling if it is marked as
      // rejected in either the current local or current remote description.
      if (had_been_rejected && transceiver->stopped()) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(transceiver->media_type(), mid,
                                             RtpTransceiverDirection::kInactive,
                                             /*stopped=*/true));
        recycleable_mline_indices.push(i);
      } else {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForTransceiver(transceiver, mid));
        // CreateOffer shouldn't really cause any state changes in
        // PeerConnection, but we need a way to match new transceivers to new
        // media sections in SetLocalDescription and JSEP specifies this is done
        // by recording the index of the media section generated for the
        // transceiver in the offer.
        transceiver->internal()->set_mline_index(i);
      }
    } else {
      RTC_CHECK_EQ(cricket::MEDIA_TYPE_DATA, media_type);
      RTC_CHECK(GetDataMid());
      if (had_been_rejected || mid != *GetDataMid()) {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForRejectedData(mid));
      } else {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForActiveData(mid));
      }
    }
  }
  // Next, look for transceivers that are newly added (that is, are not stopped
  // and not associated). Reuse media sections marked as recyclable first,
  // otherwise append to the end of the offer. New media sections should be
  // added in the order they were added to the PeerConnection.
  for (const auto& transceiver : transceivers_) {
    if (transceiver->mid() || transceiver->stopped()) {
      continue;
    }
    size_t mline_index;
    if (!recycleable_mline_indices.empty()) {
      mline_index = recycleable_mline_indices.front();
      recycleable_mline_indices.pop();
      session_options->media_description_options[mline_index] =
          GetMediaDescriptionOptionsForTransceiver(transceiver,
                                                   AllocateMid(&used_mids));
    } else {
      mline_index = session_options->media_description_options.size();
      session_options->media_description_options.push_back(
          GetMediaDescriptionOptionsForTransceiver(transceiver,
                                                   AllocateMid(&used_mids)));
    }
    // See comment above for why CreateOffer changes the transceiver's state.
    transceiver->internal()->set_mline_index(mline_index);
  }
  // Lastly, add a m-section if we have local data channels and an m section
  // does not already exist.
  if (!GetDataMid() && HasDataChannels()) {
    session_options->media_description_options.push_back(
        GetMediaDescriptionOptionsForActiveData(AllocateMid(&used_mids)));
  }
}

void PeerConnection::GetOptionsForAnswer(
    const RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  ExtractSharedMediaSessionOptions(offer_answer_options, session_options);

  if (IsUnifiedPlan()) {
    GetOptionsForUnifiedPlanAnswer(offer_answer_options, session_options);
  } else {
    GetOptionsForPlanBAnswer(offer_answer_options, session_options);
  }

  // Intentionally unset the data channel type for RTP data channel. Otherwise
  // the RTP data channels would be successfully negotiated by default and the
  // unit tests in WebRtcDataBrowserTest will fail when building with chromium.
  // We want to leave RTP data channels broken, so people won't try to use them.
  if (!rtp_data_channels_.empty() || data_channel_type() != cricket::DCT_RTP) {
    session_options->data_channel_type = data_channel_type();
  }

  // Apply ICE renomination flag.
  for (auto& options : session_options->media_description_options) {
    options.transport_options.enable_ice_renomination =
        configuration_.enable_ice_renomination;
  }

  session_options->rtcp_cname = rtcp_cname_;
  session_options->crypto_options = GetCryptoOptions();
  session_options->is_unified_plan = IsUnifiedPlan();
  session_options->pooled_ice_credentials =
      network_thread()->Invoke<std::vector<cricket::IceParameters>>(
          RTC_FROM_HERE,
          rtc::Bind(&cricket::PortAllocator::GetPooledIceCredentials,
                    port_allocator_.get()));
}

void PeerConnection::GetOptionsForPlanBAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  // Figure out transceiver directional preferences.
  bool send_audio = HasRtpSender(cricket::MEDIA_TYPE_AUDIO);
  bool send_video = HasRtpSender(cricket::MEDIA_TYPE_VIDEO);

  // By default, generate sendrecv/recvonly m= sections. The direction is also
  // restricted by the direction in the offer.
  bool recv_audio = true;
  bool recv_video = true;

  // The "offer_to_receive_X" options allow those defaults to be overridden.
  if (offer_answer_options.offer_to_receive_audio !=
      RTCOfferAnswerOptions::kUndefined) {
    recv_audio = (offer_answer_options.offer_to_receive_audio > 0);
  }
  if (offer_answer_options.offer_to_receive_video !=
      RTCOfferAnswerOptions::kUndefined) {
    recv_video = (offer_answer_options.offer_to_receive_video > 0);
  }

  absl::optional<size_t> audio_index;
  absl::optional<size_t> video_index;
  absl::optional<size_t> data_index;

  // Generate m= sections that match those in the offer.
  // Note that mediasession.cc will handle intersection our preferred
  // direction with the offered direction.
  GenerateMediaDescriptionOptions(
      remote_description(),
      RtpTransceiverDirectionFromSendRecv(send_audio, recv_audio),
      RtpTransceiverDirectionFromSendRecv(send_video, recv_video), &audio_index,
      &video_index, &data_index, session_options);

  cricket::MediaDescriptionOptions* audio_media_description_options =
      !audio_index ? nullptr
                   : &session_options->media_description_options[*audio_index];
  cricket::MediaDescriptionOptions* video_media_description_options =
      !video_index ? nullptr
                   : &session_options->media_description_options[*video_index];

  AddRtpSenderOptions(GetSendersInternal(), audio_media_description_options,
                      video_media_description_options,
                      offer_answer_options.num_simulcast_layers);
}

void PeerConnection::GetOptionsForUnifiedPlanAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  // Rules for generating an answer are dictated by JSEP sections 5.3.1 (Initial
  // Answers) and 5.3.2 (Subsequent Answers).
  RTC_DCHECK(remote_description());
  RTC_DCHECK(remote_description()->GetType() == SdpType::kOffer);
  for (const ContentInfo& content :
       remote_description()->description()->contents()) {
    cricket::MediaType media_type = content.media_description()->type();
    if (media_type == cricket::MEDIA_TYPE_AUDIO ||
        media_type == cricket::MEDIA_TYPE_VIDEO) {
      auto transceiver = GetAssociatedTransceiver(content.name);
      RTC_CHECK(transceiver);
      session_options->media_description_options.push_back(
          GetMediaDescriptionOptionsForTransceiver(transceiver, content.name));
    } else {
      RTC_CHECK_EQ(cricket::MEDIA_TYPE_DATA, media_type);
      // Reject all data sections if data channels are disabled.
      // Reject a data section if it has already been rejected.
      // Reject all data sections except for the first one.
      if (data_channel_type_ == cricket::DCT_NONE || content.rejected ||
          content.name != *GetDataMid()) {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForRejectedData(content.name));
      } else {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForActiveData(content.name));
      }
    }
  }
}

void PeerConnection::GenerateMediaDescriptionOptions(
    const SessionDescriptionInterface* session_desc,
    RtpTransceiverDirection audio_direction,
    RtpTransceiverDirection video_direction,
    absl::optional<size_t>* audio_index,
    absl::optional<size_t>* video_index,
    absl::optional<size_t>* data_index,
    cricket::MediaSessionOptions* session_options) {
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
  // Direction for data sections is meaningless, but legacy endpoints might
  // expect sendrecv.
  cricket::MediaDescriptionOptions options(cricket::MEDIA_TYPE_DATA, mid,
                                           RtpTransceiverDirection::kSendRecv,
                                           /*stopped=*/false);
  AddRtpDataChannelOptions(rtp_data_channels_, &options);
  return options;
}

cricket::MediaDescriptionOptions
PeerConnection::GetMediaDescriptionOptionsForRejectedData(
    const std::string& mid) const {
  cricket::MediaDescriptionOptions options(cricket::MEDIA_TYPE_DATA, mid,
                                           RtpTransceiverDirection::kInactive,
                                           /*stopped=*/true);
  AddRtpDataChannelOptions(rtp_data_channels_, &options);
  return options;
}

absl::optional<std::string> PeerConnection::GetDataMid() const {
  switch (data_channel_type_) {
    case cricket::DCT_RTP:
      if (!rtp_data_channel_) {
        return absl::nullopt;
      }
      return rtp_data_channel_->content_name();
    case cricket::DCT_SCTP:
      return sctp_mid_;
    case cricket::DCT_MEDIA_TRANSPORT:
      return media_transport_data_mid_;
    default:
      return absl::nullopt;
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
          RtpSenderInfo(kDefaultStreamId, default_sender_id, 0));
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
    const std::string& channel_label = params.first_stream_id();
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
    std::string label = params.first_stream_id().empty()
                            ? rtc::ToString(params.first_ssrc())
                            : params.first_stream_id();
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
                           "CreateDataChannel failed.";
    return;
  }
  channel->SetReceiveSsrc(remote_ssrc);
  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  Observer()->OnDataChannel(std::move(proxy_channel));
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
  if (DataChannel::IsSctpLike(data_channel_type_)) {
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
                           "because the id is already in use or out of range.";
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
    RTC_DCHECK(DataChannel::IsSctpLike(data_channel_type_));
    sctp_data_channels_.push_back(channel);
    channel->SignalClosed.connect(this,
                                  &PeerConnection::OnSctpDataChannelClosed);
  }

  SignalDataChannelCreated_(channel.get());
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
        // After the closing procedure is done, it's safe to use this ID for
        // another data channel.
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
  Observer()->OnDataChannel(std::move(proxy_channel));
  NoteUsageEvent(UsageEvent::DATA_ADDED);
}

bool PeerConnection::HandleOpenMessage_s(
    const cricket::ReceiveDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  if (params.type == cricket::DMT_CONTROL && IsOpenMessage(buffer)) {
    // Received OPEN message; parse and signal that a new data channel should
    // be created.
    std::string label;
    InternalDataChannelInit config;
    config.id = params.ssrc;
    if (!ParseDataChannelOpenMessage(buffer, &label, &config)) {
      RTC_LOG(LS_WARNING) << "Failed to parse the OPEN message for ssrc "
                          << params.ssrc;
      return true;
    }
    config.open_handshake_role = InternalDataChannelInit::kAcker;
    OnDataChannelOpenMessage(label, config);
    return true;
  }
  return false;
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
PeerConnection::GetAudioTransceiver() const {
  // This method only works with Plan B SDP, where there is a single
  // audio/video transceiver.
  RTC_DCHECK(!IsUnifiedPlan());
  for (auto transceiver : transceivers_) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
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
    if (transceiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
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
  for (const auto& transceiver : transceivers_) {
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
  for (const auto& transceiver : transceivers_) {
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
  for (const auto& transceiver : transceivers_) {
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

DataChannel* PeerConnection::FindDataChannelBySid(int sid) const {
  for (const auto& channel : sctp_data_channels_) {
    if (channel->id() == sid) {
      return channel;
    }
  }
  return nullptr;
}

bool PeerConnection::InitializePortAllocator_n(
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    const RTCConfiguration& configuration) {
  port_allocator_->Initialize();
  // To handle both internal and externally created port allocator, we will
  // enable BUNDLE here.
  port_allocator_flags_ = port_allocator_->flags();
  port_allocator_flags_ |= cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                           cricket::PORTALLOCATOR_ENABLE_IPV6 |
                           cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI;
  // If the disable-IPv6 flag was specified, we'll not override it
  // by experiment.
  if (configuration.disable_ipv6) {
    port_allocator_flags_ &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  } else if (webrtc::field_trial::FindFullName("WebRTC-IPv6Default")
                 .find("Disabled") == 0) {
    port_allocator_flags_ &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  }

  if (configuration.disable_ipv6_on_wifi) {
    port_allocator_flags_ &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI);
    RTC_LOG(LS_INFO) << "IPv6 candidates on Wi-Fi are disabled.";
  }

  if (configuration.tcp_candidate_policy == kTcpCandidatePolicyDisabled) {
    port_allocator_flags_ |= cricket::PORTALLOCATOR_DISABLE_TCP;
    RTC_LOG(LS_INFO) << "TCP candidates are disabled.";
  }

  if (configuration.candidate_network_policy ==
      kCandidateNetworkPolicyLowCost) {
    port_allocator_flags_ |= cricket::PORTALLOCATOR_DISABLE_COSTLY_NETWORKS;
    RTC_LOG(LS_INFO) << "Do not gather candidates on high-cost networks";
  }

  if (configuration.disable_link_local_networks) {
    port_allocator_flags_ |= cricket::PORTALLOCATOR_DISABLE_LINK_LOCAL_NETWORKS;
    RTC_LOG(LS_INFO) << "Disable candidates on link-local network interfaces.";
  }

  port_allocator_->set_flags(port_allocator_flags_);
  // No step delay is used while allocating ports.
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);
  port_allocator_->set_candidate_filter(
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
      configuration.ice_candidate_pool_size, configuration.prune_turn_ports,
      configuration.turn_customizer,
      configuration.stun_candidate_keepalive_interval);
  return true;
}

bool PeerConnection::ReconfigurePortAllocator_n(
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    IceTransportsType type,
    int candidate_pool_size,
    bool prune_turn_ports,
    webrtc::TurnCustomizer* turn_customizer,
    absl::optional<int> stun_candidate_keepalive_interval) {
  port_allocator_->set_candidate_filter(
      ConvertIceTransportTypeToCandidateFilter(type));
  // According to JSEP, after setLocalDescription, changing the candidate pool
  // size is not allowed, and changing the set of ICE servers will not result
  // in new candidates being gathered.
  if (local_description()) {
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
      prune_turn_ports, turn_customizer, stun_candidate_keepalive_interval);
}

cricket::ChannelManager* PeerConnection::channel_manager() const {
  return factory_->channel_manager();
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

cricket::ChannelInterface* PeerConnection::GetChannel(
    const std::string& content_name) {
  for (const auto& transceiver : transceivers_) {
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
    RTC_LOG(LS_INFO)
        << "Local and Remote descriptions must be applied to get the "
           "SSL Role of the SCTP transport.";
    return false;
  }
  if (!sctp_transport_ && !media_transport_) {
    RTC_LOG(LS_INFO) << "Non-rejected SCTP m= section is needed to get the "
                        "SSL Role of the SCTP transport.";
    return false;
  }

  absl::optional<rtc::SSLRole> dtls_role;
  if (sctp_mid_) {
    dtls_role = transport_controller_->GetDtlsRole(*sctp_mid_);
  } else if (is_caller_) {
    dtls_role = *is_caller_ ? rtc::SSL_SERVER : rtc::SSL_CLIENT;
  }
  if (dtls_role) {
    *role = *dtls_role;
    return true;
  }
  return false;
}

bool PeerConnection::GetSslRole(const std::string& content_name,
                                rtc::SSLRole* role) {
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

RTCError PeerConnection::UpdateSessionState(
    SdpType type,
    cricket::ContentSource source,
    const cricket::SessionDescription* description) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  // If there's already a pending error then no state transition should happen.
  // But all call-sites should be verifying this before calling us!
  RTC_DCHECK(session_error() == SessionError::kNone);

  // If this is answer-ish we're ready to let media flow.
  if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
    EnableSending();
  }

  // Update the signaling state according to the specified state machine (see
  // https://w3c.github.io/webrtc-pc/#rtcsignalingstate-enum).
  if (type == SdpType::kOffer) {
    ChangeSignalingState(source == cricket::CS_LOCAL
                             ? PeerConnectionInterface::kHaveLocalOffer
                             : PeerConnectionInterface::kHaveRemoteOffer);
  } else if (type == SdpType::kPrAnswer) {
    ChangeSignalingState(source == cricket::CS_LOCAL
                             ? PeerConnectionInterface::kHaveLocalPrAnswer
                             : PeerConnectionInterface::kHaveRemotePrAnswer);
  } else {
    RTC_DCHECK(type == SdpType::kAnswer);
    ChangeSignalingState(PeerConnectionInterface::kStable);
  }

  // Update internal objects according to the session description's media
  // descriptions.
  RTCError error = PushdownMediaDescription(type, source);
  if (!error.ok()) {
    return error;
  }

  return RTCError::OK();
}

RTCError PeerConnection::PushdownMediaDescription(
    SdpType type,
    cricket::ContentSource source) {
  const SessionDescriptionInterface* sdesc =
      (source == cricket::CS_LOCAL ? local_description()
                                   : remote_description());
  RTC_DCHECK(sdesc);

  // Push down the new SDP media section for each audio/video transceiver.
  for (const auto& transceiver : transceivers_) {
    const ContentInfo* content_info =
        FindMediaSectionForTransceiver(transceiver, sdesc);
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
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, std::move(error));
    }
  }

  // If using the RtpDataChannel, push down the new SDP section for it too.
  if (rtp_data_channel_) {
    const ContentInfo* data_content =
        cricket::GetFirstDataContent(sdesc->description());
    if (data_content && !data_content->rejected) {
      const MediaContentDescription* data_desc =
          data_content->media_description();
      if (data_desc) {
        std::string error;
        bool success =
            (source == cricket::CS_LOCAL)
                ? rtp_data_channel_->SetLocalContent(data_desc, type, &error)
                : rtp_data_channel_->SetRemoteContent(data_desc, type, &error);
        if (!success) {
          LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                               std::move(error));
        }
      }
    }
  }

  // Need complete offer/answer with an SCTP m= section before starting SCTP,
  // according to https://tools.ietf.org/html/draft-ietf-mmusic-sctp-sdp-19
  if (sctp_transport_ && local_description() && remote_description() &&
      cricket::GetFirstDataContent(local_description()->description()) &&
      cricket::GetFirstDataContent(remote_description()->description())) {
    bool success = network_thread()->Invoke<bool>(
        RTC_FROM_HERE,
        rtc::Bind(&PeerConnection::PushdownSctpParameters_n, this, source));
    if (!success) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                           "Failed to push down SCTP parameters.");
    }
  }

  return RTCError::OK();
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
  ice_config.ice_check_interval_strong_connectivity =
      config.ice_check_interval_strong_connectivity;
  ice_config.ice_check_interval_weak_connectivity =
      config.ice_check_interval_weak_connectivity;
  ice_config.ice_check_min_interval = config.ice_check_min_interval;
  ice_config.stun_keepalive_interval = config.stun_candidate_keepalive_interval;
  ice_config.regather_all_networks_interval_range =
      config.ice_regather_interval_range;
  ice_config.network_preference = config.network_preference;
  return ice_config;
}

static absl::string_view GetTrackIdBySsrc(
    const SessionDescriptionInterface* session_description,
    uint32_t ssrc) {
  if (!session_description) {
    return {};
  }
  for (const cricket::ContentInfo& content :
       session_description->description()->contents()) {
    const cricket::MediaContentDescription& media =
        *content.media_description();
    if (media.type() == cricket::MEDIA_TYPE_AUDIO ||
        media.type() == cricket::MEDIA_TYPE_VIDEO) {
      const cricket::StreamParams* stream_params =
          cricket::GetStreamBySsrc(media.streams(), ssrc);
      if (stream_params) {
        return stream_params->id;
      }
    }
  }
  return {};
}

absl::string_view PeerConnection::GetLocalTrackIdBySsrc(uint32_t ssrc) {
  return GetTrackIdBySsrc(local_description(), ssrc);
}

absl::string_view PeerConnection::GetRemoteTrackIdBySsrc(uint32_t ssrc) {
  return GetTrackIdBySsrc(remote_description(), ssrc);
}

bool PeerConnection::SendData(const cricket::SendDataParams& params,
                              const rtc::CopyOnWriteBuffer& payload,
                              cricket::SendDataResult* result) {
  if (!rtp_data_channel_ && !sctp_transport_ && !media_transport_) {
    RTC_LOG(LS_ERROR) << "SendData called when rtp_data_channel_, "
                         "sctp_transport_, and media_transport_ are NULL.";
    return false;
  }
  if (media_transport_) {
    SendDataParams send_params;
    send_params.type = ToWebrtcDataMessageType(params.type);
    send_params.ordered = params.ordered;
    if (params.max_rtx_count >= 0) {
      send_params.max_rtx_count = params.max_rtx_count;
    } else if (params.max_rtx_ms >= 0) {
      send_params.max_rtx_ms = params.max_rtx_ms;
    }
    return media_transport_->SendData(params.sid, send_params, payload).ok();
  }
  return rtp_data_channel_
             ? rtp_data_channel_->SendData(params, payload, result)
             : network_thread()->Invoke<bool>(
                   RTC_FROM_HERE,
                   Bind(&cricket::SctpTransportInternal::SendData,
                        sctp_transport_.get(), params, payload, result));
}

bool PeerConnection::ConnectDataChannel(DataChannel* webrtc_data_channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!rtp_data_channel_ && !sctp_transport_ && !media_transport_) {
    // Don't log an error here, because DataChannels are expected to call
    // ConnectDataChannel in this state. It's the only way to initially tell
    // whether or not the underlying transport is ready.
    return false;
  }
  if (media_transport_) {
    SignalMediaTransportWritable_s.connect(webrtc_data_channel,
                                           &DataChannel::OnChannelReady);
    SignalMediaTransportReceivedData_s.connect(webrtc_data_channel,
                                               &DataChannel::OnDataReceived);
    SignalMediaTransportChannelClosing_s.connect(
        webrtc_data_channel, &DataChannel::OnClosingProcedureStartedRemotely);
    SignalMediaTransportChannelClosed_s.connect(
        webrtc_data_channel, &DataChannel::OnClosingProcedureComplete);
  } else if (rtp_data_channel_) {
    rtp_data_channel_->SignalReadyToSendData.connect(
        webrtc_data_channel, &DataChannel::OnChannelReady);
    rtp_data_channel_->SignalDataReceived.connect(webrtc_data_channel,
                                                  &DataChannel::OnDataReceived);
  } else {
    SignalSctpReadyToSendData.connect(webrtc_data_channel,
                                      &DataChannel::OnChannelReady);
    SignalSctpDataReceived.connect(webrtc_data_channel,
                                   &DataChannel::OnDataReceived);
    SignalSctpClosingProcedureStartedRemotely.connect(
        webrtc_data_channel, &DataChannel::OnClosingProcedureStartedRemotely);
    SignalSctpClosingProcedureComplete.connect(
        webrtc_data_channel, &DataChannel::OnClosingProcedureComplete);
  }
  return true;
}

void PeerConnection::DisconnectDataChannel(DataChannel* webrtc_data_channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!rtp_data_channel_ && !sctp_transport_ && !media_transport_) {
    RTC_LOG(LS_ERROR)
        << "DisconnectDataChannel called when rtp_data_channel_ and "
           "sctp_transport_ are NULL.";
    return;
  }
  if (media_transport_) {
    SignalMediaTransportWritable_s.disconnect(webrtc_data_channel);
    SignalMediaTransportReceivedData_s.disconnect(webrtc_data_channel);
    SignalMediaTransportChannelClosing_s.disconnect(webrtc_data_channel);
    SignalMediaTransportChannelClosed_s.disconnect(webrtc_data_channel);
  } else if (rtp_data_channel_) {
    rtp_data_channel_->SignalReadyToSendData.disconnect(webrtc_data_channel);
    rtp_data_channel_->SignalDataReceived.disconnect(webrtc_data_channel);
  } else {
    SignalSctpReadyToSendData.disconnect(webrtc_data_channel);
    SignalSctpDataReceived.disconnect(webrtc_data_channel);
    SignalSctpClosingProcedureStartedRemotely.disconnect(webrtc_data_channel);
    SignalSctpClosingProcedureComplete.disconnect(webrtc_data_channel);
  }
}

void PeerConnection::AddSctpDataStream(int sid) {
  if (media_transport_) {
    // No-op.  Media transport does not need to add streams.
    return;
  }
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
  if (media_transport_) {
    media_transport_->CloseChannel(sid);
    return;
  }
  if (!sctp_transport_) {
    RTC_LOG(LS_ERROR) << "RemoveSctpDataStream called when sctp_transport_ is "
                         "NULL.";
    return;
  }
  network_thread()->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&cricket::SctpTransportInternal::ResetStream,
                               sctp_transport_.get(), sid));
}

bool PeerConnection::ReadyToSendData() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return (rtp_data_channel_ && rtp_data_channel_->ready_to_send_data()) ||
         (media_transport_ && media_transport_ready_to_send_data_) ||
         sctp_ready_to_send_data_;
}

void PeerConnection::OnDataReceived(int channel_id,
                                    DataMessageType type,
                                    const rtc::CopyOnWriteBuffer& buffer) {
  cricket::ReceiveDataParams params;
  params.sid = channel_id;
  params.type = ToCricketDataMessageType(type);
  media_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, params, buffer] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        if (!HandleOpenMessage_s(params, buffer)) {
          SignalMediaTransportReceivedData_s(params, buffer);
        }
      });
}

void PeerConnection::OnChannelClosing(int channel_id) {
  media_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        SignalMediaTransportChannelClosing_s(channel_id);
      });
}

void PeerConnection::OnChannelClosed(int channel_id) {
  media_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        SignalMediaTransportChannelClosed_s(channel_id);
      });
}

absl::optional<std::string> PeerConnection::sctp_transport_name() const {
  if (sctp_mid_ && transport_controller_) {
    auto dtls_transport = transport_controller_->GetDtlsTransport(*sctp_mid_);
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
  std::map<std::string, std::string> transport_names_by_mid;
  for (const auto& transceiver : transceivers_) {
    cricket::ChannelInterface* channel = transceiver->internal()->channel();
    if (channel) {
      transport_names_by_mid[channel->content_name()] =
          channel->transport_name();
    }
  }
  if (rtp_data_channel_) {
    transport_names_by_mid[rtp_data_channel_->content_name()] =
        rtp_data_channel_->transport_name();
  }
  if (sctp_transport_) {
    absl::optional<std::string> transport_name = sctp_transport_name();
    RTC_DCHECK(transport_name);
    transport_names_by_mid[*sctp_mid_] = *transport_name;
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
  SetSessionError(SessionError::kTransport,
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
                           "empty content name in candidate "
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
  RTC_HISTOGRAM_ENUMERATION(
      "WebRTC.PeerConnection.DtlsHandshakeError", static_cast<int>(error),
      static_cast<int>(rtc::SSLHandshakeError::MAX_VALUE));
}

void PeerConnection::EnableSending() {
  for (const auto& transceiver : transceivers_) {
    cricket::ChannelInterface* channel = transceiver->internal()->channel();
    if (channel && !channel->enabled()) {
      channel->Enable(true);
    }
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
  RTCError error =
      transport_controller_->AddRemoteCandidates(content.name, candidates);
  if (error.ok()) {
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

void PeerConnection::RemoveUnusedChannels(const SessionDescription* desc) {
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
    DestroyDataChannel();
  }
}

RTCErrorOr<const cricket::ContentGroup*> PeerConnection::GetEarlyBundleGroup(
    const SessionDescription& desc) const {
  const cricket::ContentGroup* bundle_group = nullptr;
  if (configuration_.bundle_policy ==
      PeerConnectionInterface::kBundlePolicyMaxBundle) {
    bundle_group = desc.GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
    if (!bundle_group) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max-bundle configured but session description "
                           "has no BUNDLE group");
    }
  }
  return std::move(bundle_group);
}

RTCError PeerConnection::CreateChannels(const SessionDescription& desc) {
  // Creating the media channels. Transports should already have been created
  // at this point.
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
  if (data_channel_type_ != cricket::DCT_NONE && data && !data->rejected &&
      !rtp_data_channel_ && !sctp_transport_ && !media_transport_) {
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
  RtpTransportInternal* rtp_transport = GetRtpTransport(mid);
  MediaTransportInterface* media_transport = nullptr;
  if (configuration_.use_media_transport) {
    media_transport = GetMediaTransport(mid);
  }

  cricket::VoiceChannel* voice_channel = channel_manager()->CreateVoiceChannel(
      call_.get(), configuration_.media_config, rtp_transport, media_transport,
      signaling_thread(), mid, SrtpRequired(), GetCryptoOptions(),
      audio_options_);
  if (!voice_channel) {
    return nullptr;
  }
  voice_channel->SignalDtlsSrtpSetupFailure.connect(
      this, &PeerConnection::OnDtlsSrtpSetupFailure);
  voice_channel->SignalSentPacket.connect(this,
                                          &PeerConnection::OnSentPacket_w);
  voice_channel->SetRtpTransport(rtp_transport);

  return voice_channel;
}

// TODO(steveanton): Perhaps this should be managed by the RtpTransceiver.
cricket::VideoChannel* PeerConnection::CreateVideoChannel(
    const std::string& mid) {
  RtpTransportInternal* rtp_transport = GetRtpTransport(mid);

  // TODO(sukhanov): Propagate media_transport to video channel.
  cricket::VideoChannel* video_channel = channel_manager()->CreateVideoChannel(
      call_.get(), configuration_.media_config, rtp_transport,
      signaling_thread(), mid, SrtpRequired(), GetCryptoOptions(),
      video_options_);
  if (!video_channel) {
    return nullptr;
  }
  video_channel->SignalDtlsSrtpSetupFailure.connect(
      this, &PeerConnection::OnDtlsSrtpSetupFailure);
  video_channel->SignalSentPacket.connect(this,
                                          &PeerConnection::OnSentPacket_w);
  video_channel->SetRtpTransport(rtp_transport);

  return video_channel;
}

bool PeerConnection::CreateDataChannel(const std::string& mid) {
  switch (data_channel_type_) {
    case cricket::DCT_MEDIA_TRANSPORT:
      if (network_thread()->Invoke<bool>(
              RTC_FROM_HERE,
              rtc::Bind(&PeerConnection::SetupMediaTransportForDataChannels_n,
                        this, mid))) {
        for (const auto& channel : sctp_data_channels_) {
          channel->OnTransportChannelCreated();
        }
        return true;
      }
      return false;
    case cricket::DCT_SCTP:
      if (!sctp_factory_) {
        RTC_LOG(LS_ERROR)
            << "Trying to create SCTP transport, but didn't compile with "
               "SCTP support (HAVE_SCTP)";
        return false;
      }
      if (!network_thread()->Invoke<bool>(
              RTC_FROM_HERE,
              rtc::Bind(&PeerConnection::CreateSctpTransport_n, this, mid))) {
        return false;
      }
      for (const auto& channel : sctp_data_channels_) {
        channel->OnTransportChannelCreated();
      }
      return true;
    case cricket::DCT_RTP:
    default:
      RtpTransportInternal* rtp_transport = GetRtpTransport(mid);
      rtp_data_channel_ = channel_manager()->CreateRtpDataChannel(
          configuration_.media_config, rtp_transport, signaling_thread(), mid,
          SrtpRequired(), GetCryptoOptions());
      if (!rtp_data_channel_) {
        return false;
      }
      rtp_data_channel_->SignalDtlsSrtpSetupFailure.connect(
          this, &PeerConnection::OnDtlsSrtpSetupFailure);
      rtp_data_channel_->SignalSentPacket.connect(
          this, &PeerConnection::OnSentPacket_w);
      rtp_data_channel_->SetRtpTransport(rtp_transport);
      return true;
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

bool PeerConnection::CreateSctpTransport_n(const std::string& mid) {
  RTC_DCHECK(network_thread()->IsCurrent());
  RTC_DCHECK(sctp_factory_);
  cricket::DtlsTransportInternal* dtls_transport =
      transport_controller_->GetDtlsTransport(mid);
  RTC_DCHECK(dtls_transport);
  sctp_transport_ = sctp_factory_->CreateSctpTransport(dtls_transport);
  RTC_DCHECK(sctp_transport_);
  sctp_invoker_.reset(new rtc::AsyncInvoker());
  sctp_transport_->SignalReadyToSendData.connect(
      this, &PeerConnection::OnSctpTransportReadyToSendData_n);
  sctp_transport_->SignalDataReceived.connect(
      this, &PeerConnection::OnSctpTransportDataReceived_n);
  // TODO(deadbeef): All we do here is AsyncInvoke to fire the signal on
  // another thread. Would be nice if there was a helper class similar to
  // sigslot::repeater that did this for us, eliminating a bunch of boilerplate
  // code.
  sctp_transport_->SignalClosingProcedureStartedRemotely.connect(
      this, &PeerConnection::OnSctpClosingProcedureStartedRemotely_n);
  sctp_transport_->SignalClosingProcedureComplete.connect(
      this, &PeerConnection::OnSctpClosingProcedureComplete_n);
  sctp_mid_ = mid;
  sctp_transport_->SetDtlsTransport(dtls_transport);
  return true;
}

void PeerConnection::DestroySctpTransport_n() {
  RTC_DCHECK(network_thread()->IsCurrent());
  sctp_transport_.reset(nullptr);
  sctp_mid_.reset();
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
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!HandleOpenMessage_s(params, payload)) {
    SignalSctpDataReceived(params, payload);
  }
}

void PeerConnection::OnSctpClosingProcedureStartedRemotely_n(int sid) {
  RTC_DCHECK(data_channel_type_ == cricket::DCT_SCTP);
  RTC_DCHECK(network_thread()->IsCurrent());
  sctp_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(),
      rtc::Bind(&sigslot::signal1<int>::operator(),
                &SignalSctpClosingProcedureStartedRemotely, sid));
}

void PeerConnection::OnSctpClosingProcedureComplete_n(int sid) {
  RTC_DCHECK(data_channel_type_ == cricket::DCT_SCTP);
  RTC_DCHECK(network_thread()->IsCurrent());
  sctp_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(),
      rtc::Bind(&sigslot::signal1<int>::operator(),
                &SignalSctpClosingProcedureComplete, sid));
}

bool PeerConnection::SetupMediaTransportForDataChannels_n(
    const std::string& mid) {
  media_transport_ = transport_controller_->GetMediaTransport(mid);
  if (!media_transport_) {
    RTC_LOG(LS_ERROR) << "Media transport is not available for data channels";
    return false;
  }

  media_transport_invoker_ = absl::make_unique<rtc::AsyncInvoker>();
  media_transport_->SetDataSink(this);
  media_transport_data_mid_ = mid;
  transport_controller_->SignalMediaTransportStateChanged.connect(
      this, &PeerConnection::OnMediaTransportStateChanged_n);
  // Check the initial state right away, in case transport is already writable.
  OnMediaTransportStateChanged_n();
  return true;
}

void PeerConnection::TeardownMediaTransportForDataChannels_n() {
  if (!media_transport_) {
    return;
  }
  transport_controller_->SignalMediaTransportStateChanged.disconnect(this);
  media_transport_data_mid_.reset();
  media_transport_->SetDataSink(nullptr);
  media_transport_invoker_ = nullptr;
  media_transport_ = nullptr;
}

void PeerConnection::OnMediaTransportStateChanged_n() {
  if (!media_transport_data_mid_ ||
      transport_controller_->GetMediaTransportState(
          *media_transport_data_mid_) != MediaTransportState::kWritable) {
    return;
  }
  media_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        media_transport_ready_to_send_data_ = true;
        SignalMediaTransportWritable_s(media_transport_ready_to_send_data_);
      });
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

RTCError PeerConnection::ValidateSessionDescription(
    const SessionDescriptionInterface* sdesc,
    cricket::ContentSource source) {
  if (session_error() != SessionError::kNone) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR, GetSessionErrorMsg());
  }

  if (!sdesc || !sdesc->description()) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, kInvalidSdp);
  }

  SdpType type = sdesc->GetType();
  if ((source == cricket::CS_LOCAL && !ExpectSetLocalDescription(type)) ||
      (source == cricket::CS_REMOTE && !ExpectSetRemoteDescription(type))) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_STATE,
        "Called in wrong state: " + GetSignalingStateString(signaling_state()));
  }

  // Verify crypto settings.
  std::string crypto_error;
  if (webrtc_session_desc_factory_->SdesPolicy() == cricket::SEC_REQUIRED ||
      dtls_enabled_) {
    RTCError crypto_error = VerifyCrypto(sdesc->description(), dtls_enabled_);
    if (!crypto_error.ok()) {
      return crypto_error;
    }
  }

  // Verify ice-ufrag and ice-pwd.
  if (!VerifyIceUfragPwdPresent(sdesc->description())) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         kSdpWithoutIceUfragPwd);
  }

  if (!ValidateBundleSettings(sdesc->description())) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         kBundleWithoutRtcpMux);
  }

  // TODO(skvlad): When the local rtcp-mux policy is Require, reject any
  // m-lines that do not rtcp-mux enabled.

  // Verify m-lines in Answer when compared against Offer.
  if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
    // With an answer we want to compare the new answer session description with
    // the offer's session description from the current negotiation.
    const cricket::SessionDescription* offer_desc =
        (source == cricket::CS_LOCAL) ? remote_description()->description()
                                      : local_description()->description();
    if (!MediaSectionsHaveSameCount(*offer_desc, *sdesc->description()) ||
        !MediaSectionsInSameOrder(*offer_desc, nullptr, *sdesc->description(),
                                  type)) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           kMlineMismatchInAnswer);
    }
  } else {
    // The re-offers should respect the order of m= sections in current
    // description. See RFC3264 Section 8 paragraph 4 for more details.
    // With a re-offer, either the current local or current remote descriptions
    // could be the most up to date, so we would like to check against both of
    // them if they exist. It could be the case that one of them has a 0 port
    // for a media section, but the other does not. This is important to check
    // against in the case that we are recycling an m= section.
    const cricket::SessionDescription* current_desc = nullptr;
    const cricket::SessionDescription* secondary_current_desc = nullptr;
    if (local_description()) {
      current_desc = local_description()->description();
      if (remote_description()) {
        secondary_current_desc = remote_description()->description();
      }
    } else if (remote_description()) {
      current_desc = remote_description()->description();
    }
    if (current_desc &&
        !MediaSectionsInSameOrder(*current_desc, secondary_current_desc,
                                  *sdesc->description(), type)) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           kMlineMismatchInSubsequentOffer);
    }
  }

  if (IsUnifiedPlan()) {
    // Ensure that each audio and video media section has at most one
    // "StreamParams". This will return an error if receiving a session
    // description from a "Plan B" endpoint which adds multiple tracks of the
    // same type. With Unified Plan, there can only be at most one track per
    // media section.
    for (const ContentInfo& content : sdesc->description()->contents()) {
      const MediaContentDescription& desc = *content.description;
      if ((desc.type() == cricket::MEDIA_TYPE_AUDIO ||
           desc.type() == cricket::MEDIA_TYPE_VIDEO) &&
          desc.streams().size() > 1u) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                             "Media section has more than one track specified "
                             "with a=ssrc lines which is not supported with "
                             "Unified Plan.");
      }
    }
  }

  return RTCError::OK();
}

bool PeerConnection::ExpectSetLocalDescription(SdpType type) {
  PeerConnectionInterface::SignalingState state = signaling_state();
  if (type == SdpType::kOffer) {
    return (state == PeerConnectionInterface::kStable) ||
           (state == PeerConnectionInterface::kHaveLocalOffer);
  } else {
    RTC_DCHECK(type == SdpType::kPrAnswer || type == SdpType::kAnswer);
    return (state == PeerConnectionInterface::kHaveRemoteOffer) ||
           (state == PeerConnectionInterface::kHaveLocalPrAnswer);
  }
}

bool PeerConnection::ExpectSetRemoteDescription(SdpType type) {
  PeerConnectionInterface::SignalingState state = signaling_state();
  if (type == SdpType::kOffer) {
    return (state == PeerConnectionInterface::kStable) ||
           (state == PeerConnectionInterface::kHaveRemoteOffer);
  } else {
    RTC_DCHECK(type == SdpType::kPrAnswer || type == SdpType::kAnswer);
    return (state == PeerConnectionInterface::kHaveLocalOffer) ||
           (state == PeerConnectionInterface::kHaveRemotePrAnswer);
  }
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
      static_cast<int>(UsageEvent::SET_LOCAL_DESCRIPTION_CALLED) |
      static_cast<int>(UsageEvent::CANDIDATE_COLLECTED);
  const int good_bits =
      static_cast<int>(UsageEvent::SET_REMOTE_DESCRIPTION_CALLED) |
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
  return true;
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
  std::map<std::string, std::set<cricket::MediaType>>
      media_types_by_transport_name;
  for (const auto& transceiver : transceivers_) {
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
         channel_stats.connection_infos) {
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
  RTC_DCHECK(worker_thread()->IsCurrent());
  RTC_DCHECK(call_);
  call_->OnSentPacket(sent_packet);
}

const std::string PeerConnection::GetTransportName(
    const std::string& content_name) {
  cricket::ChannelInterface* channel = GetChannel(content_name);
  if (channel) {
    return channel->transport_name();
  }
  if (sctp_transport_) {
    RTC_DCHECK(sctp_mid_);
    if (content_name == *sctp_mid_) {
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

void PeerConnection::DestroyDataChannel() {
  if (rtp_data_channel_) {
    OnDataChannelDestroyed();
    DestroyChannelInterface(rtp_data_channel_);
    rtp_data_channel_ = nullptr;
  }

  // Note: Cannot use rtc::Bind to create a functor to invoke because it will
  // grab a reference to this PeerConnection. If this is called from the
  // PeerConnection destructor, the RefCountedObject vtable will have already
  // been destroyed (since it is a subclass of PeerConnection) and using
  // rtc::Bind will cause "Pure virtual function called" error to appear.

  if (sctp_transport_) {
    OnDataChannelDestroyed();
    network_thread()->Invoke<void>(RTC_FROM_HERE,
                                   [this] { DestroySctpTransport_n(); });
  }

  if (media_transport_) {
    OnDataChannelDestroyed();
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
      RTC_DCHECK_RUN_ON(network_thread());
      TeardownMediaTransportForDataChannels_n();
    });
  }
}

void PeerConnection::DestroyChannelInterface(
    cricket::ChannelInterface* channel) {
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
    cricket::DtlsTransportInternal* dtls_transport,
    MediaTransportInterface* media_transport) {
  bool ret = true;
  auto base_channel = GetChannel(mid);
  if (base_channel) {
    ret = base_channel->SetRtpTransport(rtp_transport);
  }
  if (sctp_transport_ && mid == sctp_mid_) {
    sctp_transport_->SetDtlsTransport(dtls_transport);
  }

  call_->MediaTransportChange(media_transport);

  return ret;
}

PeerConnectionObserver* PeerConnection::Observer() const {
  // In earlier production code, the pointer was not cleared on close,
  // which might have led to undefined behavior if the observer was not
  // deallocated, or strange crashes if it was.
  // We use CHECK in order to catch such behavior if it exists.
  // TODO(hta): Remove or replace with DCHECK if nothing is found.
  RTC_CHECK(observer_);
  return observer_;
}

CryptoOptions PeerConnection::GetCryptoOptions() {
  // TODO(bugs.webrtc.org/9891) - Remove PeerConnectionFactory::CryptoOptions
  // after it has been removed.
  return configuration_.crypto_options.has_value()
             ? *configuration_.crypto_options
             : factory_->options().crypto_options;
}

void PeerConnection::ClearStatsCache() {
  if (stats_collector_) {
    stats_collector_->ClearCachedStatsReport();
  }
}

void PeerConnection::RequestUsagePatternReportForTesting() {
  signaling_thread()->Post(RTC_FROM_HERE, this, MSG_REPORT_USAGE_PATTERN,
                           nullptr);
}

}  // namespace webrtc
