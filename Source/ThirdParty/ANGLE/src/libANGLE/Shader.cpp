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
#include "common/angle_version_info.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
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
#include "platform/autogen/FrontendFeatures_autogen.h"

namespace gl
{

namespace
{
constexpr uint32_t kShaderCacheIdentifier = 0x12345678;

// Environment variable (and associated Android property) for the path to read and write shader
// dumps
constexpr char kShaderDumpPathVarName[]       = "ANGLE_SHADER_DUMP_PATH";
constexpr char kEShaderDumpPathPropertyName[] = "debug.angle.shader_dump_path";

size_t ComputeShaderHash(const std::string &mergedSource)
{
    return std::hash<std::string>{}(mergedSource);
}

std::string GetShaderDumpFilePath(size_t shaderHash, const char *suffix)
{
    std::stringstream path;
    std::string shaderDumpDir = GetShaderDumpFileDirectory();
    if (!shaderDumpDir.empty())
    {
        path << shaderDumpDir << "/";
    }
    path << shaderHash << "." << suffix;

    return path.str();
}
}  // anonymous namespace

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

std::string GetShaderDumpFileDirectory()
{
    // Check the environment variable for the path to save and read shader dump files.
    std::string environmentVariableDumpDir =
        angle::GetAndSetEnvironmentVarOrUnCachedAndroidProperty(kShaderDumpPathVarName,
                                                                kEShaderDumpPathPropertyName);
    if (!environmentVariableDumpDir.empty() && environmentVariableDumpDir.compare("0") != 0)
    {
        return environmentVariableDumpDir;
    }

    // Fall back to the temp dir. If that doesn't exist, use the current working directory.
    return angle::GetTempDirectory().valueOr("");
}

std::string GetShaderDumpFileName(size_t shaderHash)
{
    std::stringstream name;
    name << shaderHash << ".essl";
    return name.str();
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
};

ShaderState::ShaderState(ShaderType shaderType)
    : mCompiledState(std::make_shared<CompiledShaderState>(shaderType))
{}

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
      mRefCount(0),
      mDeleteStatus(false),
      mResourceManager(manager),
      mCurrentMaxComputeWorkGroupInvocations(0u),
      mMaxComputeSharedMemory(0u)
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

std::string Shader::joinShaderSources(GLsizei count, const char *const *string, const GLint *length)
{
    // Fast path for the most common case.
    if (count == 1)
    {
        if (length == nullptr || length[0] < 0)
            return std::string(string[0]);
        else
            return std::string(string[0], static_cast<size_t>(length[0]));
    }

    // Start with totalLength of 1 to reserve space for the null terminator
    size_t totalLength = 1;

    // First pass, calculate the total length of the joined string
    for (GLsizei i = 0; i < count; ++i)
    {
        if (length == nullptr || length[i] < 0)
            totalLength += std::strlen(string[i]);
        else
            totalLength += static_cast<size_t>(length[i]);
    }

    // Second pass, allocate the string and concatenate each shader source
    // fragment
    std::string joinedString;
    joinedString.reserve(totalLength);
    for (GLsizei i = 0; i < count; ++i)
    {
        if (length == nullptr || length[i] < 0)
            joinedString.append(string[i]);
        else
            joinedString.append(string[i], static_cast<size_t>(length[i]));
    }

    return joinedString;
}

void Shader::setSource(const Context *context,
                       GLsizei count,
                       const char *const *string,
                       const GLint *length)
{
    std::string source = joinShaderSources(count, string, length);

    // Compute the hash based on the original source before any substitutions
    size_t sourceHash = ComputeShaderHash(source);

    const angle::FrontendFeatures &frontendFeatures = context->getFrontendFeatures();

    bool substitutedShader = false;
    const char *suffix     = "essl";
    if (frontendFeatures.enableShaderSubstitution.enabled)
    {
        std::string subsitutionShaderPath = GetShaderDumpFilePath(sourceHash, suffix);

        std::string substituteShader;
        if (angle::ReadFileToString(subsitutionShaderPath, &substituteShader))
        {
            source            = std::move(substituteShader);
            substitutedShader = true;
            INFO() << "Shader substitute found, loading from " << subsitutionShaderPath;
        }
    }

    // Only dump shaders that have not been previously substituted. It would write the same data
    // back to the file.
    if (frontendFeatures.dumpShaderSource.enabled && !substitutedShader)
    {
        std::string dumpFile = GetShaderDumpFilePath(sourceHash, suffix);

        writeFile(dumpFile.c_str(), source.c_str(), source.length());
        INFO() << "Dumped shader source: " << dumpFile;
    }

    mState.mSource     = std::move(source);
    mState.mSourceHash = sourceHash;
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

    if (mState.mCompiledState->translatedSource.empty())
    {
        return 0;
    }

    return static_cast<int>(mState.mCompiledState->translatedSource.length()) + 1;
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
    return mState.mCompiledState->translatedSource;
}

size_t Shader::getSourceHash() const
{
    return mState.mSourceHash;
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

    // Create a new compiled shader state.  If any programs are currently linking using this shader,
    // they would use the old compiled state, and this shader is free to recompile in the meantime.
    mState.mCompiledState = std::make_shared<CompiledShaderState>(mState.getShaderType());

    mInfoLog.clear();

    mCurrentMaxComputeWorkGroupInvocations =
        static_cast<GLuint>(context->getCaps().maxComputeWorkGroupInvocations);
    mMaxComputeSharedMemory = context->getCaps().maxComputeSharedMemorySize;

    ShCompileOptions options = {};
    options.objectCode       = true;
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

    if (context->getFrontendFeatures().forceInitShaderVariables.enabled)
    {
        options.initOutputVariables           = true;
        options.initializeUninitializedLocals = true;
    }

    mBoundCompiler.set(context, context->getCompiler());

    ASSERT(mBoundCompiler.get());
    ShCompilerInstance compilerInstance = mBoundCompiler->getInstance(mState.getShaderType());
    ShHandle compilerHandle             = compilerInstance.getHandle();
    ASSERT(compilerHandle);

    // Find a shader in Blob Cache
    setShaderKey(context, options, compilerInstance.getShaderOutputType(),
                 compilerInstance.getBuiltInResources());
    ASSERT(!mShaderHash.empty());
    MemoryShaderCache *shaderCache = context->getMemoryShaderCache();
    if (shaderCache)
    {
        angle::Result cacheResult =
            shaderCache->getShader(context, this, options, compilerInstance, mShaderHash);

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
    mState.mCompileStatus = CompileStatus::IS_RESOLVING;

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
    bool isBinaryOutput             = outputType == SH_SPIRV_VULKAN_OUTPUT;
    mState.mCompiledState->buildCompiledShaderState(compilerHandle, isBinaryOutput);

    const angle::FrontendFeatures &frontendFeatures = context->getFrontendFeatures();
    bool substitutedTranslatedShader                = false;
    const char *suffix                              = "translated";
    if (frontendFeatures.enableTranslatedShaderSubstitution.enabled)
    {
        // To support reading/writing compiled binaries (SPIR-V
        // representation), need more file input/output facilities,
        // and figure out the byte ordering of writing the 32-bit
        // words to disk.
        if (isBinaryOutput)
        {
            INFO() << "Can not substitute compiled binary (SPIR-V) shaders yet";
        }
        else
        {
            std::string substituteShaderPath = GetShaderDumpFilePath(mState.mSourceHash, suffix);

            std::string substituteShader;
            if (angle::ReadFileToString(substituteShaderPath, &substituteShader))
            {
                mState.mCompiledState->translatedSource = std::move(substituteShader);
                substitutedTranslatedShader             = true;
                INFO() << "Trasnslated shader substitute found, loading from "
                       << substituteShaderPath;
            }
        }
    }

    // Only dump translated shaders that have not been previously substituted. It would write the
    // same data back to the file.
    if (frontendFeatures.dumpTranslatedShaders.enabled && !substitutedTranslatedShader)
    {
        if (isBinaryOutput)
        {
            INFO() << "Can not dump compiled binary (SPIR-V) shaders yet";
        }
        else
        {
            std::string dumpFile = GetShaderDumpFilePath(mState.mSourceHash, suffix);

            const std::string &translatedSource = mState.mCompiledState->translatedSource;
            writeFile(dumpFile.c_str(), translatedSource.c_str(), translatedSource.length());
            INFO() << "Dumped translated source: " << dumpFile;
        }
    }

#if !defined(NDEBUG)
    if (outputType != SH_SPIRV_VULKAN_OUTPUT)
    {
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
        shaderStream << mState.mCompiledState->translatedSource;
        mState.mCompiledState->translatedSource = shaderStream.str();
    }
#endif  // !defined(NDEBUG)

    // Validation checks for compute shaders
    if (mState.mCompiledState->shaderType == ShaderType::Compute &&
        mState.mCompiledState->localSize.isDeclared())
    {
        angle::CheckedNumeric<uint32_t> checked_local_size_product(
            mState.mCompiledState->localSize[0]);
        checked_local_size_product *= mState.mCompiledState->localSize[1];
        checked_local_size_product *= mState.mCompiledState->localSize[2];

        if (!checked_local_size_product.IsValid())
        {
            WARN() << std::endl
                   << "Integer overflow when computing the product of local_size_x, "
                   << "local_size_y and local_size_z.";
            mState.mCompileStatus = CompileStatus::NOT_COMPILED;
            return;
        }
        if (checked_local_size_product.ValueOrDie() > mCurrentMaxComputeWorkGroupInvocations)
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

    ASSERT(!mState.mCompiledState->translatedSource.empty() ||
           !mState.mCompiledState->compiledBinary.empty());

    bool success          = mCompilingState->compileEvent->postTranslate(&mInfoLog);
    mState.mCompileStatus = success ? CompileStatus::COMPILED : CompileStatus::NOT_COMPILED;

    mState.mCompiledState->successfullyCompiled = success;

    MemoryShaderCache *shaderCache = context->getMemoryShaderCache();
    if (success && shaderCache)
    {
        // Save to the shader cache.
        if (shaderCache->putShader(context, mShaderHash, this) != angle::Result::Continue)
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

angle::Result Shader::serialize(const Context *context, angle::MemoryBuffer *binaryOut) const
{
    BinaryOutputStream stream;

    stream.writeInt(kShaderCacheIdentifier);
    mState.mCompiledState->serialize(stream);

    ASSERT(binaryOut);
    if (!binaryOut->resize(stream.length()))
    {
        ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "Failed to allocate enough memory to serialize a shader. (%zu bytes)",
                           stream.length());
        return angle::Result::Incomplete;
    }

    memcpy(binaryOut->data(), stream.data(), stream.length());

    return angle::Result::Continue;
}

angle::Result Shader::deserialize(BinaryInputStream &stream)
{
    mState.mCompiledState->deserialize(stream);

    if (stream.error())
    {
        // Error while deserializing binary stream
        return angle::Result::Stop;
    }

    // Note: Currently, shader binaries are only supported on backends that don't happen to have any
    // additional state used at link time.  If other backends implement this functionality, this
    // function should call into the backend object to deserialize their part.

    return angle::Result::Continue;
}

angle::Result Shader::loadBinary(const Context *context, const void *binary, GLsizei length)
{
    return loadBinaryImpl(context, binary, length, false);
}

angle::Result Shader::loadShaderBinary(const Context *context, const void *binary, GLsizei length)
{
    return loadBinaryImpl(context, binary, length, true);
}

angle::Result Shader::loadBinaryImpl(const Context *context,
                                     const void *binary,
                                     GLsizei length,
                                     bool generatedWithOfflineCompiler)
{
    BinaryInputStream stream(binary, length);

    mState.mCompiledState = std::make_shared<CompiledShaderState>(mState.getShaderType());

    // Shader binaries generated with offline compiler have additional fields
    if (generatedWithOfflineCompiler)
    {
        // Load binary from a glShaderBinary call.
        // Validation layer should have already verified that the shader program version and shader
        // type match
        std::vector<uint8_t> commitString(angle::GetANGLEShaderProgramVersionHashSize(), 0);
        stream.readBytes(commitString.data(), commitString.size());
        ASSERT(memcmp(commitString.data(), angle::GetANGLEShaderProgramVersion(),
                      commitString.size()) == 0);

        gl::ShaderType shaderType;
        stream.readEnum(&shaderType);
        ASSERT(mState.getShaderType() == shaderType);

        // Get fields needed to generate the key for memory caches.
        ShShaderOutput outputType;
        stream.readEnum<ShShaderOutput>(&outputType);

        // Get the shader's source string.
        mState.mSource = stream.readString();

        // In the absence of element-by-element serialize/deserialize functions, read
        // ShCompileOptions and ShBuiltInResources as raw binary blobs.
        ShCompileOptions compileOptions;
        stream.readBytes(reinterpret_cast<uint8_t *>(&compileOptions), sizeof(ShCompileOptions));

        ShBuiltInResources resources;
        stream.readBytes(reinterpret_cast<uint8_t *>(&resources), sizeof(ShBuiltInResources));

        setShaderKey(context, compileOptions, outputType, resources);
    }
    else
    {
        // Load binary from shader cache.
        if (stream.readInt<uint32_t>() != kShaderCacheIdentifier)
        {
            return angle::Result::Stop;
        }
    }

    ANGLE_TRY(deserialize(stream));

    // Only successfully-compiled shaders are serialized. If deserialization is successful, we can
    // assume the CompileStatus.
    mState.mCompileStatus                       = CompileStatus::COMPILED;
    mState.mCompiledState->successfullyCompiled = true;

    return angle::Result::Continue;
}

void Shader::setShaderKey(const Context *context,
                          const ShCompileOptions &compileOptions,
                          const ShShaderOutput &outputType,
                          const ShBuiltInResources &resources)
{
    // Compute shader key.
    BinaryOutputStream hashStream;

    // Start with the shader type and source.
    hashStream.writeEnum(mState.getShaderType());
    hashStream.writeString(mState.getSource());

    // Include the shader program version hash.
    hashStream.writeString(angle::GetANGLEShaderProgramVersion());

    hashStream.writeEnum(Compiler::SelectShaderSpec(context->getState()));
    hashStream.writeEnum(outputType);
    hashStream.writeBytes(reinterpret_cast<const uint8_t *>(&compileOptions),
                          sizeof(compileOptions));

    // Include the ShBuiltInResources, which represent the extensions and constants used by the
    // shader.
    hashStream.writeBytes(reinterpret_cast<const uint8_t *>(&resources), sizeof(resources));

    // Call the secure SHA hashing function.
    const std::vector<uint8_t> &shaderKey = hashStream.getData();
    mShaderHash                           = {0};
    angle::base::SHA1HashBytes(shaderKey.data(), shaderKey.size(), mShaderHash.data());
}

}  // namespace gl
