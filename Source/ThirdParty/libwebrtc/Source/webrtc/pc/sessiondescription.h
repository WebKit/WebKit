/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SESSIONDESCRIPTION_H_
#define PC_SESSIONDESCRIPTION_H_

#include <string>
#include <vector>

#include "api/cryptoparams.h"
#include "api/rtpparameters.h"
#include "api/rtptransceiverinterface.h"
#include "media/base/codec.h"
#include "media/base/mediachannel.h"
#include "media/base/streamparams.h"
#include "p2p/base/transportinfo.h"
#include "rtc_base/constructormagic.h"

namespace cricket {

typedef std::vector<AudioCodec> AudioCodecs;
typedef std::vector<VideoCodec> VideoCodecs;
typedef std::vector<DataCodec> DataCodecs;
typedef std::vector<CryptoParams> CryptoParamsVec;
typedef std::vector<webrtc::RtpExtension> RtpHeaderExtensions;

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

class AudioContentDescription;
class VideoContentDescription;
class DataContentDescription;

// Describes a session description media section. There are subclasses for each
// media type (audio, video, data) that will have additional information.
class MediaContentDescription {
 public:
  MediaContentDescription() = default;
  virtual ~MediaContentDescription() = default;

  virtual MediaType type() const = 0;

  // Try to cast this media description to an AudioContentDescription. Returns
  // nullptr if the cast fails.
  virtual AudioContentDescription* as_audio() { return nullptr; }
  virtual const AudioContentDescription* as_audio() const { return nullptr; }

  // Try to cast this media description to a VideoContentDescription. Returns
  // nullptr if the cast fails.
  virtual VideoContentDescription* as_video() { return nullptr; }
  virtual const VideoContentDescription* as_video() const { return nullptr; }

  // Try to cast this media description to a DataContentDescription. Returns
  // nullptr if the cast fails.
  virtual DataContentDescription* as_data() { return nullptr; }
  virtual const DataContentDescription* as_data() const { return nullptr; }

  virtual bool has_codecs() const = 0;

  virtual MediaContentDescription* Copy() const = 0;

  // |protocol| is the expected media transport protocol, such as RTP/AVPF,
  // RTP/SAVPF or SCTP/DTLS.
  std::string protocol() const { return protocol_; }
  void set_protocol(const std::string& protocol) { protocol_ = protocol; }

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
  void AddCrypto(const CryptoParams& params) { cryptos_.push_back(params); }
  void set_cryptos(const std::vector<CryptoParams>& cryptos) {
    cryptos_ = cryptos;
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
  bool rtp_header_extensions_set() const { return rtp_header_extensions_set_; }
  const StreamParamsVec& streams() const { return streams_; }
  // TODO(pthatcher): Remove this by giving mediamessage.cc access
  // to MediaContentDescription
  StreamParamsVec& mutable_streams() { return streams_; }
  void AddStream(const StreamParams& stream) { streams_.push_back(stream); }
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

  // https://tools.ietf.org/html/rfc4566#section-5.7
  // May be present at the media or session level of SDP. If present at both
  // levels, the media-level attribute overwrites the session-level one.
  void set_connection_address(const rtc::SocketAddress& address) {
    connection_address_ = address;
  }
  const rtc::SocketAddress& connection_address() const {
    return connection_address_;
  }

  // Determines if it's allowed to mix one- and two-byte rtp header extensions
  // within the same rtp stream.
  enum ExtmapAllowMixed { kNo, kSession, kMedia };
  void set_extmap_allow_mixed_enum(ExtmapAllowMixed new_extmap_allow_mixed) {
    if (new_extmap_allow_mixed == kMedia &&
        extmap_allow_mixed_enum_ == kSession) {
      // Do not downgrade from session level to media level.
      return;
    }
    extmap_allow_mixed_enum_ = new_extmap_allow_mixed;
  }
  ExtmapAllowMixed extmap_allow_mixed_enum() const {
    return extmap_allow_mixed_enum_;
  }
  bool extmap_allow_mixed() const { return extmap_allow_mixed_enum_ != kNo; }

 protected:
  bool rtcp_mux_ = false;
  bool rtcp_reduced_size_ = false;
  int bandwidth_ = kAutoBandwidth;
  std::string protocol_;
  std::vector<CryptoParams> cryptos_;
  std::vector<webrtc::RtpExtension> rtp_header_extensions_;
  bool rtp_header_extensions_set_ = false;
  StreamParamsVec streams_;
  bool conference_mode_ = false;
  webrtc::RtpTransceiverDirection direction_ =
      webrtc::RtpTransceiverDirection::kSendRecv;
  rtc::SocketAddress connection_address_;
  // Mixed one- and two-byte header not included in offer on media level or
  // session level, but we will respond that we support it. The plan is to add
  // it to our offer on session level. See todo in SessionDescription.
  ExtmapAllowMixed extmap_allow_mixed_enum_ = kNo;
};

// TODO(bugs.webrtc.org/8620): Remove this alias once downstream projects have
// updated.
using ContentDescription = MediaContentDescription;

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
  void AddCodec(const C& codec) { codecs_.push_back(codec); }
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
  AudioContentDescription() {}

  virtual AudioContentDescription* Copy() const {
    return new AudioContentDescription(*this);
  }
  virtual MediaType type() const { return MEDIA_TYPE_AUDIO; }
  virtual AudioContentDescription* as_audio() { return this; }
  virtual const AudioContentDescription* as_audio() const { return this; }
};

class VideoContentDescription : public MediaContentDescriptionImpl<VideoCodec> {
 public:
  virtual VideoContentDescription* Copy() const {
    return new VideoContentDescription(*this);
  }
  virtual MediaType type() const { return MEDIA_TYPE_VIDEO; }
  virtual VideoContentDescription* as_video() { return this; }
  virtual const VideoContentDescription* as_video() const { return this; }
};

class DataContentDescription : public MediaContentDescriptionImpl<DataCodec> {
 public:
  DataContentDescription() {}

  virtual DataContentDescription* Copy() const {
    return new DataContentDescription(*this);
  }
  virtual MediaType type() const { return MEDIA_TYPE_DATA; }
  virtual DataContentDescription* as_data() { return this; }
  virtual const DataContentDescription* as_data() const { return this; }

  bool use_sctpmap() const { return use_sctpmap_; }
  void set_use_sctpmap(bool enable) { use_sctpmap_ = enable; }

 private:
  bool use_sctpmap_ = true;
};

// Protocol used for encoding media. This is the "top level" protocol that may
// be wrapped by zero or many transport protocols (UDP, ICE, etc.).
enum class MediaProtocolType {
  kRtp,  // Section will use the RTP protocol (e.g., for audio or video).
         // https://tools.ietf.org/html/rfc3550
  kSctp  // Section will use the SCTP protocol (e.g., for a data channel).
         // https://tools.ietf.org/html/rfc4960
};

// TODO(bugs.webrtc.org/8620): Remove once downstream projects have updated.
constexpr MediaProtocolType NS_JINGLE_RTP = MediaProtocolType::kRtp;
constexpr MediaProtocolType NS_JINGLE_DRAFT_SCTP = MediaProtocolType::kSctp;

// Represents a session description section. Most information about the section
// is stored in the description, which is a subclass of MediaContentDescription.
struct ContentInfo {
  friend class SessionDescription;

  explicit ContentInfo(MediaProtocolType type) : type(type) {}

  // Alias for |name|.
  std::string mid() const { return name; }
  void set_mid(const std::string& mid) { this->name = mid; }

  // Alias for |description|.
  MediaContentDescription* media_description() { return description; }
  const MediaContentDescription* media_description() const {
    return description;
  }
  void set_media_description(MediaContentDescription* desc) {
    description = desc;
  }

  // TODO(bugs.webrtc.org/8620): Rename this to mid.
  std::string name;
  MediaProtocolType type;
  bool rejected = false;
  bool bundle_only = false;
  // TODO(bugs.webrtc.org/8620): Switch to the getter and setter, and make this
  // private.
  MediaContentDescription* description = nullptr;
};

typedef std::vector<std::string> ContentNames;

// This class provides a mechanism to aggregate different media contents into a
// group. This group can also be shared with the peers in a pre-defined format.
// GroupInfo should be populated only with the |content_name| of the
// MediaDescription.
class ContentGroup {
 public:
  explicit ContentGroup(const std::string& semantics);
  ContentGroup(const ContentGroup&);
  ContentGroup(ContentGroup&&);
  ContentGroup& operator=(const ContentGroup&);
  ContentGroup& operator=(ContentGroup&&);
  ~ContentGroup();

  const std::string& semantics() const { return semantics_; }
  const ContentNames& content_names() const { return content_names_; }

  const std::string* FirstContentName() const;
  bool HasContentName(const std::string& content_name) const;
  void AddContentName(const std::string& content_name);
  bool RemoveContentName(const std::string& content_name);

 private:
  std::string semantics_;
  ContentNames content_names_;
};

typedef std::vector<ContentInfo> ContentInfos;
typedef std::vector<ContentGroup> ContentGroups;

const ContentInfo* FindContentInfoByName(const ContentInfos& contents,
                                         const std::string& name);
const ContentInfo* FindContentInfoByType(const ContentInfos& contents,
                                         const std::string& type);

// Determines how the MSID will be signaled in the SDP. These can be used as
// flags to indicate both or none.
enum MsidSignaling {
  // Signal MSID with one a=msid line in the media section.
  kMsidSignalingMediaSection = 0x1,
  // Signal MSID with a=ssrc: msid lines in the media section.
  kMsidSignalingSsrcAttribute = 0x2
};

// Describes a collection of contents, each with its own name and
// type.  Analogous to a <jingle> or <session> stanza.  Assumes that
// contents are unique be name, but doesn't enforce that.
class SessionDescription {
 public:
  SessionDescription();
  ~SessionDescription();

  SessionDescription* Copy() const;

  // Content accessors.
  const ContentInfos& contents() const { return contents_; }
  ContentInfos& contents() { return contents_; }
  const ContentInfo* GetContentByName(const std::string& name) const;
  ContentInfo* GetContentByName(const std::string& name);
  const MediaContentDescription* GetContentDescriptionByName(
      const std::string& name) const;
  MediaContentDescription* GetContentDescriptionByName(const std::string& name);
  const ContentInfo* FirstContentByType(MediaProtocolType type) const;
  const ContentInfo* FirstContent() const;

  // Content mutators.
  // Adds a content to this description. Takes ownership of ContentDescription*.
  void AddContent(const std::string& name,
                  MediaProtocolType type,
                  MediaContentDescription* description);
  void AddContent(const std::string& name,
                  MediaProtocolType type,
                  bool rejected,
                  MediaContentDescription* description);
  void AddContent(const std::string& name,
                  MediaProtocolType type,
                  bool rejected,
                  bool bundle_only,
                  MediaContentDescription* description);
  void AddContent(ContentInfo* content);

  bool RemoveContentByName(const std::string& name);

  // Transport accessors.
  const TransportInfos& transport_infos() const { return transport_infos_; }
  TransportInfos& transport_infos() { return transport_infos_; }
  const TransportInfo* GetTransportInfoByName(const std::string& name) const;
  TransportInfo* GetTransportInfoByName(const std::string& name);
  const TransportDescription* GetTransportDescriptionByName(
      const std::string& name) const {
    const TransportInfo* tinfo = GetTransportInfoByName(name);
    return tinfo ? &tinfo->description : NULL;
  }

  // Transport mutators.
  void set_transport_infos(const TransportInfos& transport_infos) {
    transport_infos_ = transport_infos;
  }
  // Adds a TransportInfo to this description.
  void AddTransportInfo(const TransportInfo& transport_info);
  bool RemoveTransportInfoByName(const std::string& name);

  // Group accessors.
  const ContentGroups& groups() const { return content_groups_; }
  const ContentGroup* GetGroupByName(const std::string& name) const;
  bool HasGroup(const std::string& name) const;

  // Group mutators.
  void AddGroup(const ContentGroup& group) { content_groups_.push_back(group); }
  // Remove the first group with the same semantics specified by |name|.
  void RemoveGroupByName(const std::string& name);

  // Global attributes.
  void set_msid_supported(bool supported) { msid_supported_ = supported; }
  bool msid_supported() const { return msid_supported_; }

  // Determines how the MSIDs were/will be signaled. Flag value composed of
  // MsidSignaling bits (see enum above).
  void set_msid_signaling(int msid_signaling) {
    msid_signaling_ = msid_signaling;
  }
  int msid_signaling() const { return msid_signaling_; }

  // Determines if it's allowed to mix one- and two-byte rtp header extensions
  // within the same rtp stream.
  void set_extmap_allow_mixed(bool supported) {
    extmap_allow_mixed_ = supported;
    MediaContentDescription::ExtmapAllowMixed media_level_setting =
        supported ? MediaContentDescription::kSession
                  : MediaContentDescription::kNo;
    for (auto& content : contents_) {
      // Do not set to kNo if the current setting is kMedia.
      if (supported || content.media_description()->extmap_allow_mixed_enum() !=
                           MediaContentDescription::kMedia) {
        content.media_description()->set_extmap_allow_mixed_enum(
            media_level_setting);
      }
    }
  }
  bool extmap_allow_mixed() const { return extmap_allow_mixed_; }

 private:
  SessionDescription(const SessionDescription&);

  ContentInfos contents_;
  TransportInfos transport_infos_;
  ContentGroups content_groups_;
  bool msid_supported_ = true;
  // Default to what Plan B would do.
  // TODO(bugs.webrtc.org/8530): Change default to kMsidSignalingMediaSection.
  int msid_signaling_ = kMsidSignalingSsrcAttribute;
  // TODO(webrtc:9985): Activate mixed one- and two-byte header extension in
  // offer at session level. It's currently not included in offer by default
  // because clients prior to https://bugs.webrtc.org/9712 cannot parse this
  // correctly. If it's included in offer to us we will respond that we support
  // it.
  bool extmap_allow_mixed_ = false;
};

// Indicates whether a session description was sent by the local client or
// received from the remote client.
enum ContentSource { CS_LOCAL, CS_REMOTE };

}  // namespace cricket

#endif  // PC_SESSIONDESCRIPTION_H_
