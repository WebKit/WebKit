//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// QualificationOrderESSL31_test.cpp:
//   OpenGL ES 3.1 removes the strict order of qualifiers imposed by the grammar.
//   This file contains tests for invalid order and usage of qualifiers in GLSL ES 3.10.

#include "gtest/gtest.h"

#include "angle_gl.h"
#include "compiler/translator/TranslatorESSL.h"
#include "GLSLANG/ShaderLang.h"
#include "tests/test_utils/compiler_test.h"

class QualificationVertexShaderTestESSL31 : public testing::Test
{
  public:
    QualificationVertexShaderTestESSL31() {}
  protected:
    virtual void SetUp()
    {
        ShBuiltInResources resources;
        ShInitBuiltInResources(&resources);

        mTranslator = new TranslatorESSL(GL_VERTEX_SHADER, SH_GLES3_1_SPEC);
        ASSERT_TRUE(mTranslator->Init(resources));
    }

    virtual void TearDown() { delete mTranslator; }

    // Return true when compilation succeeds
    bool compile(const std::string &shaderString)
    {
        const char *shaderStrings[] = {shaderString.c_str()};
        mASTRoot                    = mTranslator->compileTreeForTesting(shaderStrings, 1,
                                                      SH_INTERMEDIATE_TREE | SH_VARIABLES);
        TInfoSink &infoSink = mTranslator->getInfoSink();
        mInfoLog            = infoSink.info.c_str();
        return mASTRoot != nullptr;
    }

    const TIntermSymbol *findSymbolInAST(const TString &symbolName, TBasicType basicType)
    {
        return FindSymbolNode(mASTRoot, symbolName, basicType);
    }

  protected:
    TranslatorESSL *mTranslator;
    TIntermNode *mASTRoot;
    std::string mInfoLog;
};

// GLSL ES 3.10 has relaxed checks on qualifier order. Any order is correct.
TEST_F(QualificationVertexShaderTestESSL31, CentroidOut)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision lowp float;\n"
        "out centroid float something;\n"
        "void main(){\n"
        "   something = 1.0;\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success" << mInfoLog;
    }
    else
    {
        const TIntermSymbol *node = findSymbolInAST("something", EbtFloat);
        ASSERT_NE(nullptr, node);

        const TType &type = node->getType();
        EXPECT_EQ(EvqCentroidOut, type.getQualifier());
    }
}

// GLSL ES 3.10 has relaxed checks on qualifier order. Any order is correct.
TEST_F(QualificationVertexShaderTestESSL31, AllQualifiersMixed)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision lowp float;\n"
        "highp out invariant centroid flat vec4 something;\n"
        "void main(){\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success" << mInfoLog;
    }
    else
    {
        const TIntermSymbol *node = findSymbolInAST("something", EbtFloat);
        ASSERT_NE(nullptr, node);

        const TType &type = node->getType();
        EXPECT_TRUE(type.isInvariant());
        EXPECT_EQ(EvqFlatOut, type.getQualifier());
        EXPECT_EQ(EbpHigh, type.getPrecision());
    }
}

// GLSL ES 3.10 allows multiple layout qualifiers to be specified.
TEST_F(QualificationVertexShaderTestESSL31, MultipleLayouts)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision lowp float;\n"
        "in layout(location=1) layout(location=2) vec4 something;\n"
        "void main(){\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success" << mInfoLog;
    }
    else
    {
        const TIntermSymbol *node = findSymbolInAST("something", EbtFloat);
        ASSERT_NE(nullptr, node);

        const TType &type = node->getType();
        EXPECT_EQ(EvqVertexIn, type.getQualifier());
        EXPECT_EQ(2, type.getLayoutQualifier().location);
    }
}

// The test checks layout qualifier overriding when multiple layouts are specified.
TEST_F(QualificationVertexShaderTestESSL31, MultipleLayoutsInterfaceBlock)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision lowp float;\n"
        "out float someValue;\n"
        "layout(shared) layout(std140) layout(column_major) uniform MyInterface\n"
        "{ vec4 something; } MyInterfaceName;\n"
        "void main(){\n"
        "   someValue = MyInterfaceName.something.r;\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success" << mInfoLog;
    }
    else
    {
        const TIntermSymbol *node = findSymbolInAST("MyInterfaceName", EbtInterfaceBlock);
        ASSERT_NE(nullptr, node);

        const TType &type                = node->getType();
        TLayoutQualifier layoutQualifier = type.getLayoutQualifier();
        EXPECT_EQ(EbsStd140, layoutQualifier.blockStorage);
        EXPECT_EQ(EmpColumnMajor, layoutQualifier.matrixPacking);
    }
}

// The test checks layout qualifier overriding when multiple layouts are specified.
TEST_F(QualificationVertexShaderTestESSL31, MultipleLayoutsInterfaceBlock2)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision lowp float;\n"
        "out float someValue;\n"
        "layout(row_major) layout(std140) layout(shared) uniform MyInterface\n"
        "{ vec4 something; } MyInterfaceName;\n"
        "void main(){\n"
        "   someValue = MyInterfaceName.something.r;\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success" << mInfoLog;
    }
    else
    {
        const TIntermSymbol *node = findSymbolInAST("MyInterfaceName", EbtInterfaceBlock);
        ASSERT_NE(nullptr, node);

        const TType &type                = node->getType();
        TLayoutQualifier layoutQualifier = type.getLayoutQualifier();
        EXPECT_EQ(EbsShared, layoutQualifier.blockStorage);
        EXPECT_EQ(EmpRowMajor, layoutQualifier.matrixPacking);
    }
}
