//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextVk.cpp:
//    Implements the class methods for ContextVk.
//

#include "libANGLE/renderer/vulkan/ContextVk.h"

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/Semaphore.h"
#include "libANGLE/Surface.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/CompilerVk.h"
#include "libANGLE/renderer/vulkan/FenceNVVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/MemoryObjectVk.h"
#include "libANGLE/renderer/vulkan/OverlayVk.h"
#include "libANGLE/renderer/vulkan/ProgramPipelineVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/QueryVk.h"
#include "libANGLE/renderer/vulkan/RenderbufferVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SamplerVk.h"
#include "libANGLE/renderer/vulkan/SemaphoreVk.h"
#include "libANGLE/renderer/vulkan/ShaderVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"
#include "libANGLE/renderer/vulkan/SyncVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"
#include "libANGLE/renderer/vulkan/VertexArrayVk.h"

#include "libANGLE/trace.h"

#include <iostream>

namespace rx
{

namespace
{
// For shader uniforms such as gl_DepthRange and the viewport size.
struct GraphicsDriverUniforms
{
    std::array<float, 4> viewport;

    float halfRenderAreaHeight;
    float viewportYScale;
    float negViewportYScale;

    // 32 bits for 32 clip planes
    uint32_t enabledClipPlanes;

    uint32_t xfbActiveUnpaused;
    uint32_t xfbVerticesPerDraw;
    // NOTE: Explicit padding. Fill in with useful data when needed in the future.
    std::array<int32_t, 2> padding;

    std::array<int32_t, 4> xfbBufferOffsets;

    // .xy contain packed 8-bit values for atomic counter buffer offsets.  These offsets are
    // within Vulkan's minStorageBufferOffsetAlignment limit and are used to support unaligned
    // offsets allowed in GL.
    //
    // .zw are unused.
    std::array<uint32_t, 4> acbBufferOffsets;

    // We'll use x, y, z for near / far / diff respectively.
    std::array<float, 4> depthRange;

    // Used to pre-rotate gl_Position for swapchain images on Android (a mat2, which is padded to
    // the size of two vec4's).
    std::array<float, 8> preRotation;
};

struct ComputeDriverUniforms
{
    // Atomic counter buffer offsets with the same layout as in GraphicsDriverUniforms.
    std::array<uint32_t, 4> acbBufferOffsets;
};

GLenum DefaultGLErrorCode(VkResult result)
{
    switch (result)
    {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        case VK_ERROR_TOO_MANY_OBJECTS:
            return GL_OUT_OF_MEMORY;
        default:
            return GL_INVALID_OPERATION;
    }
}

constexpr gl::ShaderMap<vk::ImageLayout> kShaderReadOnlyImageLayouts = {
    {gl::ShaderType::Vertex, vk::ImageLayout::VertexShaderReadOnly},
    {gl::ShaderType::Fragment, vk::ImageLayout::FragmentShaderReadOnly},
    {gl::ShaderType::Geometry, vk::ImageLayout::GeometryShaderReadOnly},
    {gl::ShaderType::Compute, vk::ImageLayout::ComputeShaderReadOnly}};

constexpr gl::ShaderMap<vk::ImageLayout> kShaderWriteImageLayouts = {
    {gl::ShaderType::Vertex, vk::ImageLayout::VertexShaderWrite},
    {gl::ShaderType::Fragment, vk::ImageLayout::FragmentShaderWrite},
    {gl::ShaderType::Geometry, vk::ImageLayout::GeometryShaderWrite},
    {gl::ShaderType::Compute, vk::ImageLayout::ComputeShaderWrite}};

constexpr VkColorComponentFlags kAllColorChannelsMask =
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
     VK_COLOR_COMPONENT_A_BIT);

constexpr VkBufferUsageFlags kVertexBufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
constexpr size_t kDefaultValueSize              = sizeof(gl::VertexAttribCurrentValueData::Values);
constexpr size_t kDefaultBufferSize             = kDefaultValueSize * 16;
constexpr size_t kDefaultPoolAllocatorPageSize  = 16 * 1024;
constexpr size_t kDriverUniformsAllocatorPageSize = 4 * 1024;

constexpr size_t kInFlightCommandsLimit = 100u;

void InitializeSubmitInfo(VkSubmitInfo *submitInfo,
                          const vk::PrimaryCommandBuffer &commandBuffer,
                          const std::vector<VkSemaphore> &waitSemaphores,
                          std::vector<VkPipelineStageFlags> *waitSemaphoreStageMasks,
                          const vk::Semaphore *signalSemaphore)
{
    // Verify that the submitInfo has been zero'd out.
    ASSERT(submitInfo->signalSemaphoreCount == 0);

    submitInfo->sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo->commandBufferCount = commandBuffer.valid() ? 1 : 0;
    submitInfo->pCommandBuffers    = commandBuffer.ptr();

    if (waitSemaphoreStageMasks->size() < waitSemaphores.size())
    {
        waitSemaphoreStageMasks->resize(waitSemaphores.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }

    submitInfo->waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo->pWaitSemaphores    = waitSemaphores.data();
    submitInfo->pWaitDstStageMask  = waitSemaphoreStageMasks->data();

    if (signalSemaphore)
    {
        submitInfo->signalSemaphoreCount = 1;
        submitInfo->pSignalSemaphores    = signalSemaphore->ptr();
    }
}

uint32_t GetCoverageSampleCount(const gl::State &glState, FramebufferVk *drawFramebuffer)
{
    if (!glState.isSampleCoverageEnabled())
    {
        return 0;
    }

    // Get a fraction of the samples based on the coverage parameters.
    return static_cast<uint32_t>(
        std::round(glState.getSampleCoverageValue() * drawFramebuffer->getSamples()));
}

void ApplySampleCoverage(const gl::State &glState,
                         uint32_t coverageSampleCount,
                         uint32_t maskNumber,
                         uint32_t *maskOut)
{
    if (!glState.isSampleCoverageEnabled())
    {
        return;
    }

    uint32_t maskBitOffset = maskNumber * 32;
    uint32_t coverageMask  = coverageSampleCount >= (maskBitOffset + 32)
                                ? std::numeric_limits<uint32_t>::max()
                                : (1u << (coverageSampleCount - maskBitOffset)) - 1;

    if (glState.getSampleCoverageInvert())
    {
        coverageMask = ~coverageMask;
    }

    *maskOut &= coverageMask;
}

// When an Android surface is rotated differently than the device's native orientation, ANGLE must
// rotate gl_Position in the vertex shader.  The following are the rotation matrices used.
//
// Note: these are mat2's that are appropriately padded (4 floats per row).
using PreRotationMatrixValues = std::array<float, 8>;
constexpr angle::PackedEnumMap<rx::SurfaceRotation,
                               PreRotationMatrixValues,
                               angle::EnumSize<rx::SurfaceRotation>()>
    kPreRotationMatrices = {
        {{rx::SurfaceRotation::Identity, {{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::Rotated90Degrees,
          {{0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::Rotated180Degrees,
          {{-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::Rotated270Degrees,
          {{0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::FlippedIdentity,
          {{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::FlippedRotated90Degrees,
          {{0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::FlippedRotated180Degrees,
          {{-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::FlippedRotated270Degrees,
          {{0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f}}}}};

bool IsRotatedAspectRatio(SurfaceRotation rotation)
{
    return ((rotation == SurfaceRotation::Rotated90Degrees) ||
            (rotation == SurfaceRotation::Rotated270Degrees) ||
            (rotation == SurfaceRotation::FlippedRotated90Degrees) ||
            (rotation == SurfaceRotation::FlippedRotated270Degrees));
}

SurfaceRotation DetermineSurfaceRotation(gl::Framebuffer *framebuffer,
                                         WindowSurfaceVk *windowSurface)
{
    if (windowSurface && framebuffer->isDefault())
    {
        switch (windowSurface->getPreTransform())
        {
            case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:
                // Do not rotate gl_Position (surface matches the device's orientation):
                return SurfaceRotation::Identity;
            case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
                // Rotate gl_Position 90 degrees:
                return SurfaceRotation::Rotated90Degrees;
            case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
                // Rotate gl_Position 180 degrees:
                return SurfaceRotation::Rotated180Degrees;
            case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
                // Rotate gl_Position 270 degrees:
                return SurfaceRotation::Rotated270Degrees;
            default:
                UNREACHABLE();
                return SurfaceRotation::Identity;
        }
    }
    else
    {
        // Do not rotate gl_Position (offscreen framebuffer):
        return SurfaceRotation::Identity;
    }
}

void RotateRectangle(const SurfaceRotation rotation,
                     const bool flipY,
                     const int framebufferWidth,
                     const int framebufferHeight,
                     const gl::Rectangle &incoming,
                     gl::Rectangle *outgoing)
{
    // GLES's y-axis points up; Vulkan's points down.
    switch (rotation)
    {
        case SurfaceRotation::Identity:
            // Do not rotate gl_Position (surface matches the device's orientation):
            outgoing->x     = incoming.x;
            outgoing->y     = flipY ? framebufferHeight - incoming.y - incoming.height : incoming.y;
            outgoing->width = incoming.width;
            outgoing->height = incoming.height;
            break;
        case SurfaceRotation::Rotated90Degrees:
            // Rotate gl_Position 90 degrees:
            outgoing->x      = incoming.y;
            outgoing->y      = flipY ? incoming.x : framebufferWidth - incoming.x - incoming.width;
            outgoing->width  = incoming.height;
            outgoing->height = incoming.width;
            break;
        case SurfaceRotation::Rotated180Degrees:
            // Rotate gl_Position 180 degrees:
            outgoing->x     = framebufferWidth - incoming.x - incoming.width;
            outgoing->y     = flipY ? incoming.y : framebufferHeight - incoming.y - incoming.height;
            outgoing->width = incoming.width;
            outgoing->height = incoming.height;
            break;
        case SurfaceRotation::Rotated270Degrees:
            // Rotate gl_Position 270 degrees:
            outgoing->x      = framebufferHeight - incoming.y - incoming.height;
            outgoing->y      = flipY ? framebufferWidth - incoming.x - incoming.width : incoming.x;
            outgoing->width  = incoming.height;
            outgoing->height = incoming.width;
            break;
        default:
            UNREACHABLE();
            break;
    }
}

// Should not generate a copy with modern C++.
EventName GetTraceEventName(const char *title, uint32_t counter)
{
    EventName buf;
    snprintf(buf.data(), kMaxGpuEventNameLen - 1, "%s %u", title, counter);
    return buf;
}
}  // anonymous namespace

ContextVk::DriverUniformsDescriptorSet::DriverUniformsDescriptorSet()
    : descriptorSet(VK_NULL_HANDLE), dynamicOffset(0)
{}

ContextVk::DriverUniformsDescriptorSet::~DriverUniformsDescriptorSet() = default;

void ContextVk::DriverUniformsDescriptorSet::init(RendererVk *rendererVk)
{
    size_t minAlignment = static_cast<size_t>(
        rendererVk->getPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment);
    dynamicBuffer.init(rendererVk, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, minAlignment,
                       kDriverUniformsAllocatorPageSize, true);
}

void ContextVk::DriverUniformsDescriptorSet::destroy(RendererVk *renderer)
{
    descriptorSetLayout.reset();
    descriptorPoolBinding.reset();
    dynamicBuffer.destroy(renderer);
}

// CommandBatch implementation.
CommandBatch::CommandBatch() = default;

CommandBatch::~CommandBatch() = default;

CommandBatch::CommandBatch(CommandBatch &&other)
{
    *this = std::move(other);
}

CommandBatch &CommandBatch::operator=(CommandBatch &&other)
{
    std::swap(primaryCommands, other.primaryCommands);
    std::swap(commandPool, other.commandPool);
    std::swap(fence, other.fence);
    std::swap(serial, other.serial);
    return *this;
}

void CommandBatch::destroy(VkDevice device)
{
    primaryCommands.destroy(device);
    commandPool.destroy(device);
    fence.reset(device);
}

// CommandQueue implementation.
CommandQueue::CommandQueue()  = default;
CommandQueue::~CommandQueue() = default;

void CommandQueue::destroy(VkDevice device)
{
    mPrimaryCommandPool.destroy(device);
    ASSERT(mInFlightCommands.empty() && mGarbageQueue.empty());
}

angle::Result CommandQueue::init(vk::Context *context)
{
    RendererVk *renderer = context->getRenderer();

    // Initialize the command pool now that we know the queue family index.
    uint32_t queueFamilyIndex = renderer->getQueueFamilyIndex();
    ANGLE_TRY(mPrimaryCommandPool.init(context, queueFamilyIndex));

    return angle::Result::Continue;
}

angle::Result CommandQueue::checkCompletedCommands(vk::Context *context)
{
    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    int finishedCount = 0;

    for (CommandBatch &batch : mInFlightCommands)
    {
        VkResult result = batch.fence.get().getStatus(device);
        if (result == VK_NOT_READY)
        {
            break;
        }
        ANGLE_VK_TRY(context, result);

        renderer->onCompletedSerial(batch.serial);

        renderer->resetSharedFence(&batch.fence);
        ANGLE_TRACE_EVENT0("gpu.angle", "command buffer recycling");
        batch.commandPool.destroy(device);
        ANGLE_TRY(releasePrimaryCommandBuffer(context, std::move(batch.primaryCommands)));
        ++finishedCount;
    }

    if (finishedCount > 0)
    {
        auto beginIter = mInFlightCommands.begin();
        mInFlightCommands.erase(beginIter, beginIter + finishedCount);
    }

    Serial lastCompleted = renderer->getLastCompletedQueueSerial();

    size_t freeIndex = 0;
    for (; freeIndex < mGarbageQueue.size(); ++freeIndex)
    {
        vk::GarbageAndSerial &garbageList = mGarbageQueue[freeIndex];
        if (garbageList.getSerial() < lastCompleted)
        {
            for (vk::GarbageObject &garbage : garbageList.get())
            {
                garbage.destroy(renderer);
            }
        }
        else
        {
            break;
        }
    }

    // Remove the entries from the garbage list - they should be ready to go.
    if (freeIndex > 0)
    {
        mGarbageQueue.erase(mGarbageQueue.begin(), mGarbageQueue.begin() + freeIndex);
    }

    return angle::Result::Continue;
}

angle::Result CommandQueue::releaseToCommandBatch(vk::Context *context,
                                                  vk::PrimaryCommandBuffer &&commandBuffer,
                                                  vk::CommandPool *commandPool,
                                                  CommandBatch *batch)
{
    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    batch->primaryCommands = std::move(commandBuffer);

    if (commandPool->valid())
    {
        batch->commandPool = std::move(*commandPool);
        // Recreate CommandPool
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex        = renderer->getQueueFamilyIndex();

        ANGLE_VK_TRY(context, commandPool->init(device, poolInfo));
    }

    return angle::Result::Continue;
}

void CommandQueue::clearAllGarbage(RendererVk *renderer)
{
    for (vk::GarbageAndSerial &garbageList : mGarbageQueue)
    {
        for (vk::GarbageObject &garbage : garbageList.get())
        {
            garbage.destroy(renderer);
        }
    }
    mGarbageQueue.clear();
}

angle::Result CommandQueue::allocatePrimaryCommandBuffer(vk::Context *context,
                                                         const vk::CommandPool &commandPool,
                                                         vk::PrimaryCommandBuffer *commandBufferOut)
{
    return mPrimaryCommandPool.allocate(context, commandBufferOut);
}

angle::Result CommandQueue::releasePrimaryCommandBuffer(vk::Context *context,
                                                        vk::PrimaryCommandBuffer &&commandBuffer)
{
    ASSERT(mPrimaryCommandPool.valid());
    ANGLE_TRY(mPrimaryCommandPool.collect(context, std::move(commandBuffer)));

    return angle::Result::Continue;
}

void CommandQueue::handleDeviceLost(RendererVk *renderer)
{
    VkDevice device = renderer->getDevice();

    for (CommandBatch &batch : mInFlightCommands)
    {
        // On device loss we need to wait for fence to be signaled before destroying it
        VkResult status = batch.fence.get().wait(device, renderer->getMaxFenceWaitTimeNs());
        // If the wait times out, it is probably not possible to recover from lost device
        ASSERT(status == VK_SUCCESS || status == VK_ERROR_DEVICE_LOST);

        // On device lost, here simply destroy the CommandBuffer, it will fully cleared later
        // by CommandPool::destroy
        batch.primaryCommands.destroy(device);

        batch.commandPool.destroy(device);
        batch.fence.reset(device);
    }
    mInFlightCommands.clear();
}

bool CommandQueue::hasInFlightCommands() const
{
    return !mInFlightCommands.empty();
}

angle::Result CommandQueue::finishToSerial(vk::Context *context, Serial serial, uint64_t timeout)
{
    if (mInFlightCommands.empty())
    {
        return angle::Result::Continue;
    }
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::finishToSerial");
    // Find the first batch with serial equal to or bigger than given serial (note that
    // the batch serials are unique, otherwise upper-bound would have been necessary).
    //
    // Note: we don't check for the exact serial, because it may belong to another context.  For
    // example, imagine the following submissions:
    //
    // - Context 1: Serial 1, Serial 3, Serial 5
    // - Context 2: Serial 2, Serial 4, Serial 6
    //
    // And imagine none of the submissions have finished yet.  Now if Context 2 asks for
    // finishToSerial(3), it will have no choice but to finish until Serial 4 instead.
    size_t batchIndex = mInFlightCommands.size() - 1;
    for (size_t i = 0; i < mInFlightCommands.size(); ++i)
    {
        if (mInFlightCommands[i].serial >= serial)
        {
            batchIndex = i;
            break;
        }
    }
    const CommandBatch &batch = mInFlightCommands[batchIndex];

    // Wait for it finish
    VkDevice device = context->getDevice();
    VkResult status = batch.fence.get().wait(device, timeout);

    ANGLE_VK_TRY(context, status);

    // Clean up finished batches.
    return checkCompletedCommands(context);
}

angle::Result CommandQueue::submitFrame(vk::Context *context,
                                        egl::ContextPriority priority,
                                        const VkSubmitInfo &submitInfo,
                                        const vk::Shared<vk::Fence> &sharedFence,
                                        vk::GarbageList *currentGarbage,
                                        vk::CommandPool *commandPool,
                                        vk::PrimaryCommandBuffer &&commandBuffer)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::submitFrame");
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = 0;

    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    vk::DeviceScoped<CommandBatch> scopedBatch(device);
    CommandBatch &batch = scopedBatch.get();
    batch.fence.copy(device, sharedFence);

    ANGLE_TRY(
        renderer->queueSubmit(context, priority, submitInfo, &batch.fence.get(), &batch.serial));

    if (!currentGarbage->empty())
    {
        mGarbageQueue.emplace_back(std::move(*currentGarbage), batch.serial);
    }

    // Store the primary CommandBuffer and command pool used for secondary CommandBuffers
    // in the in-flight list.
    ANGLE_TRY(releaseToCommandBatch(context, std::move(commandBuffer), commandPool, &batch));

    mInFlightCommands.emplace_back(scopedBatch.release());

    ANGLE_TRY(checkCompletedCommands(context));

    // CPU should be throttled to avoid mInFlightCommands from growing too fast. Important for
    // off-screen scenarios.
    while (mInFlightCommands.size() > kInFlightCommandsLimit)
    {
        ANGLE_TRY(finishToSerial(context, mInFlightCommands[0].serial,
                                 renderer->getMaxFenceWaitTimeNs()));
    }

    return angle::Result::Continue;
}

vk::Shared<vk::Fence> CommandQueue::getLastSubmittedFence(const vk::Context *context) const
{
    vk::Shared<vk::Fence> fence;
    if (!mInFlightCommands.empty())
    {
        fence.copy(context->getDevice(), mInFlightCommands.back().fence);
    }

    return fence;
}

egl::ContextPriority GetContextPriority(const gl::State &state)
{
    return egl::FromEGLenum<egl::ContextPriority>(state.getContextPriority());
}

// ContextVk implementation.
ContextVk::ContextVk(const gl::State &state, gl::ErrorSet *errorSet, RendererVk *renderer)
    : ContextImpl(state, errorSet),
      vk::Context(renderer),
      mRenderPassCommandBuffer(nullptr),
      mCurrentGraphicsPipeline(nullptr),
      mCurrentComputePipeline(nullptr),
      mCurrentDrawMode(gl::PrimitiveMode::InvalidEnum),
      mCurrentWindowSurface(nullptr),
      mCurrentRotationDrawFramebuffer(SurfaceRotation::Identity),
      mCurrentRotationReadFramebuffer(SurfaceRotation::Identity),
      mVertexArray(nullptr),
      mDrawFramebuffer(nullptr),
      mProgram(nullptr),
      mExecutable(nullptr),
      mActiveQueryAnySamples(nullptr),
      mActiveQueryAnySamplesConservative(nullptr),
      mLastIndexBufferOffset(0),
      mCurrentDrawElementsType(gl::DrawElementsType::InvalidEnum),
      mXfbBaseVertex(0),
      mXfbVertexCountPerInstance(0),
      mClearColorMask(kAllColorChannelsMask),
      mFlipYForCurrentSurface(false),
      mIsAnyHostVisibleBufferWritten(false),
      mEmulateSeamfulCubeMapSampling(false),
      mUseOldRewriteStructSamplers(false),
      mPoolAllocator(kDefaultPoolAllocatorPageSize, 1),
      mHasPrimaryCommands(false),
      mGpuEventsEnabled(false),
      mGpuClockSync{std::numeric_limits<double>::max(), std::numeric_limits<double>::max()},
      mGpuEventTimestampOrigin(0),
      mPrimaryBufferCounter(0),
      mRenderPassCounter(0),
      mContextPriority(renderer->getDriverPriority(GetContextPriority(state)))
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::ContextVk");
    memset(&mClearColorValue, 0, sizeof(mClearColorValue));
    memset(&mClearDepthStencilValue, 0, sizeof(mClearDepthStencilValue));

    mNonIndexedDirtyBitsMask.set();
    mNonIndexedDirtyBitsMask.reset(DIRTY_BIT_INDEX_BUFFER);

    mIndexedDirtyBitsMask.set();

    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_PIPELINE);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_TEXTURES);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);
    if (getFeatures().supportsTransformFeedbackExtension.enabled ||
        getFeatures().emulateTransformFeedback.enabled)
    {
        mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS);
    }
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_STATE);
        mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME);
    }
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS_BINDING);

    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_PIPELINE);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_TEXTURES);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS_BINDING);

    mNewGraphicsPipelineDirtyBits.set(DIRTY_BIT_PIPELINE);
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mNewGraphicsPipelineDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME);
    }

    mGraphicsDirtyBitHandlers[DIRTY_BIT_DEFAULT_ATTRIBS] =
        &ContextVk::handleDirtyGraphicsDefaultAttribs;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_PIPELINE] = &ContextVk::handleDirtyGraphicsPipeline;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_TEXTURES] = &ContextVk::handleDirtyGraphicsTextures;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_VERTEX_BUFFERS] =
        &ContextVk::handleDirtyGraphicsVertexBuffers;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_INDEX_BUFFER] = &ContextVk::handleDirtyGraphicsIndexBuffer;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DRIVER_UNIFORMS] =
        &ContextVk::handleDirtyGraphicsDriverUniforms;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DRIVER_UNIFORMS_BINDING] =
        &ContextVk::handleDirtyGraphicsDriverUniformsBinding;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_SHADER_RESOURCES] =
        &ContextVk::handleDirtyGraphicsShaderResources;
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mGraphicsDirtyBitHandlers[DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS] =
            &ContextVk::handleDirtyGraphicsTransformFeedbackBuffersExtension;
        mGraphicsDirtyBitHandlers[DIRTY_BIT_TRANSFORM_FEEDBACK_STATE] =
            &ContextVk::handleDirtyGraphicsTransformFeedbackState;
        mGraphicsDirtyBitHandlers[DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME] =
            &ContextVk::handleDirtyGraphicsTransformFeedbackResume;
    }
    else if (getFeatures().emulateTransformFeedback.enabled)
    {
        mGraphicsDirtyBitHandlers[DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS] =
            &ContextVk::handleDirtyGraphicsTransformFeedbackBuffersEmulation;
    }

    mGraphicsDirtyBitHandlers[DIRTY_BIT_DESCRIPTOR_SETS] = &ContextVk::handleDirtyDescriptorSets;

    mComputeDirtyBitHandlers[DIRTY_BIT_PIPELINE] = &ContextVk::handleDirtyComputePipeline;
    mComputeDirtyBitHandlers[DIRTY_BIT_TEXTURES] = &ContextVk::handleDirtyComputeTextures;
    mComputeDirtyBitHandlers[DIRTY_BIT_DRIVER_UNIFORMS] =
        &ContextVk::handleDirtyComputeDriverUniforms;
    mComputeDirtyBitHandlers[DIRTY_BIT_DRIVER_UNIFORMS_BINDING] =
        &ContextVk::handleDirtyComputeDriverUniformsBinding;
    mComputeDirtyBitHandlers[DIRTY_BIT_SHADER_RESOURCES] =
        &ContextVk::handleDirtyComputeShaderResources;
    mComputeDirtyBitHandlers[DIRTY_BIT_DESCRIPTOR_SETS] = &ContextVk::handleDirtyDescriptorSets;

    mGraphicsDirtyBits = mNewGraphicsCommandBufferDirtyBits;
    mComputeDirtyBits  = mNewComputeCommandBufferDirtyBits;

    mActiveTextures.fill({nullptr, nullptr});
    mActiveImages.fill(nullptr);

    mPipelineDirtyBitsMask.set();
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_TEXTURE_BINDINGS);
}

ContextVk::~ContextVk() = default;

void ContextVk::onDestroy(const gl::Context *context)
{
    // This will not destroy any resources. It will release them to be collected after finish.
    mIncompleteTextures.onDestroy(context);

    // Flush and complete current outstanding work before destruction.
    (void)finishImpl();

    VkDevice device = getDevice();

    for (DriverUniformsDescriptorSet &driverUniforms : mDriverUniforms)
    {
        driverUniforms.destroy(mRenderer);
    }

    mDriverUniformsDescriptorPool.destroy(device);

    for (vk::DynamicBuffer &defaultBuffer : mDefaultAttribBuffers)
    {
        defaultBuffer.destroy(mRenderer);
    }

    for (vk::DynamicQueryPool &queryPool : mQueryPools)
    {
        queryPool.destroy(device);
    }

    ASSERT(mCurrentGarbage.empty());

    mCommandQueue.destroy(device);

    mResourceUseList.releaseResourceUses();

    mUtils.destroy(device);

    mRenderPassCache.destroy(device);
    mSubmitFence.reset(device);
    mShaderLibrary.destroy(device);
    mGpuEventQueryPool.destroy(device);
    mCommandPool.destroy(device);
    mPrimaryCommands.destroy(device);
}

angle::Result ContextVk::getIncompleteTexture(const gl::Context *context,
                                              gl::TextureType type,
                                              gl::Texture **textureOut)
{
    // At some point, we'll need to support multisample and we'll pass "this" instead of nullptr
    // and implement the necessary interface.
    return mIncompleteTextures.getIncompleteTexture(context, type, nullptr, textureOut);
}

angle::Result ContextVk::initialize()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::initialize");

    VkDescriptorPoolSize driverSetSize = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1};
    ANGLE_TRY(mDriverUniformsDescriptorPool.init(this, &driverSetSize, 1));

    ANGLE_TRY(mQueryPools[gl::QueryType::AnySamples].init(this, VK_QUERY_TYPE_OCCLUSION,
                                                          vk::kDefaultOcclusionQueryPoolSize));
    ANGLE_TRY(mQueryPools[gl::QueryType::AnySamplesConservative].init(
        this, VK_QUERY_TYPE_OCCLUSION, vk::kDefaultOcclusionQueryPoolSize));

    // Only initialize the timestamp query pools if the extension is available.
    if (mRenderer->getQueueFamilyProperties().timestampValidBits > 0)
    {
        ANGLE_TRY(mQueryPools[gl::QueryType::Timestamp].init(this, VK_QUERY_TYPE_TIMESTAMP,
                                                             vk::kDefaultTimestampQueryPoolSize));
        ANGLE_TRY(mQueryPools[gl::QueryType::TimeElapsed].init(this, VK_QUERY_TYPE_TIMESTAMP,
                                                               vk::kDefaultTimestampQueryPoolSize));
    }

    // Init gles to vulkan index type map
    initIndexTypeMap();

    // Init driver uniforms and get the descriptor set layouts.
    constexpr angle::PackedEnumMap<PipelineType, VkShaderStageFlags> kPipelineStages = {
        {PipelineType::Graphics, VK_SHADER_STAGE_ALL_GRAPHICS},
        {PipelineType::Compute, VK_SHADER_STAGE_COMPUTE_BIT},
    };
    for (PipelineType pipeline : angle::AllEnums<PipelineType>())
    {
        mDriverUniforms[pipeline].init(mRenderer);

        vk::DescriptorSetLayoutDesc desc =
            getDriverUniformsDescriptorSetDesc(kPipelineStages[pipeline]);
        ANGLE_TRY(mRenderer->getDescriptorSetLayout(
            this, desc, &mDriverUniforms[pipeline].descriptorSetLayout));
    }

    mGraphicsPipelineDesc.reset(new vk::GraphicsPipelineDesc());
    mGraphicsPipelineDesc->initDefaults();

    // Initialize current value/default attribute buffers.
    for (vk::DynamicBuffer &buffer : mDefaultAttribBuffers)
    {
        buffer.init(mRenderer, kVertexBufferUsage, 1, kDefaultBufferSize, true);
    }

    ANGLE_TRY(mCommandQueue.init(this));

#if ANGLE_ENABLE_VULKAN_GPU_TRACE_EVENTS
    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    // GPU tracing workaround for anglebug.com/2927.  The renderer should not emit gpu events
    // during platform discovery.
    const unsigned char *gpuEventsEnabled =
        platform->getTraceCategoryEnabledFlag(platform, "gpu.angle.gpu");
    mGpuEventsEnabled = gpuEventsEnabled && *gpuEventsEnabled;
#endif

    mEmulateSeamfulCubeMapSampling = shouldEmulateSeamfulCubeMapSampling();

    mUseOldRewriteStructSamplers = shouldUseOldRewriteStructSamplers();

    // Push a scope in the pool allocator so we can easily reinitialize on flush.
    mPoolAllocator.push();
    mOutsideRenderPassCommands = &mCommandBuffers[0];
    mRenderPassCommands        = &mCommandBuffers[1];

    mOutsideRenderPassCommands->initialize(
        &mPoolAllocator, false, mRenderer->getFeatures().preferAggregateBarrierCalls.enabled);
    mRenderPassCommands->initialize(&mPoolAllocator, true,
                                    mRenderer->getFeatures().preferAggregateBarrierCalls.enabled);
    ANGLE_TRY(startPrimaryCommandBuffer());

    if (mGpuEventsEnabled)
    {
        // GPU events should only be available if timestamp queries are available.
        ASSERT(mRenderer->getQueueFamilyProperties().timestampValidBits > 0);
        // Calculate the difference between CPU and GPU clocks for GPU event reporting.
        ANGLE_TRY(mGpuEventQueryPool.init(this, VK_QUERY_TYPE_TIMESTAMP,
                                          vk::kDefaultTimestampQueryPoolSize));
        ANGLE_TRY(synchronizeCpuGpuTime());

        mPrimaryBufferCounter++;

        EventName eventName = GetTraceEventName("Primary", mPrimaryBufferCounter);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_BEGIN, eventName));
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::startPrimaryCommandBuffer()
{
    ANGLE_TRY(mCommandQueue.allocatePrimaryCommandBuffer(this, mCommandPool, &mPrimaryCommands));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = nullptr;
    ANGLE_VK_TRY(this, mPrimaryCommands.begin(beginInfo));

    mHasPrimaryCommands = false;
    return angle::Result::Continue;
}

angle::Result ContextVk::flush(const gl::Context *context)
{
    return flushImpl(nullptr);
}

angle::Result ContextVk::finish(const gl::Context *context)
{
    return finishImpl();
}

angle::Result ContextVk::setupDraw(const gl::Context *context,
                                   gl::PrimitiveMode mode,
                                   GLint firstVertexOrInvalid,
                                   GLsizei vertexOrIndexCount,
                                   GLsizei instanceCount,
                                   gl::DrawElementsType indexTypeOrInvalid,
                                   const void *indices,
                                   DirtyBits dirtyBitMask,
                                   vk::CommandBuffer **commandBufferOut)
{
    // Set any dirty bits that depend on draw call parameters or other objects.
    if (mode != mCurrentDrawMode)
    {
        invalidateCurrentGraphicsPipeline();
        mCurrentDrawMode = mode;
        mGraphicsPipelineDesc->updateTopology(&mGraphicsPipelineTransition, mCurrentDrawMode);
    }

    // Must be called before the command buffer is started. Can call finish.
    if (mVertexArray->getStreamingVertexAttribsMask().any())
    {
        // All client attribs & any emulated buffered attribs will be updated
        ANGLE_TRY(mVertexArray->updateStreamedAttribs(context, firstVertexOrInvalid,
                                                      vertexOrIndexCount, instanceCount,
                                                      indexTypeOrInvalid, indices));

        mGraphicsDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
    }

    // This could be improved using a dirty bit. But currently it's slower to use a handler
    // function than an inlined if. We should probably replace the dirty bit dispatch table
    // with a switch with inlined handler functions.
    // TODO(jmadill): Use dirty bit. http://anglebug.com/3014
    if (!mRenderPassCommandBuffer)
    {
        gl::Rectangle scissoredRenderArea = mDrawFramebuffer->getScissoredRenderArea(this);
        ANGLE_TRY(startRenderPass(scissoredRenderArea, nullptr));
    }

    // We keep a local copy of the command buffer. It's possible that some state changes could
    // trigger a command buffer invalidation. The local copy ensures we retain the reference.
    // Command buffers are pool allocated and only deleted after submit. Thus we know the
    // command buffer will still be valid for the duration of this API call.
    *commandBufferOut = mRenderPassCommandBuffer;
    ASSERT(*commandBufferOut);

    if (mProgram && mProgram->dirtyUniforms())
    {
        ANGLE_TRY(mProgram->updateUniforms(this));
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }
    else if (mProgramPipeline && mProgramPipeline->dirtyUniforms(getState()))
    {
        ANGLE_TRY(mProgramPipeline->updateUniforms(this));
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }

    // Update transform feedback offsets on every draw call.
    if (mState.isTransformFeedbackActiveUnpaused())
    {
        ASSERT(firstVertexOrInvalid != -1);
        mXfbBaseVertex             = firstVertexOrInvalid;
        mXfbVertexCountPerInstance = vertexOrIndexCount;
        invalidateGraphicsDriverUniforms();
    }

    DirtyBits dirtyBits = mGraphicsDirtyBits & dirtyBitMask;

    if (dirtyBits.none())
        return angle::Result::Continue;

    // Flush any relevant dirty bits.
    for (size_t dirtyBit : dirtyBits)
    {
        ASSERT(mGraphicsDirtyBitHandlers[dirtyBit]);
        ANGLE_TRY((this->*mGraphicsDirtyBitHandlers[dirtyBit])(context, *commandBufferOut));
    }

    mGraphicsDirtyBits &= ~dirtyBitMask;

    return angle::Result::Continue;
}

angle::Result ContextVk::setupIndexedDraw(const gl::Context *context,
                                          gl::PrimitiveMode mode,
                                          GLsizei indexCount,
                                          GLsizei instanceCount,
                                          gl::DrawElementsType indexType,
                                          const void *indices,
                                          vk::CommandBuffer **commandBufferOut)
{
    ASSERT(mode != gl::PrimitiveMode::LineLoop);

    if (indexType != mCurrentDrawElementsType)
    {
        mCurrentDrawElementsType = indexType;
        setIndexBufferDirty();
    }

    const gl::Buffer *elementArrayBuffer = mVertexArray->getState().getElementArrayBuffer();
    if (!elementArrayBuffer)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
        ANGLE_TRY(mVertexArray->convertIndexBufferCPU(this, indexType, indexCount, indices));
    }
    else
    {
        if (indices != mLastIndexBufferOffset)
        {
            mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
            mLastIndexBufferOffset = indices;
            mVertexArray->updateCurrentElementArrayBufferOffset(mLastIndexBufferOffset);
        }
        if (shouldConvertUint8VkIndexType(indexType) && mGraphicsDirtyBits[DIRTY_BIT_INDEX_BUFFER])
        {
            BufferVk *bufferVk             = vk::GetImpl(elementArrayBuffer);
            vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

            if (bufferHelper.isHostVisible() &&
                !bufferHelper.isCurrentlyInUse(getLastCompletedQueueSerial()))
            {
                uint8_t *src = nullptr;
                ANGLE_TRY(bufferVk->mapImpl(this, reinterpret_cast<void **>(&src)));
                src += reinterpret_cast<uintptr_t>(indices);
                const size_t byteCount = static_cast<size_t>(elementArrayBuffer->getSize()) -
                                         reinterpret_cast<uintptr_t>(indices);
                ANGLE_TRY(mVertexArray->convertIndexBufferCPU(this, indexType, byteCount, src));
                ANGLE_TRY(bufferVk->unmapImpl(this));
            }
            else
            {
                ANGLE_TRY(mVertexArray->convertIndexBufferGPU(this, bufferVk, indices));
            }
        }
    }

    return setupDraw(context, mode, 0, indexCount, instanceCount, indexType, indices,
                     mIndexedDirtyBitsMask, commandBufferOut);
}

angle::Result ContextVk::setupIndirectDraw(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           DirtyBits dirtyBitMask,
                                           vk::BufferHelper *indirectBuffer,
                                           VkDeviceSize indirectBufferOffset,
                                           vk::CommandBuffer **commandBufferOut)
{
    GLint firstVertex     = -1;
    GLsizei vertexCount   = 0;
    GLsizei instanceCount = 1;

    mRenderPassCommands->bufferRead(&mResourceUseList, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                    vk::PipelineStage::DrawIndirect, indirectBuffer);

    ANGLE_TRY(setupDraw(context, mode, firstVertex, vertexCount, instanceCount,
                        gl::DrawElementsType::InvalidEnum, nullptr, dirtyBitMask,
                        commandBufferOut));

    return angle::Result::Continue;
}

angle::Result ContextVk::setupIndexedIndirectDraw(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  gl::DrawElementsType indexType,
                                                  vk::BufferHelper *indirectBuffer,
                                                  VkDeviceSize indirectBufferOffset,
                                                  vk::CommandBuffer **commandBufferOut)
{
    ASSERT(mode != gl::PrimitiveMode::LineLoop);

    if (indexType != mCurrentDrawElementsType)
    {
        mCurrentDrawElementsType = indexType;
        setIndexBufferDirty();
    }

    return setupIndirectDraw(context, mode, mIndexedDirtyBitsMask, indirectBuffer,
                             indirectBufferOffset, commandBufferOut);
}

angle::Result ContextVk::setupLineLoopIndexedIndirectDraw(const gl::Context *context,
                                                          gl::PrimitiveMode mode,
                                                          gl::DrawElementsType indexType,
                                                          vk::BufferHelper *srcIndirectBuf,
                                                          VkDeviceSize indirectBufferOffset,
                                                          vk::CommandBuffer **commandBufferOut,
                                                          vk::BufferHelper **indirectBufferOut,
                                                          VkDeviceSize *indirectBufferOffsetOut)
{
    ASSERT(mode == gl::PrimitiveMode::LineLoop);

    vk::BufferHelper *dstIndirectBuf  = nullptr;
    VkDeviceSize dstIndirectBufOffset = 0;

    ANGLE_TRY(mVertexArray->handleLineLoopIndexIndirect(this, indexType, srcIndirectBuf,
                                                        indirectBufferOffset, &dstIndirectBuf,
                                                        &dstIndirectBufOffset));

    *indirectBufferOut       = dstIndirectBuf;
    *indirectBufferOffsetOut = dstIndirectBufOffset;

    if (indexType != mCurrentDrawElementsType)
    {
        mCurrentDrawElementsType = indexType;
        setIndexBufferDirty();
    }

    return setupIndirectDraw(context, mode, mIndexedDirtyBitsMask, dstIndirectBuf,
                             dstIndirectBufOffset, commandBufferOut);
}

angle::Result ContextVk::setupLineLoopIndirectDraw(const gl::Context *context,
                                                   gl::PrimitiveMode mode,
                                                   vk::BufferHelper *indirectBuffer,
                                                   VkDeviceSize indirectBufferOffset,
                                                   vk::CommandBuffer **commandBufferOut,
                                                   vk::BufferHelper **indirectBufferOut,
                                                   VkDeviceSize *indirectBufferOffsetOut)
{
    ASSERT(mode == gl::PrimitiveMode::LineLoop);

    vk::BufferHelper *indirectBufferHelperOut = nullptr;

    ANGLE_TRY(mVertexArray->handleLineLoopIndirectDraw(
        context, indirectBuffer, indirectBufferOffset, &indirectBufferHelperOut,
        indirectBufferOffsetOut));

    *indirectBufferOut = indirectBufferHelperOut;

    if (gl::DrawElementsType::UnsignedInt != mCurrentDrawElementsType)
    {
        mCurrentDrawElementsType = gl::DrawElementsType::UnsignedInt;
        setIndexBufferDirty();
    }

    return setupIndirectDraw(context, mode, mIndexedDirtyBitsMask, indirectBufferHelperOut,
                             *indirectBufferOffsetOut, commandBufferOut);
}

angle::Result ContextVk::setupLineLoopDraw(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           GLint firstVertex,
                                           GLsizei vertexOrIndexCount,
                                           gl::DrawElementsType indexTypeOrInvalid,
                                           const void *indices,
                                           vk::CommandBuffer **commandBufferOut,
                                           uint32_t *numIndicesOut)
{
    ANGLE_TRY(mVertexArray->handleLineLoop(this, firstVertex, vertexOrIndexCount,
                                           indexTypeOrInvalid, indices, numIndicesOut));
    setIndexBufferDirty();
    mCurrentDrawElementsType = indexTypeOrInvalid != gl::DrawElementsType::InvalidEnum
                                   ? indexTypeOrInvalid
                                   : gl::DrawElementsType::UnsignedInt;
    return setupDraw(context, mode, firstVertex, vertexOrIndexCount, 1, indexTypeOrInvalid, indices,
                     mIndexedDirtyBitsMask, commandBufferOut);
}

angle::Result ContextVk::setupDispatch(const gl::Context *context,
                                       vk::CommandBuffer **commandBufferOut)
{
    // |setupDispatch| and |setupDraw| are special in that they flush dirty bits. Therefore they
    // don't use the same APIs to record commands as the functions outside ContextVk.
    // The following ensures prior commands are flushed before we start processing dirty bits.
    ANGLE_TRY(flushOutsideRenderPassCommands());
    ANGLE_TRY(endRenderPass());
    *commandBufferOut = &mOutsideRenderPassCommands->getCommandBuffer();

    if (mProgram && mProgram->dirtyUniforms())
    {
        ANGLE_TRY(mProgram->updateUniforms(this));
        mComputeDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }
    else if (mProgramPipeline && mProgramPipeline->dirtyUniforms(getState()))
    {
        ANGLE_TRY(mProgramPipeline->updateUniforms(this));
        mComputeDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }

    DirtyBits dirtyBits = mComputeDirtyBits;

    // Flush any relevant dirty bits.
    for (size_t dirtyBit : dirtyBits)
    {
        ASSERT(mComputeDirtyBitHandlers[dirtyBit]);
        ANGLE_TRY((this->*mComputeDirtyBitHandlers[dirtyBit])(context, *commandBufferOut));
    }

    mComputeDirtyBits.reset();

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDefaultAttribs(const gl::Context *context,
                                                           vk::CommandBuffer *commandBuffer)
{
    ASSERT(mDirtyDefaultAttribsMask.any());

    for (size_t attribIndex : mDirtyDefaultAttribsMask)
    {
        ANGLE_TRY(updateDefaultAttribute(attribIndex));
    }

    mDirtyDefaultAttribsMask.reset();
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsPipeline(const gl::Context *context,
                                                     vk::CommandBuffer *commandBuffer)
{
    ASSERT(mExecutable);
    mExecutable->updateEarlyFragmentTestsOptimization(this);

    if (!mCurrentGraphicsPipeline)
    {
        const vk::GraphicsPipelineDesc *descPtr;

        // Draw call shader patching, shader compilation, and pipeline cache query.
        ANGLE_TRY(mExecutable->getGraphicsPipeline(
            this, mCurrentDrawMode, *mGraphicsPipelineDesc,
            context->getState().getProgramExecutable()->getNonBuiltinAttribLocationsMask(),
            &descPtr, &mCurrentGraphicsPipeline));
        mGraphicsPipelineTransition.reset();
    }
    else if (mGraphicsPipelineTransition.any())
    {
        if (!mCurrentGraphicsPipeline->findTransition(
                mGraphicsPipelineTransition, *mGraphicsPipelineDesc, &mCurrentGraphicsPipeline))
        {
            vk::PipelineHelper *oldPipeline = mCurrentGraphicsPipeline;
            const vk::GraphicsPipelineDesc *descPtr;

            ANGLE_TRY(mExecutable->getGraphicsPipeline(
                this, mCurrentDrawMode, *mGraphicsPipelineDesc,
                context->getState().getProgramExecutable()->getNonBuiltinAttribLocationsMask(),
                &descPtr, &mCurrentGraphicsPipeline));

            oldPipeline->addTransition(mGraphicsPipelineTransition, descPtr,
                                       mCurrentGraphicsPipeline);
        }

        mGraphicsPipelineTransition.reset();
    }
    mRenderPassCommands->pauseTransformFeedbackIfStarted();
    commandBuffer->bindGraphicsPipeline(mCurrentGraphicsPipeline->getPipeline());
    // Update the queue serial for the pipeline object.
    ASSERT(mCurrentGraphicsPipeline && mCurrentGraphicsPipeline->valid());
    mCurrentGraphicsPipeline->updateSerial(getCurrentQueueSerial());
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyComputePipeline(const gl::Context *context,
                                                    vk::CommandBuffer *commandBuffer)
{
    if (!mCurrentComputePipeline)
    {
        ASSERT(mExecutable);
        ANGLE_TRY(mExecutable->getComputePipeline(this, &mCurrentComputePipeline));
    }

    commandBuffer->bindComputePipeline(mCurrentComputePipeline->get());
    mCurrentComputePipeline->updateSerial(getCurrentQueueSerial());

    return angle::Result::Continue;
}

ANGLE_INLINE angle::Result ContextVk::handleDirtyTexturesImpl(
    vk::CommandBufferHelper *commandBufferHelper)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);
    const gl::ActiveTextureMask &activeTextures = executable->getActiveSamplersMask();

    for (size_t textureUnit : activeTextures)
    {
        const vk::TextureUnit &unit = mActiveTextures[textureUnit];
        TextureVk *textureVk        = unit.texture;
        vk::ImageHelper &image      = textureVk->getImage();

        // The image should be flushed and ready to use at this point. There may still be
        // lingering staged updates in its staging buffer for unused texture mip levels or
        // layers. Therefore we can't verify it has no staged updates right here.

        // Select the appropriate vk::ImageLayout depending on whether the texture is also bound as
        // a GL image, and whether the program is a compute or graphics shader.
        vk::ImageLayout textureLayout;
        if (textureVk->isBoundAsImageTexture(mState.getContextID()))
        {
            textureLayout = executable->isCompute() ? vk::ImageLayout::ComputeShaderWrite
                                                    : vk::ImageLayout::AllGraphicsShadersReadWrite;
        }
        else
        {
            gl::ShaderBitSet shaderBits =
                executable->getSamplerShaderBitsForTextureUnitIndex(textureUnit);
            if (shaderBits.any())
            {
                gl::ShaderType shader =
                    static_cast<gl::ShaderType>(gl::ScanForward(shaderBits.bits()));
                shaderBits.reset(shader);
                // If we have multiple shader accessing it, we barrier against all shader stage read
                // given that we only support vertex/frag shaders
                if (shaderBits.any())
                {
                    textureLayout = vk::ImageLayout::AllGraphicsShadersReadOnly;
                }
                else
                {
                    textureLayout = kShaderReadOnlyImageLayouts[shader];
                }
            }
            else
            {
                textureLayout = vk::ImageLayout::AllGraphicsShadersReadOnly;
            }
        }
        // Ensure the image is in read-only layout
        commandBufferHelper->imageRead(&mResourceUseList, image.getAspectFlags(), textureLayout,
                                       &image);

        textureVk->retainImageViews(&mResourceUseList);
    }

    if (executable->hasTextures())
    {
        ANGLE_TRY(mExecutable->updateTexturesDescriptorSet(this));
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsTextures(const gl::Context *context,
                                                     vk::CommandBuffer *commandBuffer)
{
    return handleDirtyTexturesImpl(mRenderPassCommands);
}

angle::Result ContextVk::handleDirtyComputeTextures(const gl::Context *context,
                                                    vk::CommandBuffer *commandBuffer)
{
    return handleDirtyTexturesImpl(mOutsideRenderPassCommands);
}

angle::Result ContextVk::handleDirtyGraphicsVertexBuffers(const gl::Context *context,
                                                          vk::CommandBuffer *commandBuffer)
{
    uint32_t maxAttrib = context->getState().getProgramExecutable()->getMaxActiveAttribLocation();
    const gl::AttribArray<VkBuffer> &bufferHandles = mVertexArray->getCurrentArrayBufferHandles();
    const gl::AttribArray<VkDeviceSize> &bufferOffsets =
        mVertexArray->getCurrentArrayBufferOffsets();

    commandBuffer->bindVertexBuffers(0, maxAttrib, bufferHandles.data(), bufferOffsets.data());

    const gl::AttribArray<vk::BufferHelper *> &arrayBufferResources =
        mVertexArray->getCurrentArrayBuffers();

    // Mark all active vertex buffers as accessed.
    const gl::ProgramExecutable *executable = context->getState().getProgramExecutable();
    gl::AttributesMask attribsMask          = executable->getActiveAttribLocationsMask();
    for (size_t attribIndex : attribsMask)
    {
        vk::BufferHelper *arrayBuffer = arrayBufferResources[attribIndex];
        if (arrayBuffer)
        {
            mRenderPassCommands->bufferRead(&mResourceUseList, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                            vk::PipelineStage::VertexInput, arrayBuffer);
        }
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsIndexBuffer(const gl::Context *context,
                                                        vk::CommandBuffer *commandBuffer)
{
    vk::BufferHelper *elementArrayBuffer = mVertexArray->getCurrentElementArrayBuffer();
    ASSERT(elementArrayBuffer != nullptr);

    commandBuffer->bindIndexBuffer(elementArrayBuffer->getBuffer(),
                                   mVertexArray->getCurrentElementArrayBufferOffset(),
                                   getVkIndexType(mCurrentDrawElementsType));

    mRenderPassCommands->bufferRead(&mResourceUseList, VK_ACCESS_INDEX_READ_BIT,
                                    vk::PipelineStage::VertexInput, elementArrayBuffer);

    return angle::Result::Continue;
}

ANGLE_INLINE angle::Result ContextVk::handleDirtyShaderResourcesImpl(
    const gl::Context *context,
    vk::CommandBufferHelper *commandBufferHelper)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    if (executable->hasImages())
    {
        ANGLE_TRY(updateActiveImages(context, commandBufferHelper));
    }

    if (executable->hasUniformBuffers() || executable->hasStorageBuffers() ||
        executable->hasAtomicCounterBuffers() || executable->hasImages())
    {
        ANGLE_TRY(mExecutable->updateShaderResourcesDescriptorSet(this, &mResourceUseList,
                                                                  commandBufferHelper));
    }
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsShaderResources(const gl::Context *context,
                                                            vk::CommandBuffer *commandBuffer)
{
    return handleDirtyShaderResourcesImpl(context, mRenderPassCommands);
}

angle::Result ContextVk::handleDirtyComputeShaderResources(const gl::Context *context,
                                                           vk::CommandBuffer *commandBuffer)
{
    return handleDirtyShaderResourcesImpl(context, mOutsideRenderPassCommands);
}

angle::Result ContextVk::handleDirtyGraphicsTransformFeedbackBuffersEmulation(
    const gl::Context *context,
    vk::CommandBuffer *commandBuffer)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    if (!executable->hasTransformFeedbackOutput() || !mState.isTransformFeedbackActive())
    {
        return angle::Result::Continue;
    }

    TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(mState.getCurrentTransformFeedback());
    size_t bufferCount                       = executable->getTransformFeedbackBufferCount(mState);
    const gl::TransformFeedbackBuffersArray<vk::BufferHelper *> &bufferHelpers =
        transformFeedbackVk->getBufferHelpers();

    for (size_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
    {
        vk::BufferHelper *bufferHelper = bufferHelpers[bufferIndex];
        ASSERT(bufferHelper);
        mRenderPassCommands->bufferWrite(&mResourceUseList, VK_ACCESS_SHADER_WRITE_BIT,
                                         vk::PipelineStage::VertexShader, bufferHelper);
    }

    // TODO(http://anglebug.com/3570): Need to update to handle Program Pipelines
    return mProgram->getExecutable().updateTransformFeedbackDescriptorSet(
        mProgram->getState(), mProgram->getDefaultUniformBlocks(), this);
}

angle::Result ContextVk::handleDirtyGraphicsTransformFeedbackBuffersExtension(
    const gl::Context *context,
    vk::CommandBuffer *commandBuffer)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    if (!executable->hasTransformFeedbackOutput() || !mState.isTransformFeedbackActive())
        return angle::Result::Continue;

    TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(mState.getCurrentTransformFeedback());
    size_t bufferCount                       = executable->getTransformFeedbackBufferCount(mState);

    const gl::TransformFeedbackBuffersArray<vk::BufferHelper *> &bufferHelpers =
        transformFeedbackVk->getBufferHelpers();

    for (size_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
    {
        vk::BufferHelper *bufferHelper = bufferHelpers[bufferIndex];
        ASSERT(bufferHelper);
        mRenderPassCommands->bufferWrite(&mResourceUseList,
                                         VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
                                         vk::PipelineStage::TransformFeedback, bufferHelper);
    }

    const gl::TransformFeedbackBuffersArray<VkBuffer> &bufferHandles =
        transformFeedbackVk->getBufferHandles();
    const gl::TransformFeedbackBuffersArray<VkDeviceSize> &bufferOffsets =
        transformFeedbackVk->getBufferOffsets();
    const gl::TransformFeedbackBuffersArray<VkDeviceSize> &bufferSizes =
        transformFeedbackVk->getBufferSizes();

    commandBuffer->bindTransformFeedbackBuffers(static_cast<uint32_t>(bufferCount),
                                                bufferHandles.data(), bufferOffsets.data(),
                                                bufferSizes.data());

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsTransformFeedbackState(const gl::Context *context,
                                                                   vk::CommandBuffer *commandBuffer)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    if (!executable->hasTransformFeedbackOutput() || !mState.isTransformFeedbackActiveUnpaused())
    {
        return angle::Result::Continue;
    }

    TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(mState.getCurrentTransformFeedback());

    // We should have same number of counter buffers as xfb buffers have
    size_t bufferCount = executable->getTransformFeedbackBufferCount(mState);
    const gl::TransformFeedbackBuffersArray<VkBuffer> &counterBufferHandles =
        transformFeedbackVk->getCounterBufferHandles();

    bool rebindBuffers = transformFeedbackVk->getAndResetBufferRebindState();

    mRenderPassCommands->beginTransformFeedback(bufferCount, counterBufferHandles.data(),
                                                rebindBuffers);

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsTransformFeedbackResume(
    const gl::Context *context,
    vk::CommandBuffer *commandBuffer)
{
    mRenderPassCommands->resumeTransformFeedbackIfStarted();
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyDescriptorSets(const gl::Context *context,
                                                   vk::CommandBuffer *commandBuffer)
{
    ANGLE_TRY(mExecutable->updateDescriptorSets(this, commandBuffer));
    return angle::Result::Continue;
}

angle::Result ContextVk::submitFrame(const VkSubmitInfo &submitInfo,
                                     vk::PrimaryCommandBuffer &&commandBuffer)
{
    // Update overlay if active.
    gl::RunningGraphWidget *renderPassCount =
        mState.getOverlay()->getRunningGraphWidget(gl::WidgetId::VulkanRenderPassCount);
    renderPassCount->add(mRenderPassCommands->getAndResetCounter());
    renderPassCount->next();

    if (vk::CommandBufferHelper::kEnableCommandStreamDiagnostics)
    {
        dumpCommandStreamDiagnostics();
    }

    ANGLE_TRY(ensureSubmitFenceInitialized());
    ANGLE_TRY(mCommandQueue.submitFrame(this, mContextPriority, submitInfo, mSubmitFence,
                                        &mCurrentGarbage, &mCommandPool, std::move(commandBuffer)));

    // we need to explicitly notify every other Context using this VkQueue that their current
    // command buffer is no longer valid.
    onRenderPassFinished();
    mComputeDirtyBits |= mNewComputeCommandBufferDirtyBits;

    // Make sure a new fence is created for the next submission.
    mRenderer->resetSharedFence(&mSubmitFence);

    if (mGpuEventsEnabled)
    {
        ANGLE_TRY(checkCompletedGpuEvents());
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::synchronizeCpuGpuTime()
{
    ASSERT(mGpuEventsEnabled);

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    // To synchronize CPU and GPU times, we need to get the CPU timestamp as close as possible
    // to the GPU timestamp.  The process of getting the GPU timestamp is as follows:
    //
    //             CPU                            GPU
    //
    //     Record command buffer
    //     with timestamp query
    //
    //     Submit command buffer
    //
    //     Post-submission work             Begin execution
    //
    //            ????                    Write timestamp Tgpu
    //
    //            ????                       End execution
    //
    //            ????                    Return query results
    //
    //            ????
    //
    //       Get query results
    //
    // The areas of unknown work (????) on the CPU indicate that the CPU may or may not have
    // finished post-submission work while the GPU is executing in parallel. With no further
    // work, querying CPU timestamps before submission and after getting query results give the
    // bounds to Tgpu, which could be quite large.
    //
    // Using VkEvents, the GPU can be made to wait for the CPU and vice versa, in an effort to
    // reduce this range. This function implements the following procedure:
    //
    //             CPU                            GPU
    //
    //     Record command buffer
    //     with timestamp query
    //
    //     Submit command buffer
    //
    //     Post-submission work             Begin execution
    //
    //            ????                    Set Event GPUReady
    //
    //    Wait on Event GPUReady         Wait on Event CPUReady
    //
    //       Get CPU Time Ts             Wait on Event CPUReady
    //
    //      Set Event CPUReady           Wait on Event CPUReady
    //
    //      Get CPU Time Tcpu              Get GPU Time Tgpu
    //
    //    Wait on Event GPUDone            Set Event GPUDone
    //
    //       Get CPU Time Te                 End Execution
    //
    //            Idle                    Return query results
    //
    //      Get query results
    //
    // If Te-Ts > epsilon, a GPU or CPU interruption can be assumed and the operation can be
    // retried.  Once Te-Ts < epsilon, Tcpu can be taken to presumably match Tgpu.  Finding an
    // epsilon that's valid for all devices may be difficult, so the loop can be performed only
    // a limited number of times and the Tcpu,Tgpu pair corresponding to smallest Te-Ts used for
    // calibration.
    //
    // Note: Once VK_EXT_calibrated_timestamps is ubiquitous, this should be redone.

    // Make sure nothing is running
    ASSERT(!hasRecordedCommands());

    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::synchronizeCpuGpuTime");

    // Create a query used to receive the GPU timestamp
    vk::QueryHelper timestampQuery;
    ANGLE_TRY(mGpuEventQueryPool.allocateQuery(this, &timestampQuery));

    // Create the three events
    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType             = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
    eventCreateInfo.flags             = 0;

    VkDevice device = getDevice();
    vk::DeviceScoped<vk::Event> cpuReady(device), gpuReady(device), gpuDone(device);
    ANGLE_VK_TRY(this, cpuReady.get().init(device, eventCreateInfo));
    ANGLE_VK_TRY(this, gpuReady.get().init(device, eventCreateInfo));
    ANGLE_VK_TRY(this, gpuDone.get().init(device, eventCreateInfo));

    constexpr uint32_t kRetries = 10;

    // Time suffixes used are S for seconds and Cycles for cycles
    double tightestRangeS = 1e6f;
    double TcpuS          = 0;
    uint64_t TgpuCycles   = 0;
    for (uint32_t i = 0; i < kRetries; ++i)
    {
        // Reset the events
        ANGLE_VK_TRY(this, cpuReady.get().reset(device));
        ANGLE_VK_TRY(this, gpuReady.get().reset(device));
        ANGLE_VK_TRY(this, gpuDone.get().reset(device));

        // Record the command buffer
        vk::DeviceScoped<vk::PrimaryCommandBuffer> commandBatch(device);
        vk::PrimaryCommandBuffer &commandBuffer = commandBatch.get();

        ANGLE_TRY(mCommandQueue.allocatePrimaryCommandBuffer(this, mCommandPool, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo         = nullptr;

        ANGLE_VK_TRY(this, commandBuffer.begin(beginInfo));

        commandBuffer.setEvent(gpuReady.get().getHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
        commandBuffer.waitEvents(1, cpuReady.get().ptr(), VK_PIPELINE_STAGE_HOST_BIT,
                                 VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, nullptr, 0, nullptr, 0,
                                 nullptr);
        timestampQuery.writeTimestamp(this, &commandBuffer);
        commandBuffer.setEvent(gpuDone.get().getHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

        ANGLE_VK_TRY(this, commandBuffer.end());

        // Submit the command buffer
        VkSubmitInfo submitInfo = {};
        InitializeSubmitInfo(&submitInfo, commandBatch.get(), {}, &mWaitSemaphoreStageMasks,
                             nullptr);

        ANGLE_TRY(submitFrame(submitInfo, commandBatch.release()));

        // Wait for GPU to be ready.  This is a short busy wait.
        VkResult result = VK_EVENT_RESET;
        do
        {
            result = gpuReady.get().getStatus(device);
            if (result != VK_EVENT_SET && result != VK_EVENT_RESET)
            {
                ANGLE_VK_TRY(this, result);
            }
        } while (result == VK_EVENT_RESET);

        double TsS = platform->monotonicallyIncreasingTime(platform);

        // Tell the GPU to go ahead with the timestamp query.
        ANGLE_VK_TRY(this, cpuReady.get().set(device));
        double cpuTimestampS = platform->monotonicallyIncreasingTime(platform);

        // Wait for GPU to be done.  Another short busy wait.
        do
        {
            result = gpuDone.get().getStatus(device);
            if (result != VK_EVENT_SET && result != VK_EVENT_RESET)
            {
                ANGLE_VK_TRY(this, result);
            }
        } while (result == VK_EVENT_RESET);

        double TeS = platform->monotonicallyIncreasingTime(platform);

        // Get the query results
        ANGLE_TRY(finishToSerial(getLastSubmittedQueueSerial()));

        uint64_t gpuTimestampCycles = 0;
        ANGLE_TRY(timestampQuery.getUint64Result(this, &gpuTimestampCycles));

        // Use the first timestamp queried as origin.
        if (mGpuEventTimestampOrigin == 0)
        {
            mGpuEventTimestampOrigin = gpuTimestampCycles;
        }

        // Take these CPU and GPU timestamps if there is better confidence.
        double confidenceRangeS = TeS - TsS;
        if (confidenceRangeS < tightestRangeS)
        {
            tightestRangeS = confidenceRangeS;
            TcpuS          = cpuTimestampS;
            TgpuCycles     = gpuTimestampCycles;
        }
    }

    mGpuEventQueryPool.freeQuery(this, &timestampQuery);

    // timestampPeriod gives nanoseconds/cycle.
    double TgpuS =
        (TgpuCycles - mGpuEventTimestampOrigin) *
        static_cast<double>(getRenderer()->getPhysicalDeviceProperties().limits.timestampPeriod) /
        1'000'000'000.0;

    flushGpuEvents(TgpuS, TcpuS);

    mGpuClockSync.gpuTimestampS = TgpuS;
    mGpuClockSync.cpuTimestampS = TcpuS;

    return angle::Result::Continue;
}

angle::Result ContextVk::traceGpuEventImpl(vk::CommandBuffer *commandBuffer,
                                           char phase,
                                           const EventName &name)
{
    ASSERT(mGpuEventsEnabled);

    GpuEventQuery gpuEvent;
    gpuEvent.name  = name;
    gpuEvent.phase = phase;
    ANGLE_TRY(mGpuEventQueryPool.allocateQuery(this, &gpuEvent.queryHelper));

    gpuEvent.queryHelper.writeTimestamp(this, commandBuffer);

    mInFlightGpuEventQueries.push_back(std::move(gpuEvent));
    return angle::Result::Continue;
}

angle::Result ContextVk::checkCompletedGpuEvents()
{
    ASSERT(mGpuEventsEnabled);

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    int finishedCount = 0;

    Serial lastCompletedSerial = getLastCompletedQueueSerial();

    for (GpuEventQuery &eventQuery : mInFlightGpuEventQueries)
    {
        // Only check the timestamp query if the submission has finished.
        if (eventQuery.queryHelper.getStoredQueueSerial() > lastCompletedSerial)
        {
            break;
        }

        // See if the results are available.
        uint64_t gpuTimestampCycles = 0;
        bool available              = false;
        ANGLE_TRY(eventQuery.queryHelper.getUint64ResultNonBlocking(this, &gpuTimestampCycles,
                                                                    &available));
        if (!available)
        {
            break;
        }

        mGpuEventQueryPool.freeQuery(this, &eventQuery.queryHelper);

        GpuEvent gpuEvent;
        gpuEvent.gpuTimestampCycles = gpuTimestampCycles;
        gpuEvent.name               = eventQuery.name;
        gpuEvent.phase              = eventQuery.phase;

        mGpuEvents.emplace_back(gpuEvent);

        ++finishedCount;
    }

    mInFlightGpuEventQueries.erase(mInFlightGpuEventQueries.begin(),
                                   mInFlightGpuEventQueries.begin() + finishedCount);

    return angle::Result::Continue;
}

void ContextVk::flushGpuEvents(double nextSyncGpuTimestampS, double nextSyncCpuTimestampS)
{
    if (mGpuEvents.empty())
    {
        return;
    }

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    // Find the slope of the clock drift for adjustment
    double lastGpuSyncTimeS  = mGpuClockSync.gpuTimestampS;
    double lastGpuSyncDiffS  = mGpuClockSync.cpuTimestampS - mGpuClockSync.gpuTimestampS;
    double gpuSyncDriftSlope = 0;

    double nextGpuSyncTimeS = nextSyncGpuTimestampS;
    double nextGpuSyncDiffS = nextSyncCpuTimestampS - nextSyncGpuTimestampS;

    // No gpu trace events should have been generated before the clock sync, so if there is no
    // "previous" clock sync, there should be no gpu events (i.e. the function early-outs
    // above).
    ASSERT(mGpuClockSync.gpuTimestampS != std::numeric_limits<double>::max() &&
           mGpuClockSync.cpuTimestampS != std::numeric_limits<double>::max());

    gpuSyncDriftSlope =
        (nextGpuSyncDiffS - lastGpuSyncDiffS) / (nextGpuSyncTimeS - lastGpuSyncTimeS);

    for (const GpuEvent &gpuEvent : mGpuEvents)
    {
        double gpuTimestampS =
            (gpuEvent.gpuTimestampCycles - mGpuEventTimestampOrigin) *
            static_cast<double>(
                getRenderer()->getPhysicalDeviceProperties().limits.timestampPeriod) *
            1e-9;

        // Account for clock drift.
        gpuTimestampS += lastGpuSyncDiffS + gpuSyncDriftSlope * (gpuTimestampS - lastGpuSyncTimeS);

        // Generate the trace now that the GPU timestamp is available and clock drifts are
        // accounted for.
        static long long eventId = 1;
        static const unsigned char *categoryEnabled =
            TRACE_EVENT_API_GET_CATEGORY_ENABLED(platform, "gpu.angle.gpu");
        platform->addTraceEvent(platform, gpuEvent.phase, categoryEnabled, gpuEvent.name.data(),
                                eventId++, gpuTimestampS, 0, nullptr, nullptr, nullptr,
                                TRACE_EVENT_FLAG_NONE);
    }

    mGpuEvents.clear();
}

void ContextVk::clearAllGarbage()
{
    for (vk::GarbageObject &garbage : mCurrentGarbage)
    {
        garbage.destroy(mRenderer);
    }
    mCurrentGarbage.clear();
    mCommandQueue.clearAllGarbage(mRenderer);
}

void ContextVk::handleDeviceLost()
{
    mOutsideRenderPassCommands->reset();
    mRenderPassCommands->reset();

    mCommandQueue.handleDeviceLost(mRenderer);
    clearAllGarbage();

    mRenderer->notifyDeviceLost();
}

angle::Result ContextVk::drawArrays(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    GLint first,
                                    GLsizei count)
{
    vk::CommandBuffer *commandBuffer = nullptr;
    uint32_t clampedVertexCount      = gl::GetClampedVertexCount<uint32_t>(count);

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t numIndices;
        ANGLE_TRY(setupLineLoopDraw(context, mode, first, count, gl::DrawElementsType::InvalidEnum,
                                    nullptr, &commandBuffer, &numIndices));
        vk::LineLoopHelper::Draw(numIndices, 0, commandBuffer);
    }
    else
    {
        ANGLE_TRY(setupDraw(context, mode, first, count, 1, gl::DrawElementsType::InvalidEnum,
                            nullptr, mNonIndexedDirtyBitsMask, &commandBuffer));
        commandBuffer->draw(clampedVertexCount, first);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::drawArraysInstanced(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei instances)
{
    vk::CommandBuffer *commandBuffer = nullptr;

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t clampedVertexCount = gl::GetClampedVertexCount<uint32_t>(count);
        uint32_t numIndices;
        ANGLE_TRY(setupLineLoopDraw(context, mode, first, clampedVertexCount,
                                    gl::DrawElementsType::InvalidEnum, nullptr, &commandBuffer,
                                    &numIndices));
        commandBuffer->drawIndexedInstanced(numIndices, instances);
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupDraw(context, mode, first, count, instances, gl::DrawElementsType::InvalidEnum,
                        nullptr, mNonIndexedDirtyBitsMask, &commandBuffer));
    commandBuffer->drawInstanced(gl::GetClampedVertexCount<uint32_t>(count), instances, first);
    return angle::Result::Continue;
}

angle::Result ContextVk::drawArraysInstancedBaseInstance(const gl::Context *context,
                                                         gl::PrimitiveMode mode,
                                                         GLint first,
                                                         GLsizei count,
                                                         GLsizei instances,
                                                         GLuint baseInstance)
{
    vk::CommandBuffer *commandBuffer = nullptr;

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t clampedVertexCount = gl::GetClampedVertexCount<uint32_t>(count);
        uint32_t numIndices;
        ANGLE_TRY(setupLineLoopDraw(context, mode, first, clampedVertexCount,
                                    gl::DrawElementsType::InvalidEnum, nullptr, &commandBuffer,
                                    &numIndices));
        commandBuffer->drawIndexedInstancedBaseVertexBaseInstance(numIndices, instances, 0, 0,
                                                                  baseInstance);
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupDraw(context, mode, first, count, instances, gl::DrawElementsType::InvalidEnum,
                        nullptr, mNonIndexedDirtyBitsMask, &commandBuffer));
    commandBuffer->drawInstancedBaseInstance(gl::GetClampedVertexCount<uint32_t>(count), instances,
                                             first, baseInstance);
    return angle::Result::Continue;
}

angle::Result ContextVk::drawElements(const gl::Context *context,
                                      gl::PrimitiveMode mode,
                                      GLsizei count,
                                      gl::DrawElementsType type,
                                      const void *indices)
{
    vk::CommandBuffer *commandBuffer = nullptr;
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t indexCount;
        ANGLE_TRY(
            setupLineLoopDraw(context, mode, 0, count, type, indices, &commandBuffer, &indexCount));
        vk::LineLoopHelper::Draw(indexCount, 0, commandBuffer);
    }
    else
    {
        ANGLE_TRY(setupIndexedDraw(context, mode, count, 1, type, indices, &commandBuffer));
        commandBuffer->drawIndexed(count);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::drawElementsBaseVertex(const gl::Context *context,
                                                gl::PrimitiveMode mode,
                                                GLsizei count,
                                                gl::DrawElementsType type,
                                                const void *indices,
                                                GLint baseVertex)
{
    vk::CommandBuffer *commandBuffer = nullptr;
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t indexCount;
        ANGLE_TRY(
            setupLineLoopDraw(context, mode, 0, count, type, indices, &commandBuffer, &indexCount));
        vk::LineLoopHelper::Draw(indexCount, baseVertex, commandBuffer);
    }
    else
    {
        ANGLE_TRY(setupIndexedDraw(context, mode, count, 1, type, indices, &commandBuffer));
        commandBuffer->drawIndexedBaseVertex(count, baseVertex);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::drawElementsInstanced(const gl::Context *context,
                                               gl::PrimitiveMode mode,
                                               GLsizei count,
                                               gl::DrawElementsType type,
                                               const void *indices,
                                               GLsizei instances)
{
    vk::CommandBuffer *commandBuffer = nullptr;

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t indexCount;
        ANGLE_TRY(
            setupLineLoopDraw(context, mode, 0, count, type, indices, &commandBuffer, &indexCount));
        count = indexCount;
    }
    else
    {
        ANGLE_TRY(setupIndexedDraw(context, mode, count, instances, type, indices, &commandBuffer));
    }

    commandBuffer->drawIndexedInstanced(count, instances);
    return angle::Result::Continue;
}

angle::Result ContextVk::drawElementsInstancedBaseVertex(const gl::Context *context,
                                                         gl::PrimitiveMode mode,
                                                         GLsizei count,
                                                         gl::DrawElementsType type,
                                                         const void *indices,
                                                         GLsizei instances,
                                                         GLint baseVertex)
{
    vk::CommandBuffer *commandBuffer = nullptr;

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t indexCount;
        ANGLE_TRY(
            setupLineLoopDraw(context, mode, 0, count, type, indices, &commandBuffer, &indexCount));
        count = indexCount;
    }
    else
    {
        ANGLE_TRY(setupIndexedDraw(context, mode, count, instances, type, indices, &commandBuffer));
    }

    commandBuffer->drawIndexedInstancedBaseVertex(count, instances, baseVertex);
    return angle::Result::Continue;
}

angle::Result ContextVk::drawElementsInstancedBaseVertexBaseInstance(const gl::Context *context,
                                                                     gl::PrimitiveMode mode,
                                                                     GLsizei count,
                                                                     gl::DrawElementsType type,
                                                                     const void *indices,
                                                                     GLsizei instances,
                                                                     GLint baseVertex,
                                                                     GLuint baseInstance)
{
    vk::CommandBuffer *commandBuffer = nullptr;

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t indexCount;
        ANGLE_TRY(
            setupLineLoopDraw(context, mode, 0, count, type, indices, &commandBuffer, &indexCount));
        commandBuffer->drawIndexedInstancedBaseVertexBaseInstance(indexCount, instances, 0,
                                                                  baseVertex, baseInstance);
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupIndexedDraw(context, mode, count, instances, type, indices, &commandBuffer));
    commandBuffer->drawIndexedInstancedBaseVertexBaseInstance(count, instances, 0, baseVertex,
                                                              baseInstance);
    return angle::Result::Continue;
}

angle::Result ContextVk::drawRangeElements(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           GLuint start,
                                           GLuint end,
                                           GLsizei count,
                                           gl::DrawElementsType type,
                                           const void *indices)
{
    return drawElements(context, mode, count, type, indices);
}

angle::Result ContextVk::drawRangeElementsBaseVertex(const gl::Context *context,
                                                     gl::PrimitiveMode mode,
                                                     GLuint start,
                                                     GLuint end,
                                                     GLsizei count,
                                                     gl::DrawElementsType type,
                                                     const void *indices,
                                                     GLint baseVertex)
{
    return drawElementsBaseVertex(context, mode, count, type, indices, baseVertex);
}

VkDevice ContextVk::getDevice() const
{
    return mRenderer->getDevice();
}

angle::Result ContextVk::drawArraysIndirect(const gl::Context *context,
                                            gl::PrimitiveMode mode,
                                            const void *indirect)
{
    gl::Buffer *indirectBuffer            = mState.getTargetBuffer(gl::BufferBinding::DrawIndirect);
    vk::BufferHelper *currentIndirectBuf  = &vk::GetImpl(indirectBuffer)->getBuffer();
    VkDeviceSize currentIndirectBufOffset = reinterpret_cast<VkDeviceSize>(indirect);

    if (mVertexArray->getStreamingVertexAttribsMask().any())
    {
        mRenderPassCommands->bufferRead(&mResourceUseList, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                        vk::PipelineStage::DrawIndirect, currentIndirectBuf);

        // We have instanced vertex attributes that need to be emulated for Vulkan.
        // invalidate any cache and map the buffer so that we can read the indirect data.
        // Mapping the buffer will cause a flush.
        ANGLE_TRY(currentIndirectBuf->invalidate(mRenderer, 0, sizeof(VkDrawIndirectCommand)));
        uint8_t *buffPtr;
        ANGLE_TRY(currentIndirectBuf->map(this, &buffPtr));
        const VkDrawIndirectCommand *indirectData =
            reinterpret_cast<VkDrawIndirectCommand *>(buffPtr + currentIndirectBufOffset);

        ANGLE_TRY(drawArraysInstanced(context, mode, indirectData->firstVertex,
                                      indirectData->vertexCount, indirectData->instanceCount));

        currentIndirectBuf->unmap(mRenderer);
        return angle::Result::Continue;
    }

    vk::CommandBuffer *commandBuffer = nullptr;

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        ASSERT(indirectBuffer);
        vk::BufferHelper *dstIndirectBuf  = nullptr;
        VkDeviceSize dstIndirectBufOffset = 0;

        ANGLE_TRY(setupLineLoopIndirectDraw(context, mode, currentIndirectBuf,
                                            currentIndirectBufOffset, &commandBuffer,
                                            &dstIndirectBuf, &dstIndirectBufOffset));

        commandBuffer->drawIndexedIndirect(dstIndirectBuf->getBuffer(), dstIndirectBufOffset, 1, 0);
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupIndirectDraw(context, mode, mNonIndexedDirtyBitsMask, currentIndirectBuf,
                                currentIndirectBufOffset, &commandBuffer));

    commandBuffer->drawIndirect(currentIndirectBuf->getBuffer(), currentIndirectBufOffset, 1, 0);
    return angle::Result::Continue;
}

angle::Result ContextVk::drawElementsIndirect(const gl::Context *context,
                                              gl::PrimitiveMode mode,
                                              gl::DrawElementsType type,
                                              const void *indirect)
{
    VkDeviceSize currentIndirectBufOffset = reinterpret_cast<VkDeviceSize>(indirect);
    gl::Buffer *indirectBuffer            = mState.getTargetBuffer(gl::BufferBinding::DrawIndirect);
    ASSERT(indirectBuffer);
    vk::BufferHelper *currentIndirectBuf = &vk::GetImpl(indirectBuffer)->getBuffer();

    if (mVertexArray->getStreamingVertexAttribsMask().any())
    {
        mRenderPassCommands->bufferRead(&mResourceUseList, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                        vk::PipelineStage::DrawIndirect, currentIndirectBuf);

        // We have instanced vertex attributes that need to be emulated for Vulkan.
        // invalidate any cache and map the buffer so that we can read the indirect data.
        // Mapping the buffer will cause a flush.
        ANGLE_TRY(
            currentIndirectBuf->invalidate(mRenderer, 0, sizeof(VkDrawIndexedIndirectCommand)));
        uint8_t *buffPtr;
        ANGLE_TRY(currentIndirectBuf->map(this, &buffPtr));
        const VkDrawIndexedIndirectCommand *indirectData =
            reinterpret_cast<VkDrawIndexedIndirectCommand *>(buffPtr + currentIndirectBufOffset);

        ANGLE_TRY(drawElementsInstanced(context, mode, indirectData->indexCount, type, nullptr,
                                        indirectData->instanceCount));

        currentIndirectBuf->unmap(mRenderer);
        return angle::Result::Continue;
    }

    if (shouldConvertUint8VkIndexType(type) && mGraphicsDirtyBits[DIRTY_BIT_INDEX_BUFFER])
    {
        vk::BufferHelper *dstIndirectBuf;
        VkDeviceSize dstIndirectBufOffset;

        ANGLE_TRY(mVertexArray->convertIndexBufferIndirectGPU(
            this, currentIndirectBuf, currentIndirectBufOffset, &dstIndirectBuf,
            &dstIndirectBufOffset));

        currentIndirectBuf       = dstIndirectBuf;
        currentIndirectBufOffset = dstIndirectBufOffset;
    }

    vk::CommandBuffer *commandBuffer = nullptr;

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        vk::BufferHelper *dstIndirectBuf;
        VkDeviceSize dstIndirectBufOffset;

        ANGLE_TRY(setupLineLoopIndexedIndirectDraw(context, mode, type, currentIndirectBuf,
                                                   currentIndirectBufOffset, &commandBuffer,
                                                   &dstIndirectBuf, &dstIndirectBufOffset));

        currentIndirectBuf       = dstIndirectBuf;
        currentIndirectBufOffset = dstIndirectBufOffset;
    }
    else
    {
        ANGLE_TRY(setupIndexedIndirectDraw(context, mode, type, currentIndirectBuf,
                                           currentIndirectBufOffset, &commandBuffer));
    }

    commandBuffer->drawIndexedIndirect(currentIndirectBuf->getBuffer(), currentIndirectBufOffset, 1,
                                       0);
    return angle::Result::Continue;
}

void ContextVk::optimizeRenderPassForPresent(VkFramebuffer framebufferHandle)
{
    if (!mRenderPassCommands->started())
    {
        return;
    }

    if (framebufferHandle != mRenderPassCommands->getFramebufferHandle())
    {
        return;
    }

    RenderTargetVk *color0RenderTarget = mDrawFramebuffer->getColorDrawRenderTarget(0);
    if (!color0RenderTarget)
    {
        return;
    }

    // EGL1.5 spec: The contents of ancillary buffers are always undefined after calling
    // eglSwapBuffers
    RenderTargetVk *depthStencilRenderTarget = mDrawFramebuffer->getDepthStencilRenderTarget();
    if (depthStencilRenderTarget)
    {
        size_t depthStencilAttachmentIndexVk = mDrawFramebuffer->getDepthStencilAttachmentIndexVk();
        // Change depthstencil attachment storeOp to DONT_CARE
        mRenderPassCommands->invalidateRenderPassStencilAttachment(depthStencilAttachmentIndexVk);
        mRenderPassCommands->invalidateRenderPassDepthAttachment(depthStencilAttachmentIndexVk);
        // Mark content as invalid so that we will not load them in next renderpass
        depthStencilRenderTarget->invalidateContent();
    }

    // Use finalLayout instead of extra barrier for layout change to present
    vk::ImageHelper &image = color0RenderTarget->getImage();
    image.setCurrentImageLayout(vk::ImageLayout::Present);
    mRenderPassCommands->updateRenderPassAttachmentFinalLayout(0, image.getCurrentImageLayout());
}

gl::GraphicsResetStatus ContextVk::getResetStatus()
{
    if (mRenderer->isDeviceLost())
    {
        // TODO(geofflang): It may be possible to track which context caused the device lost and
        // return either GL_GUILTY_CONTEXT_RESET or GL_INNOCENT_CONTEXT_RESET.
        // http://anglebug.com/2787
        return gl::GraphicsResetStatus::UnknownContextReset;
    }

    return gl::GraphicsResetStatus::NoError;
}

std::string ContextVk::getVendorString() const
{
    UNIMPLEMENTED();
    return std::string();
}

std::string ContextVk::getRendererDescription() const
{
    return mRenderer->getRendererDescription();
}

angle::Result ContextVk::insertEventMarker(GLsizei length, const char *marker)
{
    if (!mRenderer->enableDebugUtils())
        return angle::Result::Continue;

    vk::CommandBuffer *outsideRenderPassCommandBuffer;
    ANGLE_TRY(endRenderPassAndGetCommandBuffer(&outsideRenderPassCommandBuffer));

    VkDebugUtilsLabelEXT label;
    vk::MakeDebugUtilsLabel(GL_DEBUG_SOURCE_APPLICATION, marker, &label);
    outsideRenderPassCommandBuffer->insertDebugUtilsLabelEXT(label);

    return angle::Result::Continue;
}

angle::Result ContextVk::pushGroupMarker(GLsizei length, const char *marker)
{
    if (!mRenderer->enableDebugUtils())
        return angle::Result::Continue;

    vk::CommandBuffer *outsideRenderPassCommandBuffer;
    ANGLE_TRY(endRenderPassAndGetCommandBuffer(&outsideRenderPassCommandBuffer));

    VkDebugUtilsLabelEXT label;
    vk::MakeDebugUtilsLabel(GL_DEBUG_SOURCE_APPLICATION, marker, &label);
    outsideRenderPassCommandBuffer->beginDebugUtilsLabelEXT(label);

    return angle::Result::Continue;
}

angle::Result ContextVk::popGroupMarker()
{
    if (!mRenderer->enableDebugUtils())
        return angle::Result::Continue;

    vk::CommandBuffer *outsideRenderPassCommandBuffer;
    ANGLE_TRY(endRenderPassAndGetCommandBuffer(&outsideRenderPassCommandBuffer));
    outsideRenderPassCommandBuffer->endDebugUtilsLabelEXT();

    return angle::Result::Continue;
}

angle::Result ContextVk::pushDebugGroup(const gl::Context *context,
                                        GLenum source,
                                        GLuint id,
                                        const std::string &message)
{
    if (!mRenderer->enableDebugUtils())
        return angle::Result::Continue;

    vk::CommandBuffer *outsideRenderPassCommandBuffer;
    ANGLE_TRY(endRenderPassAndGetCommandBuffer(&outsideRenderPassCommandBuffer));

    VkDebugUtilsLabelEXT label;
    vk::MakeDebugUtilsLabel(source, message.c_str(), &label);
    outsideRenderPassCommandBuffer->beginDebugUtilsLabelEXT(label);

    return angle::Result::Continue;
}

angle::Result ContextVk::popDebugGroup(const gl::Context *context)
{
    if (!mRenderer->enableDebugUtils())
        return angle::Result::Continue;

    vk::CommandBuffer *outsideRenderPassCommandBuffer;
    ANGLE_TRY(endRenderPassAndGetCommandBuffer(&outsideRenderPassCommandBuffer));
    outsideRenderPassCommandBuffer->endDebugUtilsLabelEXT();

    return angle::Result::Continue;
}

bool ContextVk::isViewportFlipEnabledForDrawFBO() const
{
    return mFlipViewportForDrawFramebuffer && mFlipYForCurrentSurface;
}

bool ContextVk::isViewportFlipEnabledForReadFBO() const
{
    return mFlipViewportForReadFramebuffer;
}

bool ContextVk::isRotatedAspectRatioForDrawFBO() const
{
    return IsRotatedAspectRatio(mCurrentRotationDrawFramebuffer);
}

bool ContextVk::isRotatedAspectRatioForReadFBO() const
{
    return IsRotatedAspectRatio(mCurrentRotationReadFramebuffer);
}

SurfaceRotation ContextVk::getRotationDrawFramebuffer() const
{
    return mCurrentRotationDrawFramebuffer;
}

SurfaceRotation ContextVk::getRotationReadFramebuffer() const
{
    return mCurrentRotationReadFramebuffer;
}

void ContextVk::updateColorMask(const gl::BlendState &blendState)
{
    mClearColorMask =
        gl_vk::GetColorComponentFlags(blendState.colorMaskRed, blendState.colorMaskGreen,
                                      blendState.colorMaskBlue, blendState.colorMaskAlpha);

    FramebufferVk *framebufferVk = vk::GetImpl(mState.getDrawFramebuffer());
    mGraphicsPipelineDesc->updateColorWriteMask(&mGraphicsPipelineTransition, mClearColorMask,
                                                framebufferVk->getEmulatedAlphaAttachmentMask());
}

void ContextVk::updateSampleMask(const gl::State &glState)
{
    // If sample coverage is enabled, emulate it by generating and applying a mask on top of the
    // sample mask.
    uint32_t coverageSampleCount = GetCoverageSampleCount(glState, mDrawFramebuffer);

    static_assert(sizeof(uint32_t) == sizeof(GLbitfield), "Vulkan assumes 32-bit sample masks");
    for (uint32_t maskNumber = 0; maskNumber < glState.getMaxSampleMaskWords(); ++maskNumber)
    {
        uint32_t mask = glState.isSampleMaskEnabled() ? glState.getSampleMaskWord(maskNumber)
                                                      : std::numeric_limits<uint32_t>::max();

        ApplySampleCoverage(glState, coverageSampleCount, maskNumber, &mask);

        mGraphicsPipelineDesc->updateSampleMask(&mGraphicsPipelineTransition, maskNumber, mask);
    }
}

void ContextVk::updateViewport(FramebufferVk *framebufferVk,
                               const gl::Rectangle &viewport,
                               float nearPlane,
                               float farPlane,
                               bool invertViewport)
{
    VkViewport vkViewport;
    const gl::Caps &caps                   = getCaps();
    const VkPhysicalDeviceLimits &limitsVk = mRenderer->getPhysicalDeviceProperties().limits;
    const int viewportBoundsRangeLow       = static_cast<int>(limitsVk.viewportBoundsRange[0]);
    const int viewportBoundsRangeHigh      = static_cast<int>(limitsVk.viewportBoundsRange[1]);

    // Clamp the viewport values to what Vulkan specifies

    // width must be greater than 0.0 and less than or equal to
    // VkPhysicalDeviceLimits::maxViewportDimensions[0]
    int correctedWidth = std::min<int>(viewport.width, caps.maxViewportWidth);
    correctedWidth     = std::max<int>(correctedWidth, 0);
    // height must be greater than 0.0 and less than or equal to
    // VkPhysicalDeviceLimits::maxViewportDimensions[1]
    int correctedHeight = std::min<int>(viewport.height, caps.maxViewportHeight);
    correctedHeight     = std::max<int>(correctedHeight, 0);
    // x and y must each be between viewportBoundsRange[0] and viewportBoundsRange[1], inclusive
    int correctedX = std::min<int>(viewport.x, viewportBoundsRangeHigh);
    correctedX     = std::max<int>(correctedX, viewportBoundsRangeLow);
    int correctedY = std::min<int>(viewport.y, viewportBoundsRangeHigh);
    correctedY     = std::max<int>(correctedY, viewportBoundsRangeLow);
    // x + width must be less than or equal to viewportBoundsRange[1]
    if ((correctedX + correctedWidth) > viewportBoundsRangeHigh)
    {
        correctedWidth = viewportBoundsRangeHigh - correctedX;
    }
    // y + height must be less than or equal to viewportBoundsRange[1]
    if ((correctedY + correctedHeight) > viewportBoundsRangeHigh)
    {
        correctedHeight = viewportBoundsRangeHigh - correctedY;
    }

    gl::Box fbDimensions = framebufferVk->getState().getDimensions();
    gl::Rectangle correctedRect =
        gl::Rectangle(correctedX, correctedY, correctedWidth, correctedHeight);
    gl::Rectangle rotatedRect;
    RotateRectangle(getRotationDrawFramebuffer(), false, fbDimensions.width, fbDimensions.height,
                    correctedRect, &rotatedRect);
    gl_vk::GetViewport(rotatedRect, nearPlane, farPlane, invertViewport,
                       // If the surface is rotated 90/270 degrees, use the framebuffer's width
                       // instead of the height for calculating the final viewport.
                       isRotatedAspectRatioForDrawFBO() ? fbDimensions.width : fbDimensions.height,
                       &vkViewport);
    mGraphicsPipelineDesc->updateViewport(&mGraphicsPipelineTransition, vkViewport);
    invalidateGraphicsDriverUniforms();
}

void ContextVk::updateDepthRange(float nearPlane, float farPlane)
{
    invalidateGraphicsDriverUniforms();
    mGraphicsPipelineDesc->updateDepthRange(&mGraphicsPipelineTransition, nearPlane, farPlane);
}

angle::Result ContextVk::updateScissor(const gl::State &glState)
{
    FramebufferVk *framebufferVk = vk::GetImpl(glState.getDrawFramebuffer());
    gl::Rectangle renderArea     = framebufferVk->getCompleteRenderArea();

    // Clip the render area to the viewport.
    gl::Rectangle viewportClippedRenderArea;
    gl::ClipRectangle(renderArea, glState.getViewport(), &viewportClippedRenderArea);

    gl::Rectangle scissoredArea = ClipRectToScissor(getState(), viewportClippedRenderArea, false);
    gl::Rectangle rotatedScissoredArea;
    RotateRectangle(getRotationDrawFramebuffer(), isViewportFlipEnabledForDrawFBO(),
                    renderArea.width, renderArea.height, scissoredArea, &rotatedScissoredArea);

    mGraphicsPipelineDesc->updateScissor(&mGraphicsPipelineTransition,
                                         gl_vk::GetRect(rotatedScissoredArea));

    // If the scissor has grown beyond the previous scissoredRenderArea, make sure the render pass
    // is restarted.  Otherwise, we can continue using the same renderpass area.
    //
    // Without a scissor, the render pass area covers the whole of the framebuffer.  With a
    // scissored clear, the render pass area could be smaller than the framebuffer size.  When the
    // scissor changes, if the scissor area is completely encompassed by the render pass area, it's
    // possible to continue using the same render pass.  However, if the current render pass area
    // is too small, we need to start a new one.  The latter can happen if a scissored clear starts
    // a render pass, the scissor is disabled and a draw call is issued to affect the whole
    // framebuffer.
    gl::Rectangle scissoredRenderArea = framebufferVk->getScissoredRenderArea(this);
    if (!mRenderPassCommands->empty())
    {
        if (!mRenderPassCommands->getRenderArea().encloses(scissoredRenderArea))
        {
            ANGLE_TRY(endRenderPass());
        }
    }

    return angle::Result::Continue;
}

void ContextVk::invalidateProgramBindingHelper(const gl::State &glState)
{
    mProgram         = nullptr;
    mProgramPipeline = nullptr;
    mExecutable      = nullptr;

    if (glState.getProgram())
    {
        mProgram    = vk::GetImpl(glState.getProgram());
        mExecutable = &mProgram->getExecutable();
    }

    if (glState.getProgramPipeline())
    {
        mProgramPipeline = vk::GetImpl(glState.getProgramPipeline());
        if (!mExecutable)
        {
            // A bound program always overrides a program pipeline
            mExecutable = &mProgramPipeline->getExecutable();
        }
    }
}

angle::Result ContextVk::invalidateProgramExecutableHelper(const gl::Context *context)
{
    const gl::State &glState = context->getState();

    if (glState.getProgramExecutable()->isCompute())
    {
        invalidateCurrentComputePipeline();
    }
    else
    {
        // No additional work is needed here. We will update the pipeline desc
        // later.
        invalidateDefaultAttributes(context->getStateCache().getActiveDefaultAttribsMask());
        invalidateVertexAndIndexBuffers();
        bool useVertexBuffer = (glState.getProgramExecutable()->getMaxActiveAttribLocation() > 0);
        mNonIndexedDirtyBitsMask.set(DIRTY_BIT_VERTEX_BUFFERS, useVertexBuffer);
        mIndexedDirtyBitsMask.set(DIRTY_BIT_VERTEX_BUFFERS, useVertexBuffer);
        mCurrentGraphicsPipeline = nullptr;
        mGraphicsPipelineTransition.reset();
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::syncState(const gl::Context *context,
                                   const gl::State::DirtyBits &dirtyBits,
                                   const gl::State::DirtyBits &bitMask)
{
    const gl::State &glState                       = context->getState();
    const gl::ProgramExecutable *programExecutable = glState.getProgramExecutable();

    if ((dirtyBits & mPipelineDirtyBitsMask).any() &&
        (programExecutable == nullptr || !programExecutable->isCompute()))
    {
        invalidateCurrentGraphicsPipeline();
    }

    for (auto iter = dirtyBits.begin(), endIter = dirtyBits.end(); iter != endIter; ++iter)
    {
        size_t dirtyBit = *iter;
        switch (dirtyBit)
        {
            case gl::State::DIRTY_BIT_SCISSOR_TEST_ENABLED:
            case gl::State::DIRTY_BIT_SCISSOR:
                ANGLE_TRY(updateScissor(glState));
                break;
            case gl::State::DIRTY_BIT_VIEWPORT:
            {
                FramebufferVk *framebufferVk = vk::GetImpl(glState.getDrawFramebuffer());
                updateViewport(framebufferVk, glState.getViewport(), glState.getNearPlane(),
                               glState.getFarPlane(), isViewportFlipEnabledForDrawFBO());
                // Update the scissor, which will be constrained to the viewport
                ANGLE_TRY(updateScissor(glState));
                break;
            }
            case gl::State::DIRTY_BIT_DEPTH_RANGE:
                updateDepthRange(glState.getNearPlane(), glState.getFarPlane());
                break;
            case gl::State::DIRTY_BIT_BLEND_ENABLED:
                mGraphicsPipelineDesc->updateBlendEnabled(&mGraphicsPipelineTransition,
                                                          glState.isBlendEnabled());
                break;
            case gl::State::DIRTY_BIT_BLEND_COLOR:
                mGraphicsPipelineDesc->updateBlendColor(&mGraphicsPipelineTransition,
                                                        glState.getBlendColor());
                break;
            case gl::State::DIRTY_BIT_BLEND_FUNCS:
                mGraphicsPipelineDesc->updateBlendFuncs(&mGraphicsPipelineTransition,
                                                        glState.getBlendState());
                break;
            case gl::State::DIRTY_BIT_BLEND_EQUATIONS:
                mGraphicsPipelineDesc->updateBlendEquations(&mGraphicsPipelineTransition,
                                                            glState.getBlendState());
                break;
            case gl::State::DIRTY_BIT_COLOR_MASK:
                updateColorMask(glState.getBlendState());
                break;
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED:
                mGraphicsPipelineDesc->updateAlphaToCoverageEnable(
                    &mGraphicsPipelineTransition, glState.isSampleAlphaToCoverageEnabled());
                ASSERT(gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE >
                       gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED);
                iter.setLaterBit(gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE);
                break;
            case gl::State::DIRTY_BIT_SAMPLE_COVERAGE_ENABLED:
                updateSampleMask(glState);
                break;
            case gl::State::DIRTY_BIT_SAMPLE_COVERAGE:
                updateSampleMask(glState);
                break;
            case gl::State::DIRTY_BIT_SAMPLE_MASK_ENABLED:
                updateSampleMask(glState);
                break;
            case gl::State::DIRTY_BIT_SAMPLE_MASK:
                updateSampleMask(glState);
                break;
            case gl::State::DIRTY_BIT_DEPTH_TEST_ENABLED:
                mGraphicsPipelineDesc->updateDepthTestEnabled(&mGraphicsPipelineTransition,
                                                              glState.getDepthStencilState(),
                                                              glState.getDrawFramebuffer());
                break;
            case gl::State::DIRTY_BIT_DEPTH_FUNC:
                mGraphicsPipelineDesc->updateDepthFunc(&mGraphicsPipelineTransition,
                                                       glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_DEPTH_MASK:
                mGraphicsPipelineDesc->updateDepthWriteEnabled(&mGraphicsPipelineTransition,
                                                               glState.getDepthStencilState(),
                                                               glState.getDrawFramebuffer());
                break;
            case gl::State::DIRTY_BIT_STENCIL_TEST_ENABLED:
                mGraphicsPipelineDesc->updateStencilTestEnabled(&mGraphicsPipelineTransition,
                                                                glState.getDepthStencilState(),
                                                                glState.getDrawFramebuffer());
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_FRONT:
                mGraphicsPipelineDesc->updateStencilFrontFuncs(&mGraphicsPipelineTransition,
                                                               glState.getStencilRef(),
                                                               glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_BACK:
                mGraphicsPipelineDesc->updateStencilBackFuncs(&mGraphicsPipelineTransition,
                                                              glState.getStencilBackRef(),
                                                              glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_STENCIL_OPS_FRONT:
                mGraphicsPipelineDesc->updateStencilFrontOps(&mGraphicsPipelineTransition,
                                                             glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_STENCIL_OPS_BACK:
                mGraphicsPipelineDesc->updateStencilBackOps(&mGraphicsPipelineTransition,
                                                            glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT:
                mGraphicsPipelineDesc->updateStencilFrontWriteMask(&mGraphicsPipelineTransition,
                                                                   glState.getDepthStencilState(),
                                                                   glState.getDrawFramebuffer());
                break;
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_BACK:
                mGraphicsPipelineDesc->updateStencilBackWriteMask(&mGraphicsPipelineTransition,
                                                                  glState.getDepthStencilState(),
                                                                  glState.getDrawFramebuffer());
                break;
            case gl::State::DIRTY_BIT_CULL_FACE_ENABLED:
            case gl::State::DIRTY_BIT_CULL_FACE:
                mGraphicsPipelineDesc->updateCullMode(&mGraphicsPipelineTransition,
                                                      glState.getRasterizerState());
                break;
            case gl::State::DIRTY_BIT_FRONT_FACE:
                mGraphicsPipelineDesc->updateFrontFace(&mGraphicsPipelineTransition,
                                                       glState.getRasterizerState(),
                                                       isViewportFlipEnabledForDrawFBO());
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED:
                mGraphicsPipelineDesc->updatePolygonOffsetFillEnabled(
                    &mGraphicsPipelineTransition, glState.isPolygonOffsetFillEnabled());
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET:
                mGraphicsPipelineDesc->updatePolygonOffset(&mGraphicsPipelineTransition,
                                                           glState.getRasterizerState());
                break;
            case gl::State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED:
                mGraphicsPipelineDesc->updateRasterizerDiscardEnabled(
                    &mGraphicsPipelineTransition, glState.isRasterizerDiscardEnabled());
                break;
            case gl::State::DIRTY_BIT_LINE_WIDTH:
                mGraphicsPipelineDesc->updateLineWidth(&mGraphicsPipelineTransition,
                                                       glState.getLineWidth());
                break;
            case gl::State::DIRTY_BIT_PRIMITIVE_RESTART_ENABLED:
                mGraphicsPipelineDesc->updatePrimitiveRestartEnabled(
                    &mGraphicsPipelineTransition, glState.isPrimitiveRestartEnabled());
                break;
            case gl::State::DIRTY_BIT_CLEAR_COLOR:
                mClearColorValue.color.float32[0] = glState.getColorClearValue().red;
                mClearColorValue.color.float32[1] = glState.getColorClearValue().green;
                mClearColorValue.color.float32[2] = glState.getColorClearValue().blue;
                mClearColorValue.color.float32[3] = glState.getColorClearValue().alpha;
                break;
            case gl::State::DIRTY_BIT_CLEAR_DEPTH:
                mClearDepthStencilValue.depthStencil.depth = glState.getDepthClearValue();
                break;
            case gl::State::DIRTY_BIT_CLEAR_STENCIL:
                mClearDepthStencilValue.depthStencil.stencil =
                    static_cast<uint32_t>(glState.getStencilClearValue());
                break;
            case gl::State::DIRTY_BIT_UNPACK_STATE:
                // This is a no-op, it's only important to use the right unpack state when we do
                // setImage or setSubImage in TextureVk, which is plumbed through the frontend
                // call
                break;
            case gl::State::DIRTY_BIT_UNPACK_BUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_PACK_STATE:
                // This is a no-op, its only important to use the right pack state when we do
                // call readPixels later on.
                break;
            case gl::State::DIRTY_BIT_PACK_BUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_DITHER_ENABLED:
                break;
            case gl::State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING:
                updateFlipViewportReadFramebuffer(context->getState());
                updateSurfaceRotationReadFramebuffer(glState);
                break;
            case gl::State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING:
            {
                // FramebufferVk::syncState signals that we should start a new command buffer.
                // But changing the binding can skip FramebufferVk::syncState if the Framebuffer
                // has no dirty bits. Thus we need to explicitly clear the current command
                // buffer to ensure we start a new one. Note that we always start a new command
                // buffer because we currently can only support one open RenderPass at a time.
                onRenderPassFinished();

                gl::Framebuffer *drawFramebuffer = glState.getDrawFramebuffer();
                mDrawFramebuffer                 = vk::GetImpl(drawFramebuffer);
                updateFlipViewportDrawFramebuffer(glState);
                updateSurfaceRotationDrawFramebuffer(glState);
                updateViewport(mDrawFramebuffer, glState.getViewport(), glState.getNearPlane(),
                               glState.getFarPlane(), isViewportFlipEnabledForDrawFBO());
                updateColorMask(glState.getBlendState());
                updateSampleMask(glState);
                mGraphicsPipelineDesc->updateRasterizationSamples(&mGraphicsPipelineTransition,
                                                                  mDrawFramebuffer->getSamples());
                mGraphicsPipelineDesc->updateFrontFace(&mGraphicsPipelineTransition,
                                                       glState.getRasterizerState(),
                                                       isViewportFlipEnabledForDrawFBO());
                ANGLE_TRY(updateScissor(glState));
                const gl::DepthStencilState depthStencilState = glState.getDepthStencilState();
                mGraphicsPipelineDesc->updateDepthTestEnabled(&mGraphicsPipelineTransition,
                                                              depthStencilState, drawFramebuffer);
                mGraphicsPipelineDesc->updateDepthWriteEnabled(&mGraphicsPipelineTransition,
                                                               depthStencilState, drawFramebuffer);
                mGraphicsPipelineDesc->updateStencilTestEnabled(&mGraphicsPipelineTransition,
                                                                depthStencilState, drawFramebuffer);
                mGraphicsPipelineDesc->updateStencilFrontWriteMask(
                    &mGraphicsPipelineTransition, depthStencilState, drawFramebuffer);
                mGraphicsPipelineDesc->updateStencilBackWriteMask(
                    &mGraphicsPipelineTransition, depthStencilState, drawFramebuffer);
                mGraphicsPipelineDesc->updateRenderPassDesc(&mGraphicsPipelineTransition,
                                                            mDrawFramebuffer->getRenderPassDesc());
                break;
            }
            case gl::State::DIRTY_BIT_RENDERBUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_VERTEX_ARRAY_BINDING:
            {
                mVertexArray = vk::GetImpl(glState.getVertexArray());
                invalidateDefaultAttributes(context->getStateCache().getActiveDefaultAttribsMask());
                mVertexArray->updateActiveAttribInfo(this);
                setIndexBufferDirty();
                break;
            }
            case gl::State::DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_DISPATCH_INDIRECT_BUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_PROGRAM_BINDING:
                invalidateProgramBindingHelper(glState);
                break;
            case gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE:
            {
                ASSERT(programExecutable);
                invalidateCurrentDefaultUniforms();
                ASSERT(gl::State::DIRTY_BIT_TEXTURE_BINDINGS >
                       gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE);
                iter.setLaterBit(gl::State::DIRTY_BIT_TEXTURE_BINDINGS);
                invalidateCurrentShaderResources();
                ANGLE_TRY(invalidateProgramExecutableHelper(context));
                break;
            }
            case gl::State::DIRTY_BIT_SAMPLER_BINDINGS:
            {
                ASSERT(gl::State::DIRTY_BIT_TEXTURE_BINDINGS >
                       gl::State::DIRTY_BIT_SAMPLER_BINDINGS);
                iter.setLaterBit(gl::State::DIRTY_BIT_TEXTURE_BINDINGS);
                break;
            }
            case gl::State::DIRTY_BIT_TEXTURE_BINDINGS:
                ANGLE_TRY(invalidateCurrentTextures(context));
                break;
            case gl::State::DIRTY_BIT_TRANSFORM_FEEDBACK_BINDING:
                // Nothing to do.
                break;
            case gl::State::DIRTY_BIT_SHADER_STORAGE_BUFFER_BINDING:
                invalidateCurrentShaderResources();
                break;
            case gl::State::DIRTY_BIT_UNIFORM_BUFFER_BINDINGS:
                invalidateCurrentShaderResources();
                break;
            case gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING:
                invalidateCurrentShaderResources();
                invalidateDriverUniforms();
                break;
            case gl::State::DIRTY_BIT_IMAGE_BINDINGS:
                invalidateCurrentShaderResources();
                break;
            case gl::State::DIRTY_BIT_MULTISAMPLING:
                // TODO(syoussefi): this should configure the pipeline to render as if
                // single-sampled, and write the results to all samples of a pixel regardless of
                // coverage. See EXT_multisample_compatibility.  http://anglebug.com/3204
                break;
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_ONE:
                // TODO(syoussefi): this is part of EXT_multisample_compatibility.  The
                // alphaToOne Vulkan feature should be enabled to support this extension.
                // http://anglebug.com/3204
                mGraphicsPipelineDesc->updateAlphaToOneEnable(&mGraphicsPipelineTransition,
                                                              glState.isSampleAlphaToOneEnabled());
                break;
            case gl::State::DIRTY_BIT_COVERAGE_MODULATION:
                break;
            case gl::State::DIRTY_BIT_FRAMEBUFFER_SRGB:
                break;
            case gl::State::DIRTY_BIT_CURRENT_VALUES:
            {
                invalidateDefaultAttributes(glState.getAndResetDirtyCurrentValues());
                break;
            }
            case gl::State::DIRTY_BIT_PROVOKING_VERTEX:
                break;
            case gl::State::DIRTY_BIT_EXTENDED:
                // Handling clip distance enabled flags, mipmap generation hint & shader derivative
                // hint.
                invalidateGraphicsDriverUniforms();
                break;
            default:
                UNREACHABLE();
                break;
        }
    }

    return angle::Result::Continue;
}

GLint ContextVk::getGPUDisjoint()
{
    // No extension seems to be available to query this information.
    return 0;
}

GLint64 ContextVk::getTimestamp()
{
    // This function should only be called if timestamp queries are available.
    ASSERT(mRenderer->getQueueFamilyProperties().timestampValidBits > 0);

    uint64_t timestamp = 0;

    (void)getTimestamp(&timestamp);

    return static_cast<GLint64>(timestamp);
}

angle::Result ContextVk::onMakeCurrent(const gl::Context *context)
{
    mRenderer->reloadVolkIfNeeded();

    // Flip viewports if FeaturesVk::flipViewportY is enabled and the user did not request that
    // the surface is flipped.
    egl::Surface *drawSurface = context->getCurrentDrawSurface();
    mFlipYForCurrentSurface =
        drawSurface != nullptr && mRenderer->getFeatures().flipViewportY.enabled &&
        !IsMaskFlagSet(drawSurface->getOrientation(), EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE);

    if (drawSurface && drawSurface->getType() == EGL_WINDOW_BIT)
    {
        mCurrentWindowSurface = GetImplAs<WindowSurfaceVk>(drawSurface);
    }
    else
    {
        mCurrentWindowSurface = nullptr;
    }

    const gl::State &glState = context->getState();
    updateFlipViewportDrawFramebuffer(glState);
    updateFlipViewportReadFramebuffer(glState);
    updateSurfaceRotationDrawFramebuffer(glState);
    updateSurfaceRotationReadFramebuffer(glState);
    invalidateDriverUniforms();

    return angle::Result::Continue;
}

angle::Result ContextVk::onUnMakeCurrent(const gl::Context *context)
{
    ANGLE_TRY(flushImpl(nullptr));
    mCurrentWindowSurface = nullptr;
    return angle::Result::Continue;
}

void ContextVk::updateFlipViewportDrawFramebuffer(const gl::State &glState)
{
    gl::Framebuffer *drawFramebuffer = glState.getDrawFramebuffer();
    mFlipViewportForDrawFramebuffer =
        drawFramebuffer->isDefault() && mRenderer->getFeatures().flipViewportY.enabled;
}

void ContextVk::updateFlipViewportReadFramebuffer(const gl::State &glState)
{
    gl::Framebuffer *readFramebuffer = glState.getReadFramebuffer();
    mFlipViewportForReadFramebuffer =
        readFramebuffer->isDefault() && mRenderer->getFeatures().flipViewportY.enabled;
}

void ContextVk::updateSurfaceRotationDrawFramebuffer(const gl::State &glState)
{
    gl::Framebuffer *drawFramebuffer = glState.getDrawFramebuffer();
    mCurrentRotationDrawFramebuffer =
        DetermineSurfaceRotation(drawFramebuffer, mCurrentWindowSurface);
}

void ContextVk::updateSurfaceRotationReadFramebuffer(const gl::State &glState)
{
    gl::Framebuffer *readFramebuffer = glState.getReadFramebuffer();
    mCurrentRotationReadFramebuffer =
        DetermineSurfaceRotation(readFramebuffer, mCurrentWindowSurface);
}

gl::Caps ContextVk::getNativeCaps() const
{
    return mRenderer->getNativeCaps();
}

const gl::TextureCapsMap &ContextVk::getNativeTextureCaps() const
{
    return mRenderer->getNativeTextureCaps();
}

const gl::Extensions &ContextVk::getNativeExtensions() const
{
    return mRenderer->getNativeExtensions();
}

const gl::Limitations &ContextVk::getNativeLimitations() const
{
    return mRenderer->getNativeLimitations();
}

CompilerImpl *ContextVk::createCompiler()
{
    return new CompilerVk();
}

ShaderImpl *ContextVk::createShader(const gl::ShaderState &state)
{
    return new ShaderVk(state);
}

ProgramImpl *ContextVk::createProgram(const gl::ProgramState &state)
{
    return new ProgramVk(state);
}

FramebufferImpl *ContextVk::createFramebuffer(const gl::FramebufferState &state)
{
    return FramebufferVk::CreateUserFBO(mRenderer, state);
}

TextureImpl *ContextVk::createTexture(const gl::TextureState &state)
{
    return new TextureVk(state, mRenderer);
}

RenderbufferImpl *ContextVk::createRenderbuffer(const gl::RenderbufferState &state)
{
    return new RenderbufferVk(state);
}

BufferImpl *ContextVk::createBuffer(const gl::BufferState &state)
{
    return new BufferVk(state);
}

VertexArrayImpl *ContextVk::createVertexArray(const gl::VertexArrayState &state)
{
    return new VertexArrayVk(this, state);
}

QueryImpl *ContextVk::createQuery(gl::QueryType type)
{
    return new QueryVk(type);
}

FenceNVImpl *ContextVk::createFenceNV()
{
    return new FenceNVVk();
}

SyncImpl *ContextVk::createSync()
{
    return new SyncVk();
}

TransformFeedbackImpl *ContextVk::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedbackVk(state);
}

SamplerImpl *ContextVk::createSampler(const gl::SamplerState &state)
{
    return new SamplerVk(state);
}

ProgramPipelineImpl *ContextVk::createProgramPipeline(const gl::ProgramPipelineState &state)
{
    return new ProgramPipelineVk(state);
}

MemoryObjectImpl *ContextVk::createMemoryObject()
{
    return new MemoryObjectVk();
}

SemaphoreImpl *ContextVk::createSemaphore()
{
    return new SemaphoreVk();
}

OverlayImpl *ContextVk::createOverlay(const gl::OverlayState &state)
{
    return new OverlayVk(state);
}

void ContextVk::invalidateCurrentDefaultUniforms()
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    if (executable->hasDefaultUniforms())
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
        mComputeDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }
}

angle::Result ContextVk::invalidateCurrentTextures(const gl::Context *context)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    if (executable->hasTextures())
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_TEXTURES);
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
        mComputeDirtyBits.set(DIRTY_BIT_TEXTURES);
        mComputeDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);

        ANGLE_TRY(updateActiveTextures(context));
    }

    return angle::Result::Continue;
}

void ContextVk::invalidateCurrentShaderResources()
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    if (executable->hasUniformBuffers() || executable->hasStorageBuffers() ||
        executable->hasAtomicCounterBuffers() || executable->hasImages())
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
        mComputeDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);
        mComputeDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }
}

void ContextVk::invalidateGraphicsDriverUniforms()
{
    mGraphicsDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS);
    mGraphicsDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS_BINDING);
}

void ContextVk::invalidateDriverUniforms()
{
    mGraphicsDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS);
    mGraphicsDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS_BINDING);
    mComputeDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS);
    mComputeDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS_BINDING);
}

void ContextVk::onDrawFramebufferChange(FramebufferVk *framebufferVk)
{
    const vk::RenderPassDesc &renderPassDesc = framebufferVk->getRenderPassDesc();

    // Ensure that the RenderPass description is updated.
    invalidateCurrentGraphicsPipeline();
    if (mGraphicsPipelineDesc->getRasterizationSamples() !=
        static_cast<uint32_t>(framebufferVk->getSamples()))
    {
        mGraphicsPipelineDesc->updateRasterizationSamples(&mGraphicsPipelineTransition,
                                                          framebufferVk->getSamples());
    }
    mGraphicsPipelineDesc->updateRenderPassDesc(&mGraphicsPipelineTransition, renderPassDesc);
}

void ContextVk::invalidateCurrentTransformFeedbackBuffers()
{
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS);
    }
    if (getFeatures().emulateTransformFeedback.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }
}

void ContextVk::onTransformFeedbackStateChanged()
{
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_STATE);
    }
    else if (getFeatures().emulateTransformFeedback.enabled)
    {
        invalidateGraphicsDriverUniforms();
    }
}

void ContextVk::invalidateGraphicsDescriptorSet(uint32_t usedDescriptorSet)
{
    // UtilsVk currently only uses set 0
    ASSERT(usedDescriptorSet == kDriverUniformsDescriptorSetIndex);
    if (mDriverUniforms[PipelineType::Graphics].descriptorSet != VK_NULL_HANDLE)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS_BINDING);
    }
}

void ContextVk::invalidateComputeDescriptorSet(uint32_t usedDescriptorSet)
{
    // UtilsVk currently only uses set 0
    ASSERT(usedDescriptorSet == kDriverUniformsDescriptorSetIndex);
    if (mDriverUniforms[PipelineType::Compute].descriptorSet != VK_NULL_HANDLE)
    {
        mComputeDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS_BINDING);
    }
}

angle::Result ContextVk::dispatchCompute(const gl::Context *context,
                                         GLuint numGroupsX,
                                         GLuint numGroupsY,
                                         GLuint numGroupsZ)
{
    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(setupDispatch(context, &commandBuffer));

    commandBuffer->dispatch(numGroupsX, numGroupsY, numGroupsZ);

    return angle::Result::Continue;
}

angle::Result ContextVk::dispatchComputeIndirect(const gl::Context *context, GLintptr indirect)
{
    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(setupDispatch(context, &commandBuffer));

    gl::Buffer *glBuffer     = getState().getTargetBuffer(gl::BufferBinding::DispatchIndirect);
    vk::BufferHelper &buffer = vk::GetImpl(glBuffer)->getBuffer();
    mOutsideRenderPassCommands->bufferRead(&mResourceUseList, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                           vk::PipelineStage::DrawIndirect, &buffer);

    commandBuffer->dispatchIndirect(buffer.getBuffer(), indirect);

    return angle::Result::Continue;
}

angle::Result ContextVk::memoryBarrier(const gl::Context *context, GLbitfield barriers)
{
    return memoryBarrierImpl(barriers, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

angle::Result ContextVk::memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers)
{
    // Note: memoryBarrierByRegion is expected to affect only the fragment pipeline, but is
    // otherwise similar to memoryBarrier.

    return memoryBarrierImpl(barriers, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

angle::Result ContextVk::memoryBarrierImpl(GLbitfield barriers, VkPipelineStageFlags stageMask)
{
    // Note: many of the barriers specified here are already covered automatically.
    //
    // The barriers that are necessary all have SHADER_WRITE as src access and the dst access is
    // determined by the given bitfield.  Currently, all image-related barriers that require the
    // image to change usage are handled through image layout transitions.  Most buffer-related
    // barriers where the buffer usage changes are also handled automatically through dirty bits.
    // The only barriers that are necessary are thus barriers in situations where the resource can
    // be written to and read from without changing the bindings.

    VkAccessFlags srcAccess = 0;
    VkAccessFlags dstAccess = 0;

    // Both IMAGE_ACCESS and STORAGE barrier flags translate to the same Vulkan dst access mask.
    constexpr GLbitfield kShaderWriteBarriers =
        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;

    if ((barriers & kShaderWriteBarriers) != 0)
    {
        srcAccess |= VK_ACCESS_SHADER_WRITE_BIT;
        dstAccess |= VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    }

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(endRenderPassAndGetCommandBuffer(&commandBuffer));

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask   = srcAccess;
    memoryBarrier.dstAccessMask   = dstAccess;

    commandBuffer->memoryBarrier(stageMask, stageMask, &memoryBarrier);

    return angle::Result::Continue;
}

vk::DynamicQueryPool *ContextVk::getQueryPool(gl::QueryType queryType)
{
    ASSERT(queryType == gl::QueryType::AnySamples ||
           queryType == gl::QueryType::AnySamplesConservative ||
           queryType == gl::QueryType::Timestamp || queryType == gl::QueryType::TimeElapsed);

    // Assert that timestamp extension is available if needed.
    ASSERT(queryType != gl::QueryType::Timestamp && queryType != gl::QueryType::TimeElapsed ||
           mRenderer->getQueueFamilyProperties().timestampValidBits > 0);
    ASSERT(mQueryPools[queryType].isValid());
    return &mQueryPools[queryType];
}

const VkClearValue &ContextVk::getClearColorValue() const
{
    return mClearColorValue;
}

const VkClearValue &ContextVk::getClearDepthStencilValue() const
{
    return mClearDepthStencilValue;
}

VkColorComponentFlags ContextVk::getClearColorMask() const
{
    return mClearColorMask;
}

void ContextVk::writeAtomicCounterBufferDriverUniformOffsets(uint32_t *offsetsOut,
                                                             size_t offsetsSize)
{
    const VkDeviceSize offsetAlignment =
        mRenderer->getPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
    size_t atomicCounterBufferCount = mState.getAtomicCounterBufferCount();

    ASSERT(atomicCounterBufferCount <= offsetsSize * 4);

    for (uint32_t bufferIndex = 0; bufferIndex < atomicCounterBufferCount; ++bufferIndex)
    {
        uint32_t offsetDiff = 0;

        const gl::OffsetBindingPointer<gl::Buffer> *atomicCounterBuffer =
            &mState.getIndexedAtomicCounterBuffer(bufferIndex);
        if (atomicCounterBuffer->get())
        {
            VkDeviceSize offset        = atomicCounterBuffer->getOffset();
            VkDeviceSize alignedOffset = (offset / offsetAlignment) * offsetAlignment;

            // GL requires the atomic counter buffer offset to be aligned with uint.
            ASSERT((offset - alignedOffset) % sizeof(uint32_t) == 0);
            offsetDiff = static_cast<uint32_t>((offset - alignedOffset) / sizeof(uint32_t));

            // We expect offsetDiff to fit in an 8-bit value.  The maximum difference is
            // minStorageBufferOffsetAlignment / 4, where minStorageBufferOffsetAlignment
            // currently has a maximum value of 256 on any device.
            ASSERT(offsetDiff < (1 << 8));
        }

        // The output array is already cleared prior to this call.
        ASSERT(bufferIndex % 4 != 0 || offsetsOut[bufferIndex / 4] == 0);

        offsetsOut[bufferIndex / 4] |= static_cast<uint8_t>(offsetDiff) << ((bufferIndex % 4) * 8);
    }
}

angle::Result ContextVk::handleDirtyGraphicsDriverUniforms(const gl::Context *context,
                                                           vk::CommandBuffer *commandBuffer)
{
    // Allocate a new region in the dynamic buffer.
    uint8_t *ptr;
    VkBuffer buffer;
    bool newBuffer;
    ANGLE_TRY(allocateDriverUniforms(sizeof(GraphicsDriverUniforms),
                                     &mDriverUniforms[PipelineType::Graphics], &buffer, &ptr,
                                     &newBuffer));

    gl::Rectangle glViewport = mState.getViewport();
    float halfRenderAreaHeight =
        static_cast<float>(mDrawFramebuffer->getState().getDimensions().height) * 0.5f;
    if (isRotatedAspectRatioForDrawFBO())
    {
        // The surface is rotated 90/270 degrees.  This changes the aspect ratio of the surface.
        // TODO(ianelliott): handle small viewport/scissor cases.  http://anglebug.com/4431
        std::swap(glViewport.width, glViewport.height);
        halfRenderAreaHeight =
            static_cast<float>(mDrawFramebuffer->getState().getDimensions().width) * 0.5f;
    }
    float scaleY = isViewportFlipEnabledForDrawFBO() ? -1.0f : 1.0f;

    uint32_t xfbActiveUnpaused = mState.isTransformFeedbackActiveUnpaused();

    float depthRangeNear = mState.getNearPlane();
    float depthRangeFar  = mState.getFarPlane();
    float depthRangeDiff = depthRangeFar - depthRangeNear;

    // Copy and flush to the device.
    GraphicsDriverUniforms *driverUniforms = reinterpret_cast<GraphicsDriverUniforms *>(ptr);
    *driverUniforms                        = {
        {static_cast<float>(glViewport.x), static_cast<float>(glViewport.y),
         static_cast<float>(glViewport.width), static_cast<float>(glViewport.height)},
        halfRenderAreaHeight,
        scaleY,
        -scaleY,
        mState.getEnabledClipDistances().bits(),
        xfbActiveUnpaused,
        mXfbVertexCountPerInstance,
        {},
        {},
        {},
        {depthRangeNear, depthRangeFar, depthRangeDiff, 0.0f},
        {}};
    memcpy(&driverUniforms->preRotation, &kPreRotationMatrices[mCurrentRotationDrawFramebuffer],
           sizeof(PreRotationMatrixValues));

    if (xfbActiveUnpaused)
    {
        TransformFeedbackVk *transformFeedbackVk =
            vk::GetImpl(mState.getCurrentTransformFeedback());
        transformFeedbackVk->getBufferOffsets(this, mXfbBaseVertex,
                                              driverUniforms->xfbBufferOffsets.data(),
                                              driverUniforms->xfbBufferOffsets.size());
    }

    writeAtomicCounterBufferDriverUniformOffsets(driverUniforms->acbBufferOffsets.data(),
                                                 driverUniforms->acbBufferOffsets.size());

    return updateDriverUniformsDescriptorSet(buffer, newBuffer, sizeof(GraphicsDriverUniforms),
                                             &mDriverUniforms[PipelineType::Graphics]);
}

angle::Result ContextVk::handleDirtyComputeDriverUniforms(const gl::Context *context,
                                                          vk::CommandBuffer *commandBuffer)
{
    // Allocate a new region in the dynamic buffer.
    uint8_t *ptr;
    VkBuffer buffer;
    bool newBuffer;
    ANGLE_TRY(allocateDriverUniforms(sizeof(ComputeDriverUniforms),
                                     &mDriverUniforms[PipelineType::Compute], &buffer, &ptr,
                                     &newBuffer));

    // Copy and flush to the device.
    ComputeDriverUniforms *driverUniforms = reinterpret_cast<ComputeDriverUniforms *>(ptr);
    *driverUniforms                       = {};

    writeAtomicCounterBufferDriverUniformOffsets(driverUniforms->acbBufferOffsets.data(),
                                                 driverUniforms->acbBufferOffsets.size());

    return updateDriverUniformsDescriptorSet(buffer, newBuffer, sizeof(ComputeDriverUniforms),
                                             &mDriverUniforms[PipelineType::Compute]);
}

void ContextVk::handleDirtyDriverUniformsBindingImpl(
    vk::CommandBuffer *commandBuffer,
    VkPipelineBindPoint bindPoint,
    const DriverUniformsDescriptorSet &driverUniforms)
{
    commandBuffer->bindDescriptorSets(
        mExecutable->getPipelineLayout(), bindPoint, kDriverUniformsDescriptorSetIndex, 1,
        &driverUniforms.descriptorSet, 1, &driverUniforms.dynamicOffset);
}

angle::Result ContextVk::handleDirtyGraphicsDriverUniformsBinding(const gl::Context *context,
                                                                  vk::CommandBuffer *commandBuffer)
{
    // Bind the driver descriptor set.
    handleDirtyDriverUniformsBindingImpl(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                         mDriverUniforms[PipelineType::Graphics]);
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyComputeDriverUniformsBinding(const gl::Context *context,
                                                                 vk::CommandBuffer *commandBuffer)
{
    // Bind the driver descriptor set.
    handleDirtyDriverUniformsBindingImpl(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                         mDriverUniforms[PipelineType::Compute]);
    return angle::Result::Continue;
}

angle::Result ContextVk::allocateDriverUniforms(size_t driverUniformsSize,
                                                DriverUniformsDescriptorSet *driverUniforms,
                                                VkBuffer *bufferOut,
                                                uint8_t **ptrOut,
                                                bool *newBufferOut)
{
    // Release any previously retained buffers.
    driverUniforms->dynamicBuffer.releaseInFlightBuffers(this);

    // Allocate a new region in the dynamic buffer.
    VkDeviceSize offset;
    ANGLE_TRY(driverUniforms->dynamicBuffer.allocate(this, driverUniformsSize, ptrOut, bufferOut,
                                                     &offset, newBufferOut));

    driverUniforms->dynamicOffset = static_cast<uint32_t>(offset);

    return angle::Result::Continue;
}

angle::Result ContextVk::updateDriverUniformsDescriptorSet(
    VkBuffer buffer,
    bool newBuffer,
    size_t driverUniformsSize,
    DriverUniformsDescriptorSet *driverUniforms)
{
    ANGLE_TRY(driverUniforms->dynamicBuffer.flush(this));

    if (!newBuffer)
    {
        return angle::Result::Continue;
    }

    // Allocate a new descriptor set.
    ANGLE_TRY(mDriverUniformsDescriptorPool.allocateSets(
        this, driverUniforms->descriptorSetLayout.get().ptr(), 1,
        &driverUniforms->descriptorPoolBinding, &driverUniforms->descriptorSet));

    // Update the driver uniform descriptor set.
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer                 = buffer;
    bufferInfo.offset                 = 0;
    bufferInfo.range                  = driverUniformsSize;

    VkWriteDescriptorSet writeInfo = {};
    writeInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet               = driverUniforms->descriptorSet;
    writeInfo.dstBinding           = 0;
    writeInfo.dstArrayElement      = 0;
    writeInfo.descriptorCount      = 1;
    writeInfo.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writeInfo.pImageInfo           = nullptr;
    writeInfo.pTexelBufferView     = nullptr;
    writeInfo.pBufferInfo          = &bufferInfo;

    vkUpdateDescriptorSets(getDevice(), 1, &writeInfo, 0, nullptr);

    return angle::Result::Continue;
}

void ContextVk::handleError(VkResult errorCode,
                            const char *file,
                            const char *function,
                            unsigned int line)
{
    ASSERT(errorCode != VK_SUCCESS);

    GLenum glErrorCode = DefaultGLErrorCode(errorCode);

    std::stringstream errorStream;
    errorStream << "Internal Vulkan error (" << errorCode << "): " << VulkanResultString(errorCode)
                << ".";

    if (errorCode == VK_ERROR_DEVICE_LOST)
    {
        WARN() << errorStream.str();
        handleDeviceLost();
    }

    mErrors->handleError(glErrorCode, errorStream.str().c_str(), file, function, line);
}

angle::Result ContextVk::updateActiveTextures(const gl::Context *context)
{
    const gl::State &glState                = mState;
    const gl::ProgramExecutable *executable = glState.getProgramExecutable();
    ASSERT(executable);

    uint32_t prevMaxIndex = mActiveTexturesDesc.getMaxIndex();
    memset(mActiveTextures.data(), 0, sizeof(mActiveTextures[0]) * prevMaxIndex);
    mActiveTexturesDesc.reset();

    const gl::ActiveTexturesCache &textures        = glState.getActiveTexturesCache();
    const gl::ActiveTextureMask &activeTextures    = executable->getActiveSamplersMask();
    const gl::ActiveTextureTypeArray &textureTypes = executable->getActiveSamplerTypes();

    for (size_t textureUnit : activeTextures)
    {
        gl::Texture *texture        = textures[textureUnit];
        gl::Sampler *sampler        = mState.getSampler(static_cast<uint32_t>(textureUnit));
        gl::TextureType textureType = textureTypes[textureUnit];

        // Null textures represent incomplete textures.
        if (texture == nullptr)
        {
            ANGLE_TRY(getIncompleteTexture(context, textureType, &texture));
        }

        TextureVk *textureVk = vk::GetImpl(texture);

        SamplerVk *samplerVk;
        Serial samplerSerial;
        if (sampler == nullptr)
        {
            samplerVk     = nullptr;
            samplerSerial = rx::kZeroSerial;
        }
        else
        {
            samplerVk     = vk::GetImpl(sampler);
            samplerSerial = samplerVk->getSerial();
        }

        mActiveTextures[textureUnit].texture = textureVk;
        mActiveTextures[textureUnit].sampler = samplerVk;
        // Cache serials from sampler and texture, but re-use texture if no sampler bound
        ASSERT(textureVk != nullptr);
        mActiveTexturesDesc.update(textureUnit, textureVk->getSerial(), samplerSerial);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::updateActiveImages(const gl::Context *context,
                                            vk::CommandBufferHelper *commandBufferHelper)
{
    const gl::State &glState                = mState;
    const gl::ProgramExecutable *executable = glState.getProgramExecutable();
    ASSERT(executable);

    mActiveImages.fill(nullptr);

    const gl::ActiveTextureMask &activeImages = executable->getActiveImagesMask();
    const gl::ActiveTextureArray<gl::ShaderBitSet> &activeImageShaderBits =
        executable->getActiveImageShaderBits();

    // Note: currently, the image layout is transitioned entirely even if only one level or layer is
    // used.  This is an issue if one subresource of the image is used as framebuffer attachment and
    // the other as image.  This is a similar issue to http://anglebug.com/2914.  Another issue
    // however is if multiple subresources of the same image are used at the same time.
    // Inefficiencies aside, setting write dependency on the same image multiple times is not
    // supported.  The following makes sure write dependencies are set only once per image.
    std::set<vk::ImageHelper *> alreadyProcessed;

    for (size_t imageUnitIndex : activeImages)
    {
        const gl::ImageUnit &imageUnit = glState.getImageUnit(imageUnitIndex);
        const gl::Texture *texture     = imageUnit.texture.get();
        if (texture == nullptr)
        {
            continue;
        }

        TextureVk *textureVk   = vk::GetImpl(texture);
        vk::ImageHelper *image = &textureVk->getImage();

        mActiveImages[imageUnitIndex] = textureVk;

        if (alreadyProcessed.find(image) != alreadyProcessed.end())
        {
            continue;
        }
        alreadyProcessed.insert(image);

        // The image should be flushed and ready to use at this point. There may still be
        // lingering staged updates in its staging buffer for unused texture mip levels or
        // layers. Therefore we can't verify it has no staged updates right here.
        vk::ImageLayout imageLayout;
        gl::ShaderBitSet shaderBits = activeImageShaderBits[imageUnitIndex];
        if (shaderBits.any())
        {
            gl::ShaderType shader = static_cast<gl::ShaderType>(gl::ScanForward(shaderBits.bits()));
            shaderBits.reset(shader);
            // This is accessed by multiple shaders
            if (shaderBits.any())
            {
                imageLayout = vk::ImageLayout::AllGraphicsShadersReadWrite;
            }
            else
            {
                imageLayout = kShaderWriteImageLayouts[shader];
            }
        }
        else
        {
            imageLayout = vk::ImageLayout::AllGraphicsShadersReadWrite;
        }
        VkImageAspectFlags aspectFlags = image->getAspectFlags();

        commandBufferHelper->imageWrite(&mResourceUseList, aspectFlags, imageLayout, image);
    }

    return angle::Result::Continue;
}

void ContextVk::insertWaitSemaphore(const vk::Semaphore *waitSemaphore)
{
    ASSERT(waitSemaphore);
    mWaitSemaphores.push_back(waitSemaphore->getHandle());
}

bool ContextVk::shouldFlush()
{
    return getRenderer()->shouldCleanupGarbage();
}

bool ContextVk::hasRecordedCommands()
{
    return !mOutsideRenderPassCommands->empty() || !mRenderPassCommands->empty() ||
           mHasPrimaryCommands;
}

angle::Result ContextVk::flushImpl(const vk::Semaphore *signalSemaphore)
{
    bool hasPendingSemaphore = signalSemaphore || !mWaitSemaphores.empty();
    if (!hasRecordedCommands() && !hasPendingSemaphore && !mGpuEventsEnabled)
    {
        return angle::Result::Continue;
    }

    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::flush");

    ANGLE_TRY(flushOutsideRenderPassCommands());
    ANGLE_TRY(endRenderPass());

    if (mIsAnyHostVisibleBufferWritten)
    {
        // Make sure all writes to host-visible buffers are flushed.  We have no way of knowing
        // whether any buffer will be mapped for readback in the future, and we can't afford to
        // flush and wait on a one-pipeline-barrier command buffer on every map().
        VkMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT;
        memoryBarrier.dstAccessMask   = VK_ACCESS_HOST_READ_BIT;

        mOutsideRenderPassCommands->getCommandBuffer().memoryBarrier(
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, &memoryBarrier);
        mIsAnyHostVisibleBufferWritten = false;
    }

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("Primary", mPrimaryBufferCounter);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_END, eventName));
    }
    ANGLE_TRY(flushOutsideRenderPassCommands());
    ANGLE_VK_TRY(this, mPrimaryCommands.end());

    // Free secondary command pool allocations and restart command buffers with the new page.
    mPoolAllocator.pop();
    mPoolAllocator.push();
    mOutsideRenderPassCommands->reset();
    mRenderPassCommands->reset();

    Serial serial = getCurrentQueueSerial();
    mResourceUseList.releaseResourceUsesAndUpdateSerials(serial);

    waitForSwapchainImageIfNecessary();

    VkSubmitInfo submitInfo = {};
    InitializeSubmitInfo(&submitInfo, mPrimaryCommands, mWaitSemaphores, &mWaitSemaphoreStageMasks,
                         signalSemaphore);

    ANGLE_TRY(submitFrame(submitInfo, std::move(mPrimaryCommands)));

    ANGLE_TRY(startPrimaryCommandBuffer());

    mRenderPassCounter = 0;
    mWaitSemaphores.clear();

    mPrimaryBufferCounter++;

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("Primary", mPrimaryBufferCounter);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_BEGIN, eventName));
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::finishImpl()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::finish");

    ANGLE_TRY(flushImpl(nullptr));

    ANGLE_TRY(finishToSerial(getLastSubmittedQueueSerial()));
    ASSERT(!mCommandQueue.hasInFlightCommands());

    clearAllGarbage();

    if (mGpuEventsEnabled)
    {
        // This loop should in practice execute once since the queue is already idle.
        while (mInFlightGpuEventQueries.size() > 0)
        {
            ANGLE_TRY(checkCompletedGpuEvents());
        }
        // Recalculate the CPU/GPU time difference to account for clock drifting.  Avoid
        // unnecessary synchronization if there is no event to be adjusted (happens when
        // finish() gets called multiple times towards the end of the application).
        if (mGpuEvents.size() > 0)
        {
            ANGLE_TRY(synchronizeCpuGpuTime());
        }
    }

    return angle::Result::Continue;
}

void ContextVk::addWaitSemaphore(VkSemaphore semaphore)
{
    mWaitSemaphores.push_back(semaphore);
}

const vk::CommandPool &ContextVk::getCommandPool() const
{
    return mCommandPool;
}

bool ContextVk::isSerialInUse(Serial serial) const
{
    return serial > getLastCompletedQueueSerial();
}

angle::Result ContextVk::checkCompletedCommands()
{
    return mCommandQueue.checkCompletedCommands(this);
}

angle::Result ContextVk::finishToSerial(Serial serial)
{
    return mCommandQueue.finishToSerial(this, serial, mRenderer->getMaxFenceWaitTimeNs());
}

angle::Result ContextVk::getCompatibleRenderPass(const vk::RenderPassDesc &desc,
                                                 vk::RenderPass **renderPassOut)
{
    return mRenderPassCache.getCompatibleRenderPass(this, getCurrentQueueSerial(), desc,
                                                    renderPassOut);
}

angle::Result ContextVk::getRenderPassWithOps(const vk::RenderPassDesc &desc,
                                              const vk::AttachmentOpsArray &ops,
                                              vk::RenderPass **renderPassOut)
{
    return mRenderPassCache.getRenderPassWithOps(this, getCurrentQueueSerial(), desc, ops,
                                                 renderPassOut);
}

angle::Result ContextVk::ensureSubmitFenceInitialized()
{
    if (mSubmitFence.isReferenced())
    {
        return angle::Result::Continue;
    }

    return mRenderer->newSharedFence(this, &mSubmitFence);
}

angle::Result ContextVk::getNextSubmitFence(vk::Shared<vk::Fence> *sharedFenceOut)
{
    ANGLE_TRY(ensureSubmitFenceInitialized());

    ASSERT(!sharedFenceOut->isReferenced());
    sharedFenceOut->copy(getDevice(), mSubmitFence);
    return angle::Result::Continue;
}

vk::Shared<vk::Fence> ContextVk::getLastSubmittedFence() const
{
    return mCommandQueue.getLastSubmittedFence(this);
}

angle::Result ContextVk::getTimestamp(uint64_t *timestampOut)
{
    // The intent of this function is to query the timestamp without stalling the GPU.
    // Currently, that seems impossible, so instead, we are going to make a small submission
    // with just a timestamp query.  First, the disjoint timer query extension says:
    //
    // > This will return the GL time after all previous commands have reached the GL server but
    // have not yet necessarily executed.
    //
    // The previous commands may be deferred at the moment and not yet flushed. The wording allows
    // us to make a submission to get the timestamp without flushing.
    //
    // Second:
    //
    // > By using a combination of this synchronous get command and the asynchronous timestamp
    // query object target, applications can measure the latency between when commands reach the
    // GL server and when they are realized in the framebuffer.
    //
    // This fits with the above strategy as well, although inevitably we are possibly
    // introducing a GPU bubble.  This function directly generates a command buffer and submits
    // it instead of using the other member functions.  This is to avoid changing any state,
    // such as the queue serial.

    // Create a query used to receive the GPU timestamp
    VkDevice device = getDevice();
    vk::DeviceScoped<vk::DynamicQueryPool> timestampQueryPool(device);
    vk::QueryHelper timestampQuery;
    ANGLE_TRY(timestampQueryPool.get().init(this, VK_QUERY_TYPE_TIMESTAMP, 1));
    ANGLE_TRY(timestampQueryPool.get().allocateQuery(this, &timestampQuery));

    // Record the command buffer
    vk::DeviceScoped<vk::PrimaryCommandBuffer> commandBatch(device);
    vk::PrimaryCommandBuffer &commandBuffer = commandBatch.get();

    ANGLE_TRY(mCommandQueue.allocatePrimaryCommandBuffer(this, mCommandPool, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = nullptr;

    ANGLE_VK_TRY(this, commandBuffer.begin(beginInfo));
    timestampQuery.writeTimestamp(this, &commandBuffer);
    ANGLE_VK_TRY(this, commandBuffer.end());

    // Create fence for the submission
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = 0;

    vk::DeviceScoped<vk::Fence> fence(device);
    ANGLE_VK_TRY(this, fence.get().init(device, fenceInfo));

    // Submit the command buffer
    VkSubmitInfo submitInfo         = {};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 0;
    submitInfo.pWaitSemaphores      = nullptr;
    submitInfo.pWaitDstStageMask    = nullptr;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = commandBuffer.ptr();
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores    = nullptr;

    Serial throwAwaySerial;
    ANGLE_TRY(
        mRenderer->queueSubmit(this, mContextPriority, submitInfo, &fence.get(), &throwAwaySerial));

    // Wait for the submission to finish.  Given no semaphores, there is hope that it would execute
    // in parallel with what's already running on the GPU.
    ANGLE_VK_TRY(this, fence.get().wait(device, mRenderer->getMaxFenceWaitTimeNs()));

    // Get the query results
    ANGLE_TRY(timestampQuery.getUint64Result(this, timestampOut));
    timestampQueryPool.get().freeQuery(this, &timestampQuery);

    // Convert results to nanoseconds.
    *timestampOut = static_cast<uint64_t>(
        *timestampOut *
        static_cast<double>(getRenderer()->getPhysicalDeviceProperties().limits.timestampPeriod));

    return mCommandQueue.releasePrimaryCommandBuffer(this, commandBatch.release());
}

void ContextVk::invalidateDefaultAttribute(size_t attribIndex)
{
    mDirtyDefaultAttribsMask.set(attribIndex);
    mGraphicsDirtyBits.set(DIRTY_BIT_DEFAULT_ATTRIBS);
}

void ContextVk::invalidateDefaultAttributes(const gl::AttributesMask &dirtyMask)
{
    if (dirtyMask.any())
    {
        mDirtyDefaultAttribsMask |= dirtyMask;
        mGraphicsDirtyBits.set(DIRTY_BIT_DEFAULT_ATTRIBS);
    }
}

angle::Result ContextVk::updateDefaultAttribute(size_t attribIndex)
{
    vk::DynamicBuffer &defaultBuffer = mDefaultAttribBuffers[attribIndex];

    defaultBuffer.releaseInFlightBuffers(this);

    uint8_t *ptr;
    VkBuffer bufferHandle = VK_NULL_HANDLE;
    VkDeviceSize offset   = 0;
    ANGLE_TRY(
        defaultBuffer.allocate(this, kDefaultValueSize, &ptr, &bufferHandle, &offset, nullptr));

    const gl::State &glState = mState;
    const gl::VertexAttribCurrentValueData &defaultValue =
        glState.getVertexAttribCurrentValues()[attribIndex];
    memcpy(ptr, &defaultValue.Values, kDefaultValueSize);
    ASSERT(!defaultBuffer.isCoherent());
    ANGLE_TRY(defaultBuffer.flush(this));

    mVertexArray->updateDefaultAttrib(this, attribIndex, bufferHandle,
                                      defaultBuffer.getCurrentBuffer(),
                                      static_cast<uint32_t>(offset));
    return angle::Result::Continue;
}

void ContextVk::waitForSwapchainImageIfNecessary()
{
    if (mCurrentWindowSurface)
    {
        vk::Semaphore waitSemaphore = mCurrentWindowSurface->getAcquireImageSemaphore();
        if (waitSemaphore.valid())
        {
            addWaitSemaphore(waitSemaphore.getHandle());
            addGarbage(&waitSemaphore);
        }
    }
}

vk::DescriptorSetLayoutDesc ContextVk::getDriverUniformsDescriptorSetDesc(
    VkShaderStageFlags shaderStages) const
{
    vk::DescriptorSetLayoutDesc desc;
    desc.update(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, shaderStages);
    return desc;
}

bool ContextVk::shouldEmulateSeamfulCubeMapSampling() const
{
    // Only allow seamful cube map sampling in non-webgl ES2.
    if (mState.getClientMajorVersion() != 2 || mState.isWebGL())
    {
        return false;
    }

    if (mRenderer->getFeatures().disallowSeamfulCubeMapEmulation.enabled)
    {
        return false;
    }

    return true;
}

bool ContextVk::shouldUseOldRewriteStructSamplers() const
{
    return mRenderer->getFeatures().forceOldRewriteStructSamplers.enabled;
}

angle::Result ContextVk::onBufferRead(VkAccessFlags readAccessType,
                                      vk::PipelineStage readStage,
                                      vk::BufferHelper *buffer)
{
    ASSERT(!buffer->isReleasedToExternal());

    ANGLE_TRY(endRenderPass());

    if (!buffer->canAccumulateRead(this, readAccessType))
    {
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    mOutsideRenderPassCommands->bufferRead(&mResourceUseList, readAccessType, readStage, buffer);

    return angle::Result::Continue;
}

angle::Result ContextVk::onBufferWrite(VkAccessFlags writeAccessType,
                                       vk::PipelineStage writeStage,
                                       vk::BufferHelper *buffer)
{
    ASSERT(!buffer->isReleasedToExternal());

    ANGLE_TRY(endRenderPass());

    if (!buffer->canAccumulateWrite(this, writeAccessType))
    {
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    mOutsideRenderPassCommands->bufferWrite(&mResourceUseList, writeAccessType, writeStage, buffer);

    return angle::Result::Continue;
}

angle::Result ContextVk::onImageRead(VkImageAspectFlags aspectFlags,
                                     vk::ImageLayout imageLayout,
                                     vk::ImageHelper *image)
{
    ASSERT(!image->isReleasedToExternal());

    ANGLE_TRY(endRenderPass());

    if (image->isLayoutChangeNecessary(imageLayout))
    {
        vk::CommandBuffer *commandBuffer;
        ANGLE_TRY(endRenderPassAndGetCommandBuffer(&commandBuffer));
        image->changeLayout(aspectFlags, imageLayout, commandBuffer);
    }
    image->retain(&mResourceUseList);
    return angle::Result::Continue;
}

angle::Result ContextVk::onImageWrite(VkImageAspectFlags aspectFlags,
                                      vk::ImageLayout imageLayout,
                                      vk::ImageHelper *image)
{
    ASSERT(!image->isReleasedToExternal());

    ANGLE_TRY(endRenderPass());

    // Barriers are always required for image writes.
    ASSERT(image->isLayoutChangeNecessary(imageLayout));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(endRenderPassAndGetCommandBuffer(&commandBuffer));

    image->changeLayout(aspectFlags, imageLayout, commandBuffer);
    image->retain(&mResourceUseList);

    return angle::Result::Continue;
}

angle::Result ContextVk::flushAndBeginRenderPass(
    const vk::Framebuffer &framebuffer,
    const gl::Rectangle &renderArea,
    const vk::RenderPassDesc &renderPassDesc,
    const vk::AttachmentOpsArray &renderPassAttachmentOps,
    const vk::ClearValuesArray &clearValues,
    vk::CommandBuffer **commandBufferOut)
{
    // Flush any outside renderPass commands first
    ANGLE_TRY(flushOutsideRenderPassCommands());
    // Next end any currently outstanding renderPass
    vk::CommandBuffer *outsideRenderPassCommandBuffer;
    ANGLE_TRY(endRenderPassAndGetCommandBuffer(&outsideRenderPassCommandBuffer));

    gl::Rectangle rotatedRenderArea = renderArea;
    if (isRotatedAspectRatioForDrawFBO())
    {
        // The surface is rotated 90/270 degrees.  This changes the aspect ratio of
        // the surface.  Swap the width and height of the renderArea.
        // TODO(ianelliott): handle small viewport/scissor cases.  http://anglebug.com/4431
        std::swap(rotatedRenderArea.width, rotatedRenderArea.height);
    }

    mRenderPassCommands->beginRenderPass(framebuffer, rotatedRenderArea, renderPassDesc,
                                         renderPassAttachmentOps, clearValues, commandBufferOut);
    return angle::Result::Continue;
}

angle::Result ContextVk::startRenderPass(gl::Rectangle renderArea,
                                         vk::CommandBuffer **commandBufferOut)
{
    mGraphicsDirtyBits |= mNewGraphicsCommandBufferDirtyBits;
    ANGLE_TRY(mDrawFramebuffer->startNewRenderPass(this, renderArea, &mRenderPassCommandBuffer));
    if (mActiveQueryAnySamples)
    {
        mActiveQueryAnySamples->getQueryHelper()->beginOcclusionQuery(this,
                                                                      mRenderPassCommandBuffer);
    }
    if (mActiveQueryAnySamplesConservative)
    {
        mActiveQueryAnySamplesConservative->getQueryHelper()->beginOcclusionQuery(
            this, mRenderPassCommandBuffer);
    }

    if (commandBufferOut)
    {
        *commandBufferOut = mRenderPassCommandBuffer;
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::endRenderPass()
{
    if (mRenderPassCommands->empty())
    {
        onRenderPassFinished();
        return angle::Result::Continue;
    }

    ASSERT(mOutsideRenderPassCommands->empty());
    if (mActiveQueryAnySamples)
    {
        mActiveQueryAnySamples->getQueryHelper()->endOcclusionQuery(this, mRenderPassCommandBuffer);
        ANGLE_TRY(mActiveQueryAnySamples->stashQueryHelper(this));
    }
    if (mActiveQueryAnySamplesConservative)
    {
        mActiveQueryAnySamplesConservative->getQueryHelper()->endOcclusionQuery(
            this, mRenderPassCommandBuffer);
        ANGLE_TRY(mActiveQueryAnySamplesConservative->stashQueryHelper(this));
    }

    onRenderPassFinished();

    if (mGpuEventsEnabled)
    {
        mRenderPassCounter++;

        EventName eventName = GetTraceEventName("RP", mRenderPassCounter);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_BEGIN, eventName));
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    mRenderPassCommands->pauseTransformFeedbackIfStarted();

    ANGLE_TRY(mRenderPassCommands->flushToPrimary(this, &mPrimaryCommands));
    mHasPrimaryCommands = true;

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("RP", mRenderPassCounter);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_END, eventName));
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    return angle::Result::Continue;
}

void ContextVk::onRenderPassImageWrite(VkImageAspectFlags aspectFlags,
                                       vk::ImageLayout imageLayout,
                                       vk::ImageHelper *image)
{
    mRenderPassCommands->imageWrite(&mResourceUseList, aspectFlags, imageLayout, image);
}

angle::Result ContextVk::syncExternalMemory()
{
    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(endRenderPassAndGetCommandBuffer(&commandBuffer));

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    commandBuffer->memoryBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &memoryBarrier);
    return angle::Result::Continue;
}

void ContextVk::addCommandBufferDiagnostics(const std::string &commandBufferDiagnostics)
{
    mCommandBufferDiagnostics.push_back(commandBufferDiagnostics);
}

void ContextVk::dumpCommandStreamDiagnostics()
{
    std::ostream &out = std::cout;

    if (mCommandBufferDiagnostics.empty())
        return;

    out << "digraph {\n"
        << "  node [shape=plaintext fontname=\"Consolas\"]\n";

    for (size_t index = 0; index < mCommandBufferDiagnostics.size(); ++index)
    {
        const std::string &payload = mCommandBufferDiagnostics[index];
        out << "  cb" << index << " [label =\"" << payload << "\"];\n";
    }

    for (size_t index = 0; index < mCommandBufferDiagnostics.size() - 1; ++index)
    {
        out << "  cb" << index << " -> cb" << index + 1 << "\n";
    }

    mCommandBufferDiagnostics.clear();

    out << "}\n";
}

void ContextVk::initIndexTypeMap()
{
    // Init gles-vulkan index type map
    mIndexTypeMap[gl::DrawElementsType::UnsignedByte] =
        mRenderer->getFeatures().supportsIndexTypeUint8.enabled ? VK_INDEX_TYPE_UINT8_EXT
                                                                : VK_INDEX_TYPE_UINT16;
    mIndexTypeMap[gl::DrawElementsType::UnsignedShort] = VK_INDEX_TYPE_UINT16;
    mIndexTypeMap[gl::DrawElementsType::UnsignedInt]   = VK_INDEX_TYPE_UINT32;
}

VkIndexType ContextVk::getVkIndexType(gl::DrawElementsType glIndexType) const
{
    return mIndexTypeMap[glIndexType];
}

size_t ContextVk::getVkIndexTypeSize(gl::DrawElementsType glIndexType) const
{
    gl::DrawElementsType elementsType = shouldConvertUint8VkIndexType(glIndexType)
                                            ? gl::DrawElementsType::UnsignedShort
                                            : glIndexType;
    ASSERT(elementsType < gl::DrawElementsType::EnumCount);

    // Use GetDrawElementsTypeSize() to get the size
    return static_cast<size_t>(gl::GetDrawElementsTypeSize(elementsType));
}

bool ContextVk::shouldConvertUint8VkIndexType(gl::DrawElementsType glIndexType) const
{
    return (glIndexType == gl::DrawElementsType::UnsignedByte &&
            !mRenderer->getFeatures().supportsIndexTypeUint8.enabled);
}

angle::Result ContextVk::flushOutsideRenderPassCommands()
{
    if (!mOutsideRenderPassCommands->empty())
    {
        ANGLE_TRY(mOutsideRenderPassCommands->flushToPrimary(this, &mPrimaryCommands));
        mHasPrimaryCommands = true;
    }
    return angle::Result::Continue;
}

void ContextVk::beginOcclusionQuery(QueryVk *queryVk)
{
    // To avoid complexity, we always start and end occlusion query inside renderpass. if renderpass
    // not yet started, we just remember it and defer the start call.
    if (mRenderPassCommands->started())
    {
        queryVk->getQueryHelper()->beginOcclusionQuery(this, mRenderPassCommandBuffer);
    }
    if (queryVk->isAnySamplesQuery())
    {
        ASSERT(mActiveQueryAnySamples == nullptr);
        mActiveQueryAnySamples = queryVk;
    }
    else if (queryVk->isAnySamplesConservativeQuery())
    {
        ASSERT(mActiveQueryAnySamplesConservative == nullptr);
        mActiveQueryAnySamplesConservative = queryVk;
    }
    else
    {
        UNREACHABLE();
    }
}

void ContextVk::endOcclusionQuery(QueryVk *queryVk)
{
    if (mRenderPassCommands->started())
    {
        queryVk->getQueryHelper()->endOcclusionQuery(this, mRenderPassCommandBuffer);
    }
    if (queryVk->isAnySamplesQuery())
    {
        ASSERT(mActiveQueryAnySamples == queryVk);
        mActiveQueryAnySamples = nullptr;
    }
    else if (queryVk->isAnySamplesConservativeQuery())
    {
        ASSERT(mActiveQueryAnySamplesConservative == queryVk);
        mActiveQueryAnySamplesConservative = nullptr;
    }
    else
    {
        UNREACHABLE();
    }
}

bool ContextVk::isRobustResourceInitEnabled() const
{
    return mState.isRobustResourceInitEnabled();
}

}  // namespace rx
