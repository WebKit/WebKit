//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextWgpu.cpp:
//    Implements the class methods for ContextWgpu.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_libc_calls
#endif

#include "libANGLE/renderer/wgpu/ContextWgpu.h"

#include "common/PackedEnums.h"
#include "common/debug.h"

#include "compiler/translator/wgsl/OutputUniformBlocks.h"
#include "libANGLE/Context.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/OverlayImpl.h"
#include "libANGLE/renderer/wgpu/BufferWgpu.h"
#include "libANGLE/renderer/wgpu/CompilerWgpu.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"
#include "libANGLE/renderer/wgpu/FenceNVWgpu.h"
#include "libANGLE/renderer/wgpu/FramebufferWgpu.h"
#include "libANGLE/renderer/wgpu/ImageWgpu.h"
#include "libANGLE/renderer/wgpu/ProgramExecutableWgpu.h"
#include "libANGLE/renderer/wgpu/ProgramPipelineWgpu.h"
#include "libANGLE/renderer/wgpu/ProgramWgpu.h"
#include "libANGLE/renderer/wgpu/QueryWgpu.h"
#include "libANGLE/renderer/wgpu/RenderbufferWgpu.h"
#include "libANGLE/renderer/wgpu/SamplerWgpu.h"
#include "libANGLE/renderer/wgpu/ShaderWgpu.h"
#include "libANGLE/renderer/wgpu/SyncWgpu.h"
#include "libANGLE/renderer/wgpu/TextureWgpu.h"
#include "libANGLE/renderer/wgpu/TransformFeedbackWgpu.h"
#include "libANGLE/renderer/wgpu/VertexArrayWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_pipeline_state.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{

namespace
{

constexpr angle::PackedEnumMap<webgpu::RenderPassClosureReason, const char *>
    kRenderPassClosureReason = {{
        {webgpu::RenderPassClosureReason::NewRenderPass,
         "Render pass closed due to starting a new render pass"},
        {webgpu::RenderPassClosureReason::FramebufferBindingChange,
         "Render pass closed due to framebuffer binding change"},
        {webgpu::RenderPassClosureReason::FramebufferInternalChange,
         "Render pass closed due to framebuffer internal change"},
        {webgpu::RenderPassClosureReason::GLFlush, "Render pass closed due to glFlush"},
        {webgpu::RenderPassClosureReason::GLFinish, "Render pass closed due to glFinish"},
        {webgpu::RenderPassClosureReason::EGLSwapBuffers,
         "Render pass closed due to eglSwapBuffers"},
        {webgpu::RenderPassClosureReason::GLReadPixels, "Render pass closed due to glReadPixels"},
        {webgpu::RenderPassClosureReason::IndexRangeReadback,
         "Render pass closed due to index buffer read back for streamed client data"},
        {webgpu::RenderPassClosureReason::VertexArrayStreaming,
         "Render pass closed for uploading streamed client data"},
        {webgpu::RenderPassClosureReason::VertexArrayLineLoop,
         "Render pass closed for line loop emulation"},
        {webgpu::RenderPassClosureReason::CopyBufferToTexture,
         "Render pass closed to update texture"},
        {webgpu::RenderPassClosureReason::CopyTextureToTexture,
         "Render pass closed to copy texture"},
        {webgpu::RenderPassClosureReason::CopyImage, "Render pass closed to copy image"},
    }};

}  // namespace

ContextWgpu::ContextWgpu(const gl::State &state, gl::ErrorSet *errorSet, DisplayWgpu *display)
    : ContextImpl(state, errorSet), mDisplay(display)
{
    mNewRenderPassDirtyBits = DirtyBits{
        DIRTY_BIT_RENDER_PIPELINE_BINDING,  // The pipeline needs to be bound for each renderpass
        DIRTY_BIT_VIEWPORT,
        DIRTY_BIT_SCISSOR,
        DIRTY_BIT_BLEND_CONSTANT,
        DIRTY_BIT_VERTEX_BUFFERS,
        DIRTY_BIT_INDEX_BUFFER,
        DIRTY_BIT_DRIVER_UNIFORMS,
        DIRTY_BIT_BIND_GROUPS,
    };
}

ContextWgpu::~ContextWgpu() {}

void ContextWgpu::onDestroy(const gl::Context *context)
{
    mImageLoadContext = {};
}

angle::Result ContextWgpu::initialize(const angle::ImageLoadContext &imageLoadContext)
{
    const DawnProcTable *wgpu = webgpu::GetProcs(this);

    mImageLoadContext = imageLoadContext;

    // Create the driver uniform bind group layout, which won't ever change.
    WGPUBindGroupLayoutEntry driverUniformBindGroupEntry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    driverUniformBindGroupEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    driverUniformBindGroupEntry.binding               = sh::kDriverUniformBlockBinding;
    driverUniformBindGroupEntry.buffer.type           = WGPUBufferBindingType_Uniform;
    driverUniformBindGroupEntry.buffer.minBindingSize = kDriverUniformSize;
    driverUniformBindGroupEntry.texture.sampleType    = WGPUTextureSampleType_BindingNotUsed;
    driverUniformBindGroupEntry.sampler.type          = WGPUSamplerBindingType_BindingNotUsed;
    driverUniformBindGroupEntry.storageTexture.access = WGPUStorageTextureAccess_BindingNotUsed;
    // Create a bind group layout with these entries.
    WGPUBindGroupLayoutDescriptor driverUniformsBindGroupLayoutDesc =
        WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    driverUniformsBindGroupLayoutDesc.entryCount = 1;
    driverUniformsBindGroupLayoutDesc.entries    = &driverUniformBindGroupEntry;
    mDriverUniformsBindGroupLayout               = webgpu::BindGroupLayoutHandle::Acquire(
        wgpu,
        wgpu->deviceCreateBindGroupLayout(getDevice().get(), &driverUniformsBindGroupLayoutDesc));

    // Driver uniforms should be set to 0 for later memcmp.
    memset(&mDriverUniforms, 0, sizeof(mDriverUniforms));

    return angle::Result::Continue;
}

angle::Result ContextWgpu::onFramebufferChange(FramebufferWgpu *framebufferWgpu,
                                               gl::Command command)
{
    // May modify framebuffer size, so invalidate driver uniforms which contain the framebuffer
    // size.
    invalidateDriverUniforms();

    // If internal framebuffer state changes, always end the render pass
    ANGLE_TRY(endRenderPass(webgpu::RenderPassClosureReason::FramebufferInternalChange));

    return angle::Result::Continue;
}

angle::Result ContextWgpu::flush(const gl::Context *context)
{
    return flush(webgpu::RenderPassClosureReason::GLFlush);
}

angle::Result ContextWgpu::flush(webgpu::RenderPassClosureReason closureReason)
{
    ANGLE_TRY(endRenderPass(closureReason));

    if (mCurrentCommandEncoder)
    {
        const DawnProcTable *wgpu                 = webgpu::GetProcs(this);
        webgpu::CommandBufferHandle commandBuffer = webgpu::CommandBufferHandle::Acquire(
            wgpu, wgpu->commandEncoderFinish(mCurrentCommandEncoder.get(), nullptr));
        mCurrentCommandEncoder            = nullptr;

        wgpu->queueSubmit(getQueue().get(), 1, &commandBuffer.get());
    }

    return angle::Result::Continue;
}

void ContextWgpu::setColorAttachmentFormat(size_t colorIndex, WGPUTextureFormat format)
{
    if (mRenderPipelineDesc.setColorAttachmentFormat(colorIndex, format))
    {
        invalidateCurrentRenderPipeline();
    }
}

void ContextWgpu::setColorAttachmentFormats(const gl::DrawBuffersArray<WGPUTextureFormat> &formats)
{
    for (size_t i = 0; i < formats.size(); i++)
    {
        setColorAttachmentFormat(i, formats[i]);
    }
}

void ContextWgpu::setDepthStencilFormat(WGPUTextureFormat format)
{
    if (mRenderPipelineDesc.setDepthStencilAttachmentFormat(format))
    {
        invalidateCurrentRenderPipeline();
    }
}

void ContextWgpu::setVertexAttribute(size_t attribIndex, webgpu::PackedVertexAttribute newAttrib)
{
    if (mRenderPipelineDesc.setVertexAttribute(attribIndex, newAttrib))
    {
        invalidateCurrentRenderPipeline();
    }
}

void ContextWgpu::invalidateVertexBuffer(size_t slot)
{
    if (mCurrentRenderPipelineAllAttributes[slot])
    {
        mDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
        mDirtyVertexBuffers.set(slot);
    }
}

void ContextWgpu::invalidateVertexBuffers()
{
    mDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
    mDirtyVertexBuffers = mCurrentRenderPipelineAllAttributes;
}

void ContextWgpu::invalidateIndexBuffer()
{
    mDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
}

void ContextWgpu::invalidateCurrentTextures()
{
    ProgramExecutableWgpu *executableWgpu = webgpu::GetImpl(mState.getProgramExecutable());
    executableWgpu->markSamplerBindingsDirty();
    mDirtyBits.set(DIRTY_BIT_BIND_GROUPS);
}

void ContextWgpu::invalidateDriverUniforms()
{
    mDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS);
}

void ContextWgpu::ensureCommandEncoderCreated()
{
    if (!mCurrentCommandEncoder)
    {
        const DawnProcTable *wgpu = webgpu::GetProcs(this);
        mCurrentCommandEncoder    = webgpu::CommandEncoderHandle::Acquire(
            wgpu, wgpu->deviceCreateCommandEncoder(getDevice().get(), nullptr));
    }
}

angle::Result ContextWgpu::getCurrentCommandEncoder(webgpu::RenderPassClosureReason closureReason,
                                                    webgpu::CommandEncoderHandle *outHandle)
{
    if (hasActiveRenderPass())
    {
        ANGLE_TRY(endRenderPass(closureReason));
    }
    ensureCommandEncoderCreated();
    *outHandle = mCurrentCommandEncoder;
    return angle::Result::Continue;
}

angle::Result ContextWgpu::finish(const gl::Context *context)
{
    const DawnProcTable *wgpu = webgpu::GetProcs(this);

    ANGLE_TRY(flush(webgpu::RenderPassClosureReason::GLFinish));

    WGPUQueueWorkDoneCallbackInfo callback = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    callback.mode                          = WGPUCallbackMode_WaitAnyOnly;
    callback.callback                      = [](WGPUQueueWorkDoneStatus status,
                           WGPUStringView message,
                           void *userdata1, void *userdata2) {
        ASSERT(userdata1 == nullptr);
        ASSERT(userdata2 == nullptr);
    };

    WGPUFutureWaitInfo onWorkSubmittedFuture = WGPU_FUTURE_WAIT_INFO_INIT;
    onWorkSubmittedFuture.future = wgpu->queueOnSubmittedWorkDone(getQueue().get(), callback);

    WGPUWaitStatus status =
        wgpu->instanceWaitAny(getInstance().get(), 1, &onWorkSubmittedFuture, -1);
    ASSERT(!webgpu::IsWgpuError(status));

    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawArrays(const gl::Context *context,
                                      gl::PrimitiveMode mode,
                                      GLint first,
                                      GLsizei count)
{
    if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    uint32_t firstIndex = 0;
    uint32_t indexCount = static_cast<uint32_t>(count);
    ANGLE_TRY(setupDraw(context, mode, first, count, 1, gl::DrawElementsType::InvalidEnum, nullptr,
                        0, &firstIndex, &indexCount));
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        mCommandBuffer.drawIndexed(indexCount, 1, firstIndex, 0, 0);
    }
    else
    {
        mCommandBuffer.draw(static_cast<uint32_t>(count), 1, static_cast<uint32_t>(first), 0);
    }
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawArraysInstanced(const gl::Context *context,
                                               gl::PrimitiveMode mode,
                                               GLint first,
                                               GLsizei count,
                                               GLsizei instanceCount)
{
    if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    uint32_t firstIndex = 0;
    uint32_t indexCount = static_cast<uint32_t>(count);
    ANGLE_TRY(setupDraw(context, mode, first, count, instanceCount,
                        gl::DrawElementsType::InvalidEnum, nullptr, 0, &firstIndex, &indexCount));
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        mCommandBuffer.drawIndexed(indexCount, static_cast<uint32_t>(instanceCount), firstIndex, 0,
                                   0);
    }
    else
    {
        mCommandBuffer.draw(indexCount, static_cast<uint32_t>(instanceCount),
                            static_cast<uint32_t>(first), 0);
    }
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawArraysInstancedBaseInstance(const gl::Context *context,
                                                           gl::PrimitiveMode mode,
                                                           GLint first,
                                                           GLsizei count,
                                                           GLsizei instanceCount,
                                                           GLuint baseInstance)
{
    if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    uint32_t firstIndex = 0;
    uint32_t indexCount = static_cast<uint32_t>(count);
    ANGLE_TRY(setupDraw(context, mode, first, count, instanceCount,
                        gl::DrawElementsType::InvalidEnum, nullptr, 0, &firstIndex, &indexCount));
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        mCommandBuffer.drawIndexed(indexCount, static_cast<uint32_t>(instanceCount), firstIndex, 0,
                                   baseInstance);
    }
    else
    {
        mCommandBuffer.draw(static_cast<uint32_t>(count), static_cast<uint32_t>(instanceCount),
                            static_cast<uint32_t>(first), baseInstance);
    }
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElements(const gl::Context *context,
                                        gl::PrimitiveMode mode,
                                        GLsizei count,
                                        gl::DrawElementsType type,
                                        const void *indices)
{
    uint32_t firstVertex = 0;
    uint32_t indexCount  = static_cast<uint32_t>(count);
    if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupDraw(context, mode, 0, count, 1, type, indices, 0, &firstVertex, &indexCount));
    mCommandBuffer.drawIndexed(indexCount, 1, firstVertex, 0, 0);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsBaseVertex(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  GLsizei count,
                                                  gl::DrawElementsType type,
                                                  const void *indices,
                                                  GLint baseVertex)
{
    uint32_t firstVertex = 0;
    uint32_t indexCount  = static_cast<uint32_t>(count);
    if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupDraw(context, mode, 0, count, 1, type, indices, baseVertex, &firstVertex,
                        &indexCount));
    mCommandBuffer.drawIndexed(indexCount, 1, firstVertex, static_cast<uint32_t>(baseVertex), 0);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsInstanced(const gl::Context *context,
                                                 gl::PrimitiveMode mode,
                                                 GLsizei count,
                                                 gl::DrawElementsType type,
                                                 const void *indices,
                                                 GLsizei instances)
{
    uint32_t firstVertex = 0;
    uint32_t indexCount  = static_cast<uint32_t>(count);
    if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(
        setupDraw(context, mode, 0, count, instances, type, indices, 0, &firstVertex, &indexCount));
    mCommandBuffer.drawIndexed(indexCount, static_cast<uint32_t>(instances), firstVertex, 0, 0);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsInstancedBaseVertex(const gl::Context *context,
                                                           gl::PrimitiveMode mode,
                                                           GLsizei count,
                                                           gl::DrawElementsType type,
                                                           const void *indices,
                                                           GLsizei instances,
                                                           GLint baseVertex)
{
    uint32_t firstVertex = 0;
    uint32_t indexCount  = static_cast<uint32_t>(count);
    if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupDraw(context, mode, 0, count, instances, type, indices, baseVertex, &firstVertex,
                        &indexCount));
    mCommandBuffer.drawIndexed(indexCount, static_cast<uint32_t>(instances), firstVertex,
                               static_cast<uint32_t>(baseVertex), 0);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsInstancedBaseVertexBaseInstance(const gl::Context *context,
                                                                       gl::PrimitiveMode mode,
                                                                       GLsizei count,
                                                                       gl::DrawElementsType type,
                                                                       const void *indices,
                                                                       GLsizei instances,
                                                                       GLint baseVertex,
                                                                       GLuint baseInstance)
{
    uint32_t firstVertex = 0;
    uint32_t indexCount  = static_cast<uint32_t>(count);
    if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupDraw(context, mode, 0, count, instances, type, indices, baseVertex, &firstVertex,
                        &indexCount));
    mCommandBuffer.drawIndexed(indexCount, static_cast<uint32_t>(instances), firstVertex,
                               static_cast<uint32_t>(baseVertex),
                               static_cast<uint32_t>(baseInstance));
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawRangeElements(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             GLuint start,
                                             GLuint end,
                                             GLsizei count,
                                             gl::DrawElementsType type,
                                             const void *indices)
{
    return drawElements(context, mode, count, type, indices);
}

angle::Result ContextWgpu::drawRangeElementsBaseVertex(const gl::Context *context,
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

angle::Result ContextWgpu::drawArraysIndirect(const gl::Context *context,
                                              gl::PrimitiveMode mode,
                                              const void *indirect)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsIndirect(const gl::Context *context,
                                                gl::PrimitiveMode mode,
                                                gl::DrawElementsType type,
                                                const void *indirect)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawArrays(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           const GLint *firsts,
                                           const GLsizei *counts,
                                           GLsizei drawcount)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawArraysInstanced(const gl::Context *context,
                                                    gl::PrimitiveMode mode,
                                                    const GLint *firsts,
                                                    const GLsizei *counts,
                                                    const GLsizei *instanceCounts,
                                                    GLsizei drawcount)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawArraysIndirect(const gl::Context *context,
                                                   gl::PrimitiveMode mode,
                                                   const void *indirect,
                                                   GLsizei drawcount,
                                                   GLsizei stride)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawElements(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             const GLsizei *counts,
                                             gl::DrawElementsType type,
                                             const GLvoid *const *indices,
                                             GLsizei drawcount)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawElementsInstanced(const gl::Context *context,
                                                      gl::PrimitiveMode mode,
                                                      const GLsizei *counts,
                                                      gl::DrawElementsType type,
                                                      const GLvoid *const *indices,
                                                      const GLsizei *instanceCounts,
                                                      GLsizei drawcount)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawElementsIndirect(const gl::Context *context,
                                                     gl::PrimitiveMode mode,
                                                     gl::DrawElementsType type,
                                                     const void *indirect,
                                                     GLsizei drawcount,
                                                     GLsizei stride)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawArraysInstancedBaseInstance(const gl::Context *context,
                                                                gl::PrimitiveMode mode,
                                                                const GLint *firsts,
                                                                const GLsizei *counts,
                                                                const GLsizei *instanceCounts,
                                                                const GLuint *baseInstances,
                                                                GLsizei drawcount)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawElementsInstancedBaseVertexBaseInstance(
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
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

gl::GraphicsResetStatus ContextWgpu::getResetStatus()
{
    return gl::GraphicsResetStatus::NoError;
}

angle::Result ContextWgpu::insertEventMarker(GLsizei length, const char *marker)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::pushGroupMarker(GLsizei length, const char *marker)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::popGroupMarker()
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::pushDebugGroup(const gl::Context *context,
                                          GLenum source,
                                          GLuint id,
                                          const std::string &message)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::popDebugGroup(const gl::Context *context)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::syncState(const gl::Context *context,
                                     const gl::state::DirtyBits dirtyBits,
                                     const gl::state::DirtyBits bitMask,
                                     const gl::state::ExtendedDirtyBits extendedDirtyBits,
                                     const gl::state::ExtendedDirtyBits extendedBitMask,
                                     gl::Command command)
{
    const gl::State &glState = context->getState();

    for (auto iter = dirtyBits.begin(), endIter = dirtyBits.end(); iter != endIter; ++iter)
    {
        size_t dirtyBit = *iter;
        switch (dirtyBit)
        {
            case gl::state::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING:
            {
                const FramebufferWgpu *framebufferWgpu =
                    webgpu::GetImpl(context->getState().getDrawFramebuffer());
                setColorAttachmentFormats(framebufferWgpu->getCurrentColorAttachmentFormats());
                setDepthStencilFormat(framebufferWgpu->getCurrentDepthStencilAttachmentFormat());

                // May modify framebuffer size, so invalidate driver uniforms which contain the
                // framebuffer size.
                invalidateDriverUniforms();
                ANGLE_TRY(endRenderPass(webgpu::RenderPassClosureReason::FramebufferBindingChange));
            }
            break;
            case gl::state::DIRTY_BIT_READ_FRAMEBUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_SCISSOR_TEST_ENABLED:
                mDirtyBits.set(DIRTY_BIT_SCISSOR);
                break;
            case gl::state::DIRTY_BIT_SCISSOR:
                mDirtyBits.set(DIRTY_BIT_SCISSOR);
                break;
            case gl::state::DIRTY_BIT_VIEWPORT:
                mDirtyBits.set(DIRTY_BIT_VIEWPORT);
                break;
            case gl::state::DIRTY_BIT_DEPTH_RANGE:
                mDirtyBits.set(DIRTY_BIT_VIEWPORT);
                // Driver uniforms include the depth range, which has now changed.
                invalidateDriverUniforms();
                break;
            case gl::state::DIRTY_BIT_BLEND_ENABLED:
            {
                const gl::BlendStateExt &blendStateExt = mState.getBlendStateExt();
                gl::DrawBufferMask enabledMask         = blendStateExt.getEnabledMask();
                for (size_t i = 0; i < blendStateExt.getDrawBufferCount(); i++)
                {
                    if (mRenderPipelineDesc.setBlendEnabled(i, enabledMask.test(i)))
                    {
                        invalidateCurrentRenderPipeline();
                    }
                }
            }
                break;
            case gl::state::DIRTY_BIT_BLEND_COLOR:
                mDirtyBits.set(DIRTY_BIT_BLEND_CONSTANT);
                break;
            case gl::state::DIRTY_BIT_BLEND_FUNCS:
            {
                const gl::BlendStateExt &blendState = mState.getBlendStateExt();
                for (size_t i = 0; i < blendState.getDrawBufferCount(); i++)
                {
                    if (mRenderPipelineDesc.setBlendFuncs(
                            i, gl_wgpu::GetBlendFactor(blendState.getSrcColorIndexed(i)),
                            gl_wgpu::GetBlendFactor(blendState.getDstColorIndexed(i)),
                            gl_wgpu::GetBlendFactor(blendState.getSrcAlphaIndexed(i)),
                            gl_wgpu::GetBlendFactor(blendState.getDstAlphaIndexed(i))))
                    {
                        invalidateCurrentRenderPipeline();
                    }
                }
            }
                break;
            case gl::state::DIRTY_BIT_BLEND_EQUATIONS:
            {
                const gl::BlendStateExt &blendState = mState.getBlendStateExt();
                for (size_t i = 0; i < blendState.getDrawBufferCount(); i++)
                {
                    if (mRenderPipelineDesc.setBlendEquations(
                            i, gl_wgpu::GetBlendEquation(blendState.getEquationColorIndexed(i)),
                            gl_wgpu::GetBlendEquation(blendState.getEquationAlphaIndexed(i))))
                    {
                        invalidateCurrentRenderPipeline();
                    }
                }
            }
                break;
            case gl::state::DIRTY_BIT_COLOR_MASK:
            {
                const gl::BlendStateExt &blendStateExt = mState.getBlendStateExt();
                for (size_t i = 0; i < blendStateExt.getDrawBufferCount(); i++)
                {
                    bool r, g, b, a;
                    blendStateExt.getColorMaskIndexed(i, &r, &g, &b, &a);
                    mRenderPipelineDesc.setColorWriteMask(i, r, g, b, a);
                }
                invalidateCurrentRenderPipeline();
            }
            break;
            case gl::state::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED:
                // Driver uniforms include the sample alpha to coverage state.
                invalidateDriverUniforms();
                break;
            case gl::state::DIRTY_BIT_SAMPLE_COVERAGE_ENABLED:
                break;
            case gl::state::DIRTY_BIT_SAMPLE_COVERAGE:
                break;
            case gl::state::DIRTY_BIT_SAMPLE_MASK_ENABLED:
                break;
            case gl::state::DIRTY_BIT_SAMPLE_MASK:
                break;
            case gl::state::DIRTY_BIT_DEPTH_TEST_ENABLED:
                // Enabled and func get combined into one state in WebGPU. Only sync it once.
                iter.setLaterBit(gl::state::DIRTY_BIT_DEPTH_FUNC);
                break;
            case gl::state::DIRTY_BIT_DEPTH_FUNC:
                if (mRenderPipelineDesc.setDepthFunc(
                        gl_wgpu::GetCompareFunc(glState.getDepthStencilState().depthFunc,
                                                glState.getDepthStencilState().depthTest)))
                {
                    invalidateCurrentRenderPipeline();
                }
                break;
            case gl::state::DIRTY_BIT_DEPTH_MASK:
                break;
            case gl::state::DIRTY_BIT_STENCIL_TEST_ENABLED:
                // Changing the state of stencil test affects both the front and back funcs.
                iter.setLaterBit(gl::state::DIRTY_BIT_STENCIL_FUNCS_FRONT);
                iter.setLaterBit(gl::state::DIRTY_BIT_STENCIL_FUNCS_BACK);
                break;
            case gl::state::DIRTY_BIT_STENCIL_FUNCS_FRONT:
                if (mRenderPipelineDesc.setStencilFrontFunc(
                        gl_wgpu::GetCompareFunc(glState.getDepthStencilState().stencilFunc,
                                                glState.getDepthStencilState().stencilTest)))
                {
                    invalidateCurrentRenderPipeline();
                }
                break;
            case gl::state::DIRTY_BIT_STENCIL_FUNCS_BACK:
                if (mRenderPipelineDesc.setStencilBackFunc(
                        gl_wgpu::GetCompareFunc(glState.getDepthStencilState().stencilBackFunc,
                                                glState.getDepthStencilState().stencilTest)))
                {
                    invalidateCurrentRenderPipeline();
                }
                break;
            case gl::state::DIRTY_BIT_STENCIL_OPS_FRONT:
            {
                WGPUStencilOperation failOp =
                    gl_wgpu::GetStencilOp(glState.getDepthStencilState().stencilFail);
                WGPUStencilOperation depthFailOp =
                    gl_wgpu::GetStencilOp(glState.getDepthStencilState().stencilPassDepthFail);
                WGPUStencilOperation passOp =
                    gl_wgpu::GetStencilOp(glState.getDepthStencilState().stencilPassDepthPass);
                if (mRenderPipelineDesc.setStencilFrontOps(failOp, depthFailOp, passOp))
                {
                    invalidateCurrentRenderPipeline();
                }
            }
            break;
            case gl::state::DIRTY_BIT_STENCIL_OPS_BACK:
            {
                WGPUStencilOperation failOp =
                    gl_wgpu::GetStencilOp(glState.getDepthStencilState().stencilBackFail);
                WGPUStencilOperation depthFailOp =
                    gl_wgpu::GetStencilOp(glState.getDepthStencilState().stencilBackPassDepthFail);
                WGPUStencilOperation passOp =
                    gl_wgpu::GetStencilOp(glState.getDepthStencilState().stencilBackPassDepthPass);
                if (mRenderPipelineDesc.setStencilBackOps(failOp, depthFailOp, passOp))
                {
                    invalidateCurrentRenderPipeline();
                }
            }
            break;
            case gl::state::DIRTY_BIT_STENCIL_WRITEMASK_FRONT:
                if (mRenderPipelineDesc.setStencilWriteMask(
                        glState.getDepthStencilState().stencilWritemask))
                {
                    invalidateCurrentRenderPipeline();
                }
                break;
            case gl::state::DIRTY_BIT_STENCIL_WRITEMASK_BACK:
                break;
            case gl::state::DIRTY_BIT_CULL_FACE_ENABLED:
            case gl::state::DIRTY_BIT_CULL_FACE:
                mRenderPipelineDesc.setCullMode(glState.getRasterizerState().cullMode,
                                                glState.getRasterizerState().cullFace);
                invalidateCurrentRenderPipeline();
                break;
            case gl::state::DIRTY_BIT_FRONT_FACE:
                mRenderPipelineDesc.setFrontFace(glState.getRasterizerState().frontFace);
                invalidateCurrentRenderPipeline();
                break;
            case gl::state::DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED:
                break;
            case gl::state::DIRTY_BIT_POLYGON_OFFSET:
                break;
            case gl::state::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED:
                break;
            case gl::state::DIRTY_BIT_LINE_WIDTH:
                break;
            case gl::state::DIRTY_BIT_PRIMITIVE_RESTART_ENABLED:
                break;
            case gl::state::DIRTY_BIT_CLEAR_COLOR:
                break;
            case gl::state::DIRTY_BIT_CLEAR_DEPTH:
                break;
            case gl::state::DIRTY_BIT_CLEAR_STENCIL:
                break;
            case gl::state::DIRTY_BIT_UNPACK_STATE:
                break;
            case gl::state::DIRTY_BIT_UNPACK_BUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_PACK_STATE:
                break;
            case gl::state::DIRTY_BIT_PACK_BUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_DITHER_ENABLED:
                break;
            case gl::state::DIRTY_BIT_RENDERBUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_VERTEX_ARRAY_BINDING:
                invalidateCurrentRenderPipeline();
                break;
            case gl::state::DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_DISPATCH_INDIRECT_BUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_PROGRAM_BINDING:
            case gl::state::DIRTY_BIT_PROGRAM_EXECUTABLE:
                invalidateCurrentRenderPipeline();
                iter.setLaterBit(gl::state::DIRTY_BIT_TEXTURE_BINDINGS);
                break;
            case gl::state::DIRTY_BIT_SAMPLER_BINDINGS:
                invalidateCurrentTextures();
                break;
            case gl::state::DIRTY_BIT_TEXTURE_BINDINGS:
                invalidateCurrentTextures();
                break;
            case gl::state::DIRTY_BIT_IMAGE_BINDINGS:
                invalidateCurrentTextures();
                break;
            case gl::state::DIRTY_BIT_TRANSFORM_FEEDBACK_BINDING:
                break;
            case gl::state::DIRTY_BIT_UNIFORM_BUFFER_BINDINGS:
                break;
            case gl::state::DIRTY_BIT_SHADER_STORAGE_BUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_MULTISAMPLING:
                break;
            case gl::state::DIRTY_BIT_SAMPLE_ALPHA_TO_ONE:
                break;
            case gl::state::DIRTY_BIT_COVERAGE_MODULATION:
                break;
            case gl::state::DIRTY_BIT_FRAMEBUFFER_SRGB_WRITE_CONTROL_MODE:
                break;
            case gl::state::DIRTY_BIT_CURRENT_VALUES:
                break;
            case gl::state::DIRTY_BIT_PROVOKING_VERTEX:
                break;
            case gl::state::DIRTY_BIT_SAMPLE_SHADING:
                break;
            case gl::state::DIRTY_BIT_PATCH_VERTICES:
                break;
            case gl::state::DIRTY_BIT_EXTENDED:
            {
                for (auto extendedIter    = extendedDirtyBits.begin(),
                          extendedEndIter = extendedDirtyBits.end();
                     extendedIter != extendedEndIter; ++extendedIter)
                {
                    const size_t extendedDirtyBit = *extendedIter;
                    switch (extendedDirtyBit)
                    {
                        case gl::state::EXTENDED_DIRTY_BIT_CLIP_CONTROL:
                            // Driver uniforms are calculated using the clip control state.
                            invalidateDriverUniforms();
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_CLIP_DISTANCES:
                            // Driver uniforms include the clip distances.
                            invalidateDriverUniforms();
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_DEPTH_CLAMP_ENABLED:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_MIPMAP_GENERATION_HINT:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_POLYGON_MODE:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_POLYGON_OFFSET_POINT_ENABLED:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_POLYGON_OFFSET_LINE_ENABLED:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_SHADER_DERIVATIVE_HINT:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_SHADING_RATE_QCOM:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_SHADING_RATE_EXT:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_LOGIC_OP_ENABLED:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_LOGIC_OP:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_BLEND_ADVANCED_COHERENT:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_FETCH_PER_SAMPLE_ENABLED:
                            break;
                        default:
                            UNREACHABLE();
                    }
                }
            }
            break;

            default:
                UNREACHABLE();
                break;
        }
    }

    return angle::Result::Continue;
}

GLint ContextWgpu::getGPUDisjoint()
{
    return 0;
}

GLint64 ContextWgpu::getTimestamp()
{
    return 0;
}

angle::Result ContextWgpu::onMakeCurrent(const gl::Context *context)
{
    return angle::Result::Continue;
}

gl::Caps ContextWgpu::getNativeCaps() const
{
    return mDisplay->getGLCaps();
}

const gl::TextureCapsMap &ContextWgpu::getNativeTextureCaps() const
{
    return mDisplay->getGLTextureCaps();
}

const gl::Extensions &ContextWgpu::getNativeExtensions() const
{
    return mDisplay->getGLExtensions();
}

const gl::Limitations &ContextWgpu::getNativeLimitations() const
{
    return mDisplay->getGLLimitations();
}

const ShPixelLocalStorageOptions &ContextWgpu::getNativePixelLocalStorageOptions() const
{
    return mDisplay->getPLSOptions();
}

CompilerImpl *ContextWgpu::createCompiler()
{
    return new CompilerWgpu();
}

ShaderImpl *ContextWgpu::createShader(const gl::ShaderState &data)
{
    return new ShaderWgpu(data);
}

ProgramImpl *ContextWgpu::createProgram(const gl::ProgramState &data)
{
    return new ProgramWgpu(data);
}

ProgramExecutableImpl *ContextWgpu::createProgramExecutable(const gl::ProgramExecutable *executable)
{
    return new ProgramExecutableWgpu(executable);
}

FramebufferImpl *ContextWgpu::createFramebuffer(const gl::FramebufferState &data)
{
    return new FramebufferWgpu(data);
}

TextureImpl *ContextWgpu::createTexture(const gl::TextureState &state)
{
    return new TextureWgpu(state);
}

RenderbufferImpl *ContextWgpu::createRenderbuffer(const gl::RenderbufferState &state)
{
    return new RenderbufferWgpu(state);
}

BufferImpl *ContextWgpu::createBuffer(const gl::BufferState &state)
{
    return new BufferWgpu(state);
}

VertexArrayImpl *ContextWgpu::createVertexArray(const gl::VertexArrayState &data,
                                                const gl::VertexArrayBuffers &vertexArrayBuffers)
{
    return new VertexArrayWgpu(data, vertexArrayBuffers);
}

QueryImpl *ContextWgpu::createQuery(gl::QueryType type)
{
    return new QueryWgpu(type);
}

FenceNVImpl *ContextWgpu::createFenceNV()
{
    return new FenceNVWgpu();
}

SyncImpl *ContextWgpu::createSync()
{
    return new SyncWgpu();
}

TransformFeedbackImpl *ContextWgpu::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedbackWgpu(state);
}

SamplerImpl *ContextWgpu::createSampler(const gl::SamplerState &state)
{
    return new SamplerWgpu(state);
}

ProgramPipelineImpl *ContextWgpu::createProgramPipeline(const gl::ProgramPipelineState &state)
{
    return new ProgramPipelineWgpu(state);
}

MemoryObjectImpl *ContextWgpu::createMemoryObject()
{
    UNREACHABLE();
    return nullptr;
}

SemaphoreImpl *ContextWgpu::createSemaphore()
{
    UNREACHABLE();
    return nullptr;
}

OverlayImpl *ContextWgpu::createOverlay(const gl::OverlayState &state)
{
    return new OverlayImpl(state);
}

angle::Result ContextWgpu::dispatchCompute(const gl::Context *context,
                                           GLuint numGroupsX,
                                           GLuint numGroupsY,
                                           GLuint numGroupsZ)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::dispatchComputeIndirect(const gl::Context *context, GLintptr indirect)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::memoryBarrier(const gl::Context *context, GLbitfield barriers)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers)
{
    return angle::Result::Continue;
}

void ContextWgpu::handleError(GLenum errorCode,
                              const char *message,
                              const char *file,
                              const char *function,
                              unsigned int line)
{
    std::stringstream errorStream;
    errorStream << "Internal Wgpu back-end error: " << message << ".";
    mErrors->handleError(errorCode, errorStream.str().c_str(), file, function, line);
}

angle::Result ContextWgpu::startRenderPass(const webgpu::PackedRenderPassDescriptor &desc)
{
    const DawnProcTable *wgpu = webgpu::GetProcs(this);

    ensureCommandEncoderCreated();

    mCurrentRenderPass = webgpu::CreateRenderPass(wgpu, mCurrentCommandEncoder, desc);
    mDirtyBits |= mNewRenderPassDirtyBits;

    return angle::Result::Continue;
}

angle::Result ContextWgpu::endRenderPass(webgpu::RenderPassClosureReason closureReason)
{
    if (mCurrentRenderPass)
    {
        const DawnProcTable *wgpu = webgpu::GetProcs(this);

        const char *reasonText = kRenderPassClosureReason[closureReason];
        ASSERT(reasonText);

        if (mCommandBuffer.hasCommands())
        {
            ANGLE_WGPU_SCOPED_DEBUG_TRY(this,
                                        mCommandBuffer.recordCommands(wgpu, mCurrentRenderPass));
            mCommandBuffer.clear();
        }

        wgpu->renderPassEncoderEnd(mCurrentRenderPass.get());
        mCurrentRenderPass = nullptr;
    }

    mDirtyBits.set(DIRTY_BIT_RENDER_PASS);

    return angle::Result::Continue;
}

angle::Result ContextWgpu::setupDraw(const gl::Context *context,
                                     gl::PrimitiveMode mode,
                                     GLint firstVertexOrInvalid,
                                     GLsizei vertexOrIndexCount,
                                     GLsizei instanceCount,
                                     gl::DrawElementsType indexTypeOrInvalid,
                                     const void *indices,
                                     GLint baseVertex,
                                     uint32_t *outFirstIndex,
                                     uint32_t *indexCountOut)
{
    gl::DrawElementsType dstIndexTypeOrInvalid = indexTypeOrInvalid;
    if (mode == gl::PrimitiveMode::LineLoop &&
        dstIndexTypeOrInvalid == gl::DrawElementsType::InvalidEnum)
    {
        if (vertexOrIndexCount >= std::numeric_limits<unsigned short>::max())
        {
            dstIndexTypeOrInvalid = gl::DrawElementsType::UnsignedInt;
        }
        else
        {
            dstIndexTypeOrInvalid = gl::DrawElementsType::UnsignedShort;
        }
    }

    if (mRenderPipelineDesc.setPrimitiveMode(mode, dstIndexTypeOrInvalid))
    {
        invalidateCurrentRenderPipeline();
    }

    ProgramExecutableWgpu *executableWgpu = webgpu::GetImpl(mState.getProgramExecutable());
    if (executableWgpu->checkDirtyUniforms() || executableWgpu->hasDirtySamplerBindings())
    {
        mDirtyBits.set(DIRTY_BIT_BIND_GROUPS);
    }

    const void *adjustedIndicesPtr = indices;
    if (mState.areClientArraysEnabled())
    {
        VertexArrayWgpu *vertexArrayWgpu = GetImplAs<VertexArrayWgpu>(mState.getVertexArray());
        // Pass in original indexTypeOrInvalid into syncClientArrays because the method will need to
        // determine if the original draw call was a DrawElements or DrawArrays call.
        ANGLE_TRY(vertexArrayWgpu->syncClientArrays(
            context, mState.getProgramExecutable()->getActiveAttribLocationsMask(), mode,
            firstVertexOrInvalid, vertexOrIndexCount, instanceCount, indexTypeOrInvalid, indices,
            baseVertex, mState.isPrimitiveRestartEnabled(), &adjustedIndicesPtr, indexCountOut));
    }

    bool reAddDirtyIndexBufferBit = false;
    if (dstIndexTypeOrInvalid != gl::DrawElementsType::InvalidEnum)
    {
        *outFirstIndex =
            gl_wgpu::GetFirstIndexForDrawCall(dstIndexTypeOrInvalid, adjustedIndicesPtr);
        if (mCurrentIndexBufferType != dstIndexTypeOrInvalid)
        {
            invalidateIndexBuffer();
        }
    }

    if (mDirtyBits.any())
    {
        for (DirtyBits::Iterator dirtyBitIter = mDirtyBits.begin();
             dirtyBitIter != mDirtyBits.end(); ++dirtyBitIter)
        {
            size_t dirtyBit = *dirtyBitIter;
            switch (dirtyBit)
            {
                case DIRTY_BIT_RENDER_PIPELINE_DESC:
                    ANGLE_TRY(handleDirtyRenderPipelineDesc(&dirtyBitIter));
                    break;

                case DIRTY_BIT_RENDER_PASS:
                    ANGLE_TRY(handleDirtyRenderPass(&dirtyBitIter));
                    break;

                case DIRTY_BIT_RENDER_PIPELINE_BINDING:
                    ANGLE_TRY(handleDirtyRenderPipelineBinding(&dirtyBitIter));
                    break;

                case DIRTY_BIT_VIEWPORT:
                    ANGLE_TRY(handleDirtyViewport(&dirtyBitIter));
                    break;

                case DIRTY_BIT_SCISSOR:
                    ANGLE_TRY(handleDirtyScissor(&dirtyBitIter));
                    break;

                case DIRTY_BIT_BLEND_CONSTANT:
                    ANGLE_TRY(handleDirtyBlendConstant(&dirtyBitIter));
                    break;

                case DIRTY_BIT_VERTEX_BUFFERS:
                    ANGLE_TRY(handleDirtyVertexBuffers(mDirtyVertexBuffers, &dirtyBitIter));
                    mDirtyVertexBuffers.reset();
                    break;

                case DIRTY_BIT_INDEX_BUFFER:
                    if (dstIndexTypeOrInvalid != gl::DrawElementsType::InvalidEnum)
                    {
                        ANGLE_TRY(handleDirtyIndexBuffer(dstIndexTypeOrInvalid, &dirtyBitIter));
                    }
                    else
                    {
                        // If this is not an indexed draw call, don't sync the index buffer. Save it
                        // for a future indexed draw call when we know what index type to use
                        reAddDirtyIndexBufferBit = true;
                    }
                    break;
                case DIRTY_BIT_DRIVER_UNIFORMS:
                    ANGLE_TRY(handleDirtyDriverUniforms(&dirtyBitIter));
                    break;
                case DIRTY_BIT_BIND_GROUPS:
                    ANGLE_TRY(handleDirtyBindGroups(&dirtyBitIter));
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
        }

        if (reAddDirtyIndexBufferBit)
        {
            // Re-add the index buffer dirty bit for a future indexed draw call.
            mDirtyBits.reset(DIRTY_BIT_INDEX_BUFFER);
        }

        mDirtyBits.reset();
    }

    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyRenderPipelineDesc(DirtyBits::Iterator *dirtyBitsIterator)
{
    ASSERT(mState.getProgramExecutable() != nullptr);
    ProgramExecutableWgpu *executable = webgpu::GetImpl(mState.getProgramExecutable());
    ASSERT(executable);

    webgpu::RenderPipelineHandle previousPipeline = std::move(mCurrentGraphicsPipeline);
    ANGLE_TRY(executable->getRenderPipeline(this, mRenderPipelineDesc, &mCurrentGraphicsPipeline));
    if (mCurrentGraphicsPipeline != previousPipeline)
    {
        dirtyBitsIterator->setLaterBit(DIRTY_BIT_RENDER_PIPELINE_BINDING);
    }
    mCurrentRenderPipelineAllAttributes =
        executable->getExecutable()->getActiveAttribLocationsMask();

    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyRenderPipelineBinding(DirtyBits::Iterator *dirtyBitsIterator)
{
    ASSERT(mCurrentGraphicsPipeline);
    mCommandBuffer.setPipeline(mCurrentGraphicsPipeline);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyViewport(DirtyBits::Iterator *dirtyBitsIterator)
{
    const gl::Framebuffer *framebuffer = mState.getDrawFramebuffer();
    const gl::Extents &framebufferSize = framebuffer->getExtents();
    const gl::Rectangle framebufferRect(0, 0, framebufferSize.width, framebufferSize.height);

    gl::Rectangle clampedViewport;
    if (!ClipRectangle(mState.getViewport(), framebufferRect, &clampedViewport))
    {
        clampedViewport = gl::Rectangle(0, 0, 1, 1);
    }

    float depthMin = mState.getNearPlane();
    float depthMax = mState.getFarPlane();

    // This clamping should be done by the front end. WebGPU requires values in this range.
    ASSERT(depthMin >= 0 && depthMin <= 1);
    ASSERT(depthMin >= 0 && depthMin <= 1);

    // WebGPU requires that the maxDepth is at least minDepth. WebGL requires the same but core GL
    // ES does not.
    if (depthMin > depthMax)
    {
        UNIMPLEMENTED();
    }

    bool isDefaultViewport = (clampedViewport == framebufferRect) && depthMin == 0 && depthMax == 1;
    if (isDefaultViewport && !mCommandBuffer.hasSetViewportCommand())
    {
        // Each render pass has a default viewport set equal to the size of the render targets. We
        // can skip setting the viewport.
        return angle::Result::Continue;
    }

    FramebufferWgpu *drawFramebufferWgpu = webgpu::GetImpl(mState.getDrawFramebuffer());
    if (drawFramebufferWgpu->flipY())
    {
        clampedViewport.y =
            drawFramebufferWgpu->getState().getDimensions().height - clampedViewport.y1();
    }

    ASSERT(mCurrentGraphicsPipeline);
    mCommandBuffer.setViewport(clampedViewport.x, clampedViewport.y, clampedViewport.width,
                               clampedViewport.height, depthMin, depthMax);

    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyScissor(DirtyBits::Iterator *dirtyBitsIterator)
{
    const gl::Framebuffer *framebuffer = mState.getDrawFramebuffer();
    const gl::Extents &framebufferSize = framebuffer->getExtents();
    const gl::Rectangle framebufferRect(0, 0, framebufferSize.width, framebufferSize.height);

    gl::Rectangle clampedScissor = framebufferRect;

    // When the GL scissor test is disabled, set the scissor to the entire size of the framebuffer
    if (mState.isScissorTestEnabled())
    {
        if (!ClipRectangle(mState.getScissor(), framebufferRect, &clampedScissor))
        {
            clampedScissor = gl::Rectangle(0, 0, 0, 0);
        }
    }

    bool isDefaultScissor = clampedScissor == framebufferRect;
    if (isDefaultScissor && !mCommandBuffer.hasSetScissorCommand())
    {
        // Each render pass has a default scissor set equal to the size of the render targets. We
        // can skip setting the scissor.
        return angle::Result::Continue;
    }

    FramebufferWgpu *framebufferWgpu = webgpu::GetImpl(framebuffer);
    if (framebufferWgpu->flipY())
    {
        clampedScissor.y = framebufferWgpu->getState().getDimensions().height - clampedScissor.y -
                           clampedScissor.height;
    }

    ASSERT(mCurrentGraphicsPipeline);
    mCommandBuffer.setScissorRect(clampedScissor.x, clampedScissor.y, clampedScissor.width,
                                  clampedScissor.height);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyBlendConstant(DirtyBits::Iterator *dirtyBitsIterator)
{
    const gl::ColorF &blendColor = mState.getBlendColor();

    bool isDefaultBlendConstant = blendColor.red == 0 && blendColor.green == 0 &&
                                  blendColor.blue == 0 && blendColor.alpha == 0;
    if (isDefaultBlendConstant && !mCommandBuffer.hasSetBlendConstantCommand())
    {
        // Each render pass has a default blend constant set to all zeroes. We can skip setting it.
        return angle::Result::Continue;
    }

    ASSERT(mCurrentGraphicsPipeline);
    mCommandBuffer.setBlendConstant(blendColor.red, blendColor.green, blendColor.blue,
                                    blendColor.alpha);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyRenderPass(DirtyBits::Iterator *dirtyBitsIterator)
{
    FramebufferWgpu *drawFramebufferWgpu = webgpu::GetImpl(mState.getDrawFramebuffer());
    ANGLE_TRY(drawFramebufferWgpu->startNewRenderPass(this));
    dirtyBitsIterator->setLaterBits(mNewRenderPassDirtyBits);
    mDirtyVertexBuffers = mCurrentRenderPipelineAllAttributes;
    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyVertexBuffers(const gl::AttributesMask &slots,
                                                    DirtyBits::Iterator *dirtyBitsIterator)
{
    VertexArrayWgpu *vertexArrayWgpu = GetImplAs<VertexArrayWgpu>(mState.getVertexArray());
    for (size_t slot : slots)
    {
        const VertexBufferWithOffset &buffer = vertexArrayWgpu->getVertexBuffer(slot);
        if (!buffer.buffer)
        {
            // Missing default attribute support
            ASSERT(!mState.getVertexArray()->getVertexAttribute(slot).enabled);
            UNIMPLEMENTED();
            continue;
        }
        if (buffer.buffer->getMappedState())
        {
            ANGLE_TRY(buffer.buffer->unmap());
        }
        mCommandBuffer.setVertexBuffer(static_cast<uint32_t>(slot), buffer.buffer->getBuffer(),
                                       buffer.offset, WGPU_WHOLE_SIZE);
    }
    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyIndexBuffer(gl::DrawElementsType indexType,
                                                  DirtyBits::Iterator *dirtyBitsIterator)
{
    VertexArrayWgpu *vertexArrayWgpu = GetImplAs<VertexArrayWgpu>(mState.getVertexArray());
    webgpu::BufferHelper *buffer     = vertexArrayWgpu->getIndexBuffer();
    ASSERT(buffer);
    if (buffer->getMappedState())
    {
        ANGLE_TRY(buffer->unmap());
    }
    mCommandBuffer.setIndexBuffer(buffer->getBuffer(),
                                  static_cast<WGPUIndexFormat>(gl_wgpu::GetIndexFormat(indexType)),
                                  0, -1);
    mCurrentIndexBufferType = indexType;
    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyBindGroups(DirtyBits::Iterator *dirtyBitsIterator)
{
    ProgramExecutableWgpu *executableWgpu = webgpu::GetImpl(mState.getProgramExecutable());
    webgpu::BindGroupHandle defaultUniformBindGroup;
    ANGLE_TRY(executableWgpu->updateUniformsAndGetBindGroup(this, &defaultUniformBindGroup));
    mCommandBuffer.setBindGroup(sh::kDefaultUniformBlockBindGroup, defaultUniformBindGroup);

    webgpu::BindGroupHandle samplerAndTextureBindGroup;
    ANGLE_TRY(executableWgpu->getSamplerAndTextureBindGroup(this, &samplerAndTextureBindGroup));
    mCommandBuffer.setBindGroup(sh::kTextureAndSamplerBindGroup, samplerAndTextureBindGroup);

    // Creating the driver uniform bind group is handled by handleDirtyDriverUniforms().
    mCommandBuffer.setBindGroup(sh::kDriverUniformBindGroup, mDriverUniformsBindGroup);

    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyDriverUniforms(DirtyBits::Iterator *dirtyBitsIterator)
{
    const DawnProcTable *wgpu = webgpu::GetProcs(this);

    DriverUniforms newDriverUniforms;
    memset(&newDriverUniforms, 0, sizeof(newDriverUniforms));

    newDriverUniforms.depthRange[0] = mState.getNearPlane();
    newDriverUniforms.depthRange[1] = mState.getFarPlane();

    FramebufferWgpu *drawFramebufferWgpu = webgpu::GetImpl(mState.getDrawFramebuffer());

    newDriverUniforms.renderArea = drawFramebufferWgpu->getState().getDimensions().height << 16 |
                                   drawFramebufferWgpu->getState().getDimensions().width;

    const float flipX        = 1.0f;
    const float flipY        = drawFramebufferWgpu->flipY() ? -1.0f : 1.0f;
    newDriverUniforms.flipXY = gl::PackSnorm4x8(
        flipX, flipY, flipX, mState.getClipOrigin() == gl::ClipOrigin::LowerLeft ? -flipY : flipY);

    // gl_ClipDistance
    const uint32_t enabledClipDistances = mState.getEnabledClipDistances().bits();
    ASSERT((enabledClipDistances & ~sh::vk::kDriverUniformsMiscEnabledClipPlanesMask) == 0);

    // GL_CLIP_DEPTH_MODE_EXT
    const uint32_t transformDepth = !mState.isClipDepthModeZeroToOne();
    ASSERT((transformDepth & ~sh::vk::kDriverUniformsMiscTransformDepthMask) == 0);

    // GL_SAMPLE_ALPHA_TO_COVERAGE
    const uint32_t alphaToCoverage = mState.isSampleAlphaToCoverageEnabled();
    ASSERT((alphaToCoverage & ~sh::vk::kDriverUniformsMiscAlphaToCoverageMask) == 0);

    newDriverUniforms.misc =
        (enabledClipDistances << sh::vk::kDriverUniformsMiscEnabledClipPlanesOffset) |
        (transformDepth << sh::vk::kDriverUniformsMiscTransformDepthOffset) |
        (alphaToCoverage << sh::vk::kDriverUniformsMiscAlphaToCoverageOffset);

    // If no change to driver uniforms, return early.
    if (memcmp(&newDriverUniforms, &mDriverUniforms, sizeof(DriverUniforms)) == 0)
    {
        return angle::Result::Continue;
    }

    // Cache the uniforms so we can check for changes later.
    memcpy(&mDriverUniforms, &newDriverUniforms, sizeof(DriverUniforms));

    // Upload the new driver uniforms to a new GPU buffer.
    webgpu::BufferHelper driverUniformBuffer;

    ANGLE_TRY(driverUniformBuffer.initBuffer(wgpu, getDevice(), sizeof(DriverUniforms),
                                             WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                                             webgpu::MapAtCreation::Yes));

    ASSERT(driverUniformBuffer.valid());

    uint8_t *bufferData = driverUniformBuffer.getMapWritePointer(0, sizeof(DriverUniforms));
    memcpy(bufferData, &mDriverUniforms, sizeof(DriverUniforms));

    ANGLE_TRY(driverUniformBuffer.unmap());

    // Now create the bind group containing the driver uniform buffer.
    WGPUBindGroupEntry bindGroupEntry = WGPU_BIND_GROUP_ENTRY_INIT;
    bindGroupEntry.binding = sh::kDriverUniformBlockBinding;
    bindGroupEntry.buffer             = driverUniformBuffer.getBuffer().get();
    bindGroupEntry.offset  = 0;
    bindGroupEntry.size    = sizeof(DriverUniforms);

    WGPUBindGroupDescriptor bindGroupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindGroupDesc.layout                  = mDriverUniformsBindGroupLayout.get();
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries    = &bindGroupEntry;
    mDriverUniformsBindGroup              = webgpu::BindGroupHandle::Acquire(
        wgpu, wgpu->deviceCreateBindGroup(getDevice().get(), &bindGroupDesc));

    // This bind group needs to be updated on the same draw call as the driver uniforms are updated.
    dirtyBitsIterator->setLaterBit(DIRTY_BIT_BIND_GROUPS);

    return angle::Result::Continue;
}

}  // namespace rx
