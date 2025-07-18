/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Integration tests that verify that certain properties remain the same
// over time.
// It is expected that these tests will have to be changed frequently.
// The error messages when the tests fail are intended to contain C++ code
// that can be pasted into the test when updating it.

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "pc/session_description.h"
#include "pc/test/integration_test_helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {

namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Not;

class FactorySignature {
 public:
  // Constant naming: kWebRtc* is for configurations that occur
  // on bots in the WebRTC repository. Other configurations are added
  // based on downstream products that need listing.
  enum class Id {
    kNotRecognized,
    kWebRtcTipOfTree,
    kWebRtcMoreConfigs1,
    kWebRtcAndroid,
    kGoogleInternal,
  };
  Id id() { return id_; }
  FactorySignature() {
    ExtractSignatureStrings();
    id_ = RecognizeSignature();
  }

 private:
  // Extract a set of strings characterizing the factory in use.
  void ExtractSignatureStrings() {
    scoped_refptr<AudioDecoderFactory> audio_decoders =
        CreateBuiltinAudioDecoderFactory();
    for (const auto& codec : audio_decoders->GetSupportedDecoders()) {
      StringBuilder sb;
      sb << "Decode audio/";
      sb << codec.format.name << "/" << codec.format.clockrate_hz << "/"
         << codec.format.num_channels;
      for (const auto& param : codec.format.parameters) {
        sb << ";" << param.first << ":" << param.second;
      }
      signature_.push_back(sb.Release());
    }
    scoped_refptr<AudioEncoderFactory> audio_encoders =
        CreateBuiltinAudioEncoderFactory();
    for (const auto& codec : audio_encoders->GetSupportedEncoders()) {
      StringBuilder sb;
      sb << "Encode audio/";
      sb << codec.format.name << "/" << codec.format.clockrate_hz << "/"
         << codec.format.num_channels;
      for (const auto& param : codec.format.parameters) {
        sb << ";" << param.first << ":" << param.second;
      }
      signature_.push_back(sb.Release());
    }
    std::unique_ptr<VideoDecoderFactory> video_decoders =
        CreateBuiltinVideoDecoderFactory();
    for (const SdpVideoFormat& format : video_decoders->GetSupportedFormats()) {
      StringBuilder sb;
      sb << "Decode video/";
      sb << format.name;
      for (const auto& kv : format.parameters) {
        sb << ";" << kv.first << ":" << kv.second;
      }
      signature_.push_back(sb.Release());
    }
    std::unique_ptr<VideoEncoderFactory> video_encoders =
        CreateBuiltinVideoEncoderFactory();
    for (const auto& format : video_encoders->GetSupportedFormats()) {
      StringBuilder sb;
      sb << "Encode video/";
      // We don't use format.ToString because that includes scalability modes,
      // which aren't supposed to influence SDP.
      sb << format.name;
      for (const auto& kv : format.parameters) {
        sb << ";" << kv.first << ":" << kv.second;
      }
      signature_.push_back(sb.Release());
    }
  }
  Id RecognizeSignature() {
    std::vector<std::string> webrtc_tip_of_tree = {
        "Decode audio/opus/48000/2;minptime:10;useinbandfec:1",
        "Decode audio/G722/8000/1",
        "Decode audio/PCMU/8000/1",
        "Decode audio/PCMA/8000/1",
        "Encode audio/opus/48000/2;minptime:10;useinbandfec:1",
        "Encode audio/G722/8000/1",
        "Encode audio/PCMU/8000/1",
        "Encode audio/PCMA/8000/1",
        "Decode video/VP8",
        "Decode video/VP9;profile-id:0",
        "Decode video/VP9;profile-id:2",
        "Decode video/VP9;profile-id:1",
        "Decode video/VP9;profile-id:3",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "42001f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "42001f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "42e01f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "42e01f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "4d001f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "4d001f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "f4001f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "f4001f",
        "Decode video/AV1;level-idx:5;profile:0;tier:0",
        "Decode video/AV1;level-idx:5;profile:1;tier:0",
        "Encode video/VP8",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "42001f",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "42001f",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "42e01f",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "42e01f",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "4d001f",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "4d001f",
        "Encode video/AV1;level-idx:5;profile:0;tier:0",
        "Encode video/VP9;profile-id:0",
        "Encode video/VP9;profile-id:2",
    };
    if (signature_ == webrtc_tip_of_tree) {
      return Id::kWebRtcTipOfTree;
    }
    std::vector<std::string> linux_more_configs_1 = {
        "Decode audio/opus/48000/2;minptime:10;useinbandfec:1",
        "Decode audio/G722/8000/1",
        "Decode audio/PCMU/8000/1",
        "Decode audio/PCMA/8000/1",
        "Encode audio/opus/48000/2;minptime:10;useinbandfec:1",
        "Encode audio/G722/8000/1",
        "Encode audio/PCMU/8000/1",
        "Encode audio/PCMA/8000/1",
        "Decode video/VP8",
        "Decode video/VP9;profile-id:0",
        "Decode video/VP9;profile-id:2",
        "Decode video/VP9;profile-id:1",
        "Decode video/VP9;profile-id:3",
        "Decode video/AV1;level-idx:5;profile:0;tier:0",
        "Decode video/AV1;level-idx:5;profile:1;tier:0",
        "Encode video/VP8",
        "Encode video/AV1;level-idx:5;profile:0;tier:0",
        "Encode video/VP9;profile-id:0",
        "Encode video/VP9;profile-id:2",
    };
    if (signature_ == linux_more_configs_1) {
      return Id::kWebRtcMoreConfigs1;
    }
    std::vector<std::string> android = {
        "Decode audio/opus/48000/2;minptime:10;useinbandfec:1",
        "Decode audio/G722/8000/1",
        "Decode audio/PCMU/8000/1",
        "Decode audio/PCMA/8000/1",
        "Encode audio/opus/48000/2;minptime:10;useinbandfec:1",
        "Encode audio/G722/8000/1",
        "Encode audio/PCMU/8000/1",
        "Encode audio/PCMA/8000/1",
        "Decode video/VP8",
        "Decode video/VP9;profile-id:0",
        "Decode video/VP9;profile-id:1",
        "Decode video/VP9;profile-id:3",
        "Decode video/AV1;level-idx:5;profile:0;tier:0",
        "Decode video/AV1;level-idx:5;profile:1;tier:0",
        "Encode video/VP8",
        "Encode video/AV1;level-idx:5;profile:0;tier:0",
        "Encode video/VP9;profile-id:0",
    };
    if (signature_ == android) {
      return Id::kWebRtcAndroid;
    }
    std::vector<std::string> google_internal = {
        "Decode audio/opus/48000/2;minptime:10;useinbandfec:1",
        "Decode audio/G722/8000/1",
        "Decode audio/PCMU/8000/1",
        "Decode audio/PCMA/8000/1",
        "Encode audio/opus/48000/2;minptime:10;useinbandfec:1",
        "Encode audio/G722/8000/1",
        "Encode audio/PCMU/8000/1",
        "Encode audio/PCMA/8000/1",
        "Decode video/VP8",
        "Decode video/VP9;profile-id:0",
        "Decode video/VP9;profile-id:1",
        "Decode video/VP9;profile-id:3",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "42001f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "42001f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "42e01f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "42e01f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "4d001f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "4d001f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "f4001f",
        "Decode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "f4001f",
        "Encode video/VP8",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "42001f",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "42001f",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "42e01f",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "42e01f",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:1;profile-level-id:"
        "4d001f",
        "Encode "
        "video/"
        "H264;level-asymmetry-allowed:1;packetization-mode:0;profile-level-id:"
        "4d001f",
        "Encode video/VP9;profile-id:0",
    };
    if (signature_ == google_internal) {
      return Id::kGoogleInternal;
    }
    // If unrecognized, produce a debug printout.
    StringBuilder sb;
    sb << "{\n";
    for (std::string str : signature_) {
      sb << "\"" << str << "\",\n";
    }
    sb << "}\n";
    RTC_LOG(LS_ERROR) << "New factory signature: " << sb.str();
    return Id::kNotRecognized;
  }

  std::vector<std::string> signature_;
  Id id_;
};

class ResultingCodecList {
 public:
  FactorySignature::Id factory_id;
  std::vector<std::string> caller_local;
  std::vector<std::string> caller_remote;
  std::vector<std::string> callee_local;
  std::vector<std::string> callee_remote;
};

class PeerConnectionIntegrationTest : public PeerConnectionIntegrationBaseTest {
 protected:
  PeerConnectionIntegrationTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}

  std::vector<std::string> CodecList(
      const SessionDescriptionInterface& desc_interface) {
    std::vector<std::string> results;
    int media_section_counter = 0;
    const SessionDescription* desc = desc_interface.description();
    for (auto& content : desc->contents()) {
      ++media_section_counter;
      const auto* media_description = content.media_description();
      const auto& codecs = media_description->codecs();
      for (const auto& codec : codecs) {
        StringBuilder str;
        str << media_section_counter << " " << absl::StrCat(codec);
        results.push_back(str.Release());
      }
    }
    return results;
  }

  // This function returns a string with a C++ initializer for a
  // ResultingCodecList object. The intended use is to paste the string from the
  // log into the source code when updating the test.
  std::string DumpAsResultingCodecList(FactorySignature::Id id,
                                       std::vector<std::string> caller_local,
                                       std::vector<std::string> caller_remote,
                                       std::vector<std::string> callee_local,
                                       std::vector<std::string> callee_remote) {
    StringBuilder sb;
    // TODO: issues.webrtc.org/397895867 - change kChangeThis to the name of
    // the value. Requires adding an AbslStringifier to the enum.
    sb << "\n{" << ".factory_id = FactorySignature::Id::kChangeThis"
       << static_cast<int>(id) << ",\n"
       << ".caller_local = {";
    for (const std::string& str : caller_local) {
      sb << "\"" << str << "\",\n";
    }
    sb << "},\n .caller_remote = {";
    for (const std::string& str : caller_remote) {
      sb << "\"" << str << "\",\n";
    }
    sb << "},\n .callee_local = {";
    for (const std::string& str : callee_local) {
      sb << "\"" << str << "\",\n";
    }
    sb << "},\n .callee_remote = {";
    for (const std::string& str : callee_remote) {
      sb << "\"" << str << "\",\n";
    }
    sb << "}}\n";
    return sb.Release();
  }
};

TEST_F(PeerConnectionIntegrationTest, BasicOfferAnswerPayloadTypesStable) {
  FactorySignature factory_signature;
  ASSERT_THAT(factory_signature.id(),
              Not(Eq(FactorySignature::Id::kNotRecognized)));
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignalingForSdpOnly();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  // Start offer/answer exchange and wait for it to complete.
  caller()->CreateAndSetAndSignalOffer();

  ASSERT_THAT(
      WaitUntil([&] { return SignalingStateStable(); }, ::testing::IsTrue()),
      IsRtcOk());

  // Extract PT and codec from all media sections, and check that they
  // are stable (what was expected).
  // Maintenance: In order to get a new golden set of strings, make the list
  // empty and run. Gmock will output a valid C++ array initializer for you.

  std::vector<ResultingCodecList> golden_answers = {
      {.factory_id = FactorySignature::Id::kWebRtcTipOfTree,
       .caller_local =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]",
            "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]",
            "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]",
            "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]",
            "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 "
            "[103:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "42001f]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 "
            "[107:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "42001f]",
            "2 [108:video/rtx/90000/0;apt=107]",
            "2 "
            "[109:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "42e01f]",
            "2 [114:video/rtx/90000/0;apt=109]",
            "2 "
            "[115:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "42e01f]",
            "2 [116:video/rtx/90000/0;apt=115]",
            "2 "
            "[117:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "4d001f]",
            "2 [118:video/rtx/90000/0;apt=117]",
            "2 "
            "[39:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "4d001f]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [45:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [46:video/rtx/90000/0;apt=45]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [100:video/VP9/90000/0;profile-id=2]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 [119:video/red/90000/0]",
            "2 [120:video/rtx/90000/0;apt=119]",
            "2 [121:video/ulpfec/90000/0]"},
       .caller_remote =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]",
            "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]",
            "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]",
            "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]",
            "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 "
            "[103:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "42001f]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 "
            "[107:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "42001f]",
            "2 [108:video/rtx/90000/0;apt=107]",
            "2 "
            "[109:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "42e01f]",
            "2 [114:video/rtx/90000/0;apt=109]",
            "2 "
            "[115:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "42e01f]",
            "2 [116:video/rtx/90000/0;apt=115]",
            "2 "
            "[117:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "4d001f]",
            "2 [118:video/rtx/90000/0;apt=117]",
            "2 "
            "[39:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "4d001f]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [45:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [46:video/rtx/90000/0;apt=45]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [100:video/VP9/90000/0;profile-id=2]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 [119:video/red/90000/0]",
            "2 [120:video/rtx/90000/0;apt=119]",
            "2 [121:video/ulpfec/90000/0]"},
       .callee_local =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]",
            "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]",
            "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]",
            "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]",
            "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 "
            "[103:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "42001f]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 "
            "[107:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "42001f]",
            "2 [108:video/rtx/90000/0;apt=107]",
            "2 "
            "[109:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "42e01f]",
            "2 [114:video/rtx/90000/0;apt=109]",
            "2 "
            "[115:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "42e01f]",
            "2 [116:video/rtx/90000/0;apt=115]",
            "2 "
            "[117:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "4d001f]",
            "2 [118:video/rtx/90000/0;apt=117]",
            "2 "
            "[39:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "4d001f]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [45:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [46:video/rtx/90000/0;apt=45]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [100:video/VP9/90000/0;profile-id=2]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 [119:video/red/90000/0]",
            "2 [120:video/rtx/90000/0;apt=119]",
            "2 [121:video/ulpfec/90000/0]"},
       .callee_remote =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]",
            "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]",
            "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]",
            "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]",
            "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 "
            "[103:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "42001f]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 "
            "[107:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "42001f]",
            "2 [108:video/rtx/90000/0;apt=107]",
            "2 "
            "[109:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "42e01f]",
            "2 [114:video/rtx/90000/0;apt=109]",
            "2 "
            "[115:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "42e01f]",
            "2 [116:video/rtx/90000/0;apt=115]",
            "2 "
            "[117:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
            "4d001f]",
            "2 [118:video/rtx/90000/0;apt=117]",
            "2 "
            "[39:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
            "4d001f]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [45:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [46:video/rtx/90000/0;apt=45]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [100:video/VP9/90000/0;profile-id=2]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 [119:video/red/90000/0]",
            "2 [120:video/rtx/90000/0;apt=119]",
            "2 [121:video/ulpfec/90000/0]"}},

      {.factory_id = FactorySignature::Id::kWebRtcMoreConfigs1,
       .caller_local =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]", "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]", "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]", "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]", "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 [39:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [100:video/VP9/90000/0;profile-id=2]",
            "2 [101:video/rtx/90000/0;apt=100]", "2 [103:video/red/90000/0]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 [107:video/ulpfec/90000/0]"},
       .caller_remote =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]", "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]", "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]", "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]", "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 [39:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [100:video/VP9/90000/0;profile-id=2]",
            "2 [101:video/rtx/90000/0;apt=100]", "2 [103:video/red/90000/0]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 [107:video/ulpfec/90000/0]"},
       .callee_local =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]", "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]", "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]", "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]", "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 [39:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [100:video/VP9/90000/0;profile-id=2]",
            "2 [101:video/rtx/90000/0;apt=100]", "2 [103:video/red/90000/0]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 [107:video/ulpfec/90000/0]"},
       .callee_remote =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]", "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]", "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]", "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]", "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 [39:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [100:video/VP9/90000/0;profile-id=2]",
            "2 [101:video/rtx/90000/0;apt=100]", "2 [103:video/red/90000/0]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 [107:video/ulpfec/90000/0]"}},
      {.factory_id = FactorySignature::Id::kWebRtcAndroid,
       .caller_local =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]", "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]", "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]", "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]", "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 [39:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]", "2 [100:video/red/90000/0]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 [103:video/ulpfec/90000/0]"},
       .caller_remote =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]", "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]", "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]", "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]", "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 [39:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]", "2 [100:video/red/90000/0]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 [103:video/ulpfec/90000/0]"},
       .callee_local =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]", "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]", "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]", "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]", "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 [39:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]", "2 [100:video/red/90000/0]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 [103:video/ulpfec/90000/0]"},
       .callee_remote =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]", "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]", "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]", "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]", "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 [39:video/AV1/90000/0;level-idx=5;profile=0;tier=0]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]", "2 [100:video/red/90000/0]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 [103:video/ulpfec/90000/0]"}},
      {.factory_id = FactorySignature::Id::kGoogleInternal,
       .caller_local =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]",
            "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]",
            "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]",
            "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]",
            "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 "
            "[100:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-"
            "id=42001f]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 "
            "[103:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-"
            "id=42001f]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 "
            "[107:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-"
            "id=42e01f]",
            "2 [108:video/rtx/90000/0;apt=107]",
            "2 "
            "[109:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-"
            "id=42e01f]",
            "2 [114:video/rtx/90000/0;apt=109]",
            "2 "
            "[115:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-"
            "id=4d001f]",
            "2 [116:video/rtx/90000/0;apt=115]",
            "2 "
            "[39:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-"
            "id=4d001f]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [117:video/red/90000/0]",
            "2 [118:video/rtx/90000/0;apt=117]",
            "2 [119:video/ulpfec/90000/0]"},
       .caller_remote =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]",
            "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]",
            "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]",
            "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]",
            "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 "
            "[100:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-"
            "id=42001f]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 "
            "[103:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-"
            "id=42001f]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 "
            "[107:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-"
            "id=42e01f]",
            "2 [108:video/rtx/90000/0;apt=107]",
            "2 "
            "[109:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-"
            "id=42e01f]",
            "2 [114:video/rtx/90000/0;apt=109]",
            "2 "
            "[115:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-"
            "id=4d001f]",
            "2 [116:video/rtx/90000/0;apt=115]",
            "2 "
            "[39:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-"
            "id=4d001f]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [117:video/red/90000/0]",
            "2 [118:video/rtx/90000/0;apt=117]",
            "2 [119:video/ulpfec/90000/0]"},
       .callee_local =
           {"1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
            "1 [63:audio/red/48000/2;=111/111]",
            "1 [9:audio/G722/8000/1]",
            "1 [0:audio/PCMU/8000/1]",
            "1 [8:audio/PCMA/8000/1]",
            "1 [13:audio/CN/8000/1]",
            "1 [110:audio/telephone-event/48000/1]",
            "1 [126:audio/telephone-event/8000/1]",
            "2 [96:video/VP8/90000/0]",
            "2 [97:video/rtx/90000/0;apt=96]",
            "2 "
            "[100:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-"
            "id=42001f]",
            "2 [101:video/rtx/90000/0;apt=100]",
            "2 "
            "[103:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-"
            "id=42001f]",
            "2 [104:video/rtx/90000/0;apt=103]",
            "2 "
            "[107:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-"
            "id=42e01f]",
            "2 [108:video/rtx/90000/0;apt=107]",
            "2 "
            "[109:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-"
            "id=42e01f]",
            "2 [114:video/rtx/90000/0;apt=109]",
            "2 "
            "[115:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-"
            "id=4d001f]",
            "2 [116:video/rtx/90000/0;apt=115]",
            "2 "
            "[39:video/H264/90000/"
            "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-"
            "id=4d001f]",
            "2 [40:video/rtx/90000/0;apt=39]",
            "2 [98:video/VP9/90000/0;profile-id=0]",
            "2 [99:video/rtx/90000/0;apt=98]",
            "2 [117:video/red/90000/0]",
            "2 [118:video/rtx/90000/0;apt=117]",
            "2 [119:video/ulpfec/90000/0]"},
       .callee_remote = {
           "1 [111:audio/opus/48000/2;minptime=10;useinbandfec=1]",
           "1 [63:audio/red/48000/2;=111/111]",
           "1 [9:audio/G722/8000/1]",
           "1 [0:audio/PCMU/8000/1]",
           "1 [8:audio/PCMA/8000/1]",
           "1 [13:audio/CN/8000/1]",
           "1 [110:audio/telephone-event/48000/1]",
           "1 [126:audio/telephone-event/8000/1]",
           "2 [96:video/VP8/90000/0]",
           "2 [97:video/rtx/90000/0;apt=96]",
           "2 "
           "[100:video/H264/90000/"
           "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
           "42001f]",
           "2 [101:video/rtx/90000/0;apt=100]",
           "2 "
           "[103:video/H264/90000/"
           "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
           "42001f]",
           "2 [104:video/rtx/90000/0;apt=103]",
           "2 "
           "[107:video/H264/90000/"
           "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
           "42e01f]",
           "2 [108:video/rtx/90000/0;apt=107]",
           "2 "
           "[109:video/H264/90000/"
           "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
           "42e01f]",
           "2 [114:video/rtx/90000/0;apt=109]",
           "2 "
           "[115:video/H264/90000/"
           "0;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
           "4d001f]",
           "2 [116:video/rtx/90000/0;apt=115]",
           "2 "
           "[39:video/H264/90000/"
           "0;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
           "4d001f]",
           "2 [40:video/rtx/90000/0;apt=39]",
           "2 [98:video/VP9/90000/0;profile-id=0]",
           "2 [99:video/rtx/90000/0;apt=98]",
           "2 [117:video/red/90000/0]",
           "2 [118:video/rtx/90000/0;apt=117]",
           "2 [119:video/ulpfec/90000/0]"}}};
  auto this_golden_it =
      std::find_if(golden_answers.begin(), golden_answers.end(),
                   [&](const ResultingCodecList& candidate) {
                     return candidate.factory_id == factory_signature.id();
                   });
  ASSERT_THAT(this_golden_it, Not(Eq(golden_answers.end())))
      << "Add this result set to golden_answers:\n"
      << DumpAsResultingCodecList(
             factory_signature.id(),
             CodecList(*caller()->pc()->local_description()),
             CodecList(*caller()->pc()->remote_description()),
             CodecList(*callee()->pc()->local_description()),
             CodecList(*callee()->pc()->remote_description()));

  const ResultingCodecList& this_golden = *this_golden_it;
  EXPECT_THAT(CodecList(*caller()->pc()->local_description()),
              ElementsAreArray(this_golden.caller_local));
  EXPECT_THAT(CodecList(*caller()->pc()->remote_description()),
              ElementsAreArray(this_golden.caller_remote));
  EXPECT_THAT(CodecList(*callee()->pc()->local_description()),
              ElementsAreArray(this_golden.callee_local));
  EXPECT_THAT(CodecList(*callee()->pc()->remote_description()),
              ElementsAreArray(this_golden.callee_remote));
}

}  // namespace
}  // namespace webrtc
