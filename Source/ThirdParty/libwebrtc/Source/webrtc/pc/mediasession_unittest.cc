/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include <vector>

#include "media/base/codec.h"
#include "media/base/testutils.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/transportdescription.h"
#include "p2p/base/transportinfo.h"
#include "pc/mediasession.h"
#include "pc/rtpmediautils.h"
#include "pc/srtpfilter.h"
#include "rtc_base/checks.h"
#include "rtc_base/fakesslidentity.h"
#include "rtc_base/gunit.h"
#include "rtc_base/messagedigest.h"
#include "rtc_base/ssladapter.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gmock.h"

#define ASSERT_CRYPTO(cd, s, cs)      \
  ASSERT_EQ(s, cd->cryptos().size()); \
  ASSERT_EQ(cs, cd->cryptos()[0].cipher_suite)

typedef std::vector<cricket::Candidate> Candidates;

using cricket::MediaContentDescription;
using cricket::MediaSessionDescriptionFactory;
using cricket::MediaDescriptionOptions;
using cricket::MediaSessionOptions;
using cricket::MediaType;
using cricket::MediaProtocolType;
using cricket::SessionDescription;
using cricket::SsrcGroup;
using cricket::StreamParams;
using cricket::StreamParamsVec;
using cricket::TransportDescription;
using cricket::TransportDescriptionFactory;
using cricket::TransportInfo;
using cricket::ContentInfo;
using cricket::CryptoParamsVec;
using cricket::AudioContentDescription;
using cricket::VideoContentDescription;
using cricket::DataContentDescription;
using cricket::GetFirstAudioContent;
using cricket::GetFirstVideoContent;
using cricket::GetFirstDataContent;
using cricket::GetFirstAudioContentDescription;
using cricket::GetFirstVideoContentDescription;
using cricket::GetFirstDataContentDescription;
using cricket::kAutoBandwidth;
using cricket::AudioCodec;
using cricket::VideoCodec;
using cricket::DataCodec;
using cricket::MEDIA_TYPE_AUDIO;
using cricket::MEDIA_TYPE_VIDEO;
using cricket::MEDIA_TYPE_DATA;
using cricket::SEC_DISABLED;
using cricket::SEC_ENABLED;
using cricket::SEC_REQUIRED;
using rtc::CS_AES_CM_128_HMAC_SHA1_32;
using rtc::CS_AES_CM_128_HMAC_SHA1_80;
using rtc::CS_AEAD_AES_128_GCM;
using rtc::CS_AEAD_AES_256_GCM;
using testing::ElementsAreArray;
using webrtc::RtpExtension;
using webrtc::RtpTransceiverDirection;

static const AudioCodec kAudioCodecs1[] = {
    AudioCodec(103, "ISAC", 16000, -1, 1),
    AudioCodec(102, "iLBC", 8000, 13300, 1),
    AudioCodec(0, "PCMU", 8000, 64000, 1),
    AudioCodec(8, "PCMA", 8000, 64000, 1),
    AudioCodec(117, "red", 8000, 0, 1),
    AudioCodec(107, "CN", 48000, 0, 1)};

static const AudioCodec kAudioCodecs2[] = {
    AudioCodec(126, "foo", 16000, 22000, 1),
    AudioCodec(0, "PCMU", 8000, 64000, 1),
    AudioCodec(127, "iLBC", 8000, 13300, 1),
};

static const AudioCodec kAudioCodecsAnswer[] = {
    AudioCodec(102, "iLBC", 8000, 13300, 1),
    AudioCodec(0, "PCMU", 8000, 64000, 1),
};

static const VideoCodec kVideoCodecs1[] = {VideoCodec(96, "H264-SVC"),
                                           VideoCodec(97, "H264")};

static const VideoCodec kVideoCodecs1Reverse[] = {VideoCodec(97, "H264"),
                                                  VideoCodec(96, "H264-SVC")};

static const VideoCodec kVideoCodecs2[] = {VideoCodec(126, "H264"),
                                           VideoCodec(127, "H263")};

static const VideoCodec kVideoCodecsAnswer[] = {VideoCodec(97, "H264")};

static const DataCodec kDataCodecs1[] = {DataCodec(98, "binary-data"),
                                         DataCodec(99, "utf8-text")};

static const DataCodec kDataCodecs2[] = {DataCodec(126, "binary-data"),
                                         DataCodec(127, "utf8-text")};

static const DataCodec kDataCodecsAnswer[] = {DataCodec(98, "binary-data"),
                                              DataCodec(99, "utf8-text")};

static const RtpExtension kAudioRtpExtension1[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 8),
    RtpExtension("http://google.com/testing/audio_something", 10),
};

static const RtpExtension kAudioRtpExtensionEncrypted1[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 8),
    RtpExtension("http://google.com/testing/audio_something", 10),
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 12, true),
};

static const RtpExtension kAudioRtpExtension2[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 2),
    RtpExtension("http://google.com/testing/audio_something_else", 8),
    RtpExtension("http://google.com/testing/both_audio_and_video", 7),
};

static const RtpExtension kAudioRtpExtension3[] = {
    RtpExtension("http://google.com/testing/audio_something", 2),
    RtpExtension("http://google.com/testing/both_audio_and_video", 3),
};

static const RtpExtension kAudioRtpExtension3ForEncryption[] = {
    RtpExtension("http://google.com/testing/audio_something", 2),
    // Use RTP extension that supports encryption.
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 3),
};

static const RtpExtension kAudioRtpExtension3ForEncryptionOffer[] = {
    RtpExtension("http://google.com/testing/audio_something", 2),
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 3),
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 14, true),
};

static const RtpExtension kAudioRtpExtensionAnswer[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 8),
};

static const RtpExtension kAudioRtpExtensionEncryptedAnswer[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 12, true),
};

static const RtpExtension kVideoRtpExtension1[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 14),
    RtpExtension("http://google.com/testing/video_something", 13),
};

static const RtpExtension kVideoRtpExtensionEncrypted1[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 14),
    RtpExtension("http://google.com/testing/video_something", 13),
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 11, true),
};

static const RtpExtension kVideoRtpExtension2[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 2),
    RtpExtension("http://google.com/testing/video_something_else", 14),
    RtpExtension("http://google.com/testing/both_audio_and_video", 7),
};

static const RtpExtension kVideoRtpExtension3[] = {
    RtpExtension("http://google.com/testing/video_something", 4),
    RtpExtension("http://google.com/testing/both_audio_and_video", 5),
};

static const RtpExtension kVideoRtpExtension3ForEncryption[] = {
    RtpExtension("http://google.com/testing/video_something", 4),
    // Use RTP extension that supports encryption.
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 5),
};

static const RtpExtension kVideoRtpExtensionAnswer[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 14),
};

static const RtpExtension kVideoRtpExtensionEncryptedAnswer[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 11, true),
};

static const uint32_t kSimulcastParamsSsrc[] = {10, 11, 20, 21, 30, 31};
static const uint32_t kSimSsrc[] = {10, 20, 30};
static const uint32_t kFec1Ssrc[] = {10, 11};
static const uint32_t kFec2Ssrc[] = {20, 21};
static const uint32_t kFec3Ssrc[] = {30, 31};

static const char kMediaStream1[] = "stream_1";
static const char kMediaStream2[] = "stream_2";
static const char kVideoTrack1[] = "video_1";
static const char kVideoTrack2[] = "video_2";
static const char kAudioTrack1[] = "audio_1";
static const char kAudioTrack2[] = "audio_2";
static const char kAudioTrack3[] = "audio_3";
static const char kDataTrack1[] = "data_1";
static const char kDataTrack2[] = "data_2";
static const char kDataTrack3[] = "data_3";

static const char* kMediaProtocols[] = {"RTP/AVP", "RTP/SAVP", "RTP/AVPF",
                                        "RTP/SAVPF"};
static const char* kMediaProtocolsDtls[] = {
    "TCP/TLS/RTP/SAVPF", "TCP/TLS/RTP/SAVP", "UDP/TLS/RTP/SAVPF",
    "UDP/TLS/RTP/SAVP"};

// SRTP cipher name negotiated by the tests. This must be updated if the
// default changes.
static const char* kDefaultSrtpCryptoSuite = CS_AES_CM_128_HMAC_SHA1_80;
static const char* kDefaultSrtpCryptoSuiteGcm = CS_AEAD_AES_256_GCM;

// These constants are used to make the code using "AddMediaSection" more
// readable.
static constexpr bool kStopped = true;
static constexpr bool kActive = false;

static bool IsMediaContentOfType(const ContentInfo* content,
                                 MediaType media_type) {
  RTC_DCHECK(content);
  return content->media_description()->type() == media_type;
}

static RtpTransceiverDirection GetMediaDirection(const ContentInfo* content) {
  RTC_DCHECK(content);
  return content->media_description()->direction();
}

static void AddRtxCodec(const VideoCodec& rtx_codec,
                        std::vector<VideoCodec>* codecs) {
  ASSERT_FALSE(cricket::FindCodecById(*codecs, rtx_codec.id));
  codecs->push_back(rtx_codec);
}

template <class T>
static std::vector<std::string> GetCodecNames(const std::vector<T>& codecs) {
  std::vector<std::string> codec_names;
  for (const auto& codec : codecs) {
    codec_names.push_back(codec.name);
  }
  return codec_names;
}

// This is used for test only. MIDs are not the identification of the
// MediaDescriptionOptions since some end points may not support MID and the SDP
// may not contain 'mid'.
std::vector<MediaDescriptionOptions>::iterator FindFirstMediaDescriptionByMid(
    const std::string& mid,
    MediaSessionOptions* opts) {
  return std::find_if(
      opts->media_description_options.begin(),
      opts->media_description_options.end(),
      [&mid](const MediaDescriptionOptions& t) { return t.mid == mid; });
}

std::vector<MediaDescriptionOptions>::const_iterator
FindFirstMediaDescriptionByMid(const std::string& mid,
                               const MediaSessionOptions& opts) {
  return std::find_if(
      opts.media_description_options.begin(),
      opts.media_description_options.end(),
      [&mid](const MediaDescriptionOptions& t) { return t.mid == mid; });
}

// Add a media section to the |session_options|.
static void AddMediaSection(MediaType type,
                            const std::string& mid,
                            RtpTransceiverDirection direction,
                            bool stopped,
                            MediaSessionOptions* opts) {
  opts->media_description_options.push_back(
      MediaDescriptionOptions(type, mid, direction, stopped));
}

static void AddAudioVideoSections(RtpTransceiverDirection direction,
                                  MediaSessionOptions* opts) {
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", direction, kActive, opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", direction, kActive, opts);
}

static void AddDataSection(cricket::DataChannelType dct,
                           RtpTransceiverDirection direction,
                           MediaSessionOptions* opts) {
  opts->data_channel_type = dct;
  AddMediaSection(MEDIA_TYPE_DATA, "data", direction, kActive, opts);
}

static void AttachSenderToMediaSection(
    const std::string& mid,
    MediaType type,
    const std::string& track_id,
    const std::vector<std::string>& stream_ids,
    int num_sim_layer,
    MediaSessionOptions* session_options) {
  auto it = FindFirstMediaDescriptionByMid(mid, session_options);
  switch (type) {
    case MEDIA_TYPE_AUDIO:
      it->AddAudioSender(track_id, stream_ids);
      break;
    case MEDIA_TYPE_VIDEO:
      it->AddVideoSender(track_id, stream_ids, num_sim_layer);
      break;
    case MEDIA_TYPE_DATA:
      RTC_CHECK(stream_ids.size() == 1U);
      it->AddRtpDataChannel(track_id, stream_ids[0]);
      break;
    default:
      RTC_NOTREACHED();
  }
}

static void DetachSenderFromMediaSection(const std::string& mid,
                                         const std::string& track_id,
                                         MediaSessionOptions* session_options) {
  std::vector<cricket::SenderOptions>& sender_options_list =
      FindFirstMediaDescriptionByMid(mid, session_options)->sender_options;
  auto sender_it =
      std::find_if(sender_options_list.begin(), sender_options_list.end(),
                   [track_id](const cricket::SenderOptions& sender_options) {
                     return sender_options.track_id == track_id;
                   });
  RTC_DCHECK(sender_it != sender_options_list.end());
  sender_options_list.erase(sender_it);
}

// Helper function used to create a default MediaSessionOptions for Plan B SDP.
// (https://tools.ietf.org/html/draft-uberti-rtcweb-plan-00).
static MediaSessionOptions CreatePlanBMediaSessionOptions() {
  MediaSessionOptions session_options;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &session_options);
  return session_options;
}

// TODO(zhihuang): Most of these tests were written while MediaSessionOptions
// was designed for Plan B SDP, where only one audio "m=" section and one video
// "m=" section could be generated, and ordering couldn't be controlled. Many of
// these tests may be obsolete as a result, and should be refactored or removed.
class MediaSessionDescriptionFactoryTest : public testing::Test {
 public:
  MediaSessionDescriptionFactoryTest() : f1_(&tdf1_), f2_(&tdf2_) {
    f1_.set_audio_codecs(MAKE_VECTOR(kAudioCodecs1),
                         MAKE_VECTOR(kAudioCodecs1));
    f1_.set_video_codecs(MAKE_VECTOR(kVideoCodecs1));
    f1_.set_data_codecs(MAKE_VECTOR(kDataCodecs1));
    f2_.set_audio_codecs(MAKE_VECTOR(kAudioCodecs2),
                         MAKE_VECTOR(kAudioCodecs2));
    f2_.set_video_codecs(MAKE_VECTOR(kVideoCodecs2));
    f2_.set_data_codecs(MAKE_VECTOR(kDataCodecs2));
    tdf1_.set_certificate(rtc::RTCCertificate::Create(
        std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("id1"))));
    tdf2_.set_certificate(rtc::RTCCertificate::Create(
        std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("id2"))));
  }

  // Create a video StreamParamsVec object with:
  // - one video stream with 3 simulcast streams and FEC,
  StreamParamsVec CreateComplexVideoStreamParamsVec() {
    SsrcGroup sim_group("SIM", MAKE_VECTOR(kSimSsrc));
    SsrcGroup fec_group1("FEC", MAKE_VECTOR(kFec1Ssrc));
    SsrcGroup fec_group2("FEC", MAKE_VECTOR(kFec2Ssrc));
    SsrcGroup fec_group3("FEC", MAKE_VECTOR(kFec3Ssrc));

    std::vector<SsrcGroup> ssrc_groups;
    ssrc_groups.push_back(sim_group);
    ssrc_groups.push_back(fec_group1);
    ssrc_groups.push_back(fec_group2);
    ssrc_groups.push_back(fec_group3);

    StreamParams simulcast_params;
    simulcast_params.id = kVideoTrack1;
    simulcast_params.ssrcs = MAKE_VECTOR(kSimulcastParamsSsrc);
    simulcast_params.ssrc_groups = ssrc_groups;
    simulcast_params.cname = "Video_SIM_FEC";
    simulcast_params.set_stream_ids({kMediaStream1});

    StreamParamsVec video_streams;
    video_streams.push_back(simulcast_params);

    return video_streams;
  }

  bool CompareCryptoParams(const CryptoParamsVec& c1,
                           const CryptoParamsVec& c2) {
    if (c1.size() != c2.size())
      return false;
    for (size_t i = 0; i < c1.size(); ++i)
      if (c1[i].tag != c2[i].tag || c1[i].cipher_suite != c2[i].cipher_suite ||
          c1[i].key_params != c2[i].key_params ||
          c1[i].session_params != c2[i].session_params)
        return false;
    return true;
  }

  // Returns true if the transport info contains "renomination" as an
  // ICE option.
  bool GetIceRenomination(const TransportInfo* transport_info) {
    const std::vector<std::string>& ice_options =
        transport_info->description.transport_options;
    auto iter =
        std::find(ice_options.begin(), ice_options.end(), "renomination");
    return iter != ice_options.end();
  }

  void TestTransportInfo(bool offer,
                         const MediaSessionOptions& options,
                         bool has_current_desc) {
    const std::string current_audio_ufrag = "current_audio_ufrag";
    const std::string current_audio_pwd = "current_audio_pwd";
    const std::string current_video_ufrag = "current_video_ufrag";
    const std::string current_video_pwd = "current_video_pwd";
    const std::string current_data_ufrag = "current_data_ufrag";
    const std::string current_data_pwd = "current_data_pwd";
    std::unique_ptr<SessionDescription> current_desc;
    std::unique_ptr<SessionDescription> desc;
    if (has_current_desc) {
      current_desc.reset(new SessionDescription());
      EXPECT_TRUE(current_desc->AddTransportInfo(TransportInfo(
          "audio",
          TransportDescription(current_audio_ufrag, current_audio_pwd))));
      EXPECT_TRUE(current_desc->AddTransportInfo(TransportInfo(
          "video",
          TransportDescription(current_video_ufrag, current_video_pwd))));
      EXPECT_TRUE(current_desc->AddTransportInfo(TransportInfo(
          "data", TransportDescription(current_data_ufrag, current_data_pwd))));
    }
    if (offer) {
      desc.reset(f1_.CreateOffer(options, current_desc.get()));
    } else {
      std::unique_ptr<SessionDescription> offer;
      offer.reset(f1_.CreateOffer(options, NULL));
      desc.reset(f1_.CreateAnswer(offer.get(), options, current_desc.get()));
    }
    ASSERT_TRUE(desc.get() != NULL);
    const TransportInfo* ti_audio = desc->GetTransportInfoByName("audio");
    if (options.has_audio()) {
      EXPECT_TRUE(ti_audio != NULL);
      if (has_current_desc) {
        EXPECT_EQ(current_audio_ufrag, ti_audio->description.ice_ufrag);
        EXPECT_EQ(current_audio_pwd, ti_audio->description.ice_pwd);
      } else {
        EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
                  ti_audio->description.ice_ufrag.size());
        EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
                  ti_audio->description.ice_pwd.size());
      }
      auto media_desc_options_it =
          FindFirstMediaDescriptionByMid("audio", options);
      EXPECT_EQ(
          media_desc_options_it->transport_options.enable_ice_renomination,
          GetIceRenomination(ti_audio));

    } else {
      EXPECT_TRUE(ti_audio == NULL);
    }
    const TransportInfo* ti_video = desc->GetTransportInfoByName("video");
    if (options.has_video()) {
      EXPECT_TRUE(ti_video != NULL);
      if (options.bundle_enabled) {
        EXPECT_EQ(ti_audio->description.ice_ufrag,
                  ti_video->description.ice_ufrag);
        EXPECT_EQ(ti_audio->description.ice_pwd, ti_video->description.ice_pwd);
      } else {
        if (has_current_desc) {
          EXPECT_EQ(current_video_ufrag, ti_video->description.ice_ufrag);
          EXPECT_EQ(current_video_pwd, ti_video->description.ice_pwd);
        } else {
          EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
                    ti_video->description.ice_ufrag.size());
          EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
                    ti_video->description.ice_pwd.size());
        }
      }
      auto media_desc_options_it =
          FindFirstMediaDescriptionByMid("video", options);
      EXPECT_EQ(
          media_desc_options_it->transport_options.enable_ice_renomination,
          GetIceRenomination(ti_video));
    } else {
      EXPECT_TRUE(ti_video == NULL);
    }
    const TransportInfo* ti_data = desc->GetTransportInfoByName("data");
    if (options.has_data()) {
      EXPECT_TRUE(ti_data != NULL);
      if (options.bundle_enabled) {
        EXPECT_EQ(ti_audio->description.ice_ufrag,
                  ti_data->description.ice_ufrag);
        EXPECT_EQ(ti_audio->description.ice_pwd, ti_data->description.ice_pwd);
      } else {
        if (has_current_desc) {
          EXPECT_EQ(current_data_ufrag, ti_data->description.ice_ufrag);
          EXPECT_EQ(current_data_pwd, ti_data->description.ice_pwd);
        } else {
          EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
                    ti_data->description.ice_ufrag.size());
          EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
                    ti_data->description.ice_pwd.size());
        }
      }
      auto media_desc_options_it =
          FindFirstMediaDescriptionByMid("data", options);
      EXPECT_EQ(
          media_desc_options_it->transport_options.enable_ice_renomination,
          GetIceRenomination(ti_data));

    } else {
      EXPECT_TRUE(ti_video == NULL);
    }
  }

  void TestCryptoWithBundle(bool offer) {
    f1_.set_secure(SEC_ENABLED);
    MediaSessionOptions options;
    AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
    AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                   &options);
    std::unique_ptr<SessionDescription> ref_desc;
    std::unique_ptr<SessionDescription> desc;
    if (offer) {
      options.bundle_enabled = false;
      ref_desc.reset(f1_.CreateOffer(options, NULL));
      options.bundle_enabled = true;
      desc.reset(f1_.CreateOffer(options, ref_desc.get()));
    } else {
      options.bundle_enabled = true;
      ref_desc.reset(f1_.CreateOffer(options, NULL));
      desc.reset(f1_.CreateAnswer(ref_desc.get(), options, NULL));
    }
    ASSERT_TRUE(desc);
    const cricket::MediaContentDescription* audio_media_desc =
        desc->GetContentDescriptionByName("audio");
    ASSERT_TRUE(audio_media_desc);
    const cricket::MediaContentDescription* video_media_desc =
        desc->GetContentDescriptionByName("video");
    ASSERT_TRUE(video_media_desc);
    EXPECT_TRUE(CompareCryptoParams(audio_media_desc->cryptos(),
                                    video_media_desc->cryptos()));
    EXPECT_EQ(1u, audio_media_desc->cryptos().size());
    EXPECT_EQ(kDefaultSrtpCryptoSuite,
              audio_media_desc->cryptos()[0].cipher_suite);

    // Verify the selected crypto is one from the reference audio
    // media content.
    const cricket::MediaContentDescription* ref_audio_media_desc =
        ref_desc->GetContentDescriptionByName("audio");
    bool found = false;
    for (size_t i = 0; i < ref_audio_media_desc->cryptos().size(); ++i) {
      if (ref_audio_media_desc->cryptos()[i].Matches(
              audio_media_desc->cryptos()[0])) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }

  // This test that the audio and video media direction is set to
  // |expected_direction_in_answer| in an answer if the offer direction is set
  // to |direction_in_offer| and the answer is willing to both send and receive.
  void TestMediaDirectionInAnswer(
      RtpTransceiverDirection direction_in_offer,
      RtpTransceiverDirection expected_direction_in_answer) {
    MediaSessionOptions offer_opts;
    AddAudioVideoSections(direction_in_offer, &offer_opts);

    std::unique_ptr<SessionDescription> offer(
        f1_.CreateOffer(offer_opts, NULL));
    ASSERT_TRUE(offer.get() != NULL);
    ContentInfo* ac_offer = offer->GetContentByName("audio");
    ASSERT_TRUE(ac_offer != NULL);
    ContentInfo* vc_offer = offer->GetContentByName("video");
    ASSERT_TRUE(vc_offer != NULL);

    MediaSessionOptions answer_opts;
    AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &answer_opts);
    std::unique_ptr<SessionDescription> answer(
        f2_.CreateAnswer(offer.get(), answer_opts, NULL));
    const AudioContentDescription* acd_answer =
        GetFirstAudioContentDescription(answer.get());
    EXPECT_EQ(expected_direction_in_answer, acd_answer->direction());
    const VideoContentDescription* vcd_answer =
        GetFirstVideoContentDescription(answer.get());
    EXPECT_EQ(expected_direction_in_answer, vcd_answer->direction());
  }

  bool VerifyNoCNCodecs(const cricket::ContentInfo* content) {
    RTC_DCHECK(content);
    RTC_CHECK(content->media_description());
    const cricket::AudioContentDescription* audio_desc =
        content->media_description()->as_audio();
    RTC_CHECK(audio_desc);
    for (const cricket::AudioCodec& codec : audio_desc->codecs()) {
      if (codec.name == "CN") {
        return false;
      }
    }
    return true;
  }

  void TestVideoGcmCipher(bool gcm_offer, bool gcm_answer) {
    MediaSessionOptions offer_opts;
    AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &offer_opts);
    offer_opts.crypto_options.srtp.enable_gcm_crypto_suites = gcm_offer;

    MediaSessionOptions answer_opts;
    AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &answer_opts);
    answer_opts.crypto_options.srtp.enable_gcm_crypto_suites = gcm_answer;

    f1_.set_secure(SEC_ENABLED);
    f2_.set_secure(SEC_ENABLED);
    std::unique_ptr<SessionDescription> offer(
        f1_.CreateOffer(offer_opts, NULL));
    ASSERT_TRUE(offer.get() != NULL);
    std::unique_ptr<SessionDescription> answer(
        f2_.CreateAnswer(offer.get(), answer_opts, NULL));
    const ContentInfo* ac = answer->GetContentByName("audio");
    const ContentInfo* vc = answer->GetContentByName("video");
    ASSERT_TRUE(ac != NULL);
    ASSERT_TRUE(vc != NULL);
    EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
    EXPECT_EQ(MediaProtocolType::kRtp, vc->type);
    const AudioContentDescription* acd = ac->media_description()->as_audio();
    const VideoContentDescription* vcd = vc->media_description()->as_video();
    EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
    EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));
    EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // negotiated auto bw
    EXPECT_EQ(0U, acd->first_ssrc());             // no sender is attached
    EXPECT_TRUE(acd->rtcp_mux());                 // negotiated rtcp-mux
    if (gcm_offer && gcm_answer) {
      ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuiteGcm);
    } else {
      ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuite);
    }
    EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
    EXPECT_THAT(vcd->codecs(), ElementsAreArray(kVideoCodecsAnswer));
    EXPECT_EQ(0U, vcd->first_ssrc());  // no sender is attached
    EXPECT_TRUE(vcd->rtcp_mux());      // negotiated rtcp-mux
    if (gcm_offer && gcm_answer) {
      ASSERT_CRYPTO(vcd, 1U, kDefaultSrtpCryptoSuiteGcm);
    } else {
      ASSERT_CRYPTO(vcd, 1U, kDefaultSrtpCryptoSuite);
    }
    EXPECT_EQ(cricket::kMediaProtocolSavpf, vcd->protocol());
  }

 protected:
  MediaSessionDescriptionFactory f1_;
  MediaSessionDescriptionFactory f2_;
  TransportDescriptionFactory tdf1_;
  TransportDescriptionFactory tdf2_;
};

// Create a typical audio offer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioOffer) {
  f1_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(
      f1_.CreateOffer(CreatePlanBMediaSessionOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc == NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_sendrecv_codecs(), acd->codecs());
  EXPECT_EQ(0U, acd->first_ssrc());             // no sender is attached.
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(acd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, acd->protocol());
}

// Create a typical video offer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoOffer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  f1_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  EXPECT_EQ(MediaProtocolType::kRtp, vc->type);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  const VideoContentDescription* vcd = vc->media_description()->as_video();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_sendrecv_codecs(), acd->codecs());
  EXPECT_EQ(0U, acd->first_ssrc());             // no sender is attached
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(acd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, acd->protocol());
  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_EQ(f1_.video_codecs(), vcd->codecs());
  EXPECT_EQ(0U, vcd->first_ssrc());             // no sender is attached
  EXPECT_EQ(kAutoBandwidth, vcd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(vcd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(vcd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, vcd->protocol());
}

// Test creating an offer with bundle where the Codecs have the same dynamic
// RTP playlod type. The test verifies that the offer don't contain the
// duplicate RTP payload types.
TEST_F(MediaSessionDescriptionFactoryTest, TestBundleOfferWithSameCodecPlType) {
  const VideoCodec& offered_video_codec = f2_.video_codecs()[0];
  const AudioCodec& offered_audio_codec = f2_.audio_sendrecv_codecs()[0];
  const DataCodec& offered_data_codec = f2_.data_codecs()[0];
  ASSERT_EQ(offered_video_codec.id, offered_audio_codec.id);
  ASSERT_EQ(offered_video_codec.id, offered_data_codec.id);

  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly, &opts);
  opts.bundle_enabled = true;
  std::unique_ptr<SessionDescription> offer(f2_.CreateOffer(opts, NULL));
  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(offer.get());
  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(offer.get());
  const DataContentDescription* dcd =
      GetFirstDataContentDescription(offer.get());
  ASSERT_TRUE(NULL != vcd);
  ASSERT_TRUE(NULL != acd);
  ASSERT_TRUE(NULL != dcd);
  EXPECT_NE(vcd->codecs()[0].id, acd->codecs()[0].id);
  EXPECT_NE(vcd->codecs()[0].id, dcd->codecs()[0].id);
  EXPECT_NE(acd->codecs()[0].id, dcd->codecs()[0].id);
  EXPECT_EQ(vcd->codecs()[0].name, offered_video_codec.name);
  EXPECT_EQ(acd->codecs()[0].name, offered_audio_codec.name);
  EXPECT_EQ(dcd->codecs()[0].name, offered_data_codec.name);
}

// Test creating an updated offer with bundle, audio, video and data
// after an audio only session has been negotiated.
TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateUpdatedVideoOfferWithBundle) {
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kInactive,
                  kStopped, &opts);
  opts.data_channel_type = cricket::DCT_NONE;
  opts.bundle_enabled = true;
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  MediaSessionOptions updated_opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &updated_opts);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                 &updated_opts);
  updated_opts.bundle_enabled = true;
  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOffer(updated_opts, answer.get()));

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(updated_offer.get());
  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  const DataContentDescription* dcd =
      GetFirstDataContentDescription(updated_offer.get());
  EXPECT_TRUE(NULL != vcd);
  EXPECT_TRUE(NULL != acd);
  EXPECT_TRUE(NULL != dcd);

  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, acd->protocol());
  ASSERT_CRYPTO(vcd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, vcd->protocol());
  ASSERT_CRYPTO(dcd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, dcd->protocol());
}

// Create a RTP data offer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateRtpDataOffer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly, &opts);
  f1_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* dc = offer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(dc != NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  EXPECT_EQ(MediaProtocolType::kRtp, dc->type);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  const DataContentDescription* dcd = dc->media_description()->as_data();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_sendrecv_codecs(), acd->codecs());
  EXPECT_EQ(0U, acd->first_ssrc());             // no sender is attched.
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(acd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, acd->protocol());
  EXPECT_EQ(MEDIA_TYPE_DATA, dcd->type());
  EXPECT_EQ(f1_.data_codecs(), dcd->codecs());
  EXPECT_EQ(0U, dcd->first_ssrc());  // no sender is attached.
  EXPECT_EQ(cricket::kDataMaxBandwidth,
            dcd->bandwidth());   // default bandwidth (auto)
  EXPECT_TRUE(dcd->rtcp_mux());  // rtcp-mux defaults on
  ASSERT_CRYPTO(dcd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, dcd->protocol());
}

// Create an SCTP data offer with bundle without error.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateSctpDataOffer) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddDataSection(cricket::DCT_SCTP, RtpTransceiverDirection::kSendRecv, &opts);
  f1_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  EXPECT_TRUE(offer.get() != NULL);
  EXPECT_TRUE(offer->GetContentByName("data") != NULL);
}

// Test creating an sctp data channel from an already generated offer.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateImplicitSctpDataOffer) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddDataSection(cricket::DCT_SCTP, RtpTransceiverDirection::kSendRecv, &opts);
  f1_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer1(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer1.get() != NULL);
  const ContentInfo* data = offer1->GetContentByName("data");
  ASSERT_TRUE(data != NULL);
  ASSERT_EQ(cricket::kMediaProtocolSctp, data->media_description()->protocol());

  // Now set data_channel_type to 'none' (default) and make sure that the
  // datachannel type that gets generated from the previous offer, is of the
  // same type.
  opts.data_channel_type = cricket::DCT_NONE;
  std::unique_ptr<SessionDescription> offer2(
      f1_.CreateOffer(opts, offer1.get()));
  data = offer2->GetContentByName("data");
  ASSERT_TRUE(data != NULL);
  EXPECT_EQ(cricket::kMediaProtocolSctp, data->media_description()->protocol());
}

// Create an audio, video offer without legacy StreamParams.
TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateOfferWithoutLegacyStreams) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  const VideoContentDescription* vcd = vc->media_description()->as_video();

  EXPECT_FALSE(vcd->has_ssrcs());  // No StreamParams.
  EXPECT_FALSE(acd->has_ssrcs());  // No StreamParams.
}

// Creates an audio+video sendonly offer.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateSendOnlyOffer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendOnly, &opts);
  AttachSenderToMediaSection("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                             {kMediaStream1}, 1, &opts);
  AttachSenderToMediaSection("audio", MEDIA_TYPE_AUDIO, kAudioTrack1,
                             {kMediaStream1}, 1, &opts);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  EXPECT_EQ(2u, offer->contents().size());
  EXPECT_TRUE(IsMediaContentOfType(&offer->contents()[0], MEDIA_TYPE_AUDIO));
  EXPECT_TRUE(IsMediaContentOfType(&offer->contents()[1], MEDIA_TYPE_VIDEO));

  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            GetMediaDirection(&offer->contents()[0]));
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            GetMediaDirection(&offer->contents()[1]));
}

// Verifies that the order of the media contents in the current
// SessionDescription is preserved in the new SessionDescription.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateOfferContentOrder) {
  MediaSessionOptions opts;
  AddDataSection(cricket::DCT_SCTP, RtpTransceiverDirection::kSendRecv, &opts);

  std::unique_ptr<SessionDescription> offer1(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer1.get() != NULL);
  EXPECT_EQ(1u, offer1->contents().size());
  EXPECT_TRUE(IsMediaContentOfType(&offer1->contents()[0], MEDIA_TYPE_DATA));

  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::unique_ptr<SessionDescription> offer2(
      f1_.CreateOffer(opts, offer1.get()));
  ASSERT_TRUE(offer2.get() != NULL);
  EXPECT_EQ(2u, offer2->contents().size());
  EXPECT_TRUE(IsMediaContentOfType(&offer2->contents()[0], MEDIA_TYPE_DATA));
  EXPECT_TRUE(IsMediaContentOfType(&offer2->contents()[1], MEDIA_TYPE_VIDEO));

  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::unique_ptr<SessionDescription> offer3(
      f1_.CreateOffer(opts, offer2.get()));
  ASSERT_TRUE(offer3.get() != NULL);
  EXPECT_EQ(3u, offer3->contents().size());
  EXPECT_TRUE(IsMediaContentOfType(&offer3->contents()[0], MEDIA_TYPE_DATA));
  EXPECT_TRUE(IsMediaContentOfType(&offer3->contents()[1], MEDIA_TYPE_VIDEO));
  EXPECT_TRUE(IsMediaContentOfType(&offer3->contents()[2], MEDIA_TYPE_AUDIO));
}

// Create a typical audio answer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioAnswer) {
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(
      f1_.CreateOffer(CreatePlanBMediaSessionOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), CreatePlanBMediaSessionOptions(), NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc == NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));
  EXPECT_EQ(0U, acd->first_ssrc());             // no sender is attached
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // negotiated auto bw
  EXPECT_TRUE(acd->rtcp_mux());                 // negotiated rtcp-mux
  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, acd->protocol());
}

// Create a typical audio answer with GCM ciphers enabled, and ensure it
// matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioAnswerGcm) {
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  MediaSessionOptions opts = CreatePlanBMediaSessionOptions();
  opts.crypto_options.srtp.enable_gcm_crypto_suites = true;
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc == NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));
  EXPECT_EQ(0U, acd->first_ssrc());             // no sender is attached
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // negotiated auto bw
  EXPECT_TRUE(acd->rtcp_mux());                 // negotiated rtcp-mux
  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuiteGcm);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, acd->protocol());
}

// Create a typical video answer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoAnswer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  EXPECT_EQ(MediaProtocolType::kRtp, vc->type);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  const VideoContentDescription* vcd = vc->media_description()->as_video();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // negotiated auto bw
  EXPECT_EQ(0U, acd->first_ssrc());             // no sender is attached
  EXPECT_TRUE(acd->rtcp_mux());                 // negotiated rtcp-mux
  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_THAT(vcd->codecs(), ElementsAreArray(kVideoCodecsAnswer));
  EXPECT_EQ(0U, vcd->first_ssrc());  // no sender is attached
  EXPECT_TRUE(vcd->rtcp_mux());      // negotiated rtcp-mux
  ASSERT_CRYPTO(vcd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, vcd->protocol());
}

// Create a typical video answer with GCM ciphers enabled, and ensure it
// matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoAnswerGcm) {
  TestVideoGcmCipher(true, true);
}

// Create a typical video answer with GCM ciphers enabled for the offer only,
// and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoAnswerGcmOffer) {
  TestVideoGcmCipher(true, false);
}

// Create a typical video answer with GCM ciphers enabled for the answer only,
// and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoAnswerGcmAnswer) {
  TestVideoGcmCipher(false, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateDataAnswer) {
  MediaSessionOptions opts = CreatePlanBMediaSessionOptions();
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly, &opts);
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* dc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(dc != NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  EXPECT_EQ(MediaProtocolType::kRtp, dc->type);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  const DataContentDescription* dcd = dc->media_description()->as_data();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // negotiated auto bw
  EXPECT_EQ(0U, acd->first_ssrc());             // no sender is attached
  EXPECT_TRUE(acd->rtcp_mux());                 // negotiated rtcp-mux
  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(MEDIA_TYPE_DATA, dcd->type());
  EXPECT_THAT(dcd->codecs(), ElementsAreArray(kDataCodecsAnswer));
  EXPECT_EQ(0U, dcd->first_ssrc());  // no sender is attached
  EXPECT_TRUE(dcd->rtcp_mux());      // negotiated rtcp-mux
  ASSERT_CRYPTO(dcd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, dcd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateDataAnswerGcm) {
  MediaSessionOptions opts = CreatePlanBMediaSessionOptions();
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly, &opts);
  opts.crypto_options.srtp.enable_gcm_crypto_suites = true;
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* dc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(dc != NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  EXPECT_EQ(MediaProtocolType::kRtp, dc->type);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  const DataContentDescription* dcd = dc->media_description()->as_data();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // negotiated auto bw
  EXPECT_EQ(0U, acd->first_ssrc());             // no sender is attached
  EXPECT_TRUE(acd->rtcp_mux());                 // negotiated rtcp-mux
  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuiteGcm);
  EXPECT_EQ(MEDIA_TYPE_DATA, dcd->type());
  EXPECT_THAT(dcd->codecs(), ElementsAreArray(kDataCodecsAnswer));
  EXPECT_EQ(0U, dcd->first_ssrc());  // no sender is attached
  EXPECT_TRUE(dcd->rtcp_mux());      // negotiated rtcp-mux
  ASSERT_CRYPTO(dcd, 1U, kDefaultSrtpCryptoSuiteGcm);
  EXPECT_EQ(cricket::kMediaProtocolSavpf, dcd->protocol());
}

// The use_sctpmap flag should be set in a DataContentDescription by default.
// The answer's use_sctpmap flag should match the offer's.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateDataAnswerUsesSctpmap) {
  MediaSessionOptions opts;
  AddDataSection(cricket::DCT_SCTP, RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  ContentInfo* dc_offer = offer->GetContentByName("data");
  ASSERT_TRUE(dc_offer != NULL);
  DataContentDescription* dcd_offer = dc_offer->media_description()->as_data();
  EXPECT_TRUE(dcd_offer->use_sctpmap());

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* dc_answer = answer->GetContentByName("data");
  ASSERT_TRUE(dc_answer != NULL);
  const DataContentDescription* dcd_answer =
      dc_answer->media_description()->as_data();
  EXPECT_TRUE(dcd_answer->use_sctpmap());
}

// The answer's use_sctpmap flag should match the offer's.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateDataAnswerWithoutSctpmap) {
  MediaSessionOptions opts;
  AddDataSection(cricket::DCT_SCTP, RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  ContentInfo* dc_offer = offer->GetContentByName("data");
  ASSERT_TRUE(dc_offer != NULL);
  DataContentDescription* dcd_offer = dc_offer->media_description()->as_data();
  dcd_offer->set_use_sctpmap(false);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* dc_answer = answer->GetContentByName("data");
  ASSERT_TRUE(dc_answer != NULL);
  const DataContentDescription* dcd_answer =
      dc_answer->media_description()->as_data();
  EXPECT_FALSE(dcd_answer->use_sctpmap());
}

// Test that a valid answer will be created for "DTLS/SCTP", "UDP/DTLS/SCTP"
// and "TCP/DTLS/SCTP" offers.
TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateDataAnswerToDifferentOfferedProtos) {
  // Need to enable DTLS offer/answer generation (disabled by default in this
  // test).
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  tdf1_.set_secure(SEC_ENABLED);
  tdf2_.set_secure(SEC_ENABLED);

  MediaSessionOptions opts;
  AddDataSection(cricket::DCT_SCTP, RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer.get() != nullptr);
  ContentInfo* dc_offer = offer->GetContentByName("data");
  ASSERT_TRUE(dc_offer != nullptr);
  DataContentDescription* dcd_offer = dc_offer->media_description()->as_data();

  std::vector<std::string> protos = {"DTLS/SCTP", "UDP/DTLS/SCTP",
                                     "TCP/DTLS/SCTP"};
  for (const std::string& proto : protos) {
    dcd_offer->set_protocol(proto);
    std::unique_ptr<SessionDescription> answer(
        f2_.CreateAnswer(offer.get(), opts, nullptr));
    const ContentInfo* dc_answer = answer->GetContentByName("data");
    ASSERT_TRUE(dc_answer != nullptr);
    const DataContentDescription* dcd_answer =
        dc_answer->media_description()->as_data();
    EXPECT_FALSE(dc_answer->rejected);
    EXPECT_EQ(proto, dcd_answer->protocol());
  }
}

// Verifies that the order of the media contents in the offer is preserved in
// the answer.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAnswerContentOrder) {
  MediaSessionOptions opts;

  // Creates a data only offer.
  AddDataSection(cricket::DCT_SCTP, RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer1(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer1.get() != NULL);

  // Appends audio to the offer.
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::unique_ptr<SessionDescription> offer2(
      f1_.CreateOffer(opts, offer1.get()));
  ASSERT_TRUE(offer2.get() != NULL);

  // Appends video to the offer.
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::unique_ptr<SessionDescription> offer3(
      f1_.CreateOffer(opts, offer2.get()));
  ASSERT_TRUE(offer3.get() != NULL);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer3.get(), opts, NULL));
  ASSERT_TRUE(answer.get() != NULL);
  EXPECT_EQ(3u, answer->contents().size());
  EXPECT_TRUE(IsMediaContentOfType(&answer->contents()[0], MEDIA_TYPE_DATA));
  EXPECT_TRUE(IsMediaContentOfType(&answer->contents()[1], MEDIA_TYPE_AUDIO));
  EXPECT_TRUE(IsMediaContentOfType(&answer->contents()[2], MEDIA_TYPE_VIDEO));
}

// TODO(deadbeef): Extend these tests to ensure the correct direction with other
// answerer settings.

// This test that the media direction is set to send/receive in an answer if
// the offer is send receive.
TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToSendReceiveOffer) {
  TestMediaDirectionInAnswer(RtpTransceiverDirection::kSendRecv,
                             RtpTransceiverDirection::kSendRecv);
}

// This test that the media direction is set to receive only in an answer if
// the offer is send only.
TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToSendOnlyOffer) {
  TestMediaDirectionInAnswer(RtpTransceiverDirection::kSendOnly,
                             RtpTransceiverDirection::kRecvOnly);
}

// This test that the media direction is set to send only in an answer if
// the offer is recv only.
TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToRecvOnlyOffer) {
  TestMediaDirectionInAnswer(RtpTransceiverDirection::kRecvOnly,
                             RtpTransceiverDirection::kSendOnly);
}

// This test that the media direction is set to inactive in an answer if
// the offer is inactive.
TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToInactiveOffer) {
  TestMediaDirectionInAnswer(RtpTransceiverDirection::kInactive,
                             RtpTransceiverDirection::kInactive);
}

// Test that a data content with an unknown protocol is rejected in an answer.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateDataAnswerToOfferWithUnknownProtocol) {
  MediaSessionOptions opts;
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly, &opts);
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ContentInfo* dc_offer = offer->GetContentByName("data");
  ASSERT_TRUE(dc_offer != NULL);
  DataContentDescription* dcd_offer = dc_offer->media_description()->as_data();
  ASSERT_TRUE(dcd_offer != NULL);
  std::string protocol = "a weird unknown protocol";
  dcd_offer->set_protocol(protocol);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const ContentInfo* dc_answer = answer->GetContentByName("data");
  ASSERT_TRUE(dc_answer != NULL);
  EXPECT_TRUE(dc_answer->rejected);
  const DataContentDescription* dcd_answer =
      dc_answer->media_description()->as_data();
  ASSERT_TRUE(dcd_answer != NULL);
  EXPECT_EQ(protocol, dcd_answer->protocol());
}

// Test that the media protocol is RTP/AVPF if DTLS and SDES are disabled.
TEST_F(MediaSessionDescriptionFactoryTest, AudioOfferAnswerWithCryptoDisabled) {
  MediaSessionOptions opts = CreatePlanBMediaSessionOptions();
  f1_.set_secure(SEC_DISABLED);
  f2_.set_secure(SEC_DISABLED);
  tdf1_.set_secure(SEC_DISABLED);
  tdf2_.set_secure(SEC_DISABLED);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  const AudioContentDescription* offer_acd =
      GetFirstAudioContentDescription(offer.get());
  ASSERT_TRUE(offer_acd != NULL);
  EXPECT_EQ(cricket::kMediaProtocolAvpf, offer_acd->protocol());

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const ContentInfo* ac_answer = answer->GetContentByName("audio");
  ASSERT_TRUE(ac_answer != NULL);
  EXPECT_FALSE(ac_answer->rejected);

  const AudioContentDescription* answer_acd =
      GetFirstAudioContentDescription(answer.get());
  ASSERT_TRUE(answer_acd != NULL);
  EXPECT_EQ(cricket::kMediaProtocolAvpf, answer_acd->protocol());
}

// Create a video offer and answer and ensure the RTP header extensions
// matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestOfferAnswerWithRtpExtensions) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  f1_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension1));
  f1_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension1));
  f2_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension2));
  f2_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension2));

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtension1),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtension1),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionAnswer),
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionAnswer),
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithEncryptedRtpExtensionsBoth) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  f1_.set_enable_encrypted_rtp_header_extensions(true);
  f2_.set_enable_encrypted_rtp_header_extensions(true);

  f1_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension1));
  f1_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension1));
  f2_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension2));
  f2_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension2));

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionEncrypted1),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionEncrypted1),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionEncryptedAnswer),
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionEncryptedAnswer),
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithEncryptedRtpExtensionsOffer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  f1_.set_enable_encrypted_rtp_header_extensions(true);

  f1_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension1));
  f1_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension1));
  f2_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension2));
  f2_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension2));

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionEncrypted1),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionEncrypted1),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionAnswer),
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionAnswer),
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithEncryptedRtpExtensionsAnswer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  f2_.set_enable_encrypted_rtp_header_extensions(true);

  f1_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension1));
  f1_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension1));
  f2_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension2));
  f2_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension2));

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtension1),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtension1),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionAnswer),
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionAnswer),
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());
}

// Create an audio, video, data answer without legacy StreamParams.
TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateAnswerWithoutLegacyStreams) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  const ContentInfo* dc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  const VideoContentDescription* vcd = vc->media_description()->as_video();
  const DataContentDescription* dcd = dc->media_description()->as_data();

  EXPECT_FALSE(acd->has_ssrcs());  // No StreamParams.
  EXPECT_FALSE(vcd->has_ssrcs());  // No StreamParams.
  EXPECT_FALSE(dcd->has_ssrcs());  // No StreamParams.
}

// Create a typical video answer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoAnswerRtcpMux) {
  MediaSessionOptions offer_opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &offer_opts);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kSendRecv,
                 &offer_opts);

  MediaSessionOptions answer_opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &answer_opts);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kSendRecv,
                 &answer_opts);

  std::unique_ptr<SessionDescription> offer;
  std::unique_ptr<SessionDescription> answer;

  offer_opts.rtcp_mux_enabled = true;
  answer_opts.rtcp_mux_enabled = true;
  offer.reset(f1_.CreateOffer(offer_opts, NULL));
  answer.reset(f2_.CreateAnswer(offer.get(), answer_opts, NULL));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(answer.get()));
  EXPECT_TRUE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstDataContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstDataContentDescription(answer.get())->rtcp_mux());

  offer_opts.rtcp_mux_enabled = true;
  answer_opts.rtcp_mux_enabled = false;
  offer.reset(f1_.CreateOffer(offer_opts, NULL));
  answer.reset(f2_.CreateAnswer(offer.get(), answer_opts, NULL));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(answer.get()));
  EXPECT_TRUE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstDataContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstDataContentDescription(answer.get())->rtcp_mux());

  offer_opts.rtcp_mux_enabled = false;
  answer_opts.rtcp_mux_enabled = true;
  offer.reset(f1_.CreateOffer(offer_opts, NULL));
  answer.reset(f2_.CreateAnswer(offer.get(), answer_opts, NULL));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(answer.get()));
  EXPECT_FALSE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstDataContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstDataContentDescription(answer.get())->rtcp_mux());

  offer_opts.rtcp_mux_enabled = false;
  answer_opts.rtcp_mux_enabled = false;
  offer.reset(f1_.CreateOffer(offer_opts, NULL));
  answer.reset(f2_.CreateAnswer(offer.get(), answer_opts, NULL));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(answer.get()));
  EXPECT_FALSE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstDataContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstDataContentDescription(answer.get())->rtcp_mux());
}

// Create an audio-only answer to a video offer.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioAnswerToVideo) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);

  opts.media_description_options[1].stopped = true;
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(vc->media_description() != NULL);
  EXPECT_TRUE(vc->rejected);
}

// Create an audio-only answer to an offer with data.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateNoDataAnswerToDataOffer) {
  MediaSessionOptions opts = CreatePlanBMediaSessionOptions();
  opts.data_channel_type = cricket::DCT_RTP;
  AddMediaSection(MEDIA_TYPE_DATA, "data", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);

  opts.media_description_options[1].stopped = true;
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* dc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(dc != NULL);
  ASSERT_TRUE(dc->media_description() != NULL);
  EXPECT_TRUE(dc->rejected);
}

// Create an answer that rejects the contents which are rejected in the offer.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerToOfferWithRejectedMedia) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  ContentInfo* ac = offer->GetContentByName("audio");
  ContentInfo* vc = offer->GetContentByName("video");
  ContentInfo* dc = offer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  ac->rejected = true;
  vc->rejected = true;
  dc->rejected = true;
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  ac = answer->GetContentByName("audio");
  vc = answer->GetContentByName("video");
  dc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  EXPECT_TRUE(ac->rejected);
  EXPECT_TRUE(vc->rejected);
  EXPECT_TRUE(dc->rejected);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerSupportsMixedOneAndTwoByteHeaderExtensions) {
  MediaSessionOptions opts;
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  // Offer without request of mixed one- and two-byte header extensions.
  offer->set_extmap_allow_mixed(false);
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer_no_support(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  EXPECT_FALSE(answer_no_support->extmap_allow_mixed());

  // Offer with request of mixed one- and two-byte header extensions.
  offer->set_extmap_allow_mixed(true);
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer_support(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  EXPECT_TRUE(answer_support->extmap_allow_mixed());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerSupportsMixedOneAndTwoByteHeaderExtensionsOnMediaLevel) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  MediaContentDescription* video_offer =
      offer->GetContentDescriptionByName("video");
  ASSERT_TRUE(video_offer);
  MediaContentDescription* audio_offer =
      offer->GetContentDescriptionByName("audio");
  ASSERT_TRUE(audio_offer);

  // Explicit disable of mixed one-two byte header support in offer.
  video_offer->set_extmap_allow_mixed_enum(MediaContentDescription::kNo);
  audio_offer->set_extmap_allow_mixed_enum(MediaContentDescription::kNo);

  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer_no_support(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  MediaContentDescription* video_answer =
      answer_no_support->GetContentDescriptionByName("video");
  MediaContentDescription* audio_answer =
      answer_no_support->GetContentDescriptionByName("audio");
  EXPECT_EQ(MediaContentDescription::kNo,
            video_answer->extmap_allow_mixed_enum());
  EXPECT_EQ(MediaContentDescription::kNo,
            audio_answer->extmap_allow_mixed_enum());

  // Enable mixed one-two byte header support in offer.
  video_offer->set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);
  audio_offer->set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer_support(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  video_answer = answer_support->GetContentDescriptionByName("video");
  audio_answer = answer_support->GetContentDescriptionByName("audio");
  EXPECT_EQ(MediaContentDescription::kMedia,
            video_answer->extmap_allow_mixed_enum());
  EXPECT_EQ(MediaContentDescription::kMedia,
            audio_answer->extmap_allow_mixed_enum());
}

// Create an audio and video offer with:
// - one video track
// - two audio tracks
// - two data tracks
// and ensure it matches what we expect. Also updates the initial offer by
// adding a new video track and replaces one of the audio tracks.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateMultiStreamVideoOffer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &opts);
  AttachSenderToMediaSection("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                             {kMediaStream1}, 1, &opts);
  AttachSenderToMediaSection("audio", MEDIA_TYPE_AUDIO, kAudioTrack1,
                             {kMediaStream1}, 1, &opts);
  AttachSenderToMediaSection("audio", MEDIA_TYPE_AUDIO, kAudioTrack2,
                             {kMediaStream1}, 1, &opts);

  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kSendRecv, &opts);
  AttachSenderToMediaSection("data", MEDIA_TYPE_DATA, kDataTrack1,
                             {kMediaStream1}, 1, &opts);
  AttachSenderToMediaSection("data", MEDIA_TYPE_DATA, kDataTrack2,
                             {kMediaStream1}, 1, &opts);

  f1_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));

  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  const ContentInfo* dc = offer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  const VideoContentDescription* vcd = vc->media_description()->as_video();
  const DataContentDescription* dcd = dc->media_description()->as_data();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_sendrecv_codecs(), acd->codecs());

  const StreamParamsVec& audio_streams = acd->streams();
  ASSERT_EQ(2U, audio_streams.size());
  EXPECT_EQ(audio_streams[0].cname, audio_streams[1].cname);
  EXPECT_EQ(kAudioTrack1, audio_streams[0].id);
  ASSERT_EQ(1U, audio_streams[0].ssrcs.size());
  EXPECT_NE(0U, audio_streams[0].ssrcs[0]);
  EXPECT_EQ(kAudioTrack2, audio_streams[1].id);
  ASSERT_EQ(1U, audio_streams[1].ssrcs.size());
  EXPECT_NE(0U, audio_streams[1].ssrcs[0]);

  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(acd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuite);

  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_EQ(f1_.video_codecs(), vcd->codecs());
  ASSERT_CRYPTO(vcd, 1U, kDefaultSrtpCryptoSuite);

  const StreamParamsVec& video_streams = vcd->streams();
  ASSERT_EQ(1U, video_streams.size());
  EXPECT_EQ(video_streams[0].cname, audio_streams[0].cname);
  EXPECT_EQ(kVideoTrack1, video_streams[0].id);
  EXPECT_EQ(kAutoBandwidth, vcd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(vcd->rtcp_mux());                 // rtcp-mux defaults on

  EXPECT_EQ(MEDIA_TYPE_DATA, dcd->type());
  EXPECT_EQ(f1_.data_codecs(), dcd->codecs());
  ASSERT_CRYPTO(dcd, 1U, kDefaultSrtpCryptoSuite);

  const StreamParamsVec& data_streams = dcd->streams();
  ASSERT_EQ(2U, data_streams.size());
  EXPECT_EQ(data_streams[0].cname, data_streams[1].cname);
  EXPECT_EQ(kDataTrack1, data_streams[0].id);
  ASSERT_EQ(1U, data_streams[0].ssrcs.size());
  EXPECT_NE(0U, data_streams[0].ssrcs[0]);
  EXPECT_EQ(kDataTrack2, data_streams[1].id);
  ASSERT_EQ(1U, data_streams[1].ssrcs.size());
  EXPECT_NE(0U, data_streams[1].ssrcs[0]);

  EXPECT_EQ(cricket::kDataMaxBandwidth,
            dcd->bandwidth());   // default bandwidth (auto)
  EXPECT_TRUE(dcd->rtcp_mux());  // rtcp-mux defaults on
  ASSERT_CRYPTO(dcd, 1U, kDefaultSrtpCryptoSuite);

  // Update the offer. Add a new video track that is not synched to the
  // other tracks and replace audio track 2 with audio track 3.
  AttachSenderToMediaSection("video", MEDIA_TYPE_VIDEO, kVideoTrack2,
                             {kMediaStream2}, 1, &opts);
  DetachSenderFromMediaSection("audio", kAudioTrack2, &opts);
  AttachSenderToMediaSection("audio", MEDIA_TYPE_AUDIO, kAudioTrack3,
                             {kMediaStream1}, 1, &opts);
  DetachSenderFromMediaSection("data", kDataTrack2, &opts);
  AttachSenderToMediaSection("data", MEDIA_TYPE_DATA, kDataTrack3,
                             {kMediaStream1}, 1, &opts);
  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOffer(opts, offer.get()));

  ASSERT_TRUE(updated_offer.get() != NULL);
  ac = updated_offer->GetContentByName("audio");
  vc = updated_offer->GetContentByName("video");
  dc = updated_offer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  const AudioContentDescription* updated_acd =
      ac->media_description()->as_audio();
  const VideoContentDescription* updated_vcd =
      vc->media_description()->as_video();
  const DataContentDescription* updated_dcd =
      dc->media_description()->as_data();

  EXPECT_EQ(acd->type(), updated_acd->type());
  EXPECT_EQ(acd->codecs(), updated_acd->codecs());
  EXPECT_EQ(vcd->type(), updated_vcd->type());
  EXPECT_EQ(vcd->codecs(), updated_vcd->codecs());
  EXPECT_EQ(dcd->type(), updated_dcd->type());
  EXPECT_EQ(dcd->codecs(), updated_dcd->codecs());
  ASSERT_CRYPTO(updated_acd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_TRUE(CompareCryptoParams(acd->cryptos(), updated_acd->cryptos()));
  ASSERT_CRYPTO(updated_vcd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_TRUE(CompareCryptoParams(vcd->cryptos(), updated_vcd->cryptos()));
  ASSERT_CRYPTO(updated_dcd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_TRUE(CompareCryptoParams(dcd->cryptos(), updated_dcd->cryptos()));

  const StreamParamsVec& updated_audio_streams = updated_acd->streams();
  ASSERT_EQ(2U, updated_audio_streams.size());
  EXPECT_EQ(audio_streams[0], updated_audio_streams[0]);
  EXPECT_EQ(kAudioTrack3, updated_audio_streams[1].id);  // New audio track.
  ASSERT_EQ(1U, updated_audio_streams[1].ssrcs.size());
  EXPECT_NE(0U, updated_audio_streams[1].ssrcs[0]);
  EXPECT_EQ(updated_audio_streams[0].cname, updated_audio_streams[1].cname);

  const StreamParamsVec& updated_video_streams = updated_vcd->streams();
  ASSERT_EQ(2U, updated_video_streams.size());
  EXPECT_EQ(video_streams[0], updated_video_streams[0]);
  EXPECT_EQ(kVideoTrack2, updated_video_streams[1].id);
  // All the media streams in one PeerConnection share one RTCP CNAME.
  EXPECT_EQ(updated_video_streams[1].cname, updated_video_streams[0].cname);

  const StreamParamsVec& updated_data_streams = updated_dcd->streams();
  ASSERT_EQ(2U, updated_data_streams.size());
  EXPECT_EQ(data_streams[0], updated_data_streams[0]);
  EXPECT_EQ(kDataTrack3, updated_data_streams[1].id);  // New data track.
  ASSERT_EQ(1U, updated_data_streams[1].ssrcs.size());
  EXPECT_NE(0U, updated_data_streams[1].ssrcs[0]);
  EXPECT_EQ(updated_data_streams[0].cname, updated_data_streams[1].cname);
  // The stream correctly got the CNAME from the MediaSessionOptions.
  // The Expected RTCP CNAME is the default one as we are using the default
  // MediaSessionOptions.
  EXPECT_EQ(updated_data_streams[0].cname, cricket::kDefaultRtcpCname);
}

// Create an offer with simulcast video stream.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateSimulcastVideoOffer) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kSendRecv,
                  kActive, &opts);
  const int num_sim_layers = 3;
  AttachSenderToMediaSection("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                             {kMediaStream1}, num_sim_layers, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));

  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(vc != NULL);
  const VideoContentDescription* vcd = vc->media_description()->as_video();

  const StreamParamsVec& video_streams = vcd->streams();
  ASSERT_EQ(1U, video_streams.size());
  EXPECT_EQ(kVideoTrack1, video_streams[0].id);
  const SsrcGroup* sim_ssrc_group =
      video_streams[0].get_ssrc_group(cricket::kSimSsrcGroupSemantics);
  ASSERT_TRUE(sim_ssrc_group != NULL);
  EXPECT_EQ(static_cast<size_t>(num_sim_layers), sim_ssrc_group->ssrcs.size());
}

// Create an audio and video answer to a standard video offer with:
// - one video track
// - two audio tracks
// - two data tracks
// and ensure it matches what we expect. Also updates the initial answer by
// adding a new video track and removes one of the audio tracks.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateMultiStreamVideoAnswer) {
  MediaSessionOptions offer_opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &offer_opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kRecvOnly,
                  kActive, &offer_opts);
  offer_opts.data_channel_type = cricket::DCT_RTP;
  AddMediaSection(MEDIA_TYPE_DATA, "data", RtpTransceiverDirection::kRecvOnly,
                  kActive, &offer_opts);
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(offer_opts, NULL));

  MediaSessionOptions answer_opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kSendRecv,
                  kActive, &answer_opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kSendRecv,
                  kActive, &answer_opts);
  AttachSenderToMediaSection("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                             {kMediaStream1}, 1, &answer_opts);
  AttachSenderToMediaSection("audio", MEDIA_TYPE_AUDIO, kAudioTrack1,
                             {kMediaStream1}, 1, &answer_opts);
  AttachSenderToMediaSection("audio", MEDIA_TYPE_AUDIO, kAudioTrack2,
                             {kMediaStream1}, 1, &answer_opts);

  AddMediaSection(MEDIA_TYPE_DATA, "data", RtpTransceiverDirection::kSendRecv,
                  kActive, &answer_opts);
  AttachSenderToMediaSection("data", MEDIA_TYPE_DATA, kDataTrack1,
                             {kMediaStream1}, 1, &answer_opts);
  AttachSenderToMediaSection("data", MEDIA_TYPE_DATA, kDataTrack2,
                             {kMediaStream1}, 1, &answer_opts);
  answer_opts.data_channel_type = cricket::DCT_RTP;

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), answer_opts, NULL));

  ASSERT_TRUE(answer.get() != NULL);
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  const ContentInfo* dc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  const VideoContentDescription* vcd = vc->media_description()->as_video();
  const DataContentDescription* dcd = dc->media_description()->as_data();
  ASSERT_CRYPTO(acd, 1U, kDefaultSrtpCryptoSuite);
  ASSERT_CRYPTO(vcd, 1U, kDefaultSrtpCryptoSuite);
  ASSERT_CRYPTO(dcd, 1U, kDefaultSrtpCryptoSuite);

  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));

  const StreamParamsVec& audio_streams = acd->streams();
  ASSERT_EQ(2U, audio_streams.size());
  EXPECT_TRUE(audio_streams[0].cname == audio_streams[1].cname);
  EXPECT_EQ(kAudioTrack1, audio_streams[0].id);
  ASSERT_EQ(1U, audio_streams[0].ssrcs.size());
  EXPECT_NE(0U, audio_streams[0].ssrcs[0]);
  EXPECT_EQ(kAudioTrack2, audio_streams[1].id);
  ASSERT_EQ(1U, audio_streams[1].ssrcs.size());
  EXPECT_NE(0U, audio_streams[1].ssrcs[0]);

  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(acd->rtcp_mux());                 // rtcp-mux defaults on

  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_THAT(vcd->codecs(), ElementsAreArray(kVideoCodecsAnswer));

  const StreamParamsVec& video_streams = vcd->streams();
  ASSERT_EQ(1U, video_streams.size());
  EXPECT_EQ(video_streams[0].cname, audio_streams[0].cname);
  EXPECT_EQ(kVideoTrack1, video_streams[0].id);
  EXPECT_EQ(kAutoBandwidth, vcd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(vcd->rtcp_mux());                 // rtcp-mux defaults on

  EXPECT_EQ(MEDIA_TYPE_DATA, dcd->type());
  EXPECT_THAT(dcd->codecs(), ElementsAreArray(kDataCodecsAnswer));

  const StreamParamsVec& data_streams = dcd->streams();
  ASSERT_EQ(2U, data_streams.size());
  EXPECT_TRUE(data_streams[0].cname == data_streams[1].cname);
  EXPECT_EQ(kDataTrack1, data_streams[0].id);
  ASSERT_EQ(1U, data_streams[0].ssrcs.size());
  EXPECT_NE(0U, data_streams[0].ssrcs[0]);
  EXPECT_EQ(kDataTrack2, data_streams[1].id);
  ASSERT_EQ(1U, data_streams[1].ssrcs.size());
  EXPECT_NE(0U, data_streams[1].ssrcs[0]);

  EXPECT_EQ(cricket::kDataMaxBandwidth,
            dcd->bandwidth());   // default bandwidth (auto)
  EXPECT_TRUE(dcd->rtcp_mux());  // rtcp-mux defaults on

  // Update the answer. Add a new video track that is not synched to the
  // other tracks and remove 1 audio track.
  AttachSenderToMediaSection("video", MEDIA_TYPE_VIDEO, kVideoTrack2,
                             {kMediaStream2}, 1, &answer_opts);
  DetachSenderFromMediaSection("audio", kAudioTrack2, &answer_opts);
  DetachSenderFromMediaSection("data", kDataTrack2, &answer_opts);
  std::unique_ptr<SessionDescription> updated_answer(
      f2_.CreateAnswer(offer.get(), answer_opts, answer.get()));

  ASSERT_TRUE(updated_answer.get() != NULL);
  ac = updated_answer->GetContentByName("audio");
  vc = updated_answer->GetContentByName("video");
  dc = updated_answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  const AudioContentDescription* updated_acd =
      ac->media_description()->as_audio();
  const VideoContentDescription* updated_vcd =
      vc->media_description()->as_video();
  const DataContentDescription* updated_dcd =
      dc->media_description()->as_data();

  ASSERT_CRYPTO(updated_acd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_TRUE(CompareCryptoParams(acd->cryptos(), updated_acd->cryptos()));
  ASSERT_CRYPTO(updated_vcd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_TRUE(CompareCryptoParams(vcd->cryptos(), updated_vcd->cryptos()));
  ASSERT_CRYPTO(updated_dcd, 1U, kDefaultSrtpCryptoSuite);
  EXPECT_TRUE(CompareCryptoParams(dcd->cryptos(), updated_dcd->cryptos()));

  EXPECT_EQ(acd->type(), updated_acd->type());
  EXPECT_EQ(acd->codecs(), updated_acd->codecs());
  EXPECT_EQ(vcd->type(), updated_vcd->type());
  EXPECT_EQ(vcd->codecs(), updated_vcd->codecs());
  EXPECT_EQ(dcd->type(), updated_dcd->type());
  EXPECT_EQ(dcd->codecs(), updated_dcd->codecs());

  const StreamParamsVec& updated_audio_streams = updated_acd->streams();
  ASSERT_EQ(1U, updated_audio_streams.size());
  EXPECT_TRUE(audio_streams[0] == updated_audio_streams[0]);

  const StreamParamsVec& updated_video_streams = updated_vcd->streams();
  ASSERT_EQ(2U, updated_video_streams.size());
  EXPECT_EQ(video_streams[0], updated_video_streams[0]);
  EXPECT_EQ(kVideoTrack2, updated_video_streams[1].id);
  // All media streams in one PeerConnection share one CNAME.
  EXPECT_EQ(updated_video_streams[1].cname, updated_video_streams[0].cname);

  const StreamParamsVec& updated_data_streams = updated_dcd->streams();
  ASSERT_EQ(1U, updated_data_streams.size());
  EXPECT_TRUE(data_streams[0] == updated_data_streams[0]);
}

// Create an updated offer after creating an answer to the original offer and
// verify that the codecs that were part of the original answer are not changed
// in the updated offer.
TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(answer.get());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());
  EXPECT_THAT(vcd->codecs(), ElementsAreArray(kVideoCodecsAnswer));

  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOffer(opts, answer.get()));

  // The expected audio codecs are the common audio codecs from the first
  // offer/answer exchange plus the audio codecs only |f2_| offer, sorted in
  // preference order.
  // TODO(wu): |updated_offer| should not include the codec
  // (i.e. |kAudioCodecs2[0]|) the other side doesn't support.
  const AudioCodec kUpdatedAudioCodecOffer[] = {
      kAudioCodecsAnswer[0], kAudioCodecsAnswer[1], kAudioCodecs2[0],
  };

  // The expected video codecs are the common video codecs from the first
  // offer/answer exchange plus the video codecs only |f2_| offer, sorted in
  // preference order.
  const VideoCodec kUpdatedVideoCodecOffer[] = {
      kVideoCodecsAnswer[0], kVideoCodecs2[1],
  };

  const AudioContentDescription* updated_acd =
      GetFirstAudioContentDescription(updated_offer.get());
  EXPECT_THAT(updated_acd->codecs(), ElementsAreArray(kUpdatedAudioCodecOffer));

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  EXPECT_THAT(updated_vcd->codecs(), ElementsAreArray(kUpdatedVideoCodecOffer));
}

// Create an updated offer after creating an answer to the original offer and
// verify that the codecs that were part of the original answer are not changed
// in the updated offer. In this test Rtx is enabled.
TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswerWithRtx) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);
  // This creates rtx for H264 with the payload type |f1_| uses.
  AddRtxCodec(VideoCodec::CreateRtxCodec(126, kVideoCodecs1[1].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);
  // This creates rtx for H264 with the payload type |f2_| uses.
  AddRtxCodec(VideoCodec::CreateRtxCodec(125, kVideoCodecs2[0].id), &f2_codecs);
  f2_.set_video_codecs(f2_codecs);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());

  std::vector<VideoCodec> expected_codecs = MAKE_VECTOR(kVideoCodecsAnswer);
  AddRtxCodec(VideoCodec::CreateRtxCodec(126, kVideoCodecs1[1].id),
              &expected_codecs);

  EXPECT_EQ(expected_codecs, vcd->codecs());

  // Now, make sure we get same result (except for the order) if |f2_| creates
  // an updated offer even though the default payload types between |f1_| and
  // |f2_| are different.
  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOffer(opts, answer.get()));
  ASSERT_TRUE(updated_offer);
  std::unique_ptr<SessionDescription> updated_answer(
      f1_.CreateAnswer(updated_offer.get(), opts, answer.get()));

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_answer.get());

  EXPECT_EQ(expected_codecs, updated_vcd->codecs());
}

// Regression test for:
// https://bugs.chromium.org/p/webrtc/issues/detail?id=8332
// Existing codecs should always appear before new codecs in re-offers. But
// under a specific set of circumstances, the existing RTX codec was ending up
// added to the end of the list.
TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswerWithRemappedRtxPayloadType) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  // We specifically choose different preferred payload types for VP8 to
  // trigger the issue.
  cricket::VideoCodec vp8_offerer(100, "VP8");
  cricket::VideoCodec vp8_offerer_rtx =
      VideoCodec::CreateRtxCodec(101, vp8_offerer.id);
  cricket::VideoCodec vp8_answerer(110, "VP8");
  cricket::VideoCodec vp8_answerer_rtx =
      VideoCodec::CreateRtxCodec(111, vp8_answerer.id);
  cricket::VideoCodec vp9(120, "VP9");
  cricket::VideoCodec vp9_rtx = VideoCodec::CreateRtxCodec(121, vp9.id);

  std::vector<VideoCodec> f1_codecs = {vp8_offerer, vp8_offerer_rtx};
  // We also specifically cause the answerer to prefer VP9, such that if it
  // *doesn't* honor the existing preferred codec (VP8) we'll notice.
  std::vector<VideoCodec> f2_codecs = {vp9, vp9_rtx, vp8_answerer,
                                       vp8_answerer_rtx};

  f1_.set_video_codecs(f1_codecs);
  f2_.set_video_codecs(f2_codecs);
  std::vector<AudioCodec> audio_codecs;
  f1_.set_audio_codecs(audio_codecs, audio_codecs);
  f2_.set_audio_codecs(audio_codecs, audio_codecs);

  // Offer will be {VP8, RTX for VP8}. Answer will be the same.
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  // Updated offer *should* be {VP8, RTX for VP8, VP9, RTX for VP9}.
  // But if the bug is triggered, RTX for VP8 ends up last.
  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOffer(opts, answer.get()));

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  std::vector<cricket::VideoCodec> codecs = vcd->codecs();
  ASSERT_EQ(4u, codecs.size());
  EXPECT_EQ(vp8_offerer, codecs[0]);
  EXPECT_EQ(vp8_offerer_rtx, codecs[1]);
  EXPECT_EQ(vp9, codecs[2]);
  EXPECT_EQ(vp9_rtx, codecs[3]);
}

// Create an updated offer that adds video after creating an audio only answer
// to the original offer. This test verifies that if a video codec and the RTX
// codec have the same default payload type as an audio codec that is already in
// use, the added codecs payload types are changed.
TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferWithVideoAndRtxAfterCreatingAudioAnswer) {
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);
  // This creates rtx for H264 with the payload type |f1_| uses.
  AddRtxCodec(VideoCodec::CreateRtxCodec(126, kVideoCodecs1[1].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs);

  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(answer.get());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));

  // Now - let |f2_| add video with RTX and let the payload type the RTX codec
  // reference  be the same as an audio codec that was negotiated in the
  // first offer/answer exchange.
  opts.media_description_options.clear();
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);
  int used_pl_type = acd->codecs()[0].id;
  f2_codecs[0].id = used_pl_type;  // Set the payload type for H264.
  AddRtxCodec(VideoCodec::CreateRtxCodec(125, used_pl_type), &f2_codecs);
  f2_.set_video_codecs(f2_codecs);

  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOffer(opts, answer.get()));
  ASSERT_TRUE(updated_offer);
  std::unique_ptr<SessionDescription> updated_answer(
      f1_.CreateAnswer(updated_offer.get(), opts, answer.get()));

  const AudioContentDescription* updated_acd =
      GetFirstAudioContentDescription(answer.get());
  EXPECT_THAT(updated_acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_answer.get());

  ASSERT_EQ("H264", updated_vcd->codecs()[0].name);
  ASSERT_EQ(cricket::kRtxCodecName, updated_vcd->codecs()[1].name);
  int new_h264_pl_type = updated_vcd->codecs()[0].id;
  EXPECT_NE(used_pl_type, new_h264_pl_type);
  VideoCodec rtx = updated_vcd->codecs()[1];
  int pt_referenced_by_rtx = rtc::FromString<int>(
      rtx.params[cricket::kCodecParamAssociatedPayloadType]);
  EXPECT_EQ(new_h264_pl_type, pt_referenced_by_rtx);
}

// Create an updated offer with RTX after creating an answer to an offer
// without RTX, and with different default payload types.
// Verify that the added RTX codec references the correct payload type.
TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferWithRtxAfterCreatingAnswerWithoutRtx) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);
  // This creates rtx for H264 with the payload type |f2_| uses.
  AddRtxCodec(VideoCodec::CreateRtxCodec(125, kVideoCodecs2[0].id), &f2_codecs);
  f2_.set_video_codecs(f2_codecs);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer.get() != nullptr);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, nullptr));

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());

  std::vector<VideoCodec> expected_codecs = MAKE_VECTOR(kVideoCodecsAnswer);
  EXPECT_EQ(expected_codecs, vcd->codecs());

  // Now, ensure that the RTX codec is created correctly when |f2_| creates an
  // updated offer, even though the default payload types are different from
  // those of |f1_|.
  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOffer(opts, answer.get()));
  ASSERT_TRUE(updated_offer);

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_offer.get());

  // New offer should attempt to add H263, and RTX for H264.
  expected_codecs.push_back(kVideoCodecs2[1]);
  AddRtxCodec(VideoCodec::CreateRtxCodec(125, kVideoCodecs1[1].id),
              &expected_codecs);
  EXPECT_EQ(expected_codecs, updated_vcd->codecs());
}

// Test that RTX is ignored when there is no associated payload type parameter.
TEST_F(MediaSessionDescriptionFactoryTest, RtxWithoutApt) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);
  // This creates RTX without associated payload type parameter.
  AddRtxCodec(VideoCodec(126, cricket::kRtxCodecName), &f1_codecs);
  f1_.set_video_codecs(f1_codecs);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);
  // This creates RTX for H264 with the payload type |f2_| uses.
  AddRtxCodec(VideoCodec::CreateRtxCodec(125, kVideoCodecs2[0].id), &f2_codecs);
  f2_.set_video_codecs(f2_codecs);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  // kCodecParamAssociatedPayloadType will always be added to the offer when RTX
  // is selected. Manually remove kCodecParamAssociatedPayloadType so that it
  // is possible to test that that RTX is dropped when
  // kCodecParamAssociatedPayloadType is missing in the offer.
  MediaContentDescription* media_desc =
      offer->GetContentDescriptionByName(cricket::CN_VIDEO);
  ASSERT_TRUE(media_desc);
  VideoContentDescription* desc = media_desc->as_video();
  std::vector<VideoCodec> codecs = desc->codecs();
  for (VideoCodec& codec : codecs) {
    if (codec.name.find(cricket::kRtxCodecName) == 0) {
      codec.params.clear();
    }
  }
  desc->set_codecs(codecs);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  std::vector<std::string> codec_names =
      GetCodecNames(GetFirstVideoContentDescription(answer.get())->codecs());
  EXPECT_EQ(codec_names.end(), std::find(codec_names.begin(), codec_names.end(),
                                         cricket::kRtxCodecName));
}

// Test that RTX will be filtered out in the answer if its associated payload
// type doesn't match the local value.
TEST_F(MediaSessionDescriptionFactoryTest, FilterOutRtxIfAptDoesntMatch) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);
  // This creates RTX for H264 in sender.
  AddRtxCodec(VideoCodec::CreateRtxCodec(126, kVideoCodecs1[1].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);
  // This creates RTX for H263 in receiver.
  AddRtxCodec(VideoCodec::CreateRtxCodec(125, kVideoCodecs2[1].id), &f2_codecs);
  f2_.set_video_codecs(f2_codecs);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  // Associated payload type doesn't match, therefore, RTX codec is removed in
  // the answer.
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  std::vector<std::string> codec_names =
      GetCodecNames(GetFirstVideoContentDescription(answer.get())->codecs());
  EXPECT_EQ(codec_names.end(), std::find(codec_names.begin(), codec_names.end(),
                                         cricket::kRtxCodecName));
}

// Test that when multiple RTX codecs are offered, only the matched RTX codec
// is added in the answer, and the unsupported RTX codec is filtered out.
TEST_F(MediaSessionDescriptionFactoryTest,
       FilterOutUnsupportedRtxWhenCreatingAnswer) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);
  // This creates RTX for H264-SVC in sender.
  AddRtxCodec(VideoCodec::CreateRtxCodec(125, kVideoCodecs1[0].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs);

  // This creates RTX for H264 in sender.
  AddRtxCodec(VideoCodec::CreateRtxCodec(126, kVideoCodecs1[1].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);
  // This creates RTX for H264 in receiver.
  AddRtxCodec(VideoCodec::CreateRtxCodec(124, kVideoCodecs2[0].id), &f2_codecs);
  f2_.set_video_codecs(f2_codecs);

  // H264-SVC codec is removed in the answer, therefore, associated RTX codec
  // for H264-SVC should also be removed.
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());
  std::vector<VideoCodec> expected_codecs = MAKE_VECTOR(kVideoCodecsAnswer);
  AddRtxCodec(VideoCodec::CreateRtxCodec(126, kVideoCodecs1[1].id),
              &expected_codecs);

  EXPECT_EQ(expected_codecs, vcd->codecs());
}

// Test that after one RTX codec has been negotiated, a new offer can attempt
// to add another.
TEST_F(MediaSessionDescriptionFactoryTest, AddSecondRtxInNewOffer) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kRecvOnly,
                  kActive, &opts);
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);
  // This creates RTX for H264 for the offerer.
  AddRtxCodec(VideoCodec::CreateRtxCodec(126, kVideoCodecs1[1].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer);
  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(offer.get());

  std::vector<VideoCodec> expected_codecs = MAKE_VECTOR(kVideoCodecs1);
  AddRtxCodec(VideoCodec::CreateRtxCodec(126, kVideoCodecs1[1].id),
              &expected_codecs);
  EXPECT_EQ(expected_codecs, vcd->codecs());

  // Now, attempt to add RTX for H264-SVC.
  AddRtxCodec(VideoCodec::CreateRtxCodec(125, kVideoCodecs1[0].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs);

  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOffer(opts, offer.get()));
  ASSERT_TRUE(updated_offer);
  vcd = GetFirstVideoContentDescription(updated_offer.get());

  AddRtxCodec(VideoCodec::CreateRtxCodec(125, kVideoCodecs1[0].id),
              &expected_codecs);
  EXPECT_EQ(expected_codecs, vcd->codecs());
}

// Test that when RTX is used in conjunction with simulcast, an RTX ssrc is
// generated for each simulcast ssrc and correctly grouped.
TEST_F(MediaSessionDescriptionFactoryTest, SimSsrcsGenerateMultipleRtxSsrcs) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kSendRecv,
                  kActive, &opts);
  // Add simulcast streams.
  AttachSenderToMediaSection("video", MEDIA_TYPE_VIDEO, "stream1",
                             {"stream1label"}, 3, &opts);

  // Use a single real codec, and then add RTX for it.
  std::vector<VideoCodec> f1_codecs;
  f1_codecs.push_back(VideoCodec(97, "H264"));
  AddRtxCodec(VideoCodec::CreateRtxCodec(125, 97), &f1_codecs);
  f1_.set_video_codecs(f1_codecs);

  // Ensure that the offer has an RTX ssrc for each regular ssrc, and that there
  // is a FID ssrc + grouping for each.
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  MediaContentDescription* media_desc =
      offer->GetContentDescriptionByName(cricket::CN_VIDEO);
  ASSERT_TRUE(media_desc);
  VideoContentDescription* desc = media_desc->as_video();
  const StreamParamsVec& streams = desc->streams();
  // Single stream.
  ASSERT_EQ(1u, streams.size());
  // Stream should have 6 ssrcs: 3 for video, 3 for RTX.
  EXPECT_EQ(6u, streams[0].ssrcs.size());
  // And should have a SIM group for the simulcast.
  EXPECT_TRUE(streams[0].has_ssrc_group("SIM"));
  // And a FID group for RTX.
  EXPECT_TRUE(streams[0].has_ssrc_group("FID"));
  std::vector<uint32_t> primary_ssrcs;
  streams[0].GetPrimarySsrcs(&primary_ssrcs);
  EXPECT_EQ(3u, primary_ssrcs.size());
  std::vector<uint32_t> fid_ssrcs;
  streams[0].GetFidSsrcs(primary_ssrcs, &fid_ssrcs);
  EXPECT_EQ(3u, fid_ssrcs.size());
}

// Test that, when the FlexFEC codec is added, a FlexFEC ssrc is created
// together with a FEC-FR grouping.
TEST_F(MediaSessionDescriptionFactoryTest, GenerateFlexfecSsrc) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kSendRecv,
                  kActive, &opts);
  // Add single stream.
  AttachSenderToMediaSection("video", MEDIA_TYPE_VIDEO, "stream1",
                             {"stream1label"}, 1, &opts);

  // Use a single real codec, and then add FlexFEC for it.
  std::vector<VideoCodec> f1_codecs;
  f1_codecs.push_back(VideoCodec(97, "H264"));
  f1_codecs.push_back(VideoCodec(118, "flexfec-03"));
  f1_.set_video_codecs(f1_codecs);

  // Ensure that the offer has a single FlexFEC ssrc and that
  // there is no FEC-FR ssrc + grouping for each.
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer.get() != nullptr);
  MediaContentDescription* media_desc =
      offer->GetContentDescriptionByName(cricket::CN_VIDEO);
  ASSERT_TRUE(media_desc);
  VideoContentDescription* desc = media_desc->as_video();
  const StreamParamsVec& streams = desc->streams();
  // Single stream.
  ASSERT_EQ(1u, streams.size());
  // Stream should have 2 ssrcs: 1 for video, 1 for FlexFEC.
  EXPECT_EQ(2u, streams[0].ssrcs.size());
  // And should have a FEC-FR group for FlexFEC.
  EXPECT_TRUE(streams[0].has_ssrc_group("FEC-FR"));
  std::vector<uint32_t> primary_ssrcs;
  streams[0].GetPrimarySsrcs(&primary_ssrcs);
  ASSERT_EQ(1u, primary_ssrcs.size());
  uint32_t flexfec_ssrc;
  EXPECT_TRUE(streams[0].GetFecFrSsrc(primary_ssrcs[0], &flexfec_ssrc));
  EXPECT_NE(flexfec_ssrc, 0u);
}

// Test that FlexFEC is disabled for simulcast.
// TODO(brandtr): Remove this test when we support simulcast, either through
// multiple FlexfecSenders, or through multistream protection.
TEST_F(MediaSessionDescriptionFactoryTest, SimSsrcsGenerateNoFlexfecSsrcs) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kSendRecv,
                  kActive, &opts);
  // Add simulcast streams.
  AttachSenderToMediaSection("video", MEDIA_TYPE_VIDEO, "stream1",
                             {"stream1label"}, 3, &opts);

  // Use a single real codec, and then add FlexFEC for it.
  std::vector<VideoCodec> f1_codecs;
  f1_codecs.push_back(VideoCodec(97, "H264"));
  f1_codecs.push_back(VideoCodec(118, "flexfec-03"));
  f1_.set_video_codecs(f1_codecs);

  // Ensure that the offer has no FlexFEC ssrcs for each regular ssrc, and that
  // there is no FEC-FR ssrc + grouping for each.
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer.get() != nullptr);
  MediaContentDescription* media_desc =
      offer->GetContentDescriptionByName(cricket::CN_VIDEO);
  ASSERT_TRUE(media_desc);
  VideoContentDescription* desc = media_desc->as_video();
  const StreamParamsVec& streams = desc->streams();
  // Single stream.
  ASSERT_EQ(1u, streams.size());
  // Stream should have 3 ssrcs: 3 for video, 0 for FlexFEC.
  EXPECT_EQ(3u, streams[0].ssrcs.size());
  // And should have a SIM group for the simulcast.
  EXPECT_TRUE(streams[0].has_ssrc_group("SIM"));
  // And not a FEC-FR group for FlexFEC.
  EXPECT_FALSE(streams[0].has_ssrc_group("FEC-FR"));
  std::vector<uint32_t> primary_ssrcs;
  streams[0].GetPrimarySsrcs(&primary_ssrcs);
  EXPECT_EQ(3u, primary_ssrcs.size());
  for (uint32_t primary_ssrc : primary_ssrcs) {
    uint32_t flexfec_ssrc;
    EXPECT_FALSE(streams[0].GetFecFrSsrc(primary_ssrc, &flexfec_ssrc));
  }
}

// Create an updated offer after creating an answer to the original offer and
// verify that the RTP header extensions that were part of the original answer
// are not changed in the updated offer.
TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswerWithRtpExtensions) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  f1_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension1));
  f1_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension1));
  f2_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension2));
  f2_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension2));

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionAnswer),
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionAnswer),
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());

  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOffer(opts, answer.get()));

  // The expected RTP header extensions in the new offer are the resulting
  // extensions from the first offer/answer exchange plus the extensions only
  // |f2_| offer.
  // Since the default local extension id |f2_| uses has already been used by
  // |f1_| for another extensions, it is changed to 13.
  const RtpExtension kUpdatedAudioRtpExtensions[] = {
      kAudioRtpExtensionAnswer[0], RtpExtension(kAudioRtpExtension2[1].uri, 13),
      kAudioRtpExtension2[2],
  };

  // Since the default local extension id |f2_| uses has already been used by
  // |f1_| for another extensions, is is changed to 12.
  const RtpExtension kUpdatedVideoRtpExtensions[] = {
      kVideoRtpExtensionAnswer[0], RtpExtension(kVideoRtpExtension2[1].uri, 12),
      kVideoRtpExtension2[2],
  };

  const AudioContentDescription* updated_acd =
      GetFirstAudioContentDescription(updated_offer.get());
  EXPECT_EQ(MAKE_VECTOR(kUpdatedAudioRtpExtensions),
            updated_acd->rtp_header_extensions());

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  EXPECT_EQ(MAKE_VECTOR(kUpdatedVideoRtpExtensions),
            updated_vcd->rtp_header_extensions());
}

// Verify that if the same RTP extension URI is used for audio and video, the
// same ID is used. Also verify that the ID isn't changed when creating an
// updated offer (this was previously a bug).
TEST_F(MediaSessionDescriptionFactoryTest, RtpExtensionIdReused) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  f1_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension3));
  f1_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension3));

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));

  // Since the audio extensions used ID 3 for "both_audio_and_video", so should
  // the video extensions.
  const RtpExtension kExpectedVideoRtpExtension[] = {
      kVideoRtpExtension3[0], kAudioRtpExtension3[1],
  };

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtension3),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kExpectedVideoRtpExtension),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());

  // Nothing should change when creating a new offer
  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOffer(opts, offer.get()));

  EXPECT_EQ(MAKE_VECTOR(kAudioRtpExtension3),
            GetFirstAudioContentDescription(updated_offer.get())
                ->rtp_header_extensions());
  EXPECT_EQ(MAKE_VECTOR(kExpectedVideoRtpExtension),
            GetFirstVideoContentDescription(updated_offer.get())
                ->rtp_header_extensions());
}

// Same as "RtpExtensionIdReused" above for encrypted RTP extensions.
TEST_F(MediaSessionDescriptionFactoryTest, RtpExtensionIdReusedEncrypted) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  f1_.set_enable_encrypted_rtp_header_extensions(true);
  f2_.set_enable_encrypted_rtp_header_extensions(true);

  f1_.set_audio_rtp_header_extensions(
      MAKE_VECTOR(kAudioRtpExtension3ForEncryption));
  f1_.set_video_rtp_header_extensions(
      MAKE_VECTOR(kVideoRtpExtension3ForEncryption));

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));

  // The extensions that are shared between audio and video should use the same
  // id.
  const RtpExtension kExpectedVideoRtpExtension[] = {
      kVideoRtpExtension3ForEncryption[0],
      kAudioRtpExtension3ForEncryptionOffer[1],
      kAudioRtpExtension3ForEncryptionOffer[2],
  };

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtension3ForEncryptionOffer),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kExpectedVideoRtpExtension),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());

  // Nothing should change when creating a new offer
  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOffer(opts, offer.get()));

  EXPECT_EQ(MAKE_VECTOR(kAudioRtpExtension3ForEncryptionOffer),
            GetFirstAudioContentDescription(updated_offer.get())
                ->rtp_header_extensions());
  EXPECT_EQ(MAKE_VECTOR(kExpectedVideoRtpExtension),
            GetFirstVideoContentDescription(updated_offer.get())
                ->rtp_header_extensions());
}

TEST(MediaSessionDescription, CopySessionDescription) {
  SessionDescription source;
  cricket::ContentGroup group(cricket::CN_AUDIO);
  source.AddGroup(group);
  AudioContentDescription* acd(new AudioContentDescription());
  acd->set_codecs(MAKE_VECTOR(kAudioCodecs1));
  acd->AddLegacyStream(1);
  source.AddContent(cricket::CN_AUDIO, MediaProtocolType::kRtp, acd);
  VideoContentDescription* vcd(new VideoContentDescription());
  vcd->set_codecs(MAKE_VECTOR(kVideoCodecs1));
  vcd->AddLegacyStream(2);
  source.AddContent(cricket::CN_VIDEO, MediaProtocolType::kRtp, vcd);

  std::unique_ptr<SessionDescription> copy(source.Copy());
  ASSERT_TRUE(copy.get() != NULL);
  EXPECT_TRUE(copy->HasGroup(cricket::CN_AUDIO));
  const ContentInfo* ac = copy->GetContentByName("audio");
  const ContentInfo* vc = copy->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  const AudioContentDescription* acd_copy = ac->media_description()->as_audio();
  EXPECT_EQ(acd->codecs(), acd_copy->codecs());
  EXPECT_EQ(1u, acd->first_ssrc());

  EXPECT_EQ(MediaProtocolType::kRtp, vc->type);
  const VideoContentDescription* vcd_copy = vc->media_description()->as_video();
  EXPECT_EQ(vcd->codecs(), vcd_copy->codecs());
  EXPECT_EQ(2u, vcd->first_ssrc());
}

// The below TestTransportInfoXXX tests create different offers/answers, and
// ensure the TransportInfo in the SessionDescription matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferAudio) {
  MediaSessionOptions options;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &options);
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoOfferIceRenomination) {
  MediaSessionOptions options;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &options);
  options.media_description_options[0]
      .transport_options.enable_ice_renomination = true;
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferAudioCurrent) {
  MediaSessionOptions options;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &options);
  TestTransportInfo(true, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferMultimedia) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                 &options);
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoOfferMultimediaCurrent) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                 &options);
  TestTransportInfo(true, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferBundle) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                 &options);
  options.bundle_enabled = true;
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoOfferBundleCurrent) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                 &options);
  options.bundle_enabled = true;
  TestTransportInfo(true, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoAnswerAudio) {
  MediaSessionOptions options;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &options);
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoAnswerIceRenomination) {
  MediaSessionOptions options;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &options);
  options.media_description_options[0]
      .transport_options.enable_ice_renomination = true;
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoAnswerAudioCurrent) {
  MediaSessionOptions options;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kRecvOnly,
                  kActive, &options);
  TestTransportInfo(false, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoAnswerMultimedia) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                 &options);
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoAnswerMultimediaCurrent) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                 &options);
  TestTransportInfo(false, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoAnswerBundle) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                 &options);
  options.bundle_enabled = true;
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoAnswerBundleCurrent) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                 &options);
  options.bundle_enabled = true;
  TestTransportInfo(false, options, true);
}

// Create an offer with bundle enabled and verify the crypto parameters are
// the common set of the available cryptos.
TEST_F(MediaSessionDescriptionFactoryTest, TestCryptoWithOfferBundle) {
  TestCryptoWithBundle(true);
}

// Create an answer with bundle enabled and verify the crypto parameters are
// the common set of the available cryptos.
TEST_F(MediaSessionDescriptionFactoryTest, TestCryptoWithAnswerBundle) {
  TestCryptoWithBundle(false);
}

// Verifies that creating answer fails if the offer has UDP/TLS/RTP/SAVPF but
// DTLS is not enabled locally.
TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferDtlsSavpfWithoutDtlsFailed) {
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  tdf1_.set_secure(SEC_DISABLED);
  tdf2_.set_secure(SEC_DISABLED);

  std::unique_ptr<SessionDescription> offer(
      f1_.CreateOffer(CreatePlanBMediaSessionOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  ContentInfo* offer_content = offer->GetContentByName("audio");
  ASSERT_TRUE(offer_content != NULL);
  AudioContentDescription* offer_audio_desc =
      offer_content->media_description()->as_audio();
  offer_audio_desc->set_protocol(cricket::kMediaProtocolDtlsSavpf);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), CreatePlanBMediaSessionOptions(), NULL));
  ASSERT_TRUE(answer != NULL);
  ContentInfo* answer_content = answer->GetContentByName("audio");
  ASSERT_TRUE(answer_content != NULL);

  ASSERT_TRUE(answer_content->rejected);
}

// Offers UDP/TLS/RTP/SAVPF and verifies the answer can be created and contains
// UDP/TLS/RTP/SAVPF.
TEST_F(MediaSessionDescriptionFactoryTest, TestOfferDtlsSavpfCreateAnswer) {
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  tdf1_.set_secure(SEC_ENABLED);
  tdf2_.set_secure(SEC_ENABLED);

  std::unique_ptr<SessionDescription> offer(
      f1_.CreateOffer(CreatePlanBMediaSessionOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  ContentInfo* offer_content = offer->GetContentByName("audio");
  ASSERT_TRUE(offer_content != NULL);
  AudioContentDescription* offer_audio_desc =
      offer_content->media_description()->as_audio();
  offer_audio_desc->set_protocol(cricket::kMediaProtocolDtlsSavpf);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), CreatePlanBMediaSessionOptions(), NULL));
  ASSERT_TRUE(answer != NULL);

  const ContentInfo* answer_content = answer->GetContentByName("audio");
  ASSERT_TRUE(answer_content != NULL);
  ASSERT_FALSE(answer_content->rejected);

  const AudioContentDescription* answer_audio_desc =
      answer_content->media_description()->as_audio();
  EXPECT_EQ(cricket::kMediaProtocolDtlsSavpf, answer_audio_desc->protocol());
}

// Test that we include both SDES and DTLS in the offer, but only include SDES
// in the answer if DTLS isn't negotiated.
TEST_F(MediaSessionDescriptionFactoryTest, TestCryptoDtls) {
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  tdf1_.set_secure(SEC_ENABLED);
  tdf2_.set_secure(SEC_DISABLED);
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  std::unique_ptr<SessionDescription> offer, answer;
  const cricket::MediaContentDescription* audio_media_desc;
  const cricket::MediaContentDescription* video_media_desc;
  const cricket::TransportDescription* audio_trans_desc;
  const cricket::TransportDescription* video_trans_desc;

  // Generate an offer with SDES and DTLS support.
  offer.reset(f1_.CreateOffer(options, NULL));
  ASSERT_TRUE(offer.get() != NULL);

  audio_media_desc = offer->GetContentDescriptionByName("audio");
  ASSERT_TRUE(audio_media_desc != NULL);
  video_media_desc = offer->GetContentDescriptionByName("video");
  ASSERT_TRUE(video_media_desc != NULL);
  EXPECT_EQ(1u, audio_media_desc->cryptos().size());
  EXPECT_EQ(1u, video_media_desc->cryptos().size());

  audio_trans_desc = offer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_trans_desc != NULL);
  video_trans_desc = offer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_trans_desc != NULL);
  ASSERT_TRUE(audio_trans_desc->identity_fingerprint.get() != NULL);
  ASSERT_TRUE(video_trans_desc->identity_fingerprint.get() != NULL);

  // Generate an answer with only SDES support, since tdf2 has crypto disabled.
  answer.reset(f2_.CreateAnswer(offer.get(), options, NULL));
  ASSERT_TRUE(answer.get() != NULL);

  audio_media_desc = answer->GetContentDescriptionByName("audio");
  ASSERT_TRUE(audio_media_desc != NULL);
  video_media_desc = answer->GetContentDescriptionByName("video");
  ASSERT_TRUE(video_media_desc != NULL);
  EXPECT_EQ(1u, audio_media_desc->cryptos().size());
  EXPECT_EQ(1u, video_media_desc->cryptos().size());

  audio_trans_desc = answer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_trans_desc != NULL);
  video_trans_desc = answer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_trans_desc != NULL);
  ASSERT_TRUE(audio_trans_desc->identity_fingerprint.get() == NULL);
  ASSERT_TRUE(video_trans_desc->identity_fingerprint.get() == NULL);

  // Enable DTLS; the answer should now only have DTLS support.
  tdf2_.set_secure(SEC_ENABLED);
  answer.reset(f2_.CreateAnswer(offer.get(), options, NULL));
  ASSERT_TRUE(answer.get() != NULL);

  audio_media_desc = answer->GetContentDescriptionByName("audio");
  ASSERT_TRUE(audio_media_desc != NULL);
  video_media_desc = answer->GetContentDescriptionByName("video");
  ASSERT_TRUE(video_media_desc != NULL);
  EXPECT_TRUE(audio_media_desc->cryptos().empty());
  EXPECT_TRUE(video_media_desc->cryptos().empty());
  EXPECT_EQ(cricket::kMediaProtocolSavpf, audio_media_desc->protocol());
  EXPECT_EQ(cricket::kMediaProtocolSavpf, video_media_desc->protocol());

  audio_trans_desc = answer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_trans_desc != NULL);
  video_trans_desc = answer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_trans_desc != NULL);
  ASSERT_TRUE(audio_trans_desc->identity_fingerprint.get() != NULL);
  ASSERT_TRUE(video_trans_desc->identity_fingerprint.get() != NULL);

  // Try creating offer again. DTLS enabled now, crypto's should be empty
  // in new offer.
  offer.reset(f1_.CreateOffer(options, offer.get()));
  ASSERT_TRUE(offer.get() != NULL);
  audio_media_desc = offer->GetContentDescriptionByName("audio");
  ASSERT_TRUE(audio_media_desc != NULL);
  video_media_desc = offer->GetContentDescriptionByName("video");
  ASSERT_TRUE(video_media_desc != NULL);
  EXPECT_TRUE(audio_media_desc->cryptos().empty());
  EXPECT_TRUE(video_media_desc->cryptos().empty());

  audio_trans_desc = offer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_trans_desc != NULL);
  video_trans_desc = offer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_trans_desc != NULL);
  ASSERT_TRUE(audio_trans_desc->identity_fingerprint.get() != NULL);
  ASSERT_TRUE(video_trans_desc->identity_fingerprint.get() != NULL);
}

// Test that an answer can't be created if cryptos are required but the offer is
// unsecure.
TEST_F(MediaSessionDescriptionFactoryTest, TestSecureAnswerToUnsecureOffer) {
  MediaSessionOptions options = CreatePlanBMediaSessionOptions();
  f1_.set_secure(SEC_DISABLED);
  tdf1_.set_secure(SEC_DISABLED);
  f2_.set_secure(SEC_REQUIRED);
  tdf1_.set_secure(SEC_ENABLED);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(options, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), options, NULL));
  EXPECT_TRUE(answer.get() == NULL);
}

// Test that we accept a DTLS offer without SDES and create an appropriate
// answer.
TEST_F(MediaSessionDescriptionFactoryTest, TestCryptoOfferDtlsButNotSdes) {
  f1_.set_secure(SEC_DISABLED);
  f2_.set_secure(SEC_ENABLED);
  tdf1_.set_secure(SEC_ENABLED);
  tdf2_.set_secure(SEC_ENABLED);
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  AddDataSection(cricket::DCT_RTP, RtpTransceiverDirection::kRecvOnly,
                 &options);

  std::unique_ptr<SessionDescription> offer, answer;

  // Generate an offer with DTLS but without SDES.
  offer.reset(f1_.CreateOffer(options, NULL));
  ASSERT_TRUE(offer.get() != NULL);

  const AudioContentDescription* audio_offer =
      GetFirstAudioContentDescription(offer.get());
  ASSERT_TRUE(audio_offer->cryptos().empty());
  const VideoContentDescription* video_offer =
      GetFirstVideoContentDescription(offer.get());
  ASSERT_TRUE(video_offer->cryptos().empty());
  const DataContentDescription* data_offer =
      GetFirstDataContentDescription(offer.get());
  ASSERT_TRUE(data_offer->cryptos().empty());

  const cricket::TransportDescription* audio_offer_trans_desc =
      offer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_offer_trans_desc->identity_fingerprint.get() != NULL);
  const cricket::TransportDescription* video_offer_trans_desc =
      offer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_offer_trans_desc->identity_fingerprint.get() != NULL);
  const cricket::TransportDescription* data_offer_trans_desc =
      offer->GetTransportDescriptionByName("data");
  ASSERT_TRUE(data_offer_trans_desc->identity_fingerprint.get() != NULL);

  // Generate an answer with DTLS.
  answer.reset(f2_.CreateAnswer(offer.get(), options, NULL));
  ASSERT_TRUE(answer.get() != NULL);

  const cricket::TransportDescription* audio_answer_trans_desc =
      answer->GetTransportDescriptionByName("audio");
  EXPECT_TRUE(audio_answer_trans_desc->identity_fingerprint.get() != NULL);
  const cricket::TransportDescription* video_answer_trans_desc =
      answer->GetTransportDescriptionByName("video");
  EXPECT_TRUE(video_answer_trans_desc->identity_fingerprint.get() != NULL);
  const cricket::TransportDescription* data_answer_trans_desc =
      answer->GetTransportDescriptionByName("data");
  EXPECT_TRUE(data_answer_trans_desc->identity_fingerprint.get() != NULL);
}

// Verifies if vad_enabled option is set to false, CN codecs are not present in
// offer or answer.
TEST_F(MediaSessionDescriptionFactoryTest, TestVADEnableOption) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(options, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* audio_content = offer->GetContentByName("audio");
  EXPECT_FALSE(VerifyNoCNCodecs(audio_content));

  options.vad_enabled = false;
  offer.reset(f1_.CreateOffer(options, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  audio_content = offer->GetContentByName("audio");
  EXPECT_TRUE(VerifyNoCNCodecs(audio_content));
  std::unique_ptr<SessionDescription> answer(
      f1_.CreateAnswer(offer.get(), options, NULL));
  ASSERT_TRUE(answer.get() != NULL);
  audio_content = answer->GetContentByName("audio");
  EXPECT_TRUE(VerifyNoCNCodecs(audio_content));
}

// Test that the generated MIDs match the existing offer.
TEST_F(MediaSessionDescriptionFactoryTest, TestMIDsMatchesExistingOffer) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio_modified",
                  RtpTransceiverDirection::kRecvOnly, kActive, &opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video_modified",
                  RtpTransceiverDirection::kRecvOnly, kActive, &opts);
  opts.data_channel_type = cricket::DCT_SCTP;
  AddMediaSection(MEDIA_TYPE_DATA, "data_modified",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  // Create offer.
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOffer(opts, offer.get()));

  const ContentInfo* audio_content = GetFirstAudioContent(updated_offer.get());
  const ContentInfo* video_content = GetFirstVideoContent(updated_offer.get());
  const ContentInfo* data_content = GetFirstDataContent(updated_offer.get());
  ASSERT_TRUE(audio_content != nullptr);
  ASSERT_TRUE(video_content != nullptr);
  ASSERT_TRUE(data_content != nullptr);
  EXPECT_EQ("audio_modified", audio_content->name);
  EXPECT_EQ("video_modified", video_content->name);
  EXPECT_EQ("data_modified", data_content->name);
}

// The following tests verify that the unified plan SDP is supported.
// Test that we can create an offer with multiple media sections of same media
// type.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateOfferWithMultipleAVMediaSections) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio_1",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AttachSenderToMediaSection("audio_1", MEDIA_TYPE_AUDIO, kAudioTrack1,
                             {kMediaStream1}, 1, &opts);

  AddMediaSection(MEDIA_TYPE_VIDEO, "video_1",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AttachSenderToMediaSection("video_1", MEDIA_TYPE_VIDEO, kVideoTrack1,
                             {kMediaStream1}, 1, &opts);

  AddMediaSection(MEDIA_TYPE_AUDIO, "audio_2",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AttachSenderToMediaSection("audio_2", MEDIA_TYPE_AUDIO, kAudioTrack2,
                             {kMediaStream2}, 1, &opts);

  AddMediaSection(MEDIA_TYPE_VIDEO, "video_2",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AttachSenderToMediaSection("video_2", MEDIA_TYPE_VIDEO, kVideoTrack2,
                             {kMediaStream2}, 1, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer);

  ASSERT_EQ(4u, offer->contents().size());
  EXPECT_FALSE(offer->contents()[0].rejected);
  const AudioContentDescription* acd =
      offer->contents()[0].media_description()->as_audio();
  ASSERT_EQ(1u, acd->streams().size());
  EXPECT_EQ(kAudioTrack1, acd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, acd->direction());

  EXPECT_FALSE(offer->contents()[1].rejected);
  const VideoContentDescription* vcd =
      offer->contents()[1].media_description()->as_video();
  ASSERT_EQ(1u, vcd->streams().size());
  EXPECT_EQ(kVideoTrack1, vcd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, vcd->direction());

  EXPECT_FALSE(offer->contents()[2].rejected);
  acd = offer->contents()[2].media_description()->as_audio();
  ASSERT_EQ(1u, acd->streams().size());
  EXPECT_EQ(kAudioTrack2, acd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, acd->direction());

  EXPECT_FALSE(offer->contents()[3].rejected);
  vcd = offer->contents()[3].media_description()->as_video();
  ASSERT_EQ(1u, vcd->streams().size());
  EXPECT_EQ(kVideoTrack2, vcd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, vcd->direction());
}

// Test that we can create an answer with multiple media sections of same media
// type.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerWithMultipleAVMediaSections) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio_1",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AttachSenderToMediaSection("audio_1", MEDIA_TYPE_AUDIO, kAudioTrack1,
                             {kMediaStream1}, 1, &opts);

  AddMediaSection(MEDIA_TYPE_VIDEO, "video_1",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AttachSenderToMediaSection("video_1", MEDIA_TYPE_VIDEO, kVideoTrack1,
                             {kMediaStream1}, 1, &opts);

  AddMediaSection(MEDIA_TYPE_AUDIO, "audio_2",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AttachSenderToMediaSection("audio_2", MEDIA_TYPE_AUDIO, kAudioTrack2,
                             {kMediaStream2}, 1, &opts);

  AddMediaSection(MEDIA_TYPE_VIDEO, "video_2",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AttachSenderToMediaSection("video_2", MEDIA_TYPE_VIDEO, kVideoTrack2,
                             {kMediaStream2}, 1, &opts);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, nullptr));

  ASSERT_EQ(4u, answer->contents().size());
  EXPECT_FALSE(answer->contents()[0].rejected);
  const AudioContentDescription* acd =
      answer->contents()[0].media_description()->as_audio();
  ASSERT_EQ(1u, acd->streams().size());
  EXPECT_EQ(kAudioTrack1, acd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, acd->direction());

  EXPECT_FALSE(answer->contents()[1].rejected);
  const VideoContentDescription* vcd =
      answer->contents()[1].media_description()->as_video();
  ASSERT_EQ(1u, vcd->streams().size());
  EXPECT_EQ(kVideoTrack1, vcd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, vcd->direction());

  EXPECT_FALSE(answer->contents()[2].rejected);
  acd = answer->contents()[2].media_description()->as_audio();
  ASSERT_EQ(1u, acd->streams().size());
  EXPECT_EQ(kAudioTrack2, acd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, acd->direction());

  EXPECT_FALSE(answer->contents()[3].rejected);
  vcd = answer->contents()[3].media_description()->as_video();
  ASSERT_EQ(1u, vcd->streams().size());
  EXPECT_EQ(kVideoTrack2, vcd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, vcd->direction());
}

// Test that the media section will be rejected in offer if the corresponding
// MediaDescriptionOptions is stopped by the offerer.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateOfferWithMediaSectionStoppedByOfferer) {
  // Create an offer with two audio sections and one of them is stopped.
  MediaSessionOptions offer_opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio1",
                  RtpTransceiverDirection::kSendRecv, kActive, &offer_opts);
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio2",
                  RtpTransceiverDirection::kInactive, kStopped, &offer_opts);
  std::unique_ptr<SessionDescription> offer(
      f1_.CreateOffer(offer_opts, nullptr));
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  EXPECT_FALSE(offer->contents()[0].rejected);
  EXPECT_TRUE(offer->contents()[1].rejected);
}

// Test that the media section will be rejected in answer if the corresponding
// MediaDescriptionOptions is stopped by the offerer.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerWithMediaSectionStoppedByOfferer) {
  // Create an offer with two audio sections and one of them is stopped.
  MediaSessionOptions offer_opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio1",
                  RtpTransceiverDirection::kSendRecv, kActive, &offer_opts);
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio2",
                  RtpTransceiverDirection::kInactive, kStopped, &offer_opts);
  std::unique_ptr<SessionDescription> offer(
      f1_.CreateOffer(offer_opts, nullptr));
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  EXPECT_FALSE(offer->contents()[0].rejected);
  EXPECT_TRUE(offer->contents()[1].rejected);

  // Create an answer based on the offer.
  MediaSessionOptions answer_opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio1",
                  RtpTransceiverDirection::kSendRecv, kActive, &answer_opts);
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio2",
                  RtpTransceiverDirection::kSendRecv, kActive, &answer_opts);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), answer_opts, nullptr));
  ASSERT_EQ(2u, answer->contents().size());
  EXPECT_FALSE(answer->contents()[0].rejected);
  EXPECT_TRUE(answer->contents()[1].rejected);
}

// Test that the media section will be rejected in answer if the corresponding
// MediaDescriptionOptions is stopped by the answerer.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerWithMediaSectionRejectedByAnswerer) {
  // Create an offer with two audio sections.
  MediaSessionOptions offer_opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio1",
                  RtpTransceiverDirection::kSendRecv, kActive, &offer_opts);
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio2",
                  RtpTransceiverDirection::kSendRecv, kActive, &offer_opts);
  std::unique_ptr<SessionDescription> offer(
      f1_.CreateOffer(offer_opts, nullptr));
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  ASSERT_FALSE(offer->contents()[0].rejected);
  ASSERT_FALSE(offer->contents()[1].rejected);

  // The answerer rejects one of the audio sections.
  MediaSessionOptions answer_opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio1",
                  RtpTransceiverDirection::kSendRecv, kActive, &answer_opts);
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio2",
                  RtpTransceiverDirection::kInactive, kStopped, &answer_opts);
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), answer_opts, nullptr));
  ASSERT_EQ(2u, answer->contents().size());
  EXPECT_FALSE(answer->contents()[0].rejected);
  EXPECT_TRUE(answer->contents()[1].rejected);

  // The TransportInfo of the rejected m= section is expected to be added in the
  // answer.
  EXPECT_EQ(offer->transport_infos().size(), answer->transport_infos().size());
}

// Test the generated media sections has the same order of the
// corresponding MediaDescriptionOptions.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateOfferRespectsMediaDescriptionOptionsOrder) {
  MediaSessionOptions opts;
  // This tests put video section first because normally audio comes first by
  // default.
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kSendRecv,
                  kActive, &opts);
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kSendRecv,
                  kActive, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));

  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  EXPECT_EQ("video", offer->contents()[0].name);
  EXPECT_EQ("audio", offer->contents()[1].name);
}

// Test that different media sections using the same codec have same payload
// type.
TEST_F(MediaSessionDescriptionFactoryTest,
       PayloadTypesSharedByMediaSectionsOfSameType) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video1",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video2",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  // Create an offer with two video sections using same codecs.
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  const VideoContentDescription* vcd1 =
      offer->contents()[0].media_description()->as_video();
  const VideoContentDescription* vcd2 =
      offer->contents()[1].media_description()->as_video();
  EXPECT_EQ(vcd1->codecs().size(), vcd2->codecs().size());
  ASSERT_EQ(2u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].name, vcd2->codecs()[0].name);
  EXPECT_EQ(vcd1->codecs()[0].id, vcd2->codecs()[0].id);
  EXPECT_EQ(vcd1->codecs()[1].name, vcd2->codecs()[1].name);
  EXPECT_EQ(vcd1->codecs()[1].id, vcd2->codecs()[1].id);

  // Create answer and negotiate the codecs.
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, nullptr));
  ASSERT_TRUE(answer);
  ASSERT_EQ(2u, answer->contents().size());
  vcd1 = answer->contents()[0].media_description()->as_video();
  vcd2 = answer->contents()[1].media_description()->as_video();
  EXPECT_EQ(vcd1->codecs().size(), vcd2->codecs().size());
  ASSERT_EQ(1u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].name, vcd2->codecs()[0].name);
  EXPECT_EQ(vcd1->codecs()[0].id, vcd2->codecs()[0].id);
}

// Test that the codec preference order per media section is respected in
// subsequent offer.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateOfferRespectsCodecPreferenceOrder) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video1",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video2",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  // Create an offer with two video sections using same codecs.
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  VideoContentDescription* vcd1 =
      offer->contents()[0].media_description()->as_video();
  const VideoContentDescription* vcd2 =
      offer->contents()[1].media_description()->as_video();
  auto video_codecs = MAKE_VECTOR(kVideoCodecs1);
  EXPECT_EQ(video_codecs, vcd1->codecs());
  EXPECT_EQ(video_codecs, vcd2->codecs());

  // Change the codec preference of the first video section and create a
  // follow-up offer.
  auto video_codecs_reverse = MAKE_VECTOR(kVideoCodecs1Reverse);
  vcd1->set_codecs(video_codecs_reverse);
  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOffer(opts, offer.get()));
  vcd1 = updated_offer->contents()[0].media_description()->as_video();
  vcd2 = updated_offer->contents()[1].media_description()->as_video();
  // The video codec preference order should be respected.
  EXPECT_EQ(video_codecs_reverse, vcd1->codecs());
  EXPECT_EQ(video_codecs, vcd2->codecs());
}

// Test that the codec preference order per media section is respected in
// the answer.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerRespectsCodecPreferenceOrder) {
  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video1",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video2",
                  RtpTransceiverDirection::kSendRecv, kActive, &opts);
  // Create an offer with two video sections using same codecs.
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  VideoContentDescription* vcd1 =
      offer->contents()[0].media_description()->as_video();
  const VideoContentDescription* vcd2 =
      offer->contents()[1].media_description()->as_video();
  auto video_codecs = MAKE_VECTOR(kVideoCodecs1);
  EXPECT_EQ(video_codecs, vcd1->codecs());
  EXPECT_EQ(video_codecs, vcd2->codecs());

  // Change the codec preference of the first video section and create an
  // answer.
  auto video_codecs_reverse = MAKE_VECTOR(kVideoCodecs1Reverse);
  vcd1->set_codecs(video_codecs_reverse);
  std::unique_ptr<SessionDescription> answer(
      f1_.CreateAnswer(offer.get(), opts, nullptr));
  vcd1 = answer->contents()[0].media_description()->as_video();
  vcd2 = answer->contents()[1].media_description()->as_video();
  // The video codec preference order should be respected.
  EXPECT_EQ(video_codecs_reverse, vcd1->codecs());
  EXPECT_EQ(video_codecs, vcd2->codecs());
}

// Test that when creating an answer, the codecs use local parameters instead of
// the remote ones.
TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerWithLocalCodecParams) {
  const std::string audio_param_name = "audio_param";
  const std::string audio_value1 = "audio_v1";
  const std::string audio_value2 = "audio_v2";
  const std::string video_param_name = "video_param";
  const std::string video_value1 = "video_v1";
  const std::string video_value2 = "video_v2";

  auto audio_codecs1 = MAKE_VECTOR(kAudioCodecs1);
  auto audio_codecs2 = MAKE_VECTOR(kAudioCodecs1);
  auto video_codecs1 = MAKE_VECTOR(kVideoCodecs1);
  auto video_codecs2 = MAKE_VECTOR(kVideoCodecs1);

  // Set the parameters for codecs.
  audio_codecs1[0].SetParam(audio_param_name, audio_value1);
  video_codecs1[0].SetParam(video_param_name, video_value1);
  audio_codecs2[0].SetParam(audio_param_name, audio_value2);
  video_codecs2[0].SetParam(video_param_name, video_value2);

  f1_.set_audio_codecs(audio_codecs1, audio_codecs1);
  f1_.set_video_codecs(video_codecs1);
  f2_.set_audio_codecs(audio_codecs2, audio_codecs2);
  f2_.set_video_codecs(video_codecs2);

  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", RtpTransceiverDirection::kSendRecv,
                  kActive, &opts);
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kSendRecv,
                  kActive, &opts);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer);
  auto offer_acd = offer->contents()[0].media_description()->as_audio();
  auto offer_vcd = offer->contents()[1].media_description()->as_video();
  std::string value;
  EXPECT_TRUE(offer_acd->codecs()[0].GetParam(audio_param_name, &value));
  EXPECT_EQ(audio_value1, value);
  EXPECT_TRUE(offer_vcd->codecs()[0].GetParam(video_param_name, &value));
  EXPECT_EQ(video_value1, value);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, nullptr));
  ASSERT_TRUE(answer);
  auto answer_acd = answer->contents()[0].media_description()->as_audio();
  auto answer_vcd = answer->contents()[1].media_description()->as_video();
  // Use the parameters from the local codecs.
  EXPECT_TRUE(answer_acd->codecs()[0].GetParam(audio_param_name, &value));
  EXPECT_EQ(audio_value2, value);
  EXPECT_TRUE(answer_vcd->codecs()[0].GetParam(video_param_name, &value));
  EXPECT_EQ(video_value2, value);
}

// Test that matching packetization-mode is part of the criteria for matching
// H264 codecs (in addition to profile-level-id). Previously, this was not the
// case, so the first H264 codec with the same profile-level-id would match and
// the payload type in the answer would be incorrect.
// This is a regression test for bugs.webrtc.org/8808
TEST_F(MediaSessionDescriptionFactoryTest,
       H264MatchCriteriaIncludesPacketizationMode) {
  // Create two H264 codecs with the same profile level ID and different
  // packetization modes.
  VideoCodec h264_pm0(96, "H264");
  h264_pm0.params[cricket::kH264FmtpProfileLevelId] = "42c01f";
  h264_pm0.params[cricket::kH264FmtpPacketizationMode] = "0";
  VideoCodec h264_pm1(97, "H264");
  h264_pm1.params[cricket::kH264FmtpProfileLevelId] = "42c01f";
  h264_pm1.params[cricket::kH264FmtpPacketizationMode] = "1";

  // Offerer will send both codecs, answerer should choose the one with matching
  // packetization mode (and not the first one it sees).
  f1_.set_video_codecs({h264_pm0, h264_pm1});
  f2_.set_video_codecs({h264_pm1});

  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_VIDEO, "video", RtpTransceiverDirection::kSendRecv,
                  kActive, &opts);

  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, nullptr));
  ASSERT_TRUE(answer);

  // Answer should have one negotiated codec with packetization-mode=1 using the
  // offered payload type.
  ASSERT_EQ(1u, answer->contents().size());
  auto answer_vcd = answer->contents()[0].media_description()->as_video();
  ASSERT_EQ(1u, answer_vcd->codecs().size());
  auto answer_codec = answer_vcd->codecs()[0];
  EXPECT_EQ(h264_pm1.id, answer_codec.id);
}

class MediaProtocolTest : public ::testing::TestWithParam<const char*> {
 public:
  MediaProtocolTest() : f1_(&tdf1_), f2_(&tdf2_) {
    f1_.set_audio_codecs(MAKE_VECTOR(kAudioCodecs1),
                         MAKE_VECTOR(kAudioCodecs1));
    f1_.set_video_codecs(MAKE_VECTOR(kVideoCodecs1));
    f1_.set_data_codecs(MAKE_VECTOR(kDataCodecs1));
    f2_.set_audio_codecs(MAKE_VECTOR(kAudioCodecs2),
                         MAKE_VECTOR(kAudioCodecs2));
    f2_.set_video_codecs(MAKE_VECTOR(kVideoCodecs2));
    f2_.set_data_codecs(MAKE_VECTOR(kDataCodecs2));
    f1_.set_secure(SEC_ENABLED);
    f2_.set_secure(SEC_ENABLED);
    tdf1_.set_certificate(rtc::RTCCertificate::Create(
        std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("id1"))));
    tdf2_.set_certificate(rtc::RTCCertificate::Create(
        std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("id2"))));
    tdf1_.set_secure(SEC_ENABLED);
    tdf2_.set_secure(SEC_ENABLED);
  }

 protected:
  MediaSessionDescriptionFactory f1_;
  MediaSessionDescriptionFactory f2_;
  TransportDescriptionFactory tdf1_;
  TransportDescriptionFactory tdf2_;
};

TEST_P(MediaProtocolTest, TestAudioVideoAcceptance) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  std::unique_ptr<SessionDescription> offer(f1_.CreateOffer(opts, nullptr));
  ASSERT_TRUE(offer.get() != nullptr);
  // Set the protocol for all the contents.
  for (auto content : offer.get()->contents()) {
    content.media_description()->set_protocol(GetParam());
  }
  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, nullptr));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac != nullptr);
  ASSERT_TRUE(vc != nullptr);
  EXPECT_FALSE(ac->rejected);  // the offer is accepted
  EXPECT_FALSE(vc->rejected);
  const AudioContentDescription* acd = ac->media_description()->as_audio();
  const VideoContentDescription* vcd = vc->media_description()->as_video();
  EXPECT_EQ(GetParam(), acd->protocol());
  EXPECT_EQ(GetParam(), vcd->protocol());
}

INSTANTIATE_TEST_CASE_P(MediaProtocolPatternTest,
                        MediaProtocolTest,
                        ::testing::ValuesIn(kMediaProtocols));
INSTANTIATE_TEST_CASE_P(MediaProtocolDtlsPatternTest,
                        MediaProtocolTest,
                        ::testing::ValuesIn(kMediaProtocolsDtls));

TEST_F(MediaSessionDescriptionFactoryTest, TestSetAudioCodecs) {
  TransportDescriptionFactory tdf;
  MediaSessionDescriptionFactory sf(&tdf);
  std::vector<AudioCodec> send_codecs = MAKE_VECTOR(kAudioCodecs1);
  std::vector<AudioCodec> recv_codecs = MAKE_VECTOR(kAudioCodecs2);

  // The merged list of codecs should contain any send codecs that are also
  // nominally in the recieve codecs list. Payload types should be picked from
  // the send codecs and a number-of-channels of 0 and 1 should be equivalent
  // (set to 1). This equals what happens when the send codecs are used in an
  // offer and the receive codecs are used in the following answer.
  const std::vector<AudioCodec> sendrecv_codecs =
      MAKE_VECTOR(kAudioCodecsAnswer);
  const std::vector<AudioCodec> no_codecs;

  RTC_CHECK_EQ(send_codecs[1].name, "iLBC")
      << "Please don't change shared test data!";
  RTC_CHECK_EQ(recv_codecs[2].name, "iLBC")
      << "Please don't change shared test data!";
  // Alter iLBC send codec to have zero channels, to test that that is handled
  // properly.
  send_codecs[1].channels = 0;

  // Alther iLBC receive codec to be lowercase, to test that case conversions
  // are handled properly.
  recv_codecs[2].name = "ilbc";

  // Test proper merge
  sf.set_audio_codecs(send_codecs, recv_codecs);
  EXPECT_EQ(send_codecs, sf.audio_send_codecs());
  EXPECT_EQ(recv_codecs, sf.audio_recv_codecs());
  EXPECT_EQ(sendrecv_codecs, sf.audio_sendrecv_codecs());

  // Test empty send codecs list
  sf.set_audio_codecs(no_codecs, recv_codecs);
  EXPECT_EQ(no_codecs, sf.audio_send_codecs());
  EXPECT_EQ(recv_codecs, sf.audio_recv_codecs());
  EXPECT_EQ(no_codecs, sf.audio_sendrecv_codecs());

  // Test empty recv codecs list
  sf.set_audio_codecs(send_codecs, no_codecs);
  EXPECT_EQ(send_codecs, sf.audio_send_codecs());
  EXPECT_EQ(no_codecs, sf.audio_recv_codecs());
  EXPECT_EQ(no_codecs, sf.audio_sendrecv_codecs());

  // Test all empty codec lists
  sf.set_audio_codecs(no_codecs, no_codecs);
  EXPECT_EQ(no_codecs, sf.audio_send_codecs());
  EXPECT_EQ(no_codecs, sf.audio_recv_codecs());
  EXPECT_EQ(no_codecs, sf.audio_sendrecv_codecs());
}

namespace {
// Compare the two vectors of codecs ignoring the payload type.
template <class Codec>
bool CodecsMatch(const std::vector<Codec>& codecs1,
                 const std::vector<Codec>& codecs2) {
  if (codecs1.size() != codecs2.size()) {
    return false;
  }

  for (size_t i = 0; i < codecs1.size(); ++i) {
    if (!codecs1[i].Matches(codecs2[i])) {
      return false;
    }
  }
  return true;
}

void TestAudioCodecsOffer(RtpTransceiverDirection direction) {
  TransportDescriptionFactory tdf;
  MediaSessionDescriptionFactory sf(&tdf);
  const std::vector<AudioCodec> send_codecs = MAKE_VECTOR(kAudioCodecs1);
  const std::vector<AudioCodec> recv_codecs = MAKE_VECTOR(kAudioCodecs2);
  const std::vector<AudioCodec> sendrecv_codecs =
      MAKE_VECTOR(kAudioCodecsAnswer);
  sf.set_audio_codecs(send_codecs, recv_codecs);

  MediaSessionOptions opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", direction, kActive, &opts);

  if (direction == RtpTransceiverDirection::kSendRecv ||
      direction == RtpTransceiverDirection::kSendOnly) {
    AttachSenderToMediaSection("audio", MEDIA_TYPE_AUDIO, kAudioTrack1,
                               {kMediaStream1}, 1, &opts);
  }

  std::unique_ptr<SessionDescription> offer(sf.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  ContentInfo* ac = offer->GetContentByName("audio");

  // If the factory didn't add any audio content to the offer, we cannot check
  // that the codecs put in are right. This happens when we neither want to
  // send nor receive audio. The checks are still in place if at some point
  // we'd instead create an inactive stream.
  if (ac) {
    AudioContentDescription* acd = ac->media_description()->as_audio();
    // sendrecv and inactive should both present lists as if the channel was
    // to be used for sending and receiving. Inactive essentially means it
    // might eventually be used anything, but we don't know more at this
    // moment.
    if (acd->direction() == RtpTransceiverDirection::kSendOnly) {
      EXPECT_TRUE(CodecsMatch<AudioCodec>(send_codecs, acd->codecs()));
    } else if (acd->direction() == RtpTransceiverDirection::kRecvOnly) {
      EXPECT_TRUE(CodecsMatch<AudioCodec>(recv_codecs, acd->codecs()));
    } else {
      EXPECT_TRUE(CodecsMatch<AudioCodec>(sendrecv_codecs, acd->codecs()));
    }
  }
}

static const AudioCodec kOfferAnswerCodecs[] = {
    AudioCodec(0, "codec0", 16000, -1, 1),
    AudioCodec(1, "codec1", 8000, 13300, 1),
    AudioCodec(2, "codec2", 8000, 64000, 1),
    AudioCodec(3, "codec3", 8000, 64000, 1),
    AudioCodec(4, "codec4", 8000, 0, 2),
    AudioCodec(5, "codec5", 32000, 0, 1),
    AudioCodec(6, "codec6", 48000, 0, 1)};

/* The codecs groups below are chosen as per the matrix below. The objective
 * is to have different sets of codecs in the inputs, to get unique sets of
 * codecs after negotiation, depending on offer and answer communication
 * directions. One-way directions in the offer should either result in the
 * opposite direction in the answer, or an inactive answer. Regardless, the
 * choice of codecs should be as if the answer contained the opposite
 * direction. Inactive offers should be treated as sendrecv/sendrecv.
 *
 *         |     Offer   |      Answer  |         Result
 *    codec|send recv sr | send recv sr | s/r  r/s sr/s sr/r sr/sr
 *     0   | x    -    - |  -    x    - |  x    -    -    -    -
 *     1   | x    x    x |  -    x    - |  x    -    -    x    -
 *     2   | -    x    - |  x    -    - |  -    x    -    -    -
 *     3   | x    x    x |  x    -    - |  -    x    x    -    -
 *     4   | -    x    - |  x    x    x |  -    x    -    -    -
 *     5   | x    -    - |  x    x    x |  x    -    -    -    -
 *     6   | x    x    x |  x    x    x |  x    x    x    x    x
 */
// Codecs used by offerer in the AudioCodecsAnswerTest
static const int kOfferSendCodecs[] = {0, 1, 3, 5, 6};
static const int kOfferRecvCodecs[] = {1, 2, 3, 4, 6};
// Codecs used in the answerer in the AudioCodecsAnswerTest.  The order is
// jumbled to catch the answer not following the order in the offer.
static const int kAnswerSendCodecs[] = {6, 5, 2, 3, 4};
static const int kAnswerRecvCodecs[] = {6, 5, 4, 1, 0};
// The resulting sets of codecs in the answer in the AudioCodecsAnswerTest
static const int kResultSend_RecvCodecs[] = {0, 1, 5, 6};
static const int kResultRecv_SendCodecs[] = {2, 3, 4, 6};
static const int kResultSendrecv_SendCodecs[] = {3, 6};
static const int kResultSendrecv_RecvCodecs[] = {1, 6};
static const int kResultSendrecv_SendrecvCodecs[] = {6};

template <typename T, int IDXS>
std::vector<T> VectorFromIndices(const T* array, const int (&indices)[IDXS]) {
  std::vector<T> out;
  out.reserve(IDXS);
  for (int idx : indices)
    out.push_back(array[idx]);

  return out;
}

void TestAudioCodecsAnswer(RtpTransceiverDirection offer_direction,
                           RtpTransceiverDirection answer_direction,
                           bool add_legacy_stream) {
  TransportDescriptionFactory offer_tdf;
  TransportDescriptionFactory answer_tdf;
  MediaSessionDescriptionFactory offer_factory(&offer_tdf);
  MediaSessionDescriptionFactory answer_factory(&answer_tdf);
  offer_factory.set_audio_codecs(
      VectorFromIndices(kOfferAnswerCodecs, kOfferSendCodecs),
      VectorFromIndices(kOfferAnswerCodecs, kOfferRecvCodecs));
  answer_factory.set_audio_codecs(
      VectorFromIndices(kOfferAnswerCodecs, kAnswerSendCodecs),
      VectorFromIndices(kOfferAnswerCodecs, kAnswerRecvCodecs));

  MediaSessionOptions offer_opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", offer_direction, kActive,
                  &offer_opts);

  if (webrtc::RtpTransceiverDirectionHasSend(offer_direction)) {
    AttachSenderToMediaSection("audio", MEDIA_TYPE_AUDIO, kAudioTrack1,
                               {kMediaStream1}, 1, &offer_opts);
  }

  std::unique_ptr<SessionDescription> offer(
      offer_factory.CreateOffer(offer_opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);

  MediaSessionOptions answer_opts;
  AddMediaSection(MEDIA_TYPE_AUDIO, "audio", answer_direction, kActive,
                  &answer_opts);

  if (webrtc::RtpTransceiverDirectionHasSend(answer_direction)) {
    AttachSenderToMediaSection("audio", MEDIA_TYPE_AUDIO, kAudioTrack1,
                               {kMediaStream1}, 1, &answer_opts);
  }
  std::unique_ptr<SessionDescription> answer(
      answer_factory.CreateAnswer(offer.get(), answer_opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");

  // If the factory didn't add any audio content to the answer, we cannot
  // check that the codecs put in are right. This happens when we neither want
  // to send nor receive audio. The checks are still in place if at some point
  // we'd instead create an inactive stream.
  if (ac) {
    ASSERT_EQ(MEDIA_TYPE_AUDIO, ac->media_description()->type());
    const AudioContentDescription* acd = ac->media_description()->as_audio();

    std::vector<AudioCodec> target_codecs;
    // For offers with sendrecv or inactive, we should never reply with more
    // codecs than offered, with these codec sets.
    switch (offer_direction) {
      case RtpTransceiverDirection::kInactive:
        target_codecs = VectorFromIndices(kOfferAnswerCodecs,
                                          kResultSendrecv_SendrecvCodecs);
        break;
      case RtpTransceiverDirection::kSendOnly:
        target_codecs =
            VectorFromIndices(kOfferAnswerCodecs, kResultSend_RecvCodecs);
        break;
      case RtpTransceiverDirection::kRecvOnly:
        target_codecs =
            VectorFromIndices(kOfferAnswerCodecs, kResultRecv_SendCodecs);
        break;
      case RtpTransceiverDirection::kSendRecv:
        if (acd->direction() == RtpTransceiverDirection::kSendOnly) {
          target_codecs =
              VectorFromIndices(kOfferAnswerCodecs, kResultSendrecv_SendCodecs);
        } else if (acd->direction() == RtpTransceiverDirection::kRecvOnly) {
          target_codecs =
              VectorFromIndices(kOfferAnswerCodecs, kResultSendrecv_RecvCodecs);
        } else {
          target_codecs = VectorFromIndices(kOfferAnswerCodecs,
                                            kResultSendrecv_SendrecvCodecs);
        }
        break;
    }

    auto format_codecs = [](const std::vector<AudioCodec>& codecs) {
      rtc::StringBuilder os;
      bool first = true;
      os << "{";
      for (const auto& c : codecs) {
        os << (first ? " " : ", ") << c.id;
        first = false;
      }
      os << " }";
      return os.Release();
    };

    EXPECT_TRUE(acd->codecs() == target_codecs)
        << "Expected: " << format_codecs(target_codecs)
        << ", got: " << format_codecs(acd->codecs()) << "; Offered: "
        << webrtc::RtpTransceiverDirectionToString(offer_direction)
        << ", answerer wants: "
        << webrtc::RtpTransceiverDirectionToString(answer_direction)
        << "; got: "
        << webrtc::RtpTransceiverDirectionToString(acd->direction());
  } else {
    EXPECT_EQ(offer_direction, RtpTransceiverDirection::kInactive)
        << "Only inactive offers are allowed to not generate any audio "
           "content";
  }
}

}  // namespace

class AudioCodecsOfferTest
    : public ::testing::TestWithParam<RtpTransceiverDirection> {};

TEST_P(AudioCodecsOfferTest, TestCodecsInOffer) {
  TestAudioCodecsOffer(GetParam());
}

INSTANTIATE_TEST_CASE_P(MediaSessionDescriptionFactoryTest,
                        AudioCodecsOfferTest,
                        ::testing::Values(RtpTransceiverDirection::kSendOnly,
                                          RtpTransceiverDirection::kRecvOnly,
                                          RtpTransceiverDirection::kSendRecv,
                                          RtpTransceiverDirection::kInactive));

class AudioCodecsAnswerTest
    : public ::testing::TestWithParam<::testing::tuple<RtpTransceiverDirection,
                                                       RtpTransceiverDirection,
                                                       bool>> {};

TEST_P(AudioCodecsAnswerTest, TestCodecsInAnswer) {
  TestAudioCodecsAnswer(::testing::get<0>(GetParam()),
                        ::testing::get<1>(GetParam()),
                        ::testing::get<2>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    MediaSessionDescriptionFactoryTest,
    AudioCodecsAnswerTest,
    ::testing::Combine(::testing::Values(RtpTransceiverDirection::kSendOnly,
                                         RtpTransceiverDirection::kRecvOnly,
                                         RtpTransceiverDirection::kSendRecv,
                                         RtpTransceiverDirection::kInactive),
                       ::testing::Values(RtpTransceiverDirection::kSendOnly,
                                         RtpTransceiverDirection::kRecvOnly,
                                         RtpTransceiverDirection::kSendRecv,
                                         RtpTransceiverDirection::kInactive),
                       ::testing::Bool()));
