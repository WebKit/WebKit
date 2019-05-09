//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ConstantFoldingTest.cpp:
//   Utilities for constant folding tests.
//

#include "tests/test_utils/ConstantFoldingTest.h"

#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "compiler/translator/TranslatorESSL.h"

using namespace sh;

void ConstantFoldingExpressionTest::evaluateFloat(const std::string &floatExpression)
{
    // We first assign the expression into a const variable so we can also verify that it gets
    // qualified as a constant expression. We then assign that constant expression into my_FragColor
    // to make sure that the value is not pruned.
    std::stringstream shaderStream;
    shaderStream << "#version 310 es\n"
                    "precision mediump float;\n"
                    "out float my_FragColor;\n"
                    "void main()\n"
                    "{\n"
                 << "    const float f = " << floatExpression << ";\n"
                 << "    my_FragColor = f;\n"
                    "}\n";
    compileAssumeSuccess(shaderStream.str());
}

void ConstantFoldingExpressionTest::evaluateInt(const std::string &intExpression)
{
    // We first assign the expression into a const variable so we can also verify that it gets
    // qualified as a constant expression. We then assign that constant expression into my_FragColor
    // to make sure that the value is not pruned.
    std::stringstream shaderStream;
    shaderStream << "#version 310 es\n"
                    "precision mediump int;\n"
                    "out int my_FragColor;\n"
                    "void main()\n"
                    "{\n"
                 << "    const int i = " << intExpression << ";\n"
                 << "    my_FragColor = i;\n"
                    "}\n";
    compileAssumeSuccess(shaderStream.str());
}

void ConstantFoldingExpressionTest::evaluateUint(const std::string &uintExpression)
{
    // We first assign the expression into a const variable so we can also verify that it gets
    // qualified as a constant expression. We then assign that constant expression into my_FragColor
    // to make sure that the value is not pruned.
    std::stringstream shaderStream;
    shaderStream << "#version 310 es\n"
                    "precision mediump int;\n"
                    "out uint my_FragColor;\n"
                    "void main()\n"
                    "{\n"
                 << "    const uint u = " << uintExpression << ";\n"
                 << "    my_FragColor = u;\n"
                    "}\n";
    compileAssumeSuccess(shaderStream.str());
}
