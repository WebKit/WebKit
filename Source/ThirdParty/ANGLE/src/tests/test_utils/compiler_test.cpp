//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// compiler_test.cpp:
//     utilities for compiler unit tests.

#include "tests/test_utils/compiler_test.h"

#include "angle_gl.h"
#include "compiler/translator/Compiler.h"

namespace
{

class ShaderVariableFinder : public TIntermTraverser
{
  public:
    ShaderVariableFinder(const TString &variableName, TBasicType basicType)
        : TIntermTraverser(true, false, false),
          mVariableName(variableName),
          mNodeFound(nullptr),
          mBasicType(basicType)
    {
    }

    void visitSymbol(TIntermSymbol *node)
    {
        if (node->getBasicType() == mBasicType && node->getSymbol() == mVariableName)
        {
            mNodeFound = node;
        }
    }

    bool isFound() const { return mNodeFound != nullptr; }
    const TIntermSymbol *getNode() const { return mNodeFound; }

  private:
    TString mVariableName;
    TIntermSymbol *mNodeFound;
    TBasicType mBasicType;
};

}  // anonymous namespace

bool compileTestShader(GLenum type,
                       ShShaderSpec spec,
                       ShShaderOutput output,
                       const std::string &shaderString,
                       ShBuiltInResources *resources,
                       ShCompileOptions compileOptions,
                       std::string *translatedCode,
                       std::string *infoLog)
{
    TCompiler *translator = ConstructCompiler(type, spec, output);
    if (!translator->Init(*resources))
    {
        SafeDelete(translator);
        return false;
    }

    const char *shaderStrings[] = { shaderString.c_str() };

    bool compilationSuccess = translator->compile(shaderStrings, 1, SH_OBJECT_CODE | compileOptions);
    TInfoSink &infoSink = translator->getInfoSink();
    if (translatedCode)
        *translatedCode = infoSink.obj.c_str();
    if (infoLog)
        *infoLog = infoSink.info.c_str();
    SafeDelete(translator);
    return compilationSuccess;
}

bool compileTestShader(GLenum type,
                       ShShaderSpec spec,
                       ShShaderOutput output,
                       const std::string &shaderString,
                       ShCompileOptions compileOptions,
                       std::string *translatedCode,
                       std::string *infoLog)
{
    ShBuiltInResources resources;
    ShInitBuiltInResources(&resources);
    return compileTestShader(type, spec, output, shaderString, &resources, compileOptions, translatedCode, infoLog);
}

MatchOutputCodeTest::MatchOutputCodeTest(GLenum shaderType,
                                         ShCompileOptions defaultCompileOptions,
                                         ShShaderOutput outputType)
    : mShaderType(shaderType), mDefaultCompileOptions(defaultCompileOptions)
{
    ShInitBuiltInResources(&mResources);
    mOutputCode[outputType] = std::string();
}

void MatchOutputCodeTest::addOutputType(const ShShaderOutput outputType)
{
    mOutputCode[outputType] = std::string();
}

ShBuiltInResources *MatchOutputCodeTest::getResources()
{
    return &mResources;
}

void MatchOutputCodeTest::compile(const std::string &shaderString)
{
    compile(shaderString, mDefaultCompileOptions);
}

void MatchOutputCodeTest::compile(const std::string &shaderString,
                                  const ShCompileOptions compileOptions)
{
    std::string infoLog;
    for (auto &code : mOutputCode)
    {
        bool compilationSuccess =
            compileWithSettings(code.first, shaderString, compileOptions, &code.second, &infoLog);
        if (!compilationSuccess)
        {
            FAIL() << "Shader compilation failed:\n" << infoLog;
        }
    }
}

bool MatchOutputCodeTest::compileWithSettings(ShShaderOutput output,
                                              const std::string &shaderString,
                                              const ShCompileOptions compileOptions,
                                              std::string *translatedCode,
                                              std::string *infoLog)
{
    return compileTestShader(mShaderType, SH_GLES3_SPEC, output, shaderString, &mResources,
                             compileOptions, translatedCode, infoLog);
}

bool MatchOutputCodeTest::foundInCode(ShShaderOutput output, const char *stringToFind) const
{
    const auto code = mOutputCode.find(output);
    EXPECT_NE(mOutputCode.end(), code);
    if (code == mOutputCode.end())
    {
        return false;
    }

    return code->second.find(stringToFind) != std::string::npos;
}

bool MatchOutputCodeTest::foundInCode(ShShaderOutput output,
                                      const char *stringToFind,
                                      const int expectedOccurrences) const
{
    const auto code = mOutputCode.find(output);
    EXPECT_NE(mOutputCode.end(), code);
    if (code == mOutputCode.end())
    {
        return false;
    }

    size_t currentPos  = 0;
    int occurencesLeft = expectedOccurrences;
    while (occurencesLeft-- > 0)
    {
        auto position = code->second.find(stringToFind, currentPos);
        if (position == std::string::npos)
        {
            return false;
        }
        currentPos = position + 1;
    }
    return code->second.find(stringToFind, currentPos) == std::string::npos;
}

bool MatchOutputCodeTest::foundInCode(const char *stringToFind) const
{
    for (auto &code : mOutputCode)
    {
        if (!foundInCode(code.first, stringToFind))
        {
            return false;
        }
    }
    return true;
}

bool MatchOutputCodeTest::foundInCode(const char *stringToFind, const int expectedOccurrences) const
{
    for (auto &code : mOutputCode)
    {
        if (!foundInCode(code.first, stringToFind, expectedOccurrences))
        {
            return false;
        }
    }
    return true;
}

bool MatchOutputCodeTest::notFoundInCode(const char *stringToFind) const
{
    for (auto &code : mOutputCode)
    {
        if (foundInCode(code.first, stringToFind))
        {
            return false;
        }
    }
    return true;
}

const TIntermSymbol *FindSymbolNode(TIntermNode *root,
                                    const TString &symbolName,
                                    TBasicType basicType)
{
    ShaderVariableFinder finder(symbolName, basicType);
    root->traverse(&finder);
    return finder.getNode();
}
