/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "test/comp_avg_pred_test.h"

using libaom_test::ACMRandom;
using libaom_test::AV1DISTWTDCOMPAVG::AV1DISTWTDCOMPAVGTest;
using libaom_test::AV1DISTWTDCOMPAVG::DistWtdCompAvgParam;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1DISTWTDCOMPAVGTest);
using libaom_test::AV1DISTWTDCOMPAVG::AV1DISTWTDCOMPAVGUPSAMPLEDTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1DISTWTDCOMPAVGUPSAMPLEDTest);
using libaom_test::AV1DISTWTDCOMPAVG::DistWtdCompAvgTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DistWtdCompAvgTest);
#if CONFIG_AV1_HIGHBITDEPTH
using libaom_test::AV1DISTWTDCOMPAVG::AV1HighBDDISTWTDCOMPAVGTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighBDDISTWTDCOMPAVGTest);
using libaom_test::AV1DISTWTDCOMPAVG::AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest);
#endif
using std::make_tuple;
using std::tuple;

uint8_t *DistWtdCompAvgTest::reference_data_ = nullptr;
uint8_t *DistWtdCompAvgTest::second_pred_ = nullptr;
uint8_t *DistWtdCompAvgTest::comp_pred_ = nullptr;
uint8_t *DistWtdCompAvgTest::comp_pred_test_ = nullptr;
uint8_t *DistWtdCompAvgTest::reference_data8_ = nullptr;
uint8_t *DistWtdCompAvgTest::second_pred8_ = nullptr;
uint8_t *DistWtdCompAvgTest::comp_pred8_ = nullptr;
uint8_t *DistWtdCompAvgTest::comp_pred8_test_ = nullptr;
uint16_t *DistWtdCompAvgTest::reference_data16_ = nullptr;
uint16_t *DistWtdCompAvgTest::second_pred16_ = nullptr;
uint16_t *DistWtdCompAvgTest::comp_pred16_ = nullptr;
uint16_t *DistWtdCompAvgTest::comp_pred16_test_ = nullptr;

namespace {

TEST_P(AV1DISTWTDCOMPAVGTest, DISABLED_Speed) { RunSpeedTest(GET_PARAM(0)); }

TEST_P(AV1DISTWTDCOMPAVGTest, CheckOutput) { RunCheckOutput(GET_PARAM(0)); }

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3, AV1DISTWTDCOMPAVGTest,
                         libaom_test::AV1DISTWTDCOMPAVG::BuildParams(
                             aom_dist_wtd_comp_avg_pred_ssse3));
#endif

TEST_P(AV1DISTWTDCOMPAVGUPSAMPLEDTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0));
}

TEST_P(AV1DISTWTDCOMPAVGUPSAMPLEDTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(0));
}

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3, AV1DISTWTDCOMPAVGUPSAMPLEDTest,
                         libaom_test::AV1DISTWTDCOMPAVG::BuildParams(
                             aom_dist_wtd_comp_avg_upsampled_pred_ssse3));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1DISTWTDCOMPAVGUPSAMPLEDTest,
                         libaom_test::AV1DISTWTDCOMPAVG::BuildParams(
                             aom_dist_wtd_comp_avg_upsampled_pred_neon));
#endif  // HAVE_NEON

TEST_P(DistWtdCompAvgTest, MaxRef) {
  FillConstant(reference_data_, reference_stride_, mask_);
  FillConstant(second_pred_, width_, 0);
  CheckCompAvg();
}

TEST_P(DistWtdCompAvgTest, MaxSecondPred) {
  FillConstant(reference_data_, reference_stride_, 0);
  FillConstant(second_pred_, width_, mask_);
  CheckCompAvg();
}

TEST_P(DistWtdCompAvgTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckCompAvg();
  reference_stride_ = tmp_stride;
}

TEST_P(DistWtdCompAvgTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckCompAvg();
  reference_stride_ = tmp_stride;
}

// TODO(chengchen): add highbd tests
const DistWtdCompAvgParam dist_wtd_comp_avg_c_tests[] = {
  make_tuple(128, 128, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(128, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(64, 128, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(64, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(64, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 4, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(4, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(4, 4, &aom_dist_wtd_comp_avg_pred_c, -1),

#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 4, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(4, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
#endif
};

INSTANTIATE_TEST_SUITE_P(C, DistWtdCompAvgTest,
                         ::testing::ValuesIn(dist_wtd_comp_avg_c_tests));

#if HAVE_SSSE3
const DistWtdCompAvgParam dist_wtd_comp_avg_ssse3_tests[] = {
  make_tuple(128, 128, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(128, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(64, 128, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(64, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(64, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 4, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(4, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(4, 4, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 4, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(4, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
#endif
};

INSTANTIATE_TEST_SUITE_P(SSSE3, DistWtdCompAvgTest,
                         ::testing::ValuesIn(dist_wtd_comp_avg_ssse3_tests));
#endif  // HAVE_SSSE3

#if HAVE_NEON
const DistWtdCompAvgParam dist_wtd_comp_avg_neon_tests[] = {
  make_tuple(128, 128, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(128, 64, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(64, 128, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(64, 64, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(64, 32, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(32, 64, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(32, 32, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(32, 16, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(16, 32, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(16, 8, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(8, 16, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(8, 8, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(8, 4, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(4, 8, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(4, 4, &aom_dist_wtd_comp_avg_pred_neon, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(16, 64, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(32, 8, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(8, 32, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(16, 4, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(4, 16, &aom_dist_wtd_comp_avg_pred_neon, -1),
#endif  // !CONFIG_REALTIME_ONLY
};

INSTANTIATE_TEST_SUITE_P(NEON, DistWtdCompAvgTest,
                         ::testing::ValuesIn(dist_wtd_comp_avg_neon_tests));
#endif  // HAVE_NEON

#if CONFIG_AV1_HIGHBITDEPTH
TEST_P(AV1HighBDDISTWTDCOMPAVGTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(1));
}

TEST_P(AV1HighBDDISTWTDCOMPAVGTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(1));
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1HighBDDISTWTDCOMPAVGTest,
                         libaom_test::AV1DISTWTDCOMPAVG::BuildParams(
                             aom_highbd_dist_wtd_comp_avg_pred_sse2, 1));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1HighBDDISTWTDCOMPAVGTest,
                         libaom_test::AV1DISTWTDCOMPAVG::BuildParams(
                             aom_highbd_dist_wtd_comp_avg_pred_neon, 1));
#endif

TEST_P(AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(1));
}

TEST_P(AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(1));
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest,
                         libaom_test::AV1DISTWTDCOMPAVG::BuildParams(
                             aom_highbd_dist_wtd_comp_avg_upsampled_pred_sse2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest,
                         libaom_test::AV1DISTWTDCOMPAVG::BuildParams(
                             aom_highbd_dist_wtd_comp_avg_upsampled_pred_neon));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace
