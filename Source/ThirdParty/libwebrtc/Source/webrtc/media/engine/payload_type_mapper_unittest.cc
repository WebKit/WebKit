/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <set>
#include <string>

#include "media/engine/payload_type_mapper.h"
#include "test/gtest.h"

namespace cricket {

class PayloadTypeMapperTest : public testing::Test {
 protected:
  PayloadTypeMapper mapper_;
};

TEST_F(PayloadTypeMapperTest, StaticPayloadTypes) {
  EXPECT_EQ(0,  mapper_.FindMappingFor({"pcmu",   8000, 1}));
  EXPECT_EQ(3,  mapper_.FindMappingFor({"gsm",    8000, 1}));
  EXPECT_EQ(4,  mapper_.FindMappingFor({"g723",   8000, 1}));
  EXPECT_EQ(5,  mapper_.FindMappingFor({"dvi4",   8000, 1}));
  EXPECT_EQ(6,  mapper_.FindMappingFor({"dvi4",  16000, 1}));
  EXPECT_EQ(7,  mapper_.FindMappingFor({"lpc",    8000, 1}));
  EXPECT_EQ(8,  mapper_.FindMappingFor({"pcma",   8000, 1}));
  EXPECT_EQ(9,  mapper_.FindMappingFor({"g722",   8000, 1}));
  EXPECT_EQ(10, mapper_.FindMappingFor({"l16",   44100, 2}));
  EXPECT_EQ(11, mapper_.FindMappingFor({"l16",   44100, 1}));
  EXPECT_EQ(12, mapper_.FindMappingFor({"qcelp",  8000, 1}));
  EXPECT_EQ(13, mapper_.FindMappingFor({"cn",     8000, 1}));
  EXPECT_EQ(14, mapper_.FindMappingFor({"mpa",   90000, 0}));
  EXPECT_EQ(14, mapper_.FindMappingFor({"mpa",   90000, 1}));
  EXPECT_EQ(15, mapper_.FindMappingFor({"g728",   8000, 1}));
  EXPECT_EQ(16, mapper_.FindMappingFor({"dvi4",  11025, 1}));
  EXPECT_EQ(17, mapper_.FindMappingFor({"dvi4",  22050, 1}));
  EXPECT_EQ(18, mapper_.FindMappingFor({"g729",   8000, 1}));
}

TEST_F(PayloadTypeMapperTest, WebRTCPayloadTypes) {
  // Tests that the payload mapper knows about the audio and data formats we've
  // been using in WebRTC, with their hard coded values.
  auto data_mapping = [this] (const char *name) {
    return mapper_.FindMappingFor({name, 0, 0});
  };
  EXPECT_EQ(kGoogleRtpDataCodecPlType, data_mapping(kGoogleRtpDataCodecName));
  EXPECT_EQ(kGoogleSctpDataCodecPlType, data_mapping(kGoogleSctpDataCodecName));

  EXPECT_EQ(102, mapper_.FindMappingFor({kIlbcCodecName,  8000, 1}));
  EXPECT_EQ(103, mapper_.FindMappingFor({kIsacCodecName, 16000, 1}));
  EXPECT_EQ(104, mapper_.FindMappingFor({kIsacCodecName, 32000, 1}));
  EXPECT_EQ(105, mapper_.FindMappingFor({kCnCodecName,   16000, 1}));
  EXPECT_EQ(106, mapper_.FindMappingFor({kCnCodecName,   32000, 1}));
  EXPECT_EQ(111, mapper_.FindMappingFor({kOpusCodecName, 48000, 2,
        {{"minptime", "10"}, {"useinbandfec", "1"}}}));
  // TODO(solenberg): Remove 16k, 32k, 48k DTMF checks once these payload types
  // are dynamically assigned.
  EXPECT_EQ(110, mapper_.FindMappingFor({kDtmfCodecName, 48000, 1}));
  EXPECT_EQ(112, mapper_.FindMappingFor({kDtmfCodecName, 32000, 1}));
  EXPECT_EQ(113, mapper_.FindMappingFor({kDtmfCodecName, 16000, 1}));
  EXPECT_EQ(126, mapper_.FindMappingFor({kDtmfCodecName, 8000, 1}));
}

TEST_F(PayloadTypeMapperTest, ValidDynamicPayloadTypes) {
  // RFC 3551 says:
  // "This profile reserves payload type numbers in the range 96-127
  // exclusively for dynamic assignment.  Applications SHOULD first use
  // values in this range for dynamic payload types.  Those applications
  // which need to define more than 32 dynamic payload types MAY bind
  // codes below 96, in which case it is RECOMMENDED that unassigned
  // payload type numbers be used first.  However, the statically assigned
  // payload types are default bindings and MAY be dynamically bound to
  // new encodings if needed."

  // Tests that the payload mapper uses values in the dynamic payload type range
  // (96 - 127) before any others and that the values returned are all valid.
  bool has_been_below_96 = false;
  std::set<int> used_payload_types;
  for (int i = 0; i != 256; ++i) {
    std::string format_name = "unknown_format_" + std::to_string(i);
    webrtc::SdpAudioFormat format(format_name.c_str(), i*100, (i % 2) + 1);
    auto opt_payload_type = mapper_.GetMappingFor(format);
    bool mapper_is_full = false;

    // There's a limited number of slots for payload types. We're fine with not
    // being able to map them all.
    if (opt_payload_type) {
      int payload_type = *opt_payload_type;
      EXPECT_FALSE(mapper_is_full) << "Mapping should not fail sporadically";
      EXPECT_EQ(used_payload_types.find(payload_type), used_payload_types.end())
          << "Payload types must not be reused";
      used_payload_types.insert(payload_type);
      EXPECT_GE(payload_type, 0) << "Negative payload types are invalid";
      EXPECT_LE(payload_type, 127) << "Payload types above 127 are invalid";
      EXPECT_FALSE(payload_type >= 96 && has_been_below_96);
      if (payload_type < 96)
        has_been_below_96 = true;

      EXPECT_EQ(payload_type, mapper_.FindMappingFor(format))
          << "Mapping must be permanent after successful call to "
             "GetMappingFor";
      EXPECT_EQ(payload_type, mapper_.GetMappingFor(format))
          << "Subsequent calls to GetMappingFor must return the same value";
    } else {
      mapper_is_full = true;
    }
  }

  // Also, we must've been able to map at least one dynamic payload type.
  EXPECT_FALSE(used_payload_types.empty())
      << "Mapper must support at least one user-defined payload type";
}

TEST_F(PayloadTypeMapperTest, ToAudioCodec) {
  webrtc::SdpAudioFormat format("unknown_format", 4711, 17);
  auto opt_payload_type = mapper_.GetMappingFor(format);
  EXPECT_TRUE(opt_payload_type);
  auto opt_audio_codec = mapper_.ToAudioCodec(format);
  EXPECT_TRUE(opt_audio_codec);

  if (opt_payload_type && opt_audio_codec) {
    int payload_type        = *opt_payload_type;
    const AudioCodec& codec = *opt_audio_codec;

    EXPECT_EQ(codec.id, payload_type);
    EXPECT_EQ(codec.name, format.name);
    EXPECT_EQ(codec.clockrate, format.clockrate_hz);
    EXPECT_EQ(codec.channels, format.num_channels);
    EXPECT_EQ(codec.params, format.parameters);
  }
}

}  // namespace cricket
