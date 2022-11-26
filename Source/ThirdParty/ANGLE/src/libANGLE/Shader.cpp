//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Shader.cpp: Implements the gl::Shader class and its  derived classes
// VertexShader and FragmentShader. Implements GL shader objects and related
// functionality. [OpenGL ES 2.0.24] section 2.10 page 24 and section 3.8 page 84.

#include "libANGLE/Shader.h"

#include <functional>
#include <sstream>

#include "GLSLANG/ShaderLang.h"
#include "common/utilities.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/MemoryShaderCache.h"
#include "libANGLE/Program.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/ShaderImpl.h"
#include "platform/FrontendFeatures_autogen.h"

namespace gl
{

namespace
{
constexpr uint32_t kShaderCacheIdentifier = 0x12345678;

template <typename VarT>
std::vector<VarT> GetActiveShaderVariables(const std::vector<VarT> *variableList)
{
    ASSERT(variableList);
    std::vector<VarT> result;
    for (size_t varIndex = 0; varIndex < variableList->size(); varIndex++)
    {
        const VarT &var = variableList->at(varIndex);
        if (var.active)
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

void WriteInterfaceBlock(gl::BinaryOutputStream *stream, const sh::InterfaceBlock &block)
{
    stream->writeString(block.name);
    stream->writeString(block.mappedName);
    stream->writeString(block.instanceName);
    stream->writeInt(block.arraySize);
    stream->writeEnum(block.layout);
    stream->writeBool(block.isRowMajorLayout);
    stream->writeInt(block.binding);
    stream->writeBool(block.staticUse);
    stream->writeBool(block.active);
    stream->writeEnum(block.blockType);

    stream->writeInt(block.fields.size());
    for (const sh::ShaderVariable &shaderVariable : block.fields)
    {
        WriteShaderVar(stream, shaderVariable);
    }
}

void LoadInterfaceBlock(gl::BinaryInputStream *stream, sh::InterfaceBlock &block)
{
    stream->readString(&block.name);
    stream->readString(&block.mappedName);
    stream->readString(&block.instanceName);
    stream->readInt(&block.arraySize);
    stream->readEnum(&block.layout);
    stream->readBool(&block.isRowMajorLayout);
    stream->readInt(&block.binding);
    stream->readBool(&block.staticUse);
    stream->readBool(&block.active);
    stream->readEnum(&block.blockType);

    size_t size = stream->readInt<size_t>();
    block.fields.resize(size);
    for (sh::ShaderVariable &shaderVariable : block.fields)
    {
        LoadShaderVar(stream, &shaderVariable);
    }
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

const char *GetShaderTypeString(ShaderType type)
{
    switch (type)
    {
        case ShaderType::Vertex:
            return "VERTEX";

        case ShaderType::Fragment:
            return "FRAGMENT";

        case ShaderType::Compute:
            return "COMPUTE";

        case ShaderType::Geometry:
            return "GEOMETRY";

        case ShaderType::TessControl:
            return "TESS_CONTROL";

        case ShaderType::TessEvaluation:
            return "TESS_EVALUATION";

        default:
            UNREACHABLE();
            return "";
    }
}

class [[nodiscard]] ScopedExit final : angle::NonCopyable
{
  public:
    ScopedExit(std::function<void()> exit) : mExit(exit) {}
    ~ScopedExit() { mExit(); }

  private:
    std::function<void()> mExit;
};

struct Shader::CompilingState
{
    std::shared_ptr<rx::WaitableCompileEvent> compileEvent;
    ShCompilerInstance shCompilerInstance;
    egl::BlobCache::Key shaderHash;
};

ShaderState::ShaderState(ShaderType shaderType)
    : mLabel(),
      mShaderType(shaderType),
      mShaderVersion(100),
      mNumViews(-1),
      mGeometryShaderInvocations(1),
      mCompileStatus(CompileStatus::NOT_COMPILED)
{
    mLocalSize.fill(-1);
}

ShaderState::~ShaderState() {}

Shader::Shader(ShaderProgramManager *manager,
               rx::GLImplFactory *implFactory,
               const gl::Limitations &rendererLimitations,
               ShaderType type,
               ShaderProgramID handle)
    : mState(type),
      mImplementation(implFactory->createShader(mState)),
      mRendererLimitations(rendererLimitations),
      mHandle(handle),
      mType(type),
      mRefCount(0),
      mDeleteStatus(false),
      mResourceManager(manager),
      mCurrentMaxComputeWorkGroupInvocations(0u)
{
    ASSERT(mImplementation);
}

void Shader::onDestroy(const gl::Context *context)
{
    resolveCompile(context);
    mImplementation->destroy();
    mBoundCompiler.set(context, nullptr);
    mImplementation.reset(nullptr);
    delete this;
}

Shader::~Shader()
{
    ASSERT(!mImplementation);
}

angle::Result Shader::setLabel(const Context *context, const std::string &label)
{
    mState.mLabel = label;

    if (mImplementation)
    {
        return mImplementation->onLabelUpdate(context);
    }
    return angle::Result::Continue;
}

const std::string &Shader::getLabel() const
{
    return mState.mLabel;
}

ShaderProgramID Shader::getHandle() const
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

const sh::BinaryBlob &Shader::getCompiledBinary(const Context *context)
{
    resolveCompile(context);
    return mState.mCompiledBinary;
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
    resolveCompile(context);

    mState.mTranslatedSource.clear();
    mState.mCompiledBinary.clear();
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
    mState.mGeometryShaderInputPrimitiveType.reset();
    mState.mGeometryShaderOutputPrimitiveType.reset();
    mState.mGeometryShaderMaxVertices.reset();
    mState.mGeometryShaderInvocations = 1;
    mState.mTessControlShaderVertices = 0;
    mState.mTessGenMode               = 0;
    mState.mTessGenSpacing            = 0;
    mState.mTessGenVertexOrder        = 0;
    mState.mTessGenPointMode          = 0;
    mState.mAdvancedBlendEquations.reset();
    mState.mHasDiscard              = false;
    mState.mEnablesPerSampleShading = false;
    mState.mSpecConstUsageBits.reset();

    mCurrentMaxComputeWorkGroupInvocations =
        static_cast<GLuint>(context->getCaps().maxComputeWorkGroupInvocations);
    mMaxComputeSharedMemory = context->getCaps().maxComputeSharedMemorySize;

    ShCompileOptions options = {};
    options.objectCode       = true;
    options.variables        = true;
    options.emulateGLDrawID  = true;

    // Add default options to WebGL shaders to prevent unexpected behavior during
    // compilation.
    if (context->isWebGL())
    {
        options.initGLPosition             = true;
        options.limitCallStackDepth        = true;
        options.limitExpressionComplexity  = true;
        options.enforcePackingRestrictions = true;
        options.initSharedVariables        = true;
    }
    else
    {
        // Per https://github.com/KhronosGroup/WebGL/pull/3278 gl_BaseVertex/gl_BaseInstance are
        // removed from WebGL
        options.emulateGLBaseVertexBaseInstance = true;
    }

    // Some targets (e.g. D3D11 Feature Level 9_3 and below) do not support non-constant loop
    // indexes in fragment shaders. Shader compilation will fail. To provide a better error
    // message we can instruct the compiler to pre-validate.
    if (mRendererLimitations.shadersRequireIndexedLoopValidation)
    {
        options.validateLoopIndexing = true;
    }

    // When clip and cull distances are used simultaneously, D3D11 can support up to four of each.
    if (context->isWebGL() || mRendererLimitations.limitSimultaneousClipAndCullDistanceUsage)
    {
        options.limitSimultaneousClipAndCullDistanceUsage = true;
    }

    if (context->getFrontendFeatures().scalarizeVecAndMatConstructorArgs.enabled)
    {
        options.scalarizeVecAndMatConstructorArgs = true;
    }

    if (context->getFrontendFeatures().forceInitShaderVariables.enabled)
    {
        options.initOutputVariables           = true;
        options.initializeUninitializedLocals = true;
    }

    mBoundCompiler.set(context, context->getCompiler());

    ASSERT(mBoundCompiler.get());
    ShCompilerInstance compilerInstance = mBoundCompiler->getInstance(mState.mShaderType);
    ShHandle compilerHandle             = compilerInstance.getHandle();
    ASSERT(compilerHandle);
    mCompilerResourcesString = compilerInstance.getBuiltinResourcesString();

    // Find a shader in Blob Cache
    egl::BlobCache::Key shaderHash = {0};
    MemoryShaderCache *shaderCache = context->getMemoryShaderCache();
    if (shaderCache)
    {
        angle::Result cacheResult =
            shaderCache->getShader(context, this, options, compilerInstance, &shaderHash);

        if (cacheResult == angle::Result::Continue)
        {
            compilerInstance.destroy();
            return;
        }
    }

    // Cache load failed, fall through normal compiling.
    mState.mCompileStatus = CompileStatus::COMPILE_REQUESTED;
    mCompilingState.reset(new CompilingState());
    mCompilingState->shCompilerInstance = std::move(compilerInstance);
    mCompilingState->shaderHash         = shaderHash;
    mCompilingState->compileEvent =
        mImplementation->compile(context, &(mCompilingState->shCompilerInstance), &options);
}

void Shader::resolveCompile(const Context *context)
{
    if (!mState.compilePending())
    {
        return;
    }

    ASSERT(mCompilingState.get());

    mCompilingState->compileEvent->wait();

    mInfoLog += mCompilingState->compileEvent->getInfoLog();

    ScopedExit exit([this]() {
        mBoundCompiler->putInstance(std::move(mCompilingState->shCompilerInstance));
        mCompilingState->compileEvent.reset();
        mCompilingState.reset();
    });

    ShHandle compilerHandle = mCompilingState->shCompilerInstance.getHandle();
    if (!mCompilingState->compileEvent->getResult())
    {
        mInfoLog += sh::GetInfoLog(compilerHandle);
        INFO() << std::endl << mInfoLog;
        mState.mCompileStatus = CompileStatus::NOT_COMPILED;
        return;
    }

    const ShShaderOutput outputType = mCompilingState->shCompilerInstance.getShaderOutputType();
    const bool isBinaryOutput       = outputType == SH_SPIRV_VULKAN_OUTPUT;

    if (isBinaryOutput)
    {
        mState.mCompiledBinary = sh::GetObjectBinaryBlob(compilerHandle);
    }
    else
    {
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

            shaderStream << "// " << line;

            // glslang complains if a comment ends with backslash
            if (!line.empty() && line.back() == '\\')
            {
                shaderStream << "\\";
            }

            shaderStream << std::endl;
        }
        shaderStream << "\n\n";
        shaderStream << mState.mTranslatedSource;
        mState.mTranslatedSource = shaderStream.str();
#endif  // !defined(NDEBUG)
    }

    // Gather the shader information
    mState.mShaderVersion = sh::GetShaderVersion(compilerHandle);

    mState.mUniforms            = GetShaderVariables(sh::GetUniforms(compilerHandle));
    mState.mUniformBlocks       = GetShaderVariables(sh::GetUniformBlocks(compilerHandle));
    mState.mShaderStorageBlocks = GetShaderVariables(sh::GetShaderStorageBlocks(compilerHandle));
    mState.mSpecConstUsageBits =
        rx::SpecConstUsageBits(sh::GetShaderSpecConstUsageBits(compilerHandle));

    switch (mState.mShaderType)
    {
        case ShaderType::Compute:
        {
            mState.mAllAttributes    = GetShaderVariables(sh::GetAttributes(compilerHandle));
            mState.mActiveAttributes = GetActiveShaderVariables(&mState.mAllAttributes);
            mState.mLocalSize        = sh::GetComputeShaderLocalGroupSize(compilerHandle);
            if (mState.mLocalSize.isDeclared())
            {
                angle::CheckedNumeric<uint32_t> checked_local_size_product(mState.mLocalSize[0]);
                checked_local_size_product *= mState.mLocalSize[1];
                checked_local_size_product *= mState.mLocalSize[2];

                if (!checked_local_size_product.IsValid())
                {
                    WARN() << std::endl
                           << "Integer overflow when computing the product of local_size_x, "
                           << "local_size_y and local_size_z.";
                    mState.mCompileStatus = CompileStatus::NOT_COMPILED;
                    return;
                }
                if (checked_local_size_product.ValueOrDie() >
                    mCurrentMaxComputeWorkGroupInvocations)
                {
                    WARN() << std::endl
                           << "The total number of invocations within a work group exceeds "
                           << "MAX_COMPUTE_WORK_GROUP_INVOCATIONS.";
                    mState.mCompileStatus = CompileStatus::NOT_COMPILED;
                    return;
                }
            }

            unsigned int sharedMemSize = sh::GetShaderSharedMemorySize(compilerHandle);
            if (sharedMemSize > mMaxComputeSharedMemory)
            {
                WARN() << std::endl << "Exceeded maximum shared memory size";
                mState.mCompileStatus = CompileStatus::NOT_COMPILED;
                return;
            }
            break;
        }
        case ShaderType::Vertex:
        {
            mState.mOutputVaryings   = GetShaderVariables(sh::GetOutputVaryings(compilerHandle));
            mState.mAllAttributes    = GetShaderVariables(sh::GetAttributes(compilerHandle));
            mState.mActiveAttributes = GetActiveShaderVariables(&mState.mAllAttributes);
            mState.mNumViews         = sh::GetVertexShaderNumViews(compilerHandle);
            break;
        }
        case ShaderType::Fragment:
        {
            mState.mAllAttributes    = GetShaderVariables(sh::GetAttributes(compilerHandle));
            mState.mActiveAttributes = GetActiveShaderVariables(&mState.mAllAttributes);
            mState.mInputVaryings    = GetShaderVariables(sh::GetInputVaryings(compilerHandle));
            // TODO(jmadill): Figure out why we only sort in the FS, and if we need to.
            std::sort(mState.mInputVaryings.begin(), mState.mInputVaryings.end(), CompareShaderVar);
            mState.mActiveOutputVariables =
                GetActiveShaderVariables(sh::GetOutputVariables(compilerHandle));
            mState.mHasDiscard              = sh::HasDiscardInFragmentShader(compilerHandle);
            mState.mEnablesPerSampleShading = sh::EnablesPerSampleShading(compilerHandle);
            mState.mAdvancedBlendEquations =
                BlendEquationBitSet(sh::GetAdvancedBlendEquations(compilerHandle));
            break;
        }
        case ShaderType::Geometry:
        {
            mState.mInputVaryings  = GetShaderVariables(sh::GetInputVaryings(compilerHandle));
            mState.mOutputVaryings = GetShaderVariables(sh::GetOutputVaryings(compilerHandle));

            if (sh::HasValidGeometryShaderInputPrimitiveType(compilerHandle))
            {
                mState.mGeometryShaderInputPrimitiveType = FromGLenum<PrimitiveMode>(
                    sh::GetGeometryShaderInputPrimitiveType(compilerHandle));
            }
            if (sh::HasValidGeometryShaderOutputPrimitiveType(compilerHandle))
            {
                mState.mGeometryShaderOutputPrimitiveType = FromGLenum<PrimitiveMode>(
                    sh::GetGeometryShaderOutputPrimitiveType(compilerHandle));
            }
            if (sh::HasValidGeometryShaderMaxVertices(compilerHandle))
            {
                mState.mGeometryShaderMaxVertices =
                    sh::GetGeometryShaderMaxVertices(compilerHandle);
            }
            mState.mGeometryShaderInvocations = sh::GetGeometryShaderInvocations(compilerHandle);
            break;
        }
        case ShaderType::TessControl:
        {
            mState.mInputVaryings  = GetShaderVariables(sh::GetInputVaryings(compilerHandle));
            mState.mOutputVaryings = GetShaderVariables(sh::GetOutputVaryings(compilerHandle));
            mState.mTessControlShaderVertices = sh::GetTessControlShaderVertices(compilerHandle);
            break;
        }
        case ShaderType::TessEvaluation:
        {
            mState.mInputVaryings  = GetShaderVariables(sh::GetInputVaryings(compilerHandle));
            mState.mOutputVaryings = GetShaderVariables(sh::GetOutputVaryings(compilerHandle));
            if (sh::HasValidTessGenMode(compilerHandle))
            {
                mState.mTessGenMode = sh::GetTessGenMode(compilerHandle);
            }
            if (sh::HasValidTessGenSpacing(compilerHandle))
            {
                mState.mTessGenSpacing = sh::GetTessGenSpacing(compilerHandle);
            }
            if (sh::HasValidTessGenVertexOrder(compilerHandle))
            {
                mState.mTessGenVertexOrder = sh::GetTessGenVertexOrder(compilerHandle);
            }
            if (sh::HasValidTessGenPointMode(compilerHandle))
            {
                mState.mTessGenPointMode = sh::GetTessGenPointMode(compilerHandle);
            }
            break;
        }

        default:
            UNREACHABLE();
    }

    ASSERT(!mState.mTranslatedSource.empty() || !mState.mCompiledBinary.empty());

    bool success          = mCompilingState->compileEvent->postTranslate(&mInfoLog);
    mState.mCompileStatus = success ? CompileStatus::COMPILED : CompileStatus::NOT_COMPILED;

    MemoryShaderCache *shaderCache = context->getMemoryShaderCache();
    if (success && shaderCache)
    {
        // Save to the shader cache.
        if (shaderCache->putShader(context, mCompilingState->shaderHash, this) !=
            angle::Result::Continue)
        {
            ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                               "Failed to save compiled shader to memory shader cache.");
        }
    }
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

bool Shader::isCompleted()
{
    return (!mState.compilePending() || mCompilingState->compileEvent->isReady());
}

int Shader::getShaderVersion(const Context *context)
{
    resolveCompile(context);
    return mState.mShaderVersion;
}

const std::vector<sh::ShaderVariable> &Shader::getInputVaryings(const Context *context)
{
    resolveCompile(context);
    return mState.getInputVaryings();
}

const std::vector<sh::ShaderVariable> &Shader::getOutputVaryings(const Context *context)
{
    resolveCompile(context);
    return mState.getOutputVaryings();
}

const std::vector<sh::ShaderVariable> &Shader::getUniforms(const Context *context)
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

const std::vector<sh::ShaderVariable> &Shader::getActiveAttributes(const Context *context)
{
    resolveCompile(context);
    return mState.getActiveAttributes();
}

const std::vector<sh::ShaderVariable> &Shader::getAllAttributes(const Context *context)
{
    resolveCompile(context);
    return mState.getAllAttributes();
}

const std::vector<sh::ShaderVariable> &Shader::getActiveOutputVariables(const Context *context)
{
    resolveCompile(context);
    return mState.getActiveOutputVariables();
}

std::string Shader::getTransformFeedbackVaryingMappedName(const Context *context,
                                                          const std::string &tfVaryingName)
{
    ASSERT(mState.getShaderType() != ShaderType::Fragment &&
           mState.getShaderType() != ShaderType::Compute);
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
            else if (varying.isStruct())
            {
                GLuint fieldIndex = 0;
                const auto *field = varying.findField(tfVaryingName, &fieldIndex);
                if (field == nullptr)
                {
                    continue;
                }
                ASSERT(field != nullptr && !field->isStruct() &&
                       (!field->isArray() || varying.isShaderIOBlock));
                std::string mappedName;
                // If it's an I/O block without an instance name, don't include the block name.
                if (!varying.isShaderIOBlock || !varying.name.empty())
                {
                    mappedName = varying.isShaderIOBlock ? varying.mappedStructOrBlockName
                                                         : varying.mappedName;
                    mappedName += '.';
                }
                return mappedName + field->mappedName;
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

Optional<PrimitiveMode> Shader::getGeometryShaderInputPrimitiveType(const Context *context)
{
    resolveCompile(context);
    return mState.mGeometryShaderInputPrimitiveType;
}

Optional<PrimitiveMode> Shader::getGeometryShaderOutputPrimitiveType(const Context *context)
{
    resolveCompile(context);
    return mState.mGeometryShaderOutputPrimitiveType;
}

int Shader::getGeometryShaderInvocations(const Context *context)
{
    resolveCompile(context);
    return mState.mGeometryShaderInvocations;
}

Optional<GLint> Shader::getGeometryShaderMaxVertices(const Context *context)
{
    resolveCompile(context);
    return mState.mGeometryShaderMaxVertices;
}

int Shader::getTessControlShaderVertices(const Context *context)
{
    resolveCompile(context);
    return mState.mTessControlShaderVertices;
}

GLenum Shader::getTessGenMode(const Context *context)
{
    resolveCompile(context);
    return mState.mTessGenMode;
}

GLenum Shader::getTessGenSpacing(const Context *context)
{
    resolveCompile(context);
    return mState.mTessGenSpacing;
}

GLenum Shader::getTessGenVertexOrder(const Context *context)
{
    resolveCompile(context);
    return mState.mTessGenVertexOrder;
}

GLenum Shader::getTessGenPointMode(const Context *context)
{
    resolveCompile(context);
    return mState.mTessGenPointMode;
}

const std::string &Shader::getCompilerResourcesString() const
{
    return mCompilerResourcesString;
}

angle::Result Shader::serialize(const Context *context, angle::MemoryBuffer *binaryOut) const
{
    BinaryOutputStream stream;

    stream.writeInt(kShaderCacheIdentifier);
    stream.writeString(mState.mLabel);
    stream.writeInt(mState.mShaderVersion);
    stream.writeString(mCompilerResourcesString);

    stream.writeInt(mState.mUniforms.size());
    for (const sh::ShaderVariable &shaderVariable : mState.mUniforms)
    {
        WriteShaderVar(&stream, shaderVariable);
    }

    stream.writeInt(mState.mUniformBlocks.size());
    for (const sh::InterfaceBlock &interfaceBlock : mState.mUniformBlocks)
    {
        WriteInterfaceBlock(&stream, interfaceBlock);
    }

    stream.writeInt(mState.mShaderStorageBlocks.size());
    for (const sh::InterfaceBlock &interfaceBlock : mState.mShaderStorageBlocks)
    {
        WriteInterfaceBlock(&stream, interfaceBlock);
    }

    stream.writeInt(mState.mSpecConstUsageBits.bits());

    switch (mType)
    {
        case ShaderType::Compute:
        {
            stream.writeInt(mState.mAllAttributes.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mAllAttributes)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeInt(mState.mActiveAttributes.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mActiveAttributes)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeInt(mState.mLocalSize[0]);
            stream.writeInt(mState.mLocalSize[1]);
            stream.writeInt(mState.mLocalSize[2]);
            break;
        }

        case ShaderType::Vertex:
        {
            stream.writeInt(mState.mOutputVaryings.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mOutputVaryings)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeInt(mState.mAllAttributes.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mAllAttributes)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeInt(mState.mActiveAttributes.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mActiveAttributes)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeInt(mState.mNumViews);
            break;
        }
        case ShaderType::Fragment:
        {
            stream.writeInt(mState.mInputVaryings.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mInputVaryings)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeInt(mState.mActiveOutputVariables.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mActiveOutputVariables)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeBool(mState.mEnablesPerSampleShading);
            stream.writeInt(mState.mAdvancedBlendEquations.bits());
            break;
        }
        case ShaderType::Geometry:
        {
            bool valid;

            stream.writeInt(mState.mInputVaryings.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mInputVaryings)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeInt(mState.mOutputVaryings.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mOutputVaryings)
            {
                WriteShaderVar(&stream, shaderVariable);
            }

            valid = (bool)mState.mGeometryShaderInputPrimitiveType.valid();
            stream.writeBool(valid);
            if (valid)
            {
                unsigned char value =
                    (unsigned char)mState.mGeometryShaderInputPrimitiveType.value();
                stream.writeBytes(&value, 1);
            }
            valid = (bool)mState.mGeometryShaderOutputPrimitiveType.valid();
            stream.writeBool(valid);
            if (valid)
            {
                unsigned char value =
                    (unsigned char)mState.mGeometryShaderOutputPrimitiveType.value();
                stream.writeBytes(&value, 1);
            }
            valid = mState.mGeometryShaderMaxVertices.valid();
            stream.writeBool(valid);
            if (valid)
            {
                int value = (int)mState.mGeometryShaderMaxVertices.value();
                stream.writeInt(value);
            }

            stream.writeInt(mState.mGeometryShaderInvocations);
            break;
        }
        case ShaderType::TessControl:
        {
            stream.writeInt(mState.mInputVaryings.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mInputVaryings)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeInt(mState.mOutputVaryings.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mOutputVaryings)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeInt(mState.mTessControlShaderVertices);
            break;
        }
        case ShaderType::TessEvaluation:
        {
            unsigned int value;

            stream.writeInt(mState.mInputVaryings.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mInputVaryings)
            {
                WriteShaderVar(&stream, shaderVariable);
            }
            stream.writeInt(mState.mOutputVaryings.size());
            for (const sh::ShaderVariable &shaderVariable : mState.mOutputVaryings)
            {
                WriteShaderVar(&stream, shaderVariable);
            }

            value = (unsigned int)(mState.mTessGenMode);
            stream.writeInt(value);

            value = (unsigned int)mState.mTessGenSpacing;
            stream.writeInt(value);

            value = (unsigned int)mState.mTessGenVertexOrder;
            stream.writeInt(value);

            value = (unsigned int)mState.mTessGenPointMode;
            stream.writeInt(value);
            break;
        }
        default:
            UNREACHABLE();
    }

    stream.writeIntVector(mState.mCompiledBinary);
    stream.writeEnum(mState.mCompileStatus);

    ASSERT(binaryOut);
    if (!binaryOut->resize(stream.length()))
    {
        std::stringstream sstream;
        sstream << "Failed to allocate enough memory to serialize a shader. (" << stream.length()
                << " bytes )";
        ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                           sstream.str().c_str());
        return angle::Result::Incomplete;
    }

    memcpy(binaryOut->data(), stream.data(), stream.length());

    return angle::Result::Continue;
}

angle::Result Shader::deserialize(const Context *context, BinaryInputStream &stream)
{
    size_t size;

    if (stream.readInt<uint32_t>() != kShaderCacheIdentifier)
    {
        return angle::Result::Stop;
    }

    stream.readString(&mState.mLabel);
    stream.readInt(&mState.mShaderVersion);
    stream.readString(&mCompilerResourcesString);

    size = stream.readInt<size_t>();
    mState.mUniforms.resize(size);
    for (sh::ShaderVariable &shaderVariable : mState.mUniforms)
    {
        LoadShaderVar(&stream, &shaderVariable);
    }

    size = stream.readInt<size_t>();
    mState.mUniformBlocks.resize(size);
    for (sh::InterfaceBlock &interfaceBlock : mState.mUniformBlocks)
    {
        LoadInterfaceBlock(&stream, interfaceBlock);
    }

    size = stream.readInt<size_t>();
    mState.mShaderStorageBlocks.resize(size);
    for (sh::InterfaceBlock &interfaceBlock : mState.mShaderStorageBlocks)
    {
        LoadInterfaceBlock(&stream, interfaceBlock);
    }

    mState.mSpecConstUsageBits = rx::SpecConstUsageBits(stream.readInt<uint32_t>());

    switch (mType)
    {
        case ShaderType::Compute:
        {
            size = stream.readInt<size_t>();
            mState.mAllAttributes.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mAllAttributes)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            size = stream.readInt<size_t>();
            mState.mActiveAttributes.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mActiveAttributes)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            stream.readInt(&mState.mLocalSize[0]);
            stream.readInt(&mState.mLocalSize[1]);
            stream.readInt(&mState.mLocalSize[2]);
            break;
        }
        case ShaderType::Vertex:
        {
            size = stream.readInt<size_t>();
            mState.mOutputVaryings.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mOutputVaryings)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            size = stream.readInt<size_t>();
            mState.mAllAttributes.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mAllAttributes)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            size = stream.readInt<size_t>();
            mState.mActiveAttributes.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mActiveAttributes)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            stream.readInt(&mState.mNumViews);
            break;
        }
        case ShaderType::Fragment:
        {
            size = stream.readInt<size_t>();
            mState.mInputVaryings.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mInputVaryings)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            size = stream.readInt<size_t>();
            mState.mActiveOutputVariables.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mActiveOutputVariables)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            stream.readBool(&mState.mEnablesPerSampleShading);
            int advancedBlendEquationBits;
            stream.readInt(&advancedBlendEquationBits);
            mState.mAdvancedBlendEquations = BlendEquationBitSet(advancedBlendEquationBits);
            break;
        }
        case ShaderType::Geometry:
        {
            bool valid;

            size = stream.readInt<size_t>();
            mState.mInputVaryings.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mInputVaryings)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            size = stream.readInt<size_t>();
            mState.mOutputVaryings.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mOutputVaryings)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }

            stream.readBool(&valid);
            if (valid)
            {
                unsigned char value;
                stream.readBytes(&value, 1);
                mState.mGeometryShaderInputPrimitiveType = static_cast<PrimitiveMode>(value);
            }
            else
            {
                mState.mGeometryShaderInputPrimitiveType.reset();
            }

            stream.readBool(&valid);
            if (valid)
            {
                unsigned char value;
                stream.readBytes(&value, 1);
                mState.mGeometryShaderOutputPrimitiveType = static_cast<PrimitiveMode>(value);
            }
            else
            {
                mState.mGeometryShaderOutputPrimitiveType.reset();
            }

            stream.readBool(&valid);
            if (valid)
            {
                int value;
                stream.readInt(&value);
                mState.mGeometryShaderMaxVertices = static_cast<GLint>(value);
            }
            else
            {
                mState.mGeometryShaderMaxVertices.reset();
            }

            stream.readInt(&mState.mGeometryShaderInvocations);
            break;
        }
        case ShaderType::TessControl:
        {
            size = stream.readInt<size_t>();
            mState.mInputVaryings.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mInputVaryings)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            size = stream.readInt<size_t>();
            mState.mOutputVaryings.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mOutputVaryings)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            stream.readInt(&mState.mTessControlShaderVertices);
            break;
        }
        case ShaderType::TessEvaluation:
        {
            unsigned int value;

            size = stream.readInt<size_t>();
            mState.mInputVaryings.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mInputVaryings)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }
            size = stream.readInt<size_t>();
            mState.mOutputVaryings.resize(size);
            for (sh::ShaderVariable &shaderVariable : mState.mOutputVaryings)
            {
                LoadShaderVar(&stream, &shaderVariable);
            }

            stream.readInt(&value);
            mState.mTessGenMode = (GLenum)value;

            stream.readInt(&value);
            mState.mTessGenSpacing = (GLenum)value;

            stream.readInt(&value);
            mState.mTessGenVertexOrder = (GLenum)value;

            stream.readInt(&value);
            mState.mTessGenPointMode = (GLenum)value;
            break;
        }
        default:
            UNREACHABLE();
    }

    stream.readIntVector<unsigned int>(&mState.mCompiledBinary);
    mState.mCompileStatus = stream.readEnum<CompileStatus>();

    return angle::Result::Continue;
}

angle::Result Shader::loadBinary(const Context *context, const void *binary, GLsizei length)
{
    BinaryInputStream stream(binary, length);
    ANGLE_TRY(deserialize(context, stream));

    return angle::Result::Continue;
}

}  // namespace gl
