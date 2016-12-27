//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Shader.cpp: Implements the gl::Shader class and its  derived classes
// VertexShader and FragmentShader. Implements GL shader objects and related
// functionality. [OpenGL ES 2.0.24] section 2.10 page 24 and section 3.8 page 84.

#include "libANGLE/Shader.h"

#include <sstream>

#include "common/utilities.h"
#include "GLSLANG/ShaderLang.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Constants.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/ShaderImpl.h"
#include "libANGLE/ResourceManager.h"

namespace gl
{

namespace
{
template <typename VarT>
std::vector<VarT> GetActiveShaderVariables(const std::vector<VarT> *variableList)
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
const std::vector<VarT> &GetShaderVariables(const std::vector<VarT> *variableList)
{
    ASSERT(variableList);
    return *variableList;
}

}  // anonymous namespace

// true if varying x has a higher priority in packing than y
bool CompareShaderVar(const sh::ShaderVariable &x, const sh::ShaderVariable &y)
{
    if (x.type == y.type)
    {
        return x.arraySize > y.arraySize;
    }

    // Special case for handling structs: we sort these to the end of the list
    if (x.type == GL_STRUCT_ANGLEX)
    {
        return false;
    }

    if (y.type == GL_STRUCT_ANGLEX)
    {
        return true;
    }

    return gl::VariableSortOrder(x.type) < gl::VariableSortOrder(y.type);
}

ShaderState::ShaderState(GLenum shaderType) : mLabel(), mShaderType(shaderType), mShaderVersion(100)
{
    mLocalSize.fill(-1);
}

ShaderState::~ShaderState()
{
}

Shader::Shader(ResourceManager *manager,
               rx::GLImplFactory *implFactory,
               const gl::Limitations &rendererLimitations,
               GLenum type,
               GLuint handle)
    : mState(type),
      mImplementation(implFactory->createShader(mState)),
      mRendererLimitations(rendererLimitations),
      mHandle(handle),
      mType(type),
      mRefCount(0),
      mDeleteStatus(false),
      mCompiled(false),
      mResourceManager(manager)
{
    ASSERT(mImplementation);
}

Shader::~Shader()
{
    SafeDelete(mImplementation);
}

void Shader::setLabel(const std::string &label)
{
    mState.mLabel = label;
}

const std::string &Shader::getLabel() const
{
    return mState.mLabel;
}

GLuint Shader::getHandle() const
{
    return mHandle;
}

void Shader::setSource(GLsizei count, const char *const *string, const GLint *length)
{
    std::ostringstream stream;

    for (int i = 0; i < count; i++)
    {
        if (length == nullptr || length[i] < 0)
        {
            stream.write(string[i], strlen(string[i]));
        }
        else
        {
            stream.write(string[i], length[i]);
        }
    }

    mState.mSource = stream.str();
}

int Shader::getInfoLogLength() const
{
    if (mInfoLog.empty())
    {
        return 0;
    }

    return (static_cast<int>(mInfoLog.length()) + 1);
}

void Shader::getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const
{
    int index = 0;

    if (bufSize > 0)
    {
        index = std::min(bufSize - 1, static_cast<GLsizei>(mInfoLog.length()));
        memcpy(infoLog, mInfoLog.c_str(), index);

        infoLog[index] = '\0';
    }

    if (length)
    {
        *length = index;
    }
}

int Shader::getSourceLength() const
{
    return mState.mSource.empty() ? 0 : (static_cast<int>(mState.mSource.length()) + 1);
}

int Shader::getTranslatedSourceLength() const
{
    if (mState.mTranslatedSource.empty())
    {
        return 0;
    }

    return (static_cast<int>(mState.mTranslatedSource.length()) + 1);
}

int Shader::getTranslatedSourceWithDebugInfoLength() const
{
    const std::string &debugInfo = mImplementation->getDebugInfo();
    if (debugInfo.empty())
    {
        return 0;
    }

    return (static_cast<int>(debugInfo.length()) + 1);
}

void Shader::getSourceImpl(const std::string &source, GLsizei bufSize, GLsizei *length, char *buffer)
{
    int index = 0;

    if (bufSize > 0)
    {
        index = std::min(bufSize - 1, static_cast<GLsizei>(source.length()));
        memcpy(buffer, source.c_str(), index);

        buffer[index] = '\0';
    }

    if (length)
    {
        *length = index;
    }
}

void Shader::getSource(GLsizei bufSize, GLsizei *length, char *buffer) const
{
    getSourceImpl(mState.mSource, bufSize, length, buffer);
}

void Shader::getTranslatedSource(GLsizei bufSize, GLsizei *length, char *buffer) const
{
    getSourceImpl(mState.mTranslatedSource, bufSize, length, buffer);
}

void Shader::getTranslatedSourceWithDebugInfo(GLsizei bufSize, GLsizei *length, char *buffer) const
{
    const std::string &debugInfo = mImplementation->getDebugInfo();
    getSourceImpl(debugInfo, bufSize, length, buffer);
}

void Shader::compile(Compiler *compiler)
{
    mState.mTranslatedSource.clear();
    mInfoLog.clear();
    mState.mShaderVersion = 100;
    mState.mVaryings.clear();
    mState.mUniforms.clear();
    mState.mInterfaceBlocks.clear();
    mState.mActiveAttributes.clear();
    mState.mActiveOutputVariables.clear();

    ShHandle compilerHandle = compiler->getCompilerHandle(mState.mShaderType);

    std::stringstream sourceStream;

    std::string sourcePath;
    ShCompileOptions additionalOptions =
        mImplementation->prepareSourceAndReturnOptions(&sourceStream, &sourcePath);
    ShCompileOptions compileOptions = (SH_OBJECT_CODE | SH_VARIABLES | additionalOptions);

    // Some targets (eg D3D11 Feature Level 9_3 and below) do not support non-constant loop indexes
    // in fragment shaders. Shader compilation will fail. To provide a better error message we can
    // instruct the compiler to pre-validate.
    if (mRendererLimitations.shadersRequireIndexedLoopValidation)
    {
        compileOptions |= SH_VALIDATE_LOOP_INDEXING;
    }

    std::string sourceString  = sourceStream.str();
    std::vector<const char *> sourceCStrings;

    if (!sourcePath.empty())
    {
        sourceCStrings.push_back(sourcePath.c_str());
    }

    sourceCStrings.push_back(sourceString.c_str());

    bool result =
        ShCompile(compilerHandle, &sourceCStrings[0], sourceCStrings.size(), compileOptions);

    if (!result)
    {
        mInfoLog = ShGetInfoLog(compilerHandle);
        TRACE("\n%s", mInfoLog.c_str());
        mCompiled = false;
        return;
    }

    mState.mTranslatedSource = ShGetObjectCode(compilerHandle);

#ifndef NDEBUG
    // Prefix translated shader with commented out un-translated shader.
    // Useful in diagnostics tools which capture the shader source.
    std::ostringstream shaderStream;
    shaderStream << "// GLSL\n";
    shaderStream << "//\n";

    size_t curPos = 0;
    while (curPos != std::string::npos)
    {
        size_t nextLine = mState.mSource.find("\n", curPos);
        size_t len      = (nextLine == std::string::npos) ? std::string::npos : (nextLine - curPos + 1);

        shaderStream << "// " << mState.mSource.substr(curPos, len);

        curPos = (nextLine == std::string::npos) ? std::string::npos : (nextLine + 1);
    }
    shaderStream << "\n\n";
    shaderStream << mState.mTranslatedSource;
    mState.mTranslatedSource = shaderStream.str();
#endif

    // Gather the shader information
    mState.mShaderVersion = ShGetShaderVersion(compilerHandle);

    mState.mVaryings        = GetShaderVariables(ShGetVaryings(compilerHandle));
    mState.mUniforms        = GetShaderVariables(ShGetUniforms(compilerHandle));
    mState.mInterfaceBlocks = GetShaderVariables(ShGetInterfaceBlocks(compilerHandle));

    switch (mState.mShaderType)
    {
        case GL_COMPUTE_SHADER:
        {
            mState.mLocalSize = ShGetComputeShaderLocalGroupSize(compilerHandle);
            break;
        }
        case GL_VERTEX_SHADER:
        {
            mState.mActiveAttributes = GetActiveShaderVariables(ShGetAttributes(compilerHandle));
            break;
        }
        case GL_FRAGMENT_SHADER:
        {
            // TODO(jmadill): Figure out why we only sort in the FS, and if we need to.
            std::sort(mState.mVaryings.begin(), mState.mVaryings.end(), CompareShaderVar);
            mState.mActiveOutputVariables =
                GetActiveShaderVariables(ShGetOutputVariables(compilerHandle));
            break;
        }
        default:
            UNREACHABLE();
    }

    ASSERT(!mState.mTranslatedSource.empty());

    mCompiled = mImplementation->postTranslateCompile(compiler, &mInfoLog);
}

void Shader::addRef()
{
    mRefCount++;
}

void Shader::release()
{
    mRefCount--;

    if (mRefCount == 0 && mDeleteStatus)
    {
        mResourceManager->deleteShader(mHandle);
    }
}

unsigned int Shader::getRefCount() const
{
    return mRefCount;
}

bool Shader::isFlaggedForDeletion() const
{
    return mDeleteStatus;
}

void Shader::flagForDeletion()
{
    mDeleteStatus = true;
}

int Shader::getShaderVersion() const
{
    return mState.mShaderVersion;
}

const std::vector<sh::Varying> &Shader::getVaryings() const
{
    return mState.getVaryings();
}

const std::vector<sh::Uniform> &Shader::getUniforms() const
{
    return mState.getUniforms();
}

const std::vector<sh::InterfaceBlock> &Shader::getInterfaceBlocks() const
{
    return mState.getInterfaceBlocks();
}

const std::vector<sh::Attribute> &Shader::getActiveAttributes() const
{
    return mState.getActiveAttributes();
}

const std::vector<sh::OutputVariable> &Shader::getActiveOutputVariables() const
{
    return mState.getActiveOutputVariables();
}

int Shader::getSemanticIndex(const std::string &attributeName) const
{
    if (!attributeName.empty())
    {
        const auto &activeAttributes = mState.getActiveAttributes();

        int semanticIndex = 0;
        for (size_t attributeIndex = 0; attributeIndex < activeAttributes.size(); attributeIndex++)
        {
            const sh::ShaderVariable &attribute = activeAttributes[attributeIndex];

            if (attribute.name == attributeName)
            {
                return semanticIndex;
            }

            semanticIndex += gl::VariableRegisterCount(attribute.type);
        }
    }

    return -1;
}

}
