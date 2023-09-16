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
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"

namespace rx
{

namespace
{
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

class Std140BlockLayoutEncoderFactory : public gl::CustomBlockLayoutEncoderFactory
{
  public:
    sh::BlockLayoutEncoder *makeEncoder() override { return new sh::Std140BlockEncoder(); }
};

class LinkTaskVk final : public vk::Context, public angle::Closure
{
  public:
    LinkTaskVk(RendererVk *renderer,
               PipelineLayoutCache &pipelineLayoutCache,
               DescriptorSetLayoutCache &descriptorSetLayoutCache,
               const gl::ProgramState &state,
               gl::ProgramMergedVaryings &&mergedVaryings,
               const gl::ProgramLinkedResources &resources,
               bool isGLES1,
               vk::PipelineRobustness pipelineRobustness,
               vk::PipelineProtectedAccess pipelineProtectedAccess)
        : vk::Context(renderer),
          mState(state),
          mExecutable(&mState.getExecutable()),
          mMergedVaryings(std::move(mergedVaryings)),
          mResources(resources),
          mIsGLES1(isGLES1),
          mPipelineRobustness(pipelineRobustness),
          mPipelineProtectedAccess(pipelineProtectedAccess),
          mPipelineLayoutCache(pipelineLayoutCache),
          mDescriptorSetLayoutCache(descriptorSetLayoutCache)
    {}

    void operator()() override
    {
        angle::Result result = linkImpl();
        ASSERT((result == angle::Result::Continue) == (mErrorCode == VK_SUCCESS));
    }

    void handleError(VkResult result,
                     const char *file,
                     const char *function,
                     unsigned int line) override
    {
        mErrorCode     = result;
        mErrorFile     = file;
        mErrorFunction = function;
        mErrorLine     = line;
    }

    angle::Result getResult(ContextVk *contextVk)
    {
        ProgramExecutableVk *executableVk = vk::GetImpl(mExecutable);

        // Clean up garbage first, it's not no matter what may fail below.
        mCompatibleRenderPass.destroy(contextVk->getDevice());

        ANGLE_TRY(executableVk->initializeDescriptorPools(contextVk,
                                                          &contextVk->getDescriptorSetLayoutCache(),
                                                          &contextVk->getMetaDescriptorPools()));

        // If the program uses framebuffer fetch and this is the first time this happens, switch the
        // context to "framebuffer fetch mode".  In this mode, all render passes assume framebuffer
        // fetch may be used, so they are prepared to accept a program that uses input attachments.
        // This is done only when a program with framebuffer fetch is created to avoid potential
        // performance impact on applications that don't use this extension.  If other contexts in
        // the share group use this program, they will lazily switch to this mode.
        //
        // This is purely an optimization (to avoid creating and later releasing) non-framebuffer
        // fetch render passes.
        if (contextVk->getFeatures().permanentlySwitchToFramebufferFetchMode.enabled &&
            mExecutable->usesFramebufferFetch())
        {
            ANGLE_TRY(contextVk->switchToFramebufferFetchMode(true));
        }

        // Update the relevant perf counters
        angle::VulkanPerfCounters &from = contextVk->getPerfCounters();
        angle::VulkanPerfCounters &to   = getPerfCounters();

        to.pipelineCreationCacheHits += from.pipelineCreationCacheHits;
        to.pipelineCreationCacheMisses += from.pipelineCreationCacheMisses;
        to.pipelineCreationTotalCacheHitsDurationNs +=
            from.pipelineCreationTotalCacheHitsDurationNs;
        to.pipelineCreationTotalCacheMissesDurationNs +=
            from.pipelineCreationTotalCacheMissesDurationNs;

        // Forward any errors
        if (mErrorCode != VK_SUCCESS)
        {
            contextVk->handleError(mErrorCode, mErrorFile, mErrorFunction, mErrorLine);
            return angle::Result::Stop;
        }
        return angle::Result::Continue;
    }

  private:
    angle::Result linkImpl();

    void linkResources();
    angle::Result initDefaultUniformBlocks();
    void generateUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> *layoutMapOut,
                                      gl::ShaderMap<size_t> *requiredBufferSizeOut);
    void initDefaultUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> *layoutMapOut);

    // The front-end ensures that the program is not accessed while linking, so it is safe to
    // direclty access the state from a potentially parallel job.
    const gl::ProgramState &mState;
    const gl::ProgramExecutable *mExecutable;
    const gl::ProgramMergedVaryings mMergedVaryings;
    const gl::ProgramLinkedResources &mResources;
    const bool mIsGLES1;
    const vk::PipelineRobustness mPipelineRobustness;
    const vk::PipelineProtectedAccess mPipelineProtectedAccess;

    // Helpers that are interally thread-safe
    PipelineLayoutCache &mPipelineLayoutCache;
    DescriptorSetLayoutCache &mDescriptorSetLayoutCache;

    // Temporary objects to clean up at the end
    vk::RenderPass mCompatibleRenderPass;

    // Error handling
    VkResult mErrorCode        = VK_SUCCESS;
    const char *mErrorFile     = nullptr;
    const char *mErrorFunction = nullptr;
    unsigned int mErrorLine    = 0;
};

angle::Result LinkTaskVk::linkImpl()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramVk::LinkTaskVk::run");
    ProgramExecutableVk *executableVk = vk::GetImpl(mExecutable);

    // Link resources before calling GetShaderSource to make sure they are ready for the set/binding
    // assignment done in that function.
    linkResources();

    executableVk->clearVariableInfoMap();

    // Gather variable info and compiled SPIR-V binaries.
    executableVk->assignAllSpvLocations(this, mState, mResources);

    gl::ShaderMap<const angle::spirv::Blob *> spirvBlobs;
    SpvGetShaderSpirvCode(mState, &spirvBlobs);

    if (getFeatures().varyingsRequireMatchingPrecisionInSpirv.enabled &&
        getFeatures().enablePrecisionQualifiers.enabled)
    {
        executableVk->resolvePrecisionMismatch(mMergedVaryings);
    }

    // Compile the shaders.
    ANGLE_TRY(executableVk->initShaders(this, mExecutable->getLinkedShaderStages(), spirvBlobs,
                                        mIsGLES1));

    ANGLE_TRY(initDefaultUniformBlocks());

    ANGLE_TRY(executableVk->createPipelineLayout(this, &mPipelineLayoutCache,
                                                 &mDescriptorSetLayoutCache, nullptr));

    // Warm up the pipeline cache by creating a few placeholder pipelines.  This is not done for
    // separable programs, and is deferred to when the program pipeline is finalized.
    //
    // The cache warm up is skipped for GLES1 for two reasons:
    //
    // - Since GLES1 shaders are limited, the individual programs don't necessarily add new
    //   pipelines, but rather it's draw time state that controls that.  Since the programs are
    //   generated at draw time, it's just as well to let the pipelines be created using the
    //   renderer's shared cache.
    // - Individual GLES1 tests are long, and this adds a considerable overhead to those tests
    if (!mState.isSeparable() && !mIsGLES1)
    {
        ANGLE_TRY(executableVk->warmUpPipelineCache(
            this, mPipelineRobustness, mPipelineProtectedAccess, &mCompatibleRenderPass));
    }

    return angle::Result::Continue;
}

void LinkTaskVk::linkResources()
{
    Std140BlockLayoutEncoderFactory std140EncoderFactory;
    gl::ProgramLinkedResourcesLinker linker(&std140EncoderFactory);

    linker.linkResources(mState, mResources);
}

angle::Result LinkTaskVk::initDefaultUniformBlocks()
{
    ProgramExecutableVk *executableVk = vk::GetImpl(mExecutable);

    // Process vertex and fragment uniforms into std140 packing.
    gl::ShaderMap<sh::BlockLayoutMap> layoutMap;
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);

    generateUniformLayoutMapping(&layoutMap, &requiredBufferSize);
    initDefaultUniformLayoutMapping(&layoutMap);

    // All uniform initializations are complete, now resize the buffers accordingly and return
    return executableVk->resizeUniformBlockMemory(this, requiredBufferSize);
}

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

void LinkTaskVk::generateUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> *layoutMapOut,
                                              gl::ShaderMap<size_t> *requiredBufferSizeOut)
{
    for (const gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
    {
        const gl::SharedCompiledShaderState &shader = mState.getAttachedShader(shaderType);

        if (shader)
        {
            const std::vector<sh::ShaderVariable> &uniforms = shader->uniforms;
            InitDefaultUniformBlock(uniforms, &(*layoutMapOut)[shaderType],
                                    &(*requiredBufferSizeOut)[shaderType]);
        }
    }
}

void LinkTaskVk::initDefaultUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> *layoutMapOut)
{
    // Init the default block layout info.
    ProgramExecutableVk *executableVk = vk::GetImpl(mExecutable);
    const auto &uniforms              = mExecutable->getUniforms();

    for (const gl::VariableLocation &location : mState.getUniformLocations())
    {
        gl::ShaderMap<sh::BlockMemberInfo> layoutInfo;

        if (location.used() && !location.ignored)
        {
            const auto &uniform = uniforms[location.index];
            if (uniform.isInDefaultBlock() && !uniform.isSampler() && !uniform.isImage() &&
                !uniform.isFragmentInOut())
            {
                std::string uniformName = mExecutable->getUniformNameByIndex(location.index);
                if (uniform.isArray())
                {
                    // Gets the uniform name without the [0] at the end.
                    uniformName = gl::StripLastArrayIndex(uniformName);
                    ASSERT(uniformName.size() !=
                           mExecutable->getUniformNameByIndex(location.index).size());
                }

                bool found = false;

                for (const gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
                {
                    auto it = (*layoutMapOut)[shaderType].find(uniformName);
                    if (it != (*layoutMapOut)[shaderType].end())
                    {
                        found                  = true;
                        layoutInfo[shaderType] = it->second;
                    }
                }

                ASSERT(found);
            }
        }

        for (const gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
        {
            executableVk->getSharedDefaultUniformBlock(shaderType)
                ->uniformLayout.push_back(layoutInfo[shaderType]);
        }
    }
}

// The event for parallelized/lockless link.
class LinkEventVulkan final : public LinkEvent
{
  public:
    LinkEventVulkan(std::shared_ptr<angle::WorkerThreadPool> workerPool,
                    std::shared_ptr<LinkTaskVk> linkTask)
        : mLinkTask(linkTask),
          mWaitableEvent(
              std::shared_ptr<angle::WaitableEvent>(workerPool->postWorkerTask(mLinkTask)))
    {}

    angle::Result wait(const gl::Context *context) override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramVK::LinkEvent::wait");

        mWaitableEvent->wait();

        return mLinkTask->getResult(vk::GetImpl(context));
    }

    bool isLinking() override { return !mWaitableEvent->isReady(); }

  private:
    std::shared_ptr<LinkTaskVk> mLinkTask;
    std::shared_ptr<angle::WaitableEvent> mWaitableEvent;
};

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
}  // anonymous namespace

// ProgramVk implementation.
ProgramVk::ProgramVk(const gl::ProgramState &state) : ProgramImpl(state) {}

ProgramVk::~ProgramVk() = default;

void ProgramVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    reset(contextVk);
}

void ProgramVk::reset(ContextVk *contextVk)
{
    getExecutable()->reset(contextVk);
}

std::unique_ptr<rx::LinkEvent> ProgramVk::load(const gl::Context *context,
                                               gl::BinaryInputStream *stream)
{
    ContextVk *contextVk = vk::GetImpl(context);

    reset(contextVk);

    return getExecutable()->load(contextVk, mState.isSeparable(), stream);
}

void ProgramVk::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    ContextVk *contextVk = vk::GetImpl(context);
    getExecutable()->save(contextVk, mState.isSeparable(), stream);
}

void ProgramVk::setBinaryRetrievableHint(bool retrievable)
{
    // Nothing to do here yet.
}

void ProgramVk::setSeparable(bool separable)
{
    // Nothing to do here yet.
}

std::unique_ptr<LinkEvent> ProgramVk::link(const gl::Context *context,
                                           const gl::ProgramLinkedResources &resources,
                                           gl::ProgramMergedVaryings &&mergedVaryings)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramVk::link");

    ContextVk *contextVk = vk::GetImpl(context);
    reset(contextVk);

    std::shared_ptr<LinkTaskVk> linkTask = std::make_shared<LinkTaskVk>(
        contextVk->getRenderer(), contextVk->getPipelineLayoutCache(),
        contextVk->getDescriptorSetLayoutCache(), mState, std::move(mergedVaryings), resources,
        context->getState().isGLES1(), contextVk->pipelineRobustness(),
        contextVk->pipelineProtectedAccess());
    return std::make_unique<LinkEventVulkan>(context->getShaderCompileThreadPool(), linkTask);
}

GLboolean ProgramVk::validate(const gl::Caps &caps)
{
    // No-op. The spec is very vague about the behavior of validation.
    return GL_TRUE;
}

angle::Result ProgramVk::syncState(const gl::Context *context,
                                   const gl::Program::DirtyBits &dirtyBits)
{
    ASSERT(dirtyBits.any());
    // Push dirty bits to executable so that they can be used later.
    getExecutable()->mDirtyBits |= dirtyBits;
    return angle::Result::Continue;
}

template <typename T>
void ProgramVk::setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType)
{
    const gl::VariableLocation &locationInfo  = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform    = mState.getUniforms()[locationInfo.index];
    const gl::ProgramExecutable &glExecutable = mState.getExecutable();
    ProgramExecutableVk *executableVk         = vk::GetImpl(&glExecutable);

    ASSERT(!linkedUniform.isSampler());

    if (linkedUniform.type == entryPointType)
    {
        for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
        {
            DefaultUniformBlockVk &uniformBlock = *executableVk->mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.getElementComponents();
            UpdateDefaultUniformBlock(count, locationInfo.arrayIndex, componentCount, v, layoutInfo,
                                      &uniformBlock.uniformData);
            executableVk->mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
    else
    {
        for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
        {
            DefaultUniformBlockVk &uniformBlock = *executableVk->mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.getElementComponents();

            ASSERT(linkedUniform.type == gl::VariableBoolVectorType(entryPointType));

            GLint initialArrayOffset =
                locationInfo.arrayIndex * layoutInfo.arrayStride + layoutInfo.offset;
            for (GLint i = 0; i < count; i++)
            {
                GLint elementOffset = i * layoutInfo.arrayStride + initialArrayOffset;
                GLint *dst =
                    reinterpret_cast<GLint *>(uniformBlock.uniformData.data() + elementOffset);
                const T *source = v + i * componentCount;

                for (int c = 0; c < componentCount; c++)
                {
                    dst[c] = (source[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE;
                }
            }

            executableVk->mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
}

template <typename T>
void ProgramVk::getUniformImpl(GLint location, T *v, GLenum entryPointType) const
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];
    const ProgramExecutableVk *executableVk  = getExecutable();

    ASSERT(!linkedUniform.isSampler() && !linkedUniform.isImage());

    const gl::ShaderType shaderType = linkedUniform.getFirstActiveShaderType();
    ASSERT(shaderType != gl::ShaderType::InvalidEnum);

    const DefaultUniformBlockVk &uniformBlock = *executableVk->mDefaultUniformBlocks[shaderType];
    const sh::BlockMemberInfo &layoutInfo     = uniformBlock.uniformLayout[location];

    ASSERT(gl::GetUniformTypeInfo(linkedUniform.type).componentType == entryPointType ||
           gl::GetUniformTypeInfo(linkedUniform.type).componentType ==
               gl::VariableBoolVectorType(entryPointType));

    if (gl::IsMatrixType(linkedUniform.getType()))
    {
        const uint8_t *ptrToElement = uniformBlock.uniformData.data() + layoutInfo.offset +
                                      (locationInfo.arrayIndex * layoutInfo.arrayStride);
        GetMatrixUniform(linkedUniform.getType(), v, reinterpret_cast<const T *>(ptrToElement),
                         false);
    }
    else
    {
        ReadFromDefaultUniformBlock(linkedUniform.getElementComponents(), locationInfo.arrayIndex,
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
    const gl::ProgramExecutable &glExecutable = mState.getExecutable();
    ProgramExecutableVk *executableVk         = vk::GetImpl(&glExecutable);

    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        DefaultUniformBlockVk &uniformBlock   = *executableVk->mDefaultUniformBlocks[shaderType];
        const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

        // Assume an offset of -1 means the block is unused.
        if (layoutInfo.offset == -1)
        {
            continue;
        }

        SetFloatUniformMatrixGLSL<cols, rows>::Run(
            locationInfo.arrayIndex, linkedUniform.getBasicTypeElementCount(), count, transpose,
            value, uniformBlock.uniformData.data() + layoutInfo.offset);

        executableVk->mDefaultUniformBlocksDirty.set(shaderType);
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
}  // namespace rx
