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
// If the total size of copyBufferToImage commands in the outside command buffer reaches the
// threshold below, the latter is flushed.
static constexpr VkDeviceSize kMaxBufferToImageCopySize = 64 * 1024 * 1024;
constexpr size_t kDescriptorBufferViewsInitialSize      = 0;

// For shader uniforms such as gl_DepthRange and the viewport size.
struct GraphicsDriverUniforms
{
    // Contain packed 8-bit values for atomic counter buffer offsets.  These offsets are within
    // Vulkan's minStorageBufferOffsetAlignment limit and are used to support unaligned offsets
    // allowed in GL.
    std::array<uint32_t, 2> acbBufferOffsets;

    // .x is near, .y is far
    std::array<float, 2> depthRange;

    // Used to flip gl_FragCoord.  Packed uvec2
    uint32_t renderArea;

    // Packed vec4 of snorm8
    uint32_t flipXY;

    // Only the lower 16 bits used
    uint32_t dither;

    // Various bits of state:
    // - Surface rotation
    // - Advanced blend equation
    // - Sample count
    // - Enabled clip planes
    // - Depth transformation
    uint32_t misc;
};
static_assert(sizeof(GraphicsDriverUniforms) % (sizeof(uint32_t) * 4) == 0,
              "GraphicsDriverUniforms should be 16bytes aligned");

// Only used when transform feedback is emulated.
struct GraphicsDriverUniformsExtended
{
    GraphicsDriverUniforms common;

    // Only used with transform feedback emulation
    std::array<int32_t, 4> xfbBufferOffsets;
    int32_t xfbVerticesPerInstance;

    int32_t padding[3];
};
static_assert(sizeof(GraphicsDriverUniformsExtended) % (sizeof(uint32_t) * 4) == 0,
              "GraphicsDriverUniformsExtended should be 16bytes aligned");

struct ComputeDriverUniforms
{
    // Atomic counter buffer offsets with the same layout as in GraphicsDriverUniforms.
    std::array<uint32_t, 4> acbBufferOffsets;
};

uint32_t MakeFlipUniform(bool flipX, bool flipY, bool invertViewport)
{
    // Create snorm values of either -1 or 1, based on whether flipping is enabled or not
    // respectively.
    constexpr uint8_t kSnormOne      = 0x7F;
    constexpr uint8_t kSnormMinusOne = 0x81;

    // .xy are flips for the fragment stage.
    uint32_t x = flipX ? kSnormMinusOne : kSnormOne;
    uint32_t y = flipY ? kSnormMinusOne : kSnormOne;

    // .zw are flips for the vertex stage.
    uint32_t z = x;
    uint32_t w = flipY != invertViewport ? kSnormMinusOne : kSnormOne;

    return x | y << 8 | z << 16 | w << 24;
}

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
    {gl::ShaderType::TessControl, vk::ImageLayout::PreFragmentShadersReadOnly},
    {gl::ShaderType::TessEvaluation, vk::ImageLayout::PreFragmentShadersReadOnly},
    {gl::ShaderType::Geometry, vk::ImageLayout::PreFragmentShadersReadOnly},
    {gl::ShaderType::Fragment, vk::ImageLayout::FragmentShaderReadOnly},
    {gl::ShaderType::Compute, vk::ImageLayout::ComputeShaderReadOnly}};

constexpr gl::ShaderMap<vk::ImageLayout> kShaderWriteImageLayouts = {
    {gl::ShaderType::Vertex, vk::ImageLayout::VertexShaderWrite},
    {gl::ShaderType::TessControl, vk::ImageLayout::PreFragmentShadersWrite},
    {gl::ShaderType::TessEvaluation, vk::ImageLayout::PreFragmentShadersWrite},
    {gl::ShaderType::Geometry, vk::ImageLayout::PreFragmentShadersWrite},
    {gl::ShaderType::Fragment, vk::ImageLayout::FragmentShaderWrite},
    {gl::ShaderType::Compute, vk::ImageLayout::ComputeShaderWrite}};

constexpr VkBufferUsageFlags kVertexBufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
constexpr size_t kDynamicVertexDataSize         = 16 * 1024;

bool CanMultiDrawIndirectUseCmd(ContextVk *contextVk,
                                VertexArrayVk *vertexArray,
                                gl::PrimitiveMode mode,
                                GLsizei drawcount,
                                GLsizei stride)
{
    // Use the generic implementation if multiDrawIndirect is disabled, if line loop is being used
    // for multiDraw, if drawcount is greater than maxDrawIndirectCount, or if there are streaming
    // vertex attributes.
    ASSERT(drawcount > 1);
    const bool supportsMultiDrawIndirect =
        contextVk->getFeatures().supportsMultiDrawIndirect.enabled;
    const bool isMultiDrawLineLoop = (mode == gl::PrimitiveMode::LineLoop);
    const bool isDrawCountBeyondLimit =
        (static_cast<uint32_t>(drawcount) >
         contextVk->getRenderer()->getPhysicalDeviceProperties().limits.maxDrawIndirectCount);
    const bool isMultiDrawWithStreamingAttribs = vertexArray->getStreamingVertexAttribsMask().any();

    const bool canMultiDrawIndirectUseCmd = supportsMultiDrawIndirect && !isMultiDrawLineLoop &&
                                            !isDrawCountBeyondLimit &&
                                            !isMultiDrawWithStreamingAttribs;
    return canMultiDrawIndirectUseCmd;
}

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

void ApplySampleCoverage(const gl::State &glState, uint32_t coverageSampleCount, uint32_t *maskOut)
{
    if (!glState.isSampleCoverageEnabled())
    {
        return;
    }

    uint32_t coverageMask = angle::BitMask<uint32_t>(coverageSampleCount);

    if (glState.getSampleCoverageInvert())
    {
        coverageMask = ~coverageMask;
    }

    *maskOut &= coverageMask;
}

bool IsRenderPassStartedAndUsesImage(const vk::RenderPassCommandBufferHelper &renderPassCommands,
                                     const vk::ImageHelper &image)
{
    return renderPassCommands.started() && renderPassCommands.usesImage(image);
}

bool IsRenderPassStartedAndTransitionsImageLayout(
    const vk::RenderPassCommandBufferHelper &renderPassCommands,
    vk::ImageHelper &image)
{
    return renderPassCommands.started() && renderPassCommands.isImageWithLayoutTransition(image);
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
EventName GetTraceEventName(const char *title, uint64_t counter)
{
    EventName buf;
    snprintf(buf.data(), kMaxGpuEventNameLen - 1, "%s %llu", title,
             static_cast<unsigned long long>(counter));
    return buf;
}

vk::ResourceAccess GetColorAccess(const gl::State &state,
                                  const gl::FramebufferState &framebufferState,
                                  const gl::DrawBufferMask &emulatedAlphaMask,
                                  bool hasFramebufferFetch,
                                  size_t colorIndexGL)
{
    // No access if draw buffer is disabled altogether
    // Without framebuffer fetch:
    //   No access if color output is masked, or rasterizer discard is enabled
    // With framebuffer fetch:
    //   Read access if color output is masked, or rasterizer discard is enabled

    if (!framebufferState.getEnabledDrawBuffers().test(colorIndexGL))
    {
        return vk::ResourceAccess::Unused;
    }

    const gl::BlendStateExt &blendStateExt = state.getBlendStateExt();
    uint8_t colorMask                      = gl::BlendStateExt::ColorMaskStorage::GetValueIndexed(
                             colorIndexGL, blendStateExt.getColorMaskBits());
    if (emulatedAlphaMask[colorIndexGL])
    {
        colorMask &= ~VK_COLOR_COMPONENT_A_BIT;
    }
    const bool isOutputMasked = colorMask == 0 || state.isRasterizerDiscardEnabled();

    if (isOutputMasked)
    {
        return hasFramebufferFetch ? vk::ResourceAccess::ReadOnly : vk::ResourceAccess::Unused;
    }

    return vk::ResourceAccess::Write;
}

vk::ResourceAccess GetDepthAccess(const gl::DepthStencilState &dsState,
                                  UpdateDepthFeedbackLoopReason reason)
{
    // Skip if depth/stencil not actually accessed.
    if (reason == UpdateDepthFeedbackLoopReason::None)
    {
        return vk::ResourceAccess::Unused;
    }

    // Note that clear commands don't respect depth test enable, only the mask
    // Note Other state can be stated here too in the future, such as rasterizer discard.
    if (!dsState.depthTest && reason != UpdateDepthFeedbackLoopReason::Clear)
    {
        return vk::ResourceAccess::Unused;
    }
    return dsState.isDepthMaskedOut() ? vk::ResourceAccess::ReadOnly : vk::ResourceAccess::Write;
}

vk::ResourceAccess GetStencilAccess(const gl::DepthStencilState &dsState,
                                    UpdateDepthFeedbackLoopReason reason)
{
    // Skip if depth/stencil not actually accessed.
    if (reason == UpdateDepthFeedbackLoopReason::None)
    {
        return vk::ResourceAccess::Unused;
    }

    // Note that clear commands don't respect stencil test enable, only the mask
    // Note Other state can be stated here too in the future, such as rasterizer discard.
    if (!dsState.stencilTest && reason != UpdateDepthFeedbackLoopReason::Clear)
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

vk::ImageLayout GetImageReadLayout(TextureVk *textureVk,
                                   const gl::ProgramExecutable &executable,
                                   size_t textureUnit,
                                   PipelineType pipelineType)
{
    vk::ImageHelper &image = textureVk->getImage();

    // If this texture has been bound as image and currently executable program accesses images,
    // we consider this image's layout as writeable.
    if (textureVk->hasBeenBoundAsImage() && executable.hasImages())
    {
        return pipelineType == PipelineType::Compute ? vk::ImageLayout::ComputeShaderWrite
                                                     : vk::ImageLayout::AllGraphicsShadersWrite;
    }

    gl::ShaderBitSet remainingShaderBits =
        executable.getSamplerShaderBitsForTextureUnitIndex(textureUnit);
    ASSERT(remainingShaderBits.any());
    gl::ShaderType firstShader = remainingShaderBits.first();
    gl::ShaderType lastShader  = remainingShaderBits.last();
    remainingShaderBits.reset(firstShader);
    remainingShaderBits.reset(lastShader);

    if (image.hasRenderPassUsageFlag(vk::RenderPassUsage::RenderTargetAttachment))
    {
        // Right now we set this flag only when RenderTargetAttachment is set since we do
        // not track all textures in the renderpass.
        image.setRenderPassUsageFlag(vk::RenderPassUsage::TextureSampler);

        if (image.isDepthOrStencil())
        {
            if (image.hasRenderPassUsageFlag(vk::RenderPassUsage::ReadOnlyAttachment))
            {
                if (firstShader == gl::ShaderType::Fragment)
                {
                    ASSERT(remainingShaderBits.none() && lastShader == firstShader);
                    return vk::ImageLayout::DSAttachmentReadAndFragmentShaderRead;
                }
                return vk::ImageLayout::DSAttachmentReadAndAllShadersRead;
            }

            return firstShader == gl::ShaderType::Fragment
                       ? vk::ImageLayout::DSAttachmentWriteAndFragmentShaderRead
                       : vk::ImageLayout::DSAttachmentWriteAndAllShadersRead;
        }

        return firstShader == gl::ShaderType::Fragment
                   ? vk::ImageLayout::ColorAttachmentAndFragmentShaderRead
                   : vk::ImageLayout::ColorAttachmentAndAllShadersRead;
    }

    if (image.isDepthOrStencil())
    {
        // We always use a depth-stencil read-only layout for any depth Textures to simplify
        // our implementation's handling of depth-stencil read-only mode. We don't have to
        // split a RenderPass to transition a depth texture from shader-read to read-only.
        // This improves performance in Manhattan. Future optimizations are likely possible
        // here including using specialized barriers without breaking the RenderPass.
        if (firstShader == gl::ShaderType::Fragment)
        {
            ASSERT(remainingShaderBits.none() && lastShader == firstShader);
            return vk::ImageLayout::DSAttachmentReadAndFragmentShaderRead;
        }
        return vk::ImageLayout::DSAttachmentReadAndAllShadersRead;
    }

    // We barrier against either:
    // - Vertex only
    // - Fragment only
    // - Pre-fragment only (vertex, geometry and tessellation together)
    if (remainingShaderBits.any() || firstShader != lastShader)
    {
        return lastShader == gl::ShaderType::Fragment ? vk::ImageLayout::AllGraphicsShadersReadOnly
                                                      : vk::ImageLayout::PreFragmentShadersReadOnly;
    }

    return kShaderReadOnlyImageLayouts[firstShader];
}

vk::ImageLayout GetImageWriteLayoutAndSubresource(const gl::ImageUnit &imageUnit,
                                                  vk::ImageHelper &image,
                                                  gl::ShaderBitSet shaderStages,
                                                  gl::LevelIndex *levelOut,
                                                  uint32_t *layerStartOut,
                                                  uint32_t *layerCountOut)
{
    *levelOut = gl::LevelIndex(static_cast<uint32_t>(imageUnit.level));

    *layerStartOut = 0;
    *layerCountOut = image.getLayerCount();
    if (imageUnit.layered)
    {
        *layerStartOut = imageUnit.layered;
        *layerCountOut = 1;
    }

    gl::ShaderType firstShader = shaderStages.first();
    gl::ShaderType lastShader  = shaderStages.last();
    shaderStages.reset(firstShader);
    shaderStages.reset(lastShader);
    // We barrier against either:
    // - Vertex only
    // - Fragment only
    // - Pre-fragment only (vertex, geometry and tessellation together)
    if (shaderStages.any() || firstShader != lastShader)
    {
        return lastShader == gl::ShaderType::Fragment ? vk::ImageLayout::AllGraphicsShadersWrite
                                                      : vk::ImageLayout::PreFragmentShadersWrite;
    }

    return kShaderWriteImageLayouts[firstShader];
}

void OnTextureBufferRead(ContextVk *contextVk,
                         BufferVk *bufferVk,
                         gl::ShaderBitSet stages,
                         vk::CommandBufferHelperCommon *commandBufferHelper)
{
    vk::BufferHelper &buffer = bufferVk->getBuffer();

    ASSERT(stages.any());

    // TODO: accept multiple stages in bufferRead.  http://anglebug.com/3573
    for (gl::ShaderType stage : stages)
    {
        // Note: if another range of the same buffer is simultaneously used for storage,
        // such as for transform feedback output, or SSBO, unnecessary barriers can be
        // generated.
        commandBufferHelper->bufferRead(contextVk, VK_ACCESS_SHADER_READ_BIT,
                                        vk::GetPipelineStage(stage), &buffer);
    }
}

void OnImageBufferWrite(ContextVk *contextVk,
                        BufferVk *bufferVk,
                        gl::ShaderBitSet stages,
                        vk::CommandBufferHelperCommon *commandBufferHelper)
{
    vk::BufferHelper &buffer = bufferVk->getBuffer();

    // TODO: accept multiple stages in bufferWrite.  http://anglebug.com/3573
    for (gl::ShaderType stage : stages)
    {
        commandBufferHelper->bufferWrite(contextVk,
                                         VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                         vk::GetPipelineStage(stage), &buffer);
    }
}

constexpr angle::PackedEnumMap<RenderPassClosureReason, const char *> kRenderPassClosureReason = {{
    {RenderPassClosureReason::AlreadySpecifiedElsewhere, nullptr},
    {RenderPassClosureReason::ContextDestruction, "Render pass closed due to context destruction"},
    {RenderPassClosureReason::ContextChange, "Render pass closed due to context change"},
    {RenderPassClosureReason::GLFlush, "Render pass closed due to glFlush()"},
    {RenderPassClosureReason::GLFinish, "Render pass closed due to glFinish()"},
    {RenderPassClosureReason::EGLSwapBuffers, "Render pass closed due to eglSwapBuffers()"},
    {RenderPassClosureReason::EGLWaitClient, "Render pass closed due to eglWaitClient()"},
    {RenderPassClosureReason::FramebufferBindingChange,
     "Render pass closed due to framebuffer binding change"},
    {RenderPassClosureReason::FramebufferChange, "Render pass closed due to framebuffer change"},
    {RenderPassClosureReason::NewRenderPass,
     "Render pass closed due to starting a new render pass"},
    {RenderPassClosureReason::BufferUseThenXfbWrite,
     "Render pass closed due to buffer use as transform feedback output after prior use in render "
     "pass"},
    {RenderPassClosureReason::XfbWriteThenVertexIndexBuffer,
     "Render pass closed due to transform feedback buffer use as vertex/index input"},
    {RenderPassClosureReason::XfbWriteThenIndirectDrawBuffer,
     "Render pass closed due to indirect draw buffer previously used as transform feedback output "
     "in render pass"},
    {RenderPassClosureReason::XfbResumeAfterDrawBasedClear,
     "Render pass closed due to transform feedback resume after clear through draw"},
    {RenderPassClosureReason::DepthStencilUseInFeedbackLoop,
     "Render pass closed due to depth/stencil attachment use under feedback loop"},
    {RenderPassClosureReason::DepthStencilWriteAfterFeedbackLoop,
     "Render pass closed due to depth/stencil attachment write after feedback loop"},
    {RenderPassClosureReason::PipelineBindWhileXfbActive,
     "Render pass closed due to graphics pipeline change while transform feedback is active"},
    {RenderPassClosureReason::BufferWriteThenMap,
     "Render pass closed due to mapping buffer being written to by said render pass"},
    {RenderPassClosureReason::BufferWriteThenOutOfRPRead,
     "Render pass closed due to non-render-pass read of buffer that was written to in render pass"},
    {RenderPassClosureReason::BufferUseThenOutOfRPWrite,
     "Render pass closed due to non-render-pass write of buffer that was used in render pass"},
    {RenderPassClosureReason::ImageUseThenOutOfRPRead,
     "Render pass closed due to non-render-pass read of image that was used in render pass"},
    {RenderPassClosureReason::ImageUseThenOutOfRPWrite,
     "Render pass closed due to non-render-pass write of image that was used in render pass"},
    {RenderPassClosureReason::XfbWriteThenComputeRead,
     "Render pass closed due to compute read of buffer previously used as transform feedback "
     "output in render pass"},
    {RenderPassClosureReason::XfbWriteThenIndirectDispatchBuffer,
     "Render pass closed due to indirect dispatch buffer previously used as transform feedback "
     "output in render pass"},
    {RenderPassClosureReason::ImageAttachmentThenComputeRead,
     "Render pass closed due to compute read of image previously used as framebuffer attachment in "
     "render pass"},
    {RenderPassClosureReason::GetQueryResult, "Render pass closed due to getting query result"},
    {RenderPassClosureReason::BeginNonRenderPassQuery,
     "Render pass closed due to non-render-pass query begin"},
    {RenderPassClosureReason::EndNonRenderPassQuery,
     "Render pass closed due to non-render-pass query end"},
    {RenderPassClosureReason::TimestampQuery, "Render pass closed due to timestamp query"},
    {RenderPassClosureReason::GLReadPixels, "Render pass closed due to glReadPixels()"},
    {RenderPassClosureReason::BufferUseThenReleaseToExternal,
     "Render pass closed due to buffer (used by render pass) release to external"},
    {RenderPassClosureReason::ImageUseThenReleaseToExternal,
     "Render pass closed due to image (used by render pass) release to external"},
    {RenderPassClosureReason::BufferInUseWhenSynchronizedMap,
     "Render pass closed due to mapping buffer in use by GPU without GL_MAP_UNSYNCHRONIZED_BIT"},
    {RenderPassClosureReason::ImageOrphan, "Render pass closed due to EGL image being orphaned"},
    {RenderPassClosureReason::GLMemoryBarrierThenStorageResource,
     "Render pass closed due to glMemoryBarrier before storage output in render pass"},
    {RenderPassClosureReason::StorageResourceUseThenGLMemoryBarrier,
     "Render pass closed due to glMemoryBarrier after storage output in render pass"},
    {RenderPassClosureReason::ExternalSemaphoreSignal,
     "Render pass closed due to external semaphore signal"},
    {RenderPassClosureReason::SyncObjectInit, "Render pass closed due to sync object insertion"},
    {RenderPassClosureReason::SyncObjectWithFdInit,
     "Render pass closed due to sync object with fd insertion"},
    {RenderPassClosureReason::SyncObjectClientWait,
     "Render pass closed due to sync object client wait"},
    {RenderPassClosureReason::SyncObjectServerWait,
     "Render pass closed due to sync object server wait"},
    {RenderPassClosureReason::SyncObjectGetStatus,
     "Render pass closed due to sync object get status"},
    {RenderPassClosureReason::XfbPause, "Render pass closed due to transform feedback pause"},
    {RenderPassClosureReason::FramebufferFetchEmulation,
     "Render pass closed due to framebuffer fetch emulation"},
    {RenderPassClosureReason::ColorBufferInvalidate,
     "Render pass closed due to glInvalidateFramebuffer() on a color buffer"},
    {RenderPassClosureReason::GenerateMipmapOnCPU,
     "Render pass closed due to fallback to CPU when generating mipmaps"},
    {RenderPassClosureReason::CopyTextureOnCPU,
     "Render pass closed due to fallback to CPU when copying texture"},
    {RenderPassClosureReason::TextureReformatToRenderable,
     "Render pass closed due to reformatting texture to a renderable fallback"},
    {RenderPassClosureReason::DeviceLocalBufferMap,
     "Render pass closed due to mapping device local buffer"},
    {RenderPassClosureReason::PrepareForBlit, "Render pass closed prior to draw-based blit"},
    {RenderPassClosureReason::PrepareForImageCopy,
     "Render pass closed prior to draw-based image copy"},
    {RenderPassClosureReason::TemporaryForImageClear,
     "Temporary render pass used for image clear closed"},
    {RenderPassClosureReason::TemporaryForImageCopy,
     "Temporary render pass used for image copy closed"},
    {RenderPassClosureReason::TemporaryForOverlayDraw,
     "Temporary render pass used for overlay draw closed"},
}};

VkDependencyFlags GetLocalDependencyFlags(ContextVk *contextVk)
{
    VkDependencyFlags dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    if (contextVk->getCurrentViewCount() > 0)
    {
        dependencyFlags |= VK_DEPENDENCY_VIEW_LOCAL_BIT;
    }
    return dependencyFlags;
}

void DumpPipelineCacheGraph(ContextVk *contextVk, const std::ostringstream &graph)
{
    std::ostream &out = std::cout;

    out << "digraph {\n"
        << " node [shape=box";
    if (contextVk->getFeatures().supportsPipelineCreationFeedback.enabled)
    {
        out << ",color=green";
    }
    out << "]\n";
    out << graph.str();
    out << "}\n";
}

bool BlendModeSupportsDither(const gl::State &state, size_t colorIndex)
{
    // Specific combinations of color blend modes are known to work with our dithering emulation.
    // Note we specifically don't check alpha blend, as dither isn't applied to alpha.
    // See http://b/232574868 for more discussion and reasoning.
    return state.getBlendStateExt().getSrcColorIndexed(colorIndex) == GL_SRC_ALPHA &&
           state.getBlendStateExt().getDstColorIndexed(colorIndex) == GL_ONE_MINUS_SRC_ALPHA;
}

bool shouldUseGraphicsDriverUniformsExtended(const ContextVk *contextVk)
{
    return contextVk->getFeatures().emulateTransformFeedback.enabled;
}

}  // anonymous namespace

void ContextVk::flushDescriptorSetUpdates()
{
    mPerfCounters.writeDescriptorSets +=
        mUpdateDescriptorSetsBuilder.flushDescriptorSetUpdates(getDevice());
}

ANGLE_INLINE void ContextVk::onRenderPassFinished(RenderPassClosureReason reason)
{
    pauseRenderPassQueriesIfActive();

    if (mRenderPassCommandBuffer != nullptr)
    {
        // If reason is specified, add it to the command buffer right before ending the render pass,
        // so it will show up in GPU debuggers.
        const char *reasonText = kRenderPassClosureReason[reason];
        if (reasonText)
        {
            insertEventMarkerImpl(GL_DEBUG_SOURCE_API, reasonText);
        }
    }

    mRenderPassCommandBuffer = nullptr;
    mGraphicsDirtyBits.set(DIRTY_BIT_RENDER_PASS);
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
      mActiveRenderPassQueries{},
      mLastIndexBufferOffset(nullptr),
      mCurrentIndexBufferOffset(0),
      mCurrentDrawElementsType(gl::DrawElementsType::InvalidEnum),
      mXfbBaseVertex(0),
      mXfbVertexCountPerInstance(0),
      mClearColorValue{},
      mClearDepthStencilValue{},
      mClearColorMasks(0),
      mFlipYForCurrentSurface(false),
      mFlipViewportForDrawFramebuffer(false),
      mFlipViewportForReadFramebuffer(false),
      mIsAnyHostVisibleBufferWritten(false),
      mEmulateSeamfulCubeMapSampling(false),
      mOutsideRenderPassCommands(nullptr),
      mRenderPassCommands(nullptr),
      mQueryEventType(GraphicsEventCmdBuf::NotInQueryCmd),
      mGpuEventsEnabled(false),
      mPrimaryBufferEventCounter(0),
      mHasDeferredFlush(false),
      mHasAnyCommandsPendingSubmission(false),
      mIsInFramebufferFetchMode(false),
      mTotalBufferToImageCopySize(0),
      mGpuClockSync{std::numeric_limits<double>::max(), std::numeric_limits<double>::max()},
      mGpuEventTimestampOrigin(0),
      mContextPriority(renderer->getDriverPriority(GetContextPriority(state))),
      mShareGroupVk(vk::GetImpl(state.getShareGroup()))
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::ContextVk");
    memset(&mClearColorValue, 0, sizeof(mClearColorValue));
    memset(&mClearDepthStencilValue, 0, sizeof(mClearDepthStencilValue));
    memset(&mViewport, 0, sizeof(mViewport));
    memset(&mScissor, 0, sizeof(mScissor));

    // Ensure viewport is within Vulkan requirements
    vk::ClampViewport(&mViewport);

    mNonIndexedDirtyBitsMask.set();
    mNonIndexedDirtyBitsMask.reset(DIRTY_BIT_INDEX_BUFFER);

    mIndexedDirtyBitsMask.set();

    // Once a command buffer is ended, all bindings (through |vkCmdBind*| calls) are lost per Vulkan
    // spec.  Once a new command buffer is allocated, we must make sure every previously bound
    // resource is bound again.
    //
    // Note that currently these dirty bits are set every time a new render pass command buffer is
    // begun.  However, using ANGLE's SecondaryCommandBuffer, the Vulkan command buffer (which is
    // the primary command buffer) is not ended, so technically we don't need to rebind these.
    mNewGraphicsCommandBufferDirtyBits = DirtyBits{
        DIRTY_BIT_RENDER_PASS,      DIRTY_BIT_COLOR_ACCESS,     DIRTY_BIT_DEPTH_STENCIL_ACCESS,
        DIRTY_BIT_PIPELINE_BINDING, DIRTY_BIT_TEXTURES,         DIRTY_BIT_VERTEX_BUFFERS,
        DIRTY_BIT_INDEX_BUFFER,     DIRTY_BIT_SHADER_RESOURCES, DIRTY_BIT_DESCRIPTOR_SETS,
        DIRTY_BIT_DRIVER_UNIFORMS,
    };
    if (getFeatures().supportsTransformFeedbackExtension.enabled ||
        getFeatures().emulateTransformFeedback.enabled)
    {
        mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS);
    }

    mNewComputeCommandBufferDirtyBits =
        DirtyBits{DIRTY_BIT_PIPELINE_BINDING, DIRTY_BIT_TEXTURES, DIRTY_BIT_SHADER_RESOURCES,
                  DIRTY_BIT_DESCRIPTOR_SETS, DIRTY_BIT_DRIVER_UNIFORMS};

    mDynamicStateDirtyBits = DirtyBits{
        DIRTY_BIT_DYNAMIC_VIEWPORT,           DIRTY_BIT_DYNAMIC_SCISSOR,
        DIRTY_BIT_DYNAMIC_LINE_WIDTH,         DIRTY_BIT_DYNAMIC_DEPTH_BIAS,
        DIRTY_BIT_DYNAMIC_BLEND_CONSTANTS,    DIRTY_BIT_DYNAMIC_STENCIL_COMPARE_MASK,
        DIRTY_BIT_DYNAMIC_STENCIL_WRITE_MASK, DIRTY_BIT_DYNAMIC_STENCIL_REFERENCE,
    };
    if (getFeatures().supportsExtendedDynamicState.enabled)
    {
        mDynamicStateDirtyBits |= DirtyBits{
            DIRTY_BIT_DYNAMIC_CULL_MODE,
            DIRTY_BIT_DYNAMIC_FRONT_FACE,
            DIRTY_BIT_VERTEX_BUFFERS,
            DIRTY_BIT_DYNAMIC_DEPTH_TEST_ENABLE,
            DIRTY_BIT_DYNAMIC_DEPTH_WRITE_ENABLE,
            DIRTY_BIT_DYNAMIC_DEPTH_COMPARE_OP,
            DIRTY_BIT_DYNAMIC_STENCIL_TEST_ENABLE,
            DIRTY_BIT_DYNAMIC_STENCIL_OP,
        };
    }
    if (getFeatures().supportsExtendedDynamicState2.enabled)
    {
        mDynamicStateDirtyBits |= DirtyBits{
            DIRTY_BIT_DYNAMIC_RASTERIZER_DISCARD_ENABLE,
            DIRTY_BIT_DYNAMIC_DEPTH_BIAS_ENABLE,
            DIRTY_BIT_DYNAMIC_PRIMITIVE_RESTART_ENABLE,
        };
    }
    if (getFeatures().supportsFragmentShadingRate.enabled)
    {
        mDynamicStateDirtyBits.set(DIRTY_BIT_DYNAMIC_FRAGMENT_SHADING_RATE);
    }

    mNewGraphicsCommandBufferDirtyBits |= mDynamicStateDirtyBits;

    mGraphicsDirtyBitHandlers[DIRTY_BIT_MEMORY_BARRIER] =
        &ContextVk::handleDirtyGraphicsMemoryBarrier;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DEFAULT_ATTRIBS] =
        &ContextVk::handleDirtyGraphicsDefaultAttribs;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_PIPELINE_DESC] =
        &ContextVk::handleDirtyGraphicsPipelineDesc;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_READ_ONLY_DEPTH_FEEDBACK_LOOP_MODE] =
        &ContextVk::handleDirtyGraphicsReadOnlyDepthFeedbackLoopMode;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_RENDER_PASS]  = &ContextVk::handleDirtyGraphicsRenderPass;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_EVENT_LOG]    = &ContextVk::handleDirtyGraphicsEventLog;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_COLOR_ACCESS] = &ContextVk::handleDirtyGraphicsColorAccess;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DEPTH_STENCIL_ACCESS] =
        &ContextVk::handleDirtyGraphicsDepthStencilAccess;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_PIPELINE_BINDING] =
        &ContextVk::handleDirtyGraphicsPipelineBinding;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_TEXTURES] = &ContextVk::handleDirtyGraphicsTextures;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_VERTEX_BUFFERS] =
        &ContextVk::handleDirtyGraphicsVertexBuffers;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_INDEX_BUFFER] = &ContextVk::handleDirtyGraphicsIndexBuffer;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_UNIFORMS]     = &ContextVk::handleDirtyGraphicsUniforms;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DRIVER_UNIFORMS] =
        &ContextVk::handleDirtyGraphicsDriverUniforms;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_SHADER_RESOURCES] =
        &ContextVk::handleDirtyGraphicsShaderResources;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_FRAMEBUFFER_FETCH_BARRIER] =
        &ContextVk::handleDirtyGraphicsFramebufferFetchBarrier;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_BLEND_BARRIER] =
        &ContextVk::handleDirtyGraphicsBlendBarrier;
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mGraphicsDirtyBitHandlers[DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS] =
            &ContextVk::handleDirtyGraphicsTransformFeedbackBuffersExtension;
        mGraphicsDirtyBitHandlers[DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME] =
            &ContextVk::handleDirtyGraphicsTransformFeedbackResume;
    }
    else if (getFeatures().emulateTransformFeedback.enabled)
    {
        mGraphicsDirtyBitHandlers[DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS] =
            &ContextVk::handleDirtyGraphicsTransformFeedbackBuffersEmulation;
    }

    mGraphicsDirtyBitHandlers[DIRTY_BIT_DESCRIPTOR_SETS] =
        &ContextVk::handleDirtyGraphicsDescriptorSets;

    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_VIEWPORT] =
        &ContextVk::handleDirtyGraphicsDynamicViewport;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_SCISSOR] =
        &ContextVk::handleDirtyGraphicsDynamicScissor;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_LINE_WIDTH] =
        &ContextVk::handleDirtyGraphicsDynamicLineWidth;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_DEPTH_BIAS] =
        &ContextVk::handleDirtyGraphicsDynamicDepthBias;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_BLEND_CONSTANTS] =
        &ContextVk::handleDirtyGraphicsDynamicBlendConstants;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_STENCIL_COMPARE_MASK] =
        &ContextVk::handleDirtyGraphicsDynamicStencilCompareMask;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_STENCIL_WRITE_MASK] =
        &ContextVk::handleDirtyGraphicsDynamicStencilWriteMask;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_STENCIL_REFERENCE] =
        &ContextVk::handleDirtyGraphicsDynamicStencilReference;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_CULL_MODE] =
        &ContextVk::handleDirtyGraphicsDynamicCullMode;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_FRONT_FACE] =
        &ContextVk::handleDirtyGraphicsDynamicFrontFace;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_DEPTH_TEST_ENABLE] =
        &ContextVk::handleDirtyGraphicsDynamicDepthTestEnable;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_DEPTH_WRITE_ENABLE] =
        &ContextVk::handleDirtyGraphicsDynamicDepthWriteEnable;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_DEPTH_COMPARE_OP] =
        &ContextVk::handleDirtyGraphicsDynamicDepthCompareOp;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_STENCIL_TEST_ENABLE] =
        &ContextVk::handleDirtyGraphicsDynamicStencilTestEnable;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_STENCIL_OP] =
        &ContextVk::handleDirtyGraphicsDynamicStencilOp;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_RASTERIZER_DISCARD_ENABLE] =
        &ContextVk::handleDirtyGraphicsDynamicRasterizerDiscardEnable;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_DEPTH_BIAS_ENABLE] =
        &ContextVk::handleDirtyGraphicsDynamicDepthBiasEnable;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_PRIMITIVE_RESTART_ENABLE] =
        &ContextVk::handleDirtyGraphicsDynamicPrimitiveRestartEnable;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DYNAMIC_FRAGMENT_SHADING_RATE] =
        &ContextVk::handleDirtyGraphicsDynamicFragmentShadingRate;

    mComputeDirtyBitHandlers[DIRTY_BIT_MEMORY_BARRIER] =
        &ContextVk::handleDirtyComputeMemoryBarrier;
    mComputeDirtyBitHandlers[DIRTY_BIT_EVENT_LOG]     = &ContextVk::handleDirtyComputeEventLog;
    mComputeDirtyBitHandlers[DIRTY_BIT_PIPELINE_DESC] = &ContextVk::handleDirtyComputePipelineDesc;
    mComputeDirtyBitHandlers[DIRTY_BIT_PIPELINE_BINDING] =
        &ContextVk::handleDirtyComputePipelineBinding;
    mComputeDirtyBitHandlers[DIRTY_BIT_TEXTURES] = &ContextVk::handleDirtyComputeTextures;
    mComputeDirtyBitHandlers[DIRTY_BIT_UNIFORMS] = &ContextVk::handleDirtyComputeUniforms;
    mComputeDirtyBitHandlers[DIRTY_BIT_DRIVER_UNIFORMS] =
        &ContextVk::handleDirtyComputeDriverUniforms;
    mComputeDirtyBitHandlers[DIRTY_BIT_SHADER_RESOURCES] =
        &ContextVk::handleDirtyComputeShaderResources;
    mComputeDirtyBitHandlers[DIRTY_BIT_DESCRIPTOR_SETS] =
        &ContextVk::handleDirtyComputeDescriptorSets;

    mGraphicsDirtyBits = mNewGraphicsCommandBufferDirtyBits;
    mComputeDirtyBits  = mNewComputeCommandBufferDirtyBits;

    mActiveImages.fill(nullptr);

    // The following dirty bits don't affect the program pipeline:
    //
    // - READ_FRAMEBUFFER_BINDING only affects operations that read from said framebuffer,
    // - CLEAR_* only affect following clear calls,
    // - PACK/UNPACK_STATE only affect texture data upload/download,
    // - *_BINDING only affect descriptor sets.
    //
    // Additionally, state that is set dynamically doesn't invalidate the program pipeline.
    //
    mPipelineDirtyBitsMask.set();
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_CLEAR_COLOR);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_CLEAR_DEPTH);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_CLEAR_STENCIL);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_UNPACK_STATE);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_UNPACK_BUFFER_BINDING);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_PACK_STATE);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_PACK_BUFFER_BINDING);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_RENDERBUFFER_BINDING);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_DISPATCH_INDIRECT_BUFFER_BINDING);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_SAMPLER_BINDINGS);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_TEXTURE_BINDINGS);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_IMAGE_BINDINGS);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_TRANSFORM_FEEDBACK_BINDING);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_UNIFORM_BUFFER_BINDINGS);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_SHADER_STORAGE_BUFFER_BINDING);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING);

    // Dynamic state in core Vulkan 1.0:
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_VIEWPORT);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_SCISSOR_TEST_ENABLED);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_SCISSOR);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_LINE_WIDTH);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_POLYGON_OFFSET);
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_BLEND_COLOR);
    if (!getFeatures().useNonZeroStencilWriteMaskStaticState.enabled)
    {
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_STENCIL_WRITEMASK_BACK);
    }

    // Dynamic state in VK_EXT_extended_dynamic_state:
    if (getFeatures().supportsExtendedDynamicState.enabled)
    {
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_CULL_FACE_ENABLED);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_CULL_FACE);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_FRONT_FACE);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_DEPTH_TEST_ENABLED);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_DEPTH_MASK);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_DEPTH_FUNC);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_STENCIL_TEST_ENABLED);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_STENCIL_FUNCS_FRONT);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_STENCIL_FUNCS_BACK);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_STENCIL_OPS_FRONT);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_STENCIL_OPS_BACK);
    }

    if (getFeatures().supportsExtendedDynamicState2.enabled)
    {
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED);
        mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_PRIMITIVE_RESTART_ENABLED);
    }

    angle::PerfMonitorCounterGroup vulkanGroup;
    vulkanGroup.name = "vulkan";

#define ANGLE_ADD_PERF_MONITOR_COUNTER_GROUP(COUNTER) \
    {                                                 \
        angle::PerfMonitorCounter counter;            \
        counter.name  = #COUNTER;                     \
        counter.value = 0;                            \
        vulkanGroup.counters.push_back(counter);      \
    }

    ANGLE_VK_PERF_COUNTERS_X(ANGLE_ADD_PERF_MONITOR_COUNTER_GROUP)

#undef ANGLE_ADD_PERF_MONITOR_COUNTER_GROUP

    mPerfMonitorCounters.push_back(vulkanGroup);
}

ContextVk::~ContextVk()
{
    if (!mPipelineCacheGraph.str().empty())
    {
        DumpPipelineCacheGraph(this, mPipelineCacheGraph);
    }
}

void ContextVk::onDestroy(const gl::Context *context)
{
    // Remove context from the share group
    mShareGroupVk->removeContext(this);

    // This will not destroy any resources. It will release them to be collected after finish.
    mIncompleteTextures.onDestroy(context);

    // Flush and complete current outstanding work before destruction.
    (void)finishImpl(RenderPassClosureReason::ContextDestruction);

    VkDevice device = getDevice();

    mDefaultUniformStorage.release(mRenderer);
    mEmptyBuffer.release(mRenderer);

    for (vk::DynamicBuffer &defaultBuffer : mStreamedVertexBuffers)
    {
        defaultBuffer.destroy(mRenderer);
    }

    for (vk::DynamicQueryPool &queryPool : mQueryPools)
    {
        queryPool.destroy(device);
    }

    // Recycle current commands buffers.
    mRenderer->recycleOutsideRenderPassCommandBufferHelper(device, &mOutsideRenderPassCommands);
    mRenderer->recycleRenderPassCommandBufferHelper(device, &mRenderPassCommands);

    mUtils.destroy(mRenderer);

    mRenderPassCache.destroy(mRenderer);
    mShaderLibrary.destroy(device);
    mGpuEventQueryPool.destroy(device);
    mCommandPools.outsideRenderPassPool.destroy(device);
    mCommandPools.renderPassPool.destroy(device);

    ASSERT(mCurrentGarbage.empty());
}

VertexArrayVk *ContextVk::getVertexArray() const
{
    return vk::GetImpl(mState.getVertexArray());
}

FramebufferVk *ContextVk::getDrawFramebuffer() const
{
    return vk::GetImpl(mState.getDrawFramebuffer());
}

ProgramVk *ContextVk::getProgram() const
{
    return vk::SafeGetImpl(mState.getProgram());
}

ProgramPipelineVk *ContextVk::getProgramPipeline() const
{
    return vk::SafeGetImpl(mState.getProgramPipeline());
}

angle::Result ContextVk::getIncompleteTexture(const gl::Context *context,
                                              gl::TextureType type,
                                              gl::SamplerFormat format,
                                              gl::Texture **textureOut)
{
    return mIncompleteTextures.getIncompleteTexture(context, type, format, this, textureOut);
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

    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        ANGLE_TRY(mQueryPools[gl::QueryType::TransformFeedbackPrimitivesWritten].init(
            this, VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,
            vk::kDefaultTransformFeedbackQueryPoolSize));
    }

    // If VK_EXT_primitives_generated_query is supported, use that to implement the OpenGL query.
    // Otherwise, the primitives generated query is provided through the Vulkan pipeline statistics
    // query if supported.
    if (getFeatures().supportsPrimitivesGeneratedQuery.enabled)
    {
        ANGLE_TRY(mQueryPools[gl::QueryType::PrimitivesGenerated].init(
            this, VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT,
            vk::kDefaultPrimitivesGeneratedQueryPoolSize));
    }
    else if (getFeatures().supportsPipelineStatisticsQuery.enabled)
    {
        ANGLE_TRY(mQueryPools[gl::QueryType::PrimitivesGenerated].init(
            this, VK_QUERY_TYPE_PIPELINE_STATISTICS, vk::kDefaultPrimitivesGeneratedQueryPoolSize));
    }

    // Init GLES to Vulkan index type map.
    initIndexTypeMap();

    mGraphicsPipelineDesc.reset(new vk::GraphicsPipelineDesc());
    mGraphicsPipelineDesc->initDefaults(this);

    // Initialize current value/default attribute buffers.
    for (vk::DynamicBuffer &buffer : mStreamedVertexBuffers)
    {
        buffer.init(mRenderer, kVertexBufferUsage, 1, kDynamicVertexDataSize, true);
    }

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

    // Assign initial command buffers from queue
    ANGLE_TRY(vk::OutsideRenderPassCommandBuffer::InitializeCommandPool(
        this, &mCommandPools.outsideRenderPassPool, mRenderer->getDeviceQueueIndex(),
        hasProtectedContent()));
    ANGLE_TRY(vk::RenderPassCommandBuffer::InitializeCommandPool(
        this, &mCommandPools.renderPassPool, mRenderer->getDeviceQueueIndex(),
        hasProtectedContent()));
    ANGLE_TRY(mRenderer->getOutsideRenderPassCommandBufferHelper(
        this, &mCommandPools.outsideRenderPassPool, &mOutsideRenderPassCommands));
    ANGLE_TRY(mRenderer->getRenderPassCommandBufferHelper(this, &mCommandPools.renderPassPool,
                                                          &mRenderPassCommands));

    if (mGpuEventsEnabled)
    {
        // GPU events should only be available if timestamp queries are available.
        ASSERT(mRenderer->getQueueFamilyProperties().timestampValidBits > 0);
        // Calculate the difference between CPU and GPU clocks for GPU event reporting.
        ANGLE_TRY(mGpuEventQueryPool.init(this, VK_QUERY_TYPE_TIMESTAMP,
                                          vk::kDefaultTimestampQueryPoolSize));
        ANGLE_TRY(synchronizeCpuGpuTime());

        EventName eventName = GetTraceEventName("Primary", mPrimaryBufferEventCounter);
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

    // Add context into the share group
    mShareGroupVk->addContext(this);

    return angle::Result::Continue;
}

angle::Result ContextVk::flush(const gl::Context *context)
{
    // Skip the flush if there's nothing recorded.  For single-buffer swapchains, flush should
    // ensure updates reach the screen, so even staged updates are considered something pending
    // submission.
    const bool isSingleBuffer =
        mCurrentWindowSurface != nullptr && mCurrentWindowSurface->isSharedPresentMode();

    if (!mHasAnyCommandsPendingSubmission && !hasStartedRenderPass() &&
        mOutsideRenderPassCommands->empty() &&
        !(isSingleBuffer && mCurrentWindowSurface->hasStagedUpdates()))
    {
        return angle::Result::Continue;
    }

    // Don't defer flushes in single-buffer mode.  In this mode, the application is not required to
    // call eglSwapBuffers(), and glFlush() is expected to ensure that work is submitted.
    if (mRenderer->getFeatures().deferFlushUntilEndRenderPass.enabled && hasStartedRenderPass() &&
        !isSingleBuffer)
    {
        mHasDeferredFlush = true;
        return angle::Result::Continue;
    }

    if (mRenderer->getFeatures().swapbuffersOnFlushOrFinishWithSingleBuffer.enabled &&
        isSingleBuffer)
    {
        return mCurrentWindowSurface->onSharedPresentContextFlush(context);
    }

    return flushImpl(nullptr, RenderPassClosureReason::GLFlush);
}

angle::Result ContextVk::finish(const gl::Context *context)
{
    if (mRenderer->getFeatures().swapbuffersOnFlushOrFinishWithSingleBuffer.enabled &&
        (mCurrentWindowSurface != nullptr) && mCurrentWindowSurface->isSharedPresentMode())
    {
        ANGLE_TRY(mCurrentWindowSurface->onSharedPresentContextFlush(context));
    }
    else
    {
        ANGLE_TRY(finishImpl(RenderPassClosureReason::GLFinish));
    }

    syncObjectPerfCounters(mRenderer->getCommandQueuePerfCounters());
    return angle::Result::Continue;
}

angle::Result ContextVk::setupDraw(const gl::Context *context,
                                   gl::PrimitiveMode mode,
                                   GLint firstVertexOrInvalid,
                                   GLsizei vertexOrIndexCount,
                                   GLsizei instanceCount,
                                   gl::DrawElementsType indexTypeOrInvalid,
                                   const void *indices,
                                   DirtyBits dirtyBitMask)
{
    // Set any dirty bits that depend on draw call parameters or other objects.
    if (mode != mCurrentDrawMode)
    {
        invalidateCurrentGraphicsPipeline();
        mCurrentDrawMode = mode;
        mGraphicsPipelineDesc->updateTopology(&mGraphicsPipelineTransition, mCurrentDrawMode);
    }

    // Must be called before the command buffer is started. Can call finish.
    VertexArrayVk *vertexArrayVk = getVertexArray();
    if (vertexArrayVk->getStreamingVertexAttribsMask().any())
    {
        // All client attribs & any emulated buffered attribs will be updated
        ANGLE_TRY(vertexArrayVk->updateStreamedAttribs(context, firstVertexOrInvalid,
                                                       vertexOrIndexCount, instanceCount,
                                                       indexTypeOrInvalid, indices));

        mGraphicsDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
    }

    ProgramExecutableVk *programExecutableVk = getExecutable();
    if (programExecutableVk->hasDirtyUniforms())
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_UNIFORMS);
    }

    // Update transform feedback offsets on every draw call when emulating transform feedback.  This
    // relies on the fact that no geometry/tessellation, indirect or indexed calls are supported in
    // ES3.1 (and emulation is not done for ES3.2).
    if (getFeatures().emulateTransformFeedback.enabled &&
        mState.isTransformFeedbackActiveUnpaused())
    {
        ASSERT(firstVertexOrInvalid != -1);
        mXfbBaseVertex             = firstVertexOrInvalid;
        mXfbVertexCountPerInstance = vertexOrIndexCount;
        invalidateGraphicsDriverUniforms();
    }

    DirtyBits dirtyBits = mGraphicsDirtyBits & dirtyBitMask;

    if (dirtyBits.none())
    {
        ASSERT(mRenderPassCommandBuffer);
        return angle::Result::Continue;
    }

    // Flush any relevant dirty bits.
    for (DirtyBits::Iterator dirtyBitIter = dirtyBits.begin(); dirtyBitIter != dirtyBits.end();
         ++dirtyBitIter)
    {
        ASSERT(mGraphicsDirtyBitHandlers[*dirtyBitIter]);
        ANGLE_TRY((this->*mGraphicsDirtyBitHandlers[*dirtyBitIter])(&dirtyBitIter, dirtyBitMask));
    }

    mGraphicsDirtyBits &= ~dirtyBitMask;

    // Render pass must be always available at this point.
    ASSERT(mRenderPassCommandBuffer);

    return angle::Result::Continue;
}

angle::Result ContextVk::setupIndexedDraw(const gl::Context *context,
                                          gl::PrimitiveMode mode,
                                          GLsizei indexCount,
                                          GLsizei instanceCount,
                                          gl::DrawElementsType indexType,
                                          const void *indices)
{
    ASSERT(mode != gl::PrimitiveMode::LineLoop);

    if (indexType != mCurrentDrawElementsType)
    {
        mCurrentDrawElementsType = indexType;
        ANGLE_TRY(onIndexBufferChange(nullptr));
    }

    VertexArrayVk *vertexArrayVk         = getVertexArray();
    const gl::Buffer *elementArrayBuffer = vertexArrayVk->getState().getElementArrayBuffer();
    if (!elementArrayBuffer)
    {
        BufferBindingDirty bindingDirty;
        ANGLE_TRY(vertexArrayVk->convertIndexBufferCPU(this, indexType, indexCount, indices,
                                                       &bindingDirty));

        // We only set dirty bit when the bound buffer actually changed.
        if (bindingDirty == BufferBindingDirty::Yes)
        {
            mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
            mCurrentIndexBufferOffset = 0;
        }
    }
    else
    {
        mCurrentIndexBufferOffset = reinterpret_cast<VkDeviceSize>(indices);

        if (indices != mLastIndexBufferOffset)
        {
            mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
            mLastIndexBufferOffset = indices;
        }

        // When you draw with LineLoop mode or GL_UNSIGNED_BYTE type, we may allocate its own
        // element buffer and modify mCurrentElementArrayBuffer. When we switch out of that draw
        // mode, we must reset mCurrentElementArrayBuffer back to the vertexArray's element buffer.
        // Since in either case we set DIRTY_BIT_INDEX_BUFFER dirty bit, we use this bit to re-sync
        // mCurrentElementArrayBuffer.
        if (mGraphicsDirtyBits[DIRTY_BIT_INDEX_BUFFER])
        {
            vertexArrayVk->updateCurrentElementArrayBuffer();
        }

        if (shouldConvertUint8VkIndexType(indexType) && mGraphicsDirtyBits[DIRTY_BIT_INDEX_BUFFER])
        {
            ANGLE_VK_PERF_WARNING(this, GL_DEBUG_SEVERITY_LOW,
                                  "Potential inefficiency emulating uint8 vertex attributes due to "
                                  "lack of hardware support");

            BufferVk *bufferVk             = vk::GetImpl(elementArrayBuffer);
            vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

            if (bufferHelper.isHostVisible() &&
                !bufferHelper.isCurrentlyInUse(getLastCompletedQueueSerial()))
            {
                uint8_t *src = nullptr;
                ANGLE_TRY(
                    bufferVk->mapImpl(this, GL_MAP_READ_BIT, reinterpret_cast<void **>(&src)));
                // Note: bufferOffset is not added here because mapImpl already adds it.
                src += reinterpret_cast<uintptr_t>(indices);
                const size_t byteCount = static_cast<size_t>(elementArrayBuffer->getSize()) -
                                         reinterpret_cast<uintptr_t>(indices);
                BufferBindingDirty bindingDirty;
                ANGLE_TRY(vertexArrayVk->convertIndexBufferCPU(this, indexType, byteCount, src,
                                                               &bindingDirty));
                ANGLE_TRY(bufferVk->unmapImpl(this));
            }
            else
            {
                ANGLE_TRY(vertexArrayVk->convertIndexBufferGPU(this, bufferVk, indices));
            }

            mCurrentIndexBufferOffset = 0;
        }
    }

    return setupDraw(context, mode, 0, indexCount, instanceCount, indexType, indices,
                     mIndexedDirtyBitsMask);
}

angle::Result ContextVk::setupIndirectDraw(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           DirtyBits dirtyBitMask,
                                           vk::BufferHelper *indirectBuffer)
{
    GLint firstVertex     = -1;
    GLsizei vertexCount   = 0;
    GLsizei instanceCount = 1;

    // Break the render pass if the indirect buffer was previously used as the output from transform
    // feedback.
    if (mCurrentTransformFeedbackBuffers.contains(indirectBuffer))
    {
        ANGLE_TRY(
            flushCommandsAndEndRenderPass(RenderPassClosureReason::XfbWriteThenIndirectDrawBuffer));
    }

    ANGLE_TRY(setupDraw(context, mode, firstVertex, vertexCount, instanceCount,
                        gl::DrawElementsType::InvalidEnum, nullptr, dirtyBitMask));

    // Process indirect buffer after render pass has started.
    mRenderPassCommands->bufferRead(this, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                    vk::PipelineStage::DrawIndirect, indirectBuffer);

    return angle::Result::Continue;
}

angle::Result ContextVk::setupIndexedIndirectDraw(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  gl::DrawElementsType indexType,
                                                  vk::BufferHelper *indirectBuffer)
{
    ASSERT(mode != gl::PrimitiveMode::LineLoop);

    if (indexType != mCurrentDrawElementsType)
    {
        mCurrentDrawElementsType = indexType;
        ANGLE_TRY(onIndexBufferChange(nullptr));
    }

    return setupIndirectDraw(context, mode, mIndexedDirtyBitsMask, indirectBuffer);
}

angle::Result ContextVk::setupLineLoopIndexedIndirectDraw(const gl::Context *context,
                                                          gl::PrimitiveMode mode,
                                                          gl::DrawElementsType indexType,
                                                          vk::BufferHelper *srcIndirectBuf,
                                                          VkDeviceSize indirectBufferOffset,
                                                          vk::BufferHelper **indirectBufferOut)
{
    ASSERT(mode == gl::PrimitiveMode::LineLoop);

    vk::BufferHelper *dstIndirectBuf = nullptr;

    VertexArrayVk *vertexArrayVk = getVertexArray();
    ANGLE_TRY(vertexArrayVk->handleLineLoopIndexIndirect(this, indexType, srcIndirectBuf,
                                                         indirectBufferOffset, &dstIndirectBuf));

    *indirectBufferOut = dstIndirectBuf;

    if (indexType != mCurrentDrawElementsType)
    {
        mCurrentDrawElementsType = indexType;
        ANGLE_TRY(onIndexBufferChange(nullptr));
    }

    return setupIndirectDraw(context, mode, mIndexedDirtyBitsMask, dstIndirectBuf);
}

angle::Result ContextVk::setupLineLoopIndirectDraw(const gl::Context *context,
                                                   gl::PrimitiveMode mode,
                                                   vk::BufferHelper *indirectBuffer,
                                                   VkDeviceSize indirectBufferOffset,
                                                   vk::BufferHelper **indirectBufferOut)
{
    ASSERT(mode == gl::PrimitiveMode::LineLoop);

    vk::BufferHelper *indirectBufferHelperOut = nullptr;

    VertexArrayVk *vertexArrayVk = getVertexArray();
    ANGLE_TRY(vertexArrayVk->handleLineLoopIndirectDraw(
        context, indirectBuffer, indirectBufferOffset, &indirectBufferHelperOut));

    *indirectBufferOut = indirectBufferHelperOut;

    if (gl::DrawElementsType::UnsignedInt != mCurrentDrawElementsType)
    {
        mCurrentDrawElementsType = gl::DrawElementsType::UnsignedInt;
        ANGLE_TRY(onIndexBufferChange(nullptr));
    }

    return setupIndirectDraw(context, mode, mIndexedDirtyBitsMask, indirectBufferHelperOut);
}

angle::Result ContextVk::setupLineLoopDraw(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           GLint firstVertex,
                                           GLsizei vertexOrIndexCount,
                                           gl::DrawElementsType indexTypeOrInvalid,
                                           const void *indices,
                                           uint32_t *numIndicesOut)
{
    mCurrentIndexBufferOffset    = 0;
    VertexArrayVk *vertexArrayVk = getVertexArray();
    ANGLE_TRY(vertexArrayVk->handleLineLoop(this, firstVertex, vertexOrIndexCount,
                                            indexTypeOrInvalid, indices, numIndicesOut));
    ANGLE_TRY(onIndexBufferChange(nullptr));
    mCurrentDrawElementsType = indexTypeOrInvalid != gl::DrawElementsType::InvalidEnum
                                   ? indexTypeOrInvalid
                                   : gl::DrawElementsType::UnsignedInt;
    return setupDraw(context, mode, firstVertex, vertexOrIndexCount, 1, indexTypeOrInvalid, indices,
                     mIndexedDirtyBitsMask);
}

angle::Result ContextVk::setupDispatch(const gl::Context *context)
{
    // Note: numerous tests miss a glMemoryBarrier call between the initial texture data upload and
    // the dispatch call.  Flush the outside render pass command buffer as a workaround.
    // TODO: Remove this and fix tests.  http://anglebug.com/5070
    ANGLE_TRY(flushOutsideRenderPassCommands());

    ProgramExecutableVk *programExecutableVk = getExecutable();
    if (programExecutableVk->hasDirtyUniforms())
    {
        mComputeDirtyBits.set(DIRTY_BIT_UNIFORMS);
        mComputeDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }

    DirtyBits dirtyBits = mComputeDirtyBits;

    // Flush any relevant dirty bits.
    for (size_t dirtyBit : dirtyBits)
    {
        ASSERT(mComputeDirtyBitHandlers[dirtyBit]);
        ANGLE_TRY((this->*mComputeDirtyBitHandlers[dirtyBit])());
    }

    mComputeDirtyBits.reset();

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsMemoryBarrier(DirtyBits::Iterator *dirtyBitsIterator,
                                                          DirtyBits dirtyBitMask)
{
    return handleDirtyMemoryBarrierImpl(dirtyBitsIterator, dirtyBitMask);
}

angle::Result ContextVk::handleDirtyComputeMemoryBarrier()
{
    return handleDirtyMemoryBarrierImpl(nullptr, {});
}

bool ContextVk::renderPassUsesStorageResources() const
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    // Storage images:
    for (size_t imageUnitIndex : executable->getActiveImagesMask())
    {
        const gl::Texture *texture = mState.getImageUnit(imageUnitIndex).texture.get();
        if (texture == nullptr)
        {
            continue;
        }

        TextureVk *textureVk = vk::GetImpl(texture);

        if (texture->getType() == gl::TextureType::Buffer)
        {
            vk::BufferHelper &buffer = vk::GetImpl(textureVk->getBuffer().get())->getBuffer();
            if (mRenderPassCommands->usesBuffer(buffer))
            {
                return true;
            }
        }
        else
        {
            vk::ImageHelper &image = textureVk->getImage();
            // Images only need to close the render pass if they need a layout transition.  Outside
            // render pass command buffer doesn't need closing as the layout transition barriers are
            // recorded in sequence with the rest of the commands.
            if (IsRenderPassStartedAndUsesImage(*mRenderPassCommands, image))
            {
                return true;
            }
        }
    }

    // Storage buffers:
    const std::vector<gl::InterfaceBlock> &blocks = executable->getShaderStorageBlocks();
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        uint32_t binding = blocks[bufferIndex].binding;
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            mState.getIndexedShaderStorageBuffer(binding);

        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        vk::BufferHelper &buffer = vk::GetImpl(bufferBinding.get())->getBuffer();
        if (mRenderPassCommands->usesBuffer(buffer))
        {
            return true;
        }
    }

    // Atomic counters:
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers =
        executable->getAtomicCounterBuffers();
    for (uint32_t bufferIndex = 0; bufferIndex < atomicCounterBuffers.size(); ++bufferIndex)
    {
        uint32_t binding = atomicCounterBuffers[bufferIndex].binding;
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            mState.getIndexedAtomicCounterBuffer(binding);

        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        vk::BufferHelper &buffer = vk::GetImpl(bufferBinding.get())->getBuffer();
        if (mRenderPassCommands->usesBuffer(buffer))
        {
            return true;
        }
    }

    return false;
}

angle::Result ContextVk::handleDirtyMemoryBarrierImpl(DirtyBits::Iterator *dirtyBitsIterator,
                                                      DirtyBits dirtyBitMask)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    const bool hasImages         = executable->hasImages();
    const bool hasStorageBuffers = executable->hasStorageBuffers();
    const bool hasAtomicCounters = executable->hasAtomicCounterBuffers();

    if (!hasImages && !hasStorageBuffers && !hasAtomicCounters)
    {
        return angle::Result::Continue;
    }

    // Break the render pass if necessary.  This is only needed for write-after-read situations, and
    // is done by checking whether current storage buffers and images are used in the render pass.
    if (renderPassUsesStorageResources())
    {
        // Either set later bits (if called during handling of graphics dirty bits), or set the
        // dirty bits directly (if called during handling of compute dirty bits).
        if (dirtyBitsIterator)
        {
            return flushDirtyGraphicsRenderPass(
                dirtyBitsIterator, dirtyBitMask,
                RenderPassClosureReason::GLMemoryBarrierThenStorageResource);
        }
        else
        {
            return flushCommandsAndEndRenderPass(
                RenderPassClosureReason::GLMemoryBarrierThenStorageResource);
        }
    }

    // Flushing outside render pass commands is cheap.  If a memory barrier has been issued in its
    // life time, just flush it instead of wasting time trying to figure out if it's necessary.
    if (mOutsideRenderPassCommands->hasGLMemoryBarrierIssued())
    {
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsEventLog(DirtyBits::Iterator *dirtyBitsIterator,
                                                     DirtyBits dirtyBitMask)
{
    return handleDirtyEventLogImpl(mRenderPassCommandBuffer);
}

angle::Result ContextVk::handleDirtyComputeEventLog()
{
    return handleDirtyEventLogImpl(&mOutsideRenderPassCommands->getCommandBuffer());
}

template <typename CommandBufferT>
angle::Result ContextVk::handleDirtyEventLogImpl(CommandBufferT *commandBuffer)
{
    // This method is called when a draw or dispatch command is being processed.  It's purpose is
    // to call the vkCmd*DebugUtilsLabelEXT functions in order to communicate to debuggers
    // (e.g. AGI) the OpenGL ES commands that the application uses.

    // Exit early if no OpenGL ES commands have been logged, or if no command buffer (for a no-op
    // draw), or if calling the vkCmd*DebugUtilsLabelEXT functions is not enabled.
    if (mEventLog.empty() || commandBuffer == nullptr || !mRenderer->angleDebuggerMode())
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

    // AGI desires no parameters on the top-level of the hierarchy.
    std::string topLevelCommand = mEventLog.back();
    size_t startOfParameters    = topLevelCommand.find("(");
    if (startOfParameters != std::string::npos)
    {
        topLevelCommand = topLevelCommand.substr(0, startOfParameters);
    }
    VkDebugUtilsLabelEXT label = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                                  nullptr,
                                  topLevelCommand.c_str(),
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

angle::Result ContextVk::handleDirtyGraphicsDefaultAttribs(DirtyBits::Iterator *dirtyBitsIterator,
                                                           DirtyBits dirtyBitMask)
{
    ASSERT(mDirtyDefaultAttribsMask.any());

    VertexArrayVk *vertexArrayVk = getVertexArray();
    for (size_t attribIndex : mDirtyDefaultAttribsMask)
    {
        ANGLE_TRY(vertexArrayVk->updateDefaultAttrib(this, attribIndex));
    }

    mDirtyDefaultAttribsMask.reset();
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsPipelineDesc(DirtyBits::Iterator *dirtyBitsIterator,
                                                         DirtyBits dirtyBitMask)
{
    const VkPipeline previousPipeline = mCurrentGraphicsPipeline
                                            ? mCurrentGraphicsPipeline->getPipeline().getHandle()
                                            : VK_NULL_HANDLE;

    ASSERT(mState.getProgramExecutable() != nullptr);
    const gl::ProgramExecutable &glExecutable = *mState.getProgramExecutable();
    ProgramExecutableVk *executableVk         = getExecutable();
    ASSERT(executableVk);

    PipelineCacheAccess pipelineCache;
    ANGLE_TRY(mRenderer->getPipelineCache(&pipelineCache));

    if (!mCurrentGraphicsPipeline)
    {
        const vk::GraphicsPipelineDesc *descPtr;

        // The desc's specialization constant depends on program's
        // specConstUsageBits. We need to update it if program has changed.
        SpecConstUsageBits usageBits = getCurrentProgramSpecConstUsageBits();
        updateGraphicsPipelineDescWithSpecConstUsageBits(usageBits);

        // Draw call shader patching, shader compilation, and pipeline cache query.
        ANGLE_TRY(executableVk->getGraphicsPipeline(
            this, mCurrentDrawMode, &pipelineCache, PipelineSource::Draw, *mGraphicsPipelineDesc,
            glExecutable, &descPtr, &mCurrentGraphicsPipeline));
        mGraphicsPipelineTransition.reset();
    }
    else if (mGraphicsPipelineTransition.any())
    {
        ASSERT(mCurrentGraphicsPipeline->valid());
        if (!mCurrentGraphicsPipeline->findTransition(
                mGraphicsPipelineTransition, *mGraphicsPipelineDesc, &mCurrentGraphicsPipeline))
        {
            vk::PipelineHelper *oldPipeline = mCurrentGraphicsPipeline;
            const vk::GraphicsPipelineDesc *descPtr;

            ANGLE_TRY(executableVk->getGraphicsPipeline(
                this, mCurrentDrawMode, &pipelineCache, PipelineSource::Draw,
                *mGraphicsPipelineDesc, glExecutable, &descPtr, &mCurrentGraphicsPipeline));

            oldPipeline->addTransition(mGraphicsPipelineTransition, descPtr,
                                       mCurrentGraphicsPipeline);
        }

        mGraphicsPipelineTransition.reset();
    }
    // Update the queue serial for the pipeline object.
    ASSERT(mCurrentGraphicsPipeline && mCurrentGraphicsPipeline->valid());

    mRenderPassCommands->retainResource(mCurrentGraphicsPipeline);

    const VkPipeline newPipeline = mCurrentGraphicsPipeline->getPipeline().getHandle();

    // If there's no change in pipeline, avoid rebinding it later.  If the rebind is due to a new
    // command buffer or UtilsVk, it will happen anyway with DIRTY_BIT_PIPELINE_BINDING.
    if (newPipeline == previousPipeline)
    {
        return angle::Result::Continue;
    }

    // VK_EXT_transform_feedback disallows binding pipelines while transform feedback is active.
    // If a new pipeline needs to be bound, the render pass should necessarily be broken (which
    // implicitly pauses transform feedback), as resuming requires a barrier on the transform
    // feedback counter buffer.
    if (mRenderPassCommands->started() && mRenderPassCommands->isTransformFeedbackActiveUnpaused())
    {
        ANGLE_TRY(flushDirtyGraphicsRenderPass(
            dirtyBitsIterator, dirtyBitMask, RenderPassClosureReason::PipelineBindWhileXfbActive));

        dirtyBitsIterator->setLaterBit(DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME);
    }

    // The pipeline needs to rebind because it's changed.
    dirtyBitsIterator->setLaterBit(DIRTY_BIT_PIPELINE_BINDING);

    return angle::Result::Continue;
}

angle::Result ContextVk::updateRenderPassDepthFeedbackLoopMode(
    UpdateDepthFeedbackLoopReason depthReason,
    UpdateDepthFeedbackLoopReason stencilReason)
{
    return updateRenderPassDepthFeedbackLoopModeImpl(nullptr, {}, depthReason, stencilReason);
}

angle::Result ContextVk::updateRenderPassDepthFeedbackLoopModeImpl(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask,
    UpdateDepthFeedbackLoopReason depthReason,
    UpdateDepthFeedbackLoopReason stencilReason)
{
    FramebufferVk *drawFramebufferVk = getDrawFramebuffer();
    if (!hasStartedRenderPass() || drawFramebufferVk->getDepthStencilRenderTarget() == nullptr)
    {
        return angle::Result::Continue;
    }

    const gl::DepthStencilState &dsState = mState.getDepthStencilState();
    vk::ResourceAccess depthAccess       = GetDepthAccess(dsState, depthReason);
    vk::ResourceAccess stencilAccess     = GetStencilAccess(dsState, stencilReason);

    if ((depthAccess == vk::ResourceAccess::Write || stencilAccess == vk::ResourceAccess::Write) &&
        drawFramebufferVk->isReadOnlyDepthFeedbackLoopMode())
    {
        // If we are switching out of read only mode and we are in feedback loop, we must end
        // renderpass here. Otherwise, updating it to writeable layout will produce a writable
        // feedback loop that is illegal in vulkan and will trigger validation errors that depth
        // texture is using the writable layout.
        if (dirtyBitsIterator)
        {
            ANGLE_TRY(flushDirtyGraphicsRenderPass(
                dirtyBitsIterator, dirtyBitMask,
                RenderPassClosureReason::DepthStencilWriteAfterFeedbackLoop));
        }
        else
        {
            ANGLE_TRY(flushCommandsAndEndRenderPass(
                RenderPassClosureReason::DepthStencilWriteAfterFeedbackLoop));
        }
        // Clear read-only depth feedback mode.
        drawFramebufferVk->setReadOnlyDepthFeedbackLoopMode(false);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsReadOnlyDepthFeedbackLoopMode(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    return updateRenderPassDepthFeedbackLoopModeImpl(dirtyBitsIterator, dirtyBitMask,
                                                     UpdateDepthFeedbackLoopReason::Draw,
                                                     UpdateDepthFeedbackLoopReason::Draw);
}

angle::Result ContextVk::handleDirtyGraphicsRenderPass(DirtyBits::Iterator *dirtyBitsIterator,
                                                       DirtyBits dirtyBitMask)
{
    // If the render pass needs to be recreated, close it using the special mid-dirty-bit-handling
    // function, so later dirty bits can be set.
    if (mRenderPassCommands->started())
    {
        ANGLE_TRY(flushDirtyGraphicsRenderPass(dirtyBitsIterator,
                                               dirtyBitMask & ~DirtyBits{DIRTY_BIT_RENDER_PASS},
                                               RenderPassClosureReason::AlreadySpecifiedElsewhere));
    }

    FramebufferVk *drawFramebufferVk  = getDrawFramebuffer();
    gl::Rectangle scissoredRenderArea = drawFramebufferVk->getRotatedScissoredRenderArea(this);
    bool renderPassDescChanged        = false;

    ANGLE_TRY(startRenderPass(scissoredRenderArea, nullptr, &renderPassDescChanged));

    // The render pass desc can change when starting the render pass, for example due to
    // multisampled-render-to-texture needs based on loadOps.  In that case, recreate the graphics
    // pipeline.
    if (renderPassDescChanged)
    {
        ANGLE_TRY(handleDirtyGraphicsPipelineDesc(dirtyBitsIterator, dirtyBitMask));
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsColorAccess(DirtyBits::Iterator *dirtyBitsIterator,
                                                        DirtyBits dirtyBitMask)
{
    FramebufferVk *drawFramebufferVk             = getDrawFramebuffer();
    const gl::FramebufferState &framebufferState = drawFramebufferVk->getState();

    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    // Update color attachment accesses
    vk::PackedAttachmentIndex colorIndexVk(0);
    for (size_t colorIndexGL : framebufferState.getColorAttachmentsMask())
    {
        if (framebufferState.getEnabledDrawBuffers().test(colorIndexGL))
        {
            vk::ResourceAccess colorAccess = GetColorAccess(
                mState, framebufferState, drawFramebufferVk->getEmulatedAlphaAttachmentMask(),
                executable->usesFramebufferFetch(), colorIndexGL);
            mRenderPassCommands->onColorAccess(colorIndexVk, colorAccess);
        }
        ++colorIndexVk;
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDepthStencilAccess(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    FramebufferVk *drawFramebufferVk = getDrawFramebuffer();
    if (drawFramebufferVk->getDepthStencilRenderTarget() == nullptr)
    {
        return angle::Result::Continue;
    }

    // Update depth/stencil attachment accesses
    const gl::DepthStencilState &dsState = mState.getDepthStencilState();
    vk::ResourceAccess depthAccess = GetDepthAccess(dsState, UpdateDepthFeedbackLoopReason::Draw);
    vk::ResourceAccess stencilAccess =
        GetStencilAccess(dsState, UpdateDepthFeedbackLoopReason::Draw);
    mRenderPassCommands->onDepthAccess(depthAccess);
    mRenderPassCommands->onStencilAccess(stencilAccess);

    drawFramebufferVk->updateRenderPassReadOnlyDepthMode(this, mRenderPassCommands);

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsPipelineBinding(DirtyBits::Iterator *dirtyBitsIterator,
                                                            DirtyBits dirtyBitMask)
{
    ASSERT(mCurrentGraphicsPipeline);

    mRenderPassCommandBuffer->bindGraphicsPipeline(mCurrentGraphicsPipeline->getPipeline());

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyComputePipelineDesc()
{
    if (mCurrentComputePipeline == nullptr)
    {
        PipelineCacheAccess pipelineCache;
        ANGLE_TRY(mRenderer->getPipelineCache(&pipelineCache));

        const gl::ProgramExecutable &glExecutable = *mState.getProgramExecutable();
        ProgramExecutableVk *executableVk         = getExecutable();
        ASSERT(executableVk);
        ANGLE_TRY(executableVk->getComputePipeline(this, &pipelineCache, PipelineSource::Draw,
                                                   glExecutable, &mCurrentComputePipeline));
    }

    ASSERT(mComputeDirtyBits.test(DIRTY_BIT_PIPELINE_BINDING));

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyComputePipelineBinding()
{
    ASSERT(mCurrentComputePipeline);

    mOutsideRenderPassCommands->getCommandBuffer().bindComputePipeline(
        mCurrentComputePipeline->getPipeline());
    mOutsideRenderPassCommands->retainResource(mCurrentComputePipeline);

    return angle::Result::Continue;
}

template <typename CommandBufferHelperT>
ANGLE_INLINE angle::Result ContextVk::handleDirtyTexturesImpl(
    CommandBufferHelperT *commandBufferHelper,
    PipelineType pipelineType)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);
    const gl::ActiveTextureMask &activeTextures = executable->getActiveSamplersMask();

    for (size_t textureUnit : activeTextures)
    {
        TextureVk *textureVk = mActiveTextures[textureUnit];

        // If it's a texture buffer, get the attached buffer.
        if (textureVk->getBuffer().get() != nullptr)
        {
            BufferVk *bufferVk = vk::GetImpl(textureVk->getBuffer().get());
            const gl::ShaderBitSet stages =
                executable->getSamplerShaderBitsForTextureUnitIndex(textureUnit);

            OnTextureBufferRead(this, bufferVk, stages, commandBufferHelper);

            textureVk->retainBufferViews(commandBufferHelper);
            continue;
        }

        // The image should be flushed and ready to use at this point. There may still be
        // lingering staged updates in its staging buffer for unused texture mip levels or
        // layers. Therefore we can't verify it has no staged updates right here.
        vk::ImageHelper &image = textureVk->getImage();

        const vk::ImageLayout imageLayout =
            GetImageReadLayout(textureVk, *executable, textureUnit, pipelineType);

        // Ensure the image is in the desired layout
        commandBufferHelper->imageRead(this, image.getAspectFlags(), imageLayout, &image);
    }

    if (executable->hasTextures())
    {
        UpdatePreCacheActiveTextures(executable->getSamplerBindings(),
                                     executable->getActiveSamplersMask(), mActiveTextures,
                                     mState.getSamplers(), &mActiveTexturesDesc);

        ProgramExecutableVk *executableVk = getExecutable();

        ANGLE_TRY(executableVk->updateTexturesDescriptorSet(
            this, *executable, mActiveTextures, mState.getSamplers(),
            mEmulateSeamfulCubeMapSampling, pipelineType, &mUpdateDescriptorSetsBuilder,
            commandBufferHelper, mActiveTexturesDesc));
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsTextures(DirtyBits::Iterator *dirtyBitsIterator,
                                                     DirtyBits dirtyBitMask)
{
    return handleDirtyTexturesImpl(mRenderPassCommands, PipelineType::Graphics);
}

angle::Result ContextVk::handleDirtyComputeTextures()
{
    return handleDirtyTexturesImpl(mOutsideRenderPassCommands, PipelineType::Compute);
}

angle::Result ContextVk::handleDirtyGraphicsVertexBuffers(DirtyBits::Iterator *dirtyBitsIterator,
                                                          DirtyBits dirtyBitMask)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    VertexArrayVk *vertexArrayVk            = getVertexArray();
    uint32_t maxAttrib = mState.getProgramExecutable()->getMaxActiveAttribLocation();
    const gl::AttribArray<VkBuffer> &bufferHandles = vertexArrayVk->getCurrentArrayBufferHandles();
    const gl::AttribArray<VkDeviceSize> &bufferOffsets =
        vertexArrayVk->getCurrentArrayBufferOffsets();

    if (getFeatures().supportsExtendedDynamicState.enabled)
    {
        const gl::AttribArray<GLuint> &bufferStrides =
            vertexArrayVk->getCurrentArrayBufferStrides();
        const gl::AttribArray<angle::FormatID> &bufferFormats =
            vertexArrayVk->getCurrentArrayBufferFormats();
        gl::AttribArray<VkDeviceSize> strides = {};

        // Set stride to 0 for mismatching formats between the program's declared attribute and that
        // which is specified in glVertexAttribPointer.  See comment in vk_cache_utils.cpp
        // (initializePipeline) for more details.
        const gl::AttributesMask &activeAttribLocations =
            executable->getNonBuiltinAttribLocationsMask();
        const gl::ComponentTypeMask &programAttribsTypeMask = executable->getAttributesTypeMask();

        for (size_t attribIndex : activeAttribLocations)
        {
            const angle::Format &intendedFormat =
                mRenderer->getFormat(bufferFormats[attribIndex]).getIntendedFormat();

            const gl::ComponentType attribType = GetVertexAttributeComponentType(
                intendedFormat.isPureInt(), intendedFormat.vertexAttribType);
            const gl::ComponentType programAttribType =
                gl::GetComponentTypeMask(programAttribsTypeMask, attribIndex);

            const bool mismatchingType =
                attribType != programAttribType && (programAttribType == gl::ComponentType::Float ||
                                                    attribType == gl::ComponentType::Float);
            strides[attribIndex] = mismatchingType ? 0 : bufferStrides[attribIndex];
        }

        // TODO: Use the sizes parameters here to fix the robustness issue worked around in
        // crbug.com/1310038
        mRenderPassCommandBuffer->bindVertexBuffers2(0, maxAttrib, bufferHandles.data(),
                                                     bufferOffsets.data(), nullptr, strides.data());
    }
    else
    {
        mRenderPassCommandBuffer->bindVertexBuffers(0, maxAttrib, bufferHandles.data(),
                                                    bufferOffsets.data());
    }

    const gl::AttribArray<vk::BufferHelper *> &arrayBufferResources =
        vertexArrayVk->getCurrentArrayBuffers();

    // Mark all active vertex buffers as accessed.
    const gl::AttributesMask attribsMask = executable->getActiveAttribLocationsMask();
    for (size_t attribIndex : attribsMask)
    {
        vk::BufferHelper *arrayBuffer = arrayBufferResources[attribIndex];
        if (arrayBuffer)
        {
            mRenderPassCommands->bufferRead(this, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                            vk::PipelineStage::VertexInput, arrayBuffer);
        }
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsIndexBuffer(DirtyBits::Iterator *dirtyBitsIterator,
                                                        DirtyBits dirtyBitMask)
{
    VertexArrayVk *vertexArrayVk         = getVertexArray();
    vk::BufferHelper *elementArrayBuffer = vertexArrayVk->getCurrentElementArrayBuffer();
    ASSERT(elementArrayBuffer != nullptr);

    VkDeviceSize bufferOffset;
    const vk::Buffer &buffer = elementArrayBuffer->getBufferForVertexArray(
        this, elementArrayBuffer->getSize(), &bufferOffset);

    mRenderPassCommandBuffer->bindIndexBuffer(buffer, bufferOffset + mCurrentIndexBufferOffset,
                                              getVkIndexType(mCurrentDrawElementsType));

    mRenderPassCommands->bufferRead(this, VK_ACCESS_INDEX_READ_BIT, vk::PipelineStage::VertexInput,
                                    elementArrayBuffer);

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsFramebufferFetchBarrier(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

    mRenderPassCommandBuffer->pipelineBarrier(
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        GetLocalDependencyFlags(this), 1, &memoryBarrier, 0, nullptr, 0, nullptr);

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsBlendBarrier(DirtyBits::Iterator *dirtyBitsIterator,
                                                         DirtyBits dirtyBitMask)
{
    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT;

    mRenderPassCommandBuffer->pipelineBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                              GetLocalDependencyFlags(this), 1, &memoryBarrier, 0,
                                              nullptr, 0, nullptr);

    return angle::Result::Continue;
}

template <typename CommandBufferHelperT>
angle::Result ContextVk::handleDirtyShaderResourcesImpl(CommandBufferHelperT *commandBufferHelper,
                                                        PipelineType pipelineType)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    const bool hasImages = executable->hasImages();
    const bool hasStorageBuffers =
        executable->hasStorageBuffers() || executable->hasAtomicCounterBuffers();
    const bool hasUniformBuffers = executable->hasUniformBuffers();

    if (!hasUniformBuffers && !hasStorageBuffers && !hasImages &&
        !executable->usesFramebufferFetch())
    {
        return angle::Result::Continue;
    }

    if (hasImages)
    {
        ANGLE_TRY(updateActiveImages(commandBufferHelper));
    }

    handleDirtyShaderBufferResourcesImpl(commandBufferHelper);

    ANGLE_TRY(updateShaderResourcesDescriptorDesc(pipelineType));

    ProgramExecutableVk *executableVk = getExecutable();

    vk::SharedDescriptorSetCacheKey newSharedCacheKey;
    ANGLE_TRY(executableVk->updateShaderResourcesDescriptorSet(
        this, &mUpdateDescriptorSetsBuilder, commandBufferHelper, mShaderBuffersDescriptorDesc,
        &newSharedCacheKey));

    mShaderBuffersDescriptorDesc.updateImagesAndBuffersWithSharedCacheKey(newSharedCacheKey);

    // Record usage of storage buffers and images in the command buffer to aid handling of
    // glMemoryBarrier.
    if (hasImages || hasStorageBuffers)
    {
        commandBufferHelper->setHasShaderStorageOutput();
    }

    return angle::Result::Continue;
}

void ContextVk::handleDirtyShaderBufferResourcesImpl(
    vk::CommandBufferHelperCommon *commandBufferHelper)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    // Process buffer barriers.
    for (const gl::ShaderType shaderType : executable->getLinkedShaderStages())
    {
        const std::vector<gl::InterfaceBlock> &ubos = executable->getUniformBlocks();

        for (const gl::InterfaceBlock &ubo : ubos)
        {
            const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
                mState.getIndexedUniformBuffer(ubo.binding);

            if (!ubo.isActive(shaderType))
            {
                continue;
            }

            if (bufferBinding.get() == nullptr)
            {
                continue;
            }

            BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
            vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

            commandBufferHelper->bufferRead(this, VK_ACCESS_UNIFORM_READ_BIT,
                                            vk::GetPipelineStage(shaderType), &bufferHelper);
        }

        const std::vector<gl::InterfaceBlock> &ssbos = executable->getShaderStorageBlocks();
        for (const gl::InterfaceBlock &ssbo : ssbos)
        {
            const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
                mState.getIndexedShaderStorageBuffer(ssbo.binding);

            if (!ssbo.isActive(shaderType))
            {
                continue;
            }

            if (bufferBinding.get() == nullptr)
            {
                continue;
            }

            BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
            vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

            // We set the SHADER_READ_BIT to be conservative.
            VkAccessFlags accessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            commandBufferHelper->bufferWrite(this, accessFlags, vk::GetPipelineStage(shaderType),
                                             &bufferHelper);
        }

        const std::vector<gl::AtomicCounterBuffer> &acbs = executable->getAtomicCounterBuffers();
        for (const gl::AtomicCounterBuffer &atomicCounterBuffer : acbs)
        {
            uint32_t binding = atomicCounterBuffer.binding;
            const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
                mState.getIndexedAtomicCounterBuffer(binding);

            if (bufferBinding.get() == nullptr)
            {
                continue;
            }

            BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
            vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

            // We set SHADER_READ_BIT to be conservative.
            commandBufferHelper->bufferWrite(this,
                                             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                             vk::GetPipelineStage(shaderType), &bufferHelper);
        }
    }
}

angle::Result ContextVk::handleDirtyGraphicsShaderResources(DirtyBits::Iterator *dirtyBitsIterator,
                                                            DirtyBits dirtyBitMask)
{
    return handleDirtyShaderResourcesImpl(mRenderPassCommands, PipelineType::Graphics);
}

angle::Result ContextVk::handleDirtyComputeShaderResources()
{
    return handleDirtyShaderResourcesImpl(mOutsideRenderPassCommands, PipelineType::Compute);
}

angle::Result ContextVk::handleDirtyGraphicsTransformFeedbackBuffersEmulation(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
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
            mRenderPassCommands->bufferWrite(this, VK_ACCESS_SHADER_WRITE_BIT,
                                             vk::PipelineStage::VertexShader, bufferHelper);
        }

        populateTransformFeedbackBufferSet(bufferCount, bufferHelpers);
    }

    ProgramExecutableVk *executableVk      = getExecutable();
    vk::BufferHelper *currentUniformBuffer = mDefaultUniformStorage.getCurrentBuffer();

    vk::DescriptorSetDescBuilder uniformsAndXfbDesc;
    uniformsAndXfbDesc.updateUniformsAndXfb(
        this, *executable, *executableVk, currentUniformBuffer, mEmptyBuffer,
        mState.isTransformFeedbackActiveUnpaused(), transformFeedbackVk);

    return executableVk->updateUniformsAndXfbDescriptorSet(
        this, &mUpdateDescriptorSetsBuilder, mRenderPassCommands, currentUniformBuffer,
        &uniformsAndXfbDesc);
}

angle::Result ContextVk::handleDirtyGraphicsTransformFeedbackBuffersExtension(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    if (!executable->hasTransformFeedbackOutput() || !mState.isTransformFeedbackActive())
    {
        return angle::Result::Continue;
    }

    TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(mState.getCurrentTransformFeedback());
    size_t bufferCount                       = executable->getTransformFeedbackBufferCount();

    const gl::TransformFeedbackBuffersArray<vk::BufferHelper *> &buffers =
        transformFeedbackVk->getBufferHelpers();
    gl::TransformFeedbackBuffersArray<vk::BufferHelper> &counterBuffers =
        transformFeedbackVk->getCounterBufferHelpers();

    // Issue necessary barriers for the transform feedback buffers.
    for (size_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
    {
        vk::BufferHelper *bufferHelper = buffers[bufferIndex];
        ASSERT(bufferHelper);
        mRenderPassCommands->bufferWrite(this, VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
                                         vk::PipelineStage::TransformFeedback, bufferHelper);
    }

    // Issue necessary barriers for the transform feedback counter buffer.  Note that the barrier is
    // issued only on the first buffer (which uses a global memory barrier), as all the counter
    // buffers of the transform feedback object are used together.  The rest of the buffers are
    // simply retained so they don't get deleted too early.
    ASSERT(counterBuffers[0].valid());
    mRenderPassCommands->bufferWrite(this,
                                     VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT |
                                         VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
                                     vk::PipelineStage::TransformFeedback, &counterBuffers[0]);
    for (size_t bufferIndex = 1; bufferIndex < bufferCount; ++bufferIndex)
    {
        mRenderPassCommands->retainReadWriteResource(&counterBuffers[bufferIndex]);
    }

    const gl::TransformFeedbackBuffersArray<VkBuffer> &bufferHandles =
        transformFeedbackVk->getBufferHandles();
    const gl::TransformFeedbackBuffersArray<VkDeviceSize> &bufferOffsets =
        transformFeedbackVk->getBufferOffsets();
    const gl::TransformFeedbackBuffersArray<VkDeviceSize> &bufferSizes =
        transformFeedbackVk->getBufferSizes();

    mRenderPassCommandBuffer->bindTransformFeedbackBuffers(
        0, static_cast<uint32_t>(bufferCount), bufferHandles.data(), bufferOffsets.data(),
        bufferSizes.data());

    if (!mState.isTransformFeedbackActiveUnpaused())
    {
        return angle::Result::Continue;
    }

    // We should have same number of counter buffers as xfb buffers have
    const gl::TransformFeedbackBuffersArray<VkBuffer> &counterBufferHandles =
        transformFeedbackVk->getCounterBufferHandles();
    const gl::TransformFeedbackBuffersArray<VkDeviceSize> &counterBufferOffsets =
        transformFeedbackVk->getCounterBufferOffsets();

    bool rebindBuffers = transformFeedbackVk->getAndResetBufferRebindState();

    mRenderPassCommands->beginTransformFeedback(bufferCount, counterBufferHandles.data(),
                                                counterBufferOffsets.data(), rebindBuffers);

    populateTransformFeedbackBufferSet(bufferCount, buffers);

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsTransformFeedbackResume(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    if (mRenderPassCommands->isTransformFeedbackStarted())
    {
        mRenderPassCommands->resumeTransformFeedback();
    }

    ANGLE_TRY(resumeXfbRenderPassQueriesIfActive());

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDescriptorSets(DirtyBits::Iterator *dirtyBitsIterator,
                                                           DirtyBits dirtyBitMask)
{
    return handleDirtyDescriptorSetsImpl(mRenderPassCommands, PipelineType::Graphics);
}

angle::Result ContextVk::handleDirtyGraphicsUniforms(DirtyBits::Iterator *dirtyBitsIterator,
                                                     DirtyBits dirtyBitMask)
{
    dirtyBitsIterator->setLaterBit(DIRTY_BIT_DESCRIPTOR_SETS);
    return handleDirtyUniformsImpl(mRenderPassCommands);
}

angle::Result ContextVk::handleDirtyComputeUniforms()
{
    return handleDirtyUniformsImpl(mOutsideRenderPassCommands);
}

angle::Result ContextVk::handleDirtyUniformsImpl(vk::CommandBufferHelperCommon *commandBufferHelper)
{
    ProgramExecutableVk *programExecutableVk = getExecutable();
    TransformFeedbackVk *transformFeedbackVk =
        vk::SafeGetImpl(mState.getCurrentTransformFeedback());
    ANGLE_TRY(programExecutableVk->updateUniforms(
        this, &mUpdateDescriptorSetsBuilder, commandBufferHelper, &mEmptyBuffer,
        *mState.getProgramExecutable(), &mDefaultUniformStorage,
        mState.isTransformFeedbackActiveUnpaused(), transformFeedbackVk));

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicViewport(DirtyBits::Iterator *dirtyBitsIterator,
                                                            DirtyBits dirtyBitMask)
{
    mRenderPassCommandBuffer->setViewport(0, 1, &mViewport);
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicScissor(DirtyBits::Iterator *dirtyBitsIterator,
                                                           DirtyBits dirtyBitMask)
{
    handleDirtyGraphicsDynamicScissorImpl(mState.isQueryActive(gl::QueryType::PrimitivesGenerated));
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicLineWidth(DirtyBits::Iterator *dirtyBitsIterator,
                                                             DirtyBits dirtyBitMask)
{
    // Clamp line width to min/max allowed values. It's not invalid GL to
    // provide out-of-range line widths, but it _is_ invalid Vulkan.
    const float lineWidth = gl::clamp(mState.getLineWidth(), mState.getCaps().minAliasedLineWidth,
                                      mState.getCaps().maxAliasedLineWidth);
    mRenderPassCommandBuffer->setLineWidth(lineWidth);
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicDepthBias(DirtyBits::Iterator *dirtyBitsIterator,
                                                             DirtyBits dirtyBitMask)
{
    const gl::RasterizerState &rasterState = mState.getRasterizerState();
    // Note: depth bias clamp is only exposed in EXT_polygon_offset_clamp.
    mRenderPassCommandBuffer->setDepthBias(rasterState.polygonOffsetUnits, 0,
                                           rasterState.polygonOffsetFactor);
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicBlendConstants(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    const gl::ColorF &color = mState.getBlendColor();
    mRenderPassCommandBuffer->setBlendConstants(color.data());
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicStencilCompareMask(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    const gl::DepthStencilState &depthStencilState = mState.getDepthStencilState();
    mRenderPassCommandBuffer->setStencilCompareMask(depthStencilState.stencilMask,
                                                    depthStencilState.stencilBackMask);
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicStencilWriteMask(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    const gl::DepthStencilState &depthStencilState = mState.getDepthStencilState();
    const gl::Framebuffer *drawFramebuffer         = mState.getDrawFramebuffer();
    uint32_t frontWritemask                        = 0;
    uint32_t backWritemask                         = 0;
    // Don't write to stencil buffers that should not exist
    if (drawFramebuffer->hasStencil())
    {
        frontWritemask = depthStencilState.stencilWritemask;
        backWritemask  = depthStencilState.stencilBackWritemask;
    }

    mRenderPassCommandBuffer->setStencilWriteMask(frontWritemask, backWritemask);
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicStencilReference(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    mRenderPassCommandBuffer->setStencilReference(mState.getStencilRef(),
                                                  mState.getStencilBackRef());
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicCullMode(DirtyBits::Iterator *dirtyBitsIterator,
                                                            DirtyBits dirtyBitMask)
{
    const gl::RasterizerState &rasterState = mState.getRasterizerState();
    mRenderPassCommandBuffer->setCullMode(gl_vk::GetCullMode(rasterState));
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicFrontFace(DirtyBits::Iterator *dirtyBitsIterator,
                                                             DirtyBits dirtyBitMask)
{
    const gl::RasterizerState &rasterState = mState.getRasterizerState();
    mRenderPassCommandBuffer->setFrontFace(
        gl_vk::GetFrontFace(rasterState.frontFace, isYFlipEnabledForDrawFBO()));
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicDepthTestEnable(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    const gl::DepthStencilState depthStencilState = mState.getDepthStencilState();
    gl::Framebuffer *drawFramebuffer              = mState.getDrawFramebuffer();

    // Only enable the depth test if the draw framebuffer has a depth buffer.
    mRenderPassCommandBuffer->setDepthTestEnable(depthStencilState.depthTest &&
                                                 drawFramebuffer->hasDepth());
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicDepthWriteEnable(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    const gl::DepthStencilState depthStencilState = mState.getDepthStencilState();
    gl::Framebuffer *drawFramebuffer              = mState.getDrawFramebuffer();

    // Only enable the depth write if the draw framebuffer has a depth buffer.
    const bool depthWriteEnabled =
        drawFramebuffer->hasDepth() && depthStencilState.depthTest && depthStencilState.depthMask;
    mRenderPassCommandBuffer->setDepthWriteEnable(depthWriteEnabled);
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicDepthCompareOp(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    const gl::DepthStencilState depthStencilState = mState.getDepthStencilState();
    mRenderPassCommandBuffer->setDepthCompareOp(gl_vk::GetCompareOp(depthStencilState.depthFunc));
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicStencilTestEnable(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    const gl::DepthStencilState depthStencilState = mState.getDepthStencilState();
    gl::Framebuffer *drawFramebuffer              = mState.getDrawFramebuffer();

    // Only enable the stencil test if the draw framebuffer has a stencil buffer.
    mRenderPassCommandBuffer->setStencilTestEnable(depthStencilState.stencilTest &&
                                                   drawFramebuffer->hasStencil());
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicStencilOp(DirtyBits::Iterator *dirtyBitsIterator,
                                                             DirtyBits dirtyBitMask)
{
    const gl::DepthStencilState depthStencilState = mState.getDepthStencilState();
    mRenderPassCommandBuffer->setStencilOp(
        VK_STENCIL_FACE_FRONT_BIT, gl_vk::GetStencilOp(depthStencilState.stencilFail),
        gl_vk::GetStencilOp(depthStencilState.stencilPassDepthPass),
        gl_vk::GetStencilOp(depthStencilState.stencilPassDepthFail),
        gl_vk::GetCompareOp(depthStencilState.stencilFunc));
    mRenderPassCommandBuffer->setStencilOp(
        VK_STENCIL_FACE_BACK_BIT, gl_vk::GetStencilOp(depthStencilState.stencilBackFail),
        gl_vk::GetStencilOp(depthStencilState.stencilBackPassDepthPass),
        gl_vk::GetStencilOp(depthStencilState.stencilBackPassDepthFail),
        gl_vk::GetCompareOp(depthStencilState.stencilBackFunc));
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicRasterizerDiscardEnable(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    const bool isEmulatingRasterizerDiscard =
        isEmulatingRasterizerDiscardDuringPrimitivesGeneratedQuery(
            mState.isQueryActive(gl::QueryType::PrimitivesGenerated));
    const bool isRasterizerDiscardEnabled = mState.isRasterizerDiscardEnabled();

    mRenderPassCommandBuffer->setRasterizerDiscardEnable(isRasterizerDiscardEnabled &&
                                                         !isEmulatingRasterizerDiscard);
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicDepthBiasEnable(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    mRenderPassCommandBuffer->setDepthBiasEnable(mState.isPolygonOffsetFillEnabled());
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicPrimitiveRestartEnable(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    mRenderPassCommandBuffer->setPrimitiveRestartEnable(mState.isPrimitiveRestartEnabled());
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDynamicFragmentShadingRate(
    DirtyBits::Iterator *dirtyBitsIterator,
    DirtyBits dirtyBitMask)
{
    gl::ShadingRate shadingRate = getState().getShadingRate();
    if (shadingRate == gl::ShadingRate::Undefined)
    {
        // Shading rate has not been set. Nothing to do, early return.
        return angle::Result::Continue;
    }

    const bool shadingRateSupported = mRenderer->isShadingRateSupported(shadingRate);
    VkExtent2D fragmentSize         = {};
    VkFragmentShadingRateCombinerOpKHR shadingRateCombinerOp[2] = {
        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR};

    switch (shadingRate)
    {
        case gl::ShadingRate::_1x1:
            ASSERT(shadingRateSupported);
            fragmentSize.width  = 1;
            fragmentSize.height = 1;
            break;
        case gl::ShadingRate::_1x2:
            ASSERT(shadingRateSupported);
            fragmentSize.width  = 1;
            fragmentSize.height = 2;
            break;
        case gl::ShadingRate::_2x1:
            ASSERT(shadingRateSupported);
            fragmentSize.width  = 2;
            fragmentSize.height = 1;
            break;
        case gl::ShadingRate::_2x2:
            ASSERT(shadingRateSupported);
            fragmentSize.width  = 2;
            fragmentSize.height = 2;
            break;
        case gl::ShadingRate::_4x2:
            if (shadingRateSupported)
            {
                fragmentSize.width  = 4;
                fragmentSize.height = 2;
            }
            else
            {
                // Fallback to shading rate that preserves aspect ratio
                fragmentSize.width  = 2;
                fragmentSize.height = 1;
            }
            break;
        case gl::ShadingRate::_4x4:
            if (shadingRateSupported)
            {
                fragmentSize.width  = 4;
                fragmentSize.height = 4;
            }
            else
            {
                // Fallback to shading rate that preserves aspect ratio
                fragmentSize.width  = 2;
                fragmentSize.height = 2;
            }
            break;
        default:
            UNREACHABLE();
            return angle::Result::Stop;
    }

    ASSERT(hasStartedRenderPass());
    mRenderPassCommandBuffer->setFragmentShadingRate(&fragmentSize, shadingRateCombinerOp);

    return angle::Result::Continue;
}

void ContextVk::handleDirtyGraphicsDynamicScissorImpl(bool isPrimitivesGeneratedQueryActive)
{
    // If primitives generated query and rasterizer discard are both active, but the Vulkan
    // implementation of the query does not support rasterizer discard, use an empty scissor to
    // emulate it.
    if (isEmulatingRasterizerDiscardDuringPrimitivesGeneratedQuery(
            isPrimitivesGeneratedQueryActive))
    {
        VkRect2D emptyScissor = {};
        mRenderPassCommandBuffer->setScissor(0, 1, &emptyScissor);
    }
    else
    {
        mRenderPassCommandBuffer->setScissor(0, 1, &mScissor);
    }
}

angle::Result ContextVk::handleDirtyComputeDescriptorSets()
{
    return handleDirtyDescriptorSetsImpl(mOutsideRenderPassCommands, PipelineType::Compute);
}

template <typename CommandBufferHelperT>
angle::Result ContextVk::handleDirtyDescriptorSetsImpl(CommandBufferHelperT *commandBufferHelper,
                                                       PipelineType pipelineType)
{
    // When using Vulkan secondary command buffers, the descriptor sets need to be updated before
    // they are bound.
    if (!commandBufferHelper->getCommandBuffer().ExecutesInline())
    {
        flushDescriptorSetUpdates();
    }

    ProgramExecutableVk *executableVk = getExecutable();
    return executableVk->bindDescriptorSets(this, commandBufferHelper,
                                            &commandBufferHelper->getCommandBuffer(), pipelineType);
}

void ContextVk::syncObjectPerfCounters(const angle::VulkanPerfCounters &commandQueuePerfCounters)
{
    mPerfCounters.descriptorSetCacheTotalSize                = 0;
    mPerfCounters.descriptorSetCacheKeySizeBytes             = 0;
    mPerfCounters.uniformsAndXfbDescriptorSetCacheHits       = 0;
    mPerfCounters.uniformsAndXfbDescriptorSetCacheMisses     = 0;
    mPerfCounters.uniformsAndXfbDescriptorSetCacheTotalSize  = 0;
    mPerfCounters.textureDescriptorSetCacheHits              = 0;
    mPerfCounters.textureDescriptorSetCacheMisses            = 0;
    mPerfCounters.textureDescriptorSetCacheTotalSize         = 0;
    mPerfCounters.shaderResourcesDescriptorSetCacheHits      = 0;
    mPerfCounters.shaderResourcesDescriptorSetCacheMisses    = 0;
    mPerfCounters.shaderResourcesDescriptorSetCacheTotalSize = 0;
    mPerfCounters.dynamicBufferAllocations                   = 0;

    // Share group descriptor set allocations and caching stats.
    memset(mVulkanCacheStats.data(), 0, sizeof(CacheStats) * mVulkanCacheStats.size());

    mShareGroupVk->getMetaDescriptorPool(DescriptorSetIndex::UniformsAndXfb)
        .accumulateDescriptorCacheStats(VulkanCacheType::UniformsAndXfbDescriptors, this);
    mShareGroupVk->getMetaDescriptorPool(DescriptorSetIndex::Texture)
        .accumulateDescriptorCacheStats(VulkanCacheType::TextureDescriptors, this);
    mShareGroupVk->getMetaDescriptorPool(DescriptorSetIndex::ShaderResource)
        .accumulateDescriptorCacheStats(VulkanCacheType::ShaderResourcesDescriptors, this);

    const CacheStats &uniCacheStats = mVulkanCacheStats[VulkanCacheType::UniformsAndXfbDescriptors];
    mPerfCounters.uniformsAndXfbDescriptorSetCacheHits      = uniCacheStats.getHitCount();
    mPerfCounters.uniformsAndXfbDescriptorSetCacheMisses    = uniCacheStats.getMissCount();
    mPerfCounters.uniformsAndXfbDescriptorSetCacheTotalSize = uniCacheStats.getSize();

    const CacheStats &texCacheStats = mVulkanCacheStats[VulkanCacheType::TextureDescriptors];
    mPerfCounters.textureDescriptorSetCacheHits      = texCacheStats.getHitCount();
    mPerfCounters.textureDescriptorSetCacheMisses    = texCacheStats.getMissCount();
    mPerfCounters.textureDescriptorSetCacheTotalSize = texCacheStats.getSize();

    const CacheStats &resCacheStats =
        mVulkanCacheStats[VulkanCacheType::ShaderResourcesDescriptors];
    mPerfCounters.shaderResourcesDescriptorSetCacheHits      = resCacheStats.getHitCount();
    mPerfCounters.shaderResourcesDescriptorSetCacheMisses    = resCacheStats.getMissCount();
    mPerfCounters.shaderResourcesDescriptorSetCacheTotalSize = resCacheStats.getSize();

    mPerfCounters.descriptorSetCacheTotalSize =
        uniCacheStats.getSize() + texCacheStats.getSize() + resCacheStats.getSize() +
        mVulkanCacheStats[VulkanCacheType::DriverUniformsDescriptors].getSize();

    mPerfCounters.descriptorSetCacheKeySizeBytes = 0;

    for (DescriptorSetIndex descriptorSetIndex : angle::AllEnums<DescriptorSetIndex>())
    {
        vk::MetaDescriptorPool &descriptorPool =
            mShareGroupVk->getMetaDescriptorPool(descriptorSetIndex);
        mPerfCounters.descriptorSetCacheKeySizeBytes += descriptorPool.getTotalCacheKeySizeBytes();
    }

    // Update perf counters from the renderer as well
    mPerfCounters.commandQueueSubmitCallsTotal =
        commandQueuePerfCounters.commandQueueSubmitCallsTotal;
    mPerfCounters.commandQueueSubmitCallsPerFrame =
        commandQueuePerfCounters.commandQueueSubmitCallsPerFrame;
    mPerfCounters.vkQueueSubmitCallsTotal    = commandQueuePerfCounters.vkQueueSubmitCallsTotal;
    mPerfCounters.vkQueueSubmitCallsPerFrame = commandQueuePerfCounters.vkQueueSubmitCallsPerFrame;

    // Return current drawFramebuffer's cache stats
    mPerfCounters.framebufferCacheSize = mShareGroupVk->getFramebufferCache().getSize();
}

void ContextVk::updateOverlayOnPresent()
{
    const gl::OverlayType *overlay = mState.getOverlay();
    ASSERT(overlay->isEnabled());

    angle::VulkanPerfCounters commandQueuePerfCounters = mRenderer->getCommandQueuePerfCounters();
    syncObjectPerfCounters(commandQueuePerfCounters);

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
    }

    {
        gl::RunningGraphWidget *descriptorSetAllocationCount =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanDescriptorSetAllocations);
        descriptorSetAllocationCount->add(mPerfCounters.descriptorSetAllocations);
        descriptorSetAllocationCount->next();
    }

    {
        gl::RunningGraphWidget *shaderResourceHitRate =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanShaderResourceDSHitRate);
        uint64_t numCacheAccesses = mPerfCounters.shaderResourcesDescriptorSetCacheHits +
                                    mPerfCounters.shaderResourcesDescriptorSetCacheMisses;
        if (numCacheAccesses > 0)
        {
            float hitRateFloat =
                static_cast<float>(mPerfCounters.shaderResourcesDescriptorSetCacheHits) /
                static_cast<float>(numCacheAccesses);
            size_t hitRate = static_cast<size_t>(hitRateFloat * 100.0f);
            shaderResourceHitRate->add(hitRate);
            shaderResourceHitRate->next();
        }
    }

    {
        gl::RunningGraphWidget *dynamicBufferAllocations =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanDynamicBufferAllocations);
        dynamicBufferAllocations->next();
    }

    {
        gl::CountWidget *cacheKeySize =
            overlay->getCountWidget(gl::WidgetId::VulkanDescriptorCacheKeySize);
        cacheKeySize->reset();
        cacheKeySize->add(mPerfCounters.descriptorSetCacheKeySizeBytes);
    }

    {
        gl::RunningGraphWidget *dynamicBufferAllocations =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanDynamicBufferAllocations);
        dynamicBufferAllocations->add(mPerfCounters.dynamicBufferAllocations);
    }

    {
        gl::RunningGraphWidget *attemptedSubmissionsWidget =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanAttemptedSubmissions);
        attemptedSubmissionsWidget->add(commandQueuePerfCounters.commandQueueSubmitCallsPerFrame);
        attemptedSubmissionsWidget->next();

        gl::RunningGraphWidget *actualSubmissionsWidget =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanActualSubmissions);
        actualSubmissionsWidget->add(commandQueuePerfCounters.vkQueueSubmitCallsPerFrame);
        actualSubmissionsWidget->next();
    }

    {
        gl::RunningGraphWidget *cacheLookupsWidget =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanPipelineCacheLookups);
        cacheLookupsWidget->add(mPerfCounters.pipelineCreationCacheHits +
                                mPerfCounters.pipelineCreationCacheMisses);
        cacheLookupsWidget->next();

        gl::RunningGraphWidget *cacheMissesWidget =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanPipelineCacheMisses);
        cacheMissesWidget->add(mPerfCounters.pipelineCreationCacheMisses);
        cacheMissesWidget->next();

        overlay->getCountWidget(gl::WidgetId::VulkanTotalPipelineCacheHitTimeMs)
            ->set(mPerfCounters.pipelineCreationTotalCacheHitsDurationNs / 1000'000);
        overlay->getCountWidget(gl::WidgetId::VulkanTotalPipelineCacheMissTimeMs)
            ->set(mPerfCounters.pipelineCreationTotalCacheMissesDurationNs / 1000'000);
    }
}

void ContextVk::addOverlayUsedBuffersCount(vk::CommandBufferHelperCommon *commandBuffer)
{
    const gl::OverlayType *overlay = mState.getOverlay();
    if (!overlay->isEnabled())
    {
        return;
    }

    gl::RunningGraphWidget *widget =
        overlay->getRunningGraphWidget(gl::WidgetId::VulkanRenderPassBufferCount);
    size_t buffersCount = commandBuffer->getUsedBuffersCount();
    if (buffersCount > 0)
    {
        widget->add(buffersCount);
        widget->next();
    }

    {
        gl::RunningGraphWidget *textureDescriptorCacheSize =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanTextureDescriptorCacheSize);
        textureDescriptorCacheSize->add(mPerfCounters.textureDescriptorSetCacheTotalSize);
        textureDescriptorCacheSize->next();
    }

    {
        gl::RunningGraphWidget *uniformDescriptorCacheSize =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanUniformDescriptorCacheSize);
        uniformDescriptorCacheSize->add(mPerfCounters.uniformsAndXfbDescriptorSetCacheTotalSize);
        uniformDescriptorCacheSize->next();
    }

    {
        gl::RunningGraphWidget *descriptorCacheSize =
            overlay->getRunningGraphWidget(gl::WidgetId::VulkanDescriptorCacheSize);
        descriptorCacheSize->add(mPerfCounters.descriptorSetCacheTotalSize);
        descriptorCacheSize->next();
    }
}

angle::Result ContextVk::submitFrame(const vk::Semaphore *signalSemaphore,
                                     Submit submission,
                                     Serial *submitSerialOut)
{
    getShareGroup()->acquireResourceUseList(
        std::move(mOutsideRenderPassCommands->releaseResourceUseList()));
    getShareGroup()->acquireResourceUseList(
        std::move(mRenderPassCommands->releaseResourceUseList()));

    ANGLE_TRY(submitCommands(signalSemaphore, submission, submitSerialOut));

    mHasAnyCommandsPendingSubmission = false;

    onRenderPassFinished(RenderPassClosureReason::AlreadySpecifiedElsewhere);
    return angle::Result::Continue;
}

angle::Result ContextVk::submitFrameOutsideCommandBufferOnly(Serial *submitSerialOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::submitFrameOutsideCommandBufferOnly");
    getShareGroup()->acquireResourceUseList(
        std::move(mOutsideRenderPassCommands->releaseResourceUseList()));

    return submitCommands(nullptr, Submit::OutsideRenderPassCommandsOnly, submitSerialOut);
}

angle::Result ContextVk::submitCommands(const vk::Semaphore *signalSemaphore,
                                        Submit submission,
                                        Serial *submitSerialOut)
{
    if (mCurrentWindowSurface)
    {
        const vk::Semaphore *waitSemaphore =
            mCurrentWindowSurface->getAndResetAcquireImageSemaphore();
        if (waitSemaphore != nullptr)
        {
            addWaitSemaphore(waitSemaphore->getHandle(),
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        }
    }

    if (vk::CommandBufferHelperCommon::kEnableCommandStreamDiagnostics)
    {
        dumpCommandStreamDiagnostics();
    }

    // Clean up garbage only when submitting all commands.  Otherwise there may be garbage
    // associated with commands that are not yet flushed.
    vk::GarbageList garbage;
    if (submission == Submit::AllCommands)
    {
        garbage = std::move(mCurrentGarbage);
    }

    ANGLE_TRY(mRenderer->submitFrame(this, hasProtectedContent(), mContextPriority,
                                     std::move(mWaitSemaphores),
                                     std::move(mWaitSemaphoreStageMasks), signalSemaphore,
                                     std::move(garbage), &mCommandPools, submitSerialOut));

    getShareGroup()->releaseResourceUseLists(*submitSerialOut);
    // Now that we have processed resourceUseList, some of pending garbage may no longer pending
    // and should be moved to garbage list.
    mRenderer->cleanupPendingSubmissionGarbage();

    mComputeDirtyBits |= mNewComputeCommandBufferDirtyBits;

    if (mGpuEventsEnabled)
    {
        ANGLE_TRY(checkCompletedGpuEvents());
    }

    mTotalBufferToImageCopySize = 0;

    return angle::Result::Continue;
}

angle::Result ContextVk::onCopyUpdate(VkDeviceSize size)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::onCopyUpdate");

    mTotalBufferToImageCopySize += size;
    // If the copy size exceeds the specified threshold, submit the outside command buffer.
    if (mTotalBufferToImageCopySize >= kMaxBufferToImageCopySize)
    {
        ANGLE_TRY(submitOutsideRenderPassCommandsImpl());
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

    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::synchronizeCpuGpuTime");

    // Create a query used to receive the GPU timestamp
    vk::QueryHelper timestampQuery;
    ANGLE_TRY(mGpuEventQueryPool.allocateQuery(this, &timestampQuery, 1));

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

        vk::ResourceUseList scratchResourceUseList;

        ANGLE_TRY(mRenderer->getCommandBufferOneOff(this, hasProtectedContent(), &commandBuffer));

        commandBuffer.setEvent(gpuReady.get().getHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
        commandBuffer.waitEvents(1, cpuReady.get().ptr(), VK_PIPELINE_STAGE_HOST_BIT,
                                 VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, nullptr, 0, nullptr, 0,
                                 nullptr);
        timestampQuery.writeTimestampToPrimary(this, &commandBuffer);
        timestampQuery.retain(&scratchResourceUseList);

        commandBuffer.setEvent(gpuDone.get().getHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

        ANGLE_VK_TRY(this, commandBuffer.end());

        Serial submitSerial;
        // vkEvent's are externally synchronized, therefore need work to be submitted before calling
        // vkGetEventStatus
        ANGLE_TRY(mRenderer->queueSubmitOneOff(
            this, std::move(commandBuffer), hasProtectedContent(), mContextPriority, nullptr, 0,
            nullptr, vk::SubmitPolicy::EnsureSubmitted, &submitSerial));
        scratchResourceUseList.releaseResourceUsesAndUpdateSerials(submitSerial);

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
        ANGLE_TRY(finishToSerial(submitSerial));

        vk::QueryResult gpuTimestampCycles(1);
        ANGLE_TRY(timestampQuery.getUint64Result(this, &gpuTimestampCycles));

        // Use the first timestamp queried as origin.
        if (mGpuEventTimestampOrigin == 0)
        {
            mGpuEventTimestampOrigin =
                gpuTimestampCycles.getResult(vk::QueryResult::kDefaultResultIndex);
        }

        // Take these CPU and GPU timestamps if there is better confidence.
        double confidenceRangeS = TeS - TsS;
        if (confidenceRangeS < tightestRangeS)
        {
            tightestRangeS = confidenceRangeS;
            TcpuS          = cpuTimestampS;
            TgpuCycles     = gpuTimestampCycles.getResult(vk::QueryResult::kDefaultResultIndex);
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

angle::Result ContextVk::traceGpuEventImpl(vk::OutsideRenderPassCommandBuffer *commandBuffer,
                                           char phase,
                                           const EventName &name)
{
    ASSERT(mGpuEventsEnabled);

    GpuEventQuery gpuEvent;
    gpuEvent.name  = name;
    gpuEvent.phase = phase;
    ANGLE_TRY(mGpuEventQueryPool.allocateQuery(this, &gpuEvent.queryHelper, 1));

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
        if (eventQuery.queryHelper.usedInRunningCommands(lastCompletedSerial))
        {
            break;
        }

        // See if the results are available.
        vk::QueryResult gpuTimestampCycles(1);
        bool available = false;
        ANGLE_TRY(eventQuery.queryHelper.getUint64ResultNonBlocking(this, &gpuTimestampCycles,
                                                                    &available));
        if (!available)
        {
            break;
        }

        mGpuEventQueryPool.freeQuery(this, &eventQuery.queryHelper);

        GpuEvent gpuEvent;
        gpuEvent.gpuTimestampCycles =
            gpuTimestampCycles.getResult(vk::QueryResult::kDefaultResultIndex);
        gpuEvent.name  = eventQuery.name;
        gpuEvent.phase = eventQuery.phase;

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
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::clearAllGarbage");

    // The VMA virtual allocator code has assertion to ensure all sub-ranges are freed before
    // virtual block gets freed. We need to ensure all completed garbage objects are actually freed
    // to avoid hitting that assertion.
    mRenderer->cleanupCompletedCommandsGarbage();

    for (vk::GarbageObject &garbage : mCurrentGarbage)
    {
        garbage.destroy(mRenderer);
    }
    mCurrentGarbage.clear();
}

void ContextVk::handleDeviceLost()
{
    (void)mOutsideRenderPassCommands->reset(this);
    (void)mRenderPassCommands->reset(this);
    mRenderer->notifyDeviceLost();
}

angle::Result ContextVk::drawArrays(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    GLint first,
                                    GLsizei count)
{
    uint32_t clampedVertexCount = gl::GetClampedVertexCount<uint32_t>(count);

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t numIndices;
        ANGLE_TRY(setupLineLoopDraw(context, mode, first, count, gl::DrawElementsType::InvalidEnum,
                                    nullptr, &numIndices));
        vk::LineLoopHelper::Draw(numIndices, 0, mRenderPassCommandBuffer);
    }
    else
    {
        ANGLE_TRY(setupDraw(context, mode, first, count, 1, gl::DrawElementsType::InvalidEnum,
                            nullptr, mNonIndexedDirtyBitsMask));
        mRenderPassCommandBuffer->draw(clampedVertexCount, first);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::drawArraysInstanced(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei instances)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t clampedVertexCount = gl::GetClampedVertexCount<uint32_t>(count);
        uint32_t numIndices;
        ANGLE_TRY(setupLineLoopDraw(context, mode, first, clampedVertexCount,
                                    gl::DrawElementsType::InvalidEnum, nullptr, &numIndices));
        mRenderPassCommandBuffer->drawIndexedInstanced(numIndices, instances);
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupDraw(context, mode, first, count, instances, gl::DrawElementsType::InvalidEnum,
                        nullptr, mNonIndexedDirtyBitsMask));
    mRenderPassCommandBuffer->drawInstanced(gl::GetClampedVertexCount<uint32_t>(count), instances,
                                            first);
    return angle::Result::Continue;
}

angle::Result ContextVk::drawArraysInstancedBaseInstance(const gl::Context *context,
                                                         gl::PrimitiveMode mode,
                                                         GLint first,
                                                         GLsizei count,
                                                         GLsizei instances,
                                                         GLuint baseInstance)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t clampedVertexCount = gl::GetClampedVertexCount<uint32_t>(count);
        uint32_t numIndices;
        ANGLE_TRY(setupLineLoopDraw(context, mode, first, clampedVertexCount,
                                    gl::DrawElementsType::InvalidEnum, nullptr, &numIndices));
        mRenderPassCommandBuffer->drawIndexedInstancedBaseVertexBaseInstance(numIndices, instances,
                                                                             0, 0, baseInstance);
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupDraw(context, mode, first, count, instances, gl::DrawElementsType::InvalidEnum,
                        nullptr, mNonIndexedDirtyBitsMask));
    mRenderPassCommandBuffer->drawInstancedBaseInstance(gl::GetClampedVertexCount<uint32_t>(count),
                                                        instances, first, baseInstance);
    return angle::Result::Continue;
}

angle::Result ContextVk::drawElements(const gl::Context *context,
                                      gl::PrimitiveMode mode,
                                      GLsizei count,
                                      gl::DrawElementsType type,
                                      const void *indices)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t indexCount;
        ANGLE_TRY(setupLineLoopDraw(context, mode, 0, count, type, indices, &indexCount));
        vk::LineLoopHelper::Draw(indexCount, 0, mRenderPassCommandBuffer);
    }
    else
    {
        ANGLE_TRY(setupIndexedDraw(context, mode, count, 1, type, indices));
        mRenderPassCommandBuffer->drawIndexed(count);
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
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t indexCount;
        ANGLE_TRY(setupLineLoopDraw(context, mode, 0, count, type, indices, &indexCount));
        vk::LineLoopHelper::Draw(indexCount, baseVertex, mRenderPassCommandBuffer);
    }
    else
    {
        ANGLE_TRY(setupIndexedDraw(context, mode, count, 1, type, indices));
        mRenderPassCommandBuffer->drawIndexedBaseVertex(count, baseVertex);
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
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t indexCount;
        ANGLE_TRY(setupLineLoopDraw(context, mode, 0, count, type, indices, &indexCount));
        count = indexCount;
    }
    else
    {
        ANGLE_TRY(setupIndexedDraw(context, mode, count, instances, type, indices));
    }

    mRenderPassCommandBuffer->drawIndexedInstanced(count, instances);
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
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t indexCount;
        ANGLE_TRY(setupLineLoopDraw(context, mode, 0, count, type, indices, &indexCount));
        count = indexCount;
    }
    else
    {
        ANGLE_TRY(setupIndexedDraw(context, mode, count, instances, type, indices));
    }

    mRenderPassCommandBuffer->drawIndexedInstancedBaseVertex(count, instances, baseVertex);
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
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        uint32_t indexCount;
        ANGLE_TRY(setupLineLoopDraw(context, mode, 0, count, type, indices, &indexCount));
        count = indexCount;
    }
    else
    {
        ANGLE_TRY(setupIndexedDraw(context, mode, count, instances, type, indices));
    }

    mRenderPassCommandBuffer->drawIndexedInstancedBaseVertexBaseInstance(count, instances, 0,
                                                                         baseVertex, baseInstance);
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
    return multiDrawArraysIndirectHelper(context, mode, indirect, 1, 0);
}

angle::Result ContextVk::drawElementsIndirect(const gl::Context *context,
                                              gl::PrimitiveMode mode,
                                              gl::DrawElementsType type,
                                              const void *indirect)
{
    return multiDrawElementsIndirectHelper(context, mode, type, indirect, 1, 0);
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

angle::Result ContextVk::multiDrawArraysIndirect(const gl::Context *context,
                                                 gl::PrimitiveMode mode,
                                                 const void *indirect,
                                                 GLsizei drawcount,
                                                 GLsizei stride)
{
    return multiDrawArraysIndirectHelper(context, mode, indirect, drawcount, stride);
}

angle::Result ContextVk::multiDrawArraysIndirectHelper(const gl::Context *context,
                                                       gl::PrimitiveMode mode,
                                                       const void *indirect,
                                                       GLsizei drawcount,
                                                       GLsizei stride)
{
    VertexArrayVk *vertexArrayVk = getVertexArray();
    if (drawcount > 1 && !CanMultiDrawIndirectUseCmd(this, vertexArrayVk, mode, drawcount, stride))
    {
        return rx::MultiDrawArraysIndirectGeneral(this, context, mode, indirect, drawcount, stride);
    }

    // Stride must be a multiple of the size of VkDrawIndirectCommand (stride = 0 is invalid when
    // drawcount > 1).
    uint32_t vkStride = (stride == 0 && drawcount > 1) ? sizeof(VkDrawIndirectCommand) : stride;

    gl::Buffer *indirectBuffer            = mState.getTargetBuffer(gl::BufferBinding::DrawIndirect);
    vk::BufferHelper *currentIndirectBuf  = &vk::GetImpl(indirectBuffer)->getBuffer();
    VkDeviceSize currentIndirectBufOffset = reinterpret_cast<VkDeviceSize>(indirect);

    if (vertexArrayVk->getStreamingVertexAttribsMask().any())
    {
        // Handling instanced vertex attributes is not covered for drawcount > 1.
        ASSERT(drawcount <= 1);

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

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        // Line loop only supports handling at most one indirect parameter.
        ASSERT(drawcount <= 1);

        ASSERT(indirectBuffer);
        vk::BufferHelper *dstIndirectBuf = nullptr;

        ANGLE_TRY(setupLineLoopIndirectDraw(context, mode, currentIndirectBuf,
                                            currentIndirectBufOffset, &dstIndirectBuf));

        mRenderPassCommandBuffer->drawIndexedIndirect(
            dstIndirectBuf->getBuffer(), dstIndirectBuf->getOffset(), drawcount, vkStride);
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupIndirectDraw(context, mode, mNonIndexedDirtyBitsMask, currentIndirectBuf));

    mRenderPassCommandBuffer->drawIndirect(
        currentIndirectBuf->getBuffer(), currentIndirectBuf->getOffset() + currentIndirectBufOffset,
        drawcount, vkStride);

    return angle::Result::Continue;
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

angle::Result ContextVk::multiDrawElementsIndirect(const gl::Context *context,
                                                   gl::PrimitiveMode mode,
                                                   gl::DrawElementsType type,
                                                   const void *indirect,
                                                   GLsizei drawcount,
                                                   GLsizei stride)
{
    return multiDrawElementsIndirectHelper(context, mode, type, indirect, drawcount, stride);
}

angle::Result ContextVk::multiDrawElementsIndirectHelper(const gl::Context *context,
                                                         gl::PrimitiveMode mode,
                                                         gl::DrawElementsType type,
                                                         const void *indirect,
                                                         GLsizei drawcount,
                                                         GLsizei stride)
{
    VertexArrayVk *vertexArrayVk = getVertexArray();
    if (drawcount > 1 && !CanMultiDrawIndirectUseCmd(this, vertexArrayVk, mode, drawcount, stride))
    {
        return rx::MultiDrawElementsIndirectGeneral(this, context, mode, type, indirect, drawcount,
                                                    stride);
    }

    // Stride must be a multiple of the size of VkDrawIndexedIndirectCommand (stride = 0 is invalid
    // when drawcount > 1).
    uint32_t vkStride =
        (stride == 0 && drawcount > 1) ? sizeof(VkDrawIndexedIndirectCommand) : stride;

    gl::Buffer *indirectBuffer = mState.getTargetBuffer(gl::BufferBinding::DrawIndirect);
    ASSERT(indirectBuffer);
    vk::BufferHelper *currentIndirectBuf  = &vk::GetImpl(indirectBuffer)->getBuffer();
    VkDeviceSize currentIndirectBufOffset = reinterpret_cast<VkDeviceSize>(indirect);

    // Reset the index buffer offset
    mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
    mCurrentIndexBufferOffset = 0;

    if (vertexArrayVk->getStreamingVertexAttribsMask().any())
    {
        // Handling instanced vertex attributes is not covered for drawcount > 1.
        ASSERT(drawcount <= 1);

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
        ANGLE_VK_PERF_WARNING(
            this, GL_DEBUG_SEVERITY_LOW,
            "Potential inefficiency emulating uint8 vertex attributes due to lack "
            "of hardware support");

        vk::BufferHelper *dstIndirectBuf;

        ANGLE_TRY(vertexArrayVk->convertIndexBufferIndirectGPU(
            this, currentIndirectBuf, currentIndirectBufOffset, &dstIndirectBuf));

        currentIndirectBuf       = dstIndirectBuf;
        currentIndirectBufOffset = 0;
    }

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        // Line loop only supports handling at most one indirect parameter.
        ASSERT(drawcount <= 1);

        vk::BufferHelper *dstIndirectBuf;

        ANGLE_TRY(setupLineLoopIndexedIndirectDraw(context, mode, type, currentIndirectBuf,
                                                   currentIndirectBufOffset, &dstIndirectBuf));

        currentIndirectBuf       = dstIndirectBuf;
        currentIndirectBufOffset = 0;
    }
    else
    {
        ANGLE_TRY(setupIndexedIndirectDraw(context, mode, type, currentIndirectBuf));
    }

    mRenderPassCommandBuffer->drawIndexedIndirect(
        currentIndirectBuf->getBuffer(), currentIndirectBuf->getOffset() + currentIndirectBufOffset,
        drawcount, vkStride);

    return angle::Result::Continue;
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

angle::Result ContextVk::optimizeRenderPassForPresent(VkFramebuffer framebufferHandle,
                                                      vk::ImageViewHelper *colorImageView,
                                                      vk::ImageHelper *colorImage,
                                                      vk::ImageHelper *colorImageMS,
                                                      vk::PresentMode presentMode,
                                                      bool *imageResolved)
{
    // Note: mRenderPassCommandBuffer may be nullptr because the render pass is marked for closure.
    // That doesn't matter and the render pass can continue to be modified.  This function shouldn't
    // rely on mRenderPassCommandBuffer.

    if (!mRenderPassCommands->started())
    {
        return angle::Result::Continue;
    }

    if (framebufferHandle != mRenderPassCommands->getFramebufferHandle())
    {
        return angle::Result::Continue;
    }

    // EGL1.5 spec: The contents of ancillary buffers are always undefined after calling
    // eglSwapBuffers
    FramebufferVk *drawFramebufferVk         = getDrawFramebuffer();
    RenderTargetVk *depthStencilRenderTarget = drawFramebufferVk->getDepthStencilRenderTarget();
    if (depthStencilRenderTarget != nullptr)
    {
        // Change depth/stencil attachment storeOp to DONT_CARE
        const gl::DepthStencilState &dsState = mState.getDepthStencilState();
        mRenderPassCommands->invalidateRenderPassDepthAttachment(
            dsState, mRenderPassCommands->getRenderArea());
        mRenderPassCommands->invalidateRenderPassStencilAttachment(
            dsState, mRenderPassCommands->getRenderArea());
    }

    // Use finalLayout instead of extra barrier for layout change to present
    if (colorImage != nullptr)
    {
        mRenderPassCommands->setImageOptimizeForPresent(colorImage);
    }

    // Resolve the multisample image
    vk::RenderPassCommandBufferHelper &commandBufferHelper = getStartedRenderPassCommands();
    gl::Rectangle renderArea                               = commandBufferHelper.getRenderArea();
    const gl::Rectangle invalidateArea(0, 0, colorImageMS->getExtents().width,
                                       colorImageMS->getExtents().height);
    if (colorImageMS->valid() && renderArea == invalidateArea)
    {
        vk::ImageOrBufferViewSubresourceSerial resolveImageViewSerial =
            colorImageView->getSubresourceSerial(gl::LevelIndex(0), 1, 0, vk::LayerMode::All,
                                                 vk::SrgbDecodeMode::SkipDecode,
                                                 gl::SrgbOverride::Default);
        ASSERT(resolveImageViewSerial.viewSerial.valid());
        drawFramebufferVk->updateColorResolveAttachment(0, resolveImageViewSerial);

        const vk::ImageView *resolveImageView = nullptr;
        ANGLE_TRY(colorImageView->getLevelLayerDrawImageView(this, *colorImage, vk::LevelIndex(0),
                                                             0, gl::SrgbWriteControlMode::Default,
                                                             &resolveImageView));
        vk::Framebuffer *newFramebuffer                      = nullptr;
        constexpr SwapchainResolveMode kSwapchainResolveMode = SwapchainResolveMode::Enabled;
        ANGLE_TRY(drawFramebufferVk->getFramebuffer(this, &newFramebuffer, resolveImageView,
                                                    kSwapchainResolveMode));

        commandBufferHelper.updateRenderPassForResolve(this, newFramebuffer,
                                                       drawFramebufferVk->getRenderPassDesc());

        onImageRenderPassWrite(gl::LevelIndex(0), 0, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                               vk::ImageLayout::ColorAttachment, colorImage);

        // Invalidate the surface.  See comment in WindowSurfaceVk::doDeferredAcquireNextImage on
        // why this is not done when in DEMAND_REFRESH mode.
        if (presentMode != vk::PresentMode::SharedDemandRefreshKHR)
        {
            commandBufferHelper.invalidateRenderPassColorAttachment(
                mState, 0, vk::PackedAttachmentIndex(0), invalidateArea);
        }

        ANGLE_TRY(
            flushCommandsAndEndRenderPass(RenderPassClosureReason::AlreadySpecifiedElsewhere));

        // Remove the resolve attachment from the draw framebuffer.
        drawFramebufferVk->removeColorResolveAttachment(0);

        *imageResolved = true;

        mPerfCounters.swapchainResolveInSubpass++;
    }

    return angle::Result::Continue;
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

angle::Result ContextVk::insertEventMarker(GLsizei length, const char *marker)
{
    insertEventMarkerImpl(GL_DEBUG_SOURCE_APPLICATION, marker);
    return angle::Result::Continue;
}

void ContextVk::insertEventMarkerImpl(GLenum source, const char *marker)
{
    if (!mRenderer->enableDebugUtils() && !mRenderer->angleDebuggerMode())
    {
        return;
    }

    VkDebugUtilsLabelEXT label;
    vk::MakeDebugUtilsLabel(source, marker, &label);

    if (hasStartedRenderPass())
    {
        mRenderPassCommandBuffer->insertDebugUtilsLabelEXT(label);
    }
    else
    {
        mOutsideRenderPassCommands->getCommandBuffer().insertDebugUtilsLabelEXT(label);
    }
}

angle::Result ContextVk::pushGroupMarker(GLsizei length, const char *marker)
{
    return pushDebugGroupImpl(GL_DEBUG_SOURCE_APPLICATION, 0, marker);
}

angle::Result ContextVk::popGroupMarker()
{
    return popDebugGroupImpl();
}

angle::Result ContextVk::pushDebugGroup(const gl::Context *context,
                                        GLenum source,
                                        GLuint id,
                                        const std::string &message)
{
    return pushDebugGroupImpl(source, id, message.c_str());
}

angle::Result ContextVk::popDebugGroup(const gl::Context *context)
{
    return popDebugGroupImpl();
}

angle::Result ContextVk::pushDebugGroupImpl(GLenum source, GLuint id, const char *message)
{
    if (!mRenderer->enableDebugUtils() && !mRenderer->angleDebuggerMode())
    {
        return angle::Result::Continue;
    }

    VkDebugUtilsLabelEXT label;
    vk::MakeDebugUtilsLabel(source, message, &label);

    if (hasStartedRenderPass())
    {
        mRenderPassCommandBuffer->beginDebugUtilsLabelEXT(label);
    }
    else
    {
        mOutsideRenderPassCommands->getCommandBuffer().beginDebugUtilsLabelEXT(label);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::popDebugGroupImpl()
{
    if (!mRenderer->enableDebugUtils() && !mRenderer->angleDebuggerMode())
    {
        return angle::Result::Continue;
    }

    if (hasStartedRenderPass())
    {
        mRenderPassCommandBuffer->endDebugUtilsLabelEXT();
    }
    else
    {
        mOutsideRenderPassCommands->getCommandBuffer().endDebugUtilsLabelEXT();
    }

    return angle::Result::Continue;
}

void ContextVk::logEvent(const char *eventString)
{
    if (!mRenderer->angleDebuggerMode())
    {
        return;
    }

    // Save this event (about an OpenGL ES command being called).
    mEventLog.push_back(eventString);

    // Set a dirty bit in order to stay off the "hot path" for when not logging.
    mGraphicsDirtyBits.set(DIRTY_BIT_EVENT_LOG);
    mComputeDirtyBits.set(DIRTY_BIT_EVENT_LOG);
}

void ContextVk::endEventLog(angle::EntryPoint entryPoint, PipelineType pipelineType)
{
    if (!mRenderer->angleDebuggerMode())
    {
        return;
    }

    if (pipelineType == PipelineType::Graphics)
    {
        ASSERT(mRenderPassCommands);
        mRenderPassCommands->getCommandBuffer().endDebugUtilsLabelEXT();
    }
    else
    {
        ASSERT(pipelineType == PipelineType::Compute);
        ASSERT(mOutsideRenderPassCommands);
        mOutsideRenderPassCommands->getCommandBuffer().endDebugUtilsLabelEXT();
    }
}
void ContextVk::endEventLogForClearOrQuery()
{
    if (!mRenderer->angleDebuggerMode())
    {
        return;
    }

    switch (mQueryEventType)
    {
        case GraphicsEventCmdBuf::InOutsideCmdBufQueryCmd:
            ASSERT(mOutsideRenderPassCommands);
            mOutsideRenderPassCommands->getCommandBuffer().endDebugUtilsLabelEXT();
            break;
        case GraphicsEventCmdBuf::InRenderPassCmdBufQueryCmd:
            ASSERT(mRenderPassCommands);
            mRenderPassCommands->getCommandBuffer().endDebugUtilsLabelEXT();
            break;
        case GraphicsEventCmdBuf::NotInQueryCmd:
            // The glClear* or gl*Query* command was noop'd or otherwise ended early.  We could
            // call handleDirtyEventLogImpl() to start the hierarchy, but it isn't clear which (if
            // any) command buffer to use.  We'll just skip processing this command (other than to
            // let it stay queued for the next time handleDirtyEventLogImpl() is called.
            return;
        default:
            UNREACHABLE();
    }

    mQueryEventType = GraphicsEventCmdBuf::NotInQueryCmd;
}

angle::Result ContextVk::handleNoopDrawEvent()
{
    // Even though this draw call is being no-op'd, we still must handle the dirty event log
    return handleDirtyEventLogImpl(mRenderPassCommandBuffer);
}

angle::Result ContextVk::handleGraphicsEventLog(GraphicsEventCmdBuf queryEventType)
{
    ASSERT(mQueryEventType == GraphicsEventCmdBuf::NotInQueryCmd || mEventLog.empty());
    if (!mRenderer->angleDebuggerMode())
    {
        return angle::Result::Continue;
    }

    mQueryEventType = queryEventType;

    switch (mQueryEventType)
    {
        case GraphicsEventCmdBuf::InOutsideCmdBufQueryCmd:
            ASSERT(mOutsideRenderPassCommands);
            return handleDirtyEventLogImpl(&mOutsideRenderPassCommands->getCommandBuffer());
        case GraphicsEventCmdBuf::InRenderPassCmdBufQueryCmd:
            ASSERT(mRenderPassCommands);
            return handleDirtyEventLogImpl(&mRenderPassCommands->getCommandBuffer());
        default:
            UNREACHABLE();
            return angle::Result::Stop;
    }
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

void ContextVk::updateColorMasks()
{
    const gl::BlendStateExt &blendStateExt = mState.getBlendStateExt();

    mClearColorMasks = blendStateExt.getColorMaskBits();

    FramebufferVk *framebufferVk = vk::GetImpl(mState.getDrawFramebuffer());
    mGraphicsPipelineDesc->updateColorWriteMasks(&mGraphicsPipelineTransition, mClearColorMasks,
                                                 framebufferVk->getEmulatedAlphaAttachmentMask(),
                                                 framebufferVk->getState().getEnabledDrawBuffers());

    onColorAccessChange();
}

void ContextVk::updateBlendFuncsAndEquations()
{
    const gl::BlendStateExt &blendStateExt = mState.getBlendStateExt();

    FramebufferVk *framebufferVk              = vk::GetImpl(mState.getDrawFramebuffer());
    mCachedDrawFramebufferColorAttachmentMask = framebufferVk->getState().getEnabledDrawBuffers();

    mGraphicsPipelineDesc->updateBlendFuncs(&mGraphicsPipelineTransition, blendStateExt,
                                            mCachedDrawFramebufferColorAttachmentMask);

    mGraphicsPipelineDesc->updateBlendEquations(&mGraphicsPipelineTransition, blendStateExt,
                                                mCachedDrawFramebufferColorAttachmentMask);
}

void ContextVk::updateSampleMaskWithRasterizationSamples(const uint32_t rasterizationSamples)
{
    // FramebufferVk::syncState could have been the origin for this call, at which point the
    // draw FBO may have changed, retrieve the latest draw FBO.
    FramebufferVk *drawFramebuffer = vk::GetImpl(mState.getDrawFramebuffer());

    // If sample coverage is enabled, emulate it by generating and applying a mask on top of the
    // sample mask.
    uint32_t coverageSampleCount = GetCoverageSampleCount(mState, drawFramebuffer);

    static_assert(sizeof(uint32_t) == sizeof(GLbitfield), "Vulkan assumes 32-bit sample masks");
    ASSERT(mState.getMaxSampleMaskWords() == 1);

    uint32_t mask = std::numeric_limits<uint16_t>::max();
    if (mState.isSampleMaskEnabled() && rasterizationSamples > 1)
    {
        mask = mState.getSampleMaskWord(0) & angle::BitMask<uint32_t>(rasterizationSamples);
    }

    ApplySampleCoverage(mState, coverageSampleCount, &mask);

    mGraphicsPipelineDesc->updateSampleMask(&mGraphicsPipelineTransition, 0, mask);
}

void ContextVk::updateFrameBufferFetchSamples(const uint32_t prevSamples, const uint32_t curSamples)
{
    const bool isPrevMultisampled = prevSamples > 1;
    const bool isCurMultisampled  = curSamples > 1;
    if (isPrevMultisampled != isCurMultisampled)
    {
        // If we change from single sample to multisample, we need to use the Shader Program with
        // ProgramTransformOptions.multisampleFramebufferFetch == true. Invalidate the graphics
        // pipeline so that we can fetch the shader with the correct permutation option in
        // handleDirtyGraphicsPipelineDesc()
        invalidateCurrentGraphicsPipeline();
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
                               float farPlane)
{

    gl::Box fbDimensions        = framebufferVk->getState().getDimensions();
    gl::Rectangle correctedRect = getCorrectedViewport(viewport);
    gl::Rectangle rotatedRect;
    RotateRectangle(getRotationDrawFramebuffer(), false, fbDimensions.width, fbDimensions.height,
                    correctedRect, &rotatedRect);

    const bool invertViewport =
        isViewportFlipEnabledForDrawFBO() && getFeatures().supportsNegativeViewport.enabled;

    gl_vk::GetViewport(
        rotatedRect, nearPlane, farPlane, invertViewport,
        // If clip space origin is upper left, viewport origin's y value will be offset by the
        // height of the viewport when clip space is mapped into screen space.
        mState.getClipSpaceOrigin() == gl::ClipSpaceOrigin::UpperLeft,
        // If the surface is rotated 90/270 degrees, use the framebuffer's width instead of the
        // height for calculating the final viewport.
        isRotatedAspectRatioForDrawFBO() ? fbDimensions.width : fbDimensions.height, &mViewport);

    // Ensure viewport is within Vulkan requirements
    vk::ClampViewport(&mViewport);

    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_VIEWPORT);
}

void ContextVk::updateFrontFace()
{
    if (getFeatures().supportsExtendedDynamicState.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_FRONT_FACE);
    }
    else
    {
        mGraphicsPipelineDesc->updateFrontFace(
            &mGraphicsPipelineTransition, mState.getRasterizerState(), isYFlipEnabledForDrawFBO());
    }
}

void ContextVk::updateDepthRange(float nearPlane, float farPlane)
{
    // GLES2.0 Section 2.12.1: Each of n and f are clamped to lie within [0, 1], as are all
    // arguments of type clampf.
    ASSERT(nearPlane >= 0.0f && nearPlane <= 1.0f);
    ASSERT(farPlane >= 0.0f && farPlane <= 1.0f);
    mViewport.minDepth = nearPlane;
    mViewport.maxDepth = farPlane;

    invalidateGraphicsDriverUniforms();
    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_VIEWPORT);
}

void ContextVk::updateScissor(const gl::State &glState)
{
    FramebufferVk *framebufferVk = vk::GetImpl(glState.getDrawFramebuffer());
    gl::Rectangle renderArea     = framebufferVk->getNonRotatedCompleteRenderArea();

    // Clip the render area to the viewport.
    gl::Rectangle viewportClippedRenderArea;
    if (!gl::ClipRectangle(renderArea, getCorrectedViewport(glState.getViewport()),
                           &viewportClippedRenderArea))
    {
        viewportClippedRenderArea = gl::Rectangle();
    }

    gl::Rectangle scissoredArea = ClipRectToScissor(getState(), viewportClippedRenderArea, false);
    gl::Rectangle rotatedScissoredArea;
    RotateRectangle(getRotationDrawFramebuffer(), isViewportFlipEnabledForDrawFBO(),
                    renderArea.width, renderArea.height, scissoredArea, &rotatedScissoredArea);
    mScissor = gl_vk::GetRect(rotatedScissoredArea);
    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_SCISSOR);

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

void ContextVk::updateDepthStencil(const gl::State &glState)
{
    updateDepthTestEnabled(glState);
    updateDepthWriteEnabled(glState);
    updateStencilTestEnabled(glState);
    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_WRITE_MASK);
}

void ContextVk::updateDepthTestEnabled(const gl::State &glState)
{
    const gl::DepthStencilState depthStencilState = glState.getDepthStencilState();
    gl::Framebuffer *drawFramebuffer              = glState.getDrawFramebuffer();

    if (getFeatures().supportsExtendedDynamicState.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_DEPTH_TEST_ENABLE);
    }
    else
    {
        mGraphicsPipelineDesc->updateDepthTestEnabled(&mGraphicsPipelineTransition,
                                                      depthStencilState, drawFramebuffer);
    }
}

void ContextVk::updateDepthWriteEnabled(const gl::State &glState)
{
    const gl::DepthStencilState depthStencilState = glState.getDepthStencilState();
    gl::Framebuffer *drawFramebuffer              = glState.getDrawFramebuffer();

    if (getFeatures().supportsExtendedDynamicState.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_DEPTH_WRITE_ENABLE);
    }
    else
    {
        mGraphicsPipelineDesc->updateDepthWriteEnabled(&mGraphicsPipelineTransition,
                                                       depthStencilState, drawFramebuffer);
    }
}

void ContextVk::updateDepthFunc(const gl::State &glState)
{
    if (getFeatures().supportsExtendedDynamicState.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_DEPTH_COMPARE_OP);
    }
    else
    {
        mGraphicsPipelineDesc->updateDepthFunc(&mGraphicsPipelineTransition,
                                               glState.getDepthStencilState());
    }
}

void ContextVk::updateStencilTestEnabled(const gl::State &glState)
{
    const gl::DepthStencilState depthStencilState = glState.getDepthStencilState();
    gl::Framebuffer *drawFramebuffer              = glState.getDrawFramebuffer();

    if (getFeatures().supportsExtendedDynamicState.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_TEST_ENABLE);
    }
    else
    {
        mGraphicsPipelineDesc->updateStencilTestEnabled(&mGraphicsPipelineTransition,
                                                        depthStencilState, drawFramebuffer);
    }
}

// If the target is a single-sampled target, sampleShading should be disabled, to use Bresenham line
// rasterization feature.
void ContextVk::updateSampleShadingWithRasterizationSamples(const uint32_t rasterizationSamples)
{
    bool sampleShadingEnable =
        (rasterizationSamples <= 1 ? false : mState.isSampleShadingEnabled());
    float minSampleShading = mState.getMinSampleShading();

    // If sample shading is not enabled, check if it should be implicitly enabled according to the
    // program.  Normally the driver should do this, but some drivers don't.
    if (rasterizationSamples > 1 && !sampleShadingEnable &&
        getFeatures().explicitlyEnablePerSampleShading.enabled)
    {
        const gl::ProgramExecutable *executable = mState.getProgramExecutable();
        if (executable && executable->enablesPerSampleShading())
        {
            sampleShadingEnable = true;
            minSampleShading    = 1.0;
        }
    }

    mGraphicsPipelineDesc->updateSampleShading(&mGraphicsPipelineTransition, sampleShadingEnable,
                                               minSampleShading);
}

// If the target is switched between a single-sampled and multisample, the dependency related to the
// rasterization sample should be updated.
void ContextVk::updateRasterizationSamples(const uint32_t rasterizationSamples)
{
    uint32_t prevSampleCount = mGraphicsPipelineDesc->getRasterizationSamples();
    updateFrameBufferFetchSamples(prevSampleCount, rasterizationSamples);
    mGraphicsPipelineDesc->updateRasterizationSamples(&mGraphicsPipelineTransition,
                                                      rasterizationSamples);
    updateSampleShadingWithRasterizationSamples(rasterizationSamples);
    updateSampleMaskWithRasterizationSamples(rasterizationSamples);
}

void ContextVk::updateRasterizerDiscardEnabled(bool isPrimitivesGeneratedQueryActive)
{
    // On some devices, when rasterizerDiscardEnable is enabled, the
    // VK_EXT_primitives_generated_query as well as the pipeline statistics query used to emulate it
    // are non-functional.  For VK_EXT_primitives_generated_query there's a feature bit but not for
    // pipeline statistics query.  If the primitives generated query is active (and rasterizer
    // discard is not supported), rasterizerDiscardEnable is set to false and the functionality
    // is otherwise emulated (by using an empty scissor).

    // If the primitives generated query implementation supports rasterizer discard, just set
    // rasterizer discard as requested.  Otherwise disable it.
    const bool isEmulatingRasterizerDiscard =
        isEmulatingRasterizerDiscardDuringPrimitivesGeneratedQuery(
            isPrimitivesGeneratedQueryActive);

    if (getFeatures().supportsExtendedDynamicState2.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_RASTERIZER_DISCARD_ENABLE);
    }
    else
    {
        const bool isRasterizerDiscardEnabled = mState.isRasterizerDiscardEnabled();

        mGraphicsPipelineDesc->updateRasterizerDiscardEnabled(
            &mGraphicsPipelineTransition,
            isRasterizerDiscardEnabled && !isEmulatingRasterizerDiscard);

        invalidateCurrentGraphicsPipeline();
    }

    if (isEmulatingRasterizerDiscard)
    {
        // If we are emulating rasterizer discard, update the scissor to use an empty one if
        // rasterizer discard is enabled.
        mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_SCISSOR);
    }
}

void ContextVk::updateAdvancedBlendEquations(const gl::ProgramExecutable *executable)
{
    if (!getFeatures().emulateAdvancedBlendEquations.enabled || executable == nullptr)
    {
        return;
    }

    // If advanced blend equations is emulated and the program uses advanced equations, update the
    // driver uniforms to pass the equation to the shader.
    if (executable->getAdvancedBlendEquations().any())
    {
        invalidateGraphicsDriverUniforms();
    }
}

void ContextVk::updateDither()
{
    if (!getFeatures().emulateDithering.enabled)
    {
        return;
    }

    FramebufferVk *framebufferVk = vk::GetImpl(mState.getDrawFramebuffer());

    // Dithering in OpenGL is vaguely defined, to the extent that no dithering is also a valid
    // dithering algorithm.  Dithering is enabled by default, but emulating it has a non-negligible
    // cost.  Similarly to some other GLES drivers, ANGLE enables dithering only on low-bit formats
    // where visual banding is particularly common; namely RGBA4444, RGBA5551 and RGB565.
    //
    // Dithering is emulated in the fragment shader and is controlled by a spec constant.  Every 2
    // bits of the spec constant correspond to one attachment, with the value indicating:
    //
    // - 00: No dithering
    // - 01: Dither for RGBA4444
    // - 10: Dither for RGBA5551
    // - 11: Dither for RGB565
    //
    uint16_t ditherControl = 0;
    if (mState.isDitherEnabled())
    {
        const gl::DrawBufferMask attachmentMask =
            framebufferVk->getState().getColorAttachmentsMask();

        for (size_t colorIndex : attachmentMask)
        {
            // As dithering is emulated in the fragment shader itself, there are a number of
            // situations that can lead to incorrect blending.  We only allow blending with specific
            // combinations know to not interfere with dithering.
            if (mState.isBlendEnabledIndexed(static_cast<GLuint>(colorIndex)) &&
                !BlendModeSupportsDither(mState, colorIndex))
            {
                continue;
            }

            RenderTargetVk *attachment   = framebufferVk->getColorDrawRenderTarget(colorIndex);
            const angle::FormatID format = attachment->getImageActualFormatID();

            uint16_t attachmentDitherControl = sh::vk::kDitherControlNoDither;
            switch (format)
            {
                case angle::FormatID::R4G4B4A4_UNORM:
                case angle::FormatID::B4G4R4A4_UNORM:
                    attachmentDitherControl = sh::vk::kDitherControlDither4444;
                    break;
                case angle::FormatID::R5G5B5A1_UNORM:
                case angle::FormatID::B5G5R5A1_UNORM:
                case angle::FormatID::A1R5G5B5_UNORM:
                    attachmentDitherControl = sh::vk::kDitherControlDither5551;
                    break;
                case angle::FormatID::R5G6B5_UNORM:
                case angle::FormatID::B5G6R5_UNORM:
                    attachmentDitherControl = sh::vk::kDitherControlDither565;
                    break;
                default:
                    break;
            }

            ditherControl |= static_cast<uint16_t>(attachmentDitherControl << 2 * colorIndex);
        }
    }

    if (ditherControl != mGraphicsPipelineDesc->getEmulatedDitherControl())
    {
        mGraphicsPipelineDesc->updateEmulatedDitherControl(&mGraphicsPipelineTransition,
                                                           ditherControl);
        invalidateCurrentGraphicsPipeline();
    }
}

void ContextVk::updateStencilWriteWorkaround()
{
    if (!getFeatures().useNonZeroStencilWriteMaskStaticState.enabled)
    {
        return;
    }

    // On certain drivers, having a stencil write mask of 0 in static state enables optimizations
    // that make the interaction of the stencil write mask dynamic state with discard and alpha to
    // coverage broken.  When the program has discard, or when alpha to coverage is enabled, these
    // optimizations are disabled by specifying a non-zero static state for stencil write mask.
    const bool programHasDiscard        = mState.getProgramExecutable()->hasDiscard();
    const bool isAlphaToCoverageEnabled = mState.isSampleAlphaToCoverageEnabled();

    mGraphicsPipelineDesc->updateNonZeroStencilWriteMaskWorkaround(
        &mGraphicsPipelineTransition, programHasDiscard || isAlphaToCoverageEnabled);
}

angle::Result ContextVk::invalidateProgramExecutableHelper(const gl::Context *context)
{
    const gl::State &glState                = context->getState();
    const gl::ProgramExecutable *executable = glState.getProgramExecutable();

    if (executable->hasLinkedShaderStage(gl::ShaderType::Compute))
    {
        invalidateCurrentComputePipeline();
    }

    if (executable->hasLinkedShaderStage(gl::ShaderType::Vertex))
    {
        invalidateCurrentGraphicsPipeline();
        // No additional work is needed here. We will update the pipeline desc
        // later.
        invalidateDefaultAttributes(context->getStateCache().getActiveDefaultAttribsMask());
        invalidateVertexAndIndexBuffers();
        bool useVertexBuffer = (executable->getMaxActiveAttribLocation() > 0);
        mNonIndexedDirtyBitsMask.set(DIRTY_BIT_VERTEX_BUFFERS, useVertexBuffer);
        mIndexedDirtyBitsMask.set(DIRTY_BIT_VERTEX_BUFFERS, useVertexBuffer);
        mCurrentGraphicsPipeline = nullptr;
        mGraphicsPipelineTransition.reset();

        const bool hasFramebufferFetch = executable->usesFramebufferFetch();
        if (mIsInFramebufferFetchMode != hasFramebufferFetch)
        {
            ASSERT(getDrawFramebuffer()->getRenderPassDesc().hasFramebufferFetch() ==
                   mIsInFramebufferFetchMode);

            ANGLE_TRY(switchToFramebufferFetchMode(hasFramebufferFetch));

            // When framebuffer fetch is enabled, attachments can be read from even if output is
            // masked, so update their access.
            onColorAccessChange();
        }

        updateStencilWriteWorkaround();
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::syncState(const gl::Context *context,
                                   const gl::State::DirtyBits &dirtyBits,
                                   const gl::State::DirtyBits &bitMask,
                                   gl::Command command)
{
    const gl::State &glState                       = context->getState();
    const gl::ProgramExecutable *programExecutable = glState.getProgramExecutable();

    if ((dirtyBits & mPipelineDirtyBitsMask).any() &&
        (programExecutable == nullptr || command != gl::Command::Dispatch))
    {
        invalidateCurrentGraphicsPipeline();
    }

    FramebufferVk *drawFramebufferVk = getDrawFramebuffer();
    VertexArrayVk *vertexArrayVk     = getVertexArray();

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
                               glState.getFarPlane());
                // Update the scissor, which will be constrained to the viewport
                updateScissor(glState);
                break;
            }
            case gl::State::DIRTY_BIT_DEPTH_RANGE:
                updateDepthRange(glState.getNearPlane(), glState.getFarPlane());
                break;
            case gl::State::DIRTY_BIT_BLEND_ENABLED:
                mGraphicsPipelineDesc->updateBlendEnabled(
                    &mGraphicsPipelineTransition, glState.getBlendStateExt().getEnabledMask());
                updateDither();
                break;
            case gl::State::DIRTY_BIT_BLEND_COLOR:
                mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_BLEND_CONSTANTS);
                break;
            case gl::State::DIRTY_BIT_BLEND_FUNCS:
                mGraphicsPipelineDesc->updateBlendFuncs(
                    &mGraphicsPipelineTransition, glState.getBlendStateExt(),
                    drawFramebufferVk->getState().getColorAttachmentsMask());
                break;
            case gl::State::DIRTY_BIT_BLEND_EQUATIONS:
                mGraphicsPipelineDesc->updateBlendEquations(
                    &mGraphicsPipelineTransition, glState.getBlendStateExt(),
                    drawFramebufferVk->getState().getColorAttachmentsMask());
                updateAdvancedBlendEquations(programExecutable);
                break;
            case gl::State::DIRTY_BIT_COLOR_MASK:
                updateColorMasks();
                break;
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED:
                mGraphicsPipelineDesc->updateAlphaToCoverageEnable(
                    &mGraphicsPipelineTransition, glState.isSampleAlphaToCoverageEnabled());
                updateStencilWriteWorkaround();

                static_assert(gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE >
                                  gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED,
                              "Dirty bit order");
                iter.setLaterBit(gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE);
                break;
            case gl::State::DIRTY_BIT_SAMPLE_COVERAGE_ENABLED:
                updateSampleMaskWithRasterizationSamples(drawFramebufferVk->getSamples());
                break;
            case gl::State::DIRTY_BIT_SAMPLE_COVERAGE:
                updateSampleMaskWithRasterizationSamples(drawFramebufferVk->getSamples());
                break;
            case gl::State::DIRTY_BIT_SAMPLE_MASK_ENABLED:
                updateSampleMaskWithRasterizationSamples(drawFramebufferVk->getSamples());
                break;
            case gl::State::DIRTY_BIT_SAMPLE_MASK:
                updateSampleMaskWithRasterizationSamples(drawFramebufferVk->getSamples());
                break;
            case gl::State::DIRTY_BIT_DEPTH_TEST_ENABLED:
                updateDepthTestEnabled(glState);
                iter.setLaterBit(gl::State::DIRTY_BIT_DEPTH_MASK);
                break;
            case gl::State::DIRTY_BIT_DEPTH_FUNC:
                updateDepthFunc(glState);
                break;
            case gl::State::DIRTY_BIT_DEPTH_MASK:
                updateDepthWriteEnabled(glState);
                onDepthStencilAccessChange();
                break;
            case gl::State::DIRTY_BIT_STENCIL_TEST_ENABLED:
                updateStencilTestEnabled(glState);
                onDepthStencilAccessChange();
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_FRONT:
                if (getFeatures().supportsExtendedDynamicState.enabled)
                {
                    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_OP);
                }
                else
                {
                    mGraphicsPipelineDesc->updateStencilFrontFuncs(&mGraphicsPipelineTransition,
                                                                   glState.getDepthStencilState());
                }
                mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_COMPARE_MASK);
                mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_REFERENCE);
                onDepthStencilAccessChange();
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_BACK:
                if (getFeatures().supportsExtendedDynamicState.enabled)
                {
                    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_OP);
                }
                else
                {
                    mGraphicsPipelineDesc->updateStencilBackFuncs(&mGraphicsPipelineTransition,
                                                                  glState.getDepthStencilState());
                }
                mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_COMPARE_MASK);
                mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_REFERENCE);
                onDepthStencilAccessChange();
                break;
            case gl::State::DIRTY_BIT_STENCIL_OPS_FRONT:
                if (getFeatures().supportsExtendedDynamicState.enabled)
                {
                    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_OP);
                }
                else
                {
                    mGraphicsPipelineDesc->updateStencilFrontOps(&mGraphicsPipelineTransition,
                                                                 glState.getDepthStencilState());
                }
                onDepthStencilAccessChange();
                break;
            case gl::State::DIRTY_BIT_STENCIL_OPS_BACK:
                if (getFeatures().supportsExtendedDynamicState.enabled)
                {
                    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_OP);
                }
                else
                {
                    mGraphicsPipelineDesc->updateStencilBackOps(&mGraphicsPipelineTransition,
                                                                glState.getDepthStencilState());
                }
                onDepthStencilAccessChange();
                break;
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT:
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_BACK:
                mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_STENCIL_WRITE_MASK);
                onDepthStencilAccessChange();
                break;
            case gl::State::DIRTY_BIT_CULL_FACE_ENABLED:
            case gl::State::DIRTY_BIT_CULL_FACE:
                if (getFeatures().supportsExtendedDynamicState.enabled)
                {
                    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_CULL_MODE);
                }
                else
                {
                    mGraphicsPipelineDesc->updateCullMode(&mGraphicsPipelineTransition,
                                                          glState.getRasterizerState());
                }
                break;
            case gl::State::DIRTY_BIT_FRONT_FACE:
                updateFrontFace();
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED:
                if (getFeatures().supportsExtendedDynamicState2.enabled)
                {
                    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_DEPTH_BIAS_ENABLE);
                }
                else
                {
                    mGraphicsPipelineDesc->updatePolygonOffsetFillEnabled(
                        &mGraphicsPipelineTransition, glState.isPolygonOffsetFillEnabled());
                }
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET:
                mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_DEPTH_BIAS);
                break;
            case gl::State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED:
                updateRasterizerDiscardEnabled(
                    mState.isQueryActive(gl::QueryType::PrimitivesGenerated));
                onColorAccessChange();
                break;
            case gl::State::DIRTY_BIT_LINE_WIDTH:
                mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_LINE_WIDTH);
                break;
            case gl::State::DIRTY_BIT_PRIMITIVE_RESTART_ENABLED:
                if (getFeatures().supportsExtendedDynamicState2.enabled)
                {
                    mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_PRIMITIVE_RESTART_ENABLE);
                }
                else
                {
                    mGraphicsPipelineDesc->updatePrimitiveRestartEnabled(
                        &mGraphicsPipelineTransition, glState.isPrimitiveRestartEnabled());
                }
                // Additionally set the index buffer dirty if conversion from uint8 might have been
                // necessary.  Otherwise if primitive restart is enabled and the index buffer is
                // translated to uint16_t with a value of 0xFFFF, it cannot be reused when primitive
                // restart is disabled.
                if (!mRenderer->getFeatures().supportsIndexTypeUint8.enabled)
                {
                    mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
                }
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
                updateDither();
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
                onRenderPassFinished(RenderPassClosureReason::FramebufferBindingChange);
                if (getFeatures().preferSubmitAtFBOBoundary.enabled)
                {
                    // This will behave as if user called glFlush, but the actual flush will be
                    // triggered at endRenderPass time.
                    mHasDeferredFlush = true;
                }

                drawFramebufferVk->setReadOnlyDepthFeedbackLoopMode(false);
                updateFlipViewportDrawFramebuffer(glState);
                updateSurfaceRotationDrawFramebuffer(glState);
                SpecConstUsageBits usageBits = getCurrentProgramSpecConstUsageBits();
                updateGraphicsPipelineDescWithSpecConstUsageBits(usageBits);
                updateViewport(drawFramebufferVk, glState.getViewport(), glState.getNearPlane(),
                               glState.getFarPlane());
                updateColorMasks();
                updateRasterizationSamples(drawFramebufferVk->getSamples());
                updateRasterizerDiscardEnabled(
                    mState.isQueryActive(gl::QueryType::PrimitivesGenerated));

                updateFrontFace();
                updateScissor(glState);
                updateDepthStencil(glState);
                updateDither();

                // Clear the blend funcs/equations for color attachment indices that no longer
                // exist.
                gl::DrawBufferMask newColorAttachmentMask =
                    drawFramebufferVk->getState().getColorAttachmentsMask();
                mGraphicsPipelineDesc->resetBlendFuncsAndEquations(
                    &mGraphicsPipelineTransition, glState.getBlendStateExt(),
                    mCachedDrawFramebufferColorAttachmentMask, newColorAttachmentMask);
                mCachedDrawFramebufferColorAttachmentMask = newColorAttachmentMask;

                mGraphicsPipelineDesc->resetSubpass(&mGraphicsPipelineTransition);

                // The framebuffer may not be in sync with usage of framebuffer fetch programs.
                drawFramebufferVk->switchToFramebufferFetchMode(this, mIsInFramebufferFetchMode);

                onDrawFramebufferRenderPassDescChange(drawFramebufferVk, nullptr);

                break;
            }
            case gl::State::DIRTY_BIT_RENDERBUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_VERTEX_ARRAY_BINDING:
            {
                invalidateDefaultAttributes(context->getStateCache().getActiveDefaultAttribsMask());
                ANGLE_TRY(vertexArrayVk->updateActiveAttribInfo(this));
                ANGLE_TRY(onIndexBufferChange(vertexArrayVk->getCurrentElementArrayBuffer()));
                break;
            }
            case gl::State::DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_DISPATCH_INDIRECT_BUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_PROGRAM_BINDING:
                static_assert(
                    gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE > gl::State::DIRTY_BIT_PROGRAM_BINDING,
                    "Dirty bit order");
                iter.setLaterBit(gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE);
                break;
            case gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE:
            {
                ASSERT(programExecutable);
                invalidateCurrentDefaultUniforms();
                updateAdvancedBlendEquations(programExecutable);
                getExecutable()->onProgramBind(*programExecutable);
                static_assert(
                    gl::State::DIRTY_BIT_TEXTURE_BINDINGS > gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE,
                    "Dirty bit order");
                iter.setLaterBit(gl::State::DIRTY_BIT_TEXTURE_BINDINGS);
                static_assert(gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING >
                                  gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE,
                              "Dirty bit order");
                iter.setLaterBit(gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING);
                ANGLE_TRY(invalidateProgramExecutableHelper(context));

                static_assert(
                    gl::State::DIRTY_BIT_SAMPLE_SHADING > gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE,
                    "Dirty bit order");
                if (getFeatures().explicitlyEnablePerSampleShading.enabled)
                {
                    iter.setLaterBit(gl::State::DIRTY_BIT_SAMPLE_SHADING);
                }

                break;
            }
            case gl::State::DIRTY_BIT_SAMPLER_BINDINGS:
            {
                static_assert(
                    gl::State::DIRTY_BIT_TEXTURE_BINDINGS > gl::State::DIRTY_BIT_SAMPLER_BINDINGS,
                    "Dirty bit order");
                iter.setLaterBit(gl::State::DIRTY_BIT_TEXTURE_BINDINGS);
                break;
            }
            case gl::State::DIRTY_BIT_TEXTURE_BINDINGS:
                ANGLE_TRY(invalidateCurrentTextures(context, command));
                break;
            case gl::State::DIRTY_BIT_TRANSFORM_FEEDBACK_BINDING:
                // Nothing to do.
                break;
            case gl::State::DIRTY_BIT_IMAGE_BINDINGS:
                static_assert(gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING >
                                  gl::State::DIRTY_BIT_IMAGE_BINDINGS,
                              "Dirty bit order");
                iter.setLaterBit(gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING);
                break;
            case gl::State::DIRTY_BIT_SHADER_STORAGE_BUFFER_BINDING:
                static_assert(gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING >
                                  gl::State::DIRTY_BIT_SHADER_STORAGE_BUFFER_BINDING,
                              "Dirty bit order");
                iter.setLaterBit(gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING);
                break;
            case gl::State::DIRTY_BIT_UNIFORM_BUFFER_BINDINGS:
                static_assert(gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING >
                                  gl::State::DIRTY_BIT_UNIFORM_BUFFER_BINDINGS,
                              "Dirty bit order");
                iter.setLaterBit(gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING);
                break;
            case gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING:
                ANGLE_TRY(invalidateCurrentShaderResources(command));
                invalidateDriverUniforms();
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
                updateSampleShadingWithRasterizationSamples(drawFramebufferVk->getSamples());
                break;
            case gl::State::DIRTY_BIT_COVERAGE_MODULATION:
                break;
            case gl::State::DIRTY_BIT_FRAMEBUFFER_SRGB_WRITE_CONTROL_MODE:
                break;
            case gl::State::DIRTY_BIT_CURRENT_VALUES:
            {
                invalidateDefaultAttributes(glState.getAndResetDirtyCurrentValues());
                break;
            }
            case gl::State::DIRTY_BIT_PROVOKING_VERTEX:
                break;
            case gl::State::DIRTY_BIT_EXTENDED:
            {
                gl::State::ExtendedDirtyBits extendedDirtyBits =
                    glState.getAndResetExtendedDirtyBits();
                for (size_t extendedDirtyBit : extendedDirtyBits)
                {
                    switch (extendedDirtyBit)
                    {
                        case gl::State::ExtendedDirtyBitType::EXTENDED_DIRTY_BIT_CLIP_CONTROL:
                            updateViewport(vk::GetImpl(glState.getDrawFramebuffer()),
                                           glState.getViewport(), glState.getNearPlane(),
                                           glState.getFarPlane());
                            // Since we are flipping the y coordinate, update front face state
                            updateFrontFace();
                            updateScissor(glState);

                            // If VK_EXT_depth_clip_control is not enabled, there's nothing needed
                            // for depth correction for EXT_clip_control.
                            // glState will be used to toggle control path of depth correction code
                            // in SPIR-V tranform options.
                            if (getFeatures().supportsDepthClipControl.enabled)
                            {
                                mGraphicsPipelineDesc->updateDepthClipControl(
                                    &mGraphicsPipelineTransition,
                                    !glState.isClipControlDepthZeroToOne());
                            }
                            else
                            {
                                invalidateGraphicsDriverUniforms();
                            }
                            break;
                        case gl::State::ExtendedDirtyBitType::EXTENDED_DIRTY_BIT_CLIP_DISTANCES:
                            invalidateGraphicsDriverUniforms();
                            break;
                        case gl::State::ExtendedDirtyBitType::
                            EXTENDED_DIRTY_BIT_MIPMAP_GENERATION_HINT:
                            break;
                        case gl::State::ExtendedDirtyBitType::
                            EXTENDED_DIRTY_BIT_SHADER_DERIVATIVE_HINT:
                            break;
                        case gl::State::ExtendedDirtyBitType::EXTENDED_DIRTY_BIT_SHADING_RATE:
                            mGraphicsDirtyBits.set(DIRTY_BIT_DYNAMIC_FRAGMENT_SHADING_RATE);
                            break;
                        default:
                            UNREACHABLE();
                    }
                }
                break;
            }
            case gl::State::DIRTY_BIT_PATCH_VERTICES:
                mGraphicsPipelineDesc->updatePatchVertices(&mGraphicsPipelineTransition,
                                                           glState.getPatchVertices());
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
    if (!getFeatures().preferDriverUniformOverSpecConst.enabled)
    {
        // Force update mGraphicsPipelineDesc
        mCurrentGraphicsPipeline = nullptr;
        invalidateCurrentGraphicsPipeline();
    }

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
    ANGLE_TRY(flushImpl(nullptr, RenderPassClosureReason::ContextChange));
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

SpecConstUsageBits ContextVk::getCurrentProgramSpecConstUsageBits() const
{
    SpecConstUsageBits usageBits;
    if (mState.getProgram())
    {
        usageBits = mState.getProgram()->getState().getSpecConstUsageBits();
    }
    else if (mState.getProgramPipeline())
    {
        usageBits = mState.getProgramPipeline()->getState().getSpecConstUsageBits();
    }
    return usageBits;
}

void ContextVk::updateGraphicsPipelineDescWithSpecConstUsageBits(SpecConstUsageBits usageBits)
{
    SurfaceRotation rotation = mCurrentRotationDrawFramebuffer;

    if (IsRotatedAspectRatio(rotation) != mGraphicsPipelineDesc->getSurfaceRotation())
    {
        // surface rotation are specialization constants, which affects program compilation. When
        // rotation changes, we need to update GraphicsPipelineDesc so that the correct pipeline
        // program object will be retrieved.
        mGraphicsPipelineDesc->updateSurfaceRotation(&mGraphicsPipelineTransition, rotation);
    }
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

angle::Result ContextVk::invalidateCurrentTextures(const gl::Context *context, gl::Command command)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    if (executable->hasTextures())
    {
        mGraphicsDirtyBits |= kTexturesAndDescSetDirtyBits;
        mComputeDirtyBits |= kTexturesAndDescSetDirtyBits;

        ANGLE_TRY(updateActiveTextures(context, command));

        if (command == gl::Command::Dispatch)
        {
            ANGLE_TRY(endRenderPassIfComputeAccessAfterGraphicsImageAccess());
        }
    }
    return angle::Result::Continue;
}

angle::Result ContextVk::invalidateCurrentShaderResources(gl::Command command)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    const bool hasImages = executable->hasImages();
    const bool hasStorageBuffers =
        executable->hasStorageBuffers() || executable->hasAtomicCounterBuffers();
    const bool hasUniformBuffers = executable->hasUniformBuffers();

    if (hasUniformBuffers || hasStorageBuffers || hasImages || executable->usesFramebufferFetch())
    {
        mGraphicsDirtyBits |= kResourcesAndDescSetDirtyBits;
        mComputeDirtyBits |= kResourcesAndDescSetDirtyBits;
    }

    // Take care of read-after-write hazards that require implicit synchronization.
    if (hasUniformBuffers && command == gl::Command::Dispatch)
    {
        ANGLE_TRY(endRenderPassIfComputeReadAfterTransformFeedbackWrite());
    }

    // Take care of implict layout transition by compute program access-after-read.
    if (hasImages && command == gl::Command::Dispatch)
    {
        ANGLE_TRY(endRenderPassIfComputeAccessAfterGraphicsImageAccess());
    }

    // If memory barrier has been issued but the command buffers haven't been flushed, make sure
    // they get a chance to do so if necessary on program and storage buffer/image binding change.
    const bool hasGLMemoryBarrierIssuedInCommandBuffers =
        mOutsideRenderPassCommands->hasGLMemoryBarrierIssued() ||
        mRenderPassCommands->hasGLMemoryBarrierIssued();

    if ((hasStorageBuffers || hasImages) && hasGLMemoryBarrierIssuedInCommandBuffers)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_MEMORY_BARRIER);
        mComputeDirtyBits.set(DIRTY_BIT_MEMORY_BARRIER);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::updateShaderResourcesDescriptorDesc(PipelineType pipelineType)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);

    const bool hasStorageBuffers =
        executable->hasStorageBuffers() || executable->hasAtomicCounterBuffers();
    const bool hasUniformBuffers = executable->hasUniformBuffers();
    const bool hasStorageImages  = executable->hasImages();
    const bool usesFetch         = executable->usesFramebufferFetch();

    if (!hasUniformBuffers && !hasStorageBuffers && !hasStorageImages && !usesFetch)
    {
        return angle::Result::Continue;
    }

    mShaderBuffersDescriptorDesc.reset();

    const VkPhysicalDeviceLimits &limits    = mRenderer->getPhysicalDeviceProperties().limits;
    const ProgramExecutableVk &executableVk = *getExecutable();
    const ShaderInterfaceVariableInfoMap &variableInfoMap = executableVk.getVariableInfoMap();

    for (gl::ShaderType shaderType : executable->getLinkedShaderStages())
    {
        mShaderBuffersDescriptorDesc.updateShaderBuffers(
            shaderType, ShaderVariableType::UniformBuffer, variableInfoMap,
            mState.getOffsetBindingPointerUniformBuffers(), executable->getUniformBlocks(),
            executableVk.getUniformBufferDescriptorType(), limits.maxUniformBufferRange,
            mEmptyBuffer);
        mShaderBuffersDescriptorDesc.updateShaderBuffers(
            shaderType, ShaderVariableType::ShaderStorageBuffer, variableInfoMap,
            mState.getOffsetBindingPointerShaderStorageBuffers(),
            executable->getShaderStorageBlocks(), executableVk.getStorageBufferDescriptorType(),
            limits.maxStorageBufferRange, mEmptyBuffer);
        mShaderBuffersDescriptorDesc.updateAtomicCounters(
            shaderType, variableInfoMap, mState.getOffsetBindingPointerAtomicCounterBuffers(),
            executable->getAtomicCounterBuffers(), limits.minStorageBufferOffsetAlignment,
            &mEmptyBuffer);
        ANGLE_TRY(mShaderBuffersDescriptorDesc.updateImages(
            this, shaderType, *executable, variableInfoMap, mActiveImages, mState.getImageUnits()));
        ANGLE_TRY(mShaderBuffersDescriptorDesc.updateInputAttachments(
            this, shaderType, *executable, variableInfoMap,
            vk::GetImpl(mState.getDrawFramebuffer())));
    }

    return angle::Result::Continue;
}

void ContextVk::invalidateGraphicsDriverUniforms()
{
    mGraphicsDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS);
}

void ContextVk::invalidateDriverUniforms()
{
    mGraphicsDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS);
    mComputeDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS);
}

angle::Result ContextVk::onFramebufferChange(FramebufferVk *framebufferVk, gl::Command command)
{
    // This is called from FramebufferVk::syncState.  Skip these updates if the framebuffer being
    // synced is the read framebuffer (which is not equal the draw framebuffer).
    if (framebufferVk != vk::GetImpl(mState.getDrawFramebuffer()))
    {
        return angle::Result::Continue;
    }

    // Always consider the render pass finished.  FramebufferVk::syncState (caller of this function)
    // normally closes the render pass, except for blit to allow an optimization.  The following
    // code nevertheless must treat the render pass closed.
    onRenderPassFinished(RenderPassClosureReason::FramebufferChange);

    // Ensure that the pipeline description is updated.
    if (mGraphicsPipelineDesc->getRasterizationSamples() !=
        static_cast<uint32_t>(framebufferVk->getSamples()))
    {
        updateRasterizationSamples(framebufferVk->getSamples());
    }

    // Update scissor.
    updateScissor(mState);

    // Update depth and stencil.
    updateDepthStencil(mState);

    // Update dither based on attachment formats.
    updateDither();

    if (mState.getProgramExecutable())
    {
        ANGLE_TRY(invalidateCurrentShaderResources(command));
    }

    onDrawFramebufferRenderPassDescChange(framebufferVk, nullptr);
    return angle::Result::Continue;
}

void ContextVk::onDrawFramebufferRenderPassDescChange(FramebufferVk *framebufferVk,
                                                      bool *renderPassDescChangedOut)
{
    mGraphicsPipelineDesc->updateRenderPassDesc(&mGraphicsPipelineTransition,
                                                framebufferVk->getRenderPassDesc());

    if (renderPassDescChangedOut)
    {
        // If render pass desc has changed while processing the dirty bits, notify the caller.
        *renderPassDescChangedOut = true;
    }
    else
    {
        // Otherwise mark the pipeline as dirty.
        invalidateCurrentGraphicsPipeline();
    }

    // Update render area in the driver uniforms.
    invalidateGraphicsDriverUniforms();
}

void ContextVk::invalidateCurrentTransformFeedbackBuffers()
{
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS);
    }
    else if (getFeatures().emulateTransformFeedback.enabled)
    {
        mGraphicsDirtyBits |= kXfbBuffersAndDescSetDirtyBits;
    }
}

void ContextVk::onTransformFeedbackStateChanged()
{
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS);
    }
    else if (getFeatures().emulateTransformFeedback.enabled)
    {
        invalidateGraphicsDriverUniforms();
        invalidateCurrentTransformFeedbackBuffers();

        // Invalidate the graphics pipeline too.  On transform feedback state change, the current
        // program may be used again, and it should switch between outputting transform feedback and
        // not.
        invalidateCurrentGraphicsPipeline();
        resetCurrentGraphicsPipeline();
    }
}

angle::Result ContextVk::onBeginTransformFeedback(
    size_t bufferCount,
    const gl::TransformFeedbackBuffersArray<vk::BufferHelper *> &buffers,
    const gl::TransformFeedbackBuffersArray<vk::BufferHelper> &counterBuffers)
{
    onTransformFeedbackStateChanged();

    bool shouldEndRenderPass = false;

    // If any of the buffers were previously used in the render pass, break the render pass as a
    // barrier is needed.
    for (size_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
    {
        const vk::BufferHelper *buffer = buffers[bufferIndex];
        if (mRenderPassCommands->usesBuffer(*buffer))
        {
            shouldEndRenderPass = true;
            break;
        }
    }

    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        // Break the render pass if the counter buffers are used too.  Note that Vulkan requires a
        // barrier on the counter buffer between pause and resume, so it cannot be resumed in the
        // same render pass.  Note additionally that we don't need to test all counters being used
        // in the render pass, as outside of the transform feedback object these buffers are
        // inaccessible and are therefore always used together.
        if (!shouldEndRenderPass && mRenderPassCommands->usesBuffer(counterBuffers[0]))
        {
            shouldEndRenderPass = true;
        }

        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME);
    }

    if (shouldEndRenderPass)
    {
        ANGLE_TRY(flushCommandsAndEndRenderPass(RenderPassClosureReason::BufferUseThenXfbWrite));
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
        // If transform feedback was already active on this render pass, break it.  This
        // is for simplicity to avoid tracking multiple simultaneously active transform feedback
        // settings in the render pass.
        if (mRenderPassCommands->isTransformFeedbackActiveUnpaused())
        {
            return flushCommandsAndEndRenderPass(RenderPassClosureReason::XfbPause);
        }
    }
    else if (getFeatures().emulateTransformFeedback.enabled)
    {
        invalidateCurrentTransformFeedbackBuffers();
    }
    return angle::Result::Continue;
}

void ContextVk::invalidateGraphicsPipelineBinding()
{
    mGraphicsDirtyBits.set(DIRTY_BIT_PIPELINE_BINDING);
}

void ContextVk::invalidateComputePipelineBinding()
{
    mComputeDirtyBits.set(DIRTY_BIT_PIPELINE_BINDING);
}

void ContextVk::invalidateGraphicsDescriptorSet(DescriptorSetIndex usedDescriptorSet)
{
    // UtilsVk currently only uses set 0
    ASSERT(usedDescriptorSet == DescriptorSetIndex::Internal);
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();

    if (executable && executable->hasUniformBuffers())
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
        return;
    }
}

void ContextVk::invalidateComputeDescriptorSet(DescriptorSetIndex usedDescriptorSet)
{
    // UtilsVk currently only uses set 0
    ASSERT(usedDescriptorSet == DescriptorSetIndex::Internal);
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();

    if (executable && executable->hasUniformBuffers())
    {
        mComputeDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
        return;
    }
}

void ContextVk::invalidateAllDynamicState()
{
    mGraphicsDirtyBits |= mDynamicStateDirtyBits;
}

angle::Result ContextVk::dispatchCompute(const gl::Context *context,
                                         GLuint numGroupsX,
                                         GLuint numGroupsY,
                                         GLuint numGroupsZ)
{
    ANGLE_TRY(setupDispatch(context));

    mOutsideRenderPassCommands->getCommandBuffer().dispatch(numGroupsX, numGroupsY, numGroupsZ);

    return angle::Result::Continue;
}

angle::Result ContextVk::dispatchComputeIndirect(const gl::Context *context, GLintptr indirect)
{
    gl::Buffer *glBuffer     = getState().getTargetBuffer(gl::BufferBinding::DispatchIndirect);
    vk::BufferHelper &buffer = vk::GetImpl(glBuffer)->getBuffer();

    // Break the render pass if the indirect buffer was previously used as the output from transform
    // feedback.
    if (mCurrentTransformFeedbackBuffers.contains(&buffer))
    {
        ANGLE_TRY(flushCommandsAndEndRenderPass(
            RenderPassClosureReason::XfbWriteThenIndirectDispatchBuffer));
    }

    ANGLE_TRY(setupDispatch(context));

    // Process indirect buffer after command buffer has started.
    mOutsideRenderPassCommands->bufferRead(this, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                           vk::PipelineStage::DrawIndirect, &buffer);

    mOutsideRenderPassCommands->getCommandBuffer().dispatchIndirect(buffer.getBuffer(),
                                                                    buffer.getOffset() + indirect);

    return angle::Result::Continue;
}

angle::Result ContextVk::memoryBarrier(const gl::Context *context, GLbitfield barriers)
{
    // First, turn GL_ALL_BARRIER_BITS into a mask that has only the valid barriers set.
    constexpr GLbitfield kCoreBarrierBits =
        GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT |
        GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_COMMAND_BARRIER_BIT |
        GL_PIXEL_BUFFER_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT |
        GL_FRAMEBUFFER_BARRIER_BIT | GL_TRANSFORM_FEEDBACK_BARRIER_BIT |
        GL_ATOMIC_COUNTER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;
    constexpr GLbitfield kExtensionBarrierBits = GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT;

    barriers &= kCoreBarrierBits | kExtensionBarrierBits;

    // GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT specifies that a fence sync or glFinish must be used
    // after the barrier for the CPU to to see the shader writes.  Since host-visible buffer writes
    // always issue a barrier automatically for the sake of glMapBuffer() (see
    // comment on |mIsAnyHostVisibleBufferWritten|), there's nothing to do for
    // GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT.
    barriers &= ~GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT;

    // If no other barrier, early out.
    if (barriers == 0)
    {
        return angle::Result::Continue;
    }

    // glMemoryBarrier for barrier bit X_BARRIER_BIT implies:
    //
    // - An execution+memory barrier: shader writes are made visible to subsequent X accesses
    //
    // Additionally, SHADER_IMAGE_ACCESS_BARRIER_BIT and SHADER_STORAGE_BARRIER_BIT imply:
    //
    // - An execution+memory barrier: all accesses are finished before image/buffer writes
    //
    // For the first barrier, we can simplify the implementation by assuming that prior writes are
    // expected to be used right after this barrier, so we can close the render pass or flush the
    // outside render pass commands right away if they have had any writes.
    //
    // It's noteworthy that some barrier bits affect draw/dispatch calls only, while others affect
    // other commands.  For the latter, since storage buffer and images are not tracked in command
    // buffers, we can't rely on the command buffers being flushed in the usual way when recording
    // these commands (i.e. through |getOutsideRenderPassCommandBuffer()| and
    // |vk::CommandBufferAccess|).  Conservatively flushing command buffers with any storage output
    // simplifies this use case.  If this needs to be avoided in the future,
    // |getOutsideRenderPassCommandBuffer()| can be modified to flush the command buffers if they
    // have had any storage output.
    //
    // For the second barrier, we need to defer closing the render pass until there's a draw or
    // dispatch call that uses storage buffers or images that were previously used in the render
    // pass.  This allows the render pass to remain open in scenarios such as this:
    //
    // - Draw using resource X
    // - glMemoryBarrier
    // - Draw/dispatch with storage buffer/image Y
    //
    // To achieve this, a dirty bit is added that breaks the render pass if any storage
    // buffer/images are used in it.  Until the render pass breaks, changing the program or storage
    // buffer/image bindings should set this dirty bit again.

    if (mRenderPassCommands->hasShaderStorageOutput())
    {
        // Break the render pass if necessary as future non-draw commands can't know if they should.
        ANGLE_TRY(flushCommandsAndEndRenderPass(
            RenderPassClosureReason::StorageResourceUseThenGLMemoryBarrier));
    }
    else if (mOutsideRenderPassCommands->hasShaderStorageOutput())
    {
        // Otherwise flush the outside render pass commands if necessary.
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    constexpr GLbitfield kWriteAfterAccessBarriers =
        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;

    if ((barriers & kWriteAfterAccessBarriers) == 0)
    {
        return angle::Result::Continue;
    }

    // Defer flushing the command buffers until a draw/dispatch with storage buffer/image is
    // encountered.
    mGraphicsDirtyBits.set(DIRTY_BIT_MEMORY_BARRIER);
    mComputeDirtyBits.set(DIRTY_BIT_MEMORY_BARRIER);

    // Make sure memory barrier is issued for future usages of storage buffers and images even if
    // there's no binding change.
    mGraphicsDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);
    mComputeDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);

    // Mark the command buffers as affected by glMemoryBarrier, so future program and storage
    // buffer/image binding changes can set DIRTY_BIT_MEMORY_BARRIER again.
    mOutsideRenderPassCommands->setGLMemoryBarrierIssued();
    mRenderPassCommands->setGLMemoryBarrierIssued();

    return angle::Result::Continue;
}

angle::Result ContextVk::memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers)
{
    // Note: memoryBarrierByRegion is expected to affect only the fragment pipeline, but is
    // otherwise similar to memoryBarrier in function.
    //
    // TODO: Optimize memoryBarrierByRegion by issuing an in-subpass pipeline barrier instead of
    // breaking the render pass.  http://anglebug.com/5132
    return memoryBarrier(context, barriers);
}

void ContextVk::framebufferFetchBarrier()
{
    mGraphicsDirtyBits.set(DIRTY_BIT_FRAMEBUFFER_FETCH_BARRIER);
}

void ContextVk::blendBarrier()
{
    if (getFeatures().emulateAdvancedBlendEquations.enabled)
    {
        // When emulated, advanced blend is implemented through framebuffer fetch.
        framebufferFetchBarrier();
    }
    else
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_BLEND_BARRIER);
    }
}

angle::Result ContextVk::acquireTextures(const gl::Context *context,
                                         const gl::TextureBarrierVector &textureBarriers)
{
    for (const gl::TextureAndLayout &textureBarrier : textureBarriers)
    {
        TextureVk *textureVk   = vk::GetImpl(textureBarrier.texture);
        vk::ImageHelper &image = textureVk->getImage();
        vk::ImageLayout layout = vk::GetImageLayoutFromGLImageLayout(textureBarrier.layout);
        // Image should not be accessed while unowned. Emulated formats may have staged updates
        // to clear the image after initialization.
        ASSERT(!image.hasStagedUpdatesInAllocatedLevels() || image.hasEmulatedImageChannels());
        image.setCurrentImageLayout(layout);
    }
    return angle::Result::Continue;
}

angle::Result ContextVk::releaseTextures(const gl::Context *context,
                                         gl::TextureBarrierVector *textureBarriers)
{
    for (gl::TextureAndLayout &textureBarrier : *textureBarriers)
    {

        TextureVk *textureVk = vk::GetImpl(textureBarrier.texture);

        ANGLE_TRY(textureVk->ensureImageInitialized(this, ImageMipLevels::EnabledLevels));

        vk::ImageHelper &image = textureVk->getImage();
        ANGLE_TRY(onImageReleaseToExternal(image));

        textureBarrier.layout =
            vk::ConvertImageLayoutToGLImageLayout(image.getCurrentImageLayout());
    }

    ANGLE_TRY(flushImpl(nullptr, RenderPassClosureReason::ImageUseThenReleaseToExternal));
    return mRenderer->ensureNoPendingWork(this);
}

vk::DynamicQueryPool *ContextVk::getQueryPool(gl::QueryType queryType)
{
    ASSERT(queryType == gl::QueryType::AnySamples ||
           queryType == gl::QueryType::AnySamplesConservative ||
           queryType == gl::QueryType::PrimitivesGenerated ||
           queryType == gl::QueryType::TransformFeedbackPrimitivesWritten ||
           queryType == gl::QueryType::Timestamp || queryType == gl::QueryType::TimeElapsed);

    // For PrimitivesGenerated queries:
    //
    // - If VK_EXT_primitives_generated_query is supported, use that.
    // - Otherwise, if pipelineStatisticsQuery is supported, use that,
    // - Otherwise, use the same pool as TransformFeedbackPrimitivesWritten and share the query as
    //   the Vulkan transform feedback query produces both results.  This option is non-conformant
    //   as the primitives generated query will not be functional without transform feedback.
    //
    if (queryType == gl::QueryType::PrimitivesGenerated &&
        !getFeatures().supportsPrimitivesGeneratedQuery.enabled &&
        !getFeatures().supportsPipelineStatisticsQuery.enabled)
    {
        queryType = gl::QueryType::TransformFeedbackPrimitivesWritten;
    }

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

void ContextVk::pauseTransformFeedbackIfActiveUnpaused()
{
    if (mRenderPassCommands->isTransformFeedbackActiveUnpaused())
    {
        ASSERT(getFeatures().supportsTransformFeedbackExtension.enabled);
        mRenderPassCommands->pauseTransformFeedback();

        // Note that this function is called when render pass break is imminent
        // (flushCommandsAndEndRenderPass(), or UtilsVk::clearFramebuffer which will close the
        // render pass after the clear).  This dirty bit allows transform feedback to resume
        // automatically on next render pass.
        mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME);
    }
}

angle::Result ContextVk::handleDirtyGraphicsDriverUniforms(DirtyBits::Iterator *dirtyBitsIterator,
                                                           DirtyBits dirtyBitMask)
{
    FramebufferVk *drawFramebufferVk = getDrawFramebuffer();

    static_assert(gl::IMPLEMENTATION_MAX_2D_TEXTURE_SIZE <= 0xFFFF,
                  "Not enough bits for render area");
    uint16_t renderAreaWidth, renderAreaHeight;
    SetBitField(renderAreaWidth, drawFramebufferVk->getState().getDimensions().width);
    SetBitField(renderAreaHeight, drawFramebufferVk->getState().getDimensions().height);
    const uint32_t renderArea = renderAreaHeight << 16 | renderAreaWidth;

    bool flipX = false;
    bool flipY = false;
    // Y-axis flipping only comes into play with the default framebuffer (i.e. a swapchain
    // image). For 0-degree rotation, an FBO or pbuffer could be the draw framebuffer, and so we
    // must check whether flipY should be positive or negative.  All other rotations, will be to
    // the default framebuffer, and so the value of isViewportFlipEnabledForDrawFBO() is assumed
    // true; the appropriate flipY value is chosen such that gl_FragCoord is positioned at the
    // lower-left corner of the window.
    switch (mCurrentRotationDrawFramebuffer)
    {
        case SurfaceRotation::Identity:
            flipY = isViewportFlipEnabledForDrawFBO();
            break;
        case SurfaceRotation::Rotated90Degrees:
            ASSERT(isViewportFlipEnabledForDrawFBO());
            break;
        case SurfaceRotation::Rotated180Degrees:
            ASSERT(isViewportFlipEnabledForDrawFBO());
            flipX = true;
            break;
        case SurfaceRotation::Rotated270Degrees:
            ASSERT(isViewportFlipEnabledForDrawFBO());
            flipX = true;
            flipY = true;
            break;
        default:
            UNREACHABLE();
            break;
    }

    const bool invertViewport =
        isViewportFlipEnabledForDrawFBO() && getFeatures().supportsNegativeViewport.enabled;

    // Create the extended driver uniform, and populate the extended data fields if necessary.
    GraphicsDriverUniformsExtended driverUniformsExt = {};
    if (shouldUseGraphicsDriverUniformsExtended(this))
    {
        if (mState.isTransformFeedbackActiveUnpaused())
        {
            TransformFeedbackVk *transformFeedbackVk =
                vk::GetImpl(mState.getCurrentTransformFeedback());
            transformFeedbackVk->getBufferOffsets(this, mXfbBaseVertex,
                                                  driverUniformsExt.xfbBufferOffsets.data(),
                                                  driverUniformsExt.xfbBufferOffsets.size());
        }
        driverUniformsExt.xfbVerticesPerInstance = static_cast<int32_t>(mXfbVertexCountPerInstance);
    }

    // Create the driver uniform object that will be used as push constant argument.
    GraphicsDriverUniforms *driverUniforms = &driverUniformsExt.common;
    uint32_t driverUniformSize             = getDriverUniformSize(PipelineType::Graphics);

    const float depthRangeNear = mState.getNearPlane();
    const float depthRangeFar  = mState.getFarPlane();
    const uint32_t numSamples  = drawFramebufferVk->getSamples();

    uint32_t advancedBlendEquation = 0;
    if (getFeatures().emulateAdvancedBlendEquations.enabled)
    {
        // Pass the advanced blend equation to shader as-is.  If the equation is not one of the
        // advanced ones, 0 is expected.
        const gl::BlendStateExt &blendStateExt = mState.getBlendStateExt();
        if (blendStateExt.getUsesAdvancedBlendEquationMask().test(0))
        {
            advancedBlendEquation = static_cast<uint32_t>(gl::FromGLenum<gl::BlendEquationType>(
                getState().getBlendStateExt().getEquationColorIndexed(0)));
        }
    }

    const uint32_t swapXY               = IsRotatedAspectRatio(mCurrentRotationDrawFramebuffer);
    const uint32_t enabledClipDistances = mState.getEnabledClipDistances().bits();
    const uint32_t transformDepth =
        getFeatures().supportsDepthClipControl.enabled ? 0 : !mState.isClipControlDepthZeroToOne();

    static_assert(angle::BitMask<uint32_t>(gl::IMPLEMENTATION_MAX_CLIP_DISTANCES) <=
                      sh::vk::kDriverUniformsMiscEnabledClipPlanesMask,
                  "Not enough bits for enabled clip planes");

    ASSERT((swapXY & ~sh::vk::kDriverUniformsMiscSwapXYMask) == 0);
    ASSERT((advancedBlendEquation & ~sh::vk::kDriverUniformsMiscAdvancedBlendEquationMask) == 0);
    ASSERT((numSamples & ~sh::vk::kDriverUniformsMiscSampleCountMask) == 0);
    ASSERT((enabledClipDistances & ~sh::vk::kDriverUniformsMiscEnabledClipPlanesMask) == 0);
    ASSERT((transformDepth & ~sh::vk::kDriverUniformsMiscTransformDepthMask) == 0);

    const uint32_t misc =
        swapXY | advancedBlendEquation << sh::vk::kDriverUniformsMiscAdvancedBlendEquationOffset |
        numSamples << sh::vk::kDriverUniformsMiscSampleCountOffset |
        enabledClipDistances << sh::vk::kDriverUniformsMiscEnabledClipPlanesOffset |
        transformDepth << sh::vk::kDriverUniformsMiscTransformDepthOffset;

    // Copy and flush to the device.
    *driverUniforms = {
        {},
        {depthRangeNear, depthRangeFar},
        renderArea,
        MakeFlipUniform(flipX, flipY, invertViewport),
        mGraphicsPipelineDesc->getEmulatedDitherControl(),
        misc,
    };

    if (mState.hasValidAtomicCounterBuffer())
    {
        writeAtomicCounterBufferDriverUniformOffsets(driverUniforms->acbBufferOffsets.data(),
                                                     driverUniforms->acbBufferOffsets.size());
    }

    // Update push constant driver uniforms.
    ProgramExecutableVk *executableVk = getExecutable();
    mRenderPassCommands->getCommandBuffer().pushConstants(
        executableVk->getPipelineLayout(), getRenderer()->getSupportedVulkanShaderStageMask(), 0,
        driverUniformSize, driverUniforms);

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyComputeDriverUniforms()
{
    // Create the driver uniform object that will be used as push constant argument.
    ComputeDriverUniforms driverUniforms = {};
    uint32_t driverUniformSize           = getDriverUniformSize(PipelineType::Compute);

    if (mState.hasValidAtomicCounterBuffer())
    {
        writeAtomicCounterBufferDriverUniformOffsets(driverUniforms.acbBufferOffsets.data(),
                                                     driverUniforms.acbBufferOffsets.size());
    }

    // Update push constant driver uniforms.
    ProgramExecutableVk *executableVk = getExecutable();
    mOutsideRenderPassCommands->getCommandBuffer().pushConstants(
        executableVk->getPipelineLayout(), getRenderer()->getSupportedVulkanShaderStageMask(), 0,
        driverUniformSize, &driverUniforms);

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

angle::Result ContextVk::updateActiveTextures(const gl::Context *context, gl::Command command)
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable);
    ProgramExecutableVk *executableVk = getExecutable();

    const gl::ActiveTexturesCache &textures        = mState.getActiveTexturesCache();
    const gl::ActiveTextureMask &activeTextures    = executable->getActiveSamplersMask();
    const gl::ActiveTextureTypeArray &textureTypes = executable->getActiveSamplerTypes();

    mActiveTextures.fill(nullptr);

    bool recreatePipelineLayout                       = false;
    ImmutableSamplerIndexMap immutableSamplerIndexMap = {};
    for (size_t textureUnit : activeTextures)
    {
        gl::Texture *texture        = textures[textureUnit];
        gl::TextureType textureType = textureTypes[textureUnit];
        ASSERT(textureType != gl::TextureType::InvalidEnum);

        const bool isIncompleteTexture = texture == nullptr;

        // Null textures represent incomplete textures.
        if (isIncompleteTexture)
        {
            ANGLE_TRY(getIncompleteTexture(
                context, textureType, executable->getSamplerFormatForTextureUnitIndex(textureUnit),
                &texture));
        }

        TextureVk *textureVk = vk::GetImpl(texture);
        ASSERT(textureVk != nullptr);

        mActiveTextures[textureUnit] = textureVk;

        if (textureType == gl::TextureType::Buffer)
        {
            continue;
        }

        if (!isIncompleteTexture && texture->isDepthOrStencil() &&
            shouldSwitchToReadOnlyDepthFeedbackLoopMode(texture, command))
        {
            FramebufferVk *drawFramebufferVk = getDrawFramebuffer();
            // Special handling for deferred clears.
            ANGLE_TRY(drawFramebufferVk->flushDeferredClears(this));

            if (hasStartedRenderPass())
            {
                if (!textureVk->getImage().hasRenderPassUsageFlag(
                        vk::RenderPassUsage::ReadOnlyAttachment))
                {
                    // To enter depth feedback loop, we must flush and start a new renderpass.
                    // Otherwise it will stick with writable layout and cause validation error.
                    ANGLE_TRY(flushCommandsAndEndRenderPass(
                        RenderPassClosureReason::DepthStencilUseInFeedbackLoop));
                }
                else
                {
                    drawFramebufferVk->updateRenderPassReadOnlyDepthMode(this, mRenderPassCommands);
                }
            }

            drawFramebufferVk->setReadOnlyDepthFeedbackLoopMode(true);
        }

        gl::Sampler *sampler = mState.getSampler(static_cast<uint32_t>(textureUnit));
        const gl::SamplerState &samplerState =
            sampler ? sampler->getSamplerState() : texture->getSamplerState();

        // GL_EXT_texture_sRGB_decode
        //   The new parameter, TEXTURE_SRGB_DECODE_EXT controls whether the
        //   decoding happens at sample time. It only applies to textures with an
        //   internal format that is sRGB and is ignored for all other textures.
        const vk::ImageHelper &image = textureVk->getImage();
        ASSERT(image.valid());
        if (image.getActualFormat().isSRGB && samplerState.getSRGBDecode() == GL_SKIP_DECODE_EXT)
        {
            // Make sure we use the MUTABLE bit for the storage. Because the "skip decode" is a
            // Sampler state we might not have caught this setting in TextureVk::syncState.
            ANGLE_TRY(textureVk->ensureMutable(this));
        }

        if (image.hasEmulatedImageFormat())
        {
            ANGLE_VK_PERF_WARNING(
                this, GL_DEBUG_SEVERITY_LOW,
                "The Vulkan driver does not support texture format 0x%04X, emulating with 0x%04X",
                image.getIntendedFormat().glInternalFormat,
                image.getActualFormat().glInternalFormat);
        }

        if (image.hasImmutableSampler())
        {
            immutableSamplerIndexMap[image.getYcbcrConversionDesc()] =
                static_cast<uint32_t>(textureUnit);
        }

        if (textureVk->getAndResetImmutableSamplerDirtyState())
        {
            recreatePipelineLayout = true;
        }
    }

    if (!executableVk->areImmutableSamplersCompatible(immutableSamplerIndexMap))
    {
        recreatePipelineLayout = true;
    }

    // Recreate the pipeline layout, if necessary.
    if (recreatePipelineLayout)
    {
        ANGLE_TRY(executableVk->createPipelineLayout(this, *executable, &mActiveTextures));

        // The default uniforms descriptor set was reset during createPipelineLayout(), so mark them
        // dirty to get everything reallocated/rebound before the next draw.
        if (executable->hasDefaultUniforms())
        {
            executableVk->setAllDefaultUniformsDirty(*executable);
        }
    }

    return angle::Result::Continue;
}

template <typename CommandBufferHelperT>
angle::Result ContextVk::updateActiveImages(CommandBufferHelperT *commandBufferHelper)
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

        TextureVk *textureVk          = vk::GetImpl(texture);
        mActiveImages[imageUnitIndex] = textureVk;

        // The image should be flushed and ready to use at this point. There may still be
        // lingering staged updates in its staging buffer for unused texture mip levels or
        // layers. Therefore we can't verify it has no staged updates right here.
        gl::ShaderBitSet shaderStages = activeImageShaderBits[imageUnitIndex];
        ASSERT(shaderStages.any());

        // Special handling of texture buffers.  They have a buffer attached instead of an image.
        if (texture->getType() == gl::TextureType::Buffer)
        {
            BufferVk *bufferVk = vk::GetImpl(textureVk->getBuffer().get());

            OnImageBufferWrite(this, bufferVk, shaderStages, commandBufferHelper);

            textureVk->retainBufferViews(commandBufferHelper);
            continue;
        }

        vk::ImageHelper *image = &textureVk->getImage();

        if (alreadyProcessed.find(image) != alreadyProcessed.end())
        {
            continue;
        }
        alreadyProcessed.insert(image);

        gl::LevelIndex level;
        uint32_t layerStart               = 0;
        uint32_t layerCount               = 0;
        const vk::ImageLayout imageLayout = GetImageWriteLayoutAndSubresource(
            imageUnit, *image, shaderStages, &level, &layerStart, &layerCount);

        commandBufferHelper->imageWrite(this, level, layerStart, layerCount,
                                        image->getAspectFlags(), imageLayout, image);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::flushImpl(const vk::Semaphore *signalSemaphore,
                                   RenderPassClosureReason renderPassClosureReason)
{
    Serial unusedSerial;
    return flushAndGetSerial(signalSemaphore, &unusedSerial, renderPassClosureReason);
}

angle::Result ContextVk::flushAndGetSerial(const vk::Semaphore *signalSemaphore,
                                           Serial *submitSerialOut,
                                           RenderPassClosureReason renderPassClosureReason)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::flushImpl");

    // We must set this to false before calling flushCommandsAndEndRenderPass to prevent it from
    // calling back to flushImpl.
    mHasDeferredFlush = false;

    // Avoid calling vkQueueSubmit() twice, since submitFrame() below will do that.
    ANGLE_TRY(flushCommandsAndEndRenderPassWithoutQueueSubmit(renderPassClosureReason));

    if (mIsAnyHostVisibleBufferWritten)
    {
        // Make sure all writes to host-visible buffers are flushed.  We have no way of knowing
        // whether any buffer will be mapped for readback in the future, and we can't afford to
        // flush and wait on a one-pipeline-barrier command buffer on every map().
        VkMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT;
        memoryBarrier.dstAccessMask   = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;

        const VkPipelineStageFlags supportedShaderStages =
            (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
             VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
             VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
             VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) &
            mRenderer->getSupportedVulkanPipelineStageMask();
        const VkPipelineStageFlags bufferWriteStages =
            VK_PIPELINE_STAGE_TRANSFER_BIT | supportedShaderStages |
            (getFeatures().supportsTransformFeedbackExtension.enabled
                 ? VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT
                 : 0);

        mOutsideRenderPassCommands->getCommandBuffer().memoryBarrier(
            bufferWriteStages, VK_PIPELINE_STAGE_HOST_BIT, &memoryBarrier);
        mIsAnyHostVisibleBufferWritten = false;
    }

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("Primary", mPrimaryBufferEventCounter);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_END, eventName));
    }
    ANGLE_TRY(flushOutsideRenderPassCommands());

    // We must add the per context dynamic buffers into resourceUseList before submission so that
    // they get retained properly until GPU completes. We do not add current buffer into
    // resourceUseList since they never get reused or freed until context gets destroyed, at which
    // time we always wait for GPU to finish before destroying the dynamic buffers.

    mDefaultUniformStorage.releaseInFlightBuffersToResourceUseList(this);

    if (mHasInFlightStreamedVertexBuffers.any())
    {
        for (size_t attribIndex : mHasInFlightStreamedVertexBuffers)
        {
            mStreamedVertexBuffers[attribIndex].releaseInFlightBuffersToResourceUseList(this);
        }
        mHasInFlightStreamedVertexBuffers.reset();
    }

    ANGLE_TRY(submitFrame(signalSemaphore, Submit::AllCommands, submitSerialOut));

    ASSERT(mWaitSemaphores.empty());
    ASSERT(mWaitSemaphoreStageMasks.empty());

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("Primary", ++mPrimaryBufferEventCounter);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_BEGIN, eventName));
    }

    // Try to detect frame boundary for both on screen and offscreen usage by detecting
    // fush/finish/swap.
    if ((renderPassClosureReason == RenderPassClosureReason::GLFlush ||
         renderPassClosureReason == RenderPassClosureReason::GLFinish ||
         renderPassClosureReason == RenderPassClosureReason::EGLSwapBuffers) &&
        mShareGroupVk->isDueForBufferPoolPrune(mRenderer))
    {
        mShareGroupVk->pruneDefaultBufferPools(mRenderer);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::finishImpl(RenderPassClosureReason renderPassClosureReason)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::finishImpl");

    ANGLE_TRY(flushImpl(nullptr, renderPassClosureReason));
    ANGLE_TRY(mRenderer->finish(this, hasProtectedContent()));

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

bool ContextVk::isSerialInUse(Serial serial) const
{
    return serial > getLastCompletedQueueSerial();
}

angle::Result ContextVk::checkCompletedCommands()
{
    return mRenderer->checkCompletedCommands(this);
}

angle::Result ContextVk::finishToSerial(Serial serial)
{
    return mRenderer->finishToSerial(this, serial);
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
    ANGLE_TRY(timestampQueryPool.get().allocateQuery(this, &timestampQuery, 1));

    vk::ResourceUseList scratchResourceUseList;

    // Record the command buffer
    vk::DeviceScoped<vk::PrimaryCommandBuffer> commandBatch(device);
    vk::PrimaryCommandBuffer &commandBuffer = commandBatch.get();

    ANGLE_TRY(mRenderer->getCommandBufferOneOff(this, hasProtectedContent(), &commandBuffer));

    timestampQuery.writeTimestampToPrimary(this, &commandBuffer);
    timestampQuery.retain(&scratchResourceUseList);
    ANGLE_VK_TRY(this, commandBuffer.end());

    // Create fence for the submission
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = 0;

    vk::DeviceScoped<vk::Fence> fence(device);
    ANGLE_VK_TRY(this, fence.get().init(device, fenceInfo));

    Serial throwAwaySerial;
    ANGLE_TRY(mRenderer->queueSubmitOneOff(this, std::move(commandBuffer), hasProtectedContent(),
                                           mContextPriority, nullptr, 0, &fence.get(),
                                           vk::SubmitPolicy::EnsureSubmitted, &throwAwaySerial));

    // Wait for the submission to finish.  Given no semaphores, there is hope that it would execute
    // in parallel with what's already running on the GPU.
    ANGLE_VK_TRY(this, fence.get().wait(device, mRenderer->getMaxFenceWaitTimeNs()));
    scratchResourceUseList.releaseResourceUsesAndUpdateSerials(throwAwaySerial);

    // Get the query results
    vk::QueryResult result(1);
    ANGLE_TRY(timestampQuery.getUint64Result(this, &result));
    *timestampOut = result.getResult(vk::QueryResult::kDefaultResultIndex);
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
        mGraphicsDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
    }
}

bool ContextVk::shouldEmulateSeamfulCubeMapSampling() const
{
    // Only allow seamful cube map sampling in non-webgl ES2.
    if (mState.getClientMajorVersion() != 2 || mState.isWebGL())
    {
        return false;
    }

    return true;
}

angle::Result ContextVk::onBufferReleaseToExternal(const vk::BufferHelper &buffer)
{
    if (mRenderPassCommands->usesBuffer(buffer))
    {
        return flushCommandsAndEndRenderPass(
            RenderPassClosureReason::BufferUseThenReleaseToExternal);
    }
    return angle::Result::Continue;
}

angle::Result ContextVk::onImageReleaseToExternal(const vk::ImageHelper &image)
{
    if (IsRenderPassStartedAndUsesImage(*mRenderPassCommands, image))
    {
        return flushCommandsAndEndRenderPass(
            RenderPassClosureReason::ImageUseThenReleaseToExternal);
    }
    return angle::Result::Continue;
}

angle::Result ContextVk::beginNewRenderPass(
    const vk::Framebuffer &framebuffer,
    const gl::Rectangle &renderArea,
    const vk::RenderPassDesc &renderPassDesc,
    const vk::AttachmentOpsArray &renderPassAttachmentOps,
    const vk::PackedAttachmentCount colorAttachmentCount,
    const vk::PackedAttachmentIndex depthStencilAttachmentIndex,
    const vk::PackedClearValuesArray &clearValues,
    vk::RenderPassCommandBuffer **commandBufferOut)
{
    // Next end any currently outstanding render pass.  The render pass is normally closed before
    // reaching here for various reasons, except typically when UtilsVk needs to start one.
    ANGLE_TRY(flushCommandsAndEndRenderPass(RenderPassClosureReason::NewRenderPass));

    mPerfCounters.renderPasses++;
    return mRenderPassCommands->beginRenderPass(
        this, framebuffer, renderArea, renderPassDesc, renderPassAttachmentOps,
        colorAttachmentCount, depthStencilAttachmentIndex, clearValues, commandBufferOut);
}

angle::Result ContextVk::startRenderPass(gl::Rectangle renderArea,
                                         vk::RenderPassCommandBuffer **commandBufferOut,
                                         bool *renderPassDescChangedOut)
{
    FramebufferVk *drawFramebufferVk = getDrawFramebuffer();
    ASSERT(drawFramebufferVk == vk::GetImpl(mState.getDrawFramebuffer()));

    ANGLE_TRY(drawFramebufferVk->startNewRenderPass(this, renderArea, &mRenderPassCommandBuffer,
                                                    renderPassDescChangedOut));

    // Make sure the render pass is not restarted if it is started by UtilsVk (as opposed to
    // setupDraw(), which clears this bit automatically).
    mGraphicsDirtyBits.reset(DIRTY_BIT_RENDER_PASS);

    ANGLE_TRY(resumeRenderPassQueriesIfActive());

    if (commandBufferOut)
    {
        *commandBufferOut = mRenderPassCommandBuffer;
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::startNextSubpass()
{
    ASSERT(hasStartedRenderPass());

    // The graphics pipelines are bound to a subpass, so update the subpass as well.
    mGraphicsPipelineDesc->nextSubpass(&mGraphicsPipelineTransition);

    return mRenderPassCommands->nextSubpass(this, &mRenderPassCommandBuffer);
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

uint32_t ContextVk::getCurrentViewCount() const
{
    FramebufferVk *drawFBO = vk::GetImpl(mState.getDrawFramebuffer());
    return drawFBO->getRenderPassDesc().viewCount();
}

angle::Result ContextVk::flushCommandsAndEndRenderPassImpl(QueueSubmitType queueSubmit,
                                                           RenderPassClosureReason reason)
{
    // Ensure we flush the RenderPass *after* the prior commands.
    ANGLE_TRY(flushOutsideRenderPassCommands());
    ASSERT(mOutsideRenderPassCommands->empty());

    if (!mRenderPassCommands->started())
    {
        onRenderPassFinished(RenderPassClosureReason::AlreadySpecifiedElsewhere);
        return angle::Result::Continue;
    }

    // Set dirty bits if render pass was open (and thus will be closed).
    mGraphicsDirtyBits |= mNewGraphicsCommandBufferDirtyBits;
    // Restart at subpass 0.
    mGraphicsPipelineDesc->resetSubpass(&mGraphicsPipelineTransition);

    mCurrentTransformFeedbackBuffers.clear();

    onRenderPassFinished(reason);

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("RP", mPerfCounters.renderPasses);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_BEGIN, eventName));
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    addOverlayUsedBuffersCount(mRenderPassCommands);

    pauseTransformFeedbackIfActiveUnpaused();

    ANGLE_TRY(mRenderPassCommands->endRenderPass(this));

    if (vk::CommandBufferHelperCommon::kEnableCommandStreamDiagnostics)
    {
        mRenderPassCommands->addCommandDiagnostics(this);
    }

    vk::RenderPass *renderPass = nullptr;
    ANGLE_TRY(getRenderPassWithOps(mRenderPassCommands->getRenderPassDesc(),
                                   mRenderPassCommands->getAttachmentOps(), &renderPass));

    flushDescriptorSetUpdates();

    getShareGroup()->acquireResourceUseList(
        std::move(mRenderPassCommands->releaseResourceUseList()));

    ANGLE_TRY(mRenderer->flushRenderPassCommands(this, hasProtectedContent(), *renderPass,
                                                 &mRenderPassCommands));

    if (mGpuEventsEnabled)
    {
        EventName eventName = GetTraceEventName("RP", mPerfCounters.renderPasses);
        ANGLE_TRY(traceGpuEvent(&mOutsideRenderPassCommands->getCommandBuffer(),
                                TRACE_EVENT_PHASE_END, eventName));
        ANGLE_TRY(flushOutsideRenderPassCommands());
    }

    mHasAnyCommandsPendingSubmission = true;

    if (mHasDeferredFlush && queueSubmit == QueueSubmitType::PerformQueueSubmit)
    {
        // If we have deferred glFlush call in the middle of renderpass, flush them now.
        ANGLE_TRY(flushImpl(nullptr, RenderPassClosureReason::AlreadySpecifiedElsewhere));
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::flushCommandsAndEndRenderPass(RenderPassClosureReason reason)
{
    return flushCommandsAndEndRenderPassImpl(QueueSubmitType::PerformQueueSubmit, reason);
}

angle::Result ContextVk::flushCommandsAndEndRenderPassWithoutQueueSubmit(
    RenderPassClosureReason reason)
{
    return flushCommandsAndEndRenderPassImpl(QueueSubmitType::SkipQueueSubmit, reason);
}

angle::Result ContextVk::flushDirtyGraphicsRenderPass(DirtyBits::Iterator *dirtyBitsIterator,
                                                      DirtyBits dirtyBitMask,
                                                      RenderPassClosureReason reason)
{
    ASSERT(mRenderPassCommands->started());

    ANGLE_TRY(flushCommandsAndEndRenderPassImpl(QueueSubmitType::PerformQueueSubmit, reason));

    // Set dirty bits that need processing on new render pass on the dirty bits iterator that's
    // being processed right now.
    dirtyBitsIterator->setLaterBits(mNewGraphicsCommandBufferDirtyBits & dirtyBitMask);

    // Additionally, make sure any dirty bits not included in the mask are left for future
    // processing.  Note that |dirtyBitMask| is removed from |mNewGraphicsCommandBufferDirtyBits|
    // after dirty bits are iterated, so there's no need to mask them out.
    mGraphicsDirtyBits |= mNewGraphicsCommandBufferDirtyBits;

    // Restart at subpass 0.
    mGraphicsPipelineDesc->resetSubpass(&mGraphicsPipelineTransition);

    return angle::Result::Continue;
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

angle::Result ContextVk::onSyncObjectInit(vk::SyncHelper *syncHelper, bool isEGLSyncObject)
{
    const bool isRenderPassStarted = mRenderPassCommands->started();

    if (isRenderPassStarted)
    {
        mRenderPassCommands->retainResource(syncHelper);
    }
    else
    {
        mOutsideRenderPassCommands->retainResource(syncHelper);
    }

    // Submit the commands:
    //
    // - This breaks the current render pass to ensure the proper ordering of the sync object in the
    //   commands,
    // - The sync object has a valid serial when it's waited on later,
    // - After waiting on the sync object, every resource that's used so far (and is being synced)
    //   will also be aware that it's finished (based on the serial) and won't incur a further wait
    //   (for example when a buffer is mapped).
    //
    // The submission is done immediately for EGL sync objects, and when no render pass is open.  If
    // a render pass is open, the submission is deferred.  This is done to be able to optimize
    // scenarios such as sync object init followed by eglSwapBuffers() (that would otherwise incur
    // another submission, as well as not being able to optimize the render-to-swapchain render
    // pass).
    if (isEGLSyncObject || !isRenderPassStarted)
    {
        return flushImpl(nullptr, RenderPassClosureReason::SyncObjectInit);
    }

    onRenderPassFinished(RenderPassClosureReason::SyncObjectInit);

    // Mark the context as having a deffered flush.  This is later used to close the render pass and
    // cause a submission in this context if another context wants to wait on the fence while the
    // original context never issued a submission naturally.  Note that this also takes care of
    // contexts that think they issued a submission (through glFlush) but that the submission got
    // deferred (due to the deferFlushUntilEndRenderPass feature).
    mHasDeferredFlush = true;

    return angle::Result::Continue;
}

angle::Result ContextVk::flushCommandsAndEndRenderPassIfDeferredSyncInit(
    RenderPassClosureReason reason)
{
    if (!mHasDeferredFlush)
    {
        return angle::Result::Continue;
    }

    return flushCommandsAndEndRenderPassImpl(QueueSubmitType::PerformQueueSubmit, reason);
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

uint32_t ContextVk::getDriverUniformSize(PipelineType pipelineType) const
{
    if (pipelineType == PipelineType::Compute)
    {
        return sizeof(ComputeDriverUniforms);
    }

    ASSERT(pipelineType == PipelineType::Graphics);
    if (shouldUseGraphicsDriverUniformsExtended(this))
    {
        return sizeof(GraphicsDriverUniformsExtended);
    }
    else
    {
        return sizeof(GraphicsDriverUniforms);
    }
}

angle::Result ContextVk::submitOutsideRenderPassCommandsImpl()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::submitOutsideRenderPassCommandsImpl");
    ANGLE_TRY(flushOutsideRenderPassCommands());
    Serial unusedSerial;
    ANGLE_TRY(submitFrameOutsideCommandBufferOnly(&unusedSerial));
    return angle::Result::Continue;
}

angle::Result ContextVk::flushOutsideRenderPassCommands()
{
    if (mOutsideRenderPassCommands->empty())
    {
        return angle::Result::Continue;
    }

    addOverlayUsedBuffersCount(mOutsideRenderPassCommands);

    if (vk::CommandBufferHelperCommon::kEnableCommandStreamDiagnostics)
    {
        mOutsideRenderPassCommands->addCommandDiagnostics(this);
    }

    flushDescriptorSetUpdates();

    getShareGroup()->acquireResourceUseList(
        std::move(mOutsideRenderPassCommands->releaseResourceUseList()));

    ANGLE_TRY(mRenderer->flushOutsideRPCommands(this, hasProtectedContent(),
                                                &mOutsideRenderPassCommands));

    // Make sure appropriate dirty bits are set, in case another thread makes a submission before
    // the next dispatch call.
    mComputeDirtyBits |= mNewComputeCommandBufferDirtyBits;

    mHasAnyCommandsPendingSubmission = true;

    mPerfCounters.flushedOutsideRenderPassCommandBuffers++;
    return angle::Result::Continue;
}

angle::Result ContextVk::beginRenderPassQuery(QueryVk *queryVk)
{
    // Emit debug-util markers before calling the query command.
    ANGLE_TRY(handleGraphicsEventLog(rx::GraphicsEventCmdBuf::InRenderPassCmdBufQueryCmd));

    // To avoid complexity, we always start and end these queries inside the render pass.  If the
    // render pass has not yet started, the query is deferred until it does.
    if (mRenderPassCommandBuffer)
    {
        ANGLE_TRY(queryVk->getQueryHelper()->beginRenderPassQuery(this));
    }

    // Update rasterizer discard emulation with primitives generated query if necessary.
    if (queryVk->getType() == gl::QueryType::PrimitivesGenerated)
    {
        updateRasterizerDiscardEnabled(true);
    }

    gl::QueryType type = queryVk->getType();

    ASSERT(mActiveRenderPassQueries[type] == nullptr);
    mActiveRenderPassQueries[type] = queryVk;

    return angle::Result::Continue;
}

angle::Result ContextVk::endRenderPassQuery(QueryVk *queryVk)
{
    gl::QueryType type = queryVk->getType();

    // Emit debug-util markers before calling the query command.
    ANGLE_TRY(handleGraphicsEventLog(rx::GraphicsEventCmdBuf::InRenderPassCmdBufQueryCmd));

    // End the query inside the render pass.  In some situations, the query may not have actually
    // been issued, so there is nothing to do there.  That is the case for transform feedback
    // queries which are deferred until a draw call with transform feedback active is issued, which
    // may have never happened.
    ASSERT(mRenderPassCommandBuffer == nullptr ||
           type == gl::QueryType::TransformFeedbackPrimitivesWritten || queryVk->hasQueryBegun());
    if (mRenderPassCommandBuffer && queryVk->hasQueryBegun())
    {
        queryVk->getQueryHelper()->endRenderPassQuery(this);
    }

    // Update rasterizer discard emulation with primitives generated query if necessary.
    if (type == gl::QueryType::PrimitivesGenerated)
    {
        updateRasterizerDiscardEnabled(false);
    }

    ASSERT(mActiveRenderPassQueries[type] == queryVk);
    mActiveRenderPassQueries[type] = nullptr;

    return angle::Result::Continue;
}

void ContextVk::pauseRenderPassQueriesIfActive()
{
    if (mRenderPassCommandBuffer == nullptr)
    {
        return;
    }

    for (QueryVk *activeQuery : mActiveRenderPassQueries)
    {
        if (activeQuery)
        {
            activeQuery->onRenderPassEnd(this);

            // No need to update rasterizer discard emulation with primitives generated query.  The
            // state will be updated when the next render pass starts.
        }
    }
}

angle::Result ContextVk::resumeRenderPassQueriesIfActive()
{
    ASSERT(mRenderPassCommandBuffer);

    // Note: these queries should be processed in order.  See comment in QueryVk::onRenderPassStart.
    for (QueryVk *activeQuery : mActiveRenderPassQueries)
    {
        if (activeQuery)
        {
            // Transform feedback queries are handled separately.
            if (activeQuery->getType() == gl::QueryType::TransformFeedbackPrimitivesWritten)
            {
                continue;
            }

            ANGLE_TRY(activeQuery->onRenderPassStart(this));

            // Update rasterizer discard emulation with primitives generated query if necessary.
            if (activeQuery->getType() == gl::QueryType::PrimitivesGenerated)
            {
                updateRasterizerDiscardEnabled(true);
            }
        }
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::resumeXfbRenderPassQueriesIfActive()
{
    ASSERT(mRenderPassCommandBuffer);

    // All other queries are handled separately.
    QueryVk *xfbQuery = mActiveRenderPassQueries[gl::QueryType::TransformFeedbackPrimitivesWritten];
    if (xfbQuery && mState.isTransformFeedbackActiveUnpaused())
    {
        ANGLE_TRY(xfbQuery->onRenderPassStart(this));
    }

    return angle::Result::Continue;
}

bool ContextVk::doesPrimitivesGeneratedQuerySupportRasterizerDiscard() const
{
    // If primitives generated is implemented with VK_EXT_primitives_generated_query, check the
    // corresponding feature bit.
    if (getFeatures().supportsPrimitivesGeneratedQuery.enabled)
    {
        return mRenderer->getPhysicalDevicePrimitivesGeneratedQueryFeatures()
                   .primitivesGeneratedQueryWithRasterizerDiscard == VK_TRUE;
    }

    // If primitives generated is emulated with pipeline statistics query, it's unknown on which
    // hardware rasterizer discard is supported.  Assume it's supported on none.
    if (getFeatures().supportsPipelineStatisticsQuery.enabled)
    {
        return false;
    }

    return true;
}

bool ContextVk::isEmulatingRasterizerDiscardDuringPrimitivesGeneratedQuery(
    bool isPrimitivesGeneratedQueryActive) const
{
    return isPrimitivesGeneratedQueryActive && mState.isRasterizerDiscardEnabled() &&
           !doesPrimitivesGeneratedQuerySupportRasterizerDiscard();
}

QueryVk *ContextVk::getActiveRenderPassQuery(gl::QueryType queryType) const
{
    return mActiveRenderPassQueries[queryType];
}

bool ContextVk::isRobustResourceInitEnabled() const
{
    return mState.isRobustResourceInitEnabled();
}

void ContextVk::setDefaultUniformBlocksMinSizeForTesting(size_t minSize)
{
    mDefaultUniformStorage.setMinimumSizeForTesting(minSize);
}

angle::Result ContextVk::initializeMultisampleTextureToBlack(const gl::Context *context,
                                                             gl::Texture *glTexture)
{
    ASSERT(glTexture->getType() == gl::TextureType::_2DMultisample);
    TextureVk *textureVk = vk::GetImpl(glTexture);

    return textureVk->initializeContentsWithBlack(context, GL_NONE,
                                                  gl::ImageIndex::Make2DMultisample());
}

void ContextVk::onProgramExecutableReset(ProgramExecutableVk *executableVk)
{
    const gl::ProgramExecutable *executable = getState().getProgramExecutable();
    if (!executable)
    {
        return;
    }

    // Only do this for the currently bound ProgramExecutableVk, since Program A can be linked while
    // Program B is currently in use and we don't want to reset/invalidate Program B's pipeline.
    if (executableVk != getExecutable())
    {
        return;
    }

    // Reset *ContextVk::mCurrentGraphicsPipeline, since programInfo.release() freed the
    // PipelineHelper that it's currently pointing to.
    // TODO(http://anglebug.com/5624): rework updateActiveTextures(), createPipelineLayout(),
    // handleDirtyGraphicsPipeline(), and ProgramPipelineVk::link().
    resetCurrentGraphicsPipeline();

    if (executable->hasLinkedShaderStage(gl::ShaderType::Compute))
    {
        invalidateCurrentComputePipeline();
    }

    if (executable->hasLinkedShaderStage(gl::ShaderType::Vertex))
    {
        invalidateCurrentGraphicsPipeline();
    }
}

bool ContextVk::shouldSwitchToReadOnlyDepthFeedbackLoopMode(gl::Texture *texture,
                                                            gl::Command command) const
{
    ASSERT(texture->isDepthOrStencil());

    // When running compute we don't have a draw FBO.
    if (command == gl::Command::Dispatch)
    {
        return false;
    }

    // The "readOnlyDepthMode" feature enables read-only depth-stencil feedback loops. We
    // only switch to "read-only" mode when there's loop. We track the depth-stencil access
    // mode in the RenderPass. The tracking tells us when we can retroactively go back and
    // change the RenderPass to read-only. If there are any writes we need to break and
    // finish the current RP before starting the read-only one.
    FramebufferVk *drawFramebufferVk = getDrawFramebuffer();
    return texture->isBoundToFramebuffer(drawFramebufferVk->getState().getFramebufferSerial()) &&
           !mState.isDepthWriteEnabled() && !drawFramebufferVk->isReadOnlyDepthFeedbackLoopMode();
}

angle::Result ContextVk::onResourceAccess(const vk::CommandBufferAccess &access)
{
    ANGLE_TRY(flushCommandBuffersIfNecessary(access));

    vk::OutsideRenderPassCommandBuffer *commandBuffer =
        &mOutsideRenderPassCommands->getCommandBuffer();

    for (const vk::CommandBufferImageAccess &imageAccess : access.getReadImages())
    {
        ASSERT(!IsRenderPassStartedAndUsesImage(*mRenderPassCommands, *imageAccess.image));

        imageAccess.image->recordReadBarrier(this, imageAccess.aspectFlags, imageAccess.imageLayout,
                                             commandBuffer);
        mOutsideRenderPassCommands->retainResource(imageAccess.image);
    }

    for (const vk::CommandBufferImageWrite &imageWrite : access.getWriteImages())
    {
        ASSERT(!IsRenderPassStartedAndUsesImage(*mRenderPassCommands, *imageWrite.access.image));

        imageWrite.access.image->recordWriteBarrier(this, imageWrite.access.aspectFlags,
                                                    imageWrite.access.imageLayout, commandBuffer);
        mOutsideRenderPassCommands->retainResource(imageWrite.access.image);
        imageWrite.access.image->onWrite(imageWrite.levelStart, imageWrite.levelCount,
                                         imageWrite.layerStart, imageWrite.layerCount,
                                         imageWrite.access.aspectFlags);
    }

    for (const vk::CommandBufferBufferAccess &bufferAccess : access.getReadBuffers())
    {
        ASSERT(!mRenderPassCommands->usesBufferForWrite(*bufferAccess.buffer));
        ASSERT(!mOutsideRenderPassCommands->usesBufferForWrite(*bufferAccess.buffer));

        mOutsideRenderPassCommands->bufferRead(this, bufferAccess.accessType, bufferAccess.stage,
                                               bufferAccess.buffer);
    }

    for (const vk::CommandBufferBufferAccess &bufferAccess : access.getWriteBuffers())
    {
        ASSERT(!mRenderPassCommands->usesBuffer(*bufferAccess.buffer));
        ASSERT(!mOutsideRenderPassCommands->usesBuffer(*bufferAccess.buffer));

        mOutsideRenderPassCommands->bufferWrite(this, bufferAccess.accessType, bufferAccess.stage,
                                                bufferAccess.buffer);
    }

    for (const vk::CommandBufferBufferExternalAcquireRelease &bufferAcquireRelease :
         access.getExternalAcquireReleaseBuffers())
    {
        mOutsideRenderPassCommands->retainReadWriteResource(bufferAcquireRelease.buffer);
    }

    for (const vk::CommandBufferResourceAccess &resourceAccess : access.getAccessResources())
    {
        mOutsideRenderPassCommands->retainResource(resourceAccess.resource);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::flushCommandBuffersIfNecessary(const vk::CommandBufferAccess &access)
{
    // Go over resources and decide whether the render pass needs to close, whether the outside
    // render pass commands need to be flushed, or neither.  Note that closing the render pass
    // implies flushing the outside render pass as well, so if that needs to be done, we can close
    // the render pass and immediately return from this function.  Otherwise, this function keeps
    // track of whether the outside render pass commands need to be closed, and if so, it will do
    // that once at the end.

    // Read images only need to close the render pass if they need a layout transition.
    for (const vk::CommandBufferImageAccess &imageAccess : access.getReadImages())
    {
        // Note that different read methods are not compatible. A shader read uses a different
        // layout than a transfer read. So we cannot support simultaneous read usage as easily as
        // for Buffers.  TODO: Don't close the render pass if the image was only used read-only in
        // the render pass.  http://anglebug.com/4984
        if (IsRenderPassStartedAndUsesImage(*mRenderPassCommands, *imageAccess.image))
        {
            return flushCommandsAndEndRenderPass(RenderPassClosureReason::ImageUseThenOutOfRPRead);
        }
    }

    // Write images only need to close the render pass if they need a layout transition.
    for (const vk::CommandBufferImageWrite &imageWrite : access.getWriteImages())
    {
        if (IsRenderPassStartedAndUsesImage(*mRenderPassCommands, *imageWrite.access.image))
        {
            return flushCommandsAndEndRenderPass(RenderPassClosureReason::ImageUseThenOutOfRPWrite);
        }
    }

    bool shouldCloseOutsideRenderPassCommands = false;

    // Read buffers only need a new command buffer if previously used for write.
    for (const vk::CommandBufferBufferAccess &bufferAccess : access.getReadBuffers())
    {
        if (mRenderPassCommands->usesBufferForWrite(*bufferAccess.buffer))
        {
            return flushCommandsAndEndRenderPass(
                RenderPassClosureReason::BufferWriteThenOutOfRPRead);
        }
        else if (mOutsideRenderPassCommands->usesBufferForWrite(*bufferAccess.buffer))
        {
            shouldCloseOutsideRenderPassCommands = true;
        }
    }

    // Write buffers always need a new command buffer if previously used.
    for (const vk::CommandBufferBufferAccess &bufferAccess : access.getWriteBuffers())
    {
        if (mRenderPassCommands->usesBuffer(*bufferAccess.buffer))
        {
            return flushCommandsAndEndRenderPass(
                RenderPassClosureReason::BufferUseThenOutOfRPWrite);
        }
        else if (mOutsideRenderPassCommands->usesBuffer(*bufferAccess.buffer))
        {
            shouldCloseOutsideRenderPassCommands = true;
        }
    }

    if (shouldCloseOutsideRenderPassCommands)
    {
        return flushOutsideRenderPassCommands();
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::endRenderPassIfComputeReadAfterTransformFeedbackWrite()
{
    // Similar to flushCommandBuffersIfNecessary(), but using uniform buffers currently bound and
    // used by the current (compute) program.  This is to handle read-after-write hazards where the
    // write originates from transform feedback.
    if (mCurrentTransformFeedbackBuffers.empty())
    {
        return angle::Result::Continue;
    }

    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable && executable->hasLinkedShaderStage(gl::ShaderType::Compute));

    // Uniform buffers:
    const std::vector<gl::InterfaceBlock> &blocks = executable->getUniformBlocks();

    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            mState.getIndexedUniformBuffer(block.binding);

        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        vk::BufferHelper &buffer = vk::GetImpl(bufferBinding.get())->getBuffer();
        if (mCurrentTransformFeedbackBuffers.contains(&buffer))
        {
            return flushCommandsAndEndRenderPass(RenderPassClosureReason::XfbWriteThenComputeRead);
        }
    }

    return angle::Result::Continue;
}

// When textures/images bound/used by current compute program and have been accessed
// as sampled texture in current renderpass, need to take care the implicit layout
// transition of these textures/images in the render pass.
angle::Result ContextVk::endRenderPassIfComputeAccessAfterGraphicsImageAccess()
{
    const gl::ProgramExecutable *executable = mState.getProgramExecutable();
    ASSERT(executable && executable->hasLinkedShaderStage(gl::ShaderType::Compute));

    for (size_t imageUnitIndex : executable->getActiveImagesMask())
    {
        const gl::Texture *texture = mState.getImageUnit(imageUnitIndex).texture.get();
        if (texture == nullptr)
        {
            continue;
        }

        TextureVk *textureVk = vk::GetImpl(texture);

        if (texture->getType() == gl::TextureType::Buffer)
        {
            continue;
        }
        else
        {
            vk::ImageHelper &image = textureVk->getImage();

            // This is to handle the implicit layout transition in renderpass of this image,
            // while it currently be bound and used by current compute program.
            if (IsRenderPassStartedAndTransitionsImageLayout(*mRenderPassCommands, image))
            {
                return flushCommandsAndEndRenderPass(
                    RenderPassClosureReason::GraphicsTextureImageAccessThenComputeAccess);
            }
        }
    }

    const gl::ActiveTexturesCache &textures        = mState.getActiveTexturesCache();
    const gl::ActiveTextureTypeArray &textureTypes = executable->getActiveSamplerTypes();

    for (size_t textureUnit : executable->getActiveSamplersMask())
    {
        gl::Texture *texture        = textures[textureUnit];
        gl::TextureType textureType = textureTypes[textureUnit];

        if (texture == nullptr || textureType == gl::TextureType::Buffer)
        {
            continue;
        }

        TextureVk *textureVk = vk::GetImpl(texture);
        ASSERT(textureVk != nullptr);
        vk::ImageHelper &image = textureVk->getImage();

        // Similar to flushCommandBuffersIfNecessary(), but using textures currently bound and used
        // by the current (compute) program.  This is to handle read-after-write hazards where the
        // write originates from a framebuffer attachment.
        if (image.hasRenderPassUsageFlag(vk::RenderPassUsage::RenderTargetAttachment) &&
            IsRenderPassStartedAndUsesImage(*mRenderPassCommands, image))
        {
            return flushCommandsAndEndRenderPass(
                RenderPassClosureReason::ImageAttachmentThenComputeRead);
        }

        // Take care of the read image layout transition require implicit synchronization.
        if (IsRenderPassStartedAndTransitionsImageLayout(*mRenderPassCommands, image))
        {
            return flushCommandsAndEndRenderPass(
                RenderPassClosureReason::GraphicsTextureImageAccessThenComputeAccess);
        }
    }

    return angle::Result::Continue;
}

ProgramExecutableVk *ContextVk::getExecutable() const
{
    gl::Program *program = mState.getProgram();
    if (program)
    {
        if (!program->hasLinkingState())
        {
            return &vk::GetImpl(program)->getExecutable();
        }
    }
    else
    {
        ProgramPipelineVk *programPipelineVk = getProgramPipeline();
        if (programPipelineVk)
        {
            return &programPipelineVk->getExecutable();
        }
    }
    return nullptr;
}

const angle::PerfMonitorCounterGroups &ContextVk::getPerfMonitorCounters()
{
    syncObjectPerfCounters(mRenderer->getCommandQueuePerfCounters());

    angle::PerfMonitorCounters &counters =
        angle::GetPerfMonitorCounterGroup(mPerfMonitorCounters, "vulkan").counters;

#define ANGLE_UPDATE_PERF_MAP(COUNTER) \
    angle::GetPerfMonitorCounter(counters, #COUNTER).value = mPerfCounters.COUNTER;

    ANGLE_VK_PERF_COUNTERS_X(ANGLE_UPDATE_PERF_MAP)

#undef ANGLE_UPDATE_PERF_MAP

    return mPerfMonitorCounters;
}

angle::Result ContextVk::bindCachedDescriptorPool(
    DescriptorSetIndex descriptorSetIndex,
    const vk::DescriptorSetLayoutDesc &descriptorSetLayoutDesc,
    uint32_t descriptorCountMultiplier,
    vk::DescriptorPoolPointer *poolPointerOut)
{
    vk::MetaDescriptorPool &descriptorPool =
        mShareGroupVk->getMetaDescriptorPool(descriptorSetIndex);
    return descriptorPool.bindCachedDescriptorPool(
        this, descriptorSetLayoutDesc, descriptorCountMultiplier,
        &mShareGroupVk->getDescriptorSetLayoutCache(), poolPointerOut);
}

angle::Result ContextVk::switchToFramebufferFetchMode(bool hasFramebufferFetch)
{
    // If framebuffer fetch is permanent, make sure we never switch out of it.
    if (getFeatures().permanentlySwitchToFramebufferFetchMode.enabled && mIsInFramebufferFetchMode)
    {
        return angle::Result::Continue;
    }

    ASSERT(mIsInFramebufferFetchMode != hasFramebufferFetch);
    mIsInFramebufferFetchMode = hasFramebufferFetch;

    // If a render pass is already open, close it.
    if (mRenderPassCommands->started())
    {
        ANGLE_TRY(
            flushCommandsAndEndRenderPass(RenderPassClosureReason::FramebufferFetchEmulation));
    }

    // If there's a draw buffer bound, switch it to framebuffer fetch mode.  Every other framebuffer
    // will switch when bound.
    if (mState.getDrawFramebuffer() != nullptr)
    {
        getDrawFramebuffer()->switchToFramebufferFetchMode(this, mIsInFramebufferFetchMode);
    }

    // Clear the render pass cache; all render passes will be incompatible from now on with the old
    // ones.
    if (getFeatures().permanentlySwitchToFramebufferFetchMode.enabled)
    {
        mRenderPassCache.clear(this);
    }

    mRenderer->onFramebufferFetchUsed();

    return angle::Result::Continue;
}

// UpdateDescriptorSetsBuilder implementation.
UpdateDescriptorSetsBuilder::UpdateDescriptorSetsBuilder()
{
    // Reserve reasonable amount of spaces so that for majority of apps we don't need to grow at all
    mDescriptorBufferInfos.reserve(kDescriptorBufferInfosInitialSize);
    mDescriptorImageInfos.reserve(kDescriptorImageInfosInitialSize);
    mWriteDescriptorSets.reserve(kDescriptorWriteInfosInitialSize);
    mBufferViews.reserve(kDescriptorBufferViewsInitialSize);
}

UpdateDescriptorSetsBuilder::~UpdateDescriptorSetsBuilder() = default;

template <typename T, const T *VkWriteDescriptorSet::*pInfo>
void UpdateDescriptorSetsBuilder::growDescriptorCapacity(std::vector<T> *descriptorVector,
                                                         size_t newSize)
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
T *UpdateDescriptorSetsBuilder::allocDescriptorInfos(std::vector<T> *descriptorVector, size_t count)
{
    size_t oldSize = descriptorVector->size();
    size_t newSize = oldSize + count;
    if (newSize > descriptorVector->capacity())
    {
        // If we have reached capacity, grow the storage and patch the descriptor set with new
        // buffer info pointer
        growDescriptorCapacity<T, pInfo>(descriptorVector, newSize);
    }
    descriptorVector->resize(newSize);
    return &(*descriptorVector)[oldSize];
}

VkDescriptorBufferInfo *UpdateDescriptorSetsBuilder::allocDescriptorBufferInfos(size_t count)
{
    return allocDescriptorInfos<VkDescriptorBufferInfo, &VkWriteDescriptorSet::pBufferInfo>(
        &mDescriptorBufferInfos, count);
}

VkDescriptorImageInfo *UpdateDescriptorSetsBuilder::allocDescriptorImageInfos(size_t count)
{
    return allocDescriptorInfos<VkDescriptorImageInfo, &VkWriteDescriptorSet::pImageInfo>(
        &mDescriptorImageInfos, count);
}

VkWriteDescriptorSet *UpdateDescriptorSetsBuilder::allocWriteDescriptorSets(size_t count)
{
    size_t oldSize = mWriteDescriptorSets.size();
    size_t newSize = oldSize + count;
    mWriteDescriptorSets.resize(newSize);
    return &mWriteDescriptorSets[oldSize];
}

VkBufferView *UpdateDescriptorSetsBuilder::allocBufferViews(size_t count)
{
    return allocDescriptorInfos<VkBufferView, &VkWriteDescriptorSet::pTexelBufferView>(
        &mBufferViews, count);
}

uint32_t UpdateDescriptorSetsBuilder::flushDescriptorSetUpdates(VkDevice device)
{
    if (mWriteDescriptorSets.empty())
    {
        ASSERT(mDescriptorBufferInfos.empty());
        ASSERT(mDescriptorImageInfos.empty());
        return 0;
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(mWriteDescriptorSets.size()),
                           mWriteDescriptorSets.data(), 0, nullptr);

    uint32_t retVal = static_cast<uint32_t>(mDescriptorImageInfos.size());

    mWriteDescriptorSets.clear();
    mDescriptorBufferInfos.clear();
    mDescriptorImageInfos.clear();
    mBufferViews.clear();

    return retVal;
}

void ContextVk::resetPerFramePerfCounters()
{
    mPerfCounters.renderPasses                           = 0;
    mPerfCounters.writeDescriptorSets                    = 0;
    mPerfCounters.flushedOutsideRenderPassCommandBuffers = 0;
    mPerfCounters.resolveImageCommands                   = 0;
    mPerfCounters.descriptorSetAllocations               = 0;
    mPerfCounters.pipelineCreationCacheHits              = 0;
    mPerfCounters.pipelineCreationCacheMisses            = 0;

    mRenderer->resetCommandQueuePerFrameCounters();

    mShareGroupVk->getMetaDescriptorPool(DescriptorSetIndex::UniformsAndXfb)
        .resetDescriptorCacheStats();
    mShareGroupVk->getMetaDescriptorPool(DescriptorSetIndex::Texture).resetDescriptorCacheStats();
    mShareGroupVk->getMetaDescriptorPool(DescriptorSetIndex::ShaderResource)
        .resetDescriptorCacheStats();
}
}  // namespace rx
