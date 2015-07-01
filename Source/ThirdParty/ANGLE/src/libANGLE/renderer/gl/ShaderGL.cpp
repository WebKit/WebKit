//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderGL.cpp: Implements the class methods for ShaderGL.

#include "libANGLE/renderer/gl/ShaderGL.h"

#include "common/debug.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/renderer/gl/CompilerGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"

template <typename VarT>
static std::vector<VarT> GetFilteredShaderVariables(const std::vector<VarT> *variableList)
{
    ASSERT(variableList);
    std::vector<VarT> result;
    for (size_t varIndex = 0; varIndex < variableList->size(); varIndex++)
    {
        const VarT &var = variableList->at(varIndex);
        if (var.staticUse)
        {
            result.push_back(var);
        }
    }
    return result;
}

template <typename VarT>
static const std::vector<VarT> &GetShaderVariables(const std::vector<VarT> *variableList)
{
    ASSERT(variableList);
    return *variableList;
}

namespace rx
{

ShaderGL::ShaderGL(GLenum type, const FunctionsGL *functions)
    : ShaderImpl(),
      mFunctions(functions),
      mType(type),
      mShaderID(0)
{
    ASSERT(mFunctions);
}

ShaderGL::~ShaderGL()
{
    if (mShaderID != 0)
    {
        mFunctions->deleteShader(mShaderID);
        mShaderID = 0;
    }
}

bool ShaderGL::compile(gl::Compiler *compiler, const std::string &source)
{
    // Reset the previous state
    mActiveAttributes.clear();
    mVaryings.clear();
    mUniforms.clear();
    mInterfaceBlocks.clear();
    mActiveOutputVariables.clear();
    if (mShaderID != 0)
    {
        mFunctions->deleteShader(mShaderID);
        mShaderID = 0;
    }

    // Translate the ESSL into GLSL
    CompilerGL *compilerGL = GetImplAs<CompilerGL>(compiler);
    ShHandle compilerHandle = compilerGL->getCompilerHandle(mType);

    int compileOptions = (SH_OBJECT_CODE | SH_VARIABLES);
    const char* sourceCString = source.c_str();
    if (!ShCompile(compilerHandle, &sourceCString, 1, compileOptions))
    {
        mInfoLog = ShGetInfoLog(compilerHandle);
        TRACE("\n%s", mInfoLog.c_str());
        return false;
    }

    mTranslatedSource = ShGetObjectCode(compilerHandle);
    const char* translatedSourceCString = mTranslatedSource.c_str();

    // Generate a shader object and set the source
    mShaderID = mFunctions->createShader(mType);
    mFunctions->shaderSource(mShaderID, 1, &translatedSourceCString, nullptr);
    mFunctions->compileShader(mShaderID);

    // Check for compile errors from the native driver
    GLint compileStatus = GL_FALSE;
    mFunctions->getShaderiv(mShaderID, GL_COMPILE_STATUS, &compileStatus);
    ASSERT(compileStatus == GL_TRUE);
    if (compileStatus == GL_FALSE)
    {
        // Compilation failed, put the error into the info log
        GLint infoLogLength = 0;
        mFunctions->getShaderiv(mShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

        std::vector<char> buf(infoLogLength);
        mFunctions->getShaderInfoLog(mShaderID, infoLogLength, nullptr, &buf[0]);

        mFunctions->deleteShader(mShaderID);
        mShaderID = 0;

        mInfoLog = &buf[0];
        TRACE("\n%s", mInfoLog.c_str());
        return false;
    }

    // Gather the shader information
    // TODO: refactor this out, gathering of the attributes, varyings and outputs should be done
    // at the gl::Shader level
    if (mType == GL_VERTEX_SHADER)
    {
        mActiveAttributes = GetFilteredShaderVariables(ShGetAttributes(compilerHandle));
    }

    const std::vector<sh::Varying> &varyings = GetShaderVariables(ShGetVaryings(compilerHandle));
    for (size_t varyingIndex = 0; varyingIndex < varyings.size(); varyingIndex++)
    {
        mVaryings.push_back(gl::PackedVarying(varyings[varyingIndex]));
    }

    mUniforms = GetShaderVariables(ShGetUniforms(compilerHandle));
    mInterfaceBlocks = GetShaderVariables(ShGetInterfaceBlocks(compilerHandle));

    if (mType == GL_FRAGMENT_SHADER)
    {
        mActiveOutputVariables = GetFilteredShaderVariables(ShGetOutputVariables(compilerHandle));
    }

    return true;
}

std::string ShaderGL::getDebugInfo() const
{
    return std::string();
}

GLuint ShaderGL::getShaderID() const
{
    return mShaderID;
}

}
