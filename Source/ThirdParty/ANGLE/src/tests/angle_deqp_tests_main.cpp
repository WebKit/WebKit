//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// angle_deqp_gtest_main:
//   Entry point for standalone dEQP tests.

#include <gtest/gtest.h>

// Defined in angle_deqp_gtest.cpp. Declared here so we don't need to make a header that we import
// in Chromium.
namespace angle
{
void InitTestHarness(int *argc, char **argv);
}  // namespace angle

int main(int argc, char **argv)
{
    angle::InitTestHarness(&argc, argv);
    testing::InitGoogleTest(&argc, argv);
    int rt = RUN_ALL_TESTS();
    return rt;
}
