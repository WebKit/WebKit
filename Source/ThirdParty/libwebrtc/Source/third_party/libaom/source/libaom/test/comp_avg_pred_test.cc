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
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1DISTWTDCOMPAVGTest);
using libaom_test::AV1DISTWTDCOMPAVG::AV1DISTWTDCOMPAVGUPSAMPLEDTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1DISTWTDCOMPAVGUPSAMPLEDTest);
#if CONFIG_AV1_HIGHBITDEPTH
using libaom_test::AV1DISTWTDCOMPAVG::AV1HighBDDISTWTDCOMPAVGTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighBDDISTWTDCOMPAVGTest);
using libaom_test::AV1DISTWTDCOMPAVG::AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest);
#endif
using std::make_tuple;
using std::tuple;

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
#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace
