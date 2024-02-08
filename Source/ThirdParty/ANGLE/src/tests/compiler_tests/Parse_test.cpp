//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Parse_test.cpp:
//   Test for parsing erroneous and correct GLSL input.
//

#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "compiler/translator/glsl/TranslatorESSL.h"
#include "gtest/gtest.h"

using namespace sh;

class ParseTest : public testing::Test
{
  public:
    ParseTest() {}

  protected:
    void SetUp() override
    {
        ShBuiltInResources resources;
        InitBuiltInResources(&resources);
        resources.FragmentPrecisionHigh = 1;

        mTranslator = new TranslatorESSL(GL_FRAGMENT_SHADER, SH_GLES3_1_SPEC);
        ASSERT_TRUE(mTranslator->Init(resources));
    }

    void TearDown() override { delete mTranslator; }

    testing::AssertionResult compile(const std::string &shaderString)
    {
        ShCompileOptions compileOptions = {};
        compileOptions.intermediateTree = true;

        const char *shaderStrings[] = {shaderString.c_str()};
        bool compilationSuccess     = mTranslator->compile(shaderStrings, 1, compileOptions);
        TInfoSink &infoSink         = mTranslator->getInfoSink();
        mInfoLog                    = RemoveSymbolIdsFromInfoLog(infoSink.info.c_str());
        if (!compilationSuccess)
        {
            return testing::AssertionFailure() << "Shader compilation failed " << mInfoLog;
        }
        return testing::AssertionSuccess();
    }

    bool foundErrorInIntermediateTree() const { return foundInIntermediateTree("ERROR:"); }

    bool foundInIntermediateTree(const char *stringToFind) const
    {
        return mInfoLog.find(stringToFind) != std::string::npos;
    }

  private:
    // Remove symbol ids from info log - the tests don't care about them.
    static std::string RemoveSymbolIdsFromInfoLog(const char *infoLog)
    {
        std::string filteredLog(infoLog);
        size_t idPrefixPos = 0u;
        do
        {
            idPrefixPos = filteredLog.find(" (symbol id");
            if (idPrefixPos != std::string::npos)
            {
                size_t idSuffixPos = filteredLog.find(")", idPrefixPos);
                filteredLog.erase(idPrefixPos, idSuffixPos - idPrefixPos + 1u);
            }
        } while (idPrefixPos != std::string::npos);
        return filteredLog;
    }

    TranslatorESSL *mTranslator;
    std::string mInfoLog;
};

TEST_F(ParseTest, UnsizedArrayConstructorNoCrash)
{
    const char kShader[] = R"(#version 310 es\n"
int A[];
int B[int[][](A)];)";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("constructing from an unsized array"));
}
