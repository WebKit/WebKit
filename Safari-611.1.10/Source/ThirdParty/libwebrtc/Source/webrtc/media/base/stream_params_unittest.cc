/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/stream_params.h"

#include <stdint.h>

#include "media/base/test_utils.h"
#include "rtc_base/arraysize.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Each;
using ::testing::Ne;

static const uint32_t kSsrcs1[] = {1};
static const uint32_t kSsrcs2[] = {1, 2};

static cricket::StreamParams CreateStreamParamsWithSsrcGroup(
    const std::string& semantics,
    const uint32_t ssrcs_in[],
    size_t len) {
  cricket::StreamParams stream;
  std::vector<uint32_t> ssrcs(ssrcs_in, ssrcs_in + len);
  cricket::SsrcGroup sg(semantics, ssrcs);
  stream.ssrcs = ssrcs;
  stream.ssrc_groups.push_back(sg);
  return stream;
}

TEST(SsrcGroup, EqualNotEqual) {
  cricket::SsrcGroup ssrc_groups[] = {
      cricket::SsrcGroup("ABC", MAKE_VECTOR(kSsrcs1)),
      cricket::SsrcGroup("ABC", MAKE_VECTOR(kSsrcs2)),
      cricket::SsrcGroup("Abc", MAKE_VECTOR(kSsrcs2)),
      cricket::SsrcGroup("abc", MAKE_VECTOR(kSsrcs2)),
  };

  for (size_t i = 0; i < arraysize(ssrc_groups); ++i) {
    for (size_t j = 0; j < arraysize(ssrc_groups); ++j) {
      EXPECT_EQ((ssrc_groups[i] == ssrc_groups[j]), (i == j));
      EXPECT_EQ((ssrc_groups[i] != ssrc_groups[j]), (i != j));
    }
  }
}

TEST(SsrcGroup, HasSemantics) {
  cricket::SsrcGroup sg1("ABC", MAKE_VECTOR(kSsrcs1));
  EXPECT_TRUE(sg1.has_semantics("ABC"));

  cricket::SsrcGroup sg2("Abc", MAKE_VECTOR(kSsrcs1));
  EXPECT_FALSE(sg2.has_semantics("ABC"));

  cricket::SsrcGroup sg3("abc", MAKE_VECTOR(kSsrcs1));
  EXPECT_FALSE(sg3.has_semantics("ABC"));
}

TEST(SsrcGroup, ToString) {
  cricket::SsrcGroup sg1("ABC", MAKE_VECTOR(kSsrcs1));
  EXPECT_STREQ("{semantics:ABC;ssrcs:[1]}", sg1.ToString().c_str());
}

TEST(StreamParams, CreateLegacy) {
  const uint32_t ssrc = 7;
  cricket::StreamParams one_sp = cricket::StreamParams::CreateLegacy(ssrc);
  EXPECT_EQ(1U, one_sp.ssrcs.size());
  EXPECT_EQ(ssrc, one_sp.first_ssrc());
  EXPECT_TRUE(one_sp.has_ssrcs());
  EXPECT_TRUE(one_sp.has_ssrc(ssrc));
  EXPECT_FALSE(one_sp.has_ssrc(ssrc + 1));
  EXPECT_FALSE(one_sp.has_ssrc_groups());
  EXPECT_EQ(0U, one_sp.ssrc_groups.size());
}

TEST(StreamParams, HasSsrcGroup) {
  cricket::StreamParams sp =
      CreateStreamParamsWithSsrcGroup("XYZ", kSsrcs2, arraysize(kSsrcs2));
  EXPECT_EQ(2U, sp.ssrcs.size());
  EXPECT_EQ(kSsrcs2[0], sp.first_ssrc());
  EXPECT_TRUE(sp.has_ssrcs());
  EXPECT_TRUE(sp.has_ssrc(kSsrcs2[0]));
  EXPECT_TRUE(sp.has_ssrc(kSsrcs2[1]));
  EXPECT_TRUE(sp.has_ssrc_group("XYZ"));
  EXPECT_EQ(1U, sp.ssrc_groups.size());
  EXPECT_EQ(2U, sp.ssrc_groups[0].ssrcs.size());
  EXPECT_EQ(kSsrcs2[0], sp.ssrc_groups[0].ssrcs[0]);
  EXPECT_EQ(kSsrcs2[1], sp.ssrc_groups[0].ssrcs[1]);
}

TEST(StreamParams, GetSsrcGroup) {
  cricket::StreamParams sp =
      CreateStreamParamsWithSsrcGroup("XYZ", kSsrcs2, arraysize(kSsrcs2));
  EXPECT_EQ(NULL, sp.get_ssrc_group("xyz"));
  EXPECT_EQ(&sp.ssrc_groups[0], sp.get_ssrc_group("XYZ"));
}

TEST(StreamParams, HasStreamWithNoSsrcs) {
  cricket::StreamParams sp_1 = cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  cricket::StreamParams sp_2 = cricket::StreamParams::CreateLegacy(kSsrcs2[0]);
  std::vector<cricket::StreamParams> streams({sp_1, sp_2});
  EXPECT_FALSE(HasStreamWithNoSsrcs(streams));

  cricket::StreamParams unsignaled_stream;
  streams.push_back(unsignaled_stream);
  EXPECT_TRUE(HasStreamWithNoSsrcs(streams));
}

TEST(StreamParams, EqualNotEqual) {
  cricket::StreamParams l1 = cricket::StreamParams::CreateLegacy(1);
  cricket::StreamParams l2 = cricket::StreamParams::CreateLegacy(2);
  cricket::StreamParams sg1 =
      CreateStreamParamsWithSsrcGroup("ABC", kSsrcs1, arraysize(kSsrcs1));
  cricket::StreamParams sg2 =
      CreateStreamParamsWithSsrcGroup("ABC", kSsrcs2, arraysize(kSsrcs2));
  cricket::StreamParams sg3 =
      CreateStreamParamsWithSsrcGroup("Abc", kSsrcs2, arraysize(kSsrcs2));
  cricket::StreamParams sg4 =
      CreateStreamParamsWithSsrcGroup("abc", kSsrcs2, arraysize(kSsrcs2));
  cricket::StreamParams sps[] = {l1, l2, sg1, sg2, sg3, sg4};

  for (size_t i = 0; i < arraysize(sps); ++i) {
    for (size_t j = 0; j < arraysize(sps); ++j) {
      EXPECT_EQ((sps[i] == sps[j]), (i == j));
      EXPECT_EQ((sps[i] != sps[j]), (i != j));
    }
  }
}

TEST(StreamParams, FidFunctions) {
  uint32_t fid_ssrc;

  cricket::StreamParams sp = cricket::StreamParams::CreateLegacy(1);
  EXPECT_FALSE(sp.AddFidSsrc(10, 20));
  EXPECT_TRUE(sp.AddFidSsrc(1, 2));
  EXPECT_TRUE(sp.GetFidSsrc(1, &fid_ssrc));
  EXPECT_EQ(2u, fid_ssrc);
  EXPECT_FALSE(sp.GetFidSsrc(15, &fid_ssrc));

  sp.add_ssrc(20);
  EXPECT_TRUE(sp.AddFidSsrc(20, 30));
  EXPECT_TRUE(sp.GetFidSsrc(20, &fid_ssrc));
  EXPECT_EQ(30u, fid_ssrc);

  // Manually create SsrcGroup to test bounds-checking
  // in GetSecondarySsrc. We construct an invalid StreamParams
  // for this.
  std::vector<uint32_t> fid_vector;
  fid_vector.push_back(13);
  cricket::SsrcGroup invalid_fid_group(cricket::kFidSsrcGroupSemantics,
                                       fid_vector);
  cricket::StreamParams sp_invalid;
  sp_invalid.add_ssrc(13);
  sp_invalid.ssrc_groups.push_back(invalid_fid_group);
  EXPECT_FALSE(sp_invalid.GetFidSsrc(13, &fid_ssrc));
}

TEST(StreamParams, GetPrimaryAndFidSsrcs) {
  cricket::StreamParams sp;
  sp.ssrcs.push_back(1);
  sp.ssrcs.push_back(2);
  sp.ssrcs.push_back(3);

  std::vector<uint32_t> primary_ssrcs;
  sp.GetPrimarySsrcs(&primary_ssrcs);
  std::vector<uint32_t> fid_ssrcs;
  sp.GetFidSsrcs(primary_ssrcs, &fid_ssrcs);
  ASSERT_EQ(1u, primary_ssrcs.size());
  EXPECT_EQ(1u, primary_ssrcs[0]);
  ASSERT_EQ(0u, fid_ssrcs.size());

  sp.ssrc_groups.push_back(
      cricket::SsrcGroup(cricket::kSimSsrcGroupSemantics, sp.ssrcs));
  sp.AddFidSsrc(1, 10);
  sp.AddFidSsrc(2, 20);

  primary_ssrcs.clear();
  sp.GetPrimarySsrcs(&primary_ssrcs);
  fid_ssrcs.clear();
  sp.GetFidSsrcs(primary_ssrcs, &fid_ssrcs);
  ASSERT_EQ(3u, primary_ssrcs.size());
  EXPECT_EQ(1u, primary_ssrcs[0]);
  EXPECT_EQ(2u, primary_ssrcs[1]);
  EXPECT_EQ(3u, primary_ssrcs[2]);
  ASSERT_EQ(2u, fid_ssrcs.size());
  EXPECT_EQ(10u, fid_ssrcs[0]);
  EXPECT_EQ(20u, fid_ssrcs[1]);
}

TEST(StreamParams, FecFrFunctions) {
  uint32_t fecfr_ssrc;

  cricket::StreamParams sp = cricket::StreamParams::CreateLegacy(1);
  EXPECT_FALSE(sp.AddFecFrSsrc(10, 20));
  EXPECT_TRUE(sp.AddFecFrSsrc(1, 2));
  EXPECT_TRUE(sp.GetFecFrSsrc(1, &fecfr_ssrc));
  EXPECT_EQ(2u, fecfr_ssrc);
  EXPECT_FALSE(sp.GetFecFrSsrc(15, &fecfr_ssrc));

  sp.add_ssrc(20);
  EXPECT_TRUE(sp.AddFecFrSsrc(20, 30));
  EXPECT_TRUE(sp.GetFecFrSsrc(20, &fecfr_ssrc));
  EXPECT_EQ(30u, fecfr_ssrc);

  // Manually create SsrcGroup to test bounds-checking
  // in GetSecondarySsrc. We construct an invalid StreamParams
  // for this.
  std::vector<uint32_t> fecfr_vector;
  fecfr_vector.push_back(13);
  cricket::SsrcGroup invalid_fecfr_group(cricket::kFecFrSsrcGroupSemantics,
                                         fecfr_vector);
  cricket::StreamParams sp_invalid;
  sp_invalid.add_ssrc(13);
  sp_invalid.ssrc_groups.push_back(invalid_fecfr_group);
  EXPECT_FALSE(sp_invalid.GetFecFrSsrc(13, &fecfr_ssrc));
}

TEST(StreamParams, ToString) {
  cricket::StreamParams sp =
      CreateStreamParamsWithSsrcGroup("XYZ", kSsrcs2, arraysize(kSsrcs2));
  sp.set_stream_ids({"stream_id"});
  EXPECT_STREQ(
      "{ssrcs:[1,2];ssrc_groups:{semantics:XYZ;ssrcs:[1,2]};stream_ids:stream_"
      "id;}",
      sp.ToString().c_str());
}

TEST(StreamParams, TestGenerateSsrcs_SingleStreamWithRtxAndFlex) {
  rtc::UniqueRandomIdGenerator generator;
  cricket::StreamParams stream;
  stream.GenerateSsrcs(1, true, true, &generator);
  uint32_t primary_ssrc = stream.first_ssrc();
  ASSERT_NE(0u, primary_ssrc);
  uint32_t rtx_ssrc = 0;
  uint32_t flex_ssrc = 0;
  EXPECT_EQ(3u, stream.ssrcs.size());
  EXPECT_TRUE(stream.GetFidSsrc(primary_ssrc, &rtx_ssrc));
  EXPECT_NE(0u, rtx_ssrc);
  EXPECT_TRUE(stream.GetFecFrSsrc(primary_ssrc, &flex_ssrc));
  EXPECT_NE(0u, flex_ssrc);
  EXPECT_FALSE(stream.has_ssrc_group(cricket::kSimSsrcGroupSemantics));
  EXPECT_TRUE(stream.has_ssrc_group(cricket::kFidSsrcGroupSemantics));
  EXPECT_TRUE(stream.has_ssrc_group(cricket::kFecFrSsrcGroupSemantics));
}

TEST(StreamParams, TestGenerateSsrcs_SingleStreamWithRtx) {
  rtc::UniqueRandomIdGenerator generator;
  cricket::StreamParams stream;
  stream.GenerateSsrcs(1, true, false, &generator);
  uint32_t primary_ssrc = stream.first_ssrc();
  ASSERT_NE(0u, primary_ssrc);
  uint32_t rtx_ssrc = 0;
  uint32_t flex_ssrc = 0;
  EXPECT_EQ(2u, stream.ssrcs.size());
  EXPECT_TRUE(stream.GetFidSsrc(primary_ssrc, &rtx_ssrc));
  EXPECT_NE(0u, rtx_ssrc);
  EXPECT_FALSE(stream.GetFecFrSsrc(primary_ssrc, &flex_ssrc));
  EXPECT_EQ(0u, flex_ssrc);
  EXPECT_FALSE(stream.has_ssrc_group(cricket::kSimSsrcGroupSemantics));
  EXPECT_TRUE(stream.has_ssrc_group(cricket::kFidSsrcGroupSemantics));
}

TEST(StreamParams, TestGenerateSsrcs_SingleStreamWithFlex) {
  rtc::UniqueRandomIdGenerator generator;
  cricket::StreamParams stream;
  stream.GenerateSsrcs(1, false, true, &generator);
  uint32_t primary_ssrc = stream.first_ssrc();
  ASSERT_NE(0u, primary_ssrc);
  uint32_t rtx_ssrc = 0;
  uint32_t flex_ssrc = 0;
  EXPECT_EQ(2u, stream.ssrcs.size());
  EXPECT_FALSE(stream.GetFidSsrc(primary_ssrc, &rtx_ssrc));
  EXPECT_EQ(0u, rtx_ssrc);
  EXPECT_TRUE(stream.GetFecFrSsrc(primary_ssrc, &flex_ssrc));
  EXPECT_NE(0u, flex_ssrc);
  EXPECT_FALSE(stream.has_ssrc_group(cricket::kSimSsrcGroupSemantics));
  EXPECT_TRUE(stream.has_ssrc_group(cricket::kFecFrSsrcGroupSemantics));
}

TEST(StreamParams, TestGenerateSsrcs_SimulcastLayersAndRtx) {
  const size_t kNumStreams = 3;
  rtc::UniqueRandomIdGenerator generator;
  cricket::StreamParams stream;
  stream.GenerateSsrcs(kNumStreams, true, false, &generator);
  EXPECT_EQ(kNumStreams * 2, stream.ssrcs.size());
  std::vector<uint32_t> primary_ssrcs, rtx_ssrcs;
  stream.GetPrimarySsrcs(&primary_ssrcs);
  EXPECT_EQ(kNumStreams, primary_ssrcs.size());
  EXPECT_THAT(primary_ssrcs, Each(Ne(0u)));
  stream.GetFidSsrcs(primary_ssrcs, &rtx_ssrcs);
  EXPECT_EQ(kNumStreams, rtx_ssrcs.size());
  EXPECT_THAT(rtx_ssrcs, Each(Ne(0u)));
  EXPECT_TRUE(stream.has_ssrc_group(cricket::kSimSsrcGroupSemantics));
  EXPECT_TRUE(stream.has_ssrc_group(cricket::kFidSsrcGroupSemantics));
}
