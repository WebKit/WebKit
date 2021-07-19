//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MSLOutput_test.cpp:
//   Tests for MSL output.
//

#include <regex>
#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "gtest/gtest.h"
#include "tests/test_utils/compiler_test.h"

using namespace sh;

class MSLVertexOutputTest : public MatchOutputCodeTest
{
  public:
    MSLVertexOutputTest() : MatchOutputCodeTest(GL_VERTEX_SHADER, 0, SH_MSL_METAL_OUTPUT) {}
};

class MSLOutputTest : public MatchOutputCodeTest
{
  public:
    MSLOutputTest() : MatchOutputCodeTest(GL_FRAGMENT_SHADER, 0, SH_MSL_METAL_OUTPUT) {}
};

TEST_F(MSLOutputTest, AnonymousStruct)
{
    const std::string &shaderString =
        R"(
        precision mediump float;
        struct { vec4 v; } anonStruct;
        void main() {
            anonStruct.v = vec4(0.0,1.0,0.0,1.0);
            gl_FragColor = anonStruct.v;
        })";
    compile(shaderString, SH_VARIABLES);
    // FIXME: This success condition is expected to fail now.
    // When WebKit build is able to run the tests, this should be changed to something else.
    ASSERT_TRUE(foundInCode(SH_MSL_METAL_OUTPUT, "__unnamed"));
}

