/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/hiprec_convolve_test_util.h"

using libaom_test::ACMRandom;
#if CONFIG_AV1_HIGHBITDEPTH
using libaom_test::AV1HighbdHiprecConvolve::AV1HighbdHiprecConvolveTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighbdHiprecConvolveTest);
#endif
using libaom_test::AV1HiprecConvolve::AV1HiprecConvolveTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HiprecConvolveTest);
using std::make_tuple;
using std::tuple;

namespace {

TEST_P(AV1HiprecConvolveTest, CheckOutput) { RunCheckOutput(GET_PARAM(3)); }
TEST_P(AV1HiprecConvolveTest, DISABLED_SpeedTest) {
  RunSpeedTest(GET_PARAM(3));
}
#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1HiprecConvolveTest,
                         libaom_test::AV1HiprecConvolve::BuildParams(
                             av1_wiener_convolve_add_src_sse2));
#endif
#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1HiprecConvolveTest,
                         libaom_test::AV1HiprecConvolve::BuildParams(
                             av1_wiener_convolve_add_src_avx2));
#endif
#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1HiprecConvolveTest,
                         libaom_test::AV1HiprecConvolve::BuildParams(
                             av1_wiener_convolve_add_src_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
#if HAVE_SSSE3 || HAVE_AVX2
TEST_P(AV1HighbdHiprecConvolveTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(4));
}
TEST_P(AV1HighbdHiprecConvolveTest, DISABLED_SpeedTest) {
  RunSpeedTest(GET_PARAM(4));
}
#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3, AV1HighbdHiprecConvolveTest,
                         libaom_test::AV1HighbdHiprecConvolve::BuildParams(
                             av1_highbd_wiener_convolve_add_src_ssse3));
#endif
#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1HighbdHiprecConvolveTest,
                         libaom_test::AV1HighbdHiprecConvolve::BuildParams(
                             av1_highbd_wiener_convolve_add_src_avx2));
#endif
#endif
#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace
