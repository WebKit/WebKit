/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/mediasession.h"

#include <algorithm>  // For std::find_if, std::sort.
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>

#include "absl/types/optional.h"
#include "api/cryptoparams.h"
#include "common_types.h"  // NOLINT(build/include)
#include "media/base/h264_profile_level_id.h"
#include "media/base/mediaconstants.h"
#include "p2p/base/p2pconstants.h"
#include "pc/channelmanager.h"
#include "pc/rtpmediautils.h"
#include "pc/srtpfilter.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/third_party/base64/base64.h"

namespace {

using webrtc::RtpTransceiverDirection;

const char kInline[] = "inline:";

void GetSupportedSdesCryptoSuiteNames(void (*func)(const rtc::CryptoOptions&,
                                                   std::vector<int>*),
                                      const rtc::CryptoOptions& crypto_options,
                                      std::vector<std::string>* names) {
  std::vector<int> crypto_suites;
  func(crypto_options, &crypto_suites);
  for (const auto crypto : crypto_suites) {
    names->push_back(rtc::SrtpCryptoSuiteToName(crypto));
  }
}
}  // namespace

namespace cricket {

// RTP Profile names
// http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xml
// RFC4585
const char kMediaProtocolAvpf[] = "RTP/AVPF";
// RFC5124
const char kMediaProtocolDtlsSavpf[] = "UDP/TLS/RTP/SAVPF";

// We always generate offers with "UDP/TLS/RTP/SAVPF" when using DTLS-SRTP,
// but we tolerate "RTP/SAVPF" in offers we receive, for compatibility.
const char kMediaProtocolSavpf[] = "RTP/SAVPF";

const char kMediaProtocolRtpPrefix[] = "RTP/";

const char kMediaProtocolSctp[] = "SCTP";
const char kMediaProtocolDtlsSctp[] = "DTLS/SCTP";
const char kMediaProtocolUdpDtlsSctp[] = "UDP/DTLS/SCTP";
const char kMediaProtocolTcpDtlsSctp[] = "TCP/DTLS/SCTP";

// Note that the below functions support some protocol strings purely for
// legacy compatibility, as required by JSEP in Section 5.1.2, Profile Names
// and Interoperability.

static bool IsDtlsRtp(const std::string& protocol) {
  // Most-likely values first.
  return protocol == "UDP/TLS/RTP/SAVPF" || protocol == "TCP/TLS/RTP/SAVPF" ||
         protocol == "UDP/TLS/RTP/SAVP" || protocol == "TCP/TLS/RTP/SAVP";
}

static bool IsPlainRtp(const std::string& protocol) {
  // Most-likely values first.
  return protocol == "RTP/SAVPF" || protocol == "RTP/AVPF" ||
         protocol == "RTP/SAVP" || protocol == "RTP/AVP";
}

static bool IsDtlsSctp(const std::string& protocol) {
  return protocol == kMediaProtocolDtlsSctp ||
         protocol == kMediaProtocolUdpDtlsSctp ||
         protocol == kMediaProtocolTcpDtlsSctp;
}

static bool IsPlainSctp(const std::string& protocol) {
  return protocol == kMediaProtocolSctp;
}

static bool IsSctp(const std::string& protocol) {
  return IsPlainSctp(protocol) || IsDtlsSctp(protocol);
}

static RtpTransceiverDirection NegotiateRtpTransceiverDirection(
    RtpTransceiverDirection offer,
    RtpTransceiverDirection wants) {
  bool offer_send = webrtc::RtpTransceiverDirectionHasSend(offer);
  bool offer_recv = webrtc::RtpTransceiverDirectionHasRecv(offer);
  bool wants_send = webrtc::RtpTransceiverDirectionHasSend(wants);
  bool wants_recv = webrtc::RtpTransceiverDirectionHasRecv(wants);
  return webrtc::RtpTransceiverDirectionFromSendRecv(offer_recv && wants_send,
                                                     offer_send && wants_recv);
}

static bool IsMediaContentOfType(const ContentInfo* content,
                                 MediaType media_type) {
  if (!content || !content->media_description()) {
    return false;
  }
  return content->media_description()->type() == media_type;
}

static bool CreateCryptoParams(int tag,
                               const std::string& cipher,
                               CryptoParams* crypto_out) {
  int key_len;
  int salt_len;
  if (!rtc::GetSrtpKeyAndSaltLengths(rtc::SrtpCryptoSuiteFromName(cipher),
                                     &key_len, &salt_len)) {
    return false;
  }

  int master_key_len = key_len + salt_len;
  std::string master_key;
  if (!rtc::CreateRandomData(master_key_len, &master_key)) {
    return false;
  }

  RTC_CHECK_EQ(master_key_len, master_key.size());
  std::string key = rtc::Base64::Encode(master_key);

  crypto_out->tag = tag;
  crypto_out->cipher_suite = cipher;
  crypto_out->key_params = kInline;
  crypto_out->key_params += key;
  return true;
}

static bool AddCryptoParams(const std::string& cipher_suite,
                            CryptoParamsVec* cryptos_out) {
  int size = static_cast<int>(cryptos_out->size());

  cryptos_out->resize(size + 1);
  return CreateCryptoParams(size, cipher_suite, &cryptos_out->at(size));
}

void AddMediaCryptos(const CryptoParamsVec& cryptos,
                     MediaContentDescription* media) {
  for (const CryptoParams& crypto : cryptos) {
    media->AddCrypto(crypto);
  }
}

bool CreateMediaCryptos(const std::vector<std::string>& crypto_suites,
                        MediaContentDescription* media) {
  CryptoParamsVec cryptos;
  for (const std::string& crypto_suite : crypto_suites) {
    if (!AddCryptoParams(crypto_suite, &cryptos)) {
      return false;
    }
  }
  AddMediaCryptos(cryptos, media);
  return true;
}

const CryptoParamsVec* GetCryptos(const ContentInfo* content) {
  if (!content || !content->media_description()) {
    return nullptr;
  }
  return &content->media_description()->cryptos();
}

bool FindMatchingCrypto(const CryptoParamsVec& cryptos,
                        const CryptoParams& crypto,
                        CryptoParams* crypto_out) {
  auto it = std::find_if(
      cryptos.begin(), cryptos.end(),
      [&crypto](const CryptoParams& c) { return crypto.Matches(c); });
  if (it == cryptos.end()) {
    return false;
  }
  *crypto_out = *it;
  return true;
}

// For audio, HMAC 32 (if enabled) is prefered over HMAC 80 because of the
// low overhead.
void GetSupportedAudioSdesCryptoSuites(const rtc::CryptoOptions& crypto_options,
                                       std::vector<int>* crypto_suites) {
  if (crypto_options.enable_gcm_crypto_suites) {
    crypto_suites->push_back(rtc::SRTP_AEAD_AES_256_GCM);
    crypto_suites->push_back(rtc::SRTP_AEAD_AES_128_GCM);
  }
  if (crypto_options.enable_aes128_sha1_32_crypto_cipher) {
    crypto_suites->push_back(rtc::SRTP_AES128_CM_SHA1_32);
  }
  crypto_suites->push_back(rtc::SRTP_AES128_CM_SHA1_80);
}

void GetSupportedAudioSdesCryptoSuiteNames(
    const rtc::CryptoOptions& crypto_options,
    std::vector<std::string>* crypto_suite_names) {
  GetSupportedSdesCryptoSuiteNames(GetSupportedAudioSdesCryptoSuites,
                                   crypto_options, crypto_suite_names);
}

void GetSupportedVideoSdesCryptoSuites(const rtc::CryptoOptions& crypto_options,
                                       std::vector<int>* crypto_suites) {
  if (crypto_options.enable_gcm_crypto_suites) {
    crypto_suites->push_back(rtc::SRTP_AEAD_AES_256_GCM);
    crypto_suites->push_back(rtc::SRTP_AEAD_AES_128_GCM);
  }
  crypto_suites->push_back(rtc::SRTP_AES128_CM_SHA1_80);
}

void GetSupportedVideoSdesCryptoSuiteNames(
    const rtc::CryptoOptions& crypto_options,
    std::vector<std::string>* crypto_suite_names) {
  GetSupportedSdesCryptoSuiteNames(GetSupportedVideoSdesCryptoSuites,
                                   crypto_options, crypto_suite_names);
}

void GetSupportedDataSdesCryptoSuites(const rtc::CryptoOptions& crypto_options,
                                      std::vector<int>* crypto_suites) {
  if (crypto_options.enable_gcm_crypto_suites) {
    crypto_suites->push_back(rtc::SRTP_AEAD_AES_256_GCM);
    crypto_suites->push_back(rtc::SRTP_AEAD_AES_128_GCM);
  }
  crypto_suites->push_back(rtc::SRTP_AES128_CM_SHA1_80);
}

void GetSupportedDataSdesCryptoSuiteNames(
    const rtc::CryptoOptions& crypto_options,
    std::vector<std::string>* crypto_suite_names) {
  GetSupportedSdesCryptoSuiteNames(GetSupportedDataSdesCryptoSuites,
                                   crypto_options, crypto_suite_names);
}

// Support any GCM cipher (if enabled through options). For video support only
// 80-bit SHA1 HMAC. For audio 32-bit HMAC is tolerated (if enabled) unless
// bundle is enabled because it is low overhead.
// Pick the crypto in the list that is supported.
static bool SelectCrypto(const MediaContentDescription* offer,
                         bool bundle,
                         const rtc::CryptoOptions& crypto_options,
                         CryptoParams* crypto_out) {
  bool audio = offer->type() == MEDIA_TYPE_AUDIO;
  const CryptoParamsVec& cryptos = offer->cryptos();

  for (const CryptoParams& crypto : cryptos) {
    if ((crypto_options.enable_gcm_crypto_suites &&
         rtc::IsGcmCryptoSuiteName(crypto.cipher_suite)) ||
        rtc::CS_AES_CM_128_HMAC_SHA1_80 == crypto.cipher_suite ||
        (rtc::CS_AES_CM_128_HMAC_SHA1_32 == crypto.cipher_suite && audio &&
         !bundle && crypto_options.enable_aes128_sha1_32_crypto_cipher)) {
      return CreateCryptoParams(crypto.tag, crypto.cipher_suite, crypto_out);
    }
  }
  return false;
}

// Generate random SSRC values that are not already present in |params_vec|.
// The generated values are added to |ssrcs|.
// |num_ssrcs| is the number of the SSRC will be generated.
static void GenerateSsrcs(const StreamParamsVec& params_vec,
                          int num_ssrcs,
                          std::vector<uint32_t>* ssrcs) {
  for (int i = 0; i < num_ssrcs; i++) {
    uint32_t candidate;
    do {
      candidate = rtc::CreateRandomNonZeroId();
    } while (GetStreamBySsrc(params_vec, candidate) ||
             std::count(ssrcs->begin(), ssrcs->end(), candidate) > 0);
    ssrcs->push_back(candidate);
  }
}

// Finds all StreamParams of all media types and attach them to stream_params.
static void GetCurrentStreamParams(const SessionDescription* sdesc,
                                   StreamParamsVec* stream_params) {
  RTC_DCHECK(stream_params);
  if (!sdesc) {
    return;
  }
  for (const ContentInfo& content : sdesc->contents()) {
    if (!content.media_description()) {
      continue;
    }
    for (const StreamParams& params : content.media_description()->streams()) {
      stream_params->push_back(params);
    }
  }
}

// Filters the data codecs for the data channel type.
void FilterDataCodecs(std::vector<DataCodec>* codecs, bool sctp) {
  // Filter RTP codec for SCTP and vice versa.
  const char* codec_name =
      sctp ? kGoogleRtpDataCodecName : kGoogleSctpDataCodecName;
  codecs->erase(std::remove_if(codecs->begin(), codecs->end(),
                               [&codec_name](const DataCodec& codec) {
                                 return CodecNamesEq(codec.name, codec_name);
                               }),
                codecs->end());
}

template <typename IdStruct>
class UsedIds {
 public:
  UsedIds(int min_allowed_id, int max_allowed_id)
      : min_allowed_id_(min_allowed_id),
        max_allowed_id_(max_allowed_id),
        next_id_(max_allowed_id) {}

  // Loops through all Id in |ids| and changes its id if it is
  // already in use by another IdStruct. Call this methods with all Id
  // in a session description to make sure no duplicate ids exists.
  // Note that typename Id must be a type of IdStruct.
  template <typename Id>
  void FindAndSetIdUsed(std::vector<Id>* ids) {
    for (const Id& id : *ids) {
      FindAndSetIdUsed(&id);
    }
  }

  // Finds and sets an unused id if the |idstruct| id is already in use.
  void FindAndSetIdUsed(IdStruct* idstruct) {
    const int original_id = idstruct->id;
    int new_id = idstruct->id;

    if (original_id > max_allowed_id_ || original_id < min_allowed_id_) {
      // If the original id is not in range - this is an id that can't be
      // dynamically changed.
      return;
    }

    if (IsIdUsed(original_id)) {
      new_id = FindUnusedId();
      RTC_LOG(LS_WARNING) << "Duplicate id found. Reassigning from "
                          << original_id << " to " << new_id;
      idstruct->id = new_id;
    }
    SetIdUsed(new_id);
  }

 private:
  // Returns the first unused id in reverse order.
  // This hopefully reduce the risk of more collisions. We want to change the
  // default ids as little as possible.
  int FindUnusedId() {
    while (IsIdUsed(next_id_) && next_id_ >= min_allowed_id_) {
      --next_id_;
    }
    RTC_DCHECK(next_id_ >= min_allowed_id_);
    return next_id_;
  }

  bool IsIdUsed(int new_id) { return id_set_.find(new_id) != id_set_.end(); }

  void SetIdUsed(int new_id) { id_set_.insert(new_id); }

  const int min_allowed_id_;
  const int max_allowed_id_;
  int next_id_;
  std::set<int> id_set_;
};

// Helper class used for finding duplicate RTP payload types among audio, video
// and data codecs. When bundle is used the payload types may not collide.
class UsedPayloadTypes : public UsedIds<Codec> {
 public:
  UsedPayloadTypes()
      : UsedIds<Codec>(kDynamicPayloadTypeMin, kDynamicPayloadTypeMax) {}

 private:
  static const int kDynamicPayloadTypeMin = 96;
  static const int kDynamicPayloadTypeMax = 127;
};

// Helper class used for finding duplicate RTP Header extension ids among
// audio and video extensions. Only applies to one-byte header extensions at the
// moment. ids > 14 will always be reported as available.
// TODO(kron): This class needs to be refactored when we start to send two-byte
// header extensions.
class UsedRtpHeaderExtensionIds : public UsedIds<webrtc::RtpExtension> {
 public:
  UsedRtpHeaderExtensionIds()
      : UsedIds<webrtc::RtpExtension>(
            webrtc::RtpExtension::kMinId,
            webrtc::RtpExtension::kOneByteHeaderExtensionMaxId) {}

 private:
};

// Adds a StreamParams for each SenderOptions in |sender_options| to
// content_description.
// |current_params| - All currently known StreamParams of any media type.
template <class C>
static bool AddStreamParams(
    const std::vector<SenderOptions>& sender_options,
    const std::string& rtcp_cname,
    StreamParamsVec* current_streams,
    MediaContentDescriptionImpl<C>* content_description) {
  // SCTP streams are not negotiated using SDP/ContentDescriptions.
  if (IsSctp(content_description->protocol())) {
    return true;
  }

  const bool include_rtx_streams =
      ContainsRtxCodec(content_description->codecs());

  const bool include_flexfec_stream =
      ContainsFlexfecCodec(content_description->codecs());

  for (const SenderOptions& sender : sender_options) {
    // groupid is empty for StreamParams generated using
    // MediaSessionDescriptionFactory.
    StreamParams* param =
        GetStreamByIds(*current_streams, "" /*group_id*/, sender.track_id);
    if (!param) {
      // This is a new sender.
      std::vector<uint32_t> ssrcs;
      GenerateSsrcs(*current_streams, sender.num_sim_layers, &ssrcs);
      StreamParams stream_param;
      stream_param.id = sender.track_id;
      // Add the generated ssrc.
      for (size_t i = 0; i < ssrcs.size(); ++i) {
        stream_param.ssrcs.push_back(ssrcs[i]);
      }
      if (sender.num_sim_layers > 1) {
        SsrcGroup group(kSimSsrcGroupSemantics, stream_param.ssrcs);
        stream_param.ssrc_groups.push_back(group);
      }
      // Generate extra ssrcs for include_rtx_streams case.
      if (include_rtx_streams) {
        // Generate an RTX ssrc for every ssrc in the group.
        std::vector<uint32_t> rtx_ssrcs;
        GenerateSsrcs(*current_streams, static_cast<int>(ssrcs.size()),
                      &rtx_ssrcs);
        for (size_t i = 0; i < ssrcs.size(); ++i) {
          stream_param.AddFidSsrc(ssrcs[i], rtx_ssrcs[i]);
        }
      }
      // Generate extra ssrc for include_flexfec_stream case.
      if (include_flexfec_stream) {
        // TODO(brandtr): Update when we support multistream protection.
        if (ssrcs.size() == 1) {
          std::vector<uint32_t> flexfec_ssrcs;
          GenerateSsrcs(*current_streams, 1, &flexfec_ssrcs);
          stream_param.AddFecFrSsrc(ssrcs[0], flexfec_ssrcs[0]);
        } else if (!ssrcs.empty()) {
          RTC_LOG(LS_WARNING)
              << "Our FlexFEC implementation only supports protecting "
                 "a single media streams. This session has multiple "
                 "media streams however, so no FlexFEC SSRC will be generated.";
        }
      }
      stream_param.cname = rtcp_cname;
      stream_param.set_stream_ids(sender.stream_ids);
      content_description->AddStream(stream_param);

      // Store the new StreamParams in current_streams.
      // This is necessary so that we can use the CNAME for other media types.
      current_streams->push_back(stream_param);
    } else {
      // Use existing generated SSRCs/groups, but update the sync_label if
      // necessary. This may be needed if a MediaStreamTrack was moved from one
      // MediaStream to another.
      param->set_stream_ids(sender.stream_ids);
      content_description->AddStream(*param);
    }
  }
  return true;
}

// Updates the transport infos of the |sdesc| according to the given
// |bundle_group|. The transport infos of the content names within the
// |bundle_group| should be updated to use the ufrag, pwd and DTLS role of the
// first content within the |bundle_group|.
static bool UpdateTransportInfoForBundle(const ContentGroup& bundle_group,
                                         SessionDescription* sdesc) {
  // The bundle should not be empty.
  if (!sdesc || !bundle_group.FirstContentName()) {
    return false;
  }

  // We should definitely have a transport for the first content.
  const std::string& selected_content_name = *bundle_group.FirstContentName();
  const TransportInfo* selected_transport_info =
      sdesc->GetTransportInfoByName(selected_content_name);
  if (!selected_transport_info) {
    return false;
  }

  // Set the other contents to use the same ICE credentials.
  const std::string& selected_ufrag =
      selected_transport_info->description.ice_ufrag;
  const std::string& selected_pwd =
      selected_transport_info->description.ice_pwd;
  ConnectionRole selected_connection_role =
      selected_transport_info->description.connection_role;
  for (TransportInfo& transport_info : sdesc->transport_infos()) {
    if (bundle_group.HasContentName(transport_info.content_name) &&
        transport_info.content_name != selected_content_name) {
      transport_info.description.ice_ufrag = selected_ufrag;
      transport_info.description.ice_pwd = selected_pwd;
      transport_info.description.connection_role = selected_connection_role;
    }
  }
  return true;
}

// Gets the CryptoParamsVec of the given |content_name| from |sdesc|, and
// sets it to |cryptos|.
static bool GetCryptosByName(const SessionDescription* sdesc,
                             const std::string& content_name,
                             CryptoParamsVec* cryptos) {
  if (!sdesc || !cryptos) {
    return false;
  }
  const ContentInfo* content = sdesc->GetContentByName(content_name);
  if (!content || !content->media_description()) {
    return false;
  }
  *cryptos = content->media_description()->cryptos();
  return true;
}

// Prunes the |target_cryptos| by removing the crypto params (cipher_suite)
// which are not available in |filter|.
static void PruneCryptos(const CryptoParamsVec& filter,
                         CryptoParamsVec* target_cryptos) {
  if (!target_cryptos) {
    return;
  }

  target_cryptos->erase(
      std::remove_if(target_cryptos->begin(), target_cryptos->end(),
                     // Returns true if the |crypto|'s cipher_suite is not
                     // found in |filter|.
                     [&filter](const CryptoParams& crypto) {
                       for (const CryptoParams& entry : filter) {
                         if (entry.cipher_suite == crypto.cipher_suite)
                           return false;
                       }
                       return true;
                     }),
      target_cryptos->end());
}

bool IsRtpProtocol(const std::string& protocol) {
  return protocol.empty() ||
         (protocol.find(cricket::kMediaProtocolRtpPrefix) != std::string::npos);
}

static bool IsRtpContent(SessionDescription* sdesc,
                         const std::string& content_name) {
  bool is_rtp = false;
  ContentInfo* content = sdesc->GetContentByName(content_name);
  if (content && content->media_description()) {
    is_rtp = IsRtpProtocol(content->media_description()->protocol());
  }
  return is_rtp;
}

// Updates the crypto parameters of the |sdesc| according to the given
// |bundle_group|. The crypto parameters of all the contents within the
// |bundle_group| should be updated to use the common subset of the
// available cryptos.
static bool UpdateCryptoParamsForBundle(const ContentGroup& bundle_group,
                                        SessionDescription* sdesc) {
  // The bundle should not be empty.
  if (!sdesc || !bundle_group.FirstContentName()) {
    return false;
  }

  bool common_cryptos_needed = false;
  // Get the common cryptos.
  const ContentNames& content_names = bundle_group.content_names();
  CryptoParamsVec common_cryptos;
  bool first = true;
  for (const std::string& content_name : content_names) {
    if (!IsRtpContent(sdesc, content_name)) {
      continue;
    }
    // The common cryptos are needed if any of the content does not have DTLS
    // enabled.
    if (!sdesc->GetTransportInfoByName(content_name)->description.secure()) {
      common_cryptos_needed = true;
    }
    if (first) {
      first = false;
      // Initial the common_cryptos with the first content in the bundle group.
      if (!GetCryptosByName(sdesc, content_name, &common_cryptos)) {
        return false;
      }
      if (common_cryptos.empty()) {
        // If there's no crypto params, we should just return.
        return true;
      }
    } else {
      CryptoParamsVec cryptos;
      if (!GetCryptosByName(sdesc, content_name, &cryptos)) {
        return false;
      }
      PruneCryptos(cryptos, &common_cryptos);
    }
  }

  if (common_cryptos.empty() && common_cryptos_needed) {
    return false;
  }

  // Update to use the common cryptos.
  for (const std::string& content_name : content_names) {
    if (!IsRtpContent(sdesc, content_name)) {
      continue;
    }
    ContentInfo* content = sdesc->GetContentByName(content_name);
    if (IsMediaContent(content)) {
      MediaContentDescription* media_desc = content->media_description();
      if (!media_desc) {
        return false;
      }
      media_desc->set_cryptos(common_cryptos);
    }
  }
  return true;
}

template <class C>
static bool ContainsRtxCodec(const std::vector<C>& codecs) {
  for (const auto& codec : codecs) {
    if (IsRtxCodec(codec)) {
      return true;
    }
  }
  return false;
}

template <class C>
static bool IsRtxCodec(const C& codec) {
  return STR_CASE_CMP(codec.name.c_str(), kRtxCodecName) == 0;
}

template <class C>
static bool ContainsFlexfecCodec(const std::vector<C>& codecs) {
  for (const auto& codec : codecs) {
    if (IsFlexfecCodec(codec)) {
      return true;
    }
  }
  return false;
}

template <class C>
static bool IsFlexfecCodec(const C& codec) {
  return STR_CASE_CMP(codec.name.c_str(), kFlexfecCodecName) == 0;
}

// Create a media content to be offered for the given |sender_options|,
// according to the given options.rtcp_mux, session_options.is_muc, codecs,
// secure_transport, crypto, and current_streams. If we don't currently have
// crypto (in current_cryptos) and it is enabled (in secure_policy), crypto is
// created (according to crypto_suites). The created content is added to the
// offer.
template <class C>
static bool CreateMediaContentOffer(
    const std::vector<SenderOptions>& sender_options,
    const MediaSessionOptions& session_options,
    const std::vector<C>& codecs,
    const SecurePolicy& secure_policy,
    const CryptoParamsVec* current_cryptos,
    const std::vector<std::string>& crypto_suites,
    const RtpHeaderExtensions& rtp_extensions,
    StreamParamsVec* current_streams,
    MediaContentDescriptionImpl<C>* offer) {
  offer->AddCodecs(codecs);

  offer->set_rtcp_mux(session_options.rtcp_mux_enabled);
  if (offer->type() == cricket::MEDIA_TYPE_VIDEO) {
    offer->set_rtcp_reduced_size(true);
  }
  offer->set_rtp_header_extensions(rtp_extensions);

  if (!AddStreamParams(sender_options, session_options.rtcp_cname,
                       current_streams, offer)) {
    return false;
  }

  if (secure_policy != SEC_DISABLED) {
    if (current_cryptos) {
      AddMediaCryptos(*current_cryptos, offer);
    }
    if (offer->cryptos().empty()) {
      if (!CreateMediaCryptos(crypto_suites, offer)) {
        return false;
      }
    }
  }

  if (secure_policy == SEC_REQUIRED && offer->cryptos().empty()) {
    return false;
  }
  return true;
}

template <class C>
static bool ReferencedCodecsMatch(const std::vector<C>& codecs1,
                                  const int codec1_id,
                                  const std::vector<C>& codecs2,
                                  const int codec2_id) {
  const C* codec1 = FindCodecById(codecs1, codec1_id);
  const C* codec2 = FindCodecById(codecs2, codec2_id);
  return codec1 != nullptr && codec2 != nullptr && codec1->Matches(*codec2);
}

template <class C>
static void NegotiateCodecs(const std::vector<C>& local_codecs,
                            const std::vector<C>& offered_codecs,
                            std::vector<C>* negotiated_codecs) {
  for (const C& ours : local_codecs) {
    C theirs;
    // Note that we intentionally only find one matching codec for each of our
    // local codecs, in case the remote offer contains duplicate codecs.
    if (FindMatchingCodec(local_codecs, offered_codecs, ours, &theirs)) {
      C negotiated = ours;
      negotiated.IntersectFeedbackParams(theirs);
      if (IsRtxCodec(negotiated)) {
        const auto apt_it =
            theirs.params.find(kCodecParamAssociatedPayloadType);
        // FindMatchingCodec shouldn't return something with no apt value.
        RTC_DCHECK(apt_it != theirs.params.end());
        negotiated.SetParam(kCodecParamAssociatedPayloadType, apt_it->second);
      }
      if (CodecNamesEq(ours.name.c_str(), kH264CodecName)) {
        webrtc::H264::GenerateProfileLevelIdForAnswer(
            ours.params, theirs.params, &negotiated.params);
      }
      negotiated.id = theirs.id;
      negotiated.name = theirs.name;
      negotiated_codecs->push_back(std::move(negotiated));
    }
  }
  // RFC3264: Although the answerer MAY list the formats in their desired
  // order of preference, it is RECOMMENDED that unless there is a
  // specific reason, the answerer list formats in the same relative order
  // they were present in the offer.
  std::unordered_map<int, int> payload_type_preferences;
  int preference = static_cast<int>(offered_codecs.size() + 1);
  for (const C& codec : offered_codecs) {
    payload_type_preferences[codec.id] = preference--;
  }
  std::sort(negotiated_codecs->begin(), negotiated_codecs->end(),
            [&payload_type_preferences](const C& a, const C& b) {
              return payload_type_preferences[a.id] >
                     payload_type_preferences[b.id];
            });
}

// Finds a codec in |codecs2| that matches |codec_to_match|, which is
// a member of |codecs1|. If |codec_to_match| is an RTX codec, both
// the codecs themselves and their associated codecs must match.
template <class C>
static bool FindMatchingCodec(const std::vector<C>& codecs1,
                              const std::vector<C>& codecs2,
                              const C& codec_to_match,
                              C* found_codec) {
  // |codec_to_match| should be a member of |codecs1|, in order to look up RTX
  // codecs' associated codecs correctly. If not, that's a programming error.
  RTC_DCHECK(std::find_if(codecs1.begin(), codecs1.end(),
                          [&codec_to_match](const C& codec) {
                            return &codec == &codec_to_match;
                          }) != codecs1.end());
  for (const C& potential_match : codecs2) {
    if (potential_match.Matches(codec_to_match)) {
      if (IsRtxCodec(codec_to_match)) {
        int apt_value_1 = 0;
        int apt_value_2 = 0;
        if (!codec_to_match.GetParam(kCodecParamAssociatedPayloadType,
                                     &apt_value_1) ||
            !potential_match.GetParam(kCodecParamAssociatedPayloadType,
                                      &apt_value_2)) {
          RTC_LOG(LS_WARNING) << "RTX missing associated payload type.";
          continue;
        }
        if (!ReferencedCodecsMatch(codecs1, apt_value_1, codecs2,
                                   apt_value_2)) {
          continue;
        }
      }
      if (found_codec) {
        *found_codec = potential_match;
      }
      return true;
    }
  }
  return false;
}

// Find the codec in |codec_list| that |rtx_codec| is associated with.
template <class C>
static const C* GetAssociatedCodec(const std::vector<C>& codec_list,
                                   const C& rtx_codec) {
  std::string associated_pt_str;
  if (!rtx_codec.GetParam(kCodecParamAssociatedPayloadType,
                          &associated_pt_str)) {
    RTC_LOG(LS_WARNING) << "RTX codec " << rtx_codec.name
                        << " is missing an associated payload type.";
    return nullptr;
  }

  int associated_pt;
  if (!rtc::FromString(associated_pt_str, &associated_pt)) {
    RTC_LOG(LS_WARNING) << "Couldn't convert payload type " << associated_pt_str
                        << " of RTX codec " << rtx_codec.name
                        << " to an integer.";
    return nullptr;
  }

  // Find the associated reference codec for the reference RTX codec.
  const C* associated_codec = FindCodecById(codec_list, associated_pt);
  if (!associated_codec) {
    RTC_LOG(LS_WARNING) << "Couldn't find associated codec with payload type "
                        << associated_pt << " for RTX codec " << rtx_codec.name
                        << ".";
  }
  return associated_codec;
}

// Adds all codecs from |reference_codecs| to |offered_codecs| that don't
// already exist in |offered_codecs| and ensure the payload types don't
// collide.
template <class C>
static void MergeCodecs(const std::vector<C>& reference_codecs,
                        std::vector<C>* offered_codecs,
                        UsedPayloadTypes* used_pltypes) {
  // Add all new codecs that are not RTX codecs.
  for (const C& reference_codec : reference_codecs) {
    if (!IsRtxCodec(reference_codec) &&
        !FindMatchingCodec<C>(reference_codecs, *offered_codecs,
                              reference_codec, nullptr)) {
      C codec = reference_codec;
      used_pltypes->FindAndSetIdUsed(&codec);
      offered_codecs->push_back(codec);
    }
  }

  // Add all new RTX codecs.
  for (const C& reference_codec : reference_codecs) {
    if (IsRtxCodec(reference_codec) &&
        !FindMatchingCodec<C>(reference_codecs, *offered_codecs,
                              reference_codec, nullptr)) {
      C rtx_codec = reference_codec;
      const C* associated_codec =
          GetAssociatedCodec(reference_codecs, rtx_codec);
      if (!associated_codec) {
        continue;
      }
      // Find a codec in the offered list that matches the reference codec.
      // Its payload type may be different than the reference codec.
      C matching_codec;
      if (!FindMatchingCodec<C>(reference_codecs, *offered_codecs,
                                *associated_codec, &matching_codec)) {
        RTC_LOG(LS_WARNING)
            << "Couldn't find matching " << associated_codec->name << " codec.";
        continue;
      }

      rtx_codec.params[kCodecParamAssociatedPayloadType] =
          rtc::ToString(matching_codec.id);
      used_pltypes->FindAndSetIdUsed(&rtx_codec);
      offered_codecs->push_back(rtx_codec);
    }
  }
}

static bool FindByUriAndEncryption(const RtpHeaderExtensions& extensions,
                                   const webrtc::RtpExtension& ext_to_match,
                                   webrtc::RtpExtension* found_extension) {
  auto it =
      std::find_if(extensions.begin(), extensions.end(),
                   [&ext_to_match](const webrtc::RtpExtension& extension) {
                     // We assume that all URIs are given in a canonical
                     // format.
                     return extension.uri == ext_to_match.uri &&
                            extension.encrypt == ext_to_match.encrypt;
                   });
  if (it == extensions.end()) {
    return false;
  }
  if (found_extension) {
    *found_extension = *it;
  }
  return true;
}

static bool FindByUri(const RtpHeaderExtensions& extensions,
                      const webrtc::RtpExtension& ext_to_match,
                      webrtc::RtpExtension* found_extension) {
  // We assume that all URIs are given in a canonical format.
  const webrtc::RtpExtension* found =
      webrtc::RtpExtension::FindHeaderExtensionByUri(extensions,
                                                     ext_to_match.uri);
  if (!found) {
    return false;
  }
  if (found_extension) {
    *found_extension = *found;
  }
  return true;
}

static bool FindByUriWithEncryptionPreference(
    const RtpHeaderExtensions& extensions,
    const webrtc::RtpExtension& ext_to_match,
    bool encryption_preference,
    webrtc::RtpExtension* found_extension) {
  const webrtc::RtpExtension* unencrypted_extension = nullptr;
  for (const webrtc::RtpExtension& extension : extensions) {
    // We assume that all URIs are given in a canonical format.
    if (extension.uri == ext_to_match.uri) {
      if (!encryption_preference || extension.encrypt) {
        if (found_extension) {
          *found_extension = extension;
        }
        return true;
      }
      unencrypted_extension = &extension;
    }
  }
  if (unencrypted_extension) {
    if (found_extension) {
      *found_extension = *unencrypted_extension;
    }
    return true;
  }
  return false;
}

// Adds all extensions from |reference_extensions| to |offered_extensions| that
// don't already exist in |offered_extensions| and ensure the IDs don't
// collide. If an extension is added, it's also added to |regular_extensions| or
// |encrypted_extensions|, and if the extension is in |regular_extensions| or
// |encrypted_extensions|, its ID is marked as used in |used_ids|.
// |offered_extensions| is for either audio or video while |regular_extensions|
// and |encrypted_extensions| are used for both audio and video. There could be
// overlap between audio extensions and video extensions.
static void MergeRtpHdrExts(const RtpHeaderExtensions& reference_extensions,
                            RtpHeaderExtensions* offered_extensions,
                            RtpHeaderExtensions* regular_extensions,
                            RtpHeaderExtensions* encrypted_extensions,
                            UsedRtpHeaderExtensionIds* used_ids) {
  for (auto reference_extension : reference_extensions) {
    if (!FindByUriAndEncryption(*offered_extensions, reference_extension,
                                nullptr)) {
      webrtc::RtpExtension existing;
      if (reference_extension.encrypt) {
        if (FindByUriAndEncryption(*encrypted_extensions, reference_extension,
                                   &existing)) {
          offered_extensions->push_back(existing);
        } else {
          used_ids->FindAndSetIdUsed(&reference_extension);
          encrypted_extensions->push_back(reference_extension);
          offered_extensions->push_back(reference_extension);
        }
      } else {
        if (FindByUriAndEncryption(*regular_extensions, reference_extension,
                                   &existing)) {
          offered_extensions->push_back(existing);
        } else {
          used_ids->FindAndSetIdUsed(&reference_extension);
          regular_extensions->push_back(reference_extension);
          offered_extensions->push_back(reference_extension);
        }
      }
    }
  }
}

static void AddEncryptedVersionsOfHdrExts(RtpHeaderExtensions* extensions,
                                          RtpHeaderExtensions* all_extensions,
                                          UsedRtpHeaderExtensionIds* used_ids) {
  RtpHeaderExtensions encrypted_extensions;
  for (const webrtc::RtpExtension& extension : *extensions) {
    webrtc::RtpExtension existing;
    // Don't add encrypted extensions again that were already included in a
    // previous offer or regular extensions that are also included as encrypted
    // extensions.
    if (extension.encrypt ||
        !webrtc::RtpExtension::IsEncryptionSupported(extension.uri) ||
        (FindByUriWithEncryptionPreference(*extensions, extension, true,
                                           &existing) &&
         existing.encrypt)) {
      continue;
    }

    if (FindByUri(*all_extensions, extension, &existing)) {
      encrypted_extensions.push_back(existing);
    } else {
      webrtc::RtpExtension encrypted(extension);
      encrypted.encrypt = true;
      used_ids->FindAndSetIdUsed(&encrypted);
      all_extensions->push_back(encrypted);
      encrypted_extensions.push_back(encrypted);
    }
  }
  extensions->insert(extensions->end(), encrypted_extensions.begin(),
                     encrypted_extensions.end());
}

static void NegotiateRtpHeaderExtensions(
    const RtpHeaderExtensions& local_extensions,
    const RtpHeaderExtensions& offered_extensions,
    bool enable_encrypted_rtp_header_extensions,
    RtpHeaderExtensions* negotiated_extenstions) {
  for (const webrtc::RtpExtension& ours : local_extensions) {
    webrtc::RtpExtension theirs;
    if (FindByUriWithEncryptionPreference(
            offered_extensions, ours, enable_encrypted_rtp_header_extensions,
            &theirs)) {
      // We respond with their RTP header extension id.
      negotiated_extenstions->push_back(theirs);
    }
  }
}

static void StripCNCodecs(AudioCodecs* audio_codecs) {
  audio_codecs->erase(std::remove_if(audio_codecs->begin(), audio_codecs->end(),
                                     [](const AudioCodec& codec) {
                                       return STR_CASE_CMP(
                                                  codec.name.c_str(),
                                                  kComfortNoiseCodecName) == 0;
                                     }),
                      audio_codecs->end());
}

// Create a media content to be answered for the given |sender_options|
// according to the given session_options.rtcp_mux, session_options.streams,
// codecs, crypto, and current_streams.  If we don't currently have crypto (in
// current_cryptos) and it is enabled (in secure_policy), crypto is created
// (according to crypto_suites). The codecs, rtcp_mux, and crypto are all
// negotiated with the offer. If the negotiation fails, this method returns
// false.  The created content is added to the offer.
template <class C>
static bool CreateMediaContentAnswer(
    const MediaContentDescriptionImpl<C>* offer,
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const std::vector<C>& local_codecs,
    const SecurePolicy& sdes_policy,
    const CryptoParamsVec* current_cryptos,
    const RtpHeaderExtensions& local_rtp_extenstions,
    bool enable_encrypted_rtp_header_extensions,
    StreamParamsVec* current_streams,
    bool bundle_enabled,
    MediaContentDescriptionImpl<C>* answer) {
  std::vector<C> negotiated_codecs;
  NegotiateCodecs(local_codecs, offer->codecs(), &negotiated_codecs);
  answer->AddCodecs(negotiated_codecs);
  answer->set_protocol(offer->protocol());
  RtpHeaderExtensions negotiated_rtp_extensions;
  NegotiateRtpHeaderExtensions(
      local_rtp_extenstions, offer->rtp_header_extensions(),
      enable_encrypted_rtp_header_extensions, &negotiated_rtp_extensions);
  answer->set_rtp_header_extensions(negotiated_rtp_extensions);

  answer->set_rtcp_mux(session_options.rtcp_mux_enabled && offer->rtcp_mux());
  if (answer->type() == cricket::MEDIA_TYPE_VIDEO) {
    answer->set_rtcp_reduced_size(offer->rtcp_reduced_size());
  }

  if (sdes_policy != SEC_DISABLED) {
    CryptoParams crypto;
    if (SelectCrypto(offer, bundle_enabled, session_options.crypto_options,
                     &crypto)) {
      if (current_cryptos) {
        FindMatchingCrypto(*current_cryptos, crypto, &crypto);
      }
      answer->AddCrypto(crypto);
    }
  }

  if (answer->cryptos().empty() && sdes_policy == SEC_REQUIRED) {
    return false;
  }

  if (!AddStreamParams(media_description_options.sender_options,
                       session_options.rtcp_cname, current_streams, answer)) {
    return false;  // Something went seriously wrong.
  }

  answer->set_direction(NegotiateRtpTransceiverDirection(
      offer->direction(), media_description_options.direction));
  return true;
}

static bool IsMediaProtocolSupported(MediaType type,
                                     const std::string& protocol,
                                     bool secure_transport) {
  // Since not all applications serialize and deserialize the media protocol,
  // we will have to accept |protocol| to be empty.
  if (protocol.empty()) {
    return true;
  }

  if (type == MEDIA_TYPE_DATA) {
    // Check for SCTP, but also for RTP for RTP-based data channels.
    // TODO(pthatcher): Remove RTP once RTP-based data channels are gone.
    if (secure_transport) {
      // Most likely scenarios first.
      return IsDtlsSctp(protocol) || IsDtlsRtp(protocol) ||
             IsPlainRtp(protocol);
    } else {
      return IsPlainSctp(protocol) || IsPlainRtp(protocol);
    }
  }

  // Allow for non-DTLS RTP protocol even when using DTLS because that's what
  // JSEP specifies.
  if (secure_transport) {
    // Most likely scenarios first.
    return IsDtlsRtp(protocol) || IsPlainRtp(protocol);
  } else {
    return IsPlainRtp(protocol);
  }
}

static void SetMediaProtocol(bool secure_transport,
                             MediaContentDescription* desc) {
  if (!desc->cryptos().empty())
    desc->set_protocol(kMediaProtocolSavpf);
  else if (secure_transport)
    desc->set_protocol(kMediaProtocolDtlsSavpf);
  else
    desc->set_protocol(kMediaProtocolAvpf);
}

// Gets the TransportInfo of the given |content_name| from the
// |current_description|. If doesn't exist, returns a new one.
static const TransportDescription* GetTransportDescription(
    const std::string& content_name,
    const SessionDescription* current_description) {
  const TransportDescription* desc = NULL;
  if (current_description) {
    const TransportInfo* info =
        current_description->GetTransportInfoByName(content_name);
    if (info) {
      desc = &info->description;
    }
  }
  return desc;
}

// Gets the current DTLS state from the transport description.
static bool IsDtlsActive(const ContentInfo* content,
                         const SessionDescription* current_description) {
  if (!content) {
    return false;
  }

  size_t msection_index = content - &current_description->contents()[0];

  if (current_description->transport_infos().size() <= msection_index) {
    return false;
  }

  return current_description->transport_infos()[msection_index]
      .description.secure();
}

void MediaDescriptionOptions::AddAudioSender(
    const std::string& track_id,
    const std::vector<std::string>& stream_ids) {
  RTC_DCHECK(type == MEDIA_TYPE_AUDIO);
  AddSenderInternal(track_id, stream_ids, 1);
}

void MediaDescriptionOptions::AddVideoSender(
    const std::string& track_id,
    const std::vector<std::string>& stream_ids,
    int num_sim_layers) {
  RTC_DCHECK(type == MEDIA_TYPE_VIDEO);
  AddSenderInternal(track_id, stream_ids, num_sim_layers);
}

void MediaDescriptionOptions::AddRtpDataChannel(const std::string& track_id,
                                                const std::string& stream_id) {
  RTC_DCHECK(type == MEDIA_TYPE_DATA);
  // TODO(steveanton): Is it the case that RtpDataChannel will never have more
  // than one stream?
  AddSenderInternal(track_id, {stream_id}, 1);
}

void MediaDescriptionOptions::AddSenderInternal(
    const std::string& track_id,
    const std::vector<std::string>& stream_ids,
    int num_sim_layers) {
  // TODO(steveanton): Support any number of stream ids.
  RTC_CHECK(stream_ids.size() == 1U);
  sender_options.push_back(SenderOptions{track_id, stream_ids, num_sim_layers});
}

bool MediaSessionOptions::HasMediaDescription(MediaType type) const {
  return std::find_if(media_description_options.begin(),
                      media_description_options.end(),
                      [type](const MediaDescriptionOptions& t) {
                        return t.type == type;
                      }) != media_description_options.end();
}

MediaSessionDescriptionFactory::MediaSessionDescriptionFactory(
    const TransportDescriptionFactory* transport_desc_factory)
    : transport_desc_factory_(transport_desc_factory) {}

MediaSessionDescriptionFactory::MediaSessionDescriptionFactory(
    ChannelManager* channel_manager,
    const TransportDescriptionFactory* transport_desc_factory)
    : transport_desc_factory_(transport_desc_factory) {
  channel_manager->GetSupportedAudioSendCodecs(&audio_send_codecs_);
  channel_manager->GetSupportedAudioReceiveCodecs(&audio_recv_codecs_);
  channel_manager->GetSupportedAudioRtpHeaderExtensions(&audio_rtp_extensions_);
  channel_manager->GetSupportedVideoCodecs(&video_codecs_);
  channel_manager->GetSupportedVideoRtpHeaderExtensions(&video_rtp_extensions_);
  channel_manager->GetSupportedDataCodecs(&data_codecs_);
  ComputeAudioCodecsIntersectionAndUnion();
}

const AudioCodecs& MediaSessionDescriptionFactory::audio_sendrecv_codecs()
    const {
  return audio_sendrecv_codecs_;
}

const AudioCodecs& MediaSessionDescriptionFactory::audio_send_codecs() const {
  return audio_send_codecs_;
}

const AudioCodecs& MediaSessionDescriptionFactory::audio_recv_codecs() const {
  return audio_recv_codecs_;
}

void MediaSessionDescriptionFactory::set_audio_codecs(
    const AudioCodecs& send_codecs,
    const AudioCodecs& recv_codecs) {
  audio_send_codecs_ = send_codecs;
  audio_recv_codecs_ = recv_codecs;
  ComputeAudioCodecsIntersectionAndUnion();
}

SessionDescription* MediaSessionDescriptionFactory::CreateOffer(
    const MediaSessionOptions& session_options,
    const SessionDescription* current_description) const {
  std::unique_ptr<SessionDescription> offer(new SessionDescription());

  StreamParamsVec current_streams;
  GetCurrentStreamParams(current_description, &current_streams);

  AudioCodecs offer_audio_codecs;
  VideoCodecs offer_video_codecs;
  DataCodecs offer_data_codecs;
  GetCodecsForOffer(current_description, &offer_audio_codecs,
                    &offer_video_codecs, &offer_data_codecs);

  if (!session_options.vad_enabled) {
    // If application doesn't want CN codecs in offer.
    StripCNCodecs(&offer_audio_codecs);
  }
  FilterDataCodecs(&offer_data_codecs,
                   session_options.data_channel_type == DCT_SCTP);

  RtpHeaderExtensions audio_rtp_extensions;
  RtpHeaderExtensions video_rtp_extensions;
  GetRtpHdrExtsToOffer(session_options, current_description,
                       &audio_rtp_extensions, &video_rtp_extensions);

  // Must have options for each existing section.
  if (current_description) {
    RTC_DCHECK(current_description->contents().size() <=
               session_options.media_description_options.size());
  }

  // Iterate through the media description options, matching with existing media
  // descriptions in |current_description|.
  size_t msection_index = 0;
  for (const MediaDescriptionOptions& media_description_options :
       session_options.media_description_options) {
    const ContentInfo* current_content = nullptr;
    if (current_description &&
        msection_index < current_description->contents().size()) {
      current_content = &current_description->contents()[msection_index];
      // Media type must match unless this media section is being recycled.
      RTC_DCHECK(current_content->rejected ||
                 IsMediaContentOfType(current_content,
                                      media_description_options.type));
    }
    switch (media_description_options.type) {
      case MEDIA_TYPE_AUDIO:
        if (!AddAudioContentForOffer(media_description_options, session_options,
                                     current_content, current_description,
                                     audio_rtp_extensions, offer_audio_codecs,
                                     &current_streams, offer.get())) {
          return nullptr;
        }
        break;
      case MEDIA_TYPE_VIDEO:
        if (!AddVideoContentForOffer(media_description_options, session_options,
                                     current_content, current_description,
                                     video_rtp_extensions, offer_video_codecs,
                                     &current_streams, offer.get())) {
          return nullptr;
        }
        break;
      case MEDIA_TYPE_DATA:
        if (!AddDataContentForOffer(media_description_options, session_options,
                                    current_content, current_description,
                                    offer_data_codecs, &current_streams,
                                    offer.get())) {
          return nullptr;
        }
        break;
      default:
        RTC_NOTREACHED();
    }
    ++msection_index;
  }

  // Bundle the contents together, if we've been asked to do so, and update any
  // parameters that need to be tweaked for BUNDLE.
  if (session_options.bundle_enabled && offer->contents().size() > 0u) {
    ContentGroup offer_bundle(GROUP_TYPE_BUNDLE);
    for (const ContentInfo& content : offer->contents()) {
      // TODO(deadbeef): There are conditions that make bundling two media
      // descriptions together illegal. For example, they use the same payload
      // type to represent different codecs, or same IDs for different header
      // extensions. We need to detect this and not try to bundle those media
      // descriptions together.
      offer_bundle.AddContentName(content.name);
    }
    offer->AddGroup(offer_bundle);
    if (!UpdateTransportInfoForBundle(offer_bundle, offer.get())) {
      RTC_LOG(LS_ERROR)
          << "CreateOffer failed to UpdateTransportInfoForBundle.";
      return nullptr;
    }
    if (!UpdateCryptoParamsForBundle(offer_bundle, offer.get())) {
      RTC_LOG(LS_ERROR) << "CreateOffer failed to UpdateCryptoParamsForBundle.";
      return nullptr;
    }
  }

  // The following determines how to signal MSIDs to ensure compatibility with
  // older endpoints (in particular, older Plan B endpoints).
  if (session_options.is_unified_plan) {
    // Be conservative and signal using both a=msid and a=ssrc lines. Unified
    // Plan answerers will look at a=msid and Plan B answerers will look at the
    // a=ssrc MSID line.
    offer->set_msid_signaling(cricket::kMsidSignalingMediaSection |
                              cricket::kMsidSignalingSsrcAttribute);
  } else {
    // Plan B always signals MSID using a=ssrc lines.
    offer->set_msid_signaling(cricket::kMsidSignalingSsrcAttribute);
  }

  return offer.release();
}

SessionDescription* MediaSessionDescriptionFactory::CreateAnswer(
    const SessionDescription* offer,
    const MediaSessionOptions& session_options,
    const SessionDescription* current_description) const {
  if (!offer) {
    return nullptr;
  }
  // The answer contains the intersection of the codecs in the offer with the
  // codecs we support. As indicated by XEP-0167, we retain the same payload ids
  // from the offer in the answer.
  std::unique_ptr<SessionDescription> answer(new SessionDescription());

  StreamParamsVec current_streams;
  GetCurrentStreamParams(current_description, &current_streams);

  // If the offer supports BUNDLE, and we want to use it too, create a BUNDLE
  // group in the answer with the appropriate content names.
  const ContentGroup* offer_bundle = offer->GetGroupByName(GROUP_TYPE_BUNDLE);
  ContentGroup answer_bundle(GROUP_TYPE_BUNDLE);
  // Transport info shared by the bundle group.
  std::unique_ptr<TransportInfo> bundle_transport;

  // Get list of all possible codecs that respects existing payload type
  // mappings and uses a single payload type space.
  //
  // Note that these lists may be further filtered for each m= section; this
  // step is done just to establish the payload type mappings shared by all
  // sections.
  AudioCodecs answer_audio_codecs;
  VideoCodecs answer_video_codecs;
  DataCodecs answer_data_codecs;
  GetCodecsForAnswer(current_description, offer, &answer_audio_codecs,
                     &answer_video_codecs, &answer_data_codecs);

  if (!session_options.vad_enabled) {
    // If application doesn't want CN codecs in answer.
    StripCNCodecs(&answer_audio_codecs);
  }
  FilterDataCodecs(&answer_data_codecs,
                   session_options.data_channel_type == DCT_SCTP);

  // Must have options for exactly as many sections as in the offer.
  RTC_DCHECK(offer->contents().size() ==
             session_options.media_description_options.size());
  // Iterate through the media description options, matching with existing
  // media descriptions in |current_description|.
  size_t msection_index = 0;
  for (const MediaDescriptionOptions& media_description_options :
       session_options.media_description_options) {
    const ContentInfo* offer_content = &offer->contents()[msection_index];
    // Media types and MIDs must match between the remote offer and the
    // MediaDescriptionOptions.
    RTC_DCHECK(
        IsMediaContentOfType(offer_content, media_description_options.type));
    RTC_DCHECK(media_description_options.mid == offer_content->name);
    const ContentInfo* current_content = nullptr;
    if (current_description &&
        msection_index < current_description->contents().size()) {
      current_content = &current_description->contents()[msection_index];
    }
    switch (media_description_options.type) {
      case MEDIA_TYPE_AUDIO:
        if (!AddAudioContentForAnswer(
                media_description_options, session_options, offer_content,
                offer, current_content, current_description,
                bundle_transport.get(), answer_audio_codecs, &current_streams,
                answer.get())) {
          return nullptr;
        }
        break;
      case MEDIA_TYPE_VIDEO:
        if (!AddVideoContentForAnswer(
                media_description_options, session_options, offer_content,
                offer, current_content, current_description,
                bundle_transport.get(), answer_video_codecs, &current_streams,
                answer.get())) {
          return nullptr;
        }
        break;
      case MEDIA_TYPE_DATA:
        if (!AddDataContentForAnswer(media_description_options, session_options,
                                     offer_content, offer, current_content,
                                     current_description,
                                     bundle_transport.get(), answer_data_codecs,
                                     &current_streams, answer.get())) {
          return nullptr;
        }
        break;
      default:
        RTC_NOTREACHED();
    }
    ++msection_index;
    // See if we can add the newly generated m= section to the BUNDLE group in
    // the answer.
    ContentInfo& added = answer->contents().back();
    if (!added.rejected && session_options.bundle_enabled && offer_bundle &&
        offer_bundle->HasContentName(added.name)) {
      answer_bundle.AddContentName(added.name);
      bundle_transport.reset(
          new TransportInfo(*answer->GetTransportInfoByName(added.name)));
    }
  }

  // If a BUNDLE group was offered, put a BUNDLE group in the answer even if
  // it's empty. RFC5888 says:
  //
  //   A SIP entity that receives an offer that contains an "a=group" line
  //   with semantics that are understood MUST return an answer that
  //   contains an "a=group" line with the same semantics.
  if (offer_bundle) {
    answer->AddGroup(answer_bundle);
  }

  if (answer_bundle.FirstContentName()) {
    // Share the same ICE credentials and crypto params across all contents,
    // as BUNDLE requires.
    if (!UpdateTransportInfoForBundle(answer_bundle, answer.get())) {
      RTC_LOG(LS_ERROR)
          << "CreateAnswer failed to UpdateTransportInfoForBundle.";
      return NULL;
    }

    if (!UpdateCryptoParamsForBundle(answer_bundle, answer.get())) {
      RTC_LOG(LS_ERROR)
          << "CreateAnswer failed to UpdateCryptoParamsForBundle.";
      return NULL;
    }
  }

  // The following determines how to signal MSIDs to ensure compatibility with
  // older endpoints (in particular, older Plan B endpoints).
  if (session_options.is_unified_plan) {
    // Unified Plan needs to look at what the offer included to find the most
    // compatible answer.
    if (offer->msid_signaling() == 0) {
      // We end up here in one of three cases:
      // 1. An empty offer. We'll reply with an empty answer so it doesn't
      //    matter what we pick here.
      // 2. A data channel only offer. We won't add any MSIDs to the answer so
      //    it also doesn't matter what we pick here.
      // 3. Media that's either sendonly or inactive from the remote endpoint.
      //    We don't have any information to say whether the endpoint is Plan B
      //    or Unified Plan, so be conservative and send both.
      answer->set_msid_signaling(cricket::kMsidSignalingMediaSection |
                                 cricket::kMsidSignalingSsrcAttribute);
    } else if (offer->msid_signaling() ==
               (cricket::kMsidSignalingMediaSection |
                cricket::kMsidSignalingSsrcAttribute)) {
      // If both a=msid and a=ssrc MSID signaling methods were used, we're
      // probably talking to a Unified Plan endpoint so respond with just
      // a=msid.
      answer->set_msid_signaling(cricket::kMsidSignalingMediaSection);
    } else {
      // Otherwise, it's clear which method the offerer is using so repeat that
      // back to them.
      answer->set_msid_signaling(offer->msid_signaling());
    }
  } else {
    // Plan B always signals MSID using a=ssrc lines.
    answer->set_msid_signaling(cricket::kMsidSignalingSsrcAttribute);
  }

  return answer.release();
}

const AudioCodecs& MediaSessionDescriptionFactory::GetAudioCodecsForOffer(
    const RtpTransceiverDirection& direction) const {
  switch (direction) {
    // If stream is inactive - generate list as if sendrecv.
    case RtpTransceiverDirection::kSendRecv:
    case RtpTransceiverDirection::kInactive:
      return audio_sendrecv_codecs_;
    case RtpTransceiverDirection::kSendOnly:
      return audio_send_codecs_;
    case RtpTransceiverDirection::kRecvOnly:
      return audio_recv_codecs_;
  }
  RTC_NOTREACHED();
  return audio_sendrecv_codecs_;
}

const AudioCodecs& MediaSessionDescriptionFactory::GetAudioCodecsForAnswer(
    const RtpTransceiverDirection& offer,
    const RtpTransceiverDirection& answer) const {
  switch (answer) {
    // For inactive and sendrecv answers, generate lists as if we were to accept
    // the offer's direction. See RFC 3264 Section 6.1.
    case RtpTransceiverDirection::kSendRecv:
    case RtpTransceiverDirection::kInactive:
      return GetAudioCodecsForOffer(
          webrtc::RtpTransceiverDirectionReversed(offer));
    case RtpTransceiverDirection::kSendOnly:
      return audio_send_codecs_;
    case RtpTransceiverDirection::kRecvOnly:
      return audio_recv_codecs_;
  }
  RTC_NOTREACHED();
  return audio_sendrecv_codecs_;
}

void MergeCodecsFromDescription(const SessionDescription* description,
                                AudioCodecs* audio_codecs,
                                VideoCodecs* video_codecs,
                                DataCodecs* data_codecs,
                                UsedPayloadTypes* used_pltypes) {
  RTC_DCHECK(description);
  for (const ContentInfo& content : description->contents()) {
    if (IsMediaContentOfType(&content, MEDIA_TYPE_AUDIO)) {
      const AudioContentDescription* audio =
          content.media_description()->as_audio();
      MergeCodecs<AudioCodec>(audio->codecs(), audio_codecs, used_pltypes);
    } else if (IsMediaContentOfType(&content, MEDIA_TYPE_VIDEO)) {
      const VideoContentDescription* video =
          content.media_description()->as_video();
      MergeCodecs<VideoCodec>(video->codecs(), video_codecs, used_pltypes);
    } else if (IsMediaContentOfType(&content, MEDIA_TYPE_DATA)) {
      const DataContentDescription* data =
          content.media_description()->as_data();
      MergeCodecs<DataCodec>(data->codecs(), data_codecs, used_pltypes);
    }
  }
}

// Getting codecs for an offer involves these steps:
//
// 1. Construct payload type -> codec mappings for current description.
// 2. Add any reference codecs that weren't already present
// 3. For each individual media description (m= section), filter codecs based
//    on the directional attribute (happens in another method).
void MediaSessionDescriptionFactory::GetCodecsForOffer(
    const SessionDescription* current_description,
    AudioCodecs* audio_codecs,
    VideoCodecs* video_codecs,
    DataCodecs* data_codecs) const {
  UsedPayloadTypes used_pltypes;
  audio_codecs->clear();
  video_codecs->clear();
  data_codecs->clear();

  // First - get all codecs from the current description if the media type
  // is used. Add them to |used_pltypes| so the payload type is not reused if a
  // new media type is added.
  if (current_description) {
    MergeCodecsFromDescription(current_description, audio_codecs, video_codecs,
                               data_codecs, &used_pltypes);
  }

  // Add our codecs that are not in |current_description|.
  MergeCodecs<AudioCodec>(all_audio_codecs_, audio_codecs, &used_pltypes);
  MergeCodecs<VideoCodec>(video_codecs_, video_codecs, &used_pltypes);
  MergeCodecs<DataCodec>(data_codecs_, data_codecs, &used_pltypes);
}

// Getting codecs for an answer involves these steps:
//
// 1. Construct payload type -> codec mappings for current description.
// 2. Add any codecs from the offer that weren't already present.
// 3. Add any remaining codecs that weren't already present.
// 4. For each individual media description (m= section), filter codecs based
//    on the directional attribute (happens in another method).
void MediaSessionDescriptionFactory::GetCodecsForAnswer(
    const SessionDescription* current_description,
    const SessionDescription* remote_offer,
    AudioCodecs* audio_codecs,
    VideoCodecs* video_codecs,
    DataCodecs* data_codecs) const {
  UsedPayloadTypes used_pltypes;
  audio_codecs->clear();
  video_codecs->clear();
  data_codecs->clear();

  // First - get all codecs from the current description if the media type
  // is used. Add them to |used_pltypes| so the payload type is not reused if a
  // new media type is added.
  if (current_description) {
    MergeCodecsFromDescription(current_description, audio_codecs, video_codecs,
                               data_codecs, &used_pltypes);
  }

  // Second - filter out codecs that we don't support at all and should ignore.
  AudioCodecs filtered_offered_audio_codecs;
  VideoCodecs filtered_offered_video_codecs;
  DataCodecs filtered_offered_data_codecs;
  for (const ContentInfo& content : remote_offer->contents()) {
    if (IsMediaContentOfType(&content, MEDIA_TYPE_AUDIO)) {
      const AudioContentDescription* audio =
          content.media_description()->as_audio();
      for (const AudioCodec& offered_audio_codec : audio->codecs()) {
        if (!FindMatchingCodec<AudioCodec>(audio->codecs(),
                                           filtered_offered_audio_codecs,
                                           offered_audio_codec, nullptr) &&
            FindMatchingCodec<AudioCodec>(audio->codecs(), all_audio_codecs_,
                                          offered_audio_codec, nullptr)) {
          filtered_offered_audio_codecs.push_back(offered_audio_codec);
        }
      }
    } else if (IsMediaContentOfType(&content, MEDIA_TYPE_VIDEO)) {
      const VideoContentDescription* video =
          content.media_description()->as_video();
      for (const VideoCodec& offered_video_codec : video->codecs()) {
        if (!FindMatchingCodec<VideoCodec>(video->codecs(),
                                           filtered_offered_video_codecs,
                                           offered_video_codec, nullptr) &&
            FindMatchingCodec<VideoCodec>(video->codecs(), video_codecs_,
                                          offered_video_codec, nullptr)) {
          filtered_offered_video_codecs.push_back(offered_video_codec);
        }
      }
    } else if (IsMediaContentOfType(&content, MEDIA_TYPE_DATA)) {
      const DataContentDescription* data =
          content.media_description()->as_data();
      for (const DataCodec& offered_data_codec : data->codecs()) {
        if (!FindMatchingCodec<DataCodec>(data->codecs(),
                                          filtered_offered_data_codecs,
                                          offered_data_codec, nullptr) &&
            FindMatchingCodec<DataCodec>(data->codecs(), data_codecs_,
                                         offered_data_codec, nullptr)) {
          filtered_offered_data_codecs.push_back(offered_data_codec);
        }
      }
    }
  }

  // Add codecs that are not in |current_description| but were in
  // |remote_offer|.
  MergeCodecs<AudioCodec>(filtered_offered_audio_codecs, audio_codecs,
                          &used_pltypes);
  MergeCodecs<VideoCodec>(filtered_offered_video_codecs, video_codecs,
                          &used_pltypes);
  MergeCodecs<DataCodec>(filtered_offered_data_codecs, data_codecs,
                         &used_pltypes);
}

void MediaSessionDescriptionFactory::GetRtpHdrExtsToOffer(
    const MediaSessionOptions& session_options,
    const SessionDescription* current_description,
    RtpHeaderExtensions* offer_audio_extensions,
    RtpHeaderExtensions* offer_video_extensions) const {
  // All header extensions allocated from the same range to avoid potential
  // issues when using BUNDLE.
  UsedRtpHeaderExtensionIds used_ids;
  RtpHeaderExtensions all_regular_extensions;
  RtpHeaderExtensions all_encrypted_extensions;
  offer_audio_extensions->clear();
  offer_video_extensions->clear();

  // First - get all extensions from the current description if the media type
  // is used.
  // Add them to |used_ids| so the local ids are not reused if a new media
  // type is added.
  if (current_description) {
    for (const ContentInfo& content : current_description->contents()) {
      if (IsMediaContentOfType(&content, MEDIA_TYPE_AUDIO)) {
        const AudioContentDescription* audio =
            content.media_description()->as_audio();
        MergeRtpHdrExts(audio->rtp_header_extensions(), offer_audio_extensions,
                        &all_regular_extensions, &all_encrypted_extensions,
                        &used_ids);
      } else if (IsMediaContentOfType(&content, MEDIA_TYPE_VIDEO)) {
        const VideoContentDescription* video =
            content.media_description()->as_video();
        MergeRtpHdrExts(video->rtp_header_extensions(), offer_video_extensions,
                        &all_regular_extensions, &all_encrypted_extensions,
                        &used_ids);
      }
    }
  }

  // Add our default RTP header extensions that are not in
  // |current_description|.
  MergeRtpHdrExts(audio_rtp_header_extensions(session_options.is_unified_plan),
                  offer_audio_extensions, &all_regular_extensions,
                  &all_encrypted_extensions, &used_ids);
  MergeRtpHdrExts(video_rtp_header_extensions(session_options.is_unified_plan),
                  offer_video_extensions, &all_regular_extensions,
                  &all_encrypted_extensions, &used_ids);

  // TODO(jbauch): Support adding encrypted header extensions to existing
  // sessions.
  if (enable_encrypted_rtp_header_extensions_ && !current_description) {
    AddEncryptedVersionsOfHdrExts(offer_audio_extensions,
                                  &all_encrypted_extensions, &used_ids);
    AddEncryptedVersionsOfHdrExts(offer_video_extensions,
                                  &all_encrypted_extensions, &used_ids);
  }
}

bool MediaSessionDescriptionFactory::AddTransportOffer(
    const std::string& content_name,
    const TransportOptions& transport_options,
    const SessionDescription* current_desc,
    SessionDescription* offer_desc) const {
  if (!transport_desc_factory_)
    return false;
  const TransportDescription* current_tdesc =
      GetTransportDescription(content_name, current_desc);
  std::unique_ptr<TransportDescription> new_tdesc(
      transport_desc_factory_->CreateOffer(transport_options, current_tdesc));
  bool ret =
      (new_tdesc.get() != NULL &&
       offer_desc->AddTransportInfo(TransportInfo(content_name, *new_tdesc)));
  if (!ret) {
    RTC_LOG(LS_ERROR) << "Failed to AddTransportOffer, content name="
                      << content_name;
  }
  return ret;
}

TransportDescription* MediaSessionDescriptionFactory::CreateTransportAnswer(
    const std::string& content_name,
    const SessionDescription* offer_desc,
    const TransportOptions& transport_options,
    const SessionDescription* current_desc,
    bool require_transport_attributes) const {
  if (!transport_desc_factory_)
    return NULL;
  const TransportDescription* offer_tdesc =
      GetTransportDescription(content_name, offer_desc);
  const TransportDescription* current_tdesc =
      GetTransportDescription(content_name, current_desc);
  return transport_desc_factory_->CreateAnswer(offer_tdesc, transport_options,
                                               require_transport_attributes,
                                               current_tdesc);
}

bool MediaSessionDescriptionFactory::AddTransportAnswer(
    const std::string& content_name,
    const TransportDescription& transport_desc,
    SessionDescription* answer_desc) const {
  if (!answer_desc->AddTransportInfo(
          TransportInfo(content_name, transport_desc))) {
    RTC_LOG(LS_ERROR) << "Failed to AddTransportAnswer, content name="
                      << content_name;
    return false;
  }
  return true;
}

// |audio_codecs| = set of all possible codecs that can be used, with correct
// payload type mappings
//
// |supported_audio_codecs| = set of codecs that are supported for the direction
// of this m= section
//
// acd->codecs() = set of previously negotiated codecs for this m= section
//
// The payload types should come from audio_codecs, but the order should come
// from acd->codecs() and then supported_codecs, to ensure that re-offers don't
// change existing codec priority, and that new codecs are added with the right
// priority.
bool MediaSessionDescriptionFactory::AddAudioContentForOffer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    const RtpHeaderExtensions& audio_rtp_extensions,
    const AudioCodecs& audio_codecs,
    StreamParamsVec* current_streams,
    SessionDescription* desc) const {
  // Filter audio_codecs (which includes all codecs, with correctly remapped
  // payload types) based on transceiver direction.
  const AudioCodecs& supported_audio_codecs =
      GetAudioCodecsForOffer(media_description_options.direction);

  AudioCodecs filtered_codecs;
  // Add the codecs from current content if it exists and is not being recycled.
  if (current_content && !current_content->rejected) {
    RTC_CHECK(IsMediaContentOfType(current_content, MEDIA_TYPE_AUDIO));
    const AudioContentDescription* acd =
        current_content->media_description()->as_audio();
    for (const AudioCodec& codec : acd->codecs()) {
      if (FindMatchingCodec<AudioCodec>(acd->codecs(), audio_codecs, codec,
                                        nullptr)) {
        filtered_codecs.push_back(codec);
      }
    }
  }
  // Add other supported audio codecs.
  AudioCodec found_codec;
  for (const AudioCodec& codec : supported_audio_codecs) {
    if (FindMatchingCodec<AudioCodec>(supported_audio_codecs, audio_codecs,
                                      codec, &found_codec) &&
        !FindMatchingCodec<AudioCodec>(supported_audio_codecs, filtered_codecs,
                                       codec, nullptr)) {
      // Use the |found_codec| from |audio_codecs| because it has the correctly
      // mapped payload type.
      filtered_codecs.push_back(found_codec);
    }
  }

  cricket::SecurePolicy sdes_policy =
      IsDtlsActive(current_content, current_description) ? cricket::SEC_DISABLED
                                                         : secure();

  std::unique_ptr<AudioContentDescription> audio(new AudioContentDescription());
  std::vector<std::string> crypto_suites;
  GetSupportedAudioSdesCryptoSuiteNames(session_options.crypto_options,
                                        &crypto_suites);
  if (!CreateMediaContentOffer(
          media_description_options.sender_options, session_options,
          filtered_codecs, sdes_policy, GetCryptos(current_content),
          crypto_suites, audio_rtp_extensions, current_streams, audio.get())) {
    return false;
  }

  bool secure_transport = (transport_desc_factory_->secure() != SEC_DISABLED);
  SetMediaProtocol(secure_transport, audio.get());

  audio->set_direction(media_description_options.direction);

  desc->AddContent(media_description_options.mid, MediaProtocolType::kRtp,
                   media_description_options.stopped, audio.release());
  if (!AddTransportOffer(media_description_options.mid,
                         media_description_options.transport_options,
                         current_description, desc)) {
    return false;
  }

  return true;
}

bool MediaSessionDescriptionFactory::AddVideoContentForOffer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    const RtpHeaderExtensions& video_rtp_extensions,
    const VideoCodecs& video_codecs,
    StreamParamsVec* current_streams,
    SessionDescription* desc) const {
  cricket::SecurePolicy sdes_policy =
      IsDtlsActive(current_content, current_description) ? cricket::SEC_DISABLED
                                                         : secure();

  std::unique_ptr<VideoContentDescription> video(new VideoContentDescription());
  std::vector<std::string> crypto_suites;
  GetSupportedVideoSdesCryptoSuiteNames(session_options.crypto_options,
                                        &crypto_suites);

  VideoCodecs filtered_codecs;
  // Add the codecs from current content if it exists and is not being recycled.
  if (current_content && !current_content->rejected) {
    RTC_CHECK(IsMediaContentOfType(current_content, MEDIA_TYPE_VIDEO));
    const VideoContentDescription* vcd =
        current_content->media_description()->as_video();
    for (const VideoCodec& codec : vcd->codecs()) {
      if (FindMatchingCodec<VideoCodec>(vcd->codecs(), video_codecs, codec,
                                        nullptr)) {
        filtered_codecs.push_back(codec);
      }
    }
  }
  // Add other supported video codecs.
  VideoCodec found_codec;
  for (const VideoCodec& codec : video_codecs_) {
    if (FindMatchingCodec<VideoCodec>(video_codecs_, video_codecs, codec,
                                      &found_codec) &&
        !FindMatchingCodec<VideoCodec>(video_codecs_, filtered_codecs, codec,
                                       nullptr)) {
      // Use the |found_codec| from |video_codecs| because it has the correctly
      // mapped payload type.
      filtered_codecs.push_back(found_codec);
    }
  }

  if (!CreateMediaContentOffer(
          media_description_options.sender_options, session_options,
          filtered_codecs, sdes_policy, GetCryptos(current_content),
          crypto_suites, video_rtp_extensions, current_streams, video.get())) {
    return false;
  }

  video->set_bandwidth(kAutoBandwidth);

  bool secure_transport = (transport_desc_factory_->secure() != SEC_DISABLED);
  SetMediaProtocol(secure_transport, video.get());

  video->set_direction(media_description_options.direction);

  desc->AddContent(media_description_options.mid, MediaProtocolType::kRtp,
                   media_description_options.stopped, video.release());
  if (!AddTransportOffer(media_description_options.mid,
                         media_description_options.transport_options,
                         current_description, desc)) {
    return false;
  }
  return true;
}

bool MediaSessionDescriptionFactory::AddDataContentForOffer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    const DataCodecs& data_codecs,
    StreamParamsVec* current_streams,
    SessionDescription* desc) const {
  bool secure_transport = (transport_desc_factory_->secure() != SEC_DISABLED);

  std::unique_ptr<DataContentDescription> data(new DataContentDescription());
  bool is_sctp = (session_options.data_channel_type == DCT_SCTP);
  // If the DataChannel type is not specified, use the DataChannel type in
  // the current description.
  if (session_options.data_channel_type == DCT_NONE && current_content) {
    RTC_CHECK(IsMediaContentOfType(current_content, MEDIA_TYPE_DATA));
    is_sctp = (current_content->media_description()->protocol() ==
               kMediaProtocolSctp);
  }

  cricket::SecurePolicy sdes_policy =
      IsDtlsActive(current_content, current_description) ? cricket::SEC_DISABLED
                                                         : secure();
  std::vector<std::string> crypto_suites;
  if (is_sctp) {
    // SDES doesn't make sense for SCTP, so we disable it, and we only
    // get SDES crypto suites for RTP-based data channels.
    sdes_policy = cricket::SEC_DISABLED;
    // Unlike SetMediaProtocol below, we need to set the protocol
    // before we call CreateMediaContentOffer.  Otherwise,
    // CreateMediaContentOffer won't know this is SCTP and will
    // generate SSRCs rather than SIDs.
    // TODO(deadbeef): Offer kMediaProtocolUdpDtlsSctp (or TcpDtlsSctp), once
    // it's safe to do so. Older versions of webrtc would reject these
    // protocols; see https://bugs.chromium.org/p/webrtc/issues/detail?id=7706.
    data->set_protocol(secure_transport ? kMediaProtocolDtlsSctp
                                        : kMediaProtocolSctp);
  } else {
    GetSupportedDataSdesCryptoSuiteNames(session_options.crypto_options,
                                         &crypto_suites);
  }

  // Even SCTP uses a "codec".
  if (!CreateMediaContentOffer(
          media_description_options.sender_options, session_options,
          data_codecs, sdes_policy, GetCryptos(current_content), crypto_suites,
          RtpHeaderExtensions(), current_streams, data.get())) {
    return false;
  }

  if (is_sctp) {
    desc->AddContent(media_description_options.mid, MediaProtocolType::kSctp,
                     data.release());
  } else {
    data->set_bandwidth(kDataMaxBandwidth);
    SetMediaProtocol(secure_transport, data.get());
    desc->AddContent(media_description_options.mid, MediaProtocolType::kRtp,
                     media_description_options.stopped, data.release());
  }
  if (!AddTransportOffer(media_description_options.mid,
                         media_description_options.transport_options,
                         current_description, desc)) {
    return false;
  }
  return true;
}

// |audio_codecs| = set of all possible codecs that can be used, with correct
// payload type mappings
//
// |supported_audio_codecs| = set of codecs that are supported for the direction
// of this m= section
//
// acd->codecs() = set of previously negotiated codecs for this m= section
//
// The payload types should come from audio_codecs, but the order should come
// from acd->codecs() and then supported_codecs, to ensure that re-offers don't
// change existing codec priority, and that new codecs are added with the right
// priority.
bool MediaSessionDescriptionFactory::AddAudioContentForAnswer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* offer_content,
    const SessionDescription* offer_description,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    const TransportInfo* bundle_transport,
    const AudioCodecs& audio_codecs,
    StreamParamsVec* current_streams,
    SessionDescription* answer) const {
  RTC_CHECK(IsMediaContentOfType(offer_content, MEDIA_TYPE_AUDIO));
  const AudioContentDescription* offer_audio_description =
      offer_content->media_description()->as_audio();

  std::unique_ptr<TransportDescription> audio_transport(
      CreateTransportAnswer(media_description_options.mid, offer_description,
                            media_description_options.transport_options,
                            current_description, bundle_transport != nullptr));
  if (!audio_transport) {
    return false;
  }

  // Pick codecs based on the requested communications direction in the offer
  // and the selected direction in the answer.
  // Note these will be filtered one final time in CreateMediaContentAnswer.
  auto wants_rtd = media_description_options.direction;
  auto offer_rtd = offer_audio_description->direction();
  auto answer_rtd = NegotiateRtpTransceiverDirection(offer_rtd, wants_rtd);
  AudioCodecs supported_audio_codecs =
      GetAudioCodecsForAnswer(offer_rtd, answer_rtd);

  AudioCodecs filtered_codecs;
  // Add the codecs from current content if it exists and is not being recycled.
  if (current_content && !current_content->rejected) {
    RTC_CHECK(IsMediaContentOfType(current_content, MEDIA_TYPE_AUDIO));
    const AudioContentDescription* acd =
        current_content->media_description()->as_audio();
    for (const AudioCodec& codec : acd->codecs()) {
      if (FindMatchingCodec<AudioCodec>(acd->codecs(), audio_codecs, codec,
                                        nullptr)) {
        filtered_codecs.push_back(codec);
      }
    }
  }
  // Add other supported audio codecs.
  for (const AudioCodec& codec : supported_audio_codecs) {
    if (FindMatchingCodec<AudioCodec>(supported_audio_codecs, audio_codecs,
                                      codec, nullptr) &&
        !FindMatchingCodec<AudioCodec>(supported_audio_codecs, filtered_codecs,
                                       codec, nullptr)) {
      // We should use the local codec with local parameters and the codec id
      // would be correctly mapped in |NegotiateCodecs|.
      filtered_codecs.push_back(codec);
    }
  }

  bool bundle_enabled = offer_description->HasGroup(GROUP_TYPE_BUNDLE) &&
                        session_options.bundle_enabled;
  std::unique_ptr<AudioContentDescription> audio_answer(
      new AudioContentDescription());
  // Do not require or create SDES cryptos if DTLS is used.
  cricket::SecurePolicy sdes_policy =
      audio_transport->secure() ? cricket::SEC_DISABLED : secure();
  if (!CreateMediaContentAnswer(
          offer_audio_description, media_description_options, session_options,
          filtered_codecs, sdes_policy, GetCryptos(current_content),
          audio_rtp_header_extensions(session_options.is_unified_plan),
          enable_encrypted_rtp_header_extensions_, current_streams,
          bundle_enabled, audio_answer.get())) {
    return false;  // Fails the session setup.
  }

  bool secure = bundle_transport ? bundle_transport->description.secure()
                                 : audio_transport->secure();
  bool rejected = media_description_options.stopped ||
                  offer_content->rejected ||
                  !IsMediaProtocolSupported(MEDIA_TYPE_AUDIO,
                                            audio_answer->protocol(), secure);
  if (!AddTransportAnswer(media_description_options.mid,
                          *(audio_transport.get()), answer)) {
    return false;
  }

  if (rejected) {
    RTC_LOG(LS_INFO) << "Audio m= section '" << media_description_options.mid
                     << "' being rejected in answer.";
  }

  answer->AddContent(media_description_options.mid, offer_content->type,
                     rejected, audio_answer.release());
  return true;
}

bool MediaSessionDescriptionFactory::AddVideoContentForAnswer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* offer_content,
    const SessionDescription* offer_description,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    const TransportInfo* bundle_transport,
    const VideoCodecs& video_codecs,
    StreamParamsVec* current_streams,
    SessionDescription* answer) const {
  RTC_CHECK(IsMediaContentOfType(offer_content, MEDIA_TYPE_VIDEO));
  const VideoContentDescription* offer_video_description =
      offer_content->media_description()->as_video();

  std::unique_ptr<TransportDescription> video_transport(
      CreateTransportAnswer(media_description_options.mid, offer_description,
                            media_description_options.transport_options,
                            current_description, bundle_transport != nullptr));
  if (!video_transport) {
    return false;
  }

  VideoCodecs filtered_codecs;
  // Add the codecs from current content if it exists and is not being recycled.
  if (current_content && !current_content->rejected) {
    RTC_CHECK(IsMediaContentOfType(current_content, MEDIA_TYPE_VIDEO));
    const VideoContentDescription* vcd =
        current_content->media_description()->as_video();
    for (const VideoCodec& codec : vcd->codecs()) {
      if (FindMatchingCodec<VideoCodec>(vcd->codecs(), video_codecs, codec,
                                        nullptr)) {
        filtered_codecs.push_back(codec);
      }
    }
  }
  // Add other supported video codecs.
  for (const VideoCodec& codec : video_codecs_) {
    if (FindMatchingCodec<VideoCodec>(video_codecs_, video_codecs, codec,
                                      nullptr) &&
        !FindMatchingCodec<VideoCodec>(video_codecs_, filtered_codecs, codec,
                                       nullptr)) {
      // We should use the local codec with local parameters and the codec id
      // would be correctly mapped in |NegotiateCodecs|.
      filtered_codecs.push_back(codec);
    }
  }

  bool bundle_enabled = offer_description->HasGroup(GROUP_TYPE_BUNDLE) &&
                        session_options.bundle_enabled;

  std::unique_ptr<VideoContentDescription> video_answer(
      new VideoContentDescription());
  // Do not require or create SDES cryptos if DTLS is used.
  cricket::SecurePolicy sdes_policy =
      video_transport->secure() ? cricket::SEC_DISABLED : secure();
  if (!CreateMediaContentAnswer(
          offer_video_description, media_description_options, session_options,
          filtered_codecs, sdes_policy, GetCryptos(current_content),
          video_rtp_header_extensions(session_options.is_unified_plan),
          enable_encrypted_rtp_header_extensions_, current_streams,
          bundle_enabled, video_answer.get())) {
    return false;  // Failed the sessin setup.
  }
  bool secure = bundle_transport ? bundle_transport->description.secure()
                                 : video_transport->secure();
  bool rejected = media_description_options.stopped ||
                  offer_content->rejected ||
                  !IsMediaProtocolSupported(MEDIA_TYPE_VIDEO,
                                            video_answer->protocol(), secure);
  if (!AddTransportAnswer(media_description_options.mid,
                          *(video_transport.get()), answer)) {
    return false;
  }

  if (!rejected) {
    video_answer->set_bandwidth(kAutoBandwidth);
  } else {
    RTC_LOG(LS_INFO) << "Video m= section '" << media_description_options.mid
                     << "' being rejected in answer.";
  }
  answer->AddContent(media_description_options.mid, offer_content->type,
                     rejected, video_answer.release());
  return true;
}

bool MediaSessionDescriptionFactory::AddDataContentForAnswer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* offer_content,
    const SessionDescription* offer_description,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    const TransportInfo* bundle_transport,
    const DataCodecs& data_codecs,
    StreamParamsVec* current_streams,
    SessionDescription* answer) const {
  std::unique_ptr<TransportDescription> data_transport(
      CreateTransportAnswer(media_description_options.mid, offer_description,
                            media_description_options.transport_options,
                            current_description, bundle_transport != nullptr));
  if (!data_transport) {
    return false;
  }

  std::unique_ptr<DataContentDescription> data_answer(
      new DataContentDescription());
  // Do not require or create SDES cryptos if DTLS is used.
  cricket::SecurePolicy sdes_policy =
      data_transport->secure() ? cricket::SEC_DISABLED : secure();
  bool bundle_enabled = offer_description->HasGroup(GROUP_TYPE_BUNDLE) &&
                        session_options.bundle_enabled;
  RTC_CHECK(IsMediaContentOfType(offer_content, MEDIA_TYPE_DATA));
  const DataContentDescription* offer_data_description =
      offer_content->media_description()->as_data();
  if (!CreateMediaContentAnswer(
          offer_data_description, media_description_options, session_options,
          data_codecs, sdes_policy, GetCryptos(current_content),
          RtpHeaderExtensions(), enable_encrypted_rtp_header_extensions_,
          current_streams, bundle_enabled, data_answer.get())) {
    return false;  // Fails the session setup.
  }

  // Respond with sctpmap if the offer uses sctpmap.
  bool offer_uses_sctpmap = offer_data_description->use_sctpmap();
  data_answer->set_use_sctpmap(offer_uses_sctpmap);

  bool secure = bundle_transport ? bundle_transport->description.secure()
                                 : data_transport->secure();

  bool rejected = session_options.data_channel_type == DCT_NONE ||
                  media_description_options.stopped ||
                  offer_content->rejected ||
                  !IsMediaProtocolSupported(MEDIA_TYPE_DATA,
                                            data_answer->protocol(), secure);
  if (!AddTransportAnswer(media_description_options.mid,
                          *(data_transport.get()), answer)) {
    return false;
  }

  if (!rejected) {
    data_answer->set_bandwidth(kDataMaxBandwidth);
  } else {
    // RFC 3264
    // The answer MUST contain the same number of m-lines as the offer.
    RTC_LOG(LS_INFO) << "Data is not supported in the answer.";
  }
  answer->AddContent(media_description_options.mid, offer_content->type,
                     rejected, data_answer.release());
  return true;
}

void MediaSessionDescriptionFactory::ComputeAudioCodecsIntersectionAndUnion() {
  audio_sendrecv_codecs_.clear();
  all_audio_codecs_.clear();
  // Compute the audio codecs union.
  for (const AudioCodec& send : audio_send_codecs_) {
    all_audio_codecs_.push_back(send);
    if (!FindMatchingCodec<AudioCodec>(audio_send_codecs_, audio_recv_codecs_,
                                       send, nullptr)) {
      // It doesn't make sense to have an RTX codec we support sending but not
      // receiving.
      RTC_DCHECK(!IsRtxCodec(send));
    }
  }
  for (const AudioCodec& recv : audio_recv_codecs_) {
    if (!FindMatchingCodec<AudioCodec>(audio_recv_codecs_, audio_send_codecs_,
                                       recv, nullptr)) {
      all_audio_codecs_.push_back(recv);
    }
  }
  // Use NegotiateCodecs to merge our codec lists, since the operation is
  // essentially the same. Put send_codecs as the offered_codecs, which is the
  // order we'd like to follow. The reasoning is that encoding is usually more
  // expensive than decoding, and prioritizing a codec in the send list probably
  // means it's a codec we can handle efficiently.
  NegotiateCodecs(audio_recv_codecs_, audio_send_codecs_,
                  &audio_sendrecv_codecs_);
}

bool IsMediaContent(const ContentInfo* content) {
  return (content && (content->type == MediaProtocolType::kRtp ||
                      content->type == MediaProtocolType::kSctp));
}

bool IsAudioContent(const ContentInfo* content) {
  return IsMediaContentOfType(content, MEDIA_TYPE_AUDIO);
}

bool IsVideoContent(const ContentInfo* content) {
  return IsMediaContentOfType(content, MEDIA_TYPE_VIDEO);
}

bool IsDataContent(const ContentInfo* content) {
  return IsMediaContentOfType(content, MEDIA_TYPE_DATA);
}

const ContentInfo* GetFirstMediaContent(const ContentInfos& contents,
                                        MediaType media_type) {
  for (const ContentInfo& content : contents) {
    if (IsMediaContentOfType(&content, media_type)) {
      return &content;
    }
  }
  return nullptr;
}

const ContentInfo* GetFirstAudioContent(const ContentInfos& contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_AUDIO);
}

const ContentInfo* GetFirstVideoContent(const ContentInfos& contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_VIDEO);
}

const ContentInfo* GetFirstDataContent(const ContentInfos& contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_DATA);
}

const ContentInfo* GetFirstMediaContent(const SessionDescription* sdesc,
                                        MediaType media_type) {
  if (sdesc == nullptr) {
    return nullptr;
  }

  return GetFirstMediaContent(sdesc->contents(), media_type);
}

const ContentInfo* GetFirstAudioContent(const SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_AUDIO);
}

const ContentInfo* GetFirstVideoContent(const SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_VIDEO);
}

const ContentInfo* GetFirstDataContent(const SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_DATA);
}

const MediaContentDescription* GetFirstMediaContentDescription(
    const SessionDescription* sdesc,
    MediaType media_type) {
  const ContentInfo* content = GetFirstMediaContent(sdesc, media_type);
  return (content ? content->media_description() : nullptr);
}

const AudioContentDescription* GetFirstAudioContentDescription(
    const SessionDescription* sdesc) {
  return static_cast<const AudioContentDescription*>(
      GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_AUDIO));
}

const VideoContentDescription* GetFirstVideoContentDescription(
    const SessionDescription* sdesc) {
  return static_cast<const VideoContentDescription*>(
      GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_VIDEO));
}

const DataContentDescription* GetFirstDataContentDescription(
    const SessionDescription* sdesc) {
  return static_cast<const DataContentDescription*>(
      GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_DATA));
}

//
// Non-const versions of the above functions.
//

ContentInfo* GetFirstMediaContent(ContentInfos* contents,
                                  MediaType media_type) {
  for (ContentInfo& content : *contents) {
    if (IsMediaContentOfType(&content, media_type)) {
      return &content;
    }
  }
  return nullptr;
}

ContentInfo* GetFirstAudioContent(ContentInfos* contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_AUDIO);
}

ContentInfo* GetFirstVideoContent(ContentInfos* contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_VIDEO);
}

ContentInfo* GetFirstDataContent(ContentInfos* contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_DATA);
}

ContentInfo* GetFirstMediaContent(SessionDescription* sdesc,
                                  MediaType media_type) {
  if (sdesc == nullptr) {
    return nullptr;
  }

  return GetFirstMediaContent(&sdesc->contents(), media_type);
}

ContentInfo* GetFirstAudioContent(SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_AUDIO);
}

ContentInfo* GetFirstVideoContent(SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_VIDEO);
}

ContentInfo* GetFirstDataContent(SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_DATA);
}

MediaContentDescription* GetFirstMediaContentDescription(
    SessionDescription* sdesc,
    MediaType media_type) {
  ContentInfo* content = GetFirstMediaContent(sdesc, media_type);
  return (content ? content->media_description() : nullptr);
}

AudioContentDescription* GetFirstAudioContentDescription(
    SessionDescription* sdesc) {
  return static_cast<AudioContentDescription*>(
      GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_AUDIO));
}

VideoContentDescription* GetFirstVideoContentDescription(
    SessionDescription* sdesc) {
  return static_cast<VideoContentDescription*>(
      GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_VIDEO));
}

DataContentDescription* GetFirstDataContentDescription(
    SessionDescription* sdesc) {
  return static_cast<DataContentDescription*>(
      GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_DATA));
}

}  // namespace cricket
