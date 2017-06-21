/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/mediasession.h"

#include <algorithm>  // For std::find_if, std::sort.
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>

#include "webrtc/base/base64.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/common_types.h"
#include "webrtc/media/base/cryptoparams.h"
#include "webrtc/media/base/h264_profile_level_id.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/p2p/base/p2pconstants.h"
#include "webrtc/pc/channelmanager.h"
#include "webrtc/pc/srtpfilter.h"

namespace {
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

RtpTransceiverDirection RtpTransceiverDirection::FromMediaContentDirection(
    MediaContentDirection md) {
  const bool send = (md == MD_SENDRECV || md == MD_SENDONLY);
  const bool recv = (md == MD_SENDRECV || md == MD_RECVONLY);
  return RtpTransceiverDirection(send, recv);
}

MediaContentDirection RtpTransceiverDirection::ToMediaContentDirection() const {
  if (send && recv) {
    return MD_SENDRECV;
  } else if (send) {
    return MD_SENDONLY;
  } else if (recv) {
    return MD_RECVONLY;
  }

  return MD_INACTIVE;
}

RtpTransceiverDirection
NegotiateRtpTransceiverDirection(RtpTransceiverDirection offer,
                                 RtpTransceiverDirection wants) {
  return RtpTransceiverDirection(offer.recv && wants.send,
                                 offer.send && wants.recv);
}

static bool IsMediaContentOfType(const ContentInfo* content,
                                 MediaType media_type) {
  if (!IsMediaContent(content)) {
    return false;
  }

  const MediaContentDescription* mdesc =
      static_cast<const MediaContentDescription*>(content->description);
  return mdesc && mdesc->type() == media_type;
}

static bool CreateCryptoParams(int tag, const std::string& cipher,
                               CryptoParams *out) {
  int key_len;
  int salt_len;
  if (!rtc::GetSrtpKeyAndSaltLengths(
      rtc::SrtpCryptoSuiteFromName(cipher), &key_len, &salt_len)) {
    return false;
  }

  int master_key_len = key_len + salt_len;
  std::string master_key;
  if (!rtc::CreateRandomData(master_key_len, &master_key)) {
    return false;
  }

  RTC_CHECK_EQ(master_key_len, master_key.size());
  std::string key = rtc::Base64::Encode(master_key);

  out->tag = tag;
  out->cipher_suite = cipher;
  out->key_params = kInline;
  out->key_params += key;
  return true;
}

static bool AddCryptoParams(const std::string& cipher_suite,
                            CryptoParamsVec *out) {
  int size = static_cast<int>(out->size());

  out->resize(size + 1);
  return CreateCryptoParams(size, cipher_suite, &out->at(size));
}

void AddMediaCryptos(const CryptoParamsVec& cryptos,
                     MediaContentDescription* media) {
  for (CryptoParamsVec::const_iterator crypto = cryptos.begin();
       crypto != cryptos.end(); ++crypto) {
    media->AddCrypto(*crypto);
  }
}

bool CreateMediaCryptos(const std::vector<std::string>& crypto_suites,
                        MediaContentDescription* media) {
  CryptoParamsVec cryptos;
  for (std::vector<std::string>::const_iterator it = crypto_suites.begin();
       it != crypto_suites.end(); ++it) {
    if (!AddCryptoParams(*it, &cryptos)) {
      return false;
    }
  }
  AddMediaCryptos(cryptos, media);
  return true;
}

const CryptoParamsVec* GetCryptos(const MediaContentDescription* media) {
  if (!media) {
    return NULL;
  }
  return &media->cryptos();
}

bool FindMatchingCrypto(const CryptoParamsVec& cryptos,
                        const CryptoParams& crypto,
                        CryptoParams* out) {
  for (CryptoParamsVec::const_iterator it = cryptos.begin();
       it != cryptos.end(); ++it) {
    if (crypto.Matches(*it)) {
      *out = *it;
      return true;
    }
  }
  return false;
}

// For audio, HMAC 32 is prefered over HMAC 80 because of the low overhead.
void GetSupportedAudioSdesCryptoSuites(const rtc::CryptoOptions& crypto_options,
                                       std::vector<int>* crypto_suites) {
  if (crypto_options.enable_gcm_crypto_suites) {
    crypto_suites->push_back(rtc::SRTP_AEAD_AES_256_GCM);
    crypto_suites->push_back(rtc::SRTP_AEAD_AES_128_GCM);
  }
  crypto_suites->push_back(rtc::SRTP_AES128_CM_SHA1_32);
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
// 80-bit SHA1 HMAC. For audio 32-bit HMAC is tolerated unless bundle is enabled
// because it is low overhead.
// Pick the crypto in the list that is supported.
static bool SelectCrypto(const MediaContentDescription* offer,
                         bool bundle,
                         const rtc::CryptoOptions& crypto_options,
                         CryptoParams *crypto) {
  bool audio = offer->type() == MEDIA_TYPE_AUDIO;
  const CryptoParamsVec& cryptos = offer->cryptos();

  for (CryptoParamsVec::const_iterator i = cryptos.begin();
       i != cryptos.end(); ++i) {
    if ((crypto_options.enable_gcm_crypto_suites &&
         rtc::IsGcmCryptoSuiteName(i->cipher_suite)) ||
        rtc::CS_AES_CM_128_HMAC_SHA1_80 == i->cipher_suite ||
        (rtc::CS_AES_CM_128_HMAC_SHA1_32 == i->cipher_suite && audio &&
         !bundle)) {
      return CreateCryptoParams(i->tag, i->cipher_suite, crypto);
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
  if (!sdesc)
    return;

  const ContentInfos& contents = sdesc->contents();
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); ++content) {
    if (!IsMediaContent(&*content)) {
      continue;
    }
    const MediaContentDescription* media =
        static_cast<const MediaContentDescription*>(
            content->description);
    const StreamParamsVec& streams = media->streams();
    for (StreamParamsVec::const_iterator it = streams.begin();
         it != streams.end(); ++it) {
      stream_params->push_back(*it);
    }
  }
}

// Filters the data codecs for the data channel type.
void FilterDataCodecs(std::vector<DataCodec>* codecs, bool sctp) {
  // Filter RTP codec for SCTP and vice versa.
  const char* codec_name =
      sctp ? kGoogleRtpDataCodecName : kGoogleSctpDataCodecName;
  for (std::vector<DataCodec>::iterator iter = codecs->begin();
       iter != codecs->end();) {
    if (CodecNamesEq(iter->name, codec_name)) {
      iter = codecs->erase(iter);
    } else {
      ++iter;
    }
  }
}

template <typename IdStruct>
class UsedIds {
 public:
  UsedIds(int min_allowed_id, int max_allowed_id)
      : min_allowed_id_(min_allowed_id),
        max_allowed_id_(max_allowed_id),
        next_id_(max_allowed_id) {
  }

  // Loops through all Id in |ids| and changes its id if it is
  // already in use by another IdStruct. Call this methods with all Id
  // in a session description to make sure no duplicate ids exists.
  // Note that typename Id must be a type of IdStruct.
  template <typename Id>
  void FindAndSetIdUsed(std::vector<Id>* ids) {
    for (typename std::vector<Id>::iterator it = ids->begin();
         it != ids->end(); ++it) {
      FindAndSetIdUsed(&*it);
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
      LOG(LS_WARNING) << "Duplicate id found. Reassigning from " << original_id
          << " to " << new_id;
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

  bool IsIdUsed(int new_id) {
    return id_set_.find(new_id) != id_set_.end();
  }

  void SetIdUsed(int new_id) {
    id_set_.insert(new_id);
  }

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
      : UsedIds<Codec>(kDynamicPayloadTypeMin, kDynamicPayloadTypeMax) {
  }


 private:
  static const int kDynamicPayloadTypeMin = 96;
  static const int kDynamicPayloadTypeMax = 127;
};

// Helper class used for finding duplicate RTP Header extension ids among
// audio and video extensions.
class UsedRtpHeaderExtensionIds : public UsedIds<webrtc::RtpExtension> {
 public:
  UsedRtpHeaderExtensionIds()
      : UsedIds<webrtc::RtpExtension>(webrtc::RtpExtension::kMinId,
                                      webrtc::RtpExtension::kMaxId) {}

 private:
};

// Adds a StreamParams for each Stream in Streams with media type
// media_type to content_description.
// |current_params| - All currently known StreamParams of any media type.
template <class C>
static bool AddStreamParams(MediaType media_type,
                            const MediaSessionOptions& options,
                            StreamParamsVec* current_streams,
                            MediaContentDescriptionImpl<C>* content_description,
                            const bool add_legacy_stream) {
  // SCTP streams are not negotiated using SDP/ContentDescriptions.
  if (IsSctp(content_description->protocol())) {
    return true;
  }

  const bool include_rtx_streams =
      ContainsRtxCodec(content_description->codecs());

  const MediaSessionOptions::Streams& streams = options.streams;
  if (streams.empty() && add_legacy_stream) {
    // TODO(perkj): Remove this legacy stream when all apps use StreamParams.
    std::vector<uint32_t> ssrcs;
    int num_ssrcs = include_rtx_streams ? 2 : 1;
    GenerateSsrcs(*current_streams, num_ssrcs, &ssrcs);
    if (include_rtx_streams) {
      content_description->AddLegacyStream(ssrcs[0], ssrcs[1]);
      content_description->set_multistream(true);
    } else {
      content_description->AddLegacyStream(ssrcs[0]);
    }
    return true;
  }

  const bool include_flexfec_stream =
      ContainsFlexfecCodec(content_description->codecs());

  MediaSessionOptions::Streams::const_iterator stream_it;
  for (stream_it = streams.begin();
       stream_it != streams.end(); ++stream_it) {
    if (stream_it->type != media_type)
      continue;  // Wrong media type.

    StreamParams* param = GetStreamByIds(*current_streams, "", stream_it->id);
    // groupid is empty for StreamParams generated using
    // MediaSessionDescriptionFactory.
    if (!param) {
      // This is a new stream.
      std::vector<uint32_t> ssrcs;
      GenerateSsrcs(*current_streams, stream_it->num_sim_layers, &ssrcs);
      StreamParams stream_param;
      stream_param.id = stream_it->id;
      // Add the generated ssrc.
      for (size_t i = 0; i < ssrcs.size(); ++i) {
        stream_param.ssrcs.push_back(ssrcs[i]);
      }
      if (stream_it->num_sim_layers > 1) {
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
        content_description->set_multistream(true);
      }
      // Generate extra ssrc for include_flexfec_stream case.
      if (include_flexfec_stream) {
        // TODO(brandtr): Update when we support multistream protection.
        if (ssrcs.size() == 1) {
          std::vector<uint32_t> flexfec_ssrcs;
          GenerateSsrcs(*current_streams, 1, &flexfec_ssrcs);
          stream_param.AddFecFrSsrc(ssrcs[0], flexfec_ssrcs[0]);
          content_description->set_multistream(true);
        } else if (!ssrcs.empty()) {
          LOG(LS_WARNING)
              << "Our FlexFEC implementation only supports protecting "
              << "a single media streams. This session has multiple "
              << "media streams however, so no FlexFEC SSRC will be generated.";
        }
      }
      stream_param.cname = options.rtcp_cname;
      stream_param.sync_label = stream_it->sync_label;
      content_description->AddStream(stream_param);

      // Store the new StreamParams in current_streams.
      // This is necessary so that we can use the CNAME for other media types.
      current_streams->push_back(stream_param);
    } else {
      // Use existing generated SSRCs/groups, but update the sync_label if
      // necessary. This may be needed if a MediaStreamTrack was moved from one
      // MediaStream to another.
      param->sync_label = stream_it->sync_label;
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
  for (TransportInfos::iterator it =
           sdesc->transport_infos().begin();
       it != sdesc->transport_infos().end(); ++it) {
    if (bundle_group.HasContentName(it->content_name) &&
        it->content_name != selected_content_name) {
      it->description.ice_ufrag = selected_ufrag;
      it->description.ice_pwd = selected_pwd;
      it->description.connection_role = selected_connection_role;
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
  if (!IsMediaContent(content) || !content->description) {
    return false;
  }

  const MediaContentDescription* media_desc =
      static_cast<const MediaContentDescription*>(content->description);
  *cryptos = media_desc->cryptos();
  return true;
}

// Predicate function used by the remove_if.
// Returns true if the |crypto|'s cipher_suite is not found in |filter|.
static bool CryptoNotFound(const CryptoParams crypto,
                           const CryptoParamsVec* filter) {
  if (filter == NULL) {
    return true;
  }
  for (CryptoParamsVec::const_iterator it = filter->begin();
       it != filter->end(); ++it) {
    if (it->cipher_suite == crypto.cipher_suite) {
      return false;
    }
  }
  return true;
}

// Prunes the |target_cryptos| by removing the crypto params (cipher_suite)
// which are not available in |filter|.
static void PruneCryptos(const CryptoParamsVec& filter,
                         CryptoParamsVec* target_cryptos) {
  if (!target_cryptos) {
    return;
  }
  target_cryptos->erase(std::remove_if(target_cryptos->begin(),
                                       target_cryptos->end(),
                                       bind2nd(ptr_fun(CryptoNotFound),
                                               &filter)),
                        target_cryptos->end());
}

static bool IsRtpProtocol(const std::string& protocol) {
  return protocol.empty() ||
         (protocol.find(cricket::kMediaProtocolRtpPrefix) != std::string::npos);
}

static bool IsRtpContent(SessionDescription* sdesc,
                         const std::string& content_name) {
  bool is_rtp = false;
  ContentInfo* content = sdesc->GetContentByName(content_name);
  if (IsMediaContent(content)) {
    MediaContentDescription* media_desc =
        static_cast<MediaContentDescription*>(content->description);
    if (!media_desc) {
      return false;
    }
    is_rtp = IsRtpProtocol(media_desc->protocol());
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
  for (ContentNames::const_iterator it = content_names.begin();
       it != content_names.end(); ++it) {
    if (!IsRtpContent(sdesc, *it)) {
      continue;
    }
    // The common cryptos are needed if any of the content does not have DTLS
    // enabled.
    if (!sdesc->GetTransportInfoByName(*it)->description.secure()) {
      common_cryptos_needed = true;
    }
    if (it == content_names.begin()) {
      // Initial the common_cryptos with the first content in the bundle group.
      if (!GetCryptosByName(sdesc, *it, &common_cryptos)) {
        return false;
      }
      if (common_cryptos.empty()) {
        // If there's no crypto params, we should just return.
        return true;
      }
    } else {
      CryptoParamsVec cryptos;
      if (!GetCryptosByName(sdesc, *it, &cryptos)) {
        return false;
      }
      PruneCryptos(cryptos, &common_cryptos);
    }
  }

  if (common_cryptos.empty() && common_cryptos_needed) {
    return false;
  }

  // Update to use the common cryptos.
  for (ContentNames::const_iterator it = content_names.begin();
       it != content_names.end(); ++it) {
    if (!IsRtpContent(sdesc, *it)) {
      continue;
    }
    ContentInfo* content = sdesc->GetContentByName(*it);
    if (IsMediaContent(content)) {
      MediaContentDescription* media_desc =
          static_cast<MediaContentDescription*>(content->description);
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

static TransportOptions GetTransportOptions(const MediaSessionOptions& options,
                                            const std::string& content_name) {
  TransportOptions transport_options;
  auto it = options.transport_options.find(content_name);
  if (it != options.transport_options.end()) {
    transport_options = it->second;
  }
  transport_options.enable_ice_renomination = options.enable_ice_renomination;
  return transport_options;
}

// Create a media content to be offered in a session-initiate,
// according to the given options.rtcp_mux, options.is_muc,
// options.streams, codecs, secure_transport, crypto, and streams.  If we don't
// currently have crypto (in current_cryptos) and it is enabled (in
// secure_policy), crypto is created (according to crypto_suites).  If
// add_legacy_stream is true, and current_streams is empty, a legacy
// stream is created.  The created content is added to the offer.
template <class C>
static bool CreateMediaContentOffer(
    const MediaSessionOptions& options,
    const std::vector<C>& codecs,
    const SecurePolicy& secure_policy,
    const CryptoParamsVec* current_cryptos,
    const std::vector<std::string>& crypto_suites,
    const RtpHeaderExtensions& rtp_extensions,
    bool add_legacy_stream,
    StreamParamsVec* current_streams,
    MediaContentDescriptionImpl<C>* offer) {
  offer->AddCodecs(codecs);

  offer->set_rtcp_mux(options.rtcp_mux_enabled);
  if (offer->type() == cricket::MEDIA_TYPE_VIDEO) {
    offer->set_rtcp_reduced_size(true);
  }
  offer->set_multistream(options.is_muc);
  offer->set_rtp_header_extensions(rtp_extensions);

  if (!AddStreamParams(offer->type(), options, current_streams, offer,
                       add_legacy_stream)) {
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
  for (const C& potential_match : codecs2) {
    if (potential_match.Matches(codec_to_match)) {
      if (IsRtxCodec(codec_to_match)) {
        int apt_value_1 = 0;
        int apt_value_2 = 0;
        if (!codec_to_match.GetParam(kCodecParamAssociatedPayloadType,
                                     &apt_value_1) ||
            !potential_match.GetParam(kCodecParamAssociatedPayloadType,
                                      &apt_value_2)) {
          LOG(LS_WARNING) << "RTX missing associated payload type.";
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

// Adds all codecs from |reference_codecs| to |offered_codecs| that dont'
// already exist in |offered_codecs| and ensure the payload types don't
// collide.
template <class C>
static void FindCodecsToOffer(
    const std::vector<C>& reference_codecs,
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

      std::string associated_pt_str;
      if (!rtx_codec.GetParam(kCodecParamAssociatedPayloadType,
                              &associated_pt_str)) {
        LOG(LS_WARNING) << "RTX codec " << rtx_codec.name
                        << " is missing an associated payload type.";
        continue;
      }

      int associated_pt;
      if (!rtc::FromString(associated_pt_str, &associated_pt)) {
        LOG(LS_WARNING) << "Couldn't convert payload type " << associated_pt_str
                        << " of RTX codec " << rtx_codec.name
                        << " to an integer.";
        continue;
      }

      // Find the associated reference codec for the reference RTX codec.
      const C* associated_codec =
          FindCodecById(reference_codecs, associated_pt);
      if (!associated_codec) {
        LOG(LS_WARNING) << "Couldn't find associated codec with payload type "
                        << associated_pt << " for RTX codec " << rtx_codec.name
                        << ".";
        continue;
      }

      // Find a codec in the offered list that matches the reference codec.
      // Its payload type may be different than the reference codec.
      C matching_codec;
      if (!FindMatchingCodec<C>(reference_codecs, *offered_codecs,
                                *associated_codec, &matching_codec)) {
        LOG(LS_WARNING) << "Couldn't find matching " << associated_codec->name
                        << " codec.";
        continue;
      }

      rtx_codec.params[kCodecParamAssociatedPayloadType] =
          rtc::ToString(matching_codec.id);
      used_pltypes->FindAndSetIdUsed(&rtx_codec);
      offered_codecs->push_back(rtx_codec);
    }
  }
}

static bool FindByUri(const RtpHeaderExtensions& extensions,
                      const webrtc::RtpExtension& ext_to_match,
                      webrtc::RtpExtension* found_extension) {
  for (RtpHeaderExtensions::const_iterator it = extensions.begin();
       it  != extensions.end(); ++it) {
    // We assume that all URIs are given in a canonical format.
    if (it->uri == ext_to_match.uri) {
      if (found_extension != NULL) {
        *found_extension = *it;
      }
      return true;
    }
  }
  return false;
}

// Iterates through |offered_extensions|, adding each one to |all_extensions|
// and |used_ids|, and resolving ID conflicts. If an offered extension has the
// same URI as one in |all_extensions|, it will re-use the same ID and won't be
// treated as a conflict.
static void FindAndSetRtpHdrExtUsed(RtpHeaderExtensions* offered_extensions,
                                    RtpHeaderExtensions* all_extensions,
                                    UsedRtpHeaderExtensionIds* used_ids) {
  for (auto& extension : *offered_extensions) {
    webrtc::RtpExtension existing;
    if (FindByUri(*all_extensions, extension, &existing)) {
      extension.id = existing.id;
    } else {
      used_ids->FindAndSetIdUsed(&extension);
      all_extensions->push_back(extension);
    }
  }
}

// Adds |reference_extensions| to |offered_extensions|, while updating
// |all_extensions| and |used_ids|.
static void FindRtpHdrExtsToOffer(
    const RtpHeaderExtensions& reference_extensions,
    RtpHeaderExtensions* offered_extensions,
    RtpHeaderExtensions* all_extensions,
    UsedRtpHeaderExtensionIds* used_ids) {
  for (auto reference_extension : reference_extensions) {
    if (!FindByUri(*offered_extensions, reference_extension, NULL)) {
      webrtc::RtpExtension existing;
      if (FindByUri(*all_extensions, reference_extension, &existing)) {
        offered_extensions->push_back(existing);
      } else {
        used_ids->FindAndSetIdUsed(&reference_extension);
        all_extensions->push_back(reference_extension);
        offered_extensions->push_back(reference_extension);
      }
    }
  }
}

static void NegotiateRtpHeaderExtensions(
    const RtpHeaderExtensions& local_extensions,
    const RtpHeaderExtensions& offered_extensions,
    RtpHeaderExtensions* negotiated_extenstions) {
  RtpHeaderExtensions::const_iterator ours;
  for (ours = local_extensions.begin();
       ours != local_extensions.end(); ++ours) {
    webrtc::RtpExtension theirs;
    if (FindByUri(offered_extensions, *ours, &theirs)) {
      // We respond with their RTP header extension id.
      negotiated_extenstions->push_back(theirs);
    }
  }
}

static void StripCNCodecs(AudioCodecs* audio_codecs) {
  AudioCodecs::iterator iter = audio_codecs->begin();
  while (iter != audio_codecs->end()) {
    if (STR_CASE_CMP(iter->name.c_str(), kComfortNoiseCodecName) == 0) {
      iter = audio_codecs->erase(iter);
    } else {
      ++iter;
    }
  }
}

// Create a media content to be answered in a session-accept,
// according to the given options.rtcp_mux, options.streams, codecs,
// crypto, and streams.  If we don't currently have crypto (in
// current_cryptos) and it is enabled (in secure_policy), crypto is
// created (according to crypto_suites).  If add_legacy_stream is
// true, and current_streams is empty, a legacy stream is created.
// The codecs, rtcp_mux, and crypto are all negotiated with the offer
// from the incoming session-initiate.  If the negotiation fails, this
// method returns false.  The created content is added to the offer.
template <class C>
static bool CreateMediaContentAnswer(
    const MediaContentDescriptionImpl<C>* offer,
    const MediaSessionOptions& options,
    const std::vector<C>& local_codecs,
    const SecurePolicy& sdes_policy,
    const CryptoParamsVec* current_cryptos,
    const RtpHeaderExtensions& local_rtp_extenstions,
    StreamParamsVec* current_streams,
    bool add_legacy_stream,
    bool bundle_enabled,
    MediaContentDescriptionImpl<C>* answer) {
  std::vector<C> negotiated_codecs;
  NegotiateCodecs(local_codecs, offer->codecs(), &negotiated_codecs);
  answer->AddCodecs(negotiated_codecs);
  answer->set_protocol(offer->protocol());
  RtpHeaderExtensions negotiated_rtp_extensions;
  NegotiateRtpHeaderExtensions(local_rtp_extenstions,
                               offer->rtp_header_extensions(),
                               &negotiated_rtp_extensions);
  answer->set_rtp_header_extensions(negotiated_rtp_extensions);

  answer->set_rtcp_mux(options.rtcp_mux_enabled && offer->rtcp_mux());
  if (answer->type() == cricket::MEDIA_TYPE_VIDEO) {
    answer->set_rtcp_reduced_size(offer->rtcp_reduced_size());
  }

  if (sdes_policy != SEC_DISABLED) {
    CryptoParams crypto;
    if (SelectCrypto(offer, bundle_enabled, options.crypto_options, &crypto)) {
      if (current_cryptos) {
        FindMatchingCrypto(*current_cryptos, crypto, &crypto);
      }
      answer->AddCrypto(crypto);
    }
  }

  if (answer->cryptos().empty() && sdes_policy == SEC_REQUIRED) {
    return false;
  }

  if (!AddStreamParams(answer->type(), options, current_streams, answer,
                       add_legacy_stream)) {
    return false;  // Something went seriously wrong.
  }

  // Make sure the answer media content direction is per default set as
  // described in RFC3264 section 6.1.
  const bool is_data = !IsRtpProtocol(answer->protocol());
  const bool has_send_streams = !answer->streams().empty();
  const bool wants_send = has_send_streams || is_data;
  const bool recv_audio =
      answer->type() == cricket::MEDIA_TYPE_AUDIO && options.recv_audio;
  const bool recv_video =
      answer->type() == cricket::MEDIA_TYPE_VIDEO && options.recv_video;
  const bool recv_data =
      answer->type() == cricket::MEDIA_TYPE_DATA;
  const bool wants_receive = recv_audio || recv_video || recv_data;

  auto offer_rtd =
      RtpTransceiverDirection::FromMediaContentDirection(offer->direction());
  auto wants_rtd = RtpTransceiverDirection(wants_send, wants_receive);
  answer->set_direction(NegotiateRtpTransceiverDirection(offer_rtd, wants_rtd)
                        .ToMediaContentDirection());
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
static bool IsDtlsActive(
    const std::string& content_name,
    const SessionDescription* current_description) {
  if (!current_description)
    return false;

  const ContentInfo* content =
      current_description->GetContentByName(content_name);
  if (!content)
    return false;

  const TransportDescription* current_tdesc =
      GetTransportDescription(content_name, current_description);
  if (!current_tdesc)
    return false;

  return current_tdesc->secure();
}

std::string MediaContentDirectionToString(MediaContentDirection direction) {
  std::string dir_str;
  switch (direction) {
    case MD_INACTIVE:
      dir_str = "inactive";
      break;
    case MD_SENDONLY:
      dir_str = "sendonly";
      break;
    case MD_RECVONLY:
      dir_str = "recvonly";
      break;
    case MD_SENDRECV:
      dir_str = "sendrecv";
      break;
    default:
      RTC_NOTREACHED();
      break;
  }

  return dir_str;
}

void MediaSessionOptions::AddSendStream(MediaType type,
                                    const std::string& id,
                                    const std::string& sync_label) {
  AddSendStreamInternal(type, id, sync_label, 1);
}

void MediaSessionOptions::AddSendVideoStream(
    const std::string& id,
    const std::string& sync_label,
    int num_sim_layers) {
  AddSendStreamInternal(MEDIA_TYPE_VIDEO, id, sync_label, num_sim_layers);
}

void MediaSessionOptions::AddSendStreamInternal(
    MediaType type,
    const std::string& id,
    const std::string& sync_label,
    int num_sim_layers) {
  streams.push_back(Stream(type, id, sync_label, num_sim_layers));

  // If we haven't already set the data_channel_type, and we add a
  // stream, we assume it's an RTP data stream.
  if (type == MEDIA_TYPE_DATA && data_channel_type == DCT_NONE)
    data_channel_type = DCT_RTP;
}

void MediaSessionOptions::RemoveSendStream(MediaType type,
                                       const std::string& id) {
  Streams::iterator stream_it = streams.begin();
  for (; stream_it != streams.end(); ++stream_it) {
    if (stream_it->type == type && stream_it->id == id) {
      streams.erase(stream_it);
      return;
    }
  }
  RTC_NOTREACHED();
}

bool MediaSessionOptions::HasSendMediaStream(MediaType type) const {
  Streams::const_iterator stream_it = streams.begin();
  for (; stream_it != streams.end(); ++stream_it) {
    if (stream_it->type == type) {
      return true;
    }
  }
  return false;
}

MediaSessionDescriptionFactory::MediaSessionDescriptionFactory(
    const TransportDescriptionFactory* transport_desc_factory)
    : secure_(SEC_DISABLED),
      add_legacy_(true),
      transport_desc_factory_(transport_desc_factory) {
}

MediaSessionDescriptionFactory::MediaSessionDescriptionFactory(
    ChannelManager* channel_manager,
    const TransportDescriptionFactory* transport_desc_factory)
    : secure_(SEC_DISABLED),
      add_legacy_(true),
      transport_desc_factory_(transport_desc_factory) {
  channel_manager->GetSupportedAudioSendCodecs(&audio_send_codecs_);
  channel_manager->GetSupportedAudioReceiveCodecs(&audio_recv_codecs_);
  channel_manager->GetSupportedAudioRtpHeaderExtensions(&audio_rtp_extensions_);
  channel_manager->GetSupportedVideoCodecs(&video_codecs_);
  channel_manager->GetSupportedVideoRtpHeaderExtensions(&video_rtp_extensions_);
  channel_manager->GetSupportedDataCodecs(&data_codecs_);
  NegotiateCodecs(audio_recv_codecs_, audio_send_codecs_,
                  &audio_sendrecv_codecs_);
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
    const AudioCodecs& send_codecs, const AudioCodecs& recv_codecs) {
  audio_send_codecs_ = send_codecs;
  audio_recv_codecs_ = recv_codecs;
  audio_sendrecv_codecs_.clear();
  // Use NegotiateCodecs to merge our codec lists, since the operation is
  // essentially the same. Put send_codecs as the offered_codecs, which is the
  // order we'd like to follow. The reasoning is that encoding is usually more
  // expensive than decoding, and prioritizing a codec in the send list probably
  // means it's a codec we can handle efficiently.
  NegotiateCodecs(recv_codecs, send_codecs, &audio_sendrecv_codecs_);
}

SessionDescription* MediaSessionDescriptionFactory::CreateOffer(
    const MediaSessionOptions& options,
    const SessionDescription* current_description) const {
  std::unique_ptr<SessionDescription> offer(new SessionDescription());

  StreamParamsVec current_streams;
  GetCurrentStreamParams(current_description, &current_streams);

  const bool wants_send =
      options.HasSendMediaStream(MEDIA_TYPE_AUDIO) || add_legacy_;
  const AudioCodecs& supported_audio_codecs =
      GetAudioCodecsForOffer({wants_send, options.recv_audio});

  AudioCodecs audio_codecs;
  VideoCodecs video_codecs;
  DataCodecs data_codecs;
  GetCodecsToOffer(current_description, supported_audio_codecs,
                   video_codecs_, data_codecs_,
                   &audio_codecs, &video_codecs, &data_codecs);

  if (!options.vad_enabled) {
    // If application doesn't want CN codecs in offer.
    StripCNCodecs(&audio_codecs);
  }

  RtpHeaderExtensions audio_rtp_extensions;
  RtpHeaderExtensions video_rtp_extensions;
  GetRtpHdrExtsToOffer(current_description, &audio_rtp_extensions,
                       &video_rtp_extensions);

  bool audio_added = false;
  bool video_added = false;
  bool data_added = false;

  // Iterate through the contents of |current_description| to maintain the order
  // of the m-lines in the new offer.
  if (current_description) {
    ContentInfos::const_iterator it = current_description->contents().begin();
    for (; it != current_description->contents().end(); ++it) {
      if (IsMediaContentOfType(&*it, MEDIA_TYPE_AUDIO)) {
        if (!AddAudioContentForOffer(options, current_description,
                                     audio_rtp_extensions, audio_codecs,
                                     &current_streams, offer.get())) {
          return NULL;
        }
        audio_added = true;
      } else if (IsMediaContentOfType(&*it, MEDIA_TYPE_VIDEO)) {
        if (!AddVideoContentForOffer(options, current_description,
                                     video_rtp_extensions, video_codecs,
                                     &current_streams, offer.get())) {
          return NULL;
        }
        video_added = true;
      } else if (IsMediaContentOfType(&*it, MEDIA_TYPE_DATA)) {
        MediaSessionOptions options_copy(options);
        if (IsSctp(static_cast<const MediaContentDescription*>(it->description)
                       ->protocol())) {
          options_copy.data_channel_type = DCT_SCTP;
        }
        if (!AddDataContentForOffer(options_copy, current_description,
                                    &data_codecs, &current_streams,
                                    offer.get())) {
          return NULL;
        }
        data_added = true;
      } else {
        RTC_NOTREACHED();
      }
    }
  }

  // Append contents that are not in |current_description|.
  if (!audio_added && options.has_audio() &&
      !AddAudioContentForOffer(options, current_description,
                               audio_rtp_extensions, audio_codecs,
                               &current_streams, offer.get())) {
    return NULL;
  }
  if (!video_added && options.has_video() &&
      !AddVideoContentForOffer(options, current_description,
                               video_rtp_extensions, video_codecs,
                               &current_streams, offer.get())) {
    return NULL;
  }
  if (!data_added && options.has_data() &&
      !AddDataContentForOffer(options, current_description, &data_codecs,
                              &current_streams, offer.get())) {
    return NULL;
  }

  // Bundle the contents together, if we've been asked to do so, and update any
  // parameters that need to be tweaked for BUNDLE.
  if (options.bundle_enabled) {
    ContentGroup offer_bundle(GROUP_TYPE_BUNDLE);
    for (ContentInfos::const_iterator content = offer->contents().begin();
       content != offer->contents().end(); ++content) {
      offer_bundle.AddContentName(content->name);
    }
    offer->AddGroup(offer_bundle);
    if (!UpdateTransportInfoForBundle(offer_bundle, offer.get())) {
      LOG(LS_ERROR) << "CreateOffer failed to UpdateTransportInfoForBundle.";
      return NULL;
    }
    if (!UpdateCryptoParamsForBundle(offer_bundle, offer.get())) {
      LOG(LS_ERROR) << "CreateOffer failed to UpdateCryptoParamsForBundle.";
      return NULL;
    }
  }

  return offer.release();
}

SessionDescription* MediaSessionDescriptionFactory::CreateAnswer(
    const SessionDescription* offer, const MediaSessionOptions& options,
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

  ContentInfos::const_iterator it = offer->contents().begin();
  for (; it != offer->contents().end(); ++it) {
    if (IsMediaContentOfType(&*it, MEDIA_TYPE_AUDIO)) {
      if (!AddAudioContentForAnswer(offer, options, current_description,
                                    bundle_transport.get(), &current_streams,
                                    answer.get())) {
        return NULL;
      }
    } else if (IsMediaContentOfType(&*it, MEDIA_TYPE_VIDEO)) {
      if (!AddVideoContentForAnswer(offer, options, current_description,
                                    bundle_transport.get(), &current_streams,
                                    answer.get())) {
        return NULL;
      }
    } else {
      RTC_DCHECK(IsMediaContentOfType(&*it, MEDIA_TYPE_DATA));
      if (!AddDataContentForAnswer(offer, options, current_description,
                                   bundle_transport.get(), &current_streams,
                                   answer.get())) {
        return NULL;
      }
    }
    // See if we can add the newly generated m= section to the BUNDLE group in
    // the answer.
    ContentInfo& added = answer->contents().back();
    if (!added.rejected && options.bundle_enabled && offer_bundle &&
        offer_bundle->HasContentName(added.name)) {
      answer_bundle.AddContentName(added.name);
      bundle_transport.reset(
          new TransportInfo(*answer->GetTransportInfoByName(added.name)));
    }
  }

  // Only put BUNDLE group in answer if nonempty.
  if (answer_bundle.FirstContentName()) {
    answer->AddGroup(answer_bundle);

    // Share the same ICE credentials and crypto params across all contents,
    // as BUNDLE requires.
    if (!UpdateTransportInfoForBundle(answer_bundle, answer.get())) {
      LOG(LS_ERROR) << "CreateAnswer failed to UpdateTransportInfoForBundle.";
      return NULL;
    }

    if (!UpdateCryptoParamsForBundle(answer_bundle, answer.get())) {
      LOG(LS_ERROR) << "CreateAnswer failed to UpdateCryptoParamsForBundle.";
      return NULL;
    }
  }

  return answer.release();
}

const AudioCodecs& MediaSessionDescriptionFactory::GetAudioCodecsForOffer(
    const RtpTransceiverDirection& direction) const {
  // If stream is inactive - generate list as if sendrecv.
  if (direction.send == direction.recv) {
    return audio_sendrecv_codecs_;
  } else if (direction.send) {
    return audio_send_codecs_;
  } else {
    return audio_recv_codecs_;
  }
}

const AudioCodecs& MediaSessionDescriptionFactory::GetAudioCodecsForAnswer(
    const RtpTransceiverDirection& offer,
    const RtpTransceiverDirection& answer) const {
  // For inactive and sendrecv answers, generate lists as if we were to accept
  // the offer's direction. See RFC 3264 Section 6.1.
  if (answer.send == answer.recv) {
    if (offer.send == offer.recv) {
      return audio_sendrecv_codecs_;
    } else if (offer.send) {
      return audio_recv_codecs_;
    } else {
      return audio_send_codecs_;
    }
  } else if (answer.send) {
    return audio_send_codecs_;
  } else {
    return audio_recv_codecs_;
  }
}

void MediaSessionDescriptionFactory::GetCodecsToOffer(
    const SessionDescription* current_description,
    const AudioCodecs& supported_audio_codecs,
    const VideoCodecs& supported_video_codecs,
    const DataCodecs& supported_data_codecs,
    AudioCodecs* audio_codecs,
    VideoCodecs* video_codecs,
    DataCodecs* data_codecs) const {
  UsedPayloadTypes used_pltypes;
  audio_codecs->clear();
  video_codecs->clear();
  data_codecs->clear();


  // First - get all codecs from the current description if the media type
  // is used.
  // Add them to |used_pltypes| so the payloadtype is not reused if a new media
  // type is added.
  if (current_description) {
    const AudioContentDescription* audio =
        GetFirstAudioContentDescription(current_description);
    if (audio) {
      *audio_codecs = audio->codecs();
      used_pltypes.FindAndSetIdUsed<AudioCodec>(audio_codecs);
    }
    const VideoContentDescription* video =
        GetFirstVideoContentDescription(current_description);
    if (video) {
      *video_codecs = video->codecs();
      used_pltypes.FindAndSetIdUsed<VideoCodec>(video_codecs);
    }
    const DataContentDescription* data =
        GetFirstDataContentDescription(current_description);
    if (data) {
      *data_codecs = data->codecs();
      used_pltypes.FindAndSetIdUsed<DataCodec>(data_codecs);
    }
  }

  // Add our codecs that are not in |current_description|.
  FindCodecsToOffer<AudioCodec>(supported_audio_codecs, audio_codecs,
                                &used_pltypes);
  FindCodecsToOffer<VideoCodec>(supported_video_codecs, video_codecs,
                                &used_pltypes);
  FindCodecsToOffer<DataCodec>(supported_data_codecs, data_codecs,
                               &used_pltypes);
}

void MediaSessionDescriptionFactory::GetRtpHdrExtsToOffer(
    const SessionDescription* current_description,
    RtpHeaderExtensions* audio_extensions,
    RtpHeaderExtensions* video_extensions) const {
  // All header extensions allocated from the same range to avoid potential
  // issues when using BUNDLE.
  UsedRtpHeaderExtensionIds used_ids;
  RtpHeaderExtensions all_extensions;
  audio_extensions->clear();
  video_extensions->clear();

  // First - get all extensions from the current description if the media type
  // is used.
  // Add them to |used_ids| so the local ids are not reused if a new media
  // type is added.
  if (current_description) {
    const AudioContentDescription* audio =
        GetFirstAudioContentDescription(current_description);
    if (audio) {
      *audio_extensions = audio->rtp_header_extensions();
      FindAndSetRtpHdrExtUsed(audio_extensions, &all_extensions, &used_ids);
    }
    const VideoContentDescription* video =
        GetFirstVideoContentDescription(current_description);
    if (video) {
      *video_extensions = video->rtp_header_extensions();
      FindAndSetRtpHdrExtUsed(video_extensions, &all_extensions, &used_ids);
    }
  }

  // Add our default RTP header extensions that are not in
  // |current_description|.
  FindRtpHdrExtsToOffer(audio_rtp_header_extensions(), audio_extensions,
                        &all_extensions, &used_ids);
  FindRtpHdrExtsToOffer(video_rtp_header_extensions(), video_extensions,
                        &all_extensions, &used_ids);
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
  bool ret = (new_tdesc.get() != NULL &&
      offer_desc->AddTransportInfo(TransportInfo(content_name, *new_tdesc)));
  if (!ret) {
    LOG(LS_ERROR)
        << "Failed to AddTransportOffer, content name=" << content_name;
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
  if (!answer_desc->AddTransportInfo(TransportInfo(content_name,
                                                   transport_desc))) {
    LOG(LS_ERROR)
        << "Failed to AddTransportAnswer, content name=" << content_name;
    return false;
  }
  return true;
}

bool MediaSessionDescriptionFactory::AddAudioContentForOffer(
    const MediaSessionOptions& options,
    const SessionDescription* current_description,
    const RtpHeaderExtensions& audio_rtp_extensions,
    const AudioCodecs& audio_codecs,
    StreamParamsVec* current_streams,
    SessionDescription* desc) const {
  const ContentInfo* current_audio_content =
      GetFirstAudioContent(current_description);
  std::string content_name =
      current_audio_content ? current_audio_content->name : CN_AUDIO;

  cricket::SecurePolicy sdes_policy =
      IsDtlsActive(content_name, current_description) ? cricket::SEC_DISABLED
                                                      : secure();

  std::unique_ptr<AudioContentDescription> audio(new AudioContentDescription());
  std::vector<std::string> crypto_suites;
  GetSupportedAudioSdesCryptoSuiteNames(options.crypto_options, &crypto_suites);
  if (!CreateMediaContentOffer(
          options,
          audio_codecs,
          sdes_policy,
          GetCryptos(GetFirstAudioContentDescription(current_description)),
          crypto_suites,
          audio_rtp_extensions,
          add_legacy_,
          current_streams,
          audio.get())) {
    return false;
  }
  audio->set_lang(lang_);

  bool secure_transport = (transport_desc_factory_->secure() != SEC_DISABLED);
  SetMediaProtocol(secure_transport, audio.get());

  auto offer_rtd =
      RtpTransceiverDirection(!audio->streams().empty(), options.recv_audio);
  audio->set_direction(offer_rtd.ToMediaContentDirection());

  desc->AddContent(content_name, NS_JINGLE_RTP, audio.release());
  if (!AddTransportOffer(content_name,
                         GetTransportOptions(options, content_name),
                         current_description, desc)) {
    return false;
  }

  return true;
}

bool MediaSessionDescriptionFactory::AddVideoContentForOffer(
    const MediaSessionOptions& options,
    const SessionDescription* current_description,
    const RtpHeaderExtensions& video_rtp_extensions,
    const VideoCodecs& video_codecs,
    StreamParamsVec* current_streams,
    SessionDescription* desc) const {
  const ContentInfo* current_video_content =
      GetFirstVideoContent(current_description);
  std::string content_name =
      current_video_content ? current_video_content->name : CN_VIDEO;

  cricket::SecurePolicy sdes_policy =
      IsDtlsActive(content_name, current_description) ? cricket::SEC_DISABLED
                                                      : secure();

  std::unique_ptr<VideoContentDescription> video(new VideoContentDescription());
  std::vector<std::string> crypto_suites;
  GetSupportedVideoSdesCryptoSuiteNames(options.crypto_options, &crypto_suites);
  if (!CreateMediaContentOffer(
          options,
          video_codecs,
          sdes_policy,
          GetCryptos(GetFirstVideoContentDescription(current_description)),
          crypto_suites,
          video_rtp_extensions,
          add_legacy_,
          current_streams,
          video.get())) {
    return false;
  }

  video->set_bandwidth(options.video_bandwidth);

  bool secure_transport = (transport_desc_factory_->secure() != SEC_DISABLED);
  SetMediaProtocol(secure_transport, video.get());

  if (!video->streams().empty()) {
    if (options.recv_video) {
      video->set_direction(MD_SENDRECV);
    } else {
      video->set_direction(MD_SENDONLY);
    }
  } else {
    if (options.recv_video) {
      video->set_direction(MD_RECVONLY);
    } else {
      video->set_direction(MD_INACTIVE);
    }
  }

  desc->AddContent(content_name, NS_JINGLE_RTP, video.release());
  if (!AddTransportOffer(content_name,
                         GetTransportOptions(options, content_name),
                         current_description, desc)) {
    return false;
  }

  return true;
}

bool MediaSessionDescriptionFactory::AddDataContentForOffer(
    const MediaSessionOptions& options,
    const SessionDescription* current_description,
    DataCodecs* data_codecs,
    StreamParamsVec* current_streams,
    SessionDescription* desc) const {
  bool secure_transport = (transport_desc_factory_->secure() != SEC_DISABLED);

  std::unique_ptr<DataContentDescription> data(new DataContentDescription());
  bool is_sctp = (options.data_channel_type == DCT_SCTP);

  FilterDataCodecs(data_codecs, is_sctp);

  const ContentInfo* current_data_content =
      GetFirstDataContent(current_description);
  std::string content_name =
      current_data_content ? current_data_content->name : CN_DATA;

  cricket::SecurePolicy sdes_policy =
      IsDtlsActive(content_name, current_description) ? cricket::SEC_DISABLED
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
    data->set_protocol(
        secure_transport ? kMediaProtocolDtlsSctp : kMediaProtocolSctp);
  } else {
    GetSupportedDataSdesCryptoSuiteNames(options.crypto_options,
                                         &crypto_suites);
  }

  if (!CreateMediaContentOffer(
          options,
          *data_codecs,
          sdes_policy,
          GetCryptos(GetFirstDataContentDescription(current_description)),
          crypto_suites,
          RtpHeaderExtensions(),
          add_legacy_,
          current_streams,
          data.get())) {
    return false;
  }

  if (is_sctp) {
    desc->AddContent(content_name, NS_JINGLE_DRAFT_SCTP, data.release());
  } else {
    data->set_bandwidth(options.data_bandwidth);
    SetMediaProtocol(secure_transport, data.get());
    desc->AddContent(content_name, NS_JINGLE_RTP, data.release());
  }
  if (!AddTransportOffer(content_name,
                         GetTransportOptions(options, content_name),
                         current_description, desc)) {
    return false;
  }
  return true;
}

bool MediaSessionDescriptionFactory::AddAudioContentForAnswer(
    const SessionDescription* offer,
    const MediaSessionOptions& options,
    const SessionDescription* current_description,
    const TransportInfo* bundle_transport,
    StreamParamsVec* current_streams,
    SessionDescription* answer) const {
  const ContentInfo* audio_content = GetFirstAudioContent(offer);
  const AudioContentDescription* offer_audio =
      static_cast<const AudioContentDescription*>(audio_content->description);

  std::unique_ptr<TransportDescription> audio_transport(
      CreateTransportAnswer(audio_content->name, offer,
                            GetTransportOptions(options, audio_content->name),
                            current_description, bundle_transport != nullptr));
  if (!audio_transport) {
    return false;
  }

  // Pick codecs based on the requested communications direction in the offer.
  const bool wants_send =
      options.HasSendMediaStream(MEDIA_TYPE_AUDIO) || add_legacy_;
  auto wants_rtd = RtpTransceiverDirection(wants_send, options.recv_audio);
  auto offer_rtd =
      RtpTransceiverDirection::FromMediaContentDirection(
          offer_audio->direction());
  auto answer_rtd = NegotiateRtpTransceiverDirection(offer_rtd, wants_rtd);
  AudioCodecs audio_codecs = GetAudioCodecsForAnswer(offer_rtd, answer_rtd);
  if (!options.vad_enabled) {
    StripCNCodecs(&audio_codecs);
  }

  bool bundle_enabled =
      offer->HasGroup(GROUP_TYPE_BUNDLE) && options.bundle_enabled;
  std::unique_ptr<AudioContentDescription> audio_answer(
      new AudioContentDescription());
  // Do not require or create SDES cryptos if DTLS is used.
  cricket::SecurePolicy sdes_policy =
      audio_transport->secure() ? cricket::SEC_DISABLED : secure();
  if (!CreateMediaContentAnswer(
          offer_audio,
          options,
          audio_codecs,
          sdes_policy,
          GetCryptos(GetFirstAudioContentDescription(current_description)),
          audio_rtp_extensions_,
          current_streams,
          add_legacy_,
          bundle_enabled,
          audio_answer.get())) {
    return false;  // Fails the session setup.
  }

  bool secure = bundle_transport ? bundle_transport->description.secure()
                                 : audio_transport->secure();
  bool rejected = !options.has_audio() || audio_content->rejected ||
                  !IsMediaProtocolSupported(MEDIA_TYPE_AUDIO,
                                            audio_answer->protocol(), secure);
  if (!rejected) {
    AddTransportAnswer(audio_content->name, *(audio_transport.get()), answer);
  } else {
    // RFC 3264
    // The answer MUST contain the same number of m-lines as the offer.
    LOG(LS_INFO) << "Audio is not supported in the answer.";
  }

  answer->AddContent(audio_content->name, audio_content->type, rejected,
                     audio_answer.release());
  return true;
}

bool MediaSessionDescriptionFactory::AddVideoContentForAnswer(
    const SessionDescription* offer,
    const MediaSessionOptions& options,
    const SessionDescription* current_description,
    const TransportInfo* bundle_transport,
    StreamParamsVec* current_streams,
    SessionDescription* answer) const {
  const ContentInfo* video_content = GetFirstVideoContent(offer);
  std::unique_ptr<TransportDescription> video_transport(
      CreateTransportAnswer(video_content->name, offer,
                            GetTransportOptions(options, video_content->name),
                            current_description, bundle_transport != nullptr));
  if (!video_transport) {
    return false;
  }

  std::unique_ptr<VideoContentDescription> video_answer(
      new VideoContentDescription());
  // Do not require or create SDES cryptos if DTLS is used.
  cricket::SecurePolicy sdes_policy =
      video_transport->secure() ? cricket::SEC_DISABLED : secure();
  bool bundle_enabled =
      offer->HasGroup(GROUP_TYPE_BUNDLE) && options.bundle_enabled;
  if (!CreateMediaContentAnswer(
          static_cast<const VideoContentDescription*>(
              video_content->description),
          options,
          video_codecs_,
          sdes_policy,
          GetCryptos(GetFirstVideoContentDescription(current_description)),
          video_rtp_extensions_,
          current_streams,
          add_legacy_,
          bundle_enabled,
          video_answer.get())) {
    return false;
  }
  bool secure = bundle_transport ? bundle_transport->description.secure()
                                 : video_transport->secure();
  bool rejected = !options.has_video() || video_content->rejected ||
                  !IsMediaProtocolSupported(MEDIA_TYPE_VIDEO,
                                            video_answer->protocol(), secure);
  if (!rejected) {
    if (!AddTransportAnswer(video_content->name, *(video_transport.get()),
                            answer)) {
      return false;
    }
    video_answer->set_bandwidth(options.video_bandwidth);
  } else {
    // RFC 3264
    // The answer MUST contain the same number of m-lines as the offer.
    LOG(LS_INFO) << "Video is not supported in the answer.";
  }
  answer->AddContent(video_content->name, video_content->type, rejected,
                     video_answer.release());
  return true;
}

bool MediaSessionDescriptionFactory::AddDataContentForAnswer(
    const SessionDescription* offer,
    const MediaSessionOptions& options,
    const SessionDescription* current_description,
    const TransportInfo* bundle_transport,
    StreamParamsVec* current_streams,
    SessionDescription* answer) const {
  const ContentInfo* data_content = GetFirstDataContent(offer);
  std::unique_ptr<TransportDescription> data_transport(
      CreateTransportAnswer(data_content->name, offer,
                            GetTransportOptions(options, data_content->name),
                            current_description, bundle_transport != nullptr));
  if (!data_transport) {
    return false;
  }
  bool is_sctp = (options.data_channel_type == DCT_SCTP);
  std::vector<DataCodec> data_codecs(data_codecs_);
  FilterDataCodecs(&data_codecs, is_sctp);

  std::unique_ptr<DataContentDescription> data_answer(
      new DataContentDescription());
  // Do not require or create SDES cryptos if DTLS is used.
  cricket::SecurePolicy sdes_policy =
      data_transport->secure() ? cricket::SEC_DISABLED : secure();
  bool bundle_enabled =
      offer->HasGroup(GROUP_TYPE_BUNDLE) && options.bundle_enabled;
  if (!CreateMediaContentAnswer(
          static_cast<const DataContentDescription*>(
              data_content->description),
          options,
          data_codecs_,
          sdes_policy,
          GetCryptos(GetFirstDataContentDescription(current_description)),
          RtpHeaderExtensions(),
          current_streams,
          add_legacy_,
          bundle_enabled,
          data_answer.get())) {
    return false;  // Fails the session setup.
  }

  // Respond with sctpmap if the offer uses sctpmap.
  const DataContentDescription* offer_data_description =
      static_cast<const DataContentDescription*>(data_content->description);
  bool offer_uses_sctpmap = offer_data_description->use_sctpmap();
  data_answer->set_use_sctpmap(offer_uses_sctpmap);

  bool secure = bundle_transport ? bundle_transport->description.secure()
                                 : data_transport->secure();

  bool rejected = !options.has_data() || data_content->rejected ||
                  !IsMediaProtocolSupported(MEDIA_TYPE_DATA,
                                            data_answer->protocol(), secure);
  if (!rejected) {
    data_answer->set_bandwidth(options.data_bandwidth);
    if (!AddTransportAnswer(data_content->name, *(data_transport.get()),
                            answer)) {
      return false;
    }
  } else {
    // RFC 3264
    // The answer MUST contain the same number of m-lines as the offer.
    LOG(LS_INFO) << "Data is not supported in the answer.";
  }
  answer->AddContent(data_content->name, data_content->type, rejected,
                     data_answer.release());
  return true;
}

bool IsMediaContent(const ContentInfo* content) {
  return (content &&
          (content->type == NS_JINGLE_RTP ||
           content->type == NS_JINGLE_DRAFT_SCTP));
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

static const ContentInfo* GetFirstMediaContent(const SessionDescription* sdesc,
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
    const SessionDescription* sdesc, MediaType media_type) {
  const ContentInfo* content = GetFirstMediaContent(sdesc, media_type);
  const ContentDescription* description = content ? content->description : NULL;
  return static_cast<const MediaContentDescription*>(description);
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

ContentInfo* GetFirstMediaContent(ContentInfos& contents,
                                  MediaType media_type) {
  for (ContentInfo& content : contents) {
    if (IsMediaContentOfType(&content, media_type)) {
      return &content;
    }
  }
  return nullptr;
}

ContentInfo* GetFirstAudioContent(ContentInfos& contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_AUDIO);
}

ContentInfo* GetFirstVideoContent(ContentInfos& contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_VIDEO);
}

ContentInfo* GetFirstDataContent(ContentInfos& contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_DATA);
}

static ContentInfo* GetFirstMediaContent(SessionDescription* sdesc,
                                         MediaType media_type) {
  if (sdesc == nullptr) {
    return nullptr;
  }

  return GetFirstMediaContent(sdesc->contents(), media_type);
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
  ContentDescription* description = content ? content->description : NULL;
  return static_cast<MediaContentDescription*>(description);
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
