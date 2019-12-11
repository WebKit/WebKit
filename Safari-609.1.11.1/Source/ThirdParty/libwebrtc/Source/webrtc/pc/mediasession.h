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

#include "api/mediatypes.h"
#include "media/base/mediaconstants.h"
#include "media/base/mediaengine.h"  // For DataChannelType
#include "p2p/base/icecredentialsiterator.h"
#include "p2p/base/transportdescriptionfactory.h"
#include "pc/jseptransport.h"
#include "pc/sessiondescription.h"

namespace cricket {

class ChannelManager;

// Default RTCP CNAME for unit tests.
const char kDefaultRtcpCname[] = "DefaultRtcpCname";

// Options for an RtpSender contained with an media description/"m=" section.
struct SenderOptions {
  std::string track_id;
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
  bool vad_enabled = true;  // When disabled, removes all CN codecs from SDP.
  bool rtcp_mux_enabled = true;
  bool bundle_enabled = false;
  bool is_unified_plan = false;
  bool offer_extmap_allow_mixed = false;
  std::string rtcp_cname = kDefaultRtcpCname;
  webrtc::CryptoOptions crypto_options;
  // List of media description options in the same order that the media
  // descriptions will be generated.
  std::vector<MediaDescriptionOptions> media_description_options;
  std::vector<IceParameters> pooled_ice_credentials;
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
  RtpHeaderExtensions audio_rtp_header_extensions(bool unified_plan) const {
    RtpHeaderExtensions extensions = audio_rtp_extensions_;
    // If we are Unified Plan, also offer the MID header extension.
    if (unified_plan) {
      extensions.push_back(webrtc::RtpExtension(
          webrtc::RtpExtension::kMidUri, webrtc::RtpExtension::kMidDefaultId));
    }
    return extensions;
  }
  const VideoCodecs& video_codecs() const { return video_codecs_; }
  void set_video_codecs(const VideoCodecs& codecs) { video_codecs_ = codecs; }
  void set_video_rtp_header_extensions(const RtpHeaderExtensions& extensions) {
    video_rtp_extensions_ = extensions;
  }
  RtpHeaderExtensions video_rtp_header_extensions(bool unified_plan) const {
    RtpHeaderExtensions extensions = video_rtp_extensions_;
    // If we are Unified Plan, also offer the MID header extension.
    if (unified_plan) {
      extensions.push_back(webrtc::RtpExtension(
          webrtc::RtpExtension::kMidUri, webrtc::RtpExtension::kMidDefaultId));
    }
    return extensions;
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
  void GetCodecsForOffer(
      const std::vector<const ContentInfo*>& current_active_contents,
      AudioCodecs* audio_codecs,
      VideoCodecs* video_codecs,
      DataCodecs* data_codecs) const;
  void GetCodecsForAnswer(
      const std::vector<const ContentInfo*>& current_active_contents,
      const SessionDescription& remote_offer,
      AudioCodecs* audio_codecs,
      VideoCodecs* video_codecs,
      DataCodecs* data_codecs) const;
  void GetRtpHdrExtsToOffer(
      const std::vector<const ContentInfo*>& current_active_contents,
      bool is_unified_plan,
      RtpHeaderExtensions* audio_extensions,
      RtpHeaderExtensions* video_extensions) const;
  bool AddTransportOffer(const std::string& content_name,
                         const TransportOptions& transport_options,
                         const SessionDescription* current_desc,
                         SessionDescription* offer,
                         IceCredentialsIterator* ice_credentials) const;

  TransportDescription* CreateTransportAnswer(
      const std::string& content_name,
      const SessionDescription* offer_desc,
      const TransportOptions& transport_options,
      const SessionDescription* current_desc,
      bool require_transport_attributes,
      IceCredentialsIterator* ice_credentials) const;

  bool AddTransportAnswer(const std::string& content_name,
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
      SessionDescription* desc,
      IceCredentialsIterator* ice_credentials) const;

  bool AddVideoContentForOffer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const RtpHeaderExtensions& video_rtp_extensions,
      const VideoCodecs& video_codecs,
      StreamParamsVec* current_streams,
      SessionDescription* desc,
      IceCredentialsIterator* ice_credentials) const;

  bool AddDataContentForOffer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const DataCodecs& data_codecs,
      StreamParamsVec* current_streams,
      SessionDescription* desc,
      IceCredentialsIterator* ice_credentials) const;

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
      SessionDescription* answer,
      IceCredentialsIterator* ice_credentials) const;

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
      SessionDescription* answer,
      IceCredentialsIterator* ice_credentials) const;

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
      SessionDescription* answer,
      IceCredentialsIterator* ice_credentials) const;

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
const ContentInfo* GetFirstMediaContent(const SessionDescription* sdesc,
                                        MediaType media_type);
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
ContentInfo* GetFirstMediaContent(SessionDescription* sdesc,
                                  MediaType media_type);
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
void GetSupportedAudioSdesCryptoSuites(
    const webrtc::CryptoOptions& crypto_options,
    std::vector<int>* crypto_suites);
void GetSupportedVideoSdesCryptoSuites(
    const webrtc::CryptoOptions& crypto_options,
    std::vector<int>* crypto_suites);
void GetSupportedDataSdesCryptoSuites(
    const webrtc::CryptoOptions& crypto_options,
    std::vector<int>* crypto_suites);
void GetSupportedAudioSdesCryptoSuiteNames(
    const webrtc::CryptoOptions& crypto_options,
    std::vector<std::string>* crypto_suite_names);
void GetSupportedVideoSdesCryptoSuiteNames(
    const webrtc::CryptoOptions& crypto_options,
    std::vector<std::string>* crypto_suite_names);
void GetSupportedDataSdesCryptoSuiteNames(
    const webrtc::CryptoOptions& crypto_options,
    std::vector<std::string>* crypto_suite_names);

// Returns true if the given media section protocol indicates use of RTP.
bool IsRtpProtocol(const std::string& protocol);

}  // namespace cricket

#endif  // PC_MEDIASESSION_H_
