//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramVk.cpp:
//    Implements the class methods for ProgramVk.
//

#include "libANGLE/renderer/vulkan/ProgramVk.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/GlslangWrapperVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"

namespace rx
{

namespace
{
// This size is picked according to the required maxUniformBufferRange in the Vulkan spec.
constexpr size_t kUniformBlockDynamicBufferMinSize = 16384u;

// Identical to Std140 encoder in all aspects, except it ignores opaque uniform types.
class VulkanDefaultBlockEncoder : public sh::Std140BlockEncoder
{
  public:
    void advanceOffset(GLenum type,
                       const std::vector<unsigned int> &arraySizes,
                       bool isRowMajorMatrix,
                       int arrayStride,
                       int matrixStride) override
    {
        if (gl::IsOpaqueType(type))
        {
            return;
        }

        sh::Std140BlockEncoder::advanceOffset(type, arraySizes, isRowMajorMatrix, arrayStride,
                                              matrixStride);
    }
};

void InitDefaultUniformBlock(const std::vector<sh::ShaderVariable> &uniforms,
                             sh::BlockLayoutMap *blockLayoutMapOut,
                             size_t *blockSizeOut)
{
    if (uniforms.empty())
    {
        *blockSizeOut = 0;
        return;
    }

    VulkanDefaultBlockEncoder blockEncoder;
    sh::GetActiveUniformBlockInfo(uniforms, "", &blockEncoder, blockLayoutMapOut);

    size_t blockSize = blockEncoder.getCurrentOffset();

    // TODO(jmadill): I think we still need a valid block for the pipeline even if zero sized.
    if (blockSize == 0)
    {
        *blockSizeOut = 0;
        return;
    }

    *blockSizeOut = blockSize;
    return;
}

template <typename T>
void UpdateDefaultUniformBlock(GLsizei count,
                               uint32_t arrayIndex,
                               int componentCount,
                               const T *v,
                               const sh::BlockMemberInfo &layoutInfo,
                               angle::MemoryBuffer *uniformData)
{
    const int elementSize = sizeof(T) * componentCount;

    uint8_t *dst = uniformData->data() + layoutInfo.offset;
    if (layoutInfo.arrayStride == 0 || layoutInfo.arrayStride == elementSize)
    {
        uint32_t arrayOffset = arrayIndex * layoutInfo.arrayStride;
        uint8_t *writePtr    = dst + arrayOffset;
        ASSERT(writePtr + (elementSize * count) <= uniformData->data() + uniformData->size());
        memcpy(writePtr, v, elementSize * count);
    }
    else
    {
        // Have to respect the arrayStride between each element of the array.
        int maxIndex = arrayIndex + count;
        for (int writeIndex = arrayIndex, readIndex = 0; writeIndex < maxIndex;
             writeIndex++, readIndex++)
        {
            const int arrayOffset = writeIndex * layoutInfo.arrayStride;
            uint8_t *writePtr     = dst + arrayOffset;
            const T *readPtr      = v + (readIndex * componentCount);
            ASSERT(writePtr + elementSize <= uniformData->data() + uniformData->size());
            memcpy(writePtr, readPtr, elementSize);
        }
    }
}

template <typename T>
void ReadFromDefaultUniformBlock(int componentCount,
                                 uint32_t arrayIndex,
                                 T *dst,
                                 const sh::BlockMemberInfo &layoutInfo,
                                 const angle::MemoryBuffer *uniformData)
{
    ASSERT(layoutInfo.offset != -1);

    const int elementSize = sizeof(T) * componentCount;
    const uint8_t *source = uniformData->data() + layoutInfo.offset;

    if (layoutInfo.arrayStride == 0 || layoutInfo.arrayStride == elementSize)
    {
        const uint8_t *readPtr = source + arrayIndex * layoutInfo.arrayStride;
        memcpy(dst, readPtr, elementSize);
    }
    else
    {
        // Have to respect the arrayStride between each element of the array.
        const int arrayOffset  = arrayIndex * layoutInfo.arrayStride;
        const uint8_t *readPtr = source + arrayOffset;
        memcpy(dst, readPtr, elementSize);
    }
}

angle::Result SyncDefaultUniformBlock(ContextVk *contextVk,
                                      vk::DynamicBuffer *dynamicBuffer,
                                      const angle::MemoryBuffer &bufferData,
                                      uint32_t *outOffset,
                                      bool *outBufferModified)
{
    dynamicBuffer->releaseInFlightBuffers(contextVk);

    ASSERT(!bufferData.empty());
    uint8_t *data       = nullptr;
    VkBuffer *outBuffer = nullptr;
    VkDeviceSize offset = 0;
    ANGLE_TRY(dynamicBuffer->allocate(contextVk, bufferData.size(), &data, outBuffer, &offset,
                                      outBufferModified));
    *outOffset = static_cast<uint32_t>(offset);
    memcpy(data, bufferData.data(), bufferData.size());
    ANGLE_TRY(dynamicBuffer->flush(contextVk));
    return angle::Result::Continue;
}

class Std140BlockLayoutEncoderFactory : public gl::CustomBlockLayoutEncoderFactory
{
  public:
    sh::BlockLayoutEncoder *makeEncoder() override { return new sh::Std140BlockEncoder(); }
};
}  // anonymous namespace

// ProgramVk implementation.
ProgramVk::ProgramVk(const gl::ProgramState &state) : ProgramImpl(state)
{
    GlslangWrapperVk::ResetGlslangProgramInterfaceInfo(&mGlslangProgramInterfaceInfo);
}

ProgramVk::~ProgramVk() = default;

void ProgramVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    reset(contextVk);
}

void ProgramVk::reset(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    mShaderInfo.release(contextVk);

    for (auto &uniformBlock : mDefaultUniformBlocks)
    {
        uniformBlock.storage.release(renderer);
    }

    GlslangWrapperVk::ResetGlslangProgramInterfaceInfo(&mGlslangProgramInterfaceInfo);

    mExecutable.reset(contextVk);
}

std::unique_ptr<rx::LinkEvent> ProgramVk::load(const gl::Context *context,
                                               gl::BinaryInputStream *stream,
                                               gl::InfoLog &infoLog)
{
    ContextVk *contextVk = vk::GetImpl(context);
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);

    reset(contextVk);

    mShaderInfo.load(stream);
    mExecutable.load(stream);

    // Deserializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const size_t uniformCount = stream->readInt<size_t>();
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
        {
            sh::BlockMemberInfo blockInfo;
            gl::LoadBlockMemberInfo(stream, &blockInfo);
            mDefaultUniformBlocks[shaderType].uniformLayout.push_back(blockInfo);
        }
    }

    // Deserializes required uniform block memory sizes
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        requiredBufferSize[shaderType] = stream->readInt<size_t>();
    }

    // Initialize and resize the mDefaultUniformBlocks' memory
    angle::Result status = resizeUniformBlockMemory(contextVk, requiredBufferSize);
    if (status != angle::Result::Continue)
    {
        return std::make_unique<LinkEventDone>(status);
    }

    const gl::ProgramExecutable &glExecutable = mState.getProgramExecutable();
    status = mExecutable.createPipelineLayout(context, glExecutable, mState);
    return std::make_unique<LinkEventDone>(status);
}

void ProgramVk::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    mShaderInfo.save(stream);
    mExecutable.save(stream);

    // Serializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const size_t uniformCount = mDefaultUniformBlocks[shaderType].uniformLayout.size();
        stream->writeInt<size_t>(uniformCount);
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
        {
            sh::BlockMemberInfo &blockInfo =
                mDefaultUniformBlocks[shaderType].uniformLayout[uniformIndex];
            gl::WriteBlockMemberInfo(stream, blockInfo);
        }
    }

    // Serializes required uniform block memory sizes
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt(mDefaultUniformBlocks[shaderType].uniformData.size());
    }
}

void ProgramVk::setBinaryRetrievableHint(bool retrievable)
{
    UNIMPLEMENTED();
}

void ProgramVk::setSeparable(bool separable)
{
    // Nohting to do here yet.
}

std::unique_ptr<LinkEvent> ProgramVk::link(const gl::Context *context,
                                           const gl::ProgramLinkedResources &resources,
                                           gl::InfoLog &infoLog)
{
    ContextVk *contextVk = vk::GetImpl(context);
    // Link resources before calling GetShaderSource to make sure they are ready for the set/binding
    // assignment done in that function.
    linkResources(resources);

    reset(contextVk);
    mExecutable.clearVariableInfoMap();

    // Gather variable info and transform sources.
    gl::ShaderMap<std::string> shaderSources;
    GlslangWrapperVk::GetShaderSource(contextVk->getRenderer()->getFeatures(), mState, resources,
                                      &mGlslangProgramInterfaceInfo, &shaderSources,
                                      &mExecutable.mVariableInfoMap);

    // Compile the shaders.
    angle::Result status =
        mShaderInfo.initShaders(contextVk, shaderSources, mExecutable.mVariableInfoMap);
    if (status != angle::Result::Continue)
    {
        return std::make_unique<LinkEventDone>(status);
    }

    status = initDefaultUniformBlocks(context);
    if (status != angle::Result::Continue)
    {
        return std::make_unique<LinkEventDone>(status);
    }

    // TODO(jie.a.chen@intel.com): Parallelize linking.
    // http://crbug.com/849576
    const gl::ProgramExecutable &glExecutable = mState.getProgramExecutable();
    status = mExecutable.createPipelineLayout(context, glExecutable, mState);
    return std::make_unique<LinkEventDone>(status);
}

void ProgramVk::linkResources(const gl::ProgramLinkedResources &resources)
{
    Std140BlockLayoutEncoderFactory std140EncoderFactory;
    gl::ProgramLinkedResourcesLinker linker(&std140EncoderFactory);

    linker.linkResources(mState, resources);
}

angle::Result ProgramVk::initDefaultUniformBlocks(const gl::Context *glContext)
{
    ContextVk *contextVk = vk::GetImpl(glContext);

    // Process vertex and fragment uniforms into std140 packing.
    gl::ShaderMap<sh::BlockLayoutMap> layoutMap;
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);

    generateUniformLayoutMapping(layoutMap, requiredBufferSize);
    initDefaultUniformLayoutMapping(layoutMap);

    // All uniform initializations are complete, now resize the buffers accordingly and return
    return resizeUniformBlockMemory(contextVk, requiredBufferSize);
}

void ProgramVk::generateUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> &layoutMap,
                                             gl::ShaderMap<size_t> &requiredBufferSize)
{
    const gl::ProgramExecutable &glExecutable = mState.getProgramExecutable();

    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        gl::Shader *shader = mState.getAttachedShader(shaderType);

        if (shader)
        {
            const std::vector<sh::ShaderVariable> &uniforms = shader->getUniforms();
            InitDefaultUniformBlock(uniforms, &layoutMap[shaderType],
                                    &requiredBufferSize[shaderType]);
        }
    }
}

void ProgramVk::initDefaultUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> &layoutMap)
{
    // Init the default block layout info.
    const auto &uniforms                      = mState.getUniforms();
    const gl::ProgramExecutable &glExecutable = mState.getProgramExecutable();

    for (const gl::VariableLocation &location : mState.getUniformLocations())
    {
        gl::ShaderMap<sh::BlockMemberInfo> layoutInfo;

        if (location.used() && !location.ignored)
        {
            const auto &uniform = uniforms[location.index];
            if (uniform.isInDefaultBlock() && !uniform.isSampler() && !uniform.isImage())
            {
                std::string uniformName = uniform.name;
                if (uniform.isArray())
                {
                    // Gets the uniform name without the [0] at the end.
                    uniformName = gl::StripLastArrayIndex(uniformName);
                    ASSERT(uniformName.size() != uniform.name.size());
                }

                bool found = false;

                for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
                {
                    auto it = layoutMap[shaderType].find(uniformName);
                    if (it != layoutMap[shaderType].end())
                    {
                        found                  = true;
                        layoutInfo[shaderType] = it->second;
                    }
                }

                ASSERT(found);
            }
        }

        for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
        {
            mDefaultUniformBlocks[shaderType].uniformLayout.push_back(layoutInfo[shaderType]);
        }
    }
}

angle::Result ProgramVk::resizeUniformBlockMemory(ContextVk *contextVk,
                                                  gl::ShaderMap<size_t> &requiredBufferSize)
{
    RendererVk *renderer                      = contextVk->getRenderer();
    const gl::ProgramExecutable &glExecutable = mState.getProgramExecutable();

    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        if (requiredBufferSize[shaderType] > 0)
        {
            if (!mDefaultUniformBlocks[shaderType].uniformData.resize(
                    requiredBufferSize[shaderType]))
            {
                ANGLE_VK_CHECK(contextVk, false, VK_ERROR_OUT_OF_HOST_MEMORY);
            }
            size_t minAlignment = static_cast<size_t>(
                renderer->getPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment);

            mDefaultUniformBlocks[shaderType].storage.init(
                renderer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                minAlignment, kUniformBlockDynamicBufferMinSize, true);

            // Initialize uniform buffer memory to zero by default.
            mDefaultUniformBlocks[shaderType].uniformData.fill(0);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }

    return angle::Result::Continue;
}

GLboolean ProgramVk::validate(const gl::Caps &caps, gl::InfoLog *infoLog)
{
    // No-op. The spec is very vague about the behavior of validation.
    return GL_TRUE;
}

template <typename T>
void ProgramVk::setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType)
{
    const gl::VariableLocation &locationInfo  = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform    = mState.getUniforms()[locationInfo.index];
    const gl::ProgramExecutable &glExecutable = mState.getProgramExecutable();

    ASSERT(!linkedUniform.isSampler());

    if (linkedUniform.typeInfo->type == entryPointType)
    {
        for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
        {
            DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.typeInfo->componentCount;
            UpdateDefaultUniformBlock(count, locationInfo.arrayIndex, componentCount, v, layoutInfo,
                                      &uniformBlock.uniformData);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
    else
    {
        for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
        {
            DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.typeInfo->componentCount;

            ASSERT(linkedUniform.typeInfo->type == gl::VariableBoolVectorType(entryPointType));

            GLint initialArrayOffset =
                locationInfo.arrayIndex * layoutInfo.arrayStride + layoutInfo.offset;
            for (GLint i = 0; i < count; i++)
            {
                GLint elementOffset = i * layoutInfo.arrayStride + initialArrayOffset;
                GLint *dest =
                    reinterpret_cast<GLint *>(uniformBlock.uniformData.data() + elementOffset);
                const T *source = v + i * componentCount;

                for (int c = 0; c < componentCount; c++)
                {
                    dest[c] = (source[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE;
                }
            }

            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
}

template <typename T>
void ProgramVk::getUniformImpl(GLint location, T *v, GLenum entryPointType) const
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    ASSERT(!linkedUniform.isSampler() && !linkedUniform.isImage());

    const gl::ShaderType shaderType = linkedUniform.getFirstShaderTypeWhereActive();
    ASSERT(shaderType != gl::ShaderType::InvalidEnum);

    const DefaultUniformBlock &uniformBlock = mDefaultUniformBlocks[shaderType];
    const sh::BlockMemberInfo &layoutInfo   = uniformBlock.uniformLayout[location];

    ASSERT(linkedUniform.typeInfo->componentType == entryPointType ||
           linkedUniform.typeInfo->componentType == gl::VariableBoolVectorType(entryPointType));

    if (gl::IsMatrixType(linkedUniform.type))
    {
        const uint8_t *ptrToElement = uniformBlock.uniformData.data() + layoutInfo.offset +
                                      (locationInfo.arrayIndex * layoutInfo.arrayStride);
        GetMatrixUniform(linkedUniform.type, v, reinterpret_cast<const T *>(ptrToElement), false);
    }
    else
    {
        ReadFromDefaultUniformBlock(linkedUniform.typeInfo->componentCount, locationInfo.arrayIndex,
                                    v, layoutInfo, &uniformBlock.uniformData);
    }
}

void ProgramVk::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT);
}

void ProgramVk::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC2);
}

void ProgramVk::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC3);
}

void ProgramVk::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC4);
}

void ProgramVk::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];
    if (linkedUniform.isSampler())
    {
        // We could potentially cache some indexing here. For now this is a no-op since the mapping
        // is handled entirely in ContextVk.
        return;
    }

    setUniformImpl(location, count, v, GL_INT);
}

void ProgramVk::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC2);
}

void ProgramVk::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC3);
}

void ProgramVk::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC4);
}

void ProgramVk::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT);
}

void ProgramVk::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC2);
}

void ProgramVk::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC3);
}

void ProgramVk::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC4);
}

template <int cols, int rows>
void ProgramVk::setUniformMatrixfv(GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat *value)
{
    const gl::VariableLocation &locationInfo  = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform    = mState.getUniforms()[locationInfo.index];
    const gl::ProgramExecutable &glExecutable = mState.getProgramExecutable();

    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
        const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

        // Assume an offset of -1 means the block is unused.
        if (layoutInfo.offset == -1)
        {
            continue;
        }

        SetFloatUniformMatrixGLSL<cols, rows>::Run(
            locationInfo.arrayIndex, linkedUniform.getArraySizeProduct(), count, transpose, value,
            uniformBlock.uniformData.data() + layoutInfo.offset);

        mDefaultUniformBlocksDirty.set(shaderType);
    }
}

void ProgramVk::setUniformMatrix2fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    setUniformMatrixfv<2, 2>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix3fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    setUniformMatrixfv<3, 3>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix4fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    setUniformMatrixfv<4, 4>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix2x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<2, 3>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix3x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<3, 2>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix2x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<2, 4>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix4x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<4, 2>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix3x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<3, 4>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix4x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<4, 3>(location, count, transpose, value);
}

void ProgramVk::getUniformfv(const gl::Context *context, GLint location, GLfloat *params) const
{
    getUniformImpl(location, params, GL_FLOAT);
}

void ProgramVk::getUniformiv(const gl::Context *context, GLint location, GLint *params) const
{
    getUniformImpl(location, params, GL_INT);
}

void ProgramVk::getUniformuiv(const gl::Context *context, GLint location, GLuint *params) const
{
    getUniformImpl(location, params, GL_UNSIGNED_INT);
}

angle::Result ProgramVk::updateUniforms(ContextVk *contextVk)
{
    ASSERT(dirtyUniforms());

    bool anyNewBufferAllocated                = false;
    uint32_t offsetIndex                      = 0;
    const gl::ProgramExecutable &glExecutable = mState.getProgramExecutable();

    // Update buffer memory by immediate mapping. This immediate update only works once.
    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        DefaultUniformBlock &uniformBlock = mDefaultUniformBlocks[shaderType];

        if (mDefaultUniformBlocksDirty[shaderType])
        {
            bool bufferModified = false;
            ANGLE_TRY(SyncDefaultUniformBlock(
                contextVk, &uniformBlock.storage, uniformBlock.uniformData,
                &mExecutable.mDynamicBufferOffsets[offsetIndex], &bufferModified));
            mDefaultUniformBlocksDirty.reset(shaderType);

            if (bufferModified)
            {
                anyNewBufferAllocated = true;
            }
        }

        ++offsetIndex;
    }

    if (anyNewBufferAllocated)
    {
        // We need to reinitialize the descriptor sets if we newly allocated buffers since we can't
        // modify the descriptor sets once initialized.
        ANGLE_TRY(mExecutable.allocateDescriptorSet(contextVk, kUniformsAndXfbDescriptorSetIndex));
        mExecutable.updateDefaultUniformsDescriptorSet(mState, mDefaultUniformBlocks, contextVk);
        mExecutable.updateTransformFeedbackDescriptorSetImpl(mState, contextVk);
    }

    return angle::Result::Continue;
}

void ProgramVk::setDefaultUniformBlocksMinSizeForTesting(size_t minSize)
{
    for (DefaultUniformBlock &block : mDefaultUniformBlocks)
    {
        block.storage.setMinimumSizeForTesting(minSize);
    }
}

}  // namespace rx
