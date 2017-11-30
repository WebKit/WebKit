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
#include "libANGLE/Context.h"

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
        return x.getArraySizeProduct() > y.getArraySizeProduct();
    }

    // Special case for handling structs: we sort these to the end of the list
    if (x.type == GL_NONE)
    {
        return false;
    }

    if (y.type == GL_NONE)
    {
        return true;
    }

    return gl::VariableSortOrder(x.type) < gl::VariableSortOrder(y.type);
}

ShaderState::ShaderState(GLenum shaderType)
    : mLabel(),
      mShaderType(shaderType),
      mShaderVersion(100),
      mNumViews(-1),
      mGeometryShaderInputPrimitiveType(GL_INVALID_VALUE),
      mGeometryShaderOutputPrimitiveType(GL_INVALID_VALUE),
      mGeometryShaderInvocations(1),
      mGeometryShaderMaxVertices(-1),
      mCompileStatus(CompileStatus::NOT_COMPILED)
{
    mLocalSize.fill(-1);
}

ShaderState::~ShaderState()
{
}

Shader::Shader(ShaderProgramManager *manager,
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
      mResourceManager(manager)
{
    ASSERT(mImplementation);
}

void Shader::onDestroy(const gl::Context *context)
{
    mBoundCompiler.set(context, nullptr);
    mImplementation.reset(nullptr);
    delete this;
}

Shader::~Shader()
{
    ASSERT(!mImplementation);
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

int Shader::getInfoLogLength(const Context *context)
{
    resolveCompile(context);
    if (mInfoLog.empty())
    {
        return 0;
    }

    return (static_cast<int>(mInfoLog.length()) + 1);
}

void Shader::getInfoLog(const Context *context, GLsizei bufSize, GLsizei *length, char *infoLog)
{
    resolveCompile(context);

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

int Shader::getTranslatedSourceLength(const Context *context)
{
    resolveCompile(context);

    if (mState.mTranslatedSource.empty())
    {
        return 0;
    }

    return (static_cast<int>(mState.mTranslatedSource.length()) + 1);
}

int Shader::getTranslatedSourceWithDebugInfoLength(const Context *context)
{
    resolveCompile(context);

    const std::string &debugInfo = mImplementation->getDebugInfo();
    if (debugInfo.empty())
    {
        return 0;
    }

    return (static_cast<int>(debugInfo.length()) + 1);
}

// static
void Shader::GetSourceImpl(const std::string &source,
                           GLsizei bufSize,
                           GLsizei *length,
                           char *buffer)
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
    GetSourceImpl(mState.mSource, bufSize, length, buffer);
}

void Shader::getTranslatedSource(const Context *context,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 char *buffer)
{
    GetSourceImpl(getTranslatedSource(context), bufSize, length, buffer);
}

const std::string &Shader::getTranslatedSource(const Context *context)
{
    resolveCompile(context);
    return mState.mTranslatedSource;
}

void Shader::getTranslatedSourceWithDebugInfo(const Context *context,
                                              GLsizei bufSize,
                                              GLsizei *length,
                                              char *buffer)
{
    resolveCompile(context);
    const std::string &debugInfo = mImplementation->getDebugInfo();
    GetSourceImpl(debugInfo, bufSize, length, buffer);
}

void Shader::compile(const Context *context)
{
    mState.mTranslatedSource.clear();
    mInfoLog.clear();
    mState.mShaderVersion = 100;
    mState.mInputVaryings.clear();
    mState.mOutputVaryings.clear();
    mState.mUniforms.clear();
    mState.mUniformBlocks.clear();
    mState.mShaderStorageBlocks.clear();
    mState.mActiveAttributes.clear();
    mState.mActiveOutputVariables.clear();
    mState.mNumViews = -1;
    mState.mGeometryShaderInputPrimitiveType  = GL_INVALID_VALUE;
    mState.mGeometryShaderOutputPrimitiveType = GL_INVALID_VALUE;
    mState.mGeometryShaderInvocations         = 1;
    mState.mGeometryShaderMaxVertices         = -1;

    mState.mCompileStatus = CompileStatus::COMPILE_REQUESTED;
    mBoundCompiler.set(context, context->getCompiler());

    // Cache the compile source and options for compilation. Must be done now, since the source
    // can change before the link call or another call that resolves the compile.

    std::stringstream sourceStream;

    mLastCompileOptions =
        mImplementation->prepareSourceAndReturnOptions(&sourceStream, &mLastCompiledSourcePath);
    mLastCompileOptions |= (SH_OBJECT_CODE | SH_VARIABLES);
    mLastCompiledSource = sourceStream.str();

    // Add default options to WebGL shaders to prevent unexpected behavior during compilation.
    if (context->getExtensions().webglCompatibility)
    {
        mLastCompileOptions |= SH_INIT_GL_POSITION;
        mLastCompileOptions |= SH_LIMIT_CALL_STACK_DEPTH;
        mLastCompileOptions |= SH_LIMIT_EXPRESSION_COMPLEXITY;
        mLastCompileOptions |= SH_ENFORCE_PACKING_RESTRICTIONS;
    }

    // Some targets (eg D3D11 Feature Level 9_3 and below) do not support non-constant loop indexes
    // in fragment shaders. Shader compilation will fail. To provide a better error message we can
    // instruct the compiler to pre-validate.
    if (mRendererLimitations.shadersRequireIndexedLoopValidation)
    {
        mLastCompileOptions |= SH_VALIDATE_LOOP_INDEXING;
    }
}

void Shader::resolveCompile(const Context *context)
{
    if (!mState.compilePending())
    {
        return;
    }

    ASSERT(mBoundCompiler.get());
    ShHandle compilerHandle = mBoundCompiler->getCompilerHandle(mState.mShaderType);

    std::vector<const char *> srcStrings;

    if (!mLastCompiledSourcePath.empty())
    {
        srcStrings.push_back(mLastCompiledSourcePath.c_str());
    }

    srcStrings.push_back(mLastCompiledSource.c_str());

    if (!sh::Compile(compilerHandle, &srcStrings[0], srcStrings.size(), mLastCompileOptions))
    {
        mInfoLog = sh::GetInfoLog(compilerHandle);
        WARN() << std::endl << mInfoLog;
        mState.mCompileStatus = CompileStatus::NOT_COMPILED;
        return;
    }

    mState.mTranslatedSource = sh::GetObjectCode(compilerHandle);

#if !defined(NDEBUG)
    // Prefix translated shader with commented out un-translated shader.
    // Useful in diagnostics tools which capture the shader source.
    std::ostringstream shaderStream;
    shaderStream << "// GLSL\n";
    shaderStream << "//\n";

    std::istringstream inputSourceStream(mState.mSource);
    std::string line;
    while (std::getline(inputSourceStream, line))
    {
        // Remove null characters from the source line
        line.erase(std::remove(line.begin(), line.end(), '\0'), line.end());

        shaderStream << "// " << line << std::endl;
    }
    shaderStream << "\n\n";
    shaderStream << mState.mTranslatedSource;
    mState.mTranslatedSource = shaderStream.str();
#endif  // !defined(NDEBUG)

    // Gather the shader information
    mState.mShaderVersion = sh::GetShaderVersion(compilerHandle);

    mState.mUniforms        = GetShaderVariables(sh::GetUniforms(compilerHandle));
    mState.mUniformBlocks       = GetShaderVariables(sh::GetUniformBlocks(compilerHandle));
    mState.mShaderStorageBlocks = GetShaderVariables(sh::GetShaderStorageBlocks(compilerHandle));

    switch (mState.mShaderType)
    {
        case GL_COMPUTE_SHADER:
        {
            mState.mLocalSize = sh::GetComputeShaderLocalGroupSize(compilerHandle);
            break;
        }
        case GL_VERTEX_SHADER:
        {
            {
                mState.mOutputVaryings = GetShaderVariables(sh::GetOutputVaryings(compilerHandle));
                mState.mActiveAttributes =
                    GetActiveShaderVariables(sh::GetAttributes(compilerHandle));
                mState.mNumViews = sh::GetVertexShaderNumViews(compilerHandle);
            }
            break;
        }
        case GL_FRAGMENT_SHADER:
        {
            mState.mInputVaryings = GetShaderVariables(sh::GetInputVaryings(compilerHandle));
            // TODO(jmadill): Figure out why we only sort in the FS, and if we need to.
            std::sort(mState.mInputVaryings.begin(), mState.mInputVaryings.end(), CompareShaderVar);
            mState.mActiveOutputVariables =
                GetActiveShaderVariables(sh::GetOutputVariables(compilerHandle));
            break;
        }
        case GL_GEOMETRY_SHADER_EXT:
        {
            mState.mInputVaryings  = GetShaderVariables(sh::GetInputVaryings(compilerHandle));
            mState.mOutputVaryings = GetShaderVariables(sh::GetOutputVaryings(compilerHandle));

            mState.mGeometryShaderInputPrimitiveType =
                sh::GetGeometryShaderInputPrimitiveType(compilerHandle);
            mState.mGeometryShaderOutputPrimitiveType =
                sh::GetGeometryShaderOutputPrimitiveType(compilerHandle);
            mState.mGeometryShaderInvocations = sh::GetGeometryShaderInvocations(compilerHandle);
            mState.mGeometryShaderMaxVertices = sh::GetGeometryShaderMaxVertices(compilerHandle);
            break;
        }
        default:
            UNREACHABLE();
    }

    ASSERT(!mState.mTranslatedSource.empty());

    bool success = mImplementation->postTranslateCompile(mBoundCompiler.get(), &mInfoLog);
    mState.mCompileStatus = success ? CompileStatus::COMPILED : CompileStatus::NOT_COMPILED;
}

void Shader::addRef()
{
    mRefCount++;
}

void Shader::release(const Context *context)
{
    mRefCount--;

    if (mRefCount == 0 && mDeleteStatus)
    {
        mResourceManager->deleteShader(context, mHandle);
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

bool Shader::isCompiled(const Context *context)
{
    resolveCompile(context);
    return mState.mCompileStatus == CompileStatus::COMPILED;
}

int Shader::getShaderVersion(const Context *context)
{
    resolveCompile(context);
    return mState.mShaderVersion;
}

const std::vector<sh::Varying> &Shader::getInputVaryings(const Context *context)
{
    resolveCompile(context);
    return mState.getInputVaryings();
}

const std::vector<sh::Varying> &Shader::getOutputVaryings(const Context *context)
{
    resolveCompile(context);
    return mState.getOutputVaryings();
}

const std::vector<sh::Uniform> &Shader::getUniforms(const Context *context)
{
    resolveCompile(context);
    return mState.getUniforms();
}

const std::vector<sh::InterfaceBlock> &Shader::getUniformBlocks(const Context *context)
{
    resolveCompile(context);
    return mState.getUniformBlocks();
}

const std::vector<sh::InterfaceBlock> &Shader::getShaderStorageBlocks(const Context *context)
{
    resolveCompile(context);
    return mState.getShaderStorageBlocks();
}

const std::vector<sh::Attribute> &Shader::getActiveAttributes(const Context *context)
{
    resolveCompile(context);
    return mState.getActiveAttributes();
}

const std::vector<sh::OutputVariable> &Shader::getActiveOutputVariables(const Context *context)
{
    resolveCompile(context);
    return mState.getActiveOutputVariables();
}

std::string Shader::getTransformFeedbackVaryingMappedName(const std::string &tfVaryingName,
                                                          const Context *context)
{
    // TODO(jiawei.shao@intel.com): support transform feedback on geometry shader.
    ASSERT(mState.getShaderType() == GL_VERTEX_SHADER);
    const auto &varyings = getOutputVaryings(context);
    auto bracketPos      = tfVaryingName.find("[");
    if (bracketPos != std::string::npos)
    {
        auto tfVaryingBaseName = tfVaryingName.substr(0, bracketPos);
        for (const auto &varying : varyings)
        {
            if (varying.name == tfVaryingBaseName)
            {
                std::string mappedNameWithArrayIndex =
                    varying.mappedName + tfVaryingName.substr(bracketPos);
                return mappedNameWithArrayIndex;
            }
        }
    }
    else
    {
        for (const auto &varying : varyings)
        {
            if (varying.name == tfVaryingName)
            {
                return varying.mappedName;
            }
        }
    }
    UNREACHABLE();
    return std::string();
}

const sh::WorkGroupSize &Shader::getWorkGroupSize(const Context *context)
{
    resolveCompile(context);
    return mState.mLocalSize;
}

int Shader::getNumViews(const Context *context)
{
    resolveCompile(context);
    return mState.mNumViews;
}

const std::string &Shader::getCompilerResourcesString() const
{
    ASSERT(mBoundCompiler.get());
    return mBoundCompiler->getBuiltinResourcesString(mState.mShaderType);
}

}  // namespace gl
