//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "GLSLANG/ShaderLang.h"
#include "gtest/gtest.h"

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

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new CompilerTestEnvironment());
    int rt = RUN_ALL_TESTS();
    return rt;
}
