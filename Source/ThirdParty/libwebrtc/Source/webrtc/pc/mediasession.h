/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Types and classes used in media session descriptions.

#ifndef PC_MEDIASESSION_H_
#define PC_MEDIASESSION_H_

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "api/cryptoparams.h"
#include "api/mediatypes.h"
#include "api/rtptransceiverinterface.h"
#include "media/base/codec.h"
#include "media/base/mediachannel.h"
#include "media/base/mediaconstants.h"
#include "media/base/mediaengine.h"  // For DataChannelType
#include "media/base/streamparams.h"
#include "p2p/base/jseptransport.h"
#include "p2p/base/sessiondescription.h"
#include "p2p/base/transportdescriptionfactory.h"

namespace cricket {

class ChannelManager;
typedef std::vector<AudioCodec> AudioCodecs;
typedef std::vector<VideoCodec> VideoCodecs;
typedef std::vector<DataCodec> DataCodecs;
typedef std::vector<CryptoParams> CryptoParamsVec;
typedef std::vector<webrtc::RtpExtension> RtpHeaderExtensions;

enum CryptoType {
  CT_NONE,
  CT_SDES,
  CT_DTLS
};

// RTC4585 RTP/AVPF
extern const char kMediaProtocolAvpf[];
// RFC5124 RTP/SAVPF
extern const char kMediaProtocolSavpf[];

extern const char kMediaProtocolDtlsSavpf[];

extern const char kMediaProtocolRtpPrefix[];

extern const char kMediaProtocolSctp[];
extern const char kMediaProtocolDtlsSctp[];
extern const char kMediaProtocolUdpDtlsSctp[];
extern const char kMediaProtocolTcpDtlsSctp[];

// Options to control how session descriptions are generated.
const int kAutoBandwidth = -1;

// Default RTCP CNAME for unit tests.
const char kDefaultRtcpCname[] = "DefaultRtcpCname";

// Options for an RtpSender contained with an media description/"m=" section.
struct SenderOptions {
  std::string track_id;
  // TODO(steveanton): As part of work towards Unified Plan, this has been
  // changed to be a vector. But for now this can only have exactly one.
  std::vector<std::string> stream_ids;
  int num_sim_layers;
};

// Options for an individual media description/"m=" section.
struct MediaDescriptionOptions {
  MediaDescriptionOptions(MediaType type,
                          const std::string& mid,
                          webrtc::RtpTransceiverDirection direction,
                          bool stopped)
      : type(type), mid(mid), direction(direction), stopped(stopped) {}

  // TODO(deadbeef): When we don't support Plan B, there will only be one
  // sender per media description and this can be simplified.
  void AddAudioSender(const std::string& track_id,
                      const std::vector<std::string>& stream_ids);
  void AddVideoSender(const std::string& track_id,
                      const std::vector<std::string>& stream_ids,
                      int num_sim_layers);

  // Internally just uses sender_options.
  void AddRtpDataChannel(const std::string& track_id,
                         const std::string& stream_id);

  MediaType type;
  std::string mid;
  webrtc::RtpTransceiverDirection direction;
  bool stopped;
  TransportOptions transport_options;
  // Note: There's no equivalent "RtpReceiverOptions" because only send
  // stream information goes in the local descriptions.
  std::vector<SenderOptions> sender_options;

 private:
  // Doesn't DCHECK on |type|.
  void AddSenderInternal(const std::string& track_id,
                         const std::vector<std::string>& stream_ids,
                         int num_sim_layers);
};

// Provides a mechanism for describing how m= sections should be generated.
// The m= section with index X will use media_description_options[X]. There
// must be an option for each existing section if creating an answer, or a
// subsequent offer.
struct MediaSessionOptions {
  MediaSessionOptions() {}

  bool has_audio() const { return HasMediaDescription(MEDIA_TYPE_AUDIO); }
  bool has_video() const { return HasMediaDescription(MEDIA_TYPE_VIDEO); }
  bool has_data() const { return HasMediaDescription(MEDIA_TYPE_DATA); }

  bool HasMediaDescription(MediaType type) const;

  DataChannelType data_channel_type = DCT_NONE;
  bool is_muc = false;
  bool vad_enabled = true;  // When disabled, removes all CN codecs from SDP.
  bool rtcp_mux_enabled = true;
  bool bundle_enabled = false;
  std::string rtcp_cname = kDefaultRtcpCname;
  rtc::CryptoOptions crypto_options;
  // List of media description options in the same order that the media
  // descriptions will be generated.
  std::vector<MediaDescriptionOptions> media_description_options;
};

// "content" (as used in XEP-0166) descriptions for voice and video.
class MediaContentDescription : public ContentDescription {
 public:
  MediaContentDescription() {}

  virtual MediaType type() const = 0;
  virtual bool has_codecs() const = 0;

  // |protocol| is the expected media transport protocol, such as RTP/AVPF,
  // RTP/SAVPF or SCTP/DTLS.
  std::string protocol() const { return protocol_; }
  void set_protocol(const std::string& protocol) { protocol_ = protocol; }

  // TODO(steveanton): Remove once |direction()| uses RtpTransceiverDirection.
  webrtc::RtpTransceiverDirection transceiver_direction() const {
    return direction_;
  }
  void set_transceiver_direction(webrtc::RtpTransceiverDirection direction) {
    direction_ = direction;
  }

  webrtc::RtpTransceiverDirection direction() const { return direction_; }
  void set_direction(webrtc::RtpTransceiverDirection direction) {
    direction_ = direction;
  }

  bool rtcp_mux() const { return rtcp_mux_; }
  void set_rtcp_mux(bool mux) { rtcp_mux_ = mux; }

  bool rtcp_reduced_size() const { return rtcp_reduced_size_; }
  void set_rtcp_reduced_size(bool reduced_size) {
    rtcp_reduced_size_ = reduced_size;
  }

  int bandwidth() const { return bandwidth_; }
  void set_bandwidth(int bandwidth) { bandwidth_ = bandwidth; }

  const std::vector<CryptoParams>& cryptos() const { return cryptos_; }
  void AddCrypto(const CryptoParams& params) {
    cryptos_.push_back(params);
  }
  void set_cryptos(const std::vector<CryptoParams>& cryptos) {
    cryptos_ = cryptos;
  }

  CryptoType crypto_required() const { return crypto_required_; }
  void set_crypto_required(CryptoType type) {
    crypto_required_ = type;
  }

  const RtpHeaderExtensions& rtp_header_extensions() const {
    return rtp_header_extensions_;
  }
  void set_rtp_header_extensions(const RtpHeaderExtensions& extensions) {
    rtp_header_extensions_ = extensions;
    rtp_header_extensions_set_ = true;
  }
  void AddRtpHeaderExtension(const webrtc::RtpExtension& ext) {
    rtp_header_extensions_.push_back(ext);
    rtp_header_extensions_set_ = true;
  }
  void AddRtpHeaderExtension(const cricket::RtpHeaderExtension& ext) {
    webrtc::RtpExtension webrtc_extension;
    webrtc_extension.uri = ext.uri;
    webrtc_extension.id = ext.id;
    rtp_header_extensions_.push_back(webrtc_extension);
    rtp_header_extensions_set_ = true;
  }
  void ClearRtpHeaderExtensions() {
    rtp_header_extensions_.clear();
    rtp_header_extensions_set_ = true;
  }
  // We can't always tell if an empty list of header extensions is
  // because the other side doesn't support them, or just isn't hooked up to
  // signal them. For now we assume an empty list means no signaling, but
  // provide the ClearRtpHeaderExtensions method to allow "no support" to be
  // clearly indicated (i.e. when derived from other information).
  bool rtp_header_extensions_set() const {
    return rtp_header_extensions_set_;
  }
  // True iff the client supports multiple streams.
  void set_multistream(bool multistream) { multistream_ = multistream; }
  bool multistream() const { return multistream_; }
  const StreamParamsVec& streams() const {
    return streams_;
  }
  // TODO(pthatcher): Remove this by giving mediamessage.cc access
  // to MediaContentDescription
  StreamParamsVec& mutable_streams() {
    return streams_;
  }
  void AddStream(const StreamParams& stream) {
    streams_.push_back(stream);
  }
  // Legacy streams have an ssrc, but nothing else.
  void AddLegacyStream(uint32_t ssrc) {
    streams_.push_back(StreamParams::CreateLegacy(ssrc));
  }
  void AddLegacyStream(uint32_t ssrc, uint32_t fid_ssrc) {
    StreamParams sp = StreamParams::CreateLegacy(ssrc);
    sp.AddFidSsrc(ssrc, fid_ssrc);
    streams_.push_back(sp);
  }
  // Sets the CNAME of all StreamParams if it have not been set.
  void SetCnameIfEmpty(const std::string& cname) {
    for (cricket::StreamParamsVec::iterator it = streams_.begin();
         it != streams_.end(); ++it) {
      if (it->cname.empty())
        it->cname = cname;
    }
  }
  uint32_t first_ssrc() const {
    if (streams_.empty()) {
      return 0;
    }
    return streams_[0].first_ssrc();
  }
  bool has_ssrcs() const {
    if (streams_.empty()) {
      return false;
    }
    return streams_[0].has_ssrcs();
  }

  void set_conference_mode(bool enable) { conference_mode_ = enable; }
  bool conference_mode() const { return conference_mode_; }

  void set_partial(bool partial) { partial_ = partial; }
  bool partial() const { return partial_;  }

  // https://tools.ietf.org/html/rfc4566#section-5.7
  // May be present at the media or session level of SDP. If present at both
  // levels, the media-level attribute overwrites the session-level one.
  void set_connection_address(const rtc::SocketAddress& address) {
    connection_address_ = address;
  }
  const rtc::SocketAddress& connection_address() const {
    return connection_address_;
  }

 protected:
  bool rtcp_mux_ = false;
  bool rtcp_reduced_size_ = false;
  int bandwidth_ = kAutoBandwidth;
  std::string protocol_;
  std::vector<CryptoParams> cryptos_;
  CryptoType crypto_required_ = CT_NONE;
  std::vector<webrtc::RtpExtension> rtp_header_extensions_;
  bool rtp_header_extensions_set_ = false;
  bool multistream_ = false;
  StreamParamsVec streams_;
  bool conference_mode_ = false;
  bool partial_ = false;
  webrtc::RtpTransceiverDirection direction_ =
      webrtc::RtpTransceiverDirection::kSendRecv;
  rtc::SocketAddress connection_address_;
};

template <class C>
class MediaContentDescriptionImpl : public MediaContentDescription {
 public:
  typedef C CodecType;

  // Codecs should be in preference order (most preferred codec first).
  const std::vector<C>& codecs() const { return codecs_; }
  void set_codecs(const std::vector<C>& codecs) { codecs_ = codecs; }
  virtual bool has_codecs() const { return !codecs_.empty(); }
  bool HasCodec(int id) {
    bool found = false;
    for (typename std::vector<C>::iterator iter = codecs_.begin();
         iter != codecs_.end(); ++iter) {
      if (iter->id == id) {
        found = true;
        break;
      }
    }
    return found;
  }
  void AddCodec(const C& codec) {
    codecs_.push_back(codec);
  }
  void AddOrReplaceCodec(const C& codec) {
    for (typename std::vector<C>::iterator iter = codecs_.begin();
         iter != codecs_.end(); ++iter) {
      if (iter->id == codec.id) {
        *iter = codec;
        return;
      }
    }
    AddCodec(codec);
  }
  void AddCodecs(const std::vector<C>& codecs) {
    typename std::vector<C>::const_iterator codec;
    for (codec = codecs.begin(); codec != codecs.end(); ++codec) {
      AddCodec(*codec);
    }
  }

 private:
  std::vector<C> codecs_;
};

class AudioContentDescription : public MediaContentDescriptionImpl<AudioCodec> {
 public:
  AudioContentDescription() :
      agc_minus_10db_(false) {}

  virtual ContentDescription* Copy() const {
    return new AudioContentDescription(*this);
  }
  virtual MediaType type() const { return MEDIA_TYPE_AUDIO; }

  const std::string &lang() const { return lang_; }
  void set_lang(const std::string &lang) { lang_ = lang; }

  bool agc_minus_10db() const { return agc_minus_10db_; }
  void set_agc_minus_10db(bool enable) {
    agc_minus_10db_ = enable;
  }

 private:
  bool agc_minus_10db_;

 private:
  std::string lang_;
};

class VideoContentDescription : public MediaContentDescriptionImpl<VideoCodec> {
 public:
  virtual ContentDescription* Copy() const {
    return new VideoContentDescription(*this);
  }
  virtual MediaType type() const { return MEDIA_TYPE_VIDEO; }
};

class DataContentDescription : public MediaContentDescriptionImpl<DataCodec> {
 public:
  DataContentDescription() {}

  virtual ContentDescription* Copy() const {
    return new DataContentDescription(*this);
  }
  virtual MediaType type() const { return MEDIA_TYPE_DATA; }

  bool use_sctpmap() const { return use_sctpmap_; }
  void set_use_sctpmap(bool enable) { use_sctpmap_ = enable; }

 private:
  bool use_sctpmap_ = true;
};

// Creates media session descriptions according to the supplied codecs and
// other fields, as well as the supplied per-call options.
// When creating answers, performs the appropriate negotiation
// of the various fields to determine the proper result.
class MediaSessionDescriptionFactory {
 public:
  // Default ctor; use methods below to set configuration.
  // The TransportDescriptionFactory is not owned by MediaSessionDescFactory,
  // so it must be kept alive by the user of this class.
  explicit MediaSessionDescriptionFactory(
      const TransportDescriptionFactory* factory);
  // This helper automatically sets up the factory to get its configuration
  // from the specified ChannelManager.
  MediaSessionDescriptionFactory(ChannelManager* cmanager,
                                 const TransportDescriptionFactory* factory);

  const AudioCodecs& audio_sendrecv_codecs() const;
  const AudioCodecs& audio_send_codecs() const;
  const AudioCodecs& audio_recv_codecs() const;
  void set_audio_codecs(const AudioCodecs& send_codecs,
                        const AudioCodecs& recv_codecs);
  void set_audio_rtp_header_extensions(const RtpHeaderExtensions& extensions) {
    audio_rtp_extensions_ = extensions;
  }
  const RtpHeaderExtensions& audio_rtp_header_extensions() const {
    return audio_rtp_extensions_;
  }
  const VideoCodecs& video_codecs() const { return video_codecs_; }
  void set_video_codecs(const VideoCodecs& codecs) { video_codecs_ = codecs; }
  void set_video_rtp_header_extensions(const RtpHeaderExtensions& extensions) {
    video_rtp_extensions_ = extensions;
  }
  const RtpHeaderExtensions& video_rtp_header_extensions() const {
    return video_rtp_extensions_;
  }
  const DataCodecs& data_codecs() const { return data_codecs_; }
  void set_data_codecs(const DataCodecs& codecs) { data_codecs_ = codecs; }
  SecurePolicy secure() const { return secure_; }
  void set_secure(SecurePolicy s) { secure_ = s; }

  void set_enable_encrypted_rtp_header_extensions(bool enable) {
    enable_encrypted_rtp_header_extensions_ = enable;
  }

  SessionDescription* CreateOffer(
      const MediaSessionOptions& options,
      const SessionDescription* current_description) const;
  SessionDescription* CreateAnswer(
      const SessionDescription* offer,
      const MediaSessionOptions& options,
      const SessionDescription* current_description) const;

 private:
  const AudioCodecs& GetAudioCodecsForOffer(
      const webrtc::RtpTransceiverDirection& direction) const;
  const AudioCodecs& GetAudioCodecsForAnswer(
      const webrtc::RtpTransceiverDirection& offer,
      const webrtc::RtpTransceiverDirection& answer) const;
  void GetCodecsForOffer(const SessionDescription* current_description,
                         AudioCodecs* audio_codecs,
                         VideoCodecs* video_codecs,
                         DataCodecs* data_codecs) const;
  void GetCodecsForAnswer(const SessionDescription* current_description,
                          const SessionDescription* remote_offer,
                          AudioCodecs* audio_codecs,
                          VideoCodecs* video_codecs,
                          DataCodecs* data_codecs) const;
  void GetRtpHdrExtsToOffer(const SessionDescription* current_description,
                            RtpHeaderExtensions* audio_extensions,
                            RtpHeaderExtensions* video_extensions) const;
  bool AddTransportOffer(
      const std::string& content_name,
      const TransportOptions& transport_options,
      const SessionDescription* current_desc,
      SessionDescription* offer) const;

  TransportDescription* CreateTransportAnswer(
      const std::string& content_name,
      const SessionDescription* offer_desc,
      const TransportOptions& transport_options,
      const SessionDescription* current_desc,
      bool require_transport_attributes) const;

  bool AddTransportAnswer(
      const std::string& content_name,
      const TransportDescription& transport_desc,
      SessionDescription* answer_desc) const;

  // Helpers for adding media contents to the SessionDescription. Returns true
  // it succeeds or the media content is not needed, or false if there is any
  // error.

  bool AddAudioContentForOffer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const RtpHeaderExtensions& audio_rtp_extensions,
      const AudioCodecs& audio_codecs,
      StreamParamsVec* current_streams,
      SessionDescription* desc) const;

  bool AddVideoContentForOffer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const RtpHeaderExtensions& video_rtp_extensions,
      const VideoCodecs& video_codecs,
      StreamParamsVec* current_streams,
      SessionDescription* desc) const;

  bool AddDataContentForOffer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const DataCodecs& data_codecs,
      StreamParamsVec* current_streams,
      SessionDescription* desc) const;

  bool AddAudioContentForAnswer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      const ContentInfo* offer_content,
      const SessionDescription* offer_description,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const TransportInfo* bundle_transport,
      const AudioCodecs& audio_codecs,
      StreamParamsVec* current_streams,
      SessionDescription* answer) const;

  bool AddVideoContentForAnswer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      const ContentInfo* offer_content,
      const SessionDescription* offer_description,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const TransportInfo* bundle_transport,
      const VideoCodecs& video_codecs,
      StreamParamsVec* current_streams,
      SessionDescription* answer) const;

  bool AddDataContentForAnswer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      const ContentInfo* offer_content,
      const SessionDescription* offer_description,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const TransportInfo* bundle_transport,
      const DataCodecs& data_codecs,
      StreamParamsVec* current_streams,
      SessionDescription* answer) const;

  void ComputeAudioCodecsIntersectionAndUnion();

  AudioCodecs audio_send_codecs_;
  AudioCodecs audio_recv_codecs_;
  // Intersection of send and recv.
  AudioCodecs audio_sendrecv_codecs_;
  // Union of send and recv.
  AudioCodecs all_audio_codecs_;
  RtpHeaderExtensions audio_rtp_extensions_;
  VideoCodecs video_codecs_;
  RtpHeaderExtensions video_rtp_extensions_;
  DataCodecs data_codecs_;
  bool enable_encrypted_rtp_header_extensions_ = false;
  // TODO(zhihuang): Rename secure_ to sdec_policy_; rename the related getter
  // and setter.
  SecurePolicy secure_ = SEC_DISABLED;
  std::string lang_;
  const TransportDescriptionFactory* transport_desc_factory_;
};

// Convenience functions.
bool IsMediaContent(const ContentInfo* content);
bool IsAudioContent(const ContentInfo* content);
bool IsVideoContent(const ContentInfo* content);
bool IsDataContent(const ContentInfo* content);
const ContentInfo* GetFirstMediaContent(const ContentInfos& contents,
                                        MediaType media_type);
const ContentInfo* GetFirstAudioContent(const ContentInfos& contents);
const ContentInfo* GetFirstVideoContent(const ContentInfos& contents);
const ContentInfo* GetFirstDataContent(const ContentInfos& contents);
const ContentInfo* GetFirstAudioContent(const SessionDescription* sdesc);
const ContentInfo* GetFirstVideoContent(const SessionDescription* sdesc);
const ContentInfo* GetFirstDataContent(const SessionDescription* sdesc);
const AudioContentDescription* GetFirstAudioContentDescription(
    const SessionDescription* sdesc);
const VideoContentDescription* GetFirstVideoContentDescription(
    const SessionDescription* sdesc);
const DataContentDescription* GetFirstDataContentDescription(
    const SessionDescription* sdesc);
// Non-const versions of the above functions.
// Useful when modifying an existing description.
ContentInfo* GetFirstMediaContent(ContentInfos* contents, MediaType media_type);
ContentInfo* GetFirstAudioContent(ContentInfos* contents);
ContentInfo* GetFirstVideoContent(ContentInfos* contents);
ContentInfo* GetFirstDataContent(ContentInfos* contents);
ContentInfo* GetFirstAudioContent(SessionDescription* sdesc);
ContentInfo* GetFirstVideoContent(SessionDescription* sdesc);
ContentInfo* GetFirstDataContent(SessionDescription* sdesc);
AudioContentDescription* GetFirstAudioContentDescription(
    SessionDescription* sdesc);
VideoContentDescription* GetFirstVideoContentDescription(
    SessionDescription* sdesc);
DataContentDescription* GetFirstDataContentDescription(
    SessionDescription* sdesc);

// Helper functions to return crypto suites used for SDES.
void GetSupportedAudioSdesCryptoSuites(const rtc::CryptoOptions& crypto_options,
                                       std::vector<int>* crypto_suites);
void GetSupportedVideoSdesCryptoSuites(const rtc::CryptoOptions& crypto_options,
                                       std::vector<int>* crypto_suites);
void GetSupportedDataSdesCryptoSuites(const rtc::CryptoOptions& crypto_options,
                                      std::vector<int>* crypto_suites);
void GetSupportedAudioSdesCryptoSuiteNames(
    const rtc::CryptoOptions& crypto_options,
    std::vector<std::string>* crypto_suite_names);
void GetSupportedVideoSdesCryptoSuiteNames(
    const rtc::CryptoOptions& crypto_options,
    std::vector<std::string>* crypto_suite_names);
void GetSupportedDataSdesCryptoSuiteNames(
    const rtc::CryptoOptions& crypto_options,
    std::vector<std::string>* crypto_suite_names);

}  // namespace cricket

#endif  // PC_MEDIASESSION_H_
