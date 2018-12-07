/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/linear_least_squares.h"

#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(LinearLeastSquares, ScalarIdentityOneObservation) {
  IncrementalLinearLeastSquares lls;
  lls.AddObservations({{1}}, {{1}});
  EXPECT_EQ(std::vector<std::vector<double>>({{1.0}}), lls.GetBestSolution());
}

TEST(LinearLeastSquares, ScalarIdentityTwoObservationsOneCall) {
  IncrementalLinearLeastSquares lls;
  lls.AddObservations({{1, 2}}, {{1, 2}});
  EXPECT_EQ(std::vector<std::vector<double>>({{1.0}}), lls.GetBestSolution());
}

TEST(LinearLeastSquares, ScalarIdentityTwoObservationsTwoCalls) {
  IncrementalLinearLeastSquares lls;
  lls.AddObservations({{1}}, {{1}});
  lls.AddObservations({{2}}, {{2}});
  EXPECT_EQ(std::vector<std::vector<double>>({{1.0}}), lls.GetBestSolution());
}

TEST(LinearLeastSquares, MatrixIdentityOneObservation) {
  IncrementalLinearLeastSquares lls;
  lls.AddObservations({{1, 2}, {3, 4}}, {{1, 2}, {3, 4}});
  EXPECT_EQ(std::vector<std::vector<double>>({{1.0, 0.0}, {0.0, 1.0}}),
            lls.GetBestSolution());
}

TEST(LinearLeastSquares, MatrixManyObservations) {
  IncrementalLinearLeastSquares lls;
  // Test that we can find the solution of the overspecified equation system:
  // [1, 2] [1, 3] = [5,  11]
  // [3, 4] [2, 4]   [11, 25]
  // [5, 6]          [17, 39]
  lls.AddObservations({{1}, {2}}, {{5}, {11}});
  lls.AddObservations({{3}, {4}}, {{11}, {25}});
  lls.AddObservations({{5}, {6}}, {{17}, {39}});

  const std::vector<std::vector<double>> result = lls.GetBestSolution();
  // We allow some numerical flexibility here.
  EXPECT_DOUBLE_EQ(1.0, result[0][0]);
  EXPECT_DOUBLE_EQ(2.0, result[0][1]);
  EXPECT_DOUBLE_EQ(3.0, result[1][0]);
  EXPECT_DOUBLE_EQ(4.0, result[1][1]);
}

TEST(LinearLeastSquares, MatrixVectorOneObservation) {
  IncrementalLinearLeastSquares lls;
  // Test that we can find the solution of the overspecified equation system:
  // [1, 2] [1] = [5]
  // [3, 4] [2]   [11]
  // [5, 6]       [17]
  lls.AddObservations({{1, 3, 5}, {2, 4, 6}}, {{5, 11, 17}});

  const std::vector<std::vector<double>> result = lls.GetBestSolution();
  // We allow some numerical flexibility here.
  EXPECT_DOUBLE_EQ(1.0, result[0][0]);
  EXPECT_DOUBLE_EQ(2.0, result[0][1]);
}

TEST(LinearLeastSquares, LinearLeastSquaresNonPerfectSolution) {
  IncrementalLinearLeastSquares lls;
  // Test that we can find the non-perfect solution of the overspecified
  // equation system:
  // [1] [20] = [21]
  // [2]        [39]
  // [3]        [60]
  // [2]        [41]
  // [1]        [19]
  lls.AddObservations({{1, 2, 3, 2, 1}}, {{21, 39, 60, 41, 19}});

  EXPECT_DOUBLE_EQ(20.0, lls.GetBestSolution()[0][0]);
}

}  // namespace test
}  // namespace webrtc
