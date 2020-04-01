/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/resource_adaptation_processor.h"

#include "test/gmock.h"
#include "test/gtest.h"
#include "video/adaptation/adaptation_counters.h"

namespace webrtc {

TEST(ResourceAdaptationProcessorTest, FirstAdaptationDown_Fps) {
  AdaptationCounters cpu;
  AdaptationCounters qp;
  AdaptationCounters total(0, 1);

  ResourceAdaptationProcessor::OnAdaptationCountChanged(total, &cpu, &qp);
  AdaptationCounters expected_cpu(0, 1);
  AdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(ResourceAdaptationProcessorTest, FirstAdaptationDown_Resolution) {
  AdaptationCounters cpu;
  AdaptationCounters qp;
  AdaptationCounters total(1, 0);

  ResourceAdaptationProcessor::OnAdaptationCountChanged(total, &cpu, &qp);
  AdaptationCounters expected_cpu(1, 0);
  AdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(ResourceAdaptationProcessorTest, LastAdaptUp_Fps) {
  AdaptationCounters cpu(0, 1);
  AdaptationCounters qp;
  AdaptationCounters total;

  ResourceAdaptationProcessor::OnAdaptationCountChanged(total, &cpu, &qp);
  AdaptationCounters expected_cpu;
  AdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(ResourceAdaptationProcessorTest, LastAdaptUp_Resolution) {
  AdaptationCounters cpu(1, 0);
  AdaptationCounters qp;
  AdaptationCounters total;

  ResourceAdaptationProcessor::OnAdaptationCountChanged(total, &cpu, &qp);
  AdaptationCounters expected_cpu;
  AdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(ResourceAdaptationProcessorTest, AdaptUpWithBorrow_Resolution) {
  AdaptationCounters cpu(0, 1);
  AdaptationCounters qp(1, 0);
  AdaptationCounters total(0, 1);

  // CPU adaptation for resolution, but no resolution adaptation left from CPU.
  // We then borrow the resolution adaptation from qp, and give qp the fps
  // adaptation from CPU.
  ResourceAdaptationProcessor::OnAdaptationCountChanged(total, &cpu, &qp);

  AdaptationCounters expected_cpu(0, 0);
  AdaptationCounters expected_qp(0, 1);
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(ResourceAdaptationProcessorTest, AdaptUpWithBorrow_Fps) {
  AdaptationCounters cpu(1, 0);
  AdaptationCounters qp(0, 1);
  AdaptationCounters total(1, 0);

  // CPU adaptation for fps, but no fps adaptation left from CPU. We then borrow
  // the fps adaptation from qp, and give qp the resolution adaptation from CPU.
  ResourceAdaptationProcessor::OnAdaptationCountChanged(total, &cpu, &qp);

  AdaptationCounters expected_cpu(0, 0);
  AdaptationCounters expected_qp(1, 0);
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

}  // namespace webrtc
