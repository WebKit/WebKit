//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ConstantFoldingTest.cpp:
//   Utilities for constant folding tests.
//

#include "tests/test_utils/ConstantFoldingTest.h"

#include "angle_gl.h"
#include "compiler/translator/TranslatorESSL.h"
#include "GLSLANG/ShaderLang.h"

using namespace sh;

void ConstantFoldingExpressionTest::evaluateFloat(const std::string &floatExpression)
{
    std::stringstream shaderStream;
    shaderStream << "#version 310 es\n"
                    "precision mediump float;\n"
                    "out float my_FragColor;\n"
                    "void main()\n"
                    "{\n"
                 << "    my_FragColor = " << floatExpression << ";\n"
                 << "}\n";
    compileAssumeSuccess(shaderStream.str());
}

void ConstantFoldingExpressionTest::evaluateInt(const std::string &intExpression)
{
    std::stringstream shaderStream;
    shaderStream << "#version 310 es\n"
                    "precision mediump int;\n"
                    "out int my_FragColor;\n"
                    "void main()\n"
                    "{\n"
                 << "    my_FragColor = " << intExpression << ";\n"
                 << "}\n";
    compileAssumeSuccess(shaderStream.str());
}

void ConstantFoldingExpressionTest::evaluateUint(const std::string &uintExpression)
{
    std::stringstream shaderStream;
    shaderStream << "#version 310 es\n"
                    "precision mediump int;\n"
                    "out uint my_FragColor;\n"
                    "void main()\n"
                    "{\n"
                 << "    my_FragColor = " << uintExpression << ";\n"
                 << "}\n";
    compileAssumeSuccess(shaderStream.str());
}
