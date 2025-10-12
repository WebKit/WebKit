//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "GLSLANG/ShaderLang.h"
#include "gtest/gtest.h"
#if defined(ANGLE_HAS_RAPIDJSON)
#    include "test_utils/runner/TestSuite.h"
#endif

class CompilerTestEnvironment : public testing::Environment
{
  public:
    void SetUp() override
    {
        if (!sh::Initialize())
        {
            FAIL() << "Failed to initialize the compiler.";
        }
    }

    void TearDown() override
    {
        if (!sh::Finalize())
        {
            FAIL() << "Failed to finalize the compiler.";
        }
    }
};

// This variable is also defined in test_utils_unittest_helper.
bool gVerbose = false;

int main(int argc, char **argv)
{
    for (int argIndex = 1; argIndex < argc; ++argIndex)
    {
        if (strcmp(argv[argIndex], "-v") == 0 || strcmp(argv[argIndex], "--verbose") == 0)
        {
            gVerbose = true;
        }
    }
#if defined(ANGLE_HAS_RAPIDJSON)
    angle::TestSuite testSuite(&argc, argv);
    testing::AddGlobalTestEnvironment(new CompilerTestEnvironment());
    return testSuite.run();
#else
    testing::AddGlobalTestEnvironment(new CompilerTestEnvironment());
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
#endif  // defined(ANGLE_HAS_RAPIDJSON)
}
