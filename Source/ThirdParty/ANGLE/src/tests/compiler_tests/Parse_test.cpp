//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Parse_test.cpp:
//   Test for parsing erroneous and correct GLSL input.
//

#include <memory>
#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "compiler/translator/glsl/TranslatorESSL.h"
#include "gtest/gtest.h"

using namespace sh;

class ParseTest : public testing::Test
{
  public:
    ParseTest()
    {
        InitBuiltInResources(&mResources);
        mResources.FragmentPrecisionHigh = 1;
        mCompileOptions.intermediateTree = true;
    }

  protected:
    void TearDown() override { mTranslator.reset(); }

    testing::AssertionResult compile(const std::string &shaderString)
    {
        if (mTranslator == nullptr)
        {
            std::unique_ptr<TranslatorESSL> translator =
                std::make_unique<TranslatorESSL>(GL_FRAGMENT_SHADER, SH_WEBGL_SPEC);
            if (!translator->Init(mResources))
            {
                return testing::AssertionFailure() << "Failed to initialize translator";
            }
            mTranslator = std::move(translator);
        }

        const char *shaderStrings[] = {shaderString.c_str()};
        bool compilationSuccess     = mTranslator->compile(shaderStrings, 1, mCompileOptions);
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

    ShBuiltInResources mResources;
    ShCompileOptions mCompileOptions{};

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

    std::unique_ptr<TranslatorESSL> mTranslator;
    std::string mInfoLog;
};

TEST_F(ParseTest, CoherentCoherentNoCrash)
{
    const char kShader[] = R"(#version 310 es
coherent coherent;)";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("coherent specified multiple times"));
}

TEST_F(ParseTest, UnsizedArrayConstructorNoCrash)
{
    const char kShader[] = R"(#version 310 es\n"
int A[];
int B[int[][](A)];)";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("constructing from an unsized array"));
}

TEST_F(ParseTest, UniformBlockNameReferenceNoCrash)
{
    const char kShader[] = R"(#version 300 es
precision mediump float;
out float o;
uniform a { float r; } UBOA;
void main() {
    o = float(UBOA);
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree(
        "interface block cannot be used as a constructor argument for this type"));
}

TEST_F(ParseTest, Precise320NoCrash)
{
    const char kShader[] = R"(#version 320 es
precision mediump float;
void main(){
    float t;
    precise t;
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("unsupported shader version"));
}

// Tests that layout(index=0) is parsed in es 100 shaders if an extension like
// EXT_shader_framebuffer_fetch is enabled, but this does not cause a crash.
TEST_F(ParseTest, ShaderFramebufferFetchLayoutIndexNoCrash)
{
    mResources.EXT_blend_func_extended      = 1;
    mResources.MaxDualSourceDrawBuffers     = 1;
    mResources.EXT_shader_framebuffer_fetch = 1;
    const char kShader[]                    = R"(
#extension GL_EXT_blend_func_extended: require
#extension GL_EXT_shader_framebuffer_fetch : require
layout(index=0)mediump vec4 c;
void main() { }
)";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("'index' : invalid layout qualifier"));
}
