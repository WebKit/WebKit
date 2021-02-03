/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sdp_offer_answer.h"

#include <algorithm>
#include <queue>
#include <set>

#include "api/media_stream_proxy.h"
#include "api/uma_metrics.h"
#include "pc/media_stream.h"
#include "pc/peer_connection.h"
#include "pc/rtp_media_utils.h"
#include "rtc_base/trace_event.h"
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

namespace {

typedef webrtc::PeerConnectionInterface::RTCOfferAnswerOptions
    RTCOfferAnswerOptions;

// Error messages
const char kInvalidSdp[] = "Invalid session description.";
const char kInvalidCandidates[] = "Description contains invalid candidates.";
const char kBundleWithoutRtcpMux[] =
    "rtcp-mux must be enabled when BUNDLE "
    "is enabled.";
const char kMlineMismatchInAnswer[] =
    "The order of m-lines in answer doesn't match order in offer. Rejecting "
    "answer.";
const char kMlineMismatchInSubsequentOffer[] =
    "The order of m-lines in subsequent offer doesn't match order from "
    "previous offer/answer.";
const char kSdpWithoutIceUfragPwd[] =
    "Called with SDP without ice-ufrag and ice-pwd.";
const char kSdpWithoutDtlsFingerprint[] =
    "Called with SDP without DTLS fingerprint.";
const char kSdpWithoutSdesCrypto[] = "Called with SDP without SDES crypto.";

// UMA metric names.
const char kSimulcastVersionApplyLocalDescription[] =
    "WebRTC.PeerConnection.Simulcast.ApplyLocalDescription";
const char kSimulcastVersionApplyRemoteDescription[] =
    "WebRTC.PeerConnection.Simulcast.ApplyRemoteDescription";
const char kSimulcastDisabled[] = "WebRTC.PeerConnection.Simulcast.Disabled";

void NoteAddIceCandidateResult(int result) {
  RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.AddIceCandidate", result,
                            kAddIceCandidateMax);
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

void ReportSimulcastApiVersion(const char* name,
                               const SessionDescription& session) {
  bool has_legacy = false;
  bool has_spec_compliant = false;
  for (const ContentInfo& content : session.contents()) {
    if (!content.media_description()) {
      continue;
    }
    has_spec_compliant |= content.media_description()->HasSimulcast();
    for (const StreamParams& sp : content.media_description()->streams()) {
      has_legacy |= sp.has_ssrc_group(cricket::kSimSsrcGroupSemantics);
    }
  }

  if (has_legacy) {
    RTC_HISTOGRAM_ENUMERATION(name, kSimulcastApiVersionLegacy,
                              kSimulcastApiVersionMax);
  }
  if (has_spec_compliant) {
    RTC_HISTOGRAM_ENUMERATION(name, kSimulcastApiVersionSpecCompliant,
                              kSimulcastApiVersionMax);
  }
  if (!has_legacy && !has_spec_compliant) {
    RTC_HISTOGRAM_ENUMERATION(name, kSimulcastApiVersionNone,
                              kSimulcastApiVersionMax);
  }
}

const ContentInfo* FindTransceiverMSection(
    RtpTransceiverProxyWithInternal<RtpTransceiver>* transceiver,
    const SessionDescriptionInterface* session_description) {
  return transceiver->mid()
             ? session_description->description()->GetContentByName(
                   *transceiver->mid())
             : nullptr;
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

static RTCError ValidateMids(const cricket::SessionDescription& description) {
  std::set<std::string> mids;
  for (const cricket::ContentInfo& content : description.contents()) {
    if (content.name.empty()) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "A media section is missing a MID attribute.");
    }
    if (!mids.insert(content.name).second) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "Duplicate a=mid value '" + content.name + "'.");
    }
  }
  return RTCError::OK();
}

bool IsValidOfferToReceiveMedia(int value) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions Options;
  return (value >= Options::kUndefined) &&
         (value <= Options::kMaxOfferToReceiveMedia);
}

bool ValidateOfferAnswerOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options) {
  return IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_audio) &&
         IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_video);
}

// Map internal signaling state name to spec name:
//  https://w3c.github.io/webrtc-pc/#rtcsignalingstate-enum
std::string GetSignalingStateString(
    PeerConnectionInterface::SignalingState state) {
  switch (state) {
    case PeerConnectionInterface::kStable:
      return "stable";
    case PeerConnectionInterface::kHaveLocalOffer:
      return "have-local-offer";
    case PeerConnectionInterface::kHaveLocalPrAnswer:
      return "have-local-pranswer";
    case PeerConnectionInterface::kHaveRemoteOffer:
      return "have-remote-offer";
    case PeerConnectionInterface::kHaveRemotePrAnswer:
      return "have-remote-pranswer";
    case PeerConnectionInterface::kClosed:
      return "closed";
  }
  RTC_NOTREACHED();
  return "";
}

// This method will extract any send encodings that were sent by the remote
// connection. This is currently only relevant for Simulcast scenario (where
// the number of layers may be communicated by the server).
static std::vector<RtpEncodingParameters> GetSendEncodingsFromRemoteDescription(
    const MediaContentDescription& desc) {
  if (!desc.HasSimulcast()) {
    return {};
  }
  std::vector<RtpEncodingParameters> result;
  const SimulcastDescription& simulcast = desc.simulcast_description();

  // This is a remote description, the parameters we are after should appear
  // as receive streams.
  for (const auto& alternatives : simulcast.receive_layers()) {
    RTC_DCHECK(!alternatives.empty());
    // There is currently no way to specify or choose from alternatives.
    // We will always use the first alternative, which is the most preferred.
    const SimulcastLayer& layer = alternatives[0];
    RtpEncodingParameters parameters;
    parameters.rid = layer.rid;
    parameters.active = !layer.is_paused;
    result.push_back(parameters);
  }

  return result;
}

static RTCError UpdateSimulcastLayerStatusInSender(
    const std::vector<SimulcastLayer>& layers,
    rtc::scoped_refptr<RtpSenderInternal> sender) {
  RTC_DCHECK(sender);
  RtpParameters parameters = sender->GetParametersInternal();
  std::vector<std::string> disabled_layers;

  // The simulcast envelope cannot be changed, only the status of the streams.
  // So we will iterate over the send encodings rather than the layers.
  for (RtpEncodingParameters& encoding : parameters.encodings) {
    auto iter = std::find_if(layers.begin(), layers.end(),
                             [&encoding](const SimulcastLayer& layer) {
                               return layer.rid == encoding.rid;
                             });
    // A layer that cannot be found may have been removed by the remote party.
    if (iter == layers.end()) {
      disabled_layers.push_back(encoding.rid);
      continue;
    }

    encoding.active = !iter->is_paused;
  }

  RTCError result = sender->SetParametersInternal(parameters);
  if (result.ok()) {
    result = sender->DisableEncodingLayers(disabled_layers);
  }

  return result;
}

static bool SimulcastIsRejected(
    const ContentInfo* local_content,
    const MediaContentDescription& answer_media_desc) {
  bool simulcast_offered = local_content &&
                           local_content->media_description() &&
                           local_content->media_description()->HasSimulcast();
  bool simulcast_answered = answer_media_desc.HasSimulcast();
  bool rids_supported = RtpExtension::FindHeaderExtensionByUri(
      answer_media_desc.rtp_header_extensions(), RtpExtension::kRidUri);
  return simulcast_offered && (!simulcast_answered || !rids_supported);
}

static RTCError DisableSimulcastInSender(
    rtc::scoped_refptr<RtpSenderInternal> sender) {
  RTC_DCHECK(sender);
  RtpParameters parameters = sender->GetParametersInternal();
  if (parameters.encodings.size() <= 1) {
    return RTCError::OK();
  }

  std::vector<std::string> disabled_layers;
  std::transform(
      parameters.encodings.begin() + 1, parameters.encodings.end(),
      std::back_inserter(disabled_layers),
      [](const RtpEncodingParameters& encoding) { return encoding.rid; });
  return sender->DisableEncodingLayers(disabled_layers);
}

// The SDP parser used to populate these values by default for the 'content
// name' if an a=mid line was absent.
static absl::string_view GetDefaultMidForPlanB(cricket::MediaType media_type) {
  switch (media_type) {
    case cricket::MEDIA_TYPE_AUDIO:
      return cricket::CN_AUDIO;
    case cricket::MEDIA_TYPE_VIDEO:
      return cricket::CN_VIDEO;
    case cricket::MEDIA_TYPE_DATA:
      return cricket::CN_DATA;
  }
  RTC_NOTREACHED();
  return "";
}

// Add options to |[audio/video]_media_description_options| from |senders|.
void AddPlanBRtpSenderOptions(
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
            sender->id(), sender->internal()->stream_ids(), {},
            SimulcastLayerList(), num_sim_layers);
      }
    }
  }
}

static cricket::MediaDescriptionOptions
GetMediaDescriptionOptionsForTransceiver(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver,
    const std::string& mid,
    bool is_create_offer) {
  // NOTE: a stopping transceiver should be treated as a stopped one in
  // createOffer as specified in
  // https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-createoffer.
  bool stopped =
      is_create_offer ? transceiver->stopping() : transceiver->stopped();
  cricket::MediaDescriptionOptions media_description_options(
      transceiver->media_type(), mid, transceiver->direction(), stopped);
  media_description_options.codec_preferences =
      transceiver->codec_preferences();
  media_description_options.header_extensions =
      transceiver->HeaderExtensionsToOffer();
  // This behavior is specified in JSEP. The gist is that:
  // 1. The MSID is included if the RtpTransceiver's direction is sendonly or
  //    sendrecv.
  // 2. If the MSID is included, then it must be included in any subsequent
  //    offer/answer exactly the same until the RtpTransceiver is stopped.
  if (stopped || (!RtpTransceiverDirectionHasSend(transceiver->direction()) &&
                  !transceiver->internal()->has_ever_been_used_to_send())) {
    return media_description_options;
  }

  cricket::SenderOptions sender_options;
  sender_options.track_id = transceiver->sender()->id();
  sender_options.stream_ids = transceiver->sender()->stream_ids();

  // The following sets up RIDs and Simulcast.
  // RIDs are included if Simulcast is requested or if any RID was specified.
  RtpParameters send_parameters =
      transceiver->internal()->sender_internal()->GetParametersInternal();
  bool has_rids = std::any_of(send_parameters.encodings.begin(),
                              send_parameters.encodings.end(),
                              [](const RtpEncodingParameters& encoding) {
                                return !encoding.rid.empty();
                              });

  std::vector<RidDescription> send_rids;
  SimulcastLayerList send_layers;
  for (const RtpEncodingParameters& encoding : send_parameters.encodings) {
    if (encoding.rid.empty()) {
      continue;
    }
    send_rids.push_back(RidDescription(encoding.rid, RidDirection::kSend));
    send_layers.AddLayer(SimulcastLayer(encoding.rid, !encoding.active));
  }

  if (has_rids) {
    sender_options.rids = send_rids;
  }

  sender_options.simulcast_layers = send_layers;
  // When RIDs are configured, we must set num_sim_layers to 0 to.
  // Otherwise, num_sim_layers must be 1 because either there is no
  // simulcast, or simulcast is acheived by munging the SDP.
  sender_options.num_sim_layers = has_rids ? 0 : 1;
  media_description_options.sender_options.push_back(sender_options);

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

// From |rtc_options|, fill parts of |session_options| shared by all generated
// m= sectionss (in other words, nothing that involves a map/array).
void ExtractSharedMediaSessionOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  session_options->vad_enabled = rtc_options.voice_activity_detection;
  session_options->bundle_enabled = rtc_options.use_rtp_mux;
  session_options->raw_packetization_for_video =
      rtc_options.raw_packetization_for_video;
}

}  // namespace

// Used by parameterless SetLocalDescription() to create an offer or answer.
// Upon completion of creating the session description, SetLocalDescription() is
// invoked with the result.
class SdpOfferAnswerHandler::ImplicitCreateSessionDescriptionObserver
    : public CreateSessionDescriptionObserver {
 public:
  ImplicitCreateSessionDescriptionObserver(
      rtc::WeakPtr<SdpOfferAnswerHandler> sdp_handler,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface>
          set_local_description_observer)
      : sdp_handler_(std::move(sdp_handler)),
        set_local_description_observer_(
            std::move(set_local_description_observer)) {}
  ~ImplicitCreateSessionDescriptionObserver() override {
    RTC_DCHECK(was_called_);
  }

  void SetOperationCompleteCallback(
      std::function<void()> operation_complete_callback) {
    operation_complete_callback_ = std::move(operation_complete_callback);
  }

  bool was_called() const { return was_called_; }

  void OnSuccess(SessionDescriptionInterface* desc_ptr) override {
    RTC_DCHECK(!was_called_);
    std::unique_ptr<SessionDescriptionInterface> desc(desc_ptr);
    was_called_ = true;

    // Abort early if |pc_| is no longer valid.
    if (!sdp_handler_) {
      operation_complete_callback_();
      return;
    }
    // DoSetLocalDescription() is a synchronous operation that invokes
    // |set_local_description_observer_| with the result.
    sdp_handler_->DoSetLocalDescription(
        std::move(desc), std::move(set_local_description_observer_));
    operation_complete_callback_();
  }

  void OnFailure(RTCError error) override {
    RTC_DCHECK(!was_called_);
    was_called_ = true;
    set_local_description_observer_->OnSetLocalDescriptionComplete(RTCError(
        error.type(), std::string("SetLocalDescription failed to create "
                                  "session description - ") +
                          error.message()));
    operation_complete_callback_();
  }

 private:
  bool was_called_ = false;
  rtc::WeakPtr<SdpOfferAnswerHandler> sdp_handler_;
  rtc::scoped_refptr<SetLocalDescriptionObserverInterface>
      set_local_description_observer_;
  std::function<void()> operation_complete_callback_;
};

// Wraps a CreateSessionDescriptionObserver and an OperationsChain operation
// complete callback. When the observer is invoked, the wrapped observer is
// invoked followed by invoking the completion callback.
class CreateSessionDescriptionObserverOperationWrapper
    : public CreateSessionDescriptionObserver {
 public:
  CreateSessionDescriptionObserverOperationWrapper(
      rtc::scoped_refptr<CreateSessionDescriptionObserver> observer,
      std::function<void()> operation_complete_callback)
      : observer_(std::move(observer)),
        operation_complete_callback_(std::move(operation_complete_callback)) {
    RTC_DCHECK(observer_);
  }
  ~CreateSessionDescriptionObserverOperationWrapper() override {
    RTC_DCHECK(was_called_);
  }

  void OnSuccess(SessionDescriptionInterface* desc) override {
    RTC_DCHECK(!was_called_);
#ifdef RTC_DCHECK_IS_ON
    was_called_ = true;
#endif  // RTC_DCHECK_IS_ON
    // Completing the operation before invoking the observer allows the observer
    // to execute SetLocalDescription() without delay.
    operation_complete_callback_();
    observer_->OnSuccess(desc);
  }

  void OnFailure(RTCError error) override {
    RTC_DCHECK(!was_called_);
#ifdef RTC_DCHECK_IS_ON
    was_called_ = true;
#endif  // RTC_DCHECK_IS_ON
    operation_complete_callback_();
    observer_->OnFailure(std::move(error));
  }

 private:
#ifdef RTC_DCHECK_IS_ON
  bool was_called_ = false;
#endif  // RTC_DCHECK_IS_ON
  rtc::scoped_refptr<CreateSessionDescriptionObserver> observer_;
  std::function<void()> operation_complete_callback_;
};

// Wrapper for SetSessionDescriptionObserver that invokes the success or failure
// callback in a posted message handled by the peer connection. This introduces
// a delay that prevents recursive API calls by the observer, but this also
// means that the PeerConnection can be modified before the observer sees the
// result of the operation. This is ill-advised for synchronizing states.
//
// Implements both the SetLocalDescriptionObserverInterface and the
// SetRemoteDescriptionObserverInterface.
class SdpOfferAnswerHandler::SetSessionDescriptionObserverAdapter
    : public SetLocalDescriptionObserverInterface,
      public SetRemoteDescriptionObserverInterface {
 public:
  SetSessionDescriptionObserverAdapter(
      rtc::WeakPtr<SdpOfferAnswerHandler> handler,
      rtc::scoped_refptr<SetSessionDescriptionObserver> inner_observer)
      : handler_(std::move(handler)),
        inner_observer_(std::move(inner_observer)) {}

  // SetLocalDescriptionObserverInterface implementation.
  void OnSetLocalDescriptionComplete(RTCError error) override {
    OnSetDescriptionComplete(std::move(error));
  }
  // SetRemoteDescriptionObserverInterface implementation.
  void OnSetRemoteDescriptionComplete(RTCError error) override {
    OnSetDescriptionComplete(std::move(error));
  }

 private:
  void OnSetDescriptionComplete(RTCError error) {
    if (!handler_)
      return;
    if (error.ok()) {
      handler_->pc_->PostSetSessionDescriptionSuccess(inner_observer_);
    } else {
      handler_->pc_->PostSetSessionDescriptionFailure(inner_observer_,
                                                      std::move(error));
    }
  }

  rtc::WeakPtr<SdpOfferAnswerHandler> handler_;
  rtc::scoped_refptr<SetSessionDescriptionObserver> inner_observer_;
};

class SdpOfferAnswerHandler::LocalIceCredentialsToReplace {
 public:
  // Sets the ICE credentials that need restarting to the ICE credentials of
  // the current and pending descriptions.
  void SetIceCredentialsFromLocalDescriptions(
      const SessionDescriptionInterface* current_local_description,
      const SessionDescriptionInterface* pending_local_description) {
    ice_credentials_.clear();
    if (current_local_description) {
      AppendIceCredentialsFromSessionDescription(*current_local_description);
    }
    if (pending_local_description) {
      AppendIceCredentialsFromSessionDescription(*pending_local_description);
    }
  }

  void ClearIceCredentials() { ice_credentials_.clear(); }

  // Returns true if we have ICE credentials that need restarting.
  bool HasIceCredentials() const { return !ice_credentials_.empty(); }

  // Returns true if |local_description| shares no ICE credentials with the
  // ICE credentials that need restarting.
  bool SatisfiesIceRestart(
      const SessionDescriptionInterface& local_description) const {
    for (const auto& transport_info :
         local_description.description()->transport_infos()) {
      if (ice_credentials_.find(std::make_pair(
              transport_info.description.ice_ufrag,
              transport_info.description.ice_pwd)) != ice_credentials_.end()) {
        return false;
      }
    }
    return true;
  }

 private:
  void AppendIceCredentialsFromSessionDescription(
      const SessionDescriptionInterface& desc) {
    for (const auto& transport_info : desc.description()->transport_infos()) {
      ice_credentials_.insert(
          std::make_pair(transport_info.description.ice_ufrag,
                         transport_info.description.ice_pwd));
    }
  }

  std::set<std::pair<std::string, std::string>> ice_credentials_;
};

SdpOfferAnswerHandler::SdpOfferAnswerHandler(PeerConnection* pc)
    : pc_(pc),
      operations_chain_(rtc::OperationsChain::Create()),
      local_ice_credentials_to_replace_(new LocalIceCredentialsToReplace()),
      weak_ptr_factory_(this) {
  operations_chain_->SetOnChainEmptyCallback(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr()]() {
        if (!this_weak_ptr)
          return;
        this_weak_ptr->OnOperationsChainEmpty();
      });
}

SdpOfferAnswerHandler::~SdpOfferAnswerHandler() {}

void SdpOfferAnswerHandler::PrepareForShutdown() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SdpOfferAnswerHandler::Close() {
  ChangeSignalingState(PeerConnectionInterface::kClosed);
}

void SdpOfferAnswerHandler::RestartIce() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  local_ice_credentials_to_replace_->SetIceCredentialsFromLocalDescriptions(
      current_local_description(), pending_local_description());
  UpdateNegotiationNeeded();
}

rtc::Thread* SdpOfferAnswerHandler::signaling_thread() const {
  return pc_->signaling_thread();
}

void SdpOfferAnswerHandler::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Chain this operation. If asynchronous operations are pending on the chain,
  // this operation will be queued to be invoked, otherwise the contents of the
  // lambda will execute immediately.
  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       observer_refptr =
           rtc::scoped_refptr<CreateSessionDescriptionObserver>(observer),
       options](std::function<void()> operations_chain_callback) {
        // Abort early if |this_weak_ptr| is no longer valid.
        if (!this_weak_ptr) {
          observer_refptr->OnFailure(
              RTCError(RTCErrorType::INTERNAL_ERROR,
                       "CreateOffer failed because the session was shut down"));
          operations_chain_callback();
          return;
        }
        // The operation completes asynchronously when the wrapper is invoked.
        rtc::scoped_refptr<CreateSessionDescriptionObserverOperationWrapper>
            observer_wrapper(new rtc::RefCountedObject<
                             CreateSessionDescriptionObserverOperationWrapper>(
                std::move(observer_refptr),
                std::move(operations_chain_callback)));
        this_weak_ptr->DoCreateOffer(options, observer_wrapper);
      });
}

void SdpOfferAnswerHandler::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc_ptr) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Chain this operation. If asynchronous operations are pending on the chain,
  // this operation will be queued to be invoked, otherwise the contents of the
  // lambda will execute immediately.
  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       observer_refptr =
           rtc::scoped_refptr<SetSessionDescriptionObserver>(observer),
       desc = std::unique_ptr<SessionDescriptionInterface>(desc_ptr)](
          std::function<void()> operations_chain_callback) mutable {
        // Abort early if |this_weak_ptr| is no longer valid.
        if (!this_weak_ptr) {
          // For consistency with SetSessionDescriptionObserverAdapter whose
          // posted messages doesn't get processed when the PC is destroyed, we
          // do not inform |observer_refptr| that the operation failed.
          operations_chain_callback();
          return;
        }
        // SetSessionDescriptionObserverAdapter takes care of making sure the
        // |observer_refptr| is invoked in a posted message.
        this_weak_ptr->DoSetLocalDescription(
            std::move(desc),
            rtc::scoped_refptr<SetLocalDescriptionObserverInterface>(
                new rtc::RefCountedObject<SetSessionDescriptionObserverAdapter>(
                    this_weak_ptr, observer_refptr)));
        // For backwards-compatability reasons, we declare the operation as
        // completed here (rather than in a post), so that the operation chain
        // is not blocked by this operation when the observer is invoked. This
        // allows the observer to trigger subsequent offer/answer operations
        // synchronously if the operation chain is now empty.
        operations_chain_callback();
      });
}

void SdpOfferAnswerHandler::SetLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Chain this operation. If asynchronous operations are pending on the chain,
  // this operation will be queued to be invoked, otherwise the contents of the
  // lambda will execute immediately.
  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(), observer,
       desc = std::move(desc)](
          std::function<void()> operations_chain_callback) mutable {
        // Abort early if |this_weak_ptr| is no longer valid.
        if (!this_weak_ptr) {
          observer->OnSetLocalDescriptionComplete(RTCError(
              RTCErrorType::INTERNAL_ERROR,
              "SetLocalDescription failed because the session was shut down"));
          operations_chain_callback();
          return;
        }
        this_weak_ptr->DoSetLocalDescription(std::move(desc), observer);
        // DoSetLocalDescription() is implemented as a synchronous operation.
        // The |observer| will already have been informed that it completed, and
        // we can mark this operation as complete without any loose ends.
        operations_chain_callback();
      });
}

void SdpOfferAnswerHandler::SetLocalDescription(
    SetSessionDescriptionObserver* observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  SetLocalDescription(
      new rtc::RefCountedObject<SetSessionDescriptionObserverAdapter>(
          weak_ptr_factory_.GetWeakPtr(), observer));
}

void SdpOfferAnswerHandler::SetLocalDescription(
    rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // The |create_sdp_observer| handles performing DoSetLocalDescription() with
  // the resulting description as well as completing the operation.
  rtc::scoped_refptr<ImplicitCreateSessionDescriptionObserver>
      create_sdp_observer(
          new rtc::RefCountedObject<ImplicitCreateSessionDescriptionObserver>(
              weak_ptr_factory_.GetWeakPtr(), observer));
  // Chain this operation. If asynchronous operations are pending on the chain,
  // this operation will be queued to be invoked, otherwise the contents of the
  // lambda will execute immediately.
  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       create_sdp_observer](std::function<void()> operations_chain_callback) {
        // The |create_sdp_observer| is responsible for completing the
        // operation.
        create_sdp_observer->SetOperationCompleteCallback(
            std::move(operations_chain_callback));
        // Abort early if |this_weak_ptr| is no longer valid. This triggers the
        // same code path as if DoCreateOffer() or DoCreateAnswer() failed.
        if (!this_weak_ptr) {
          create_sdp_observer->OnFailure(RTCError(
              RTCErrorType::INTERNAL_ERROR,
              "SetLocalDescription failed because the session was shut down"));
          return;
        }
        switch (this_weak_ptr->signaling_state()) {
          case PeerConnectionInterface::kStable:
          case PeerConnectionInterface::kHaveLocalOffer:
          case PeerConnectionInterface::kHaveRemotePrAnswer:
            // TODO(hbos): If [LastCreatedOffer] exists and still represents the
            // current state of the system, use that instead of creating another
            // offer.
            this_weak_ptr->DoCreateOffer(
                PeerConnectionInterface::RTCOfferAnswerOptions(),
                create_sdp_observer);
            break;
          case PeerConnectionInterface::kHaveLocalPrAnswer:
          case PeerConnectionInterface::kHaveRemoteOffer:
            // TODO(hbos): If [LastCreatedAnswer] exists and still represents
            // the current state of the system, use that instead of creating
            // another answer.
            this_weak_ptr->DoCreateAnswer(
                PeerConnectionInterface::RTCOfferAnswerOptions(),
                create_sdp_observer);
            break;
          case PeerConnectionInterface::kClosed:
            create_sdp_observer->OnFailure(RTCError(
                RTCErrorType::INVALID_STATE,
                "SetLocalDescription called when PeerConnection is closed."));
            break;
        }
      });
}

RTCError SdpOfferAnswerHandler::ApplyLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(desc);

  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  pc_->stats()->UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

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

  // Report statistics about any use of simulcast.
  ReportSimulcastApiVersion(kSimulcastVersionApplyLocalDescription,
                            *local_description()->description());

  if (!is_caller_) {
    if (remote_description()) {
      // Remote description was applied first, so this PC is the callee.
      is_caller_ = false;
    } else {
      // Local description is applied first, so this PC is the caller.
      is_caller_ = true;
    }
  }

  RTCError error = pc_->PushdownTransportDescription(cricket::CS_LOCAL, type);
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
    for (const auto& transceiver : pc_->transceivers_.List()) {
      if (transceiver->stopped()) {
        continue;
      }

      // 2.2.7.1.1.(6-9): Set sender and receiver's transport slots.
      // Note that code paths that don't set MID won't be able to use
      // information about DTLS transports.
      if (transceiver->mid()) {
        auto dtls_transport =
            pc_->LookupDtlsTransportByMidInternal(*transceiver->mid());
        transceiver->internal()->sender_internal()->set_transport(
            dtls_transport);
        transceiver->internal()->receiver_internal()->set_transport(
            dtls_transport);
      }

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
          pc_->ProcessRemovalOfRemoteTrack(transceiver, &remove_list,
                                           &removed_streams);
        }
        // 2.2.7.1.6.2: Set transceiver's [[CurrentDirection]] and
        // [[FiredDirection]] slots to direction.
        transceiver->internal()->set_current_direction(media_desc->direction());
        transceiver->internal()->set_fired_direction(media_desc->direction());
      }
    }
    auto observer = pc_->Observer();
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
      RTCError error = pc_->CreateChannels(*local_description()->description());
      if (!error.ok()) {
        return error;
      }
    }
    // Remove unused channels if MediaContentDescription is rejected.
    pc_->RemoveUnusedChannels(local_description()->description());
  }

  error = UpdateSessionState(type, cricket::CS_LOCAL,
                             local_description()->description());
  if (!error.ok()) {
    return error;
  }

  if (remote_description()) {
    // Now that we have a local description, we can push down remote candidates.
    pc_->UseCandidatesInSessionDescription(remote_description());
  }

  pending_ice_restarts_.clear();
  if (pc_->session_error() != PeerConnection::SessionError::kNone) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         pc_->GetSessionErrorMsg());
  }

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (IsSctpLike(pc_->data_channel_type()) && pc_->GetSctpSslRole(&role)) {
    pc_->data_channel_controller()->AllocateSctpSids(role);
  }

  if (IsUnifiedPlan()) {
    for (const auto& transceiver : pc_->transceivers_.List()) {
      if (transceiver->stopped()) {
        continue;
      }
      const ContentInfo* content =
          FindMediaSectionForTransceiver(transceiver, local_description());
      if (!content) {
        continue;
      }
      cricket::ChannelInterface* channel = transceiver->internal()->channel();
      if (content->rejected || !channel || channel->local_streams().empty()) {
        // 0 is a special value meaning "this sender has no associated send
        // stream". Need to call this so the sender won't attempt to configure
        // a no longer existing stream and run into DCHECKs in the lower
        // layers.
        transceiver->internal()->sender_internal()->SetSsrc(0);
      } else {
        // Get the StreamParams from the channel which could generate SSRCs.
        const std::vector<StreamParams>& streams = channel->local_streams();
        transceiver->internal()->sender_internal()->set_stream_ids(
            streams[0].stream_ids());
        transceiver->internal()->sender_internal()->SetSsrc(
            streams[0].first_ssrc());
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
        pc_->RemoveSenders(cricket::MEDIA_TYPE_AUDIO);
      } else {
        const cricket::AudioContentDescription* audio_desc =
            audio_content->media_description()->as_audio();
        pc_->UpdateLocalSenders(audio_desc->streams(), audio_desc->type());
      }
    }

    const cricket::ContentInfo* video_content =
        GetFirstVideoContent(local_description()->description());
    if (video_content) {
      if (video_content->rejected) {
        pc_->RemoveSenders(cricket::MEDIA_TYPE_VIDEO);
      } else {
        const cricket::VideoContentDescription* video_desc =
            video_content->media_description()->as_video();
        pc_->UpdateLocalSenders(video_desc->streams(), video_desc->type());
      }
    }
  }

  const cricket::ContentInfo* data_content =
      GetFirstDataContent(local_description()->description());
  if (data_content) {
    const cricket::RtpDataContentDescription* rtp_data_desc =
        data_content->media_description()->as_rtp_data();
    // rtp_data_desc will be null if this is an SCTP description.
    if (rtp_data_desc) {
      pc_->data_channel_controller()->UpdateLocalRtpDataChannels(
          rtp_data_desc->streams());
    }
  }

  if (type == SdpType::kAnswer &&
      local_ice_credentials_to_replace_->SatisfiesIceRestart(
          *current_local_description_)) {
    local_ice_credentials_to_replace_->ClearIceCredentials();
  }

  return RTCError::OK();
}

void SdpOfferAnswerHandler::SetRemoteDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc_ptr) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Chain this operation. If asynchronous operations are pending on the chain,
  // this operation will be queued to be invoked, otherwise the contents of the
  // lambda will execute immediately.
  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       observer_refptr =
           rtc::scoped_refptr<SetSessionDescriptionObserver>(observer),
       desc = std::unique_ptr<SessionDescriptionInterface>(desc_ptr)](
          std::function<void()> operations_chain_callback) mutable {
        // Abort early if |this_weak_ptr| is no longer valid.
        if (!this_weak_ptr) {
          // For consistency with SetSessionDescriptionObserverAdapter whose
          // posted messages doesn't get processed when the PC is destroyed, we
          // do not inform |observer_refptr| that the operation failed.
          operations_chain_callback();
          return;
        }
        // SetSessionDescriptionObserverAdapter takes care of making sure the
        // |observer_refptr| is invoked in a posted message.
        this_weak_ptr->DoSetRemoteDescription(
            std::move(desc),
            rtc::scoped_refptr<SetRemoteDescriptionObserverInterface>(
                new rtc::RefCountedObject<SetSessionDescriptionObserverAdapter>(
                    this_weak_ptr, observer_refptr)));
        // For backwards-compatability reasons, we declare the operation as
        // completed here (rather than in a post), so that the operation chain
        // is not blocked by this operation when the observer is invoked. This
        // allows the observer to trigger subsequent offer/answer operations
        // synchronously if the operation chain is now empty.
        operations_chain_callback();
      });
}

void SdpOfferAnswerHandler::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Chain this operation. If asynchronous operations are pending on the chain,
  // this operation will be queued to be invoked, otherwise the contents of the
  // lambda will execute immediately.
  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(), observer,
       desc = std::move(desc)](
          std::function<void()> operations_chain_callback) mutable {
        // Abort early if |this_weak_ptr| is no longer valid.
        if (!this_weak_ptr) {
          observer->OnSetRemoteDescriptionComplete(RTCError(
              RTCErrorType::INTERNAL_ERROR,
              "SetRemoteDescription failed because the session was shut down"));
          operations_chain_callback();
          return;
        }
        this_weak_ptr->DoSetRemoteDescription(std::move(desc),
                                              std::move(observer));
        // DoSetRemoteDescription() is implemented as a synchronous operation.
        // The |observer| will already have been informed that it completed, and
        // we can mark this operation as complete without any loose ends.
        operations_chain_callback();
      });
}

RTCError SdpOfferAnswerHandler::ApplyRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(desc);

  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  pc_->stats()->UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

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

  // Report statistics about any use of simulcast.
  ReportSimulcastApiVersion(kSimulcastVersionApplyRemoteDescription,
                            *remote_description()->description());

  RTCError error = pc_->PushdownTransportDescription(cricket::CS_REMOTE, type);
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
      RTCError error =
          pc_->CreateChannels(*remote_description()->description());
      if (!error.ok()) {
        return error;
      }
    }
    // Remove unused channels if MediaContentDescription is rejected.
    pc_->RemoveUnusedChannels(remote_description()->description());
  }

  // NOTE: Candidates allocation will be initiated only when
  // SetLocalDescription is called.
  error = UpdateSessionState(type, cricket::CS_REMOTE,
                             remote_description()->description());
  if (!error.ok()) {
    return error;
  }

  if (local_description() &&
      !pc_->UseCandidatesInSessionDescription(remote_description())) {
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

  if (pc_->session_error() != PeerConnection::SessionError::kNone) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         pc_->GetSessionErrorMsg());
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
      pc_->ice_connection_state() ==
          PeerConnectionInterface::kIceConnectionNew) {
    pc_->SetIceConnectionState(PeerConnectionInterface::kIceConnectionChecking);
  }

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (IsSctpLike(pc_->data_channel_type()) && pc_->GetSctpSslRole(&role)) {
    pc_->data_channel_controller()->AllocateSctpSids(role);
  }

  if (IsUnifiedPlan()) {
    std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
        now_receiving_transceivers;
    std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> remove_list;
    std::vector<rtc::scoped_refptr<MediaStreamInterface>> added_streams;
    std::vector<rtc::scoped_refptr<MediaStreamInterface>> removed_streams;
    for (const auto& transceiver : pc_->transceivers_.List()) {
      const ContentInfo* content =
          FindMediaSectionForTransceiver(transceiver, remote_description());
      if (!content) {
        continue;
      }
      const MediaContentDescription* media_desc = content->media_description();
      RtpTransceiverDirection local_direction =
          RtpTransceiverDirectionReversed(media_desc->direction());
      // Roughly the same as steps 2.2.8.6 of section 4.4.1.6 "Set the
      // RTCSessionDescription: Set the associated remote streams given
      // transceiver.[[Receiver]], msids, addList, and removeList".
      // https://w3c.github.io/webrtc-pc/#set-the-rtcsessiondescription
      if (RtpTransceiverDirectionHasRecv(local_direction)) {
        std::vector<std::string> stream_ids;
        if (!media_desc->streams().empty()) {
          // The remote description has signaled the stream IDs.
          stream_ids = media_desc->streams()[0].stream_ids();
        }
        pc_->transceivers_.StableState(transceiver)
            ->SetRemoteStreamIdsIfUnset(transceiver->receiver()->stream_ids());

        RTC_LOG(LS_INFO) << "Processing the MSIDs for MID=" << content->name
                         << " (" << GetStreamIdsString(stream_ids) << ").";
        SetAssociatedRemoteStreams(transceiver->internal()->receiver_internal(),
                                   stream_ids, &added_streams,
                                   &removed_streams);
        // From the WebRTC specification, steps 2.2.8.5/6 of section 4.4.1.6
        // "Set the RTCSessionDescription: If direction is sendrecv or recvonly,
        // and transceiver's current direction is neither sendrecv nor recvonly,
        // process the addition of a remote track for the media description.
        if (!transceiver->fired_direction() ||
            !RtpTransceiverDirectionHasRecv(*transceiver->fired_direction())) {
          RTC_LOG(LS_INFO)
              << "Processing the addition of a remote track for MID="
              << content->name << ".";
          now_receiving_transceivers.push_back(transceiver);
        }
      }
      // 2.2.8.1.9: If direction is "sendonly" or "inactive", and transceiver's
      // [[FiredDirection]] slot is either "sendrecv" or "recvonly", process the
      // removal of a remote track for the media description, given transceiver,
      // removeList, and muteTracks.
      if (!RtpTransceiverDirectionHasRecv(local_direction) &&
          (transceiver->fired_direction() &&
           RtpTransceiverDirectionHasRecv(*transceiver->fired_direction()))) {
        pc_->ProcessRemovalOfRemoteTrack(transceiver, &remove_list,
                                         &removed_streams);
      }
      // 2.2.8.1.10: Set transceiver's [[FiredDirection]] slot to direction.
      transceiver->internal()->set_fired_direction(local_direction);
      // 2.2.8.1.11: If description is of type "answer" or "pranswer", then run
      // the following steps:
      if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
        // 2.2.8.1.11.1: Set transceiver's [[CurrentDirection]] slot to
        // direction.
        transceiver->internal()->set_current_direction(local_direction);
        // 2.2.8.1.11.[3-6]: Set the transport internal slots.
        if (transceiver->mid()) {
          auto dtls_transport =
              pc_->LookupDtlsTransportByMidInternal(*transceiver->mid());
          transceiver->internal()->sender_internal()->set_transport(
              dtls_transport);
          transceiver->internal()->receiver_internal()->set_transport(
              dtls_transport);
        }
      }
      // 2.2.8.1.12: If the media description is rejected, and transceiver is
      // not already stopped, stop the RTCRtpTransceiver transceiver.
      if (content->rejected && !transceiver->stopped()) {
        RTC_LOG(LS_INFO) << "Stopping transceiver for MID=" << content->name
                         << " since the media section was rejected.";
        transceiver->internal()->StopTransceiverProcedure();
      }
      if (!content->rejected &&
          RtpTransceiverDirectionHasRecv(local_direction)) {
        if (!media_desc->streams().empty() &&
            media_desc->streams()[0].has_ssrcs()) {
          uint32_t ssrc = media_desc->streams()[0].first_ssrc();
          transceiver->internal()->receiver_internal()->SetupMediaChannel(ssrc);
        } else {
          transceiver->internal()
              ->receiver_internal()
              ->SetupUnsignaledMediaChannel();
        }
      }
    }
    // Once all processing has finished, fire off callbacks.
    auto observer = pc_->Observer();
    for (const auto& transceiver : now_receiving_transceivers) {
      pc_->stats()->AddTrack(transceiver->receiver()->track());
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
  const cricket::RtpDataContentDescription* rtp_data_desc =
      GetFirstRtpDataContentDescription(remote_description()->description());

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
        pc_->RemoveSenders(cricket::MEDIA_TYPE_AUDIO);
      } else {
        bool default_audio_track_needed =
            !remote_peer_supports_msid_ &&
            RtpTransceiverDirectionHasSend(audio_desc->direction());
        pc_->UpdateRemoteSendersList(GetActiveStreams(audio_desc),
                                     default_audio_track_needed,
                                     audio_desc->type(), new_streams);
      }
    }

    // Find all video rtp streams and create corresponding remote VideoTracks
    // and MediaStreams.
    if (video_content) {
      if (video_content->rejected) {
        pc_->RemoveSenders(cricket::MEDIA_TYPE_VIDEO);
      } else {
        bool default_video_track_needed =
            !remote_peer_supports_msid_ &&
            RtpTransceiverDirectionHasSend(video_desc->direction());
        pc_->UpdateRemoteSendersList(GetActiveStreams(video_desc),
                                     default_video_track_needed,
                                     video_desc->type(), new_streams);
      }
    }

    // If this is an RTP data transport, update the DataChannels with the
    // information from the remote peer.
    if (rtp_data_desc) {
      pc_->data_channel_controller()->UpdateRemoteRtpDataChannels(
          GetActiveStreams(rtp_data_desc));
    }

    // Iterate new_streams and notify the observer about new MediaStreams.
    auto observer = pc_->Observer();
    for (size_t i = 0; i < new_streams->count(); ++i) {
      MediaStreamInterface* new_stream = new_streams->at(i);
      pc_->stats()->AddStream(new_stream);
      observer->OnAddStream(
          rtc::scoped_refptr<MediaStreamInterface>(new_stream));
    }

    pc_->UpdateEndedRemoteMediaStreams();
  }

  if (type == SdpType::kAnswer &&
      local_ice_credentials_to_replace_->SatisfiesIceRestart(
          *current_local_description_)) {
    local_ice_credentials_to_replace_->ClearIceCredentials();
  }

  return RTCError::OK();
}

void SdpOfferAnswerHandler::DoSetLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::DoSetLocalDescription");

  if (!observer) {
    RTC_LOG(LS_ERROR) << "SetLocalDescription - observer is NULL.";
    return;
  }

  if (!desc) {
    observer->OnSetLocalDescriptionComplete(
        RTCError(RTCErrorType::INTERNAL_ERROR, "SessionDescription is NULL."));
    return;
  }

  // If a session error has occurred the PeerConnection is in a possibly
  // inconsistent state so fail right away.
  if (pc_->session_error() != PeerConnection::SessionError::kNone) {
    std::string error_message = pc_->GetSessionErrorMsg();
    RTC_LOG(LS_ERROR) << "SetLocalDescription: " << error_message;
    observer->OnSetLocalDescriptionComplete(
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }

  // For SLD we support only explicit rollback.
  if (desc->GetType() == SdpType::kRollback) {
    if (IsUnifiedPlan()) {
      observer->OnSetLocalDescriptionComplete(Rollback(desc->GetType()));
    } else {
      observer->OnSetLocalDescriptionComplete(
          RTCError(RTCErrorType::UNSUPPORTED_OPERATION,
                   "Rollback not supported in Plan B"));
    }
    return;
  }

  RTCError error = ValidateSessionDescription(desc.get(), cricket::CS_LOCAL);
  if (!error.ok()) {
    std::string error_message = GetSetDescriptionErrorMessage(
        cricket::CS_LOCAL, desc->GetType(), error);
    RTC_LOG(LS_ERROR) << error_message;
    observer->OnSetLocalDescriptionComplete(
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
    pc_->SetSessionError(PeerConnection::SessionError::kContent,
                         error.message());
    std::string error_message =
        GetSetDescriptionErrorMessage(cricket::CS_LOCAL, type, error);
    RTC_LOG(LS_ERROR) << error_message;
    observer->OnSetLocalDescriptionComplete(
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }
  RTC_DCHECK(local_description());

  if (local_description()->GetType() == SdpType::kAnswer) {
    pc_->RemoveStoppedTransceivers();

    // TODO(deadbeef): We already had to hop to the network thread for
    // MaybeStartGathering...
    pc_->network_thread()->Invoke<void>(
        RTC_FROM_HERE, rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                                 pc_->port_allocator_.get()));
    // Make UMA notes about what was agreed to.
    pc_->ReportNegotiatedSdpSemantics(*local_description());
  }

  observer->OnSetLocalDescriptionComplete(RTCError::OK());
  pc_->NoteUsageEvent(
      PeerConnection::UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED);

  // Check if negotiation is needed. We must do this after informing the
  // observer that SetLocalDescription() has completed to ensure negotiation is
  // not needed prior to the promise resolving.
  if (IsUnifiedPlan()) {
    bool was_negotiation_needed = is_negotiation_needed_;
    UpdateNegotiationNeeded();
    if (signaling_state() == PeerConnectionInterface::kStable &&
        was_negotiation_needed && is_negotiation_needed_) {
      // Legacy version.
      pc_->Observer()->OnRenegotiationNeeded();
      // Spec-compliant version; the event may get invalidated before firing.
      GenerateNegotiationNeededEvent();
    }
  }

  // MaybeStartGathering needs to be called after informing the observer so that
  // we don't signal any candidates before signaling that SetLocalDescription
  // completed.
  pc_->transport_controller_->MaybeStartGathering();
}

void SdpOfferAnswerHandler::DoCreateOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    rtc::scoped_refptr<CreateSessionDescriptionObserver> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::DoCreateOffer");

  if (!observer) {
    RTC_LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }

  if (pc_->IsClosed()) {
    std::string error = "CreateOffer called when PeerConnection is closed.";
    RTC_LOG(LS_ERROR) << error;
    pc_->PostCreateSessionDescriptionFailure(
        observer, RTCError(RTCErrorType::INVALID_STATE, std::move(error)));
    return;
  }

  // If a session error has occurred the PeerConnection is in a possibly
  // inconsistent state so fail right away.
  if (pc_->session_error() != PeerConnection::SessionError::kNone) {
    std::string error_message = pc_->GetSessionErrorMsg();
    RTC_LOG(LS_ERROR) << "CreateOffer: " << error_message;
    pc_->PostCreateSessionDescriptionFailure(
        observer,
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }

  if (!ValidateOfferAnswerOptions(options)) {
    std::string error = "CreateOffer called with invalid options.";
    RTC_LOG(LS_ERROR) << error;
    pc_->PostCreateSessionDescriptionFailure(
        observer, RTCError(RTCErrorType::INVALID_PARAMETER, std::move(error)));
    return;
  }

  // Legacy handling for offer_to_receive_audio and offer_to_receive_video.
  // Specified in WebRTC section 4.4.3.2 "Legacy configuration extensions".
  if (IsUnifiedPlan()) {
    RTCError error = pc_->HandleLegacyOfferOptions(options);
    if (!error.ok()) {
      pc_->PostCreateSessionDescriptionFailure(observer, std::move(error));
      return;
    }
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForOffer(options, &session_options);
  webrtc_session_desc_factory_->CreateOffer(observer, options, session_options);
}

void SdpOfferAnswerHandler::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Chain this operation. If asynchronous operations are pending on the chain,
  // this operation will be queued to be invoked, otherwise the contents of the
  // lambda will execute immediately.
  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       observer_refptr =
           rtc::scoped_refptr<CreateSessionDescriptionObserver>(observer),
       options](std::function<void()> operations_chain_callback) {
        // Abort early if |this_weak_ptr| is no longer valid.
        if (!this_weak_ptr) {
          observer_refptr->OnFailure(RTCError(
              RTCErrorType::INTERNAL_ERROR,
              "CreateAnswer failed because the session was shut down"));
          operations_chain_callback();
          return;
        }
        // The operation completes asynchronously when the wrapper is invoked.
        rtc::scoped_refptr<CreateSessionDescriptionObserverOperationWrapper>
            observer_wrapper(new rtc::RefCountedObject<
                             CreateSessionDescriptionObserverOperationWrapper>(
                std::move(observer_refptr),
                std::move(operations_chain_callback)));
        this_weak_ptr->DoCreateAnswer(options, observer_wrapper);
      });
}

void SdpOfferAnswerHandler::DoCreateAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    rtc::scoped_refptr<CreateSessionDescriptionObserver> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::DoCreateAnswer");
  if (!observer) {
    RTC_LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }

  // If a session error has occurred the PeerConnection is in a possibly
  // inconsistent state so fail right away.
  if (pc_->session_error() != PeerConnection::SessionError::kNone) {
    std::string error_message = pc_->GetSessionErrorMsg();
    RTC_LOG(LS_ERROR) << "CreateAnswer: " << error_message;
    pc_->PostCreateSessionDescriptionFailure(
        observer,
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }

  if (!(signaling_state_ == PeerConnectionInterface::kHaveRemoteOffer ||
        signaling_state_ == PeerConnectionInterface::kHaveLocalPrAnswer)) {
    std::string error =
        "PeerConnection cannot create an answer in a state other than "
        "have-remote-offer or have-local-pranswer.";
    RTC_LOG(LS_ERROR) << error;
    pc_->PostCreateSessionDescriptionFailure(
        observer, RTCError(RTCErrorType::INVALID_STATE, std::move(error)));
    return;
  }

  // The remote description should be set if we're in the right state.
  RTC_DCHECK(remote_description());

  if (IsUnifiedPlan()) {
    if (options.offer_to_receive_audio !=
        PeerConnectionInterface::RTCOfferAnswerOptions::kUndefined) {
      RTC_LOG(LS_WARNING) << "CreateAnswer: offer_to_receive_audio is not "
                             "supported with Unified Plan semantics. Use the "
                             "RtpTransceiver API instead.";
    }
    if (options.offer_to_receive_video !=
        PeerConnectionInterface::RTCOfferAnswerOptions::kUndefined) {
      RTC_LOG(LS_WARNING) << "CreateAnswer: offer_to_receive_video is not "
                             "supported with Unified Plan semantics. Use the "
                             "RtpTransceiver API instead.";
    }
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForAnswer(options, &session_options);
  webrtc_session_desc_factory_->CreateAnswer(observer, session_options);
}

void SdpOfferAnswerHandler::DoSetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::DoSetRemoteDescription");

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
  if (pc_->session_error() != PeerConnection::SessionError::kNone) {
    std::string error_message = pc_->GetSessionErrorMsg();
    RTC_LOG(LS_ERROR) << "SetRemoteDescription: " << error_message;
    observer->OnSetRemoteDescriptionComplete(
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }
  if (IsUnifiedPlan()) {
    if (pc_->configuration()->enable_implicit_rollback) {
      if (desc->GetType() == SdpType::kOffer &&
          signaling_state() == PeerConnectionInterface::kHaveLocalOffer) {
        Rollback(desc->GetType());
      }
    }
    // Explicit rollback.
    if (desc->GetType() == SdpType::kRollback) {
      observer->OnSetRemoteDescriptionComplete(Rollback(desc->GetType()));
      return;
    }
  } else if (desc->GetType() == SdpType::kRollback) {
    observer->OnSetRemoteDescriptionComplete(
        RTCError(RTCErrorType::UNSUPPORTED_OPERATION,
                 "Rollback not supported in Plan B"));
    return;
  }
  if (desc->GetType() == SdpType::kOffer) {
    // Report to UMA the format of the received offer.
    pc_->ReportSdpFormatReceived(*desc);
  }

  // Handle remote descriptions missing a=mid lines for interop with legacy end
  // points.
  FillInMissingRemoteMids(desc->description());

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
    pc_->SetSessionError(PeerConnection::SessionError::kContent,
                         error.message());
    std::string error_message =
        GetSetDescriptionErrorMessage(cricket::CS_REMOTE, type, error);
    RTC_LOG(LS_ERROR) << error_message;
    observer->OnSetRemoteDescriptionComplete(
        RTCError(error.type(), std::move(error_message)));
    return;
  }
  RTC_DCHECK(remote_description());

  if (type == SdpType::kAnswer) {
    pc_->RemoveStoppedTransceivers();
    // TODO(deadbeef): We already had to hop to the network thread for
    // MaybeStartGathering...
    pc_->network_thread()->Invoke<void>(
        RTC_FROM_HERE, rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                                 pc_->port_allocator_.get()));
    // Make UMA notes about what was agreed to.
    pc_->ReportNegotiatedSdpSemantics(*remote_description());
  }

  observer->OnSetRemoteDescriptionComplete(RTCError::OK());
  pc_->NoteUsageEvent(
      PeerConnection::UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED);

  // Check if negotiation is needed. We must do this after informing the
  // observer that SetRemoteDescription() has completed to ensure negotiation is
  // not needed prior to the promise resolving.
  if (IsUnifiedPlan()) {
    bool was_negotiation_needed = is_negotiation_needed_;
    UpdateNegotiationNeeded();
    if (signaling_state() == PeerConnectionInterface::kStable &&
        was_negotiation_needed && is_negotiation_needed_) {
      // Legacy version.
      pc_->Observer()->OnRenegotiationNeeded();
      // Spec-compliant version; the event may get invalidated before firing.
      GenerateNegotiationNeededEvent();
    }
  }
}

void SdpOfferAnswerHandler::SetAssociatedRemoteStreams(
    rtc::scoped_refptr<RtpReceiverInternal> receiver,
    const std::vector<std::string>& stream_ids,
    std::vector<rtc::scoped_refptr<MediaStreamInterface>>* added_streams,
    std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> media_streams;
  for (const std::string& stream_id : stream_ids) {
    rtc::scoped_refptr<MediaStreamInterface> stream =
        pc_->remote_streams_internal()->find(stream_id);
    if (!stream) {
      stream = MediaStreamProxy::Create(rtc::Thread::Current(),
                                        MediaStream::Create(stream_id));
      pc_->remote_streams_internal()->AddStream(stream);
      added_streams->push_back(stream);
    }
    media_streams.push_back(stream);
  }
  // Special case: "a=msid" missing, use random stream ID.
  if (media_streams.empty() &&
      !(remote_description()->description()->msid_signaling() &
        cricket::kMsidSignalingMediaSection)) {
    if (!missing_msid_default_stream_) {
      missing_msid_default_stream_ = MediaStreamProxy::Create(
          rtc::Thread::Current(), MediaStream::Create(rtc::CreateRandomUuid()));
      added_streams->push_back(missing_msid_default_stream_);
    }
    media_streams.push_back(missing_msid_default_stream_);
  }
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> previous_streams =
      receiver->streams();
  // SetStreams() will add/remove the receiver's track to/from the streams. This
  // differs from the spec - the spec uses an "addList" and "removeList" to
  // update the stream-track relationships in a later step. We do this earlier,
  // changing the order of things, but the end-result is the same.
  // TODO(hbos): When we remove remote_streams(), use set_stream_ids()
  // instead. https://crbug.com/webrtc/9480
  receiver->SetStreams(media_streams);
  pc_->RemoveRemoteStreamsIfEmpty(previous_streams, removed_streams);
}

bool SdpOfferAnswerHandler::AddIceCandidate(
    const IceCandidateInterface* ice_candidate) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::AddIceCandidate");
  if (pc_->IsClosed()) {
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
  bool ready = pc_->ReadyToUseRemoteCandidate(ice_candidate, nullptr, &valid);
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
    bool result = pc_->UseCandidate(ice_candidate);
    if (result) {
      pc_->NoteUsageEvent(
          PeerConnection::UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED);
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

void SdpOfferAnswerHandler::AddIceCandidate(
    std::unique_ptr<IceCandidateInterface> candidate,
    std::function<void(RTCError)> callback) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Chain this operation. If asynchronous operations are pending on the chain,
  // this operation will be queued to be invoked, otherwise the contents of the
  // lambda will execute immediately.
  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       candidate = std::move(candidate), callback = std::move(callback)](
          std::function<void()> operations_chain_callback) {
        if (!this_weak_ptr) {
          operations_chain_callback();
          callback(RTCError(
              RTCErrorType::INVALID_STATE,
              "AddIceCandidate failed because the session was shut down"));
          return;
        }
        if (!this_weak_ptr->AddIceCandidate(candidate.get())) {
          operations_chain_callback();
          // Fail with an error type and message consistent with Chromium.
          // TODO(hbos): Fail with error types according to spec.
          callback(RTCError(RTCErrorType::UNSUPPORTED_OPERATION,
                            "Error processing ICE candidate"));
          return;
        }
        operations_chain_callback();
        callback(RTCError::OK());
      });
}

bool SdpOfferAnswerHandler::RemoveIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::RemoveIceCandidates");
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (pc_->IsClosed()) {
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
  RTCError error =
      pc_->transport_controller_->RemoveRemoteCandidates(candidates);
  if (!error.ok()) {
    RTC_LOG(LS_ERROR)
        << "RemoveIceCandidates: Error when removing remote candidates: "
        << error.message();
  }
  return true;
}

void SdpOfferAnswerHandler::AddLocalIceCandidate(
    const JsepIceCandidate* candidate) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (local_description()) {
    mutable_local_description()->AddCandidate(candidate);
  }
}

void SdpOfferAnswerHandler::RemoveLocalIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (local_description()) {
    mutable_local_description()->RemoveCandidates(candidates);
  }
}

const SessionDescriptionInterface* SdpOfferAnswerHandler::local_description()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return pending_local_description_ ? pending_local_description_.get()
                                    : current_local_description_.get();
}

const SessionDescriptionInterface* SdpOfferAnswerHandler::remote_description()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return pending_remote_description_ ? pending_remote_description_.get()
                                     : current_remote_description_.get();
}

const SessionDescriptionInterface*
SdpOfferAnswerHandler::current_local_description() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return current_local_description_.get();
}

const SessionDescriptionInterface*
SdpOfferAnswerHandler::current_remote_description() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return current_remote_description_.get();
}

const SessionDescriptionInterface*
SdpOfferAnswerHandler::pending_local_description() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return pending_local_description_.get();
}

const SessionDescriptionInterface*
SdpOfferAnswerHandler::pending_remote_description() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return pending_remote_description_.get();
}

PeerConnectionInterface::SignalingState SdpOfferAnswerHandler::signaling_state()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return signaling_state_;
}

void SdpOfferAnswerHandler::ChangeSignalingState(
    PeerConnectionInterface::SignalingState signaling_state) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (signaling_state_ == signaling_state) {
    return;
  }
  RTC_LOG(LS_INFO) << "Session: " << pc_->session_id() << " Old state: "
                   << GetSignalingStateString(signaling_state_)
                   << " New state: "
                   << GetSignalingStateString(signaling_state);
  signaling_state_ = signaling_state;
  pc_->Observer()->OnSignalingChange(signaling_state_);
}

RTCError SdpOfferAnswerHandler::UpdateSessionState(
    SdpType type,
    cricket::ContentSource source,
    const cricket::SessionDescription* description) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  // If there's already a pending error then no state transition should happen.
  // But all call-sites should be verifying this before calling us!
  RTC_DCHECK(pc_->session_error() == PeerConnection::SessionError::kNone);

  // If this is answer-ish we're ready to let media flow.
  if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
    pc_->EnableSending();
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
    pc_->transceivers_.DiscardStableStates();
    have_pending_rtp_data_channel_ = false;
  }

  // Update internal objects according to the session description's media
  // descriptions.
  RTCError error = pc_->PushdownMediaDescription(type, source);
  if (!error.ok()) {
    return error;
  }

  return RTCError::OK();
}

bool SdpOfferAnswerHandler::ShouldFireNegotiationNeededEvent(
    uint32_t event_id) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Plan B? Always fire to conform with useless legacy behavior.
  if (!IsUnifiedPlan()) {
    return true;
  }
  // The event ID has been invalidated. Either negotiation is no longer needed
  // or a newer negotiation needed event has been generated.
  if (event_id != negotiation_needed_event_id_) {
    return false;
  }
  // The chain is no longer empty, update negotiation needed when it becomes
  // empty. This should generate a newer negotiation needed event, making this
  // one obsolete.
  if (!operations_chain_->IsEmpty()) {
    // Since we just suppressed an event that would have been fired, if
    // negotiation is still needed by the time the chain becomes empty again, we
    // must make sure to generate another event if negotiation is needed then.
    // This happens when |is_negotiation_needed_| goes from false to true, so we
    // set it to false until UpdateNegotiationNeeded() is called.
    is_negotiation_needed_ = false;
    update_negotiation_needed_on_empty_chain_ = true;
    return false;
  }
  // We must not fire if the signaling state is no longer "stable". If
  // negotiation is still needed when we return to "stable", a new negotiation
  // needed event will be generated, so this one can safely be suppressed.
  if (signaling_state_ != PeerConnectionInterface::kStable) {
    return false;
  }
  // All checks have passed - please fire "negotiationneeded" now!
  return true;
}

RTCError SdpOfferAnswerHandler::Rollback(SdpType desc_type) {
  auto state = signaling_state();
  if (state != PeerConnectionInterface::kHaveLocalOffer &&
      state != PeerConnectionInterface::kHaveRemoteOffer) {
    return RTCError(RTCErrorType::INVALID_STATE,
                    "Called in wrong signalingState: " +
                        GetSignalingStateString(signaling_state()));
  }
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> all_added_streams;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> all_removed_streams;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> removed_receivers;

  for (auto&& transceivers_stable_state_pair :
       pc_->transceivers_.StableStates()) {
    auto transceiver = transceivers_stable_state_pair.first;
    auto state = transceivers_stable_state_pair.second;

    if (state.remote_stream_ids()) {
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> added_streams;
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> removed_streams;
      SetAssociatedRemoteStreams(transceiver->internal()->receiver_internal(),
                                 state.remote_stream_ids().value(),
                                 &added_streams, &removed_streams);
      all_added_streams.insert(all_added_streams.end(), added_streams.begin(),
                               added_streams.end());
      all_removed_streams.insert(all_removed_streams.end(),
                                 removed_streams.begin(),
                                 removed_streams.end());
      if (!state.has_m_section() && !state.newly_created()) {
        continue;
      }
    }

    RTC_DCHECK(transceiver->internal()->mid().has_value());
    pc_->DestroyTransceiverChannel(transceiver);

    if (signaling_state() == PeerConnectionInterface::kHaveRemoteOffer &&
        transceiver->receiver()) {
      removed_receivers.push_back(transceiver->receiver());
    }
    if (state.newly_created()) {
      if (transceiver->internal()->reused_for_addtrack()) {
        transceiver->internal()->set_created_by_addtrack(true);
      } else {
        pc_->transceivers_.Remove(transceiver);
      }
    }
    transceiver->internal()->sender_internal()->set_transport(nullptr);
    transceiver->internal()->receiver_internal()->set_transport(nullptr);
    transceiver->internal()->set_mid(state.mid());
    transceiver->internal()->set_mline_index(state.mline_index());
  }
  pc_->transport_controller_->RollbackTransports();
  if (have_pending_rtp_data_channel_) {
    pc_->DestroyDataChannelTransport();
    have_pending_rtp_data_channel_ = false;
  }
  pc_->transceivers_.DiscardStableStates();
  pending_local_description_.reset();
  pending_remote_description_.reset();
  ChangeSignalingState(PeerConnectionInterface::kStable);

  // Once all processing has finished, fire off callbacks.
  for (const auto& receiver : removed_receivers) {
    pc_->Observer()->OnRemoveTrack(receiver);
  }
  for (const auto& stream : all_added_streams) {
    pc_->Observer()->OnAddStream(stream);
  }
  for (const auto& stream : all_removed_streams) {
    pc_->Observer()->OnRemoveStream(stream);
  }

  // The assumption is that in case of implicit rollback UpdateNegotiationNeeded
  // gets called in SetRemoteDescription.
  if (desc_type == SdpType::kRollback) {
    UpdateNegotiationNeeded();
    if (is_negotiation_needed_) {
      // Legacy version.
      pc_->Observer()->OnRenegotiationNeeded();
      // Spec-compliant version; the event may get invalidated before firing.
      GenerateNegotiationNeededEvent();
    }
  }
  return RTCError::OK();
}

bool SdpOfferAnswerHandler::IsUnifiedPlan() const {
  return pc_->IsUnifiedPlan();
}

void SdpOfferAnswerHandler::OnOperationsChainEmpty() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (pc_->IsClosed() || !update_negotiation_needed_on_empty_chain_)
    return;
  update_negotiation_needed_on_empty_chain_ = false;
  // Firing when chain is empty is only supported in Unified Plan to avoid Plan
  // B regressions. (In Plan B, onnegotiationneeded is already broken anyway, so
  // firing it even more might just be confusing.)
  if (IsUnifiedPlan()) {
    UpdateNegotiationNeeded();
  }
}

absl::optional<bool> SdpOfferAnswerHandler::is_caller() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return is_caller_;
}

bool SdpOfferAnswerHandler::HasNewIceCredentials() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return local_ice_credentials_to_replace_->HasIceCredentials();
}

bool SdpOfferAnswerHandler::IceRestartPending(
    const std::string& content_name) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return pending_ice_restarts_.find(content_name) !=
         pending_ice_restarts_.end();
}

void SdpOfferAnswerHandler::UpdateNegotiationNeeded() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!IsUnifiedPlan()) {
    pc_->Observer()->OnRenegotiationNeeded();
    GenerateNegotiationNeededEvent();
    return;
  }

  // In the spec, a task is queued here to run the following steps - this is
  // meant to ensure we do not fire onnegotiationneeded prematurely if multiple
  // changes are being made at once. In order to support Chromium's
  // implementation where the JavaScript representation of the PeerConnection
  // lives on a separate thread though, the queuing of a task is instead
  // performed by the PeerConnectionObserver posting from the signaling thread
  // to the JavaScript main thread that negotiation is needed. And because the
  // Operations Chain lives on the WebRTC signaling thread,
  // ShouldFireNegotiationNeededEvent() must be called before firing the event
  // to ensure the Operations Chain is still empty and the event has not been
  // invalidated.

  // If connection's [[IsClosed]] slot is true, abort these steps.
  if (pc_->IsClosed())
    return;

  // If connection's signaling state is not "stable", abort these steps.
  if (signaling_state() != PeerConnectionInterface::kStable)
    return;

  // NOTE
  // The negotiation-needed flag will be updated once the state transitions to
  // "stable", as part of the steps for setting an RTCSessionDescription.

  // If the result of checking if negotiation is needed is false, clear the
  // negotiation-needed flag by setting connection's [[NegotiationNeeded]] slot
  // to false, and abort these steps.
  bool is_negotiation_needed = CheckIfNegotiationIsNeeded();
  if (!is_negotiation_needed) {
    is_negotiation_needed_ = false;
    // Invalidate any negotiation needed event that may previosuly have been
    // generated.
    ++negotiation_needed_event_id_;
    return;
  }

  // If connection's [[NegotiationNeeded]] slot is already true, abort these
  // steps.
  if (is_negotiation_needed_)
    return;

  // Set connection's [[NegotiationNeeded]] slot to true.
  is_negotiation_needed_ = true;

  // Queue a task that runs the following steps:
  // If connection's [[IsClosed]] slot is true, abort these steps.
  // If connection's [[NegotiationNeeded]] slot is false, abort these steps.
  // Fire an event named negotiationneeded at connection.
  pc_->Observer()->OnRenegotiationNeeded();
  // Fire the spec-compliant version; when ShouldFireNegotiationNeededEvent() is
  // used in the task queued by the observer, this event will only fire when the
  // chain is empty.
  GenerateNegotiationNeededEvent();
}

bool SdpOfferAnswerHandler::CheckIfNegotiationIsNeeded() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // 1. If any implementation-specific negotiation is required, as described at
  // the start of this section, return true.

  // 2. If connection.[[LocalIceCredentialsToReplace]] is not empty, return
  // true.
  if (local_ice_credentials_to_replace_->HasIceCredentials()) {
    return true;
  }

  // 3. Let description be connection.[[CurrentLocalDescription]].
  const SessionDescriptionInterface* description = current_local_description();
  if (!description)
    return true;

  // 4. If connection has created any RTCDataChannels, and no m= section in
  // description has been negotiated yet for data, return true.
  if (pc_->data_channel_controller()->HasSctpDataChannels()) {
    if (!cricket::GetFirstDataContent(description->description()->contents()))
      return true;
  }

  // 5. For each transceiver in connection's set of transceivers, perform the
  // following checks:
  for (const auto& transceiver : pc_->transceivers_.List()) {
    const ContentInfo* current_local_msection =
        FindTransceiverMSection(transceiver.get(), description);

    const ContentInfo* current_remote_msection = FindTransceiverMSection(
        transceiver.get(), current_remote_description());

    // 5.4 If transceiver is stopped and is associated with an m= section,
    // but the associated m= section is not yet rejected in
    // connection.[[CurrentLocalDescription]] or
    // connection.[[CurrentRemoteDescription]], return true.
    if (transceiver->stopped()) {
      RTC_DCHECK(transceiver->stopping());
      if (current_local_msection && !current_local_msection->rejected &&
          ((current_remote_msection && !current_remote_msection->rejected) ||
           !current_remote_msection)) {
        return true;
      }
      continue;
    }

    // 5.1 If transceiver.[[Stopping]] is true and transceiver.[[Stopped]] is
    // false, return true.
    if (transceiver->stopping() && !transceiver->stopped())
      return true;

    // 5.2 If transceiver isn't stopped and isn't yet associated with an m=
    // section in description, return true.
    if (!current_local_msection)
      return true;

    const MediaContentDescription* current_local_media_description =
        current_local_msection->media_description();
    // 5.3 If transceiver isn't stopped and is associated with an m= section
    // in description then perform the following checks:

    // 5.3.1 If transceiver.[[Direction]] is "sendrecv" or "sendonly", and the
    // associated m= section in description either doesn't contain a single
    // "a=msid" line, or the number of MSIDs from the "a=msid" lines in this
    // m= section, or the MSID values themselves, differ from what is in
    // transceiver.sender.[[AssociatedMediaStreamIds]], return true.
    if (RtpTransceiverDirectionHasSend(transceiver->direction())) {
      if (current_local_media_description->streams().size() == 0)
        return true;

      std::vector<std::string> msection_msids;
      for (const auto& stream : current_local_media_description->streams()) {
        for (const std::string& msid : stream.stream_ids())
          msection_msids.push_back(msid);
      }

      std::vector<std::string> transceiver_msids =
          transceiver->sender()->stream_ids();
      if (msection_msids.size() != transceiver_msids.size())
        return true;

      absl::c_sort(transceiver_msids);
      absl::c_sort(msection_msids);
      if (transceiver_msids != msection_msids)
        return true;
    }

    // 5.3.2 If description is of type "offer", and the direction of the
    // associated m= section in neither connection.[[CurrentLocalDescription]]
    // nor connection.[[CurrentRemoteDescription]] matches
    // transceiver.[[Direction]], return true.
    if (description->GetType() == SdpType::kOffer) {
      if (!current_remote_description())
        return true;

      if (!current_remote_msection)
        return true;

      RtpTransceiverDirection current_local_direction =
          current_local_media_description->direction();
      RtpTransceiverDirection current_remote_direction =
          current_remote_msection->media_description()->direction();
      if (transceiver->direction() != current_local_direction &&
          transceiver->direction() !=
              RtpTransceiverDirectionReversed(current_remote_direction)) {
        return true;
      }
    }

    // 5.3.3 If description is of type "answer", and the direction of the
    // associated m= section in the description does not match
    // transceiver.[[Direction]] intersected with the offered direction (as
    // described in [JSEP] (section 5.3.1.)), return true.
    if (description->GetType() == SdpType::kAnswer) {
      if (!remote_description())
        return true;

      const ContentInfo* offered_remote_msection =
          FindTransceiverMSection(transceiver.get(), remote_description());

      RtpTransceiverDirection offered_direction =
          offered_remote_msection
              ? offered_remote_msection->media_description()->direction()
              : RtpTransceiverDirection::kInactive;

      if (current_local_media_description->direction() !=
          (RtpTransceiverDirectionIntersection(
              transceiver->direction(),
              RtpTransceiverDirectionReversed(offered_direction)))) {
        return true;
      }
    }
  }

  // If all the preceding checks were performed and true was not returned,
  // nothing remains to be negotiated; return false.
  return false;
}

void SdpOfferAnswerHandler::GenerateNegotiationNeededEvent() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  ++negotiation_needed_event_id_;
  pc_->Observer()->OnNegotiationNeededEvent(negotiation_needed_event_id_);
}

RTCError SdpOfferAnswerHandler::ValidateSessionDescription(
    const SessionDescriptionInterface* sdesc,
    cricket::ContentSource source) {
  if (pc_->session_error() != PeerConnection::SessionError::kNone) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         pc_->GetSessionErrorMsg());
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

  RTCError error = ValidateMids(*sdesc->description());
  if (!error.ok()) {
    return error;
  }

  // Verify crypto settings.
  std::string crypto_error;
  if (webrtc_session_desc_factory_->SdesPolicy() == cricket::SEC_REQUIRED ||
      pc_->dtls_enabled()) {
    RTCError crypto_error =
        VerifyCrypto(sdesc->description(), pc_->dtls_enabled());
    if (!crypto_error.ok()) {
      return crypto_error;
    }
  }

  // Verify ice-ufrag and ice-pwd.
  if (!VerifyIceUfragPwdPresent(sdesc->description())) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         kSdpWithoutIceUfragPwd);
  }

  if (!pc_->ValidateBundleSettings(sdesc->description())) {
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
      const MediaContentDescription& desc = *content.media_description();
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

RTCError SdpOfferAnswerHandler::UpdateTransceiversAndDataChannels(
    cricket::ContentSource source,
    const SessionDescriptionInterface& new_session,
    const SessionDescriptionInterface* old_local_description,
    const SessionDescriptionInterface* old_remote_description) {
  RTC_DCHECK_RUN_ON(signaling_thread());
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
    pc_->mid_generator()->AddKnownId(new_content.name);
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
        // In the case where a transceiver is rejected locally, we don't
        // expect to find a transceiver, but might find it in the case
        // where state is still "stopping", not "stopped".
        if (new_content.rejected) {
          continue;
        }
        return transceiver_or_error.MoveError();
      }
      auto transceiver = transceiver_or_error.MoveValue();
      RTCError error =
          UpdateTransceiverChannel(transceiver, new_content, bundle_group);
      if (!error.ok()) {
        return error;
      }
    } else if (media_type == cricket::MEDIA_TYPE_DATA) {
      if (pc_->GetDataMid() && new_content.name != *(pc_->GetDataMid())) {
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

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
SdpOfferAnswerHandler::AssociateTransceiver(
    cricket::ContentSource source,
    SdpType type,
    size_t mline_index,
    const ContentInfo& content,
    const ContentInfo* old_local_content,
    const ContentInfo* old_remote_content) {
  RTC_DCHECK(IsUnifiedPlan());
  // If this is an offer then the m= section might be recycled. If the m=
  // section is being recycled (defined as: rejected in the current local or
  // remote description and not rejected in new description), the transceiver
  // should have been removed by RemoveStoppedTransceivers().
  if (IsMediaSectionBeingRecycled(type, content, old_local_content,
                                  old_remote_content)) {
    const std::string& old_mid =
        (old_local_content && old_local_content->rejected)
            ? old_local_content->name
            : old_remote_content->name;
    auto old_transceiver = pc_->GetAssociatedTransceiver(old_mid);
    // The transceiver should be disassociated in RemoveStoppedTransceivers()
    RTC_DCHECK(!old_transceiver);
  }
  const MediaContentDescription* media_desc = content.media_description();
  auto transceiver = pc_->GetAssociatedTransceiver(content.name);
  if (source == cricket::CS_LOCAL) {
    // Find the RtpTransceiver that corresponds to this m= section, using the
    // mapping between transceivers and m= section indices established when
    // creating the offer.
    if (!transceiver) {
      transceiver = pc_->GetTransceiverByMLineIndex(mline_index);
    }
    if (!transceiver) {
      // This may happen normally when media sections are rejected.
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "Transceiver not found based on m-line index");
    }
  } else {
    RTC_DCHECK_EQ(source, cricket::CS_REMOTE);
    // If the m= section is sendrecv or recvonly, and there are RtpTransceivers
    // of the same type...
    // When simulcast is requested, a transceiver cannot be associated because
    // AddTrack cannot be called to initialize it.
    if (!transceiver &&
        RtpTransceiverDirectionHasRecv(media_desc->direction()) &&
        !media_desc->HasSimulcast()) {
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
      std::vector<RtpEncodingParameters> send_encodings =
          GetSendEncodingsFromRemoteDescription(*media_desc);
      auto sender = pc_->CreateSender(media_desc->type(), sender_id, nullptr,
                                      {}, send_encodings);
      std::string receiver_id;
      if (!media_desc->streams().empty()) {
        receiver_id = media_desc->streams()[0].id;
      } else {
        receiver_id = rtc::CreateRandomUuid();
      }
      auto receiver = pc_->CreateReceiver(media_desc->type(), receiver_id);
      transceiver = pc_->CreateAndAddTransceiver(sender, receiver);
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kRecvOnly);
      if (type == SdpType::kOffer) {
        pc_->transceivers_.StableState(transceiver)->set_newly_created();
      }
    }
    // Check if the offer indicated simulcast but the answer rejected it.
    // This can happen when simulcast is not supported on the remote party.
    if (SimulcastIsRejected(old_local_content, *media_desc)) {
      RTC_HISTOGRAM_BOOLEAN(kSimulcastDisabled, true);
      RTCError error =
          DisableSimulcastInSender(transceiver->internal()->sender_internal());
      if (!error.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to remove rejected simulcast.";
        return std::move(error);
      }
    }
  }
  RTC_DCHECK(transceiver);
  if (transceiver->media_type() != media_desc->type()) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Transceiver type does not match media description type.");
  }
  if (media_desc->HasSimulcast()) {
    std::vector<SimulcastLayer> layers =
        source == cricket::CS_LOCAL
            ? media_desc->simulcast_description().send_layers().GetAllLayers()
            : media_desc->simulcast_description()
                  .receive_layers()
                  .GetAllLayers();
    RTCError error = UpdateSimulcastLayerStatusInSender(
        layers, transceiver->internal()->sender_internal());
    if (!error.ok()) {
      RTC_LOG(LS_ERROR) << "Failed updating status for simulcast layers.";
      return std::move(error);
    }
  }
  if (type == SdpType::kOffer) {
    bool state_changes = transceiver->internal()->mid() != content.name ||
                         transceiver->internal()->mline_index() != mline_index;
    if (state_changes) {
      pc_->transceivers_.StableState(transceiver)
          ->SetMSectionIfUnset(transceiver->internal()->mid(),
                               transceiver->internal()->mline_index());
    }
  }
  // Associate the found or created RtpTransceiver with the m= section by
  // setting the value of the RtpTransceiver's mid property to the MID of the m=
  // section, and establish a mapping between the transceiver and the index of
  // the m= section.
  transceiver->internal()->set_mid(content.name);
  transceiver->internal()->set_mline_index(mline_index);
  return std::move(transceiver);
}

RTCErrorOr<const cricket::ContentGroup*>
SdpOfferAnswerHandler::GetEarlyBundleGroup(
    const SessionDescription& desc) const {
  const cricket::ContentGroup* bundle_group = nullptr;
  if (pc_->configuration()->bundle_policy ==
      PeerConnectionInterface::kBundlePolicyMaxBundle) {
    bundle_group = desc.GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
    if (!bundle_group) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max-bundle configured but session description "
                           "has no BUNDLE group");
    }
  }
  return bundle_group;
}

RTCError SdpOfferAnswerHandler::UpdateTransceiverChannel(
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
      pc_->DestroyChannelInterface(channel);
    }
  } else {
    if (!channel) {
      if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
        channel = pc_->CreateVoiceChannel(content.name);
      } else {
        RTC_DCHECK_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->media_type());
        channel = pc_->CreateVideoChannel(content.name);
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

RTCError SdpOfferAnswerHandler::UpdateDataChannel(
    cricket::ContentSource source,
    const cricket::ContentInfo& content,
    const cricket::ContentGroup* bundle_group) {
  if (pc_->data_channel_type() == cricket::DCT_NONE) {
    // If data channels are disabled, ignore this media section. CreateAnswer
    // will take care of rejecting it.
    return RTCError::OK();
  }
  if (content.rejected) {
    RTC_LOG(LS_INFO) << "Rejected data channel, mid=" << content.mid();
    pc_->DestroyDataChannelTransport();
  } else {
    if (!pc_->data_channel_controller()->rtp_data_channel() &&
        !pc_->data_channel_controller()->data_channel_transport()) {
      RTC_LOG(LS_INFO) << "Creating data channel, mid=" << content.mid();
      if (!pc_->CreateDataChannel(content.name)) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                             "Failed to create data channel.");
      }
    }
    if (source == cricket::CS_REMOTE) {
      const MediaContentDescription* data_desc = content.media_description();
      if (data_desc && cricket::IsRtpProtocol(data_desc->protocol())) {
        pc_->data_channel_controller()->UpdateRemoteRtpDataChannels(
            GetActiveStreams(data_desc));
      }
    }
  }
  return RTCError::OK();
}

bool SdpOfferAnswerHandler::ExpectSetLocalDescription(SdpType type) {
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

bool SdpOfferAnswerHandler::ExpectSetRemoteDescription(SdpType type) {
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

void SdpOfferAnswerHandler::FillInMissingRemoteMids(
    cricket::SessionDescription* new_remote_description) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(new_remote_description);
  const cricket::ContentInfos no_infos;
  const cricket::ContentInfos& local_contents =
      (local_description() ? local_description()->description()->contents()
                           : no_infos);
  const cricket::ContentInfos& remote_contents =
      (remote_description() ? remote_description()->description()->contents()
                            : no_infos);
  for (size_t i = 0; i < new_remote_description->contents().size(); ++i) {
    cricket::ContentInfo& content = new_remote_description->contents()[i];
    if (!content.name.empty()) {
      continue;
    }
    std::string new_mid;
    absl::string_view source_explanation;
    if (IsUnifiedPlan()) {
      if (i < local_contents.size()) {
        new_mid = local_contents[i].name;
        source_explanation = "from the matching local media section";
      } else if (i < remote_contents.size()) {
        new_mid = remote_contents[i].name;
        source_explanation = "from the matching previous remote media section";
      } else {
        new_mid = pc_->mid_generator()->GenerateString();
        source_explanation = "generated just now";
      }
    } else {
      new_mid = std::string(
          GetDefaultMidForPlanB(content.media_description()->type()));
      source_explanation = "to match pre-existing behavior";
    }
    RTC_DCHECK(!new_mid.empty());
    content.name = new_mid;
    new_remote_description->transport_infos()[i].content_name = new_mid;
    RTC_LOG(LS_INFO) << "SetRemoteDescription: Remote media section at i=" << i
                     << " is missing an a=mid line. Filling in the value '"
                     << new_mid << "' " << source_explanation << ".";
  }
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
SdpOfferAnswerHandler::FindAvailableTransceiverToReceive(
    cricket::MediaType media_type) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());
  // From JSEP section 5.10 (Applying a Remote Description):
  // If the m= section is sendrecv or recvonly, and there are RtpTransceivers of
  // the same type that were added to the PeerConnection by addTrack and are not
  // associated with any m= section and are not stopped, find the first such
  // RtpTransceiver.
  for (auto transceiver : pc_->transceivers_.List()) {
    if (transceiver->media_type() == media_type &&
        transceiver->internal()->created_by_addtrack() && !transceiver->mid() &&
        !transceiver->stopped()) {
      return transceiver;
    }
  }
  return nullptr;
}

const cricket::ContentInfo*
SdpOfferAnswerHandler::FindMediaSectionForTransceiver(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver,
    const SessionDescriptionInterface* sdesc) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
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

void SdpOfferAnswerHandler::GetOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
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
  if (pc_->data_channel_controller()->HasRtpDataChannels() ||
      pc_->data_channel_type() != cricket::DCT_RTP) {
    session_options->data_channel_type = pc_->data_channel_type();
  }

  // Apply ICE restart flag and renomination flag.
  bool ice_restart = offer_answer_options.ice_restart || HasNewIceCredentials();
  for (auto& options : session_options->media_description_options) {
    options.transport_options.ice_restart = ice_restart;
    options.transport_options.enable_ice_renomination =
        pc_->configuration()->enable_ice_renomination;
  }

  session_options->rtcp_cname = pc_->rtcp_cname_;
  session_options->crypto_options = pc_->GetCryptoOptions();
  session_options->pooled_ice_credentials =
      pc_->network_thread()->Invoke<std::vector<cricket::IceParameters>>(
          RTC_FROM_HERE,
          rtc::Bind(&cricket::PortAllocator::GetPooledIceCredentials,
                    pc_->port_allocator_.get()));
  session_options->offer_extmap_allow_mixed =
      pc_->configuration()->offer_extmap_allow_mixed;

  // Allow fallback for using obsolete SCTP syntax.
  // Note that the default in |session_options| is true, while
  // the default in |options| is false.
  session_options->use_obsolete_sctp_sdp =
      offer_answer_options.use_obsolete_sctp_sdp;
}

void SdpOfferAnswerHandler::GetOptionsForPlanBOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  // Figure out transceiver directional preferences.
  bool send_audio = !pc_->GetAudioTransceiver()->internal()->senders().empty();
  bool send_video = !pc_->GetVideoTransceiver()->internal()->senders().empty();

  // By default, generate sendrecv/recvonly m= sections.
  bool recv_audio = true;
  bool recv_video = true;

  // By default, only offer a new m= section if we have media to send with it.
  bool offer_new_audio_description = send_audio;
  bool offer_new_video_description = send_video;
  bool offer_new_data_description =
      pc_->data_channel_controller()->HasDataChannels();

  // The "offer_to_receive_X" options allow those defaults to be overridden.
  if (offer_answer_options.offer_to_receive_audio !=
      PeerConnectionInterface::RTCOfferAnswerOptions::kUndefined) {
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
    pc_->GenerateMediaDescriptionOptions(
        local_description(),
        RtpTransceiverDirectionFromSendRecv(send_audio, recv_audio),
        RtpTransceiverDirectionFromSendRecv(send_video, recv_video),
        &audio_index, &video_index, &data_index, session_options);
  }

  // Add audio/video/data m= sections to the end if needed.
  if (!audio_index && offer_new_audio_description) {
    cricket::MediaDescriptionOptions options(
        cricket::MEDIA_TYPE_AUDIO, cricket::CN_AUDIO,
        RtpTransceiverDirectionFromSendRecv(send_audio, recv_audio), false);
    options.header_extensions =
        pc_->channel_manager()->GetSupportedAudioRtpHeaderExtensions();
    session_options->media_description_options.push_back(options);
    audio_index = session_options->media_description_options.size() - 1;
  }
  if (!video_index && offer_new_video_description) {
    cricket::MediaDescriptionOptions options(
        cricket::MEDIA_TYPE_VIDEO, cricket::CN_VIDEO,
        RtpTransceiverDirectionFromSendRecv(send_video, recv_video), false);
    options.header_extensions =
        pc_->channel_manager()->GetSupportedVideoRtpHeaderExtensions();
    session_options->media_description_options.push_back(options);
    video_index = session_options->media_description_options.size() - 1;
  }
  if (!data_index && offer_new_data_description) {
    session_options->media_description_options.push_back(
        pc_->GetMediaDescriptionOptionsForActiveData(cricket::CN_DATA));
    data_index = session_options->media_description_options.size() - 1;
  }

  cricket::MediaDescriptionOptions* audio_media_description_options =
      !audio_index ? nullptr
                   : &session_options->media_description_options[*audio_index];
  cricket::MediaDescriptionOptions* video_media_description_options =
      !video_index ? nullptr
                   : &session_options->media_description_options[*video_index];

  AddPlanBRtpSenderOptions(pc_->GetSendersInternal(),
                           audio_media_description_options,
                           video_media_description_options,
                           offer_answer_options.num_simulcast_layers);
}

void SdpOfferAnswerHandler::GetOptionsForUnifiedPlanOffer(
    const RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  // Rules for generating an offer are dictated by JSEP sections 5.2.1 (Initial
  // Offers) and 5.2.2 (Subsequent Offers).
  RTC_DCHECK_EQ(session_options->media_description_options.size(), 0);
  const ContentInfos no_infos;
  const ContentInfos& local_contents =
      (local_description() ? local_description()->description()->contents()
                           : no_infos);
  const ContentInfos& remote_contents =
      (remote_description() ? remote_description()->description()->contents()
                            : no_infos);
  // The mline indices that can be recycled. New transceivers should reuse these
  // slots first.
  std::queue<size_t> recycleable_mline_indices;
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
      // A media section is considered eligible for recycling if it is marked as
      // rejected in either the current local or current remote description.
      auto transceiver = pc_->GetAssociatedTransceiver(mid);
      if (!transceiver) {
        // No associated transceiver. The media section has been stopped.
        recycleable_mline_indices.push(i);
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(media_type, mid,
                                             RtpTransceiverDirection::kInactive,
                                             /*stopped=*/true));
      } else {
        // NOTE: a stopping transceiver should be treated as a stopped one in
        // createOffer as specified in
        // https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-createoffer.
        if (had_been_rejected && transceiver->stopping()) {
          session_options->media_description_options.push_back(
              cricket::MediaDescriptionOptions(
                  transceiver->media_type(), mid,
                  RtpTransceiverDirection::kInactive,
                  /*stopped=*/true));
          recycleable_mline_indices.push(i);
        } else {
          session_options->media_description_options.push_back(
              GetMediaDescriptionOptionsForTransceiver(
                  transceiver, mid,
                  /*is_create_offer=*/true));
          // CreateOffer shouldn't really cause any state changes in
          // PeerConnection, but we need a way to match new transceivers to new
          // media sections in SetLocalDescription and JSEP specifies this is
          // done by recording the index of the media section generated for the
          // transceiver in the offer.
          transceiver->internal()->set_mline_index(i);
        }
      }
    } else {
      RTC_CHECK_EQ(cricket::MEDIA_TYPE_DATA, media_type);
      if (had_been_rejected) {
        session_options->media_description_options.push_back(
            pc_->GetMediaDescriptionOptionsForRejectedData(mid));
      } else {
        RTC_CHECK(pc_->GetDataMid());
        if (mid == *(pc_->GetDataMid())) {
          session_options->media_description_options.push_back(
              pc_->GetMediaDescriptionOptionsForActiveData(mid));
        } else {
          session_options->media_description_options.push_back(
              pc_->GetMediaDescriptionOptionsForRejectedData(mid));
        }
      }
    }
  }

  // Next, look for transceivers that are newly added (that is, are not stopped
  // and not associated). Reuse media sections marked as recyclable first,
  // otherwise append to the end of the offer. New media sections should be
  // added in the order they were added to the PeerConnection.
  for (const auto& transceiver : pc_->transceivers_.List()) {
    if (transceiver->mid() || transceiver->stopping()) {
      continue;
    }
    size_t mline_index;
    if (!recycleable_mline_indices.empty()) {
      mline_index = recycleable_mline_indices.front();
      recycleable_mline_indices.pop();
      session_options->media_description_options[mline_index] =
          GetMediaDescriptionOptionsForTransceiver(
              transceiver, pc_->mid_generator()->GenerateString(),
              /*is_create_offer=*/true);
    } else {
      mline_index = session_options->media_description_options.size();
      session_options->media_description_options.push_back(
          GetMediaDescriptionOptionsForTransceiver(
              transceiver, pc_->mid_generator()->GenerateString(),
              /*is_create_offer=*/true));
    }
    // See comment above for why CreateOffer changes the transceiver's state.
    transceiver->internal()->set_mline_index(mline_index);
  }
  // Lastly, add a m-section if we have local data channels and an m section
  // does not already exist.
  if (!pc_->GetDataMid() && pc_->data_channel_controller()->HasDataChannels()) {
    session_options->media_description_options.push_back(
        pc_->GetMediaDescriptionOptionsForActiveData(
            pc_->mid_generator()->GenerateString()));
  }
}

void SdpOfferAnswerHandler::GetOptionsForAnswer(
    const RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
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
  if (pc_->data_channel_controller()->HasRtpDataChannels() ||
      pc_->data_channel_type() != cricket::DCT_RTP) {
    session_options->data_channel_type = pc_->data_channel_type();
  }

  // Apply ICE renomination flag.
  for (auto& options : session_options->media_description_options) {
    options.transport_options.enable_ice_renomination =
        pc_->configuration()->enable_ice_renomination;
  }

  session_options->rtcp_cname = pc_->rtcp_cname_;
  session_options->crypto_options = pc_->GetCryptoOptions();
  session_options->pooled_ice_credentials =
      pc_->network_thread()->Invoke<std::vector<cricket::IceParameters>>(
          RTC_FROM_HERE,
          rtc::Bind(&cricket::PortAllocator::GetPooledIceCredentials,
                    pc_->port_allocator_.get()));
}

void SdpOfferAnswerHandler::GetOptionsForPlanBAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  // Figure out transceiver directional preferences.
  bool send_audio = !pc_->GetAudioTransceiver()->internal()->senders().empty();
  bool send_video = !pc_->GetVideoTransceiver()->internal()->senders().empty();

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
  pc_->GenerateMediaDescriptionOptions(
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

  AddPlanBRtpSenderOptions(pc_->GetSendersInternal(),
                           audio_media_description_options,
                           video_media_description_options,
                           offer_answer_options.num_simulcast_layers);
}

void SdpOfferAnswerHandler::GetOptionsForUnifiedPlanAnswer(
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
      auto transceiver = pc_->GetAssociatedTransceiver(content.name);
      if (transceiver) {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForTransceiver(
                transceiver, content.name,
                /*is_create_offer=*/false));
      } else {
        // This should only happen with rejected transceivers.
        RTC_DCHECK(content.rejected);
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(media_type, content.name,
                                             RtpTransceiverDirection::kInactive,
                                             /*stopped=*/true));
      }
    } else {
      RTC_CHECK_EQ(cricket::MEDIA_TYPE_DATA, media_type);
      // Reject all data sections if data channels are disabled.
      // Reject a data section if it has already been rejected.
      // Reject all data sections except for the first one.
      if (pc_->data_channel_type() == cricket::DCT_NONE || content.rejected ||
          content.name != *(pc_->GetDataMid())) {
        session_options->media_description_options.push_back(
            pc_->GetMediaDescriptionOptionsForRejectedData(content.name));
      } else {
        session_options->media_description_options.push_back(
            pc_->GetMediaDescriptionOptionsForActiveData(content.name));
      }
    }
  }
}

}  // namespace webrtc
