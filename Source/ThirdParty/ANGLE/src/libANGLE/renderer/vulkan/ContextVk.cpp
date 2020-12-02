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
#include "libANGLE/Display.h"
#include "libANGLE/Program.h"
#include "libANGLE/Semaphore.h"
#include "libANGLE/Surface.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/CompilerVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
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
// For DesciptorSetUpdates
constexpr size_t kDescriptorBufferInfosInitialSize = 8;
constexpr size_t kDescriptorImageInfosInitialSize  = 4;
constexpr size_t kDescriptorWriteInfosInitialSize =
    kDescriptorBufferInfosInitialSize + kDescriptorImageInfosInitialSize;

// For shader uniforms such as gl_DepthRange and the viewport size.
struct GraphicsDriverUniforms
{
    std::array<float, 4> viewport;

    // Used to flip gl_FragCoord (both .xy for Android pre-rotation; only .y for desktop)
    std::array<float, 2> halfRenderArea;
    std::array<float, 2> flipXY;
    std::array<float, 2> negFlipXY;

    // 32 bits for 32 clip planes
    uint32_t enabledClipPlanes;

    uint32_t xfbActiveUnpaused;
    uint32_t xfbVerticesPerDraw;
    std::array<int32_t, 3> padding;

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
    // Used to pre-rotate gl_FragCoord for swapchain images on Android (a mat2, which is padded to
    // the size of two vec4's).
    std::array<float, 8> fragRotation;
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

constexpr VkBufferUsageFlags kVertexBufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
constexpr size_t kDefaultValueSize              = sizeof(gl::VertexAttribCurrentValueData::Values);
constexpr size_t kDefaultBufferSize             = kDefaultValueSize * 16;
constexpr size_t kDriverUniformsAllocatorPageSize = 4 * 1024;

uint32_t GetCoverageSampleCount(const gl::State &glState, FramebufferVk *drawFramebuffer)
{
    if (!glState.isSampleCoverageEnabled())
    {
        return 0;
    }

    // Get a fraction of the samples based on the coverage parameters.
    // There are multiple ways to obtain an integer value from a float -
    //     truncation, ceil and round
    //
    // round() provides a more even distribution of values but doesn't seem to play well
    // with all vendors (AMD). A way to work around this is to increase the comparison threshold
    // of deqp tests. Though this takes care of deqp tests other apps would still have issues.
    //
    // Truncation provides an uneven distribution near the edges of the interval but seems to
    // play well with all vendors.
    //
    // We are going with truncation for expediency.
    return static_cast<uint32_t>(glState.getSampleCoverageValue() * drawFramebuffer->getSamples());
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
// rotate gl_Position in the vertex shader and gl_FragCoord in the fragment shader.  The following
// are the rotation matrices used.
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

constexpr angle::PackedEnumMap<rx::SurfaceRotation,
                               PreRotationMatrixValues,
                               angle::EnumSize<rx::SurfaceRotation>()>
    kFragRotationMatrices = {
        {{rx::SurfaceRotation::Identity, {{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::Rotated90Degrees,
          {{0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::Rotated180Degrees,
          {{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::Rotated270Degrees,
          {{0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::FlippedIdentity, {{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::FlippedRotated90Degrees,
          {{0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f}}},
         {rx::SurfaceRotation::FlippedRotated180Degrees,
          {{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}}},
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

// Should not generate a copy with modern C++.
EventName GetTraceEventName(const char *title, uint32_t counter)
{
    EventName buf;
    snprintf(buf.data(), kMaxGpuEventNameLen - 1, "%s %u", title, counter);
    return buf;
}

vk::ResourceAccess GetDepthAccess(const gl::DepthStencilState &dsState)
{
    if (!dsState.depthTest)
    {
        return vk::ResourceAccess::Unused;
    }
    return dsState.isDepthMaskedOut() ? vk::ResourceAccess::ReadOnly : vk::ResourceAccess::Write;
}

vk::ResourceAccess GetStencilAccess(const gl::DepthStencilState &dsState)
{
    if (!dsState.stencilTest)
    {
        return vk::ResourceAccess::Unused;
    }

    return dsState.isStencilNoOp() && dsState.isStencilBackNoOp() ? vk::ResourceAccess::ReadOnly
                                                                  : vk::ResourceAccess::Write;
}

egl::ContextPriority GetContextPriority(const gl::State &state)
{
    return egl::FromEGLenum<egl::ContextPriority>(state.getContextPriority());
}
}  // anonymous namespace

ANGLE_INLINE void ContextVk::flushDescriptorSetUpdates()
{
    if (mWriteDescriptorSets.empty())
    {
        ASSERT(mDescriptorBufferInfos.empty());
        ASSERT(mDescriptorImageInfos.empty());
        return;
    }

    vkUpdateDescriptorSets(getDevice(), static_cast<uint32_t>(mWriteDescriptorSets.size()),
                           mWriteDescriptorSets.data(), 0, nullptr);
    mWriteDescriptorSets.clear();
    mDescriptorBufferInfos.clear();
    mDescriptorImageInfos.clear();
}

// ContextVk::ScopedDescriptorSetUpdates implementation.
class ContextVk::ScopedDescriptorSetUpdates final : angle::NonCopyable
{
  public:
    ANGLE_INLINE ScopedDescriptorSetUpdates(ContextVk *contextVk) : mContextVk(contextVk) {}
    ANGLE_INLINE ~ScopedDescriptorSetUpdates() { mContextVk->flushDescriptorSetUpdates(); }

  private:
    ContextVk *mContextVk;
};

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
    descriptorSetCache.clear();
}

void ContextVk::DriverUniformsDescriptorSet::destroy(RendererVk *renderer)
{
    descriptorSetLayout.reset();
    descriptorPoolBinding.reset();
    dynamicBuffer.destroy(renderer);
    descriptorSetCache.clear();
}

// ContextVk implementation.
ContextVk::ContextVk(const gl::State &state, gl::ErrorSet *errorSet, RendererVk *renderer)
    : ContextImpl(state, errorSet),
      vk::Context(renderer),
      mGraphicsDirtyBitHandlers{},
      mComputeDirtyBitHandlers{},
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
      mClearColorMasks(0),
      mFlipYForCurrentSurface(false),
      mIsAnyHostVisibleBufferWritten(false),
      mEmulateSeamfulCubeMapSampling(false),
      mUseOldRewriteStructSamplers(false),
      mOutsideRenderPassCommands(nullptr),
      mRenderPassCommands(nullptr),
      mGpuEventsEnabled(false),
      mSyncObjectPendingFlush(false),
      mDeferredFlushCount(0),
      mGpuClockSync{std::numeric_limits<double>::max(), std::numeric_limits<double>::max()},
      mGpuEventTimestampOrigin(0),
      mPerfCounters{},
      mObjectPerfCounters{},
      mContextPriority(renderer->getDriverPriority(GetContextPriority(state))),
      mCurrentIndirectBuffer(nullptr),
      mShareGroupVk(vk::GetImpl(state.getShareGroup()))
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
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS_BINDING);

    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_PIPELINE);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_TEXTURES);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS_BINDING);

    mGraphicsDirtyBitHandlers[DIRTY_BIT_EVENT_LOG] = &ContextVk::handleDirtyGraphicsEventLog;
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

    mComputeDirtyBitHandlers[DIRTY_BIT_EVENT_LOG] = &ContextVk::handleDirtyGraphicsEventLog;
    mComputeDirtyBitHandlers[DIRTY_BIT_PIPELINE]  = &ContextVk::handleDirtyComputePipeline;
    mComputeDirtyBitHandlers[DIRTY_BIT_TEXTURES]  = &ContextVk::handleDirtyComputeTextures;
    mComputeDirtyBitHandlers[DIRTY_BIT_DRIVER_UNIFORMS] =
        &ContextVk::handleDirtyComputeDriverUniforms;
    mComputeDirtyBitHandlers[DIRTY_BIT_DRIVER_UNIFORMS_BINDING] =
        &ContextVk::handleDirtyComputeDriverUniformsBinding;
    mComputeDirtyBitHandlers[DIRTY_BIT_SHADER_RESOURCES] =
        &ContextVk::handleDirtyComputeShaderResources;
    mComputeDirtyBitHandlers[DIRTY_BIT_DESCRIPTOR_SETS] = &ContextVk::handleDirtyDescriptorSets;

    mGraphicsDirtyBits = mNewGraphicsCommandBufferDirtyBits;
    mComputeDirtyBits  = mNewComputeCommandBufferDirtyBits;

    mActiveTextures.fill({nullptr, nullptr, true});
    mActiveImages.fill(nullptr);

    mPipelineDirtyBitsMask.set();
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_TEXTURE_BINDINGS);

    // Reserve reasonable amount of spaces so that for majority of apps we don't need to grow at all
    mDescriptorBufferInfos.reserve(kDescriptorBufferInfosInitialSize);
    mDescriptorImageInfos.reserve(kDescriptorImageInfosInitialSize);
    mWriteDescriptorSets.reserve(kDescriptorWriteInfosInitialSize);

    mObjectPerfCounters.descriptorSetsAllocated.fill(0);
}

ContextVk::~ContextVk() = default;

void ContextVk::onDestroy(const gl::Context *context)
{
    outputCumulativePerfCounters();

    // Remove context from the share group
    mShareGroupVk->getShareContextSet()->erase(this);

    // This will not destroy any resources. It will release them to be collected after finish.
    mIncompleteTextures.onDestroy(context);

    // Flush and complete current outstanding work before destruction.
    (void)finishImpl();

    VkDevice device = getDevice();

    for (DriverUniformsDescriptorSet &driverUniforms : mDriverUniforms)
    {
        driverUniforms.destroy(mRenderer);
    }

    for (vk::DynamicDescriptorPool &dynamicDescriptorPool : mDriverUniformsDescriptorPools)
    {
        dynamicDescriptorPool.destroy(device);
    }

    mDefaultUniformStorage.release(mRenderer);
    mEmptyBuffer.release(mRenderer);
    mStagingBuffer.release(mRenderer);

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

    mRenderer->releaseSharedResources(&mResourceUseList);

    mUtils.destroy(mRenderer);

    mRenderPassCache.destroy(device);
    mSubmitFence.reset(device);
    mShaderLibrary.destroy(device);
    mGpuEventQueryPool.destroy(device);
    mCommandPool.destroy(device);

    // This will clean up any outstanding buffer allocations
    (void)mRenderer->cleanupGarbage(false);
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
        ANGLE_TRY(getDescriptorSetLayoutCache().getDescriptorSetLayout(
            this, desc, &mDriverUniforms[pipeline].descriptorSetLayout));

        vk::DescriptorSetLayoutBindingVector bindingVector;
        std::vector<VkSampler> immutableSamplers;
        desc.unpackBindings(&bindingVector, &immutableSamplers);
        std::vector<VkDescriptorPoolSize> descriptorPoolSizes;

        for (const VkDescriptorSetLayoutBinding &binding : bindingVector)
        {
            if (binding.descriptorCount > 0)
            {
                VkDescriptorPoolSize poolSize = {};

                poolSize.type            = binding.descriptorType;
                poolSize.descriptorCount = binding.descriptorCount;
                descriptorPoolSizes.emplace_back(poolSize);
            }
        }
        if (!descriptorPoolSizes.empty())
        {
            ANGLE_TRY(mDriverUniformsDescriptorPools[pipeline].init(
                this, descriptorPoolSizes.data(), descriptorPoolSizes.size(),
                mDriverUniforms[pipeline].descriptorSetLayout.get().getHandle()));
        }
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

    // Prepare command buffer queue by:
    //  1. Initializing each command buffer (as non-renderpass initially)
    //  2. Put a pointer to each command buffer into queue
    for (vk::CommandBufferHelper &commandBuffer : mCommandBuffers)
    {
        commandBuffer.initialize(false);
        recycleCommandBuffer(&commandBuffer);
    }
    // Now assign initial command buffers from queue
    getNextAvailableCommandBuffer(&mOutsideRenderPassCommands, false);
    getNextAvailableCommandBuffer(&mRenderPassCommands, true);

    if (mGpuEventsEnabled)
    {
        // GPU events should only be available if timestamp queries are available.
        ASSERT(mRenderer->getQueueFamilyProperties().timestampValidBits > 0);
        // Calculate the difference between CPU and GPU clocks for GPU event reporting.
        ANGLE_TRY(mGpuEventQueryPool.init(this, VK_QUERY_TYPE_TIMESTAMP,
                                          vk::kDefaultTimestampQueryPoolSize));
        ANGLE_TRY(synchronizeCpuGpuTime());

        mPerfCounters.primaryBuffers++;

        EventName eventName = GetTraceEventName("Primary", mPerfCounters.primaryBuffers);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_BEGIN, eventName));
    }

    size_t minAlignment = static_cast<size_t>(
        mRenderer->getPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment);
    mDefaultUniformStorage.init(mRenderer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, minAlignment,
                                mRenderer->getDefaultUniformBufferSize(), true);

    // Initialize an "empty" buffer for use with default uniform blocks where there are no uniforms,
    // or atomic counter buffer array indices that are unused.
    constexpr VkBufferUsageFlags kEmptyBufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkBufferCreateInfo emptyBufferInfo          = {};
    emptyBufferInfo.sType                       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    emptyBufferInfo.flags                       = 0;
    emptyBufferInfo.size                        = 16;
    emptyBufferInfo.usage                       = kEmptyBufferUsage;
    emptyBufferInfo.sharingMode                 = VK_SHARING_MODE_EXCLUSIVE;
    emptyBufferInfo.queueFamilyIndexCount       = 0;
    emptyBufferInfo.pQueueFamilyIndices         = nullptr;
    constexpr VkMemoryPropertyFlags kMemoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ANGLE_TRY(mEmptyBuffer.init(this, emptyBufferInfo, kMemoryType));

    constexpr VkImageUsageFlags kStagingBufferUsageFlags =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    size_t stagingBufferAlignment =
        static_cast<size_t>(mRenderer->getPhysicalDeviceProperties().limits.minMemoryMapAlignment);
    constexpr size_t kStagingBufferSize = 1024u * 1024u;  // 1M
    mStagingBuffer.init(mRenderer, kStagingBufferUsageFlags, stagingBufferAlignment,
                        kStagingBufferSize, true);

    // Add context into the share group
    mShareGroupVk->getShareContextSet()->insert(this);

    return angle::Result::Continue;
}

angle::Result ContextVk::flush(const gl::Context *context)
{
    // If we are in middle of renderpass and it is not a shared context, then we will defer the
    // glFlush call here until the renderpass ends. If sync object has been used, we must respect
    // glFlush call, otherwise we a wait for sync object without GL_SYNC_FLUSH_COMMANDS_BIT may
    // never come back.
    if (mRenderer->getFeatures().deferFlushUntilEndRenderPass.enabled && !context->isShared() &&
        !mSyncObjectPendingFlush && hasStartedRenderPass())
    {
        mDeferredFlushCount++;
        return angle::Result::Continue;
    }

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
        gl::Rectangle scissoredRenderArea = mDrawFramebuffer->getRotatedScissoredRenderArea(this);
        ANGLE_TRY(startRenderPass(scissoredRenderArea, nullptr));
    }

    // We keep a local copy of the command buffer. It's possible that some state changes could
    // trigger a command buffer invalidation. The local copy ensures we retain the reference.
    // Command buffers are pool allocated and only deleted after submit. Thus we know the
    // command buffer will still be valid for the duration of this API call.
    *commandBufferOut = mRenderPassCommandBuffer;
    ASSERT(*commandBufferOut);

    // Create a local object to ensure we flush the descriptor updates to device when we leave this
    // function
    ScopedDescriptorSetUpdates descriptorSetUpdates(this);

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
        ANGLE_TRY(onIndexBufferChange(nullptr));
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
            ANGLE_PERF_WARNING(getDebug(), GL_DEBUG_SEVERITY_LOW,
                               "Potential inefficiency emulating uint8 vertex attributes due to "
                               "lack of hardware support");

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

    if (indirectBuffer != mCurrentIndirectBuffer)
    {
        ANGLE_TRY(flushCommandsAndEndRenderPass());
        mCurrentIndirectBuffer = indirectBuffer;
    }

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
        ANGLE_TRY(onIndexBufferChange(nullptr));
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
        ANGLE_TRY(onIndexBufferChange(nullptr));
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
        ANGLE_TRY(onIndexBufferChange(nullptr));
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
    ANGLE_TRY(onIndexBufferChange(nullptr));
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
    ANGLE_TRY(flushCommandsAndEndRenderPass());
    *commandBufferOut = &mOutsideRenderPassCommands->getCommandBuffer();

    // Create a local object to ensure we flush the descriptor updates to device when we leave this
    // function
    ScopedDescriptorSetUpdates descriptorSetUpdates(this);

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

angle::Result ContextVk::handleDirtyGraphicsEventLog(const gl::Context *context,
                                                     vk::CommandBuffer *commandBuffer)
{
    if (mEventLog.empty())
    {
        return angle::Result::Continue;
    }

    // Insert OpenGL ES commands into debug label.  We create a 3-level cascade here for
    // OpenGL-ES-first debugging in AGI.  Here's the general outline of commands:
    // -glDrawCommand
    // --vkCmdBeginDebugUtilsLabelEXT() #1 for "glDrawCommand"
    // --OpenGL ES Commands
    // ---vkCmdBeginDebugUtilsLabelEXT() #2 for "OpenGL ES Commands"
    // ---Individual OpenGL ES Commands leading up to glDrawCommand
    // ----vkCmdBeginDebugUtilsLabelEXT() #3 for each individual OpenGL ES Command
    // ----vkCmdEndDebugUtilsLabelEXT() #3 for each individual OpenGL ES Command
    // ----...More Individual OGL Commands...
    // ----Final Individual OGL command will be the same glDrawCommand shown in #1 above
    // ---vkCmdEndDebugUtilsLabelEXT() #2 for "OpenGL ES Commands"
    // --VK SetupDraw & Draw-related commands will be embedded here under glDraw #1
    // --vkCmdEndDebugUtilsLabelEXT() #1 is called after each vkDraw* or vkDispatch* call
    VkDebugUtilsLabelEXT label = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                                  nullptr,
                                  mEventLog.back().c_str(),
                                  {0.0f, 0.0f, 0.0f, 0.0f}};
    // This is #1 from comment above
    commandBuffer->beginDebugUtilsLabelEXT(label);
    std::string oglCmds = "OpenGL ES Commands";
    label.pLabelName    = oglCmds.c_str();
    // This is #2 from comment above
    commandBuffer->beginDebugUtilsLabelEXT(label);
    for (uint32_t i = 0; i < mEventLog.size(); ++i)
    {
        label.pLabelName = mEventLog[i].c_str();
        // NOTE: We have to use a begin/end pair here because AGI does not promote the
        // pLabelName from an insertDebugUtilsLabelEXT() call to the Commands panel.
        // Internal bug b/169243237 is tracking this and once the insert* call shows the
        // pLabelName similar to begin* call, we can switch these to insert* calls instead.
        // This is #3 from comment above.
        commandBuffer->beginDebugUtilsLabelEXT(label);
        commandBuffer->endDebugUtilsLabelEXT();
    }
    commandBuffer->endDebugUtilsLabelEXT();
    // The final end* call for #1 above is made in the ContextVk::draw* or
    //  ContextVk::dispatch* function calls.

    mEventLog.clear();
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

    resumeTransformFeedbackIfStarted();

    commandBuffer->bindGraphicsPipeline(mCurrentGraphicsPipeline->getPipeline());
    // Update the queue serial for the pipeline object.
    ASSERT(mCurrentGraphicsPipeline && mCurrentGraphicsPipeline->valid());
    // TODO: https://issuetracker.google.com/issues/169788986: Need to change this so that we get
    // the actual serial used when this work is submitted.
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
    // TODO: https://issuetracker.google.com/issues/169788986: Need to change this so that we get
    // the actual serial used when this work is submitted.
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
        if (textureVk->hasBeenBoundAsImage())
        {
            textureLayout = executable->isCompute() ? vk::ImageLayout::ComputeShaderWrite
                                                    : vk::ImageLayout::AllGraphicsShadersWrite;
        }
        else if (image.isDepthOrStencil())
        {
            // We always use a depth-stencil read-only layout for any depth Textures to simplify our
            // implementation's handling of depth-stencil read-only mode. We don't have to split a
            // RenderPass to transition a depth texture from shader-read to read-only. This improves
            // performance in Manhattan. Future optimizations are likely possible here including
            // using specialized barriers without breaking the RenderPass.
            textureLayout = vk::ImageLayout::DepthStencilReadOnly;
        }
        else
        {
            gl::ShaderBitSet remainingShaderBits =
                executable->getSamplerShaderBitsForTextureUnitIndex(textureUnit);
            ASSERT(remainingShaderBits.any());
            gl::ShaderType firstShader = remainingShaderBits.first();
            remainingShaderBits.reset(firstShader);
            // If we have multiple shader accessing it, we barrier against all shader stage read
            // given that we only support vertex/frag shaders
            if (remainingShaderBits.any())
            {
                textureLayout = vk::ImageLayout::AllGraphicsShadersReadOnly;
            }
            else
            {
                textureLayout = kShaderReadOnlyImageLayouts[firstShader];
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

    if (!executable->hasTransformFeedbackOutput())
    {
        return angle::Result::Continue;
    }

    TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(mState.getCurrentTransformFeedback());

    if (mState.isTransformFeedbackActiveUnpaused())
    {
        size_t bufferCount = executable->getTransformFeedbackBufferCount();
        const gl::TransformFeedbackBuffersArray<vk::BufferHelper *> &bufferHelpers =
            transformFeedbackVk->getBufferHelpers();

        for (size_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
        {
            vk::BufferHelper *bufferHelper = bufferHelpers[bufferIndex];
            ASSERT(bufferHelper);
            mRenderPassCommands->bufferWrite(&mResourceUseList, VK_ACCESS_SHADER_WRITE_BIT,
                                             vk::PipelineStage::VertexShader,
                                             vk::AliasingMode::Disallowed, bufferHelper);
        }
    }

    // TODO(http://anglebug.com/3570): Need to update to handle Program Pipelines
    vk::BufferHelper *uniformBuffer      = mDefaultUniformStorage.getCurrentBuffer();
    vk::UniformsAndXfbDesc xfbBufferDesc = transformFeedbackVk->getTransformFeedbackDesc();
    xfbBufferDesc.updateDefaultUniformBuffer(uniformBuffer ? uniformBuffer->getBufferSerial()
                                                           : vk::kInvalidBufferSerial);

    return mProgram->getExecutable().updateTransformFeedbackDescriptorSet(
        mProgram->getState(), mProgram->getDefaultUniformBlocks(), uniformBuffer, this,
        xfbBufferDesc);
}

angle::Result ContextVk::handleDirtyGraphicsTransformFeedbackBuffersExtension(
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
    size_t bufferCount                       = executable->getTransformFeedbackBufferCount();

    const gl::TransformFeedbackBuffersArray<vk::BufferHelper *> &bufferHelpers =
        transformFeedbackVk->getBufferHelpers();

    for (size_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
    {
        vk::BufferHelper *bufferHelper = bufferHelpers[bufferIndex];
        ASSERT(bufferHelper);
        mRenderPassCommands->bufferWrite(
            &mResourceUseList, VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
            vk::PipelineStage::TransformFeedback, vk::AliasingMode::Disallowed, bufferHelper);
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
    size_t bufferCount = executable->getTransformFeedbackBufferCount();
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
    if (mRenderPassCommands->isTransformFeedbackStarted())
    {
        mRenderPassCommands->resumeTransformFeedback();
    }
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyDescriptorSets(const gl::Context *context,
                                                   vk::CommandBuffer *commandBuffer)
{
    ANGLE_TRY(mExecutable->updateDescriptorSets(this, commandBuffer));
    return angle::Result::Continue;
}

void ContextVk::updateOverlayOnPresent()
{
    const gl::OverlayType *overlay = mState.getOverlay();
    ASSERT(overlay->isEnabled());

    // Update overlay if active.
    {
        gl::RunningGraphWidget *renderPassCount =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanRenderPassCount);
        renderPassCount->add(mRenderPassCommands->getAndResetCounter());
        renderPassCount->next();
    }

    {
        gl::RunningGraphWidget *writeDescriptorSetCount =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanWriteDescriptorSetCount);
        writeDescriptorSetCount->add(mPerfCounters.writeDescriptorSets);
        writeDescriptorSetCount->next();

        mPerfCounters.writeDescriptorSets = 0;
    }

    {
        uint32_t descriptorSetAllocations = 0;

        // ContextVk's descriptor set allocations
        for (const uint32_t count : mObjectPerfCounters.descriptorSetsAllocated)
        {
            descriptorSetAllocations += count;
        }
        // UtilsVk's descriptor set allocations
        descriptorSetAllocations += mUtils.getObjectPerfCounters().descriptorSetsAllocated;
        // ProgramExecutableVk's descriptor set allocations
        const gl::State &state = getState();
        const gl::ShaderProgramManager &shadersAndPrograms =
            state.getShaderProgramManagerForCapture();
        const gl::ResourceMap<gl::Program, gl::ShaderProgramID> &programs =
            shadersAndPrograms.getProgramsForCaptureAndPerf();
        for (const std::pair<GLuint, gl::Program *> &resource : programs)
        {
            ProgramVk *programVk = vk::GetImpl(resource.second);
            ProgramExecutableVk::PerfCounters progPerfCounters =
                programVk->getExecutable().getObjectPerfCounters();

            for (const uint32_t count : progPerfCounters.descriptorSetsAllocated)
            {
                descriptorSetAllocations += count;
            }
        }

        gl::RunningGraphWidget *descriptorSetAllocationCount =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanDescriptorSetAllocations);
        descriptorSetAllocationCount->add(descriptorSetAllocations -
                                          mPerfCounters.descriptorSetAllocations);
        descriptorSetAllocationCount->next();
        mPerfCounters.descriptorSetAllocations = descriptorSetAllocations;
    }
}

void ContextVk::addOverlayUsedBuffersCount(vk::CommandBufferHelper *commandBuffer)
{
    const gl::OverlayType *overlay = mState.getOverlay();
    if (!overlay->isEnabled())
    {
        return;
    }

    gl::RunningHistogramWidget *widget =
        overlay->getRunningHistogramWidget(gl::WidgetId::VulkanRenderPassBufferCount);
    size_t buffersCount = commandBuffer->getUsedBuffersCount();
    if (buffersCount > 0)
    {
        widget->add(buffersCount);
        widget->next();
    }
}

void ContextVk::commandProcessorSyncErrors()
{
    while (mRenderer->hasPendingError())
    {
        vk::Error error = mRenderer->getAndClearPendingError();
        if (error.mErrorCode != VK_SUCCESS)
        {
            handleError(error.mErrorCode, error.mFile, error.mFunction, error.mLine);
        }
    }
}

void ContextVk::commandProcessorSyncErrorsAndQueueCommand(vk::CommandProcessorTask *command)
{
    commandProcessorSyncErrors();
    mRenderer->queueCommand(this, command);
}

angle::Result ContextVk::submitFrame(const vk::Semaphore *signalSemaphore)
{
    if (mCurrentWindowSurface)
    {
        vk::Semaphore waitSemaphore = mCurrentWindowSurface->getAcquireImageSemaphore();
        if (waitSemaphore.valid())
        {
            addWaitSemaphore(waitSemaphore.getHandle(),
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            addGarbage(&waitSemaphore);
        }
    }

    if (vk::CommandBufferHelper::kEnableCommandStreamDiagnostics)
    {
        dumpCommandStreamDiagnostics();
    }

    if (mRenderer->getFeatures().commandProcessor.enabled)
    {
        vk::CommandProcessorTask flushAndQueueSubmit;
        flushAndQueueSubmit.initFlushAndQueueSubmit(
            std::move(mWaitSemaphores), std::move(mWaitSemaphoreStageMasks), signalSemaphore,
            mContextPriority, std::move(mCurrentGarbage), std::move(mResourceUseList));

        commandProcessorSyncErrorsAndQueueCommand(&flushAndQueueSubmit);
    }
    else
    {
        ANGLE_TRY(ensureSubmitFenceInitialized());
        ANGLE_TRY(mCommandQueue.submitFrame(this, mContextPriority, mWaitSemaphores,
                                            mWaitSemaphoreStageMasks, signalSemaphore, mSubmitFence,
                                            &mResourceUseList, &mCurrentGarbage, &mCommandPool));
        // Make sure a new fence is created for the next submission.
        mRenderer->resetSharedFence(&mSubmitFence);

        mWaitSemaphores.clear();
        mWaitSemaphoreStageMasks.clear();
    }

    onRenderPassFinished();
    mComputeDirtyBits |= mNewComputeCommandBufferDirtyBits;

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

    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::synchronizeCpuGpuTime");

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

        ANGLE_TRY(mRenderer->getCommandBufferOneOff(this, &commandBuffer));

        commandBuffer.setEvent(gpuReady.get().getHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
        commandBuffer.waitEvents(1, cpuReady.get().ptr(), VK_PIPELINE_STAGE_HOST_BIT,
                                 VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, nullptr, 0, nullptr, 0,
                                 nullptr);
        timestampQuery.writeTimestampToPrimary(this, &commandBuffer);
        commandBuffer.setEvent(gpuDone.get().getHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

        ANGLE_VK_TRY(this, commandBuffer.end());

        Serial throwAwaySerial;
        ANGLE_TRY(mRenderer->queueSubmitOneOff(this, std::move(commandBuffer), mContextPriority,
                                               nullptr, &throwAwaySerial));

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
        // Note: This LastSubmittedQueueSerial may include more work then was submitted above if
        // another thread had submitted work.
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
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::finishAllWork");
    for (vk::GarbageObject &garbage : mCurrentGarbage)
    {
        garbage.destroy(mRenderer);
    }
    mCurrentGarbage.clear();
    if (mRenderer->getFeatures().commandProcessor.enabled)
    {
        // Issue command to CommandProcessor to ensure all work is complete, which will return any
        // garbage items as well.
        mRenderer->finishAllWork(this);
    }
    else
    {
        mCommandQueue.clearAllGarbage(mRenderer);
    }
}

void ContextVk::handleDeviceLost()
{
    mOutsideRenderPassCommands->reset();
    mRenderPassCommands->reset();

    if (mRenderer->getFeatures().commandProcessor.enabled)
    {
        mRenderer->handleDeviceLost();
    }
    else
    {
        mCommandQueue.handleDeviceLost(mRenderer);
    }
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
        ANGLE_PERF_WARNING(getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "Potential inefficiency emulating uint8 vertex attributes due to lack "
                           "of hardware support");

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

angle::Result ContextVk::multiDrawArrays(const gl::Context *context,
                                         gl::PrimitiveMode mode,
                                         const GLint *firsts,
                                         const GLsizei *counts,
                                         GLsizei drawcount)
{
    return rx::MultiDrawArraysGeneral(this, context, mode, firsts, counts, drawcount);
}

angle::Result ContextVk::multiDrawArraysInstanced(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  const GLint *firsts,
                                                  const GLsizei *counts,
                                                  const GLsizei *instanceCounts,
                                                  GLsizei drawcount)
{
    return rx::MultiDrawArraysInstancedGeneral(this, context, mode, firsts, counts, instanceCounts,
                                               drawcount);
}

angle::Result ContextVk::multiDrawElements(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           const GLsizei *counts,
                                           gl::DrawElementsType type,
                                           const GLvoid *const *indices,
                                           GLsizei drawcount)
{
    return rx::MultiDrawElementsGeneral(this, context, mode, counts, type, indices, drawcount);
}

angle::Result ContextVk::multiDrawElementsInstanced(const gl::Context *context,
                                                    gl::PrimitiveMode mode,
                                                    const GLsizei *counts,
                                                    gl::DrawElementsType type,
                                                    const GLvoid *const *indices,
                                                    const GLsizei *instanceCounts,
                                                    GLsizei drawcount)
{
    return rx::MultiDrawElementsInstancedGeneral(this, context, mode, counts, type, indices,
                                                 instanceCounts, drawcount);
}

angle::Result ContextVk::multiDrawArraysInstancedBaseInstance(const gl::Context *context,
                                                              gl::PrimitiveMode mode,
                                                              const GLint *firsts,
                                                              const GLsizei *counts,
                                                              const GLsizei *instanceCounts,
                                                              const GLuint *baseInstances,
                                                              GLsizei drawcount)
{
    return rx::MultiDrawArraysInstancedBaseInstanceGeneral(
        this, context, mode, firsts, counts, instanceCounts, baseInstances, drawcount);
}

angle::Result ContextVk::multiDrawElementsInstancedBaseVertexBaseInstance(
    const gl::Context *context,
    gl::PrimitiveMode mode,
    const GLsizei *counts,
    gl::DrawElementsType type,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount)
{
    return rx::MultiDrawElementsInstancedBaseVertexBaseInstanceGeneral(
        this, context, mode, counts, type, indices, instanceCounts, baseVertices, baseInstances,
        drawcount);
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
        // Change depthstencil attachment storeOp to DONT_CARE
        const gl::DepthStencilState &dsState = mState.getDepthStencilState();
        mRenderPassCommands->invalidateRenderPassStencilAttachment(
            dsState, mRenderPassCommands->getRenderArea());
        mRenderPassCommands->invalidateRenderPassDepthAttachment(
            dsState, mRenderPassCommands->getRenderArea());
        // Mark content as invalid so that we will not load them in next renderpass
        depthStencilRenderTarget->invalidateEntireContent(this);
        depthStencilRenderTarget->invalidateEntireStencilContent(this);
    }

    // Use finalLayout instead of extra barrier for layout change to present
    vk::ImageHelper &image = color0RenderTarget->getImageForWrite();
    image.setCurrentImageLayout(vk::ImageLayout::Present);
    // TODO(syoussefi):  We currently don't store the layout of the resolve attachments, so once
    // multisampled backbuffers are optimized to use resolve attachments, this information needs to
    // be stored somewhere.  http://anglebug.com/4836
    mRenderPassCommands->updateRenderPassAttachmentFinalLayout(vk::kAttachmentIndexZero,
                                                               image.getCurrentImageLayout());
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

    VkDebugUtilsLabelEXT label;
    vk::MakeDebugUtilsLabel(GL_DEBUG_SOURCE_APPLICATION, marker, &label);
    mOutsideRenderPassCommands->getCommandBuffer().insertDebugUtilsLabelEXT(label);

    return angle::Result::Continue;
}

angle::Result ContextVk::pushGroupMarker(GLsizei length, const char *marker)
{
    if (!mRenderer->enableDebugUtils())
        return angle::Result::Continue;

    VkDebugUtilsLabelEXT label;
    vk::MakeDebugUtilsLabel(GL_DEBUG_SOURCE_APPLICATION, marker, &label);
    mOutsideRenderPassCommands->getCommandBuffer().beginDebugUtilsLabelEXT(label);

    return angle::Result::Continue;
}

angle::Result ContextVk::popGroupMarker()
{
    if (!mRenderer->enableDebugUtils())
        return angle::Result::Continue;

    mOutsideRenderPassCommands->getCommandBuffer().endDebugUtilsLabelEXT();

    return angle::Result::Continue;
}

angle::Result ContextVk::pushDebugGroup(const gl::Context *context,
                                        GLenum source,
                                        GLuint id,
                                        const std::string &message)
{
    if (!mRenderer->enableDebugUtils())
        return angle::Result::Continue;

    VkDebugUtilsLabelEXT label;
    vk::MakeDebugUtilsLabel(source, message.c_str(), &label);
    mOutsideRenderPassCommands->getCommandBuffer().beginDebugUtilsLabelEXT(label);

    return angle::Result::Continue;
}

angle::Result ContextVk::popDebugGroup(const gl::Context *context)
{
    if (!mRenderer->enableDebugUtils())
        return angle::Result::Continue;

    mOutsideRenderPassCommands->getCommandBuffer().endDebugUtilsLabelEXT();

    return angle::Result::Continue;
}

void ContextVk::logEvent(const char *eventString)
{
    // Save this event (about an OpenGL ES command being called).
    mEventLog.push_back(eventString);

    // Set a dirty bit in order to stay off the "hot path" for when not logging.
    mGraphicsDirtyBits.set(DIRTY_BIT_EVENT_LOG);
    mComputeDirtyBits.set(DIRTY_BIT_EVENT_LOG);
}

void ContextVk::endEventLog(gl::EntryPoint entryPoint)
{
    ASSERT(mRenderPassCommands);
    mRenderPassCommands->getCommandBuffer().endDebugUtilsLabelEXT();
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

void ContextVk::updateColorMasks(const gl::BlendStateExt &blendStateExt)
{
    mClearColorMasks = blendStateExt.mColorMask;

    FramebufferVk *framebufferVk = vk::GetImpl(mState.getDrawFramebuffer());
    mGraphicsPipelineDesc->updateColorWriteMasks(&mGraphicsPipelineTransition, mClearColorMasks,
                                                 framebufferVk->getEmulatedAlphaAttachmentMask(),
                                                 framebufferVk->getState().getEnabledDrawBuffers());
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

gl::Rectangle ContextVk::getCorrectedViewport(const gl::Rectangle &viewport) const
{
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
    // x and y must each be between viewportBoundsRange[0] and viewportBoundsRange[1], inclusive.
    // Viewport size cannot be 0 so ensure there is always size for a 1x1 viewport
    int correctedX = std::min<int>(viewport.x, viewportBoundsRangeHigh - 1);
    correctedX     = std::max<int>(correctedX, viewportBoundsRangeLow);
    int correctedY = std::min<int>(viewport.y, viewportBoundsRangeHigh - 1);
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

    return gl::Rectangle(correctedX, correctedY, correctedWidth, correctedHeight);
}

void ContextVk::updateViewport(FramebufferVk *framebufferVk,
                               const gl::Rectangle &viewport,
                               float nearPlane,
                               float farPlane,
                               bool invertViewport)
{

    gl::Box fbDimensions        = framebufferVk->getState().getDimensions();
    gl::Rectangle correctedRect = getCorrectedViewport(viewport);
    gl::Rectangle rotatedRect;
    RotateRectangle(getRotationDrawFramebuffer(), false, fbDimensions.width, fbDimensions.height,
                    correctedRect, &rotatedRect);

    VkViewport vkViewport;
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

void ContextVk::updateScissor(const gl::State &glState)
{
    FramebufferVk *framebufferVk = vk::GetImpl(glState.getDrawFramebuffer());
    gl::Rectangle renderArea     = framebufferVk->getNonRotatedCompleteRenderArea();

    // Clip the render area to the viewport.
    gl::Rectangle viewportClippedRenderArea;
    gl::ClipRectangle(renderArea, getCorrectedViewport(glState.getViewport()),
                      &viewportClippedRenderArea);

    gl::Rectangle scissoredArea = ClipRectToScissor(getState(), viewportClippedRenderArea, false);
    gl::Rectangle rotatedScissoredArea;
    RotateRectangle(getRotationDrawFramebuffer(), isViewportFlipEnabledForDrawFBO(),
                    renderArea.width, renderArea.height, scissoredArea, &rotatedScissoredArea);

    mGraphicsPipelineDesc->updateScissor(&mGraphicsPipelineTransition,
                                         gl_vk::GetRect(rotatedScissoredArea));

    // If the scissor has grown beyond the previous scissoredRenderArea, grow the render pass render
    // area.  The only undesirable effect this may have is that if the render area does not cover a
    // previously invalidated area, that invalidate will have to be discarded.
    if (mRenderPassCommandBuffer &&
        !mRenderPassCommands->getRenderArea().encloses(rotatedScissoredArea))
    {
        ASSERT(mRenderPassCommands->started());
        mRenderPassCommands->growRenderArea(this, rotatedScissoredArea);
    }
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

    if (mProgram)
    {
        mProgram->onProgramBind();
    }
    else if (mProgramPipeline)
    {
        mProgramPipeline->onProgramBind(this);
    }
}

angle::Result ContextVk::invalidateProgramExecutableHelper(const gl::Context *context)
{
    const gl::State &glState                = context->getState();
    const gl::ProgramExecutable *executable = glState.getProgramExecutable();

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
        bool useVertexBuffer = (executable->getMaxActiveAttribLocation() > 0);
        mNonIndexedDirtyBitsMask.set(DIRTY_BIT_VERTEX_BUFFERS, useVertexBuffer);
        mIndexedDirtyBitsMask.set(DIRTY_BIT_VERTEX_BUFFERS, useVertexBuffer);
        mCurrentGraphicsPipeline = nullptr;
        mGraphicsPipelineTransition.reset();

        ASSERT(mExecutable);
        mExecutable->updateEarlyFragmentTestsOptimization(this);
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
                updateScissor(glState);
                break;
            case gl::State::DIRTY_BIT_VIEWPORT:
            {
                FramebufferVk *framebufferVk = vk::GetImpl(glState.getDrawFramebuffer());
                updateViewport(framebufferVk, glState.getViewport(), glState.getNearPlane(),
                               glState.getFarPlane(), isViewportFlipEnabledForDrawFBO());
                // Update the scissor, which will be constrained to the viewport
                updateScissor(glState);
                break;
            }
            case gl::State::DIRTY_BIT_DEPTH_RANGE:
                updateDepthRange(glState.getNearPlane(), glState.getFarPlane());
                break;
            case gl::State::DIRTY_BIT_BLEND_ENABLED:
                mGraphicsPipelineDesc->updateBlendEnabled(&mGraphicsPipelineTransition,
                                                          glState.getBlendStateExt().mEnabledMask);
                break;
            case gl::State::DIRTY_BIT_BLEND_COLOR:
                mGraphicsPipelineDesc->updateBlendColor(&mGraphicsPipelineTransition,
                                                        glState.getBlendColor());
                break;
            case gl::State::DIRTY_BIT_BLEND_FUNCS:
                mGraphicsPipelineDesc->updateBlendFuncs(&mGraphicsPipelineTransition,
                                                        glState.getBlendStateExt());
                break;
            case gl::State::DIRTY_BIT_BLEND_EQUATIONS:
                mGraphicsPipelineDesc->updateBlendEquations(&mGraphicsPipelineTransition,
                                                            glState.getBlendStateExt());
                break;
            case gl::State::DIRTY_BIT_COLOR_MASK:
                updateColorMasks(glState.getBlendStateExt());
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
            {
                mGraphicsPipelineDesc->updateDepthTestEnabled(&mGraphicsPipelineTransition,
                                                              glState.getDepthStencilState(),
                                                              glState.getDrawFramebuffer());
                ANGLE_TRY(updateRenderPassDepthStencilAccess());
                break;
            }
            case gl::State::DIRTY_BIT_DEPTH_FUNC:
                mGraphicsPipelineDesc->updateDepthFunc(&mGraphicsPipelineTransition,
                                                       glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_DEPTH_MASK:
            {
                mGraphicsPipelineDesc->updateDepthWriteEnabled(&mGraphicsPipelineTransition,
                                                               glState.getDepthStencilState(),
                                                               glState.getDrawFramebuffer());
                ANGLE_TRY(updateRenderPassDepthStencilAccess());
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_TEST_ENABLED:
            {
                mGraphicsPipelineDesc->updateStencilTestEnabled(&mGraphicsPipelineTransition,
                                                                glState.getDepthStencilState(),
                                                                glState.getDrawFramebuffer());
                ANGLE_TRY(updateRenderPassDepthStencilAccess());
                break;
            }
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
                // buffer to ensure we start a new one. We don't actually close the render pass here
                // as some optimizations in non-draw commands require the render pass to remain
                // open, such as invalidate or blit. Note that we always start a new command buffer
                // because we currently can only support one open RenderPass at a time.
                onRenderPassFinished();

                gl::Framebuffer *drawFramebuffer = glState.getDrawFramebuffer();
                mDrawFramebuffer                 = vk::GetImpl(drawFramebuffer);
                mDrawFramebuffer->setReadOnlyDepthFeedbackLoopMode(false);
                updateFlipViewportDrawFramebuffer(glState);
                updateSurfaceRotationDrawFramebuffer(glState);
                updateViewport(mDrawFramebuffer, glState.getViewport(), glState.getNearPlane(),
                               glState.getFarPlane(), isViewportFlipEnabledForDrawFBO());
                updateColorMasks(glState.getBlendStateExt());
                updateSampleMask(glState);
                mGraphicsPipelineDesc->updateRasterizationSamples(&mGraphicsPipelineTransition,
                                                                  mDrawFramebuffer->getSamples());
                mGraphicsPipelineDesc->updateFrontFace(&mGraphicsPipelineTransition,
                                                       glState.getRasterizerState(),
                                                       isViewportFlipEnabledForDrawFBO());
                updateScissor(glState);
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
                onDrawFramebufferRenderPassDescChange(mDrawFramebuffer);
                break;
            }
            case gl::State::DIRTY_BIT_RENDERBUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_VERTEX_ARRAY_BINDING:
            {
                mVertexArray = vk::GetImpl(glState.getVertexArray());
                invalidateDefaultAttributes(context->getStateCache().getActiveDefaultAttribsMask());
                ANGLE_TRY(mVertexArray->updateActiveAttribInfo(this));
                ANGLE_TRY(onIndexBufferChange(mVertexArray->getCurrentElementArrayBuffer()));
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
            case gl::State::DIRTY_BIT_SAMPLE_SHADING:
                mGraphicsPipelineDesc->updateSampleShading(&mGraphicsPipelineTransition,
                                                           glState.isSampleShadingEnabled(),
                                                           glState.getMinSampleShading());
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

    // Flip viewports if the user did not request that the surface is flipped.
    egl::Surface *drawSurface = context->getCurrentDrawSurface();
    mFlipYForCurrentSurface =
        drawSurface != nullptr &&
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

    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    if (executable && executable->hasTransformFeedbackOutput() &&
        mState.isTransformFeedbackActive())
    {
        onTransformFeedbackStateChanged();
        if (getFeatures().supportsTransformFeedbackExtension.enabled)
        {
            mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME);
        }
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::onUnMakeCurrent(const gl::Context *context)
{
    ANGLE_TRY(flushImpl(nullptr));
    if (mRenderer->getFeatures().commandProcessor.enabled)
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::onUnMakeCurrent");
        mRenderer->finishAllWork(this);
    }
    mCurrentWindowSurface = nullptr;
    return angle::Result::Continue;
}

void ContextVk::updateFlipViewportDrawFramebuffer(const gl::State &glState)
{
    // The default framebuffer (originating from the swapchain) is rendered upside-down due to the
    // difference in the coordinate systems of Vulkan and GLES.  Rendering upside-down has the
    // effect that rendering is done the same way as OpenGL.  The KHR_MAINTENANCE_1 extension is
    // subsequently enabled to allow negative viewports.  We inverse rendering to the backbuffer by
    // reversing the height of the viewport and increasing Y by the height.  So if the viewport was
    // (0, 0, width, height), it becomes (0, height, width, -height).  Unfortunately, when we start
    // doing this, we also need to adjust a number of places since the rendering now happens
    // upside-down.  Affected places so far:
    //
    // - readPixels
    // - copyTexImage
    // - framebuffer blit
    // - generating mipmaps
    // - Point sprites tests
    // - texStorage
    gl::Framebuffer *drawFramebuffer = glState.getDrawFramebuffer();
    mFlipViewportForDrawFramebuffer  = drawFramebuffer->isDefault();
}

void ContextVk::updateFlipViewportReadFramebuffer(const gl::State &glState)
{
    gl::Framebuffer *readFramebuffer = glState.getReadFramebuffer();
    mFlipViewportForReadFramebuffer  = readFramebuffer->isDefault();
}

void ContextVk::updateSurfaceRotationDrawFramebuffer(const gl::State &glState)
{
    gl::Framebuffer *drawFramebuffer = glState.getDrawFramebuffer();
    mCurrentRotationDrawFramebuffer =
        DetermineSurfaceRotation(drawFramebuffer, mCurrentWindowSurface);

    if (mCurrentRotationDrawFramebuffer != mGraphicsPipelineDesc->getSurfaceRotation())
    {
        // surface rotation are specialization constants, which affects program compilation. When
        // rotation changes, we need to update GraphicsPipelineDesc so that the correct pipeline
        // program object will be retrieved.
        mGraphicsPipelineDesc->updateSurfaceRotation(&mGraphicsPipelineTransition,
                                                     mCurrentRotationDrawFramebuffer);
    }
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

void ContextVk::onFramebufferChange(FramebufferVk *framebufferVk)
{
    // This is called from FramebufferVk::syncState.  Skip these updates if the framebuffer being
    // synced is the read framebuffer (which is not equal the draw framebuffer).
    if (framebufferVk != vk::GetImpl(mState.getDrawFramebuffer()))
    {
        return;
    }

    // Ensure that the pipeline description is updated.
    if (mGraphicsPipelineDesc->getRasterizationSamples() !=
        static_cast<uint32_t>(framebufferVk->getSamples()))
    {
        mGraphicsPipelineDesc->updateRasterizationSamples(&mGraphicsPipelineTransition,
                                                          framebufferVk->getSamples());
    }

    // Update scissor.
    updateScissor(mState);

    onDrawFramebufferRenderPassDescChange(framebufferVk);
}

void ContextVk::onDrawFramebufferRenderPassDescChange(FramebufferVk *framebufferVk)
{
    invalidateCurrentGraphicsPipeline();
    mGraphicsPipelineDesc->updateRenderPassDesc(&mGraphicsPipelineTransition,
                                                framebufferVk->getRenderPassDesc());
}

void ContextVk::invalidateCurrentTransformFeedbackBuffers()
{
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS);
    }
    else if (getFeatures().emulateTransformFeedback.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS);
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }
}

void ContextVk::onTransformFeedbackStateChanged()
{
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_STATE);
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS);
    }
    else if (getFeatures().emulateTransformFeedback.enabled)
    {
        invalidateGraphicsDriverUniforms();
        invalidateCurrentTransformFeedbackBuffers();
    }
}

angle::Result ContextVk::onBeginTransformFeedback(
    size_t bufferCount,
    const gl::TransformFeedbackBuffersArray<vk::BufferHelper *> &buffers)
{
    onTransformFeedbackStateChanged();

    for (size_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
    {
        const vk::BufferHelper *buffer = buffers[bufferIndex];
        if (mCurrentTransformFeedbackBuffers.contains(buffer) ||
            mRenderPassCommands->usesBuffer(*buffer))
        {
            ANGLE_TRY(flushCommandsAndEndRenderPass());
            break;
        }
    }

    populateTransformFeedbackBufferSet(bufferCount, buffers);

    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME);
    }

    return angle::Result::Continue;
}

void ContextVk::populateTransformFeedbackBufferSet(
    size_t bufferCount,
    const gl::TransformFeedbackBuffersArray<vk::BufferHelper *> &buffers)
{
    for (size_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
    {
        vk::BufferHelper *buffer = buffers[bufferIndex];
        if (!mCurrentTransformFeedbackBuffers.contains(buffer))
        {
            mCurrentTransformFeedbackBuffers.insert(buffer);
        }
    }
}

void ContextVk::onEndTransformFeedback()
{
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        if (mRenderPassCommands->isTransformFeedbackStarted())
        {
            mRenderPassCommands->endTransformFeedback();
        }
    }
    else if (getFeatures().emulateTransformFeedback.enabled)
    {
        onTransformFeedbackStateChanged();
    }
}

angle::Result ContextVk::onPauseTransformFeedback()
{
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        return flushCommandsAndEndRenderPass();
    }
    else if (getFeatures().emulateTransformFeedback.enabled)
    {
        invalidateCurrentTransformFeedbackBuffers();
        return flushCommandsAndEndRenderPass();
    }
    return angle::Result::Continue;
}

void ContextVk::invalidateGraphicsDescriptorSet(DescriptorSetIndex usedDescriptorSet)
{
    // UtilsVk currently only uses set 0
    ASSERT(usedDescriptorSet == DescriptorSetIndex::DriverUniforms);
    if (mDriverUniforms[PipelineType::Graphics].descriptorSet != VK_NULL_HANDLE)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS_BINDING);
    }
}

void ContextVk::invalidateComputeDescriptorSet(DescriptorSetIndex usedDescriptorSet)
{
    // UtilsVk currently only uses set 0
    ASSERT(usedDescriptorSet == DescriptorSetIndex::DriverUniforms);
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

    ANGLE_TRY(flushCommandsAndEndRenderPass());

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask   = srcAccess;
    memoryBarrier.dstAccessMask   = dstAccess;

    mOutsideRenderPassCommands->getCommandBuffer().memoryBarrier(stageMask, stageMask,
                                                                 &memoryBarrier);

    if ((barriers & GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT) != 0)
    {
        // We need to make sure that all device-writes are host-visible, force a finish
        ANGLE_TRY(finishImpl());
    }

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

gl::BlendStateExt::ColorMaskStorage::Type ContextVk::getClearColorMasks() const
{
    return mClearColorMasks;
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

ANGLE_INLINE void ContextVk::resumeTransformFeedbackIfStarted()
{
    if (mRenderPassCommands->isTransformFeedbackStarted())
    {
        ASSERT(getFeatures().supportsTransformFeedbackExtension.enabled);
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME);
        mRenderPassCommands->pauseTransformFeedback();
    }
}

angle::Result ContextVk::handleDirtyGraphicsDriverUniforms(const gl::Context *context,
                                                           vk::CommandBuffer *commandBuffer)
{
    // Allocate a new region in the dynamic buffer.
    uint8_t *ptr;
    bool newBuffer;
    ANGLE_TRY(allocateDriverUniforms(sizeof(GraphicsDriverUniforms),
                                     &mDriverUniforms[PipelineType::Graphics], &ptr, &newBuffer));

    gl::Rectangle glViewport = mState.getViewport();
    float halfRenderAreaWidth =
        static_cast<float>(mDrawFramebuffer->getState().getDimensions().width) * 0.5f;
    float halfRenderAreaHeight =
        static_cast<float>(mDrawFramebuffer->getState().getDimensions().height) * 0.5f;
    if (isRotatedAspectRatioForDrawFBO())
    {
        // The surface is rotated 90/270 degrees.  This changes the aspect ratio of the surface.
        std::swap(glViewport.x, glViewport.y);
        std::swap(glViewport.width, glViewport.height);
    }
    float flipX = 1.0f;
    float flipY = -1.0f;
    // Y-axis flipping only comes into play with the default framebuffer (i.e. a swapchain image).
    // For 0-degree rotation, an FBO or pbuffer could be the draw framebuffer, and so we must check
    // whether flipY should be positive or negative.  All other rotations, will be to the default
    // framebuffer, and so the value of isViewportFlipEnabledForDrawFBO() is assumed true; the
    // appropriate flipY value is chosen such that gl_FragCoord is positioned at the lower-left
    // corner of the window.
    switch (mCurrentRotationDrawFramebuffer)
    {
        case SurfaceRotation::Identity:
            flipX = 1.0f;
            flipY = isViewportFlipEnabledForDrawFBO() ? -1.0f : 1.0f;
            break;
        case SurfaceRotation::Rotated90Degrees:
            ASSERT(isViewportFlipEnabledForDrawFBO());
            flipX = 1.0f;
            flipY = 1.0f;
            std::swap(halfRenderAreaWidth, halfRenderAreaHeight);
            break;
        case SurfaceRotation::Rotated180Degrees:
            ASSERT(isViewportFlipEnabledForDrawFBO());
            flipX = -1.0f;
            flipY = 1.0f;
            break;
        case SurfaceRotation::Rotated270Degrees:
            ASSERT(isViewportFlipEnabledForDrawFBO());
            flipX = -1.0f;
            flipY = -1.0f;
            break;
        default:
            UNREACHABLE();
            break;
    }

    uint32_t xfbActiveUnpaused = mState.isTransformFeedbackActiveUnpaused();

    float depthRangeNear = mState.getNearPlane();
    float depthRangeFar  = mState.getFarPlane();
    float depthRangeDiff = depthRangeFar - depthRangeNear;

    // Copy and flush to the device.
    GraphicsDriverUniforms *driverUniforms = reinterpret_cast<GraphicsDriverUniforms *>(ptr);
    *driverUniforms                        = {
        {static_cast<float>(glViewport.x), static_cast<float>(glViewport.y),
         static_cast<float>(glViewport.width), static_cast<float>(glViewport.height)},
        {halfRenderAreaWidth, halfRenderAreaHeight},
        {flipX, flipY},
        {flipX, -flipY},
        mState.getEnabledClipDistances().bits(),
        xfbActiveUnpaused,
        mXfbVertexCountPerInstance,
        {},
        {},
        {},
        {depthRangeNear, depthRangeFar, depthRangeDiff, 0.0f},
        {},
        {}};
    memcpy(&driverUniforms->preRotation, &kPreRotationMatrices[mCurrentRotationDrawFramebuffer],
           sizeof(PreRotationMatrixValues));
    memcpy(&driverUniforms->fragRotation, &kFragRotationMatrices[mCurrentRotationDrawFramebuffer],
           sizeof(PreRotationMatrixValues));

    if (xfbActiveUnpaused)
    {
        TransformFeedbackVk *transformFeedbackVk =
            vk::GetImpl(mState.getCurrentTransformFeedback());
        transformFeedbackVk->getBufferOffsets(this, mXfbBaseVertex,
                                              driverUniforms->xfbBufferOffsets.data(),
                                              driverUniforms->xfbBufferOffsets.size());
    }

    if (mState.hasValidAtomicCounterBuffer())
    {
        writeAtomicCounterBufferDriverUniformOffsets(driverUniforms->acbBufferOffsets.data(),
                                                     driverUniforms->acbBufferOffsets.size());
    }

    return updateDriverUniformsDescriptorSet(newBuffer, sizeof(GraphicsDriverUniforms),
                                             &mDriverUniforms[PipelineType::Graphics]);
}

angle::Result ContextVk::handleDirtyComputeDriverUniforms(const gl::Context *context,
                                                          vk::CommandBuffer *commandBuffer)
{
    // Allocate a new region in the dynamic buffer.
    uint8_t *ptr;
    bool newBuffer;
    ANGLE_TRY(allocateDriverUniforms(sizeof(ComputeDriverUniforms),
                                     &mDriverUniforms[PipelineType::Compute], &ptr, &newBuffer));

    // Copy and flush to the device.
    ComputeDriverUniforms *driverUniforms = reinterpret_cast<ComputeDriverUniforms *>(ptr);
    *driverUniforms                       = {};

    if (mState.hasValidAtomicCounterBuffer())
    {
        writeAtomicCounterBufferDriverUniformOffsets(driverUniforms->acbBufferOffsets.data(),
                                                     driverUniforms->acbBufferOffsets.size());
    }

    return updateDriverUniformsDescriptorSet(newBuffer, sizeof(ComputeDriverUniforms),
                                             &mDriverUniforms[PipelineType::Compute]);
}

void ContextVk::handleDirtyDriverUniformsBindingImpl(vk::CommandBuffer *commandBuffer,
                                                     VkPipelineBindPoint bindPoint,
                                                     DriverUniformsDescriptorSet *driverUniforms)
{
    // The descriptor pool that this descriptor set was allocated from needs to be retained when the
    // descriptor set is used in a new command. Since the descriptor pools are specific to each
    // ContextVk, we only need to retain them once to ensure the reference count and Serial are
    // updated correctly.
    if (!driverUniforms->descriptorPoolBinding.get().usedInRecordedCommands())
    {
        driverUniforms->descriptorPoolBinding.get().retain(&mResourceUseList);
    }

    commandBuffer->bindDescriptorSets(
        mExecutable->getPipelineLayout(), bindPoint, DescriptorSetIndex::DriverUniforms, 1,
        &driverUniforms->descriptorSet, 1, &driverUniforms->dynamicOffset);
}

angle::Result ContextVk::handleDirtyGraphicsDriverUniformsBinding(const gl::Context *context,
                                                                  vk::CommandBuffer *commandBuffer)
{
    // Bind the driver descriptor set.
    handleDirtyDriverUniformsBindingImpl(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                         &mDriverUniforms[PipelineType::Graphics]);
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyComputeDriverUniformsBinding(const gl::Context *context,
                                                                 vk::CommandBuffer *commandBuffer)
{
    // Bind the driver descriptor set.
    handleDirtyDriverUniformsBindingImpl(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                         &mDriverUniforms[PipelineType::Compute]);
    return angle::Result::Continue;
}

angle::Result ContextVk::allocateDriverUniforms(size_t driverUniformsSize,
                                                DriverUniformsDescriptorSet *driverUniforms,
                                                uint8_t **ptrOut,
                                                bool *newBufferOut)
{
    // Allocate a new region in the dynamic buffer. The allocate call may put buffer into dynamic
    // buffer's mInflightBuffers. During command submission time, these inflight buffers are added
    // into context's mResourceUseList which will ensure they get tagged with queue serial number
    // before moving them into the free list.
    VkDeviceSize offset;
    ANGLE_TRY(driverUniforms->dynamicBuffer.allocate(this, driverUniformsSize, ptrOut, nullptr,
                                                     &offset, newBufferOut));

    driverUniforms->dynamicOffset = static_cast<uint32_t>(offset);

    return angle::Result::Continue;
}

angle::Result ContextVk::updateDriverUniformsDescriptorSet(
    bool newBuffer,
    size_t driverUniformsSize,
    DriverUniformsDescriptorSet *driverUniforms)
{
    ANGLE_TRY(driverUniforms->dynamicBuffer.flush(this));

    if (!newBuffer)
    {
        return angle::Result::Continue;
    }

    const vk::BufferHelper *buffer = driverUniforms->dynamicBuffer.getCurrentBuffer();
    vk::BufferSerial bufferSerial  = buffer->getBufferSerial();
    // Look up in the cache first
    if (driverUniforms->descriptorSetCache.get(bufferSerial.getValue(),
                                               &driverUniforms->descriptorSet))
    {
        // The descriptor pool that this descriptor set was allocated from needs to be retained each
        // time the descriptor set is used in a new command.
        driverUniforms->descriptorPoolBinding.get().retain(&mResourceUseList);
        return angle::Result::Continue;
    }

    // Allocate a new descriptor set.
    bool isCompute            = getState().getProgramExecutable()->isCompute();
    PipelineType pipelineType = isCompute ? PipelineType::Compute : PipelineType::Graphics;
    bool newPoolAllocated;
    ANGLE_TRY(mDriverUniformsDescriptorPools[pipelineType].allocateSetsAndGetInfo(
        this, driverUniforms->descriptorSetLayout.get().ptr(), 1,
        &driverUniforms->descriptorPoolBinding, &driverUniforms->descriptorSet, &newPoolAllocated));
    mObjectPerfCounters.descriptorSetsAllocated[ToUnderlying(pipelineType)]++;

    // Clear descriptor set cache. It may no longer be valid.
    if (newPoolAllocated)
    {
        driverUniforms->descriptorSetCache.clear();
    }

    // Update the driver uniform descriptor set.
    VkDescriptorBufferInfo &bufferInfo = allocDescriptorBufferInfo();
    bufferInfo.buffer                  = buffer->getBuffer().getHandle();
    bufferInfo.offset                  = 0;
    bufferInfo.range                   = driverUniformsSize;

    VkWriteDescriptorSet &writeInfo = allocWriteDescriptorSet();
    writeInfo.sType                 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet                = driverUniforms->descriptorSet;
    writeInfo.dstBinding            = 0;
    writeInfo.dstArrayElement       = 0;
    writeInfo.descriptorCount       = 1;
    writeInfo.descriptorType        = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writeInfo.pImageInfo            = nullptr;
    writeInfo.pTexelBufferView      = nullptr;
    writeInfo.pBufferInfo           = &bufferInfo;

    // Add into descriptor set cache
    driverUniforms->descriptorSetCache.insert(bufferSerial.getValue(),
                                              driverUniforms->descriptorSet);

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
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    uint32_t prevMaxIndex = mActiveTexturesDesc.getMaxIndex();
    memset(mActiveTextures.data(), 0, sizeof(mActiveTextures[0]) * prevMaxIndex);
    mActiveTexturesDesc.reset();

    const gl::ActiveTexturesCache &textures        = mState.getActiveTexturesCache();
    const gl::ActiveTextureMask &activeTextures    = executable->getActiveSamplersMask();
    const gl::ActiveTextureTypeArray &textureTypes = executable->getActiveSamplerTypes();

    bool haveImmutableSampler = false;
    for (size_t textureUnit : activeTextures)
    {
        gl::Texture *texture        = textures[textureUnit];
        gl::Sampler *sampler        = mState.getSampler(static_cast<uint32_t>(textureUnit));
        gl::TextureType textureType = textureTypes[textureUnit];
        ASSERT(textureType != gl::TextureType::InvalidEnum);

        vk::TextureUnit &activeTexture = mActiveTextures[textureUnit];

        // Null textures represent incomplete textures.
        if (texture == nullptr)
        {
            ANGLE_TRY(getIncompleteTexture(context, textureType, &texture));
        }
        else if (shouldSwitchToReadOnlyDepthFeedbackLoopMode(context, texture))
        {
            // The "readOnlyDepthMode" feature enables read-only depth-stencil feedback loops. We
            // only switch to "read-only" mode when there's loop. We track the depth-stencil access
            // mode in the RenderPass. The tracking tells us when we can retroactively go back and
            // change the RenderPass to read-only. If there are any writes we need to break and
            // finish the current RP before starting the read-only one.
            ASSERT(!mState.isDepthWriteEnabled());

            // Special handling for deferred clears.
            ANGLE_TRY(mDrawFramebuffer->flushDeferredClears(this));

            if (hasStartedRenderPass())
            {
                if (!mRenderPassCommands->isReadOnlyDepthMode())
                {
                    // To enter depth feedback loop, we must flush and start a new renderpass.
                    // Otherwise it will stick with writable layout and cause validation error.
                    ANGLE_TRY(flushCommandsAndEndRenderPass());
                }
                else
                {
                    mDrawFramebuffer->updateRenderPassReadOnlyDepthMode(this, mRenderPassCommands);
                }
            }

            mDrawFramebuffer->setReadOnlyDepthFeedbackLoopMode(true);
        }

        TextureVk *textureVk = vk::GetImpl(texture);
        ASSERT(textureVk != nullptr);

        const SamplerVk *samplerVk = sampler ? vk::GetImpl(sampler) : nullptr;

        const vk::SamplerHelper &samplerHelper =
            samplerVk ? samplerVk->getSampler() : textureVk->getSampler();
        const gl::SamplerState &samplerState =
            sampler ? sampler->getSamplerState() : texture->getSamplerState();
        activeTexture.texture    = textureVk;
        activeTexture.sampler    = &samplerHelper;
        activeTexture.srgbDecode = samplerState.getSRGBDecode();

        if (activeTexture.srgbDecode == GL_SKIP_DECODE_EXT)
        {
            // Make sure we use the MUTABLE bit for the storage. Because the "skip decode" is a
            // Sampler state we might not have caught this setting in TextureVk::syncState.
            ANGLE_TRY(textureVk->ensureMutable(this));
        }

        vk::ImageViewSubresourceSerial imageViewSerial =
            textureVk->getImageViewSubresourceSerial(samplerState);
        mActiveTexturesDesc.update(textureUnit, imageViewSerial, samplerHelper.getSamplerSerial());

        if (textureVk->getImage().hasImmutableSampler())
        {
            haveImmutableSampler = true;
        }
    }

    if (haveImmutableSampler)
    {
        // TODO(http://anglebug.com/5033): This will recreate the descriptor pools each time, which
        // will likely affect performance negatively.
        ANGLE_TRY(mExecutable->createPipelineLayout(context, &mActiveTextures));
        invalidateCurrentGraphicsPipeline();
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
                imageLayout = vk::ImageLayout::AllGraphicsShadersWrite;
            }
            else
            {
                imageLayout = kShaderWriteImageLayouts[shader];
            }
        }
        else
        {
            imageLayout = vk::ImageLayout::AllGraphicsShadersWrite;
        }
        VkImageAspectFlags aspectFlags = image->getAspectFlags();

        uint32_t layerStart = 0;
        uint32_t layerCount = image->getLayerCount();
        if (imageUnit.layered)
        {
            layerStart = imageUnit.layered;
            layerCount = 1;
        }

        commandBufferHelper->imageWrite(
            &mResourceUseList, gl::LevelIndex(static_cast<uint32_t>(imageUnit.level)), layerStart,
            layerCount, aspectFlags, imageLayout, vk::AliasingMode::Allowed, image);
    }

    return angle::Result::Continue;
}

bool ContextVk::hasRecordedCommands()
{
    ASSERT(mOutsideRenderPassCommands && mRenderPassCommands);
    return !mOutsideRenderPassCommands->empty() || mRenderPassCommands->started();
}

angle::Result ContextVk::flushImpl(const vk::Semaphore *signalSemaphore)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::flushImpl");

    // We must set this to zero before calling flushCommandsAndEndRenderPass to prevent it from
    // calling back to flushImpl.
    mDeferredFlushCount     = 0;
    mSyncObjectPendingFlush = false;

    ANGLE_TRY(flushCommandsAndEndRenderPass());

    if (mIsAnyHostVisibleBufferWritten)
    {
        // Make sure all writes to host-visible buffers are flushed.  We have no way of knowing
        // whether any buffer will be mapped for readback in the future, and we can't afford to
        // flush and wait on a one-pipeline-barrier command buffer on every map().
        VkMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT;
        memoryBarrier.dstAccessMask   = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;

        mOutsideRenderPassCommands->getCommandBuffer().memoryBarrier(
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, &memoryBarrier);
        mIsAnyHostVisibleBufferWritten = false;
    }

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("Primary", mPerfCounters.primaryBuffers);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_END, eventName));
    }
    ANGLE_TRY(flushOutsideRenderPassCommands());

    // We must add the per context dynamic buffers into mResourceUseList before submission so that
    // they get retained properly until GPU completes. We do not add current buffer into
    // mResourceUseList since they never get reused or freed until context gets destroyed, at which
    // time we always wait for GPU to finish before destroying the dynamic buffers.
    for (DriverUniformsDescriptorSet &driverUniform : mDriverUniforms)
    {
        driverUniform.dynamicBuffer.releaseInFlightBuffersToResourceUseList(this);
    }
    mDefaultUniformStorage.releaseInFlightBuffersToResourceUseList(this);
    mStagingBuffer.releaseInFlightBuffersToResourceUseList(this);

    ANGLE_TRY(submitFrame(signalSemaphore));

    mPerfCounters.renderPasses                           = 0;
    mPerfCounters.writeDescriptorSets                    = 0;
    mPerfCounters.flushedOutsideRenderPassCommandBuffers = 0;
    mPerfCounters.resolveImageCommands                   = 0;

    ASSERT(mWaitSemaphores.empty());
    ASSERT(mWaitSemaphoreStageMasks.empty());

    mPerfCounters.primaryBuffers++;

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("Primary", mPerfCounters.primaryBuffers);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_BEGIN, eventName));
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::finishImpl()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::finishImpl");

    ANGLE_TRY(flushImpl(nullptr));

    if (mRenderer->getFeatures().commandProcessor.enabled)
    {
        ANGLE_TRY(finishToSerial(getLastSubmittedQueueSerial()));
    }
    else
    {
        ANGLE_TRY(finishToSerial(getLastSubmittedQueueSerial()));
        ASSERT(!mCommandQueue.hasInFlightCommands());
    }

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

void ContextVk::addWaitSemaphore(VkSemaphore semaphore, VkPipelineStageFlags stageMask)
{
    mWaitSemaphores.push_back(semaphore);
    mWaitSemaphoreStageMasks.push_back(stageMask);
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
    if (mRenderer->getFeatures().commandProcessor.enabled)
    {
        // TODO: https://issuetracker.google.com/169788986 - would be better if we could just wait
        // for the work we need but that requires QueryHelper to use the actual serial for the
        // query.
        mRenderer->checkCompletedCommands(this);
        return angle::Result::Continue;
    }

    return mCommandQueue.checkCompletedCommands(this);
}

angle::Result ContextVk::finishToSerial(Serial serial)
{
    if (mRenderer->getFeatures().commandProcessor.enabled)
    {
        mRenderer->finishToSerial(this, serial);
        return angle::Result::Continue;
    }
    return mCommandQueue.finishToSerial(this, serial, mRenderer->getMaxFenceWaitTimeNs());
}

angle::Result ContextVk::getCompatibleRenderPass(const vk::RenderPassDesc &desc,
                                                 vk::RenderPass **renderPassOut)
{
    // Note: Each context has it's own RenderPassCache so no locking needed.
    return mRenderPassCache.getCompatibleRenderPass(this, desc, renderPassOut);
}

angle::Result ContextVk::getRenderPassWithOps(const vk::RenderPassDesc &desc,
                                              const vk::AttachmentOpsArray &ops,
                                              vk::RenderPass **renderPassOut)
{
    // Note: Each context has it's own RenderPassCache so no locking needed.
    return mRenderPassCache.getRenderPassWithOps(this, desc, ops, renderPassOut);
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
    ASSERT(!getRenderer()->getFeatures().commandProcessor.enabled);

    ANGLE_TRY(ensureSubmitFenceInitialized());

    ASSERT(!sharedFenceOut->isReferenced());
    sharedFenceOut->copy(getDevice(), mSubmitFence);
    return angle::Result::Continue;
}

vk::Shared<vk::Fence> ContextVk::getLastSubmittedFence() const
{
    if (mRenderer->getFeatures().commandProcessor.enabled)
    {
        return mRenderer->getLastSubmittedFence(this);
    }
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

    ANGLE_TRY(mRenderer->getCommandBufferOneOff(this, &commandBuffer));

    timestampQuery.writeTimestampToPrimary(this, &commandBuffer);
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
    ANGLE_TRY(mRenderer->queueSubmitOneOff(this, std::move(commandBuffer), mContextPriority,
                                           &fence.get(), &throwAwaySerial));

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

    return angle::Result::Continue;
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

    return mVertexArray->updateDefaultAttrib(this, attribIndex, bufferHandle,
                                             defaultBuffer.getCurrentBuffer(),
                                             static_cast<uint32_t>(offset));
}

vk::DescriptorSetLayoutDesc ContextVk::getDriverUniformsDescriptorSetDesc(
    VkShaderStageFlags shaderStages) const
{
    vk::DescriptorSetLayoutDesc desc;
    desc.update(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, shaderStages, nullptr);
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

    if (mRenderPassCommands->usesBufferForWrite(*buffer))
    {
        ANGLE_TRY(flushCommandsAndEndRenderPass());
    }
    else if (mOutsideRenderPassCommands->usesBufferForWrite(*buffer))
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

    if (mRenderPassCommands->usesBuffer(*buffer))
    {
        ANGLE_TRY(flushCommandsAndEndRenderPass());
    }
    else if (mOutsideRenderPassCommands->usesBuffer(*buffer))
    {
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    mOutsideRenderPassCommands->bufferWrite(&mResourceUseList, writeAccessType, writeStage,
                                            vk::AliasingMode::Disallowed, buffer);

    return angle::Result::Continue;
}

angle::Result ContextVk::endRenderPassIfImageUsed(const vk::ImageHelper &image)
{
    if (mRenderPassCommands->started() && mRenderPassCommands->usesImageInRenderPass(image))
    {
        return flushCommandsAndEndRenderPass();
    }
    else
    {
        return angle::Result::Continue;
    }
}

angle::Result ContextVk::onImageRead(VkImageAspectFlags aspectFlags,
                                     vk::ImageLayout imageLayout,
                                     vk::ImageHelper *image)
{
    ASSERT(!image->isReleasedToExternal());
    ASSERT(image->getImageSerial().valid());

    // Note that different read methods are not compatible. A shader read uses a different layout
    // than a transfer read. So we cannot support simultaneous read usage as easily as for Buffers.
    // TODO: Don't close the render pass if the image was only used read-only in the render pass.
    // http://anglebug.com/4984
    ANGLE_TRY(endRenderPassIfImageUsed(*image));

    image->recordReadBarrier(aspectFlags, imageLayout,
                             &mOutsideRenderPassCommands->getCommandBuffer());
    image->retain(&mResourceUseList);

    return angle::Result::Continue;
}

angle::Result ContextVk::onImageWrite(gl::LevelIndex levelStart,
                                      uint32_t levelCount,
                                      uint32_t layerStart,
                                      uint32_t layerCount,
                                      VkImageAspectFlags aspectFlags,
                                      vk::ImageLayout imageLayout,
                                      vk::ImageHelper *image)
{
    ASSERT(!image->isReleasedToExternal());
    ASSERT(image->getImageSerial().valid());

    ANGLE_TRY(endRenderPassIfImageUsed(*image));

    image->recordWriteBarrier(aspectFlags, imageLayout,
                              &mOutsideRenderPassCommands->getCommandBuffer());
    image->retain(&mResourceUseList);
    image->onWrite(levelStart, levelCount, layerStart, layerCount, aspectFlags);

    return angle::Result::Continue;
}

angle::Result ContextVk::onBufferReleaseToExternal(const vk::BufferHelper &buffer)
{
    if (mRenderPassCommands->usesBuffer(buffer))
    {
        ANGLE_TRY(flushCommandsAndEndRenderPass());
    }
    return angle::Result::Continue;
}

angle::Result ContextVk::onImageReleaseToExternal(const vk::ImageHelper &image)
{
    return endRenderPassIfImageUsed(image);
}

angle::Result ContextVk::beginNewRenderPass(
    const vk::Framebuffer &framebuffer,
    const gl::Rectangle &renderArea,
    const vk::RenderPassDesc &renderPassDesc,
    const vk::AttachmentOpsArray &renderPassAttachmentOps,
    const vk::PackedAttachmentIndex depthStencilAttachmentIndex,
    const vk::PackedClearValuesArray &clearValues,
    vk::CommandBuffer **commandBufferOut)
{
    // Next end any currently outstanding renderPass
    ANGLE_TRY(flushCommandsAndEndRenderPass());

    mRenderPassCommands->beginRenderPass(framebuffer, renderArea, renderPassDesc,
                                         renderPassAttachmentOps, depthStencilAttachmentIndex,
                                         clearValues, commandBufferOut);
    // Restart at subpass 0.
    mGraphicsPipelineDesc->resetSubpass(&mGraphicsPipelineTransition);

    mPerfCounters.renderPasses++;

    return angle::Result::Continue;
}

angle::Result ContextVk::startRenderPass(gl::Rectangle renderArea,
                                         vk::CommandBuffer **commandBufferOut)
{
    mGraphicsDirtyBits |= mNewGraphicsCommandBufferDirtyBits;

    ANGLE_TRY(mDrawFramebuffer->startNewRenderPass(this, renderArea, &mRenderPassCommandBuffer));

    ANGLE_TRY(resumeOcclusionQueryIfActive());

    const gl::DepthStencilState &dsState = mState.getDepthStencilState();
    vk::ResourceAccess depthAccess       = GetDepthAccess(dsState);
    vk::ResourceAccess stencilAccess     = GetStencilAccess(dsState);
    mRenderPassCommands->onDepthAccess(depthAccess);
    mRenderPassCommands->onStencilAccess(stencilAccess);

    mDrawFramebuffer->updateRenderPassReadOnlyDepthMode(this, mRenderPassCommands);

    if (commandBufferOut)
    {
        *commandBufferOut = mRenderPassCommandBuffer;
    }

    return angle::Result::Continue;
}

void ContextVk::startNextSubpass()
{
    ASSERT(hasStartedRenderPass());

    mRenderPassCommands->getCommandBuffer().nextSubpass(VK_SUBPASS_CONTENTS_INLINE);

    // The graphics pipelines are bound to a subpass, so update the subpass as well.
    mGraphicsPipelineDesc->nextSubpass(&mGraphicsPipelineTransition);
}

void ContextVk::restoreFinishedRenderPass(vk::Framebuffer *framebuffer)
{
    if (mRenderPassCommandBuffer != nullptr)
    {
        // The render pass isn't finished yet, so nothing to restore.
        return;
    }

    if (mRenderPassCommands->started() &&
        mRenderPassCommands->getFramebufferHandle() == framebuffer->getHandle())
    {
        // There is already a render pass open for this framebuffer, so just restore the
        // pointer rather than starting a whole new render pass. One possible path here
        // is if the draw framebuffer binding has changed from FBO A -> B -> A, without
        // any commands that started a new render pass for FBO B (such as a clear being
        // issued that was deferred).
        mRenderPassCommandBuffer = &mRenderPassCommands->getCommandBuffer();
        ASSERT(hasStartedRenderPass());
    }
}

uint32_t ContextVk::getCurrentSubpassIndex() const
{
    return mGraphicsPipelineDesc->getSubpass();
}

angle::Result ContextVk::flushCommandsAndEndRenderPass()
{
    // Ensure we flush the RenderPass *after* the prior commands.
    ANGLE_TRY(flushOutsideRenderPassCommands());
    ASSERT(mOutsideRenderPassCommands->empty());

    if (!mRenderPassCommands->started())
    {
        ASSERT(!mDeferredFlushCount);
        onRenderPassFinished();
        return angle::Result::Continue;
    }

    ANGLE_TRY(pauseOcclusionQueryIfActive());

    mCurrentTransformFeedbackBuffers.clear();
    mCurrentIndirectBuffer = nullptr;

    // Reset serials for XFB if active.
    if (mState.isTransformFeedbackActiveUnpaused())
    {
        const gl::ProgramExecutable *executable = mState.getProgramExecutable();
        ASSERT(executable);
        size_t xfbBufferCount = executable->getTransformFeedbackBufferCount();

        TransformFeedbackVk *transformFeedbackVk =
            vk::GetImpl(mState.getCurrentTransformFeedback());

        populateTransformFeedbackBufferSet(xfbBufferCount, transformFeedbackVk->getBufferHelpers());
    }

    onRenderPassFinished();

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("RP", mPerfCounters.renderPasses);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_BEGIN, eventName));
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    addOverlayUsedBuffersCount(mRenderPassCommands);

    resumeTransformFeedbackIfStarted();

    mRenderPassCommands->endRenderPass(this);

    if (vk::CommandBufferHelper::kEnableCommandStreamDiagnostics)
    {
        mRenderPassCommands->addCommandDiagnostics(this);
    }

    vk::RenderPass *renderPass = nullptr;
    ANGLE_TRY(getRenderPassWithOps(mRenderPassCommands->getRenderPassDesc(),
                                   mRenderPassCommands->getAttachmentOps(), &renderPass));

    if (mRenderer->getFeatures().commandProcessor.enabled)
    {
        mRenderPassCommands->markClosed();
        vk::CommandProcessorTask flushToPrimary;
        flushToPrimary.initProcessCommands(this, mRenderPassCommands, renderPass);
        ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::flushInsideRenderPassCommands");
        commandProcessorSyncErrorsAndQueueCommand(&flushToPrimary);
        getNextAvailableCommandBuffer(&mRenderPassCommands, true);
    }
    else
    {
        ANGLE_TRY(mCommandQueue.flushRenderPassCommands(this, *renderPass, mRenderPassCommands));
    }

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("RP", mPerfCounters.renderPasses);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_END, eventName));
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    if (mDeferredFlushCount > 0)
    {
        // If we have deferred glFlush call in the middle of renderpass, flush them now.
        ANGLE_TRY(flushImpl(nullptr));
    }

    return angle::Result::Continue;
}

void ContextVk::getNextAvailableCommandBuffer(vk::CommandBufferHelper **commandBuffer,
                                              bool hasRenderPass)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::getNextAvailableCommandBuffer");
    std::unique_lock<std::mutex> lock(mCommandBufferQueueMutex);
    // Only wake if notified and command queue is not empty
    mAvailableCommandBufferCondition.wait(lock,
                                          [this] { return !mAvailableCommandBuffers.empty(); });
    *commandBuffer = mAvailableCommandBuffers.front();
    ASSERT((*commandBuffer)->empty());
    mAvailableCommandBuffers.pop();
    lock.unlock();
    (*commandBuffer)->setHasRenderPass(hasRenderPass);
    (*commandBuffer)->markOpen();
}

void ContextVk::recycleCommandBuffer(vk::CommandBufferHelper *commandBuffer)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::recycleCommandBuffer");
    std::lock_guard<std::mutex> queueLock(mCommandBufferQueueMutex);
    ASSERT(commandBuffer->empty());
    mAvailableCommandBuffers.push(commandBuffer);
    mAvailableCommandBufferCondition.notify_one();
}

angle::Result ContextVk::syncExternalMemory()
{
    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    mOutsideRenderPassCommands->getCommandBuffer().memoryBarrier(
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &memoryBarrier);
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
    if (mOutsideRenderPassCommands->empty())
    {
        return angle::Result::Continue;
    }

    addOverlayUsedBuffersCount(mOutsideRenderPassCommands);

    if (vk::CommandBufferHelper::kEnableCommandStreamDiagnostics)
    {
        mOutsideRenderPassCommands->addCommandDiagnostics(this);
    }

    if (mRenderer->getFeatures().commandProcessor.enabled)
    {
        mOutsideRenderPassCommands->markClosed();
        vk::CommandProcessorTask flushToPrimary;
        flushToPrimary.initProcessCommands(this, mOutsideRenderPassCommands, nullptr);
        ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::flushOutsideRenderPassCommands");
        commandProcessorSyncErrorsAndQueueCommand(&flushToPrimary);
        getNextAvailableCommandBuffer(&mOutsideRenderPassCommands, false);
    }
    else
    {
        ANGLE_TRY(mCommandQueue.flushOutsideRPCommands(this, mOutsideRenderPassCommands));
    }
    mPerfCounters.flushedOutsideRenderPassCommandBuffers++;
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
    if (mRenderPassCommands->started() && mRenderPassCommandBuffer)
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

angle::Result ContextVk::pauseOcclusionQueryIfActive()
{
    if (mRenderPassCommandBuffer == nullptr)
    {
        return angle::Result::Continue;
    }

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

    return angle::Result::Continue;
}

angle::Result ContextVk::resumeOcclusionQueryIfActive()
{
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

    return angle::Result::Continue;
}

bool ContextVk::isRobustResourceInitEnabled() const
{
    return mState.isRobustResourceInitEnabled();
}

template <typename T, const T *VkWriteDescriptorSet::*pInfo>
void ContextVk::growDesciptorCapacity(std::vector<T> *descriptorVector, size_t newSize)
{
    const T *const oldInfoStart = descriptorVector->empty() ? nullptr : &(*descriptorVector)[0];
    size_t newCapacity          = std::max(descriptorVector->capacity() << 1, newSize);
    descriptorVector->reserve(newCapacity);

    if (oldInfoStart)
    {
        // patch mWriteInfo with new BufferInfo/ImageInfo pointers
        for (VkWriteDescriptorSet &set : mWriteDescriptorSets)
        {
            if (set.*pInfo)
            {
                size_t index = set.*pInfo - oldInfoStart;
                set.*pInfo   = &(*descriptorVector)[index];
            }
        }
    }
}

template <typename T, const T *VkWriteDescriptorSet::*pInfo>
T *ContextVk::allocDescriptorInfos(std::vector<T> *descriptorVector, size_t count)
{
    size_t oldSize = descriptorVector->size();
    size_t newSize = oldSize + count;
    if (newSize > descriptorVector->capacity())
    {
        // If we have reached capacity, grow the storage and patch the descriptor set with new
        // buffer info pointer
        growDesciptorCapacity<T, pInfo>(descriptorVector, newSize);
    }
    descriptorVector->resize(newSize);
    return &(*descriptorVector)[oldSize];
}

VkDescriptorBufferInfo *ContextVk::allocDescriptorBufferInfos(size_t count)
{
    return allocDescriptorInfos<VkDescriptorBufferInfo, &VkWriteDescriptorSet::pBufferInfo>(
        &mDescriptorBufferInfos, count);
}

VkDescriptorImageInfo *ContextVk::allocDescriptorImageInfos(size_t count)
{
    return allocDescriptorInfos<VkDescriptorImageInfo, &VkWriteDescriptorSet::pImageInfo>(
        &mDescriptorImageInfos, count);
}

VkWriteDescriptorSet *ContextVk::allocWriteDescriptorSets(size_t count)
{
    mPerfCounters.writeDescriptorSets += count;

    size_t oldSize = mWriteDescriptorSets.size();
    size_t newSize = oldSize + count;
    mWriteDescriptorSets.resize(newSize);
    return &mWriteDescriptorSets[oldSize];
}

void ContextVk::setDefaultUniformBlocksMinSizeForTesting(size_t minSize)
{
    mDefaultUniformStorage.setMinimumSizeForTesting(minSize);
}

void ContextVk::invalidateGraphicsPipelineAndDescriptorSets()
{
    mGraphicsDirtyBits.set(DIRTY_BIT_PIPELINE);
    mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
}

angle::Result ContextVk::updateRenderPassDepthStencilAccess()
{
    if (hasStartedRenderPass() && mDrawFramebuffer->getDepthStencilRenderTarget())
    {
        const gl::DepthStencilState &dsState = mState.getDepthStencilState();
        vk::ResourceAccess depthAccess       = GetDepthAccess(dsState);
        vk::ResourceAccess stencilAccess     = GetStencilAccess(dsState);

        if ((depthAccess == vk::ResourceAccess::Write ||
             stencilAccess == vk::ResourceAccess::Write) &&
            mDrawFramebuffer->isReadOnlyDepthFeedbackLoopMode())
        {
            // If we are switching out of read only mode and we are in feedback loop, we must end
            // renderpass here. Otherwise, updating it to writeable layout will produce a writable
            // feedback loop that is illegal in vulkan and will trigger validation errors that depth
            // texture is using the writable layout.
            ANGLE_TRY(flushCommandsAndEndRenderPass());
            // Clear read-only depth feedback mode.
            mDrawFramebuffer->setReadOnlyDepthFeedbackLoopMode(false);
        }
        else
        {
            mRenderPassCommands->onDepthAccess(depthAccess);
            mRenderPassCommands->onStencilAccess(stencilAccess);

            mDrawFramebuffer->updateRenderPassReadOnlyDepthMode(this, mRenderPassCommands);
        }
    }

    return angle::Result::Continue;
}

bool ContextVk::shouldSwitchToReadOnlyDepthFeedbackLoopMode(const gl::Context *context,
                                                            gl::Texture *texture) const
{
    const gl::ProgramExecutable *programExecutable = mState.getProgramExecutable();

    // When running compute we don't have a draw FBO.
    if (programExecutable->isCompute())
    {
        return false;
    }

    return texture->isDepthOrStencil() &&
           texture->isBoundToFramebuffer(mDrawFramebuffer->getState().getFramebufferSerial()) &&
           !mDrawFramebuffer->isReadOnlyDepthFeedbackLoopMode();
}

// Requires that trace is enabled to see the output, which is supported with is_debug=true
void ContextVk::outputCumulativePerfCounters()
{
    if (!vk::kOutputCumulativePerfCounters)
    {
        return;
    }

    {
        INFO() << "Context Descriptor Set Allocations: ";

        for (size_t pipelineType = 0;
             pipelineType < mObjectPerfCounters.descriptorSetsAllocated.size(); ++pipelineType)
        {
            uint32_t count = mObjectPerfCounters.descriptorSetsAllocated[pipelineType];
            if (count > 0)
            {
                INFO() << "    PipelineType " << pipelineType << ": " << count;
            }
        }
    }
}
}  // namespace rx
