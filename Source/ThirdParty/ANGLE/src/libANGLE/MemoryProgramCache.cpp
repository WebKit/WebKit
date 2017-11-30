//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MemoryProgramCache: Stores compiled and linked programs in memory so they don't
//   always have to be re-compiled. Can be used in conjunction with the platform
//   layer to warm up the cache from disk.

#include "libANGLE/MemoryProgramCache.h"

#include <GLSLANG/ShaderVars.h>
#include <anglebase/sha1.h>

#include "common/utilities.h"
#include "common/version.h"
#include "libANGLE/BinaryStream.h"
#include "libANGLE/Context.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "platform/Platform.h"

namespace gl
{

namespace
{
enum CacheResult
{
    kCacheMiss,
    kCacheHitMemory,
    kCacheHitDisk,
    kCacheResultMax,
};

constexpr unsigned int kWarningLimit = 3;

void WriteShaderVar(BinaryOutputStream *stream, const sh::ShaderVariable &var)
{
    stream->writeInt(var.type);
    stream->writeInt(var.precision);
    stream->writeString(var.name);
    stream->writeString(var.mappedName);
    stream->writeIntVector(var.arraySizes);
    stream->writeInt(var.staticUse);
    stream->writeString(var.structName);
    ASSERT(var.fields.empty());
}

void LoadShaderVar(BinaryInputStream *stream, sh::ShaderVariable *var)
{
    var->type       = stream->readInt<GLenum>();
    var->precision  = stream->readInt<GLenum>();
    var->name       = stream->readString();
    var->mappedName = stream->readString();
    stream->readIntVector<unsigned int>(&var->arraySizes);
    var->staticUse  = stream->readBool();
    var->structName = stream->readString();
}

void WriteShaderVariableBuffer(BinaryOutputStream *stream, const ShaderVariableBuffer &var)
{
    stream->writeInt(var.binding);
    stream->writeInt(var.dataSize);

    stream->writeInt(var.vertexStaticUse);
    stream->writeInt(var.fragmentStaticUse);
    stream->writeInt(var.computeStaticUse);

    stream->writeInt(var.memberIndexes.size());
    for (unsigned int memberCounterIndex : var.memberIndexes)
    {
        stream->writeInt(memberCounterIndex);
    }
}

void LoadShaderVariableBuffer(BinaryInputStream *stream, ShaderVariableBuffer *var)
{
    var->binding           = stream->readInt<int>();
    var->dataSize          = stream->readInt<unsigned int>();
    var->vertexStaticUse   = stream->readBool();
    var->fragmentStaticUse = stream->readBool();
    var->computeStaticUse  = stream->readBool();

    unsigned int numMembers = stream->readInt<unsigned int>();
    for (unsigned int blockMemberIndex = 0; blockMemberIndex < numMembers; blockMemberIndex++)
    {
        var->memberIndexes.push_back(stream->readInt<unsigned int>());
    }
}

void WriteBufferVariable(BinaryOutputStream *stream, const BufferVariable &var)
{
    WriteShaderVar(stream, var);

    stream->writeInt(var.bufferIndex);
    stream->writeInt(var.blockInfo.offset);
    stream->writeInt(var.blockInfo.arrayStride);
    stream->writeInt(var.blockInfo.matrixStride);
    stream->writeInt(var.blockInfo.isRowMajorMatrix);
    stream->writeInt(var.blockInfo.topLevelArrayStride);
    stream->writeInt(var.topLevelArraySize);
    stream->writeInt(var.vertexStaticUse);
    stream->writeInt(var.fragmentStaticUse);
    stream->writeInt(var.computeStaticUse);
}

void LoadBufferVariable(BinaryInputStream *stream, BufferVariable *var)
{
    LoadShaderVar(stream, var);

    var->bufferIndex                   = stream->readInt<int>();
    var->blockInfo.offset              = stream->readInt<int>();
    var->blockInfo.arrayStride         = stream->readInt<int>();
    var->blockInfo.matrixStride        = stream->readInt<int>();
    var->blockInfo.isRowMajorMatrix    = stream->readBool();
    var->blockInfo.topLevelArrayStride = stream->readInt<int>();
    var->topLevelArraySize             = stream->readInt<int>();
    var->vertexStaticUse               = stream->readBool();
    var->fragmentStaticUse             = stream->readBool();
    var->computeStaticUse              = stream->readBool();
}

void WriteInterfaceBlock(BinaryOutputStream *stream, const InterfaceBlock &block)
{
    stream->writeString(block.name);
    stream->writeString(block.mappedName);
    stream->writeInt(block.isArray);
    stream->writeInt(block.arrayElement);

    WriteShaderVariableBuffer(stream, block);
}

void LoadInterfaceBlock(BinaryInputStream *stream, InterfaceBlock *block)
{
    block->name         = stream->readString();
    block->mappedName   = stream->readString();
    block->isArray      = stream->readBool();
    block->arrayElement = stream->readInt<unsigned int>();

    LoadShaderVariableBuffer(stream, block);
}

class HashStream final : angle::NonCopyable
{
  public:
    std::string str() { return mStringStream.str(); }

    template <typename T>
    HashStream &operator<<(T value)
    {
        mStringStream << value << kSeparator;
        return *this;
    }

  private:
    static constexpr char kSeparator = ':';
    std::ostringstream mStringStream;
};

HashStream &operator<<(HashStream &stream, const Shader *shader)
{
    if (shader)
    {
        stream << shader->getSourceString().c_str() << shader->getSourceString().length()
               << shader->getCompilerResourcesString().c_str();
    }
    return stream;
}

HashStream &operator<<(HashStream &stream, const Program::Bindings &bindings)
{
    for (const auto &binding : bindings)
    {
        stream << binding.first << binding.second;
    }
    return stream;
}

HashStream &operator<<(HashStream &stream, const std::vector<std::string> &strings)
{
    for (const auto &str : strings)
    {
        stream << str;
    }
    return stream;
}

}  // anonymous namespace

MemoryProgramCache::MemoryProgramCache(size_t maxCacheSizeBytes)
    : mProgramBinaryCache(maxCacheSizeBytes), mIssuedWarnings(0)
{
}

MemoryProgramCache::~MemoryProgramCache()
{
}

// static
LinkResult MemoryProgramCache::Deserialize(const Context *context,
                                           const Program *program,
                                           ProgramState *state,
                                           const uint8_t *binary,
                                           size_t length,
                                           InfoLog &infoLog)
{
    BinaryInputStream stream(binary, length);

    unsigned char commitString[ANGLE_COMMIT_HASH_SIZE];
    stream.readBytes(commitString, ANGLE_COMMIT_HASH_SIZE);
    if (memcmp(commitString, ANGLE_COMMIT_HASH, sizeof(unsigned char) * ANGLE_COMMIT_HASH_SIZE) !=
        0)
    {
        infoLog << "Invalid program binary version.";
        return false;
    }

    int majorVersion = stream.readInt<int>();
    int minorVersion = stream.readInt<int>();
    if (majorVersion != context->getClientMajorVersion() ||
        minorVersion != context->getClientMinorVersion())
    {
        infoLog << "Cannot load program binaries across different ES context versions.";
        return false;
    }

    state->mComputeShaderLocalSize[0] = stream.readInt<int>();
    state->mComputeShaderLocalSize[1] = stream.readInt<int>();
    state->mComputeShaderLocalSize[2] = stream.readInt<int>();

    state->mNumViews = stream.readInt<int>();

    static_assert(MAX_VERTEX_ATTRIBS <= sizeof(unsigned long) * 8,
                  "Too many vertex attribs for mask");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
    state->mActiveAttribLocationsMask = stream.readInt<unsigned long>();
#pragma clang diagnostic pop

    unsigned int attribCount = stream.readInt<unsigned int>();
    ASSERT(state->mAttributes.empty());
    for (unsigned int attribIndex = 0; attribIndex < attribCount; ++attribIndex)
    {
        sh::Attribute attrib;
        LoadShaderVar(&stream, &attrib);
        attrib.location = stream.readInt<int>();
        state->mAttributes.push_back(attrib);
    }

    unsigned int uniformCount = stream.readInt<unsigned int>();
    ASSERT(state->mUniforms.empty());
    for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
    {
        LinkedUniform uniform;
        LoadShaderVar(&stream, &uniform);

        uniform.bufferIndex                = stream.readInt<int>();
        uniform.blockInfo.offset           = stream.readInt<int>();
        uniform.blockInfo.arrayStride      = stream.readInt<int>();
        uniform.blockInfo.matrixStride     = stream.readInt<int>();
        uniform.blockInfo.isRowMajorMatrix = stream.readBool();

        uniform.typeInfo = &GetUniformTypeInfo(uniform.type);

        state->mUniforms.push_back(uniform);
    }

    const unsigned int uniformIndexCount = stream.readInt<unsigned int>();
    ASSERT(state->mUniformLocations.empty());
    for (unsigned int uniformIndexIndex = 0; uniformIndexIndex < uniformIndexCount;
         uniformIndexIndex++)
    {
        VariableLocation variable;
        stream.readInt(&variable.arrayIndex);
        stream.readInt(&variable.index);
        stream.readBool(&variable.ignored);

        state->mUniformLocations.push_back(variable);
    }

    unsigned int uniformBlockCount = stream.readInt<unsigned int>();
    ASSERT(state->mUniformBlocks.empty());
    for (unsigned int uniformBlockIndex = 0; uniformBlockIndex < uniformBlockCount;
         ++uniformBlockIndex)
    {
        InterfaceBlock uniformBlock;
        LoadInterfaceBlock(&stream, &uniformBlock);
        state->mUniformBlocks.push_back(uniformBlock);

        state->mActiveUniformBlockBindings.set(uniformBlockIndex, uniformBlock.binding != 0);
    }

    unsigned int bufferVariableCount = stream.readInt<unsigned int>();
    ASSERT(state->mBufferVariables.empty());
    for (unsigned int index = 0; index < bufferVariableCount; ++index)
    {
        BufferVariable bufferVariable;
        LoadBufferVariable(&stream, &bufferVariable);
        state->mBufferVariables.push_back(bufferVariable);
    }

    unsigned int shaderStorageBlockCount = stream.readInt<unsigned int>();
    ASSERT(state->mShaderStorageBlocks.empty());
    for (unsigned int shaderStorageBlockIndex = 0;
         shaderStorageBlockIndex < shaderStorageBlockCount; ++shaderStorageBlockIndex)
    {
        InterfaceBlock shaderStorageBlock;
        LoadInterfaceBlock(&stream, &shaderStorageBlock);
        state->mShaderStorageBlocks.push_back(shaderStorageBlock);
    }

    unsigned int atomicCounterBufferCount = stream.readInt<unsigned int>();
    ASSERT(state->mAtomicCounterBuffers.empty());
    for (unsigned int bufferIndex = 0; bufferIndex < atomicCounterBufferCount; ++bufferIndex)
    {
        AtomicCounterBuffer atomicCounterBuffer;
        LoadShaderVariableBuffer(&stream, &atomicCounterBuffer);

        state->mAtomicCounterBuffers.push_back(atomicCounterBuffer);
    }

    unsigned int transformFeedbackVaryingCount = stream.readInt<unsigned int>();

    // Reject programs that use transform feedback varyings if the hardware cannot support them.
    if (transformFeedbackVaryingCount > 0 &&
        context->getWorkarounds().disableProgramCachingForTransformFeedback)
    {
        infoLog << "Current driver does not support transform feedback in binary programs.";
        return false;
    }

    ASSERT(state->mLinkedTransformFeedbackVaryings.empty());
    for (unsigned int transformFeedbackVaryingIndex = 0;
         transformFeedbackVaryingIndex < transformFeedbackVaryingCount;
         ++transformFeedbackVaryingIndex)
    {
        sh::Varying varying;
        stream.readIntVector<unsigned int>(&varying.arraySizes);
        stream.readInt(&varying.type);
        stream.readString(&varying.name);

        GLuint arrayIndex = stream.readInt<GLuint>();

        state->mLinkedTransformFeedbackVaryings.emplace_back(varying, arrayIndex);
    }

    stream.readInt(&state->mTransformFeedbackBufferMode);

    unsigned int outputCount = stream.readInt<unsigned int>();
    ASSERT(state->mOutputVariables.empty());
    for (unsigned int outputIndex = 0; outputIndex < outputCount; ++outputIndex)
    {
        sh::OutputVariable output;
        LoadShaderVar(&stream, &output);
        output.location = stream.readInt<int>();
        state->mOutputVariables.push_back(output);
    }

    unsigned int outputVarCount = stream.readInt<unsigned int>();
    ASSERT(state->mOutputLocations.empty());
    for (unsigned int outputIndex = 0; outputIndex < outputVarCount; ++outputIndex)
    {
        VariableLocation locationData;
        stream.readInt(&locationData.arrayIndex);
        stream.readInt(&locationData.index);
        stream.readBool(&locationData.ignored);
        state->mOutputLocations.push_back(locationData);
    }

    unsigned int outputTypeCount = stream.readInt<unsigned int>();
    for (unsigned int outputIndex = 0; outputIndex < outputTypeCount; ++outputIndex)
    {
        state->mOutputVariableTypes.push_back(stream.readInt<GLenum>());
    }
    static_assert(IMPLEMENTATION_MAX_DRAW_BUFFERS < 8 * sizeof(uint32_t),
                  "All bits of DrawBufferMask can be contained in an uint32_t");
    state->mActiveOutputVariables = stream.readInt<uint32_t>();

    unsigned int samplerRangeLow  = stream.readInt<unsigned int>();
    unsigned int samplerRangeHigh = stream.readInt<unsigned int>();
    state->mSamplerUniformRange   = RangeUI(samplerRangeLow, samplerRangeHigh);
    unsigned int samplerCount = stream.readInt<unsigned int>();
    for (unsigned int samplerIndex = 0; samplerIndex < samplerCount; ++samplerIndex)
    {
        GLenum textureType  = stream.readInt<GLenum>();
        size_t bindingCount = stream.readInt<size_t>();
        bool unreferenced   = stream.readBool();
        state->mSamplerBindings.emplace_back(
            SamplerBinding(textureType, bindingCount, unreferenced));
    }

    unsigned int imageRangeLow  = stream.readInt<unsigned int>();
    unsigned int imageRangeHigh = stream.readInt<unsigned int>();
    state->mImageUniformRange   = RangeUI(imageRangeLow, imageRangeHigh);
    unsigned int imageBindingCount = stream.readInt<unsigned int>();
    for (unsigned int imageIndex = 0; imageIndex < imageBindingCount; ++imageIndex)
    {
        unsigned int elementCount = stream.readInt<unsigned int>();
        ImageBinding imageBinding(elementCount);
        for (unsigned int i = 0; i < elementCount; ++i)
        {
            imageBinding.boundImageUnits[i] = stream.readInt<unsigned int>();
        }
        state->mImageBindings.emplace_back(imageBinding);
    }

    unsigned int atomicCounterRangeLow  = stream.readInt<unsigned int>();
    unsigned int atomicCounterRangeHigh = stream.readInt<unsigned int>();
    state->mAtomicCounterUniformRange   = RangeUI(atomicCounterRangeLow, atomicCounterRangeHigh);

    static_assert(SHADER_TYPE_MAX <= sizeof(unsigned long) * 8, "Too many shader types");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
    state->mLinkedShaderStages = stream.readInt<unsigned long>();
#pragma clang diagnostic pop

    return program->getImplementation()->load(context, infoLog, &stream);
}

// static
void MemoryProgramCache::Serialize(const Context *context,
                                   const gl::Program *program,
                                   angle::MemoryBuffer *binaryOut)
{
    BinaryOutputStream stream;

    stream.writeBytes(reinterpret_cast<const unsigned char *>(ANGLE_COMMIT_HASH),
                      ANGLE_COMMIT_HASH_SIZE);

    // nullptr context is supported when computing binary length.
    if (context)
    {
        stream.writeInt(context->getClientVersion().major);
        stream.writeInt(context->getClientVersion().minor);
    }
    else
    {
        stream.writeInt(2);
        stream.writeInt(0);
    }

    const auto &state = program->getState();

    const auto &computeLocalSize = state.getComputeShaderLocalSize();

    stream.writeInt(computeLocalSize[0]);
    stream.writeInt(computeLocalSize[1]);
    stream.writeInt(computeLocalSize[2]);

    stream.writeInt(state.mNumViews);

    stream.writeInt(state.getActiveAttribLocationsMask().to_ulong());

    stream.writeInt(state.getAttributes().size());
    for (const sh::Attribute &attrib : state.getAttributes())
    {
        WriteShaderVar(&stream, attrib);
        stream.writeInt(attrib.location);
    }

    stream.writeInt(state.getUniforms().size());
    for (const LinkedUniform &uniform : state.getUniforms())
    {
        WriteShaderVar(&stream, uniform);

        // FIXME: referenced

        stream.writeInt(uniform.bufferIndex);
        stream.writeInt(uniform.blockInfo.offset);
        stream.writeInt(uniform.blockInfo.arrayStride);
        stream.writeInt(uniform.blockInfo.matrixStride);
        stream.writeInt(uniform.blockInfo.isRowMajorMatrix);
    }

    stream.writeInt(state.getUniformLocations().size());
    for (const auto &variable : state.getUniformLocations())
    {
        stream.writeInt(variable.arrayIndex);
        stream.writeIntOrNegOne(variable.index);
        stream.writeInt(variable.ignored);
    }

    stream.writeInt(state.getUniformBlocks().size());
    for (const InterfaceBlock &uniformBlock : state.getUniformBlocks())
    {
        WriteInterfaceBlock(&stream, uniformBlock);
    }

    stream.writeInt(state.getBufferVariables().size());
    for (const BufferVariable &bufferVariable : state.getBufferVariables())
    {
        WriteBufferVariable(&stream, bufferVariable);
    }

    stream.writeInt(state.getShaderStorageBlocks().size());
    for (const InterfaceBlock &shaderStorageBlock : state.getShaderStorageBlocks())
    {
        WriteInterfaceBlock(&stream, shaderStorageBlock);
    }

    stream.writeInt(state.mAtomicCounterBuffers.size());
    for (const auto &atomicCounterBuffer : state.mAtomicCounterBuffers)
    {
        WriteShaderVariableBuffer(&stream, atomicCounterBuffer);
    }

    // Warn the app layer if saving a binary with unsupported transform feedback.
    if (!state.getLinkedTransformFeedbackVaryings().empty() &&
        context->getWorkarounds().disableProgramCachingForTransformFeedback)
    {
        WARN() << "Saving program binary with transform feedback, which is not supported on this "
                  "driver.";
    }

    stream.writeInt(state.getLinkedTransformFeedbackVaryings().size());
    for (const auto &var : state.getLinkedTransformFeedbackVaryings())
    {
        stream.writeIntVector(var.arraySizes);
        stream.writeInt(var.type);
        stream.writeString(var.name);

        stream.writeIntOrNegOne(var.arrayIndex);
    }

    stream.writeInt(state.getTransformFeedbackBufferMode());

    stream.writeInt(state.getOutputVariables().size());
    for (const sh::OutputVariable &output : state.getOutputVariables())
    {
        WriteShaderVar(&stream, output);
        stream.writeInt(output.location);
    }

    stream.writeInt(state.getOutputLocations().size());
    for (const auto &outputVar : state.getOutputLocations())
    {
        stream.writeInt(outputVar.arrayIndex);
        stream.writeIntOrNegOne(outputVar.index);
        stream.writeInt(outputVar.ignored);
    }

    stream.writeInt(state.mOutputVariableTypes.size());
    for (const auto &outputVariableType : state.mOutputVariableTypes)
    {
        stream.writeInt(outputVariableType);
    }

    static_assert(IMPLEMENTATION_MAX_DRAW_BUFFERS < 8 * sizeof(uint32_t),
                  "All bits of DrawBufferMask can be contained in an uint32_t");
    stream.writeInt(static_cast<uint32_t>(state.mActiveOutputVariables.to_ulong()));

    stream.writeInt(state.getSamplerUniformRange().low());
    stream.writeInt(state.getSamplerUniformRange().high());

    stream.writeInt(state.getSamplerBindings().size());
    for (const auto &samplerBinding : state.getSamplerBindings())
    {
        stream.writeInt(samplerBinding.textureType);
        stream.writeInt(samplerBinding.boundTextureUnits.size());
        stream.writeInt(samplerBinding.unreferenced);
    }

    stream.writeInt(state.getImageUniformRange().low());
    stream.writeInt(state.getImageUniformRange().high());

    stream.writeInt(state.getImageBindings().size());
    for (const auto &imageBinding : state.getImageBindings())
    {
        stream.writeInt(imageBinding.boundImageUnits.size());
        for (size_t i = 0; i < imageBinding.boundImageUnits.size(); ++i)
        {
            stream.writeInt(imageBinding.boundImageUnits[i]);
        }
    }

    stream.writeInt(state.getAtomicCounterUniformRange().low());
    stream.writeInt(state.getAtomicCounterUniformRange().high());

    stream.writeInt(state.getLinkedShaderStages().to_ulong());

    program->getImplementation()->save(context, &stream);

    ASSERT(binaryOut);
    binaryOut->resize(stream.length());
    memcpy(binaryOut->data(), stream.data(), stream.length());
}

// static
void MemoryProgramCache::ComputeHash(const Context *context,
                                     const Program *program,
                                     ProgramHash *hashOut)
{
    const Shader *vertexShader   = program->getAttachedVertexShader();
    const Shader *fragmentShader = program->getAttachedFragmentShader();
    const Shader *computeShader  = program->getAttachedComputeShader();

    // Compute the program hash. Start with the shader hashes and resource strings.
    HashStream hashStream;
    hashStream << vertexShader << fragmentShader << computeShader;

    // Add some ANGLE metadata and Context properties, such as version and back-end.
    hashStream << ANGLE_COMMIT_HASH << context->getClientMajorVersion()
               << context->getClientMinorVersion() << context->getString(GL_RENDERER);

    // Hash pre-link program properties.
    hashStream << program->getAttributeBindings() << program->getUniformLocationBindings()
               << program->getFragmentInputBindings()
               << program->getState().getTransformFeedbackVaryingNames()
               << program->getState().getTransformFeedbackBufferMode();

    // Call the secure SHA hashing function.
    const std::string &programKey = hashStream.str();
    angle::base::SHA1HashBytes(reinterpret_cast<const unsigned char *>(programKey.c_str()),
                               programKey.length(), hashOut->data());
}

LinkResult MemoryProgramCache::getProgram(const Context *context,
                                          const Program *program,
                                          ProgramState *state,
                                          ProgramHash *hashOut)
{
    ComputeHash(context, program, hashOut);
    const angle::MemoryBuffer *binaryProgram = nullptr;
    LinkResult result(false);
    if (get(*hashOut, &binaryProgram))
    {
        InfoLog infoLog;
        ANGLE_TRY_RESULT(Deserialize(context, program, state, binaryProgram->data(),
                                     binaryProgram->size(), infoLog),
                         result);
        ANGLE_HISTOGRAM_BOOLEAN("GPU.ANGLE.ProgramCache.LoadBinarySuccess", result.getResult());
        if (!result.getResult())
        {
            // Cache load failed, evict.
            if (mIssuedWarnings++ < kWarningLimit)
            {
                WARN() << "Failed to load binary from cache: " << infoLog.str();

                if (mIssuedWarnings == kWarningLimit)
                {
                    WARN() << "Reaching warning limit for cache load failures, silencing "
                              "subsequent warnings.";
                }
            }
            remove(*hashOut);
        }
    }
    return result;
}

bool MemoryProgramCache::get(const ProgramHash &programHash, const angle::MemoryBuffer **programOut)
{
    const CacheEntry *entry = nullptr;
    if (!mProgramBinaryCache.get(programHash, &entry))
    {
        ANGLE_HISTOGRAM_ENUMERATION("GPU.ANGLE.ProgramCache.CacheResult", kCacheMiss,
                                    kCacheResultMax);
        return false;
    }

    if (entry->second == CacheSource::PutProgram)
    {
        ANGLE_HISTOGRAM_ENUMERATION("GPU.ANGLE.ProgramCache.CacheResult", kCacheHitMemory,
                                    kCacheResultMax);
    }
    else
    {
        ANGLE_HISTOGRAM_ENUMERATION("GPU.ANGLE.ProgramCache.CacheResult", kCacheHitDisk,
                                    kCacheResultMax);
    }

    *programOut = &entry->first;
    return true;
}

bool MemoryProgramCache::getAt(size_t index,
                               ProgramHash *hashOut,
                               const angle::MemoryBuffer **programOut)
{
    const CacheEntry *entry = nullptr;
    if (!mProgramBinaryCache.getAt(index, hashOut, &entry))
    {
        return false;
    }

    *programOut = &entry->first;
    return true;
}

void MemoryProgramCache::remove(const ProgramHash &programHash)
{
    bool result = mProgramBinaryCache.eraseByKey(programHash);
    ASSERT(result);
}

void MemoryProgramCache::putProgram(const ProgramHash &programHash,
                                    const Context *context,
                                    const Program *program)
{
    CacheEntry newEntry;
    Serialize(context, program, &newEntry.first);
    newEntry.second = CacheSource::PutProgram;

    ANGLE_HISTOGRAM_COUNTS("GPU.ANGLE.ProgramCache.ProgramBinarySizeBytes",
                           static_cast<int>(newEntry.first.size()));

    const CacheEntry *result =
        mProgramBinaryCache.put(programHash, std::move(newEntry), newEntry.first.size());
    if (!result)
    {
        ERR() << "Failed to store binary program in memory cache, program is too large.";
    }
    else
    {
        auto *platform = ANGLEPlatformCurrent();
        platform->cacheProgram(platform, programHash, result->first.size(), result->first.data());
    }
}

void MemoryProgramCache::updateProgram(const Context *context, const Program *program)
{
    gl::ProgramHash programHash;
    ComputeHash(context, program, &programHash);
    putProgram(programHash, context, program);
}

void MemoryProgramCache::putBinary(const ProgramHash &programHash,
                                   const uint8_t *binary,
                                   size_t length)
{
    // Copy the binary.
    CacheEntry newEntry;
    newEntry.first.resize(length);
    memcpy(newEntry.first.data(), binary, length);
    newEntry.second = CacheSource::PutBinary;

    // Store the binary.
    const CacheEntry *result = mProgramBinaryCache.put(programHash, std::move(newEntry), length);
    if (!result)
    {
        ERR() << "Failed to store binary program in memory cache, program is too large.";
    }
}

void MemoryProgramCache::clear()
{
    mProgramBinaryCache.clear();
    mIssuedWarnings = 0;
}

void MemoryProgramCache::resize(size_t maxCacheSizeBytes)
{
    mProgramBinaryCache.resize(maxCacheSizeBytes);
}

size_t MemoryProgramCache::entryCount() const
{
    return mProgramBinaryCache.entryCount();
}

size_t MemoryProgramCache::trim(size_t limit)
{
    return mProgramBinaryCache.shrinkToSize(limit);
}

size_t MemoryProgramCache::size() const
{
    return mProgramBinaryCache.size();
}

size_t MemoryProgramCache::maxSize() const
{
    return mProgramBinaryCache.maxSize();
}

}  // namespace gl
