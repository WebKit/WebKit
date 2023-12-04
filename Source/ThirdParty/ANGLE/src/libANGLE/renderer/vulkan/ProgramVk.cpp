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

class LinkTaskVk final : public vk::Context, public LinkTask
{
  public:
    LinkTaskVk(RendererVk *renderer,
               PipelineLayoutCache &pipelineLayoutCache,
               DescriptorSetLayoutCache &descriptorSetLayoutCache,
               const gl::ProgramState &state,
               bool isGLES1,
               vk::PipelineRobustness pipelineRobustness,
               vk::PipelineProtectedAccess pipelineProtectedAccess)
        : vk::Context(renderer),
          mState(state),
          mExecutable(&mState.getExecutable()),
          mIsGLES1(isGLES1),
          mPipelineRobustness(pipelineRobustness),
          mPipelineProtectedAccess(pipelineProtectedAccess),
          mPipelineLayoutCache(pipelineLayoutCache),
          mDescriptorSetLayoutCache(descriptorSetLayoutCache)
    {}
    ~LinkTaskVk() override = default;

    std::vector<std::shared_ptr<LinkSubTask>> link(const gl::ProgramLinkedResources &resources,
                                                   const gl::ProgramMergedVaryings &mergedVaryings,
                                                   bool *areSubTasksOptionalOut) override
    {
        std::vector<std::shared_ptr<LinkSubTask>> subTasks;
        angle::Result result = linkImpl(resources, mergedVaryings, &subTasks);
        ASSERT((result == angle::Result::Continue) == (mErrorCode == VK_SUCCESS));

        // In the Vulkan backend, the only subtasks are pipeline warm up, which is not required for
        // link.  Setting this flag allows the expensive warm up to be run in a thread without
        // holding up the link results.
        *areSubTasksOptionalOut = true;
        return subTasks;
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

    angle::Result getResult(const gl::Context *context, gl::InfoLog &infoLog) override
    {
        ContextVk *contextVk              = vk::GetImpl(context);
        ProgramExecutableVk *executableVk = vk::GetImpl(mExecutable);

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
    angle::Result linkImpl(const gl::ProgramLinkedResources &resources,
                           const gl::ProgramMergedVaryings &mergedVaryings,
                           std::vector<std::shared_ptr<LinkSubTask>> *subTasksOut);

    void linkResources(const gl::ProgramLinkedResources &resources);
    angle::Result initDefaultUniformBlocks();
    void generateUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> *layoutMapOut,
                                      gl::ShaderMap<size_t> *requiredBufferSizeOut);
    void initDefaultUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> *layoutMapOut);

    // The front-end ensures that the program is not accessed while linking, so it is safe to
    // direclty access the state from a potentially parallel job.
    const gl::ProgramState &mState;
    const gl::ProgramExecutable *mExecutable;
    const bool mIsGLES1;
    const vk::PipelineRobustness mPipelineRobustness;
    const vk::PipelineProtectedAccess mPipelineProtectedAccess;

    // Helpers that are interally thread-safe
    PipelineLayoutCache &mPipelineLayoutCache;
    DescriptorSetLayoutCache &mDescriptorSetLayoutCache;

    // Error handling
    VkResult mErrorCode        = VK_SUCCESS;
    const char *mErrorFile     = nullptr;
    const char *mErrorFunction = nullptr;
    unsigned int mErrorLine    = 0;
};

class WarmUpTask : public vk::Context, public LinkSubTask
{
  public:
    WarmUpTask(RendererVk *renderer,
               ProgramExecutableVk *executableVk,
               vk::PipelineRobustness pipelineRobustness,
               vk::PipelineProtectedAccess pipelineProtectedAccess)
        : vk::Context(renderer),
          mExecutableVk(executableVk),
          mPipelineRobustness(pipelineRobustness),
          mPipelineProtectedAccess(pipelineProtectedAccess)
    {}

    ~WarmUpTask() override = default;

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

    angle::Result getResult(const gl::Context *context, gl::InfoLog &infoLog) override
    {
        ContextVk *contextVk = vk::GetImpl(context);

        // Clean up garbage first, it's done no matter what may fail below.
        mCompatibleRenderPass.destroy(contextVk->getDevice());

        // Forward any errors
        if (mErrorCode != VK_SUCCESS)
        {
            contextVk->handleError(mErrorCode, mErrorFile, mErrorFunction, mErrorLine);
            return angle::Result::Stop;
        }
        return angle::Result::Continue;
    }

    void operator()() override
    {
        angle::Result result = mExecutableVk->warmUpPipelineCache(
            this, mPipelineRobustness, mPipelineProtectedAccess, &mCompatibleRenderPass);
        ASSERT((result == angle::Result::Continue) == (mErrorCode == VK_SUCCESS));
    }

  private:
    // The front-end ensures that the program is not modified while the subtask is running, so it is
    // safe to directly access the executable from this parallel job.  Note that this is the reason
    // why the front-end does not let the parallel job continue when a relink happens or the first
    // draw with this program.
    ProgramExecutableVk *mExecutableVk;
    const vk::PipelineRobustness mPipelineRobustness;
    const vk::PipelineProtectedAccess mPipelineProtectedAccess;

    // Temporary objects to clean up at the end
    vk::RenderPass mCompatibleRenderPass;

    // Error handling
    VkResult mErrorCode        = VK_SUCCESS;
    const char *mErrorFile     = nullptr;
    const char *mErrorFunction = nullptr;
    unsigned int mErrorLine    = 0;
};

angle::Result LinkTaskVk::linkImpl(const gl::ProgramLinkedResources &resources,
                                   const gl::ProgramMergedVaryings &mergedVaryings,
                                   std::vector<std::shared_ptr<LinkSubTask>> *subTasksOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "LinkTaskVk::linkImpl");
    ProgramExecutableVk *executableVk = vk::GetImpl(mExecutable);

    // Link resources before calling GetShaderSource to make sure they are ready for the set/binding
    // assignment done in that function.
    linkResources(resources);

    executableVk->clearVariableInfoMap();

    // Gather variable info and compiled SPIR-V binaries.
    executableVk->assignAllSpvLocations(this, mState, resources);

    gl::ShaderMap<const angle::spirv::Blob *> spirvBlobs;
    SpvGetShaderSpirvCode(mState, &spirvBlobs);

    if (getFeatures().varyingsRequireMatchingPrecisionInSpirv.enabled &&
        getFeatures().enablePrecisionQualifiers.enabled)
    {
        executableVk->resolvePrecisionMismatch(mergedVaryings);
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
    if (!mState.isSeparable() && !mIsGLES1 && getFeatures().warmUpPipelineCacheAtLink.enabled)
    {
        subTasksOut->push_back(std::make_shared<WarmUpTask>(
            mRenderer, executableVk, mPipelineRobustness, mPipelineProtectedAccess));
    }

    return angle::Result::Continue;
}

void LinkTaskVk::linkResources(const gl::ProgramLinkedResources &resources)
{
    Std140BlockLayoutEncoderFactory std140EncoderFactory;
    gl::ProgramLinkedResourcesLinker linker(&std140EncoderFactory);

    linker.linkResources(mState, resources);
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

    for (const gl::VariableLocation &location : mExecutable->getUniformLocations())
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
}  // anonymous namespace

// ProgramVk implementation.
ProgramVk::ProgramVk(const gl::ProgramState &state) : ProgramImpl(state) {}

ProgramVk::~ProgramVk() = default;

void ProgramVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    getExecutable()->reset(contextVk);
}

angle::Result ProgramVk::load(const gl::Context *context,
                              gl::BinaryInputStream *stream,
                              std::shared_ptr<LinkTask> *loadTaskOut,
                              bool *successOut)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // TODO: parallelize program load.  http://anglebug.com/8297
    *loadTaskOut = {};

    return getExecutable()->load(contextVk, mState.isSeparable(), stream, successOut);
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

angle::Result ProgramVk::link(const gl::Context *context, std::shared_ptr<LinkTask> *linkTaskOut)
{
    ContextVk *contextVk = vk::GetImpl(context);

    *linkTaskOut = std::shared_ptr<LinkTask>(new LinkTaskVk(
        contextVk->getRenderer(), contextVk->getPipelineLayoutCache(),
        contextVk->getDescriptorSetLayoutCache(), mState, context->getState().isGLES1(),
        contextVk->pipelineRobustness(), contextVk->pipelineProtectedAccess()));

    return angle::Result::Continue;
}

GLboolean ProgramVk::validate(const gl::Caps &caps)
{
    // No-op. The spec is very vague about the behavior of validation.
    return GL_TRUE;
}

}  // namespace rx
