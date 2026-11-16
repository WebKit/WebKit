//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HashNames_test.cpp:
//   Tests for hashed identifier names in the translated GLSL output.
//

#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "gtest/gtest.h"
#include "tests/test_utils/compiler_test.h"

using namespace sh;

namespace
{

// A hash that depends only on the first character so that distinct identifiers
// beginning with the same letter map to the same hashed name.
khronos_uint64_t FirstCharHash(const char *str, size_t len)
{
    return len > 0 ? static_cast<khronos_uint64_t>(str[0]) : 0u;
}

class HashNamesTest : public MatchOutputCodeTest
{
  public:
    HashNamesTest() : MatchOutputCodeTest(GL_FRAGMENT_SHADER, SH_ESSL_OUTPUT)
    {
        getResources()->HashFunction = FirstCharHash;
    }
};

// When two distinct source identifiers map to the same hash value the
// translator must still emit distinct names so the inner declaration does not
// shadow the outer one in the translated output.
TEST_F(HashNamesTest, CollidingNamesEmitDistinctIdentifiers)
{
    const std::string &shaderString =
        R"(precision mediump float;
        void main()
        {
            float aOuter[8];
            aOuter[0] = 1.0;
            {
                float aInner[2];
                aInner[0] = 2.0;
                gl_FragColor = vec4(aOuter[0] + aInner[0]);
            }
        })";
    compile(shaderString);

    // The first identifier encountered keeps the bare hashed name.
    EXPECT_TRUE(foundInCode("webgl_61[8]"));
    // The second identifier must be emitted with a different name.
    EXPECT_FALSE(foundInCode("webgl_61[2]"));
}

// When more than two source identifiers map to the same hash value each must be
// emitted with its own distinct name.
TEST_F(HashNamesTest, MultipleCollidingNamesEmitDistinctIdentifiers)
{
    const std::string &shaderString =
        R"(precision mediump float;
        void main()
        {
            float aFirst[5];
            aFirst[0] = 1.0;
            float aSecond[3];
            aSecond[0] = 2.0;
            float aThird[7];
            aThird[0] = 3.0;
            gl_FragColor = vec4(aFirst[0] + aSecond[0] + aThird[0]);
        })";
    compile(shaderString);

    EXPECT_TRUE(foundInCode("webgl_61[5]"));
    EXPECT_FALSE(foundInCode("webgl_61[3]"));
    EXPECT_FALSE(foundInCode("webgl_61[7]"));
}

}  // anonymous namespace
