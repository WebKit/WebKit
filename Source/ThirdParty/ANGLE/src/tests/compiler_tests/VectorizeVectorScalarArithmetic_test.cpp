//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VectorizeVectorScalarArithmetic_test.cpp:
//  Tests shader compilation with SH_REWRITE_VECTOR_SCALAR_ARITHMETIC workaround on.

#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "gtest/gtest.h"
#include "tests/test_utils/ShaderCompileTreeTest.h"

using namespace sh;

class VectorizeVectorScalarArithmeticTest : public ShaderCompileTreeTest
{
  public:
    VectorizeVectorScalarArithmeticTest() : ShaderCompileTreeTest()
    {
        mExtraCompileOptions = SH_REWRITE_VECTOR_SCALAR_ARITHMETIC;
    }

  protected:
    ::GLenum getShaderType() const override { return GL_FRAGMENT_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_GLES3_1_SPEC; }
};

// Test that two ops that generate statements in the parent block inside the same statement don't
// trigger an assert.
TEST_F(VectorizeVectorScalarArithmeticTest, TwoMutatedOpsWithSideEffectsInsideSameStatement)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        out vec4 res;
        uniform float uf;

        void main()
        {
            res = vec4(0.0);
            float f = uf;
            res += f *= f, res += f *= f;
        })";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}
