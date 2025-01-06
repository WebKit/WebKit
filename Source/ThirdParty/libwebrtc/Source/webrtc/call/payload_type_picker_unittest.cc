/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/payload_type_picker.h"

#include "call/payload_type.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "test/gtest.h"

namespace webrtc {

TEST(PayloadTypePicker, PayloadTypeAssignmentWorks) {
  // Note: This behavior is due to be deprecated and removed.
  PayloadType pt_a(1);
  PayloadType pt_b = 1;  // Implicit conversion
  EXPECT_EQ(pt_a, pt_b);
  int pt_as_int = pt_a;  // Implicit conversion
  EXPECT_EQ(1, pt_as_int);
}

TEST(PayloadTypePicker, InstantiateTypes) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
}

TEST(PayloadTypePicker, StoreAndRecall) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  const PayloadType a_payload_type(123);
  const PayloadType not_a_payload_type(44);
  cricket::Codec a_codec = cricket::CreateVideoCodec(0, "vp8");
  auto error = recorder.AddMapping(a_payload_type, a_codec);
  ASSERT_TRUE(error.ok());
  auto result = recorder.LookupCodec(a_payload_type);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), a_codec);
  auto result_pt = recorder.LookupPayloadType(a_codec);
  ASSERT_TRUE(result_pt.ok());
  EXPECT_EQ(result_pt.value(), a_payload_type);
  EXPECT_FALSE(recorder.LookupCodec(not_a_payload_type).ok());
}

TEST(PayloadTypePicker, ModifyingPtIsIgnored) {
  // Arguably a spec violation, but happens in production.
  // To be decided: Whether we should disallow codec change, fmtp change
  // or both.
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  const PayloadType a_payload_type(123);
  cricket::Codec a_codec =
      cricket::CreateVideoCodec(cricket::Codec::kIdNotSet, "vp8");
  cricket::Codec b_codec =
      cricket::CreateVideoCodec(cricket::Codec::kIdNotSet, "vp9");
  recorder.AddMapping(a_payload_type, a_codec);
  auto error = recorder.AddMapping(a_payload_type, b_codec);
  EXPECT_TRUE(error.ok());
  auto result = recorder.LookupCodec(a_payload_type);
  // Redefinition should be accepted.
  EXPECT_EQ(result.value(), b_codec);
}

TEST(PayloadTypePicker, ModifyingPtIsAnErrorIfDisallowed) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  const PayloadType a_payload_type(123);
  cricket::Codec a_codec =
      cricket::CreateVideoCodec(cricket::Codec::kIdNotSet, "vp8");
  cricket::Codec b_codec =
      cricket::CreateVideoCodec(cricket::Codec::kIdNotSet, "vp9");
  recorder.DisallowRedefinition();
  recorder.AddMapping(a_payload_type, a_codec);
  auto error = recorder.AddMapping(a_payload_type, b_codec);
  EXPECT_FALSE(error.ok());
  auto result = recorder.LookupCodec(a_payload_type);
  // Attempted redefinition should be ignored.
  EXPECT_EQ(result.value(), a_codec);
  recorder.ReallowRedefinition();
}

TEST(PayloadTypePicker, RollbackAndCommit) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  const PayloadType a_payload_type(123);
  const PayloadType b_payload_type(124);
  const PayloadType not_a_payload_type(44);

  cricket::Codec a_codec = cricket::CreateVideoCodec(0, "vp8");

  cricket::Codec b_codec = cricket::CreateVideoCodec(0, "vp9");
  auto error = recorder.AddMapping(a_payload_type, a_codec);
  ASSERT_TRUE(error.ok());
  recorder.Commit();
  ASSERT_TRUE(recorder.AddMapping(b_payload_type, b_codec).ok());
  {
    auto result = recorder.LookupCodec(a_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), a_codec);
  }
  {
    auto result = recorder.LookupCodec(b_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), b_codec);
  }
  recorder.Rollback();
  {
    auto result = recorder.LookupCodec(a_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), a_codec);
  }
  {
    auto result = recorder.LookupCodec(b_payload_type);
    ASSERT_FALSE(result.ok());
  }
  ASSERT_TRUE(recorder.AddMapping(b_payload_type, b_codec).ok());
  // Rollback after a new checkpoint has no effect.
  recorder.Commit();
  recorder.Rollback();
  {
    auto result = recorder.LookupCodec(b_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), b_codec);
  }
}

TEST(PayloadTypePicker, StaticValueIsGood) {
  PayloadTypePicker picker;
  cricket::Codec a_codec =
      cricket::CreateAudioCodec(-1, cricket::kPcmuCodecName, 8000, 1);
  auto result = picker.SuggestMapping(a_codec, nullptr);
  // In the absence of existing mappings, PCMU always has 0 as PT.
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), PayloadType(0));
}

TEST(PayloadTypePicker, DynamicValueIsGood) {
  PayloadTypePicker picker;
  cricket::Codec a_codec = cricket::CreateAudioCodec(-1, "lyra", 8000, 1);
  auto result = picker.SuggestMapping(a_codec, nullptr);
  // This should result in a value from the dynamic range; since this is the
  // first assignment, it should be in the upper range.
  ASSERT_TRUE(result.ok());
  EXPECT_GE(result.value(), PayloadType(96));
  EXPECT_LE(result.value(), PayloadType(127));
}

TEST(PayloadTypePicker, RecordedValueReturned) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  cricket::Codec a_codec = cricket::CreateAudioCodec(-1, "lyra", 8000, 1);
  recorder.AddMapping(47, a_codec);
  auto result = picker.SuggestMapping(a_codec, &recorder);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(47, result.value());
}

TEST(PayloadTypePicker, RecordedValueExcluded) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder1(picker);
  PayloadTypeRecorder recorder2(picker);
  cricket::Codec a_codec = cricket::CreateAudioCodec(-1, "lyra", 8000, 1);
  cricket::Codec b_codec = cricket::CreateAudioCodec(-1, "mlcodec", 8000, 1);
  recorder1.AddMapping(47, a_codec);
  recorder2.AddMapping(47, b_codec);
  auto result = picker.SuggestMapping(b_codec, &recorder1);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(47, result.value());
}

}  // namespace webrtc
