//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// angle_perftests_main.cpp
//   Entry point for the gtest-based performance tests.
//

#include <gtest/gtest.h>

extern bool g_OnlyOneRunFrame;

int main(int argc, char **argv)
{
    for (int i = 0; i < argc; ++i)
    {
        if (strcmp("--one-frame-only", argv[i]) == 0)
        {
            g_OnlyOneRunFrame = true;
        }
    }

    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new testing::Environment());
    int rt = RUN_ALL_TESTS();
    return rt;
}
