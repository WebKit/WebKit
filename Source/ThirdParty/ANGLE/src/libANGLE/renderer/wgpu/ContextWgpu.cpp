//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextWgpu.cpp:
//    Implements the class methods for ContextWgpu.
//

#include "libANGLE/renderer/wgpu/ContextWgpu.h"

#include "common/debug.h"

#include "libANGLE/Context.h"
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
    }};

}  // namespace

ContextWgpu::ContextWgpu(const gl::State &state, gl::ErrorSet *errorSet, DisplayWgpu *display)
    : ContextImpl(state, errorSet), mDisplay(display)
{
    mNewRenderPassDirtyBits = DirtyBits{
        DIRTY_BIT_RENDER_PIPELINE_BINDING,  // The pipeline needs to be bound for each renderpass
    };
}

ContextWgpu::~ContextWgpu() {}

void ContextWgpu::onDestroy(const gl::Context *context)
{
    mImageLoadContext = {};
}

angle::Result ContextWgpu::initialize(const angle::ImageLoadContext &imageLoadContext)
{
    mImageLoadContext = imageLoadContext;
    return angle::Result::Continue;
}

angle::Result ContextWgpu::onFramebufferChange(FramebufferWgpu *framebufferWgpu,
                                               gl::Command command)
{
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
        wgpu::CommandBuffer commandBuffer = mCurrentCommandEncoder.Finish();
        mCurrentCommandEncoder            = nullptr;

        getQueue().Submit(1, &commandBuffer);
    }

    return angle::Result::Continue;
}

void ContextWgpu::setColorAttachmentFormat(size_t colorIndex, wgpu::TextureFormat format)
{
    if (mRenderPipelineDesc.setColorAttachmentFormat(colorIndex, format))
    {
        invalidateCurrentRenderPipeline();
    }
}

void ContextWgpu::setColorAttachmentFormats(
    const gl::DrawBuffersArray<wgpu::TextureFormat> &formats)
{
    for (size_t i = 0; i < formats.size(); i++)
    {
        setColorAttachmentFormat(i, formats[i]);
    }
}

void ContextWgpu::setDepthStencilFormat(wgpu::TextureFormat format)
{
    if (mRenderPipelineDesc.setDepthStencilAttachmentFormat(format))
    {
        invalidateCurrentRenderPipeline();
    }
}

angle::Result ContextWgpu::finish(const gl::Context *context)
{
    ANGLE_TRY(flush(webgpu::RenderPassClosureReason::GLFinish));

    wgpu::Future onWorkSubmittedFuture = getQueue().OnSubmittedWorkDone(
        wgpu::CallbackMode::WaitAnyOnly, [](wgpu::QueueWorkDoneStatus status) {});
    wgpu::WaitStatus status = getInstance().WaitAny(onWorkSubmittedFuture, -1);
    ASSERT(!webgpu::IsWgpuError(status));

    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawArrays(const gl::Context *context,
                                      gl::PrimitiveMode mode,
                                      GLint first,
                                      GLsizei count)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }
    else if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(
        setupDraw(context, mode, first, count, 1, gl::DrawElementsType::InvalidEnum, nullptr));
    mCommandBuffer.draw(static_cast<uint32_t>(count), 1, static_cast<uint32_t>(first), 0);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawArraysInstanced(const gl::Context *context,
                                               gl::PrimitiveMode mode,
                                               GLint first,
                                               GLsizei count,
                                               GLsizei instanceCount)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }
    else if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupDraw(context, mode, first, count, instanceCount,
                        gl::DrawElementsType::InvalidEnum, nullptr));
    mCommandBuffer.draw(static_cast<uint32_t>(count), static_cast<uint32_t>(instanceCount),
                        static_cast<uint32_t>(first), 0);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawArraysInstancedBaseInstance(const gl::Context *context,
                                                           gl::PrimitiveMode mode,
                                                           GLint first,
                                                           GLsizei count,
                                                           GLsizei instanceCount,
                                                           GLuint baseInstance)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }
    else if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupDraw(context, mode, first, count, instanceCount,
                        gl::DrawElementsType::InvalidEnum, nullptr));
    mCommandBuffer.draw(static_cast<uint32_t>(count), static_cast<uint32_t>(instanceCount),
                        static_cast<uint32_t>(first), baseInstance);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElements(const gl::Context *context,
                                        gl::PrimitiveMode mode,
                                        GLsizei count,
                                        gl::DrawElementsType type,
                                        const void *indices)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }
    else if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupIndexedDraw(context, mode, count, 1, type, indices));
    // TODO: draw
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsBaseVertex(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  GLsizei count,
                                                  gl::DrawElementsType type,
                                                  const void *indices,
                                                  GLint baseVertex)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }
    else if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupIndexedDraw(context, mode, count, 1, type, indices));
    // TODO: draw
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsInstanced(const gl::Context *context,
                                                 gl::PrimitiveMode mode,
                                                 GLsizei count,
                                                 gl::DrawElementsType type,
                                                 const void *indices,
                                                 GLsizei instances)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }
    else if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupIndexedDraw(context, mode, count, instances, type, indices));
    // TODO: draw
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
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }
    else if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupIndexedDraw(context, mode, count, instances, type, indices));
    // TODO: draw
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
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }
    else if (mode == gl::PrimitiveMode::TriangleFan)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ANGLE_TRY(setupIndexedDraw(context, mode, count, instances, type, indices));
    // TODO: draw
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

                ANGLE_TRY(endRenderPass(webgpu::RenderPassClosureReason::FramebufferBindingChange));
            }
            break;
            case gl::state::DIRTY_BIT_READ_FRAMEBUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_SCISSOR_TEST_ENABLED:
                break;
            case gl::state::DIRTY_BIT_SCISSOR:
                break;
            case gl::state::DIRTY_BIT_VIEWPORT:
                break;
            case gl::state::DIRTY_BIT_DEPTH_RANGE:
                break;
            case gl::state::DIRTY_BIT_BLEND_ENABLED:
                break;
            case gl::state::DIRTY_BIT_BLEND_COLOR:
                break;
            case gl::state::DIRTY_BIT_BLEND_FUNCS:
                break;
            case gl::state::DIRTY_BIT_BLEND_EQUATIONS:
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
                break;
            case gl::state::DIRTY_BIT_DEPTH_FUNC:
                if (mRenderPipelineDesc.setDepthFunc(
                        gl_wgpu::getCompareFunc(glState.getDepthStencilState().depthFunc)))
                {
                    invalidateCurrentRenderPipeline();
                }
                break;
            case gl::state::DIRTY_BIT_DEPTH_MASK:
                break;
            case gl::state::DIRTY_BIT_STENCIL_TEST_ENABLED:
                break;
            case gl::state::DIRTY_BIT_STENCIL_FUNCS_FRONT:
                if (mRenderPipelineDesc.setStencilFrontFunc(
                        gl_wgpu::getCompareFunc(glState.getDepthStencilState().stencilFunc)))
                {
                    invalidateCurrentRenderPipeline();
                }
                break;
            case gl::state::DIRTY_BIT_STENCIL_FUNCS_BACK:
                if (mRenderPipelineDesc.setStencilBackFunc(
                        gl_wgpu::getCompareFunc(glState.getDepthStencilState().stencilBackFunc)))
                {
                    invalidateCurrentRenderPipeline();
                }
                break;
            case gl::state::DIRTY_BIT_STENCIL_OPS_FRONT:
            {
                wgpu::StencilOperation failOp =
                    gl_wgpu::getStencilOp(glState.getDepthStencilState().stencilFail);
                wgpu::StencilOperation depthFailOp =
                    gl_wgpu::getStencilOp(glState.getDepthStencilState().stencilPassDepthFail);
                wgpu::StencilOperation passOp =
                    gl_wgpu::getStencilOp(glState.getDepthStencilState().stencilPassDepthPass);
                if (mRenderPipelineDesc.setStencilFrontOps(failOp, depthFailOp, passOp))
                {
                    invalidateCurrentRenderPipeline();
                }
            }
                break;
            case gl::state::DIRTY_BIT_STENCIL_OPS_BACK:
            {
                wgpu::StencilOperation failOp =
                    gl_wgpu::getStencilOp(glState.getDepthStencilState().stencilBackFail);
                wgpu::StencilOperation depthFailOp =
                    gl_wgpu::getStencilOp(glState.getDepthStencilState().stencilBackPassDepthFail);
                wgpu::StencilOperation passOp =
                    gl_wgpu::getStencilOp(glState.getDepthStencilState().stencilBackPassDepthPass);
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
                break;
            case gl::state::DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_DISPATCH_INDIRECT_BUFFER_BINDING:
                break;
            case gl::state::DIRTY_BIT_PROGRAM_BINDING:
            case gl::state::DIRTY_BIT_PROGRAM_EXECUTABLE:
                invalidateCurrentRenderPipeline();
                break;
            case gl::state::DIRTY_BIT_SAMPLER_BINDINGS:
                break;
            case gl::state::DIRTY_BIT_TEXTURE_BINDINGS:
                break;
            case gl::state::DIRTY_BIT_IMAGE_BINDINGS:
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
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_CLIP_DISTANCES:
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
                        case gl::state::EXTENDED_DIRTY_BIT_SHADING_RATE:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_LOGIC_OP_ENABLED:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_LOGIC_OP:
                            break;
                        case gl::state::EXTENDED_DIRTY_BIT_BLEND_ADVANCED_COHERENT:
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

VertexArrayImpl *ContextWgpu::createVertexArray(const gl::VertexArrayState &data)
{
    return new VertexArrayWgpu(data);
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

angle::Result ContextWgpu::startRenderPass(const wgpu::RenderPassDescriptor &desc)
{
    if (!mCurrentCommandEncoder)
    {
        mCurrentCommandEncoder = getDevice().CreateCommandEncoder(nullptr);
    }

    mCurrentRenderPass = mCurrentCommandEncoder.BeginRenderPass(&desc);
    mDirtyBits |= mNewRenderPassDirtyBits;

    return angle::Result::Continue;
}

angle::Result ContextWgpu::endRenderPass(webgpu::RenderPassClosureReason closureReason)
{
    if (mCurrentRenderPass)
    {
        const char *reasonText = kRenderPassClosureReason[closureReason];
        ASSERT(reasonText);

        if (mCommandBuffer.hasCommands())
        {
            mCommandBuffer.recordCommands(mCurrentRenderPass);
            mCommandBuffer.clear();
        }

        mCurrentRenderPass.End();
        mCurrentRenderPass = nullptr;
    }

    mDirtyBits.set(DIRTY_BIT_RENDER_PASS);

    return angle::Result::Continue;
}

angle::Result ContextWgpu::setupIndexedDraw(const gl::Context *context,
                                            gl::PrimitiveMode mode,
                                            GLsizei indexCount,
                                            GLsizei instanceCount,
                                            gl::DrawElementsType indexType,
                                            const void *indices)
{
    // TODO: handle index buffer binding

    return setupDraw(context, mode, 0, indexCount, instanceCount, indexType, indices);
}

angle::Result ContextWgpu::setupDraw(const gl::Context *context,
                                     gl::PrimitiveMode mode,
                                     GLint firstVertexOrInvalid,
                                     GLsizei vertexOrIndexCount,
                                     GLsizei instanceCount,
                                     gl::DrawElementsType indexTypeOrInvalid,
                                     const void *indices)
{
    if (mRenderPipelineDesc.setPrimitiveMode(mode, indexTypeOrInvalid))
    {
        invalidateCurrentRenderPipeline();
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

                default:
                    UNREACHABLE();
                    break;
            }
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

    wgpu::RenderPipeline previousPipeline = std::move(mCurrentGraphicsPipeline);
    ANGLE_TRY(executable->getRenderPipeline(this, mRenderPipelineDesc, &mCurrentGraphicsPipeline));
    if (mCurrentGraphicsPipeline != previousPipeline)
    {
        dirtyBitsIterator->setLaterBit(DIRTY_BIT_RENDER_PIPELINE_BINDING);
    }

    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyRenderPipelineBinding(DirtyBits::Iterator *dirtyBitsIterator)
{
    ASSERT(mCurrentGraphicsPipeline);
    mCommandBuffer.setPipeline(mCurrentGraphicsPipeline);
    return angle::Result::Continue;
}

angle::Result ContextWgpu::handleDirtyRenderPass(DirtyBits::Iterator *dirtyBitsIterator)
{
    FramebufferWgpu *drawFramebufferWgpu = webgpu::GetImpl(mState.getDrawFramebuffer());
    ANGLE_TRY(drawFramebufferWgpu->startNewRenderPass(this));
    dirtyBitsIterator->setLaterBits(mNewRenderPassDirtyBits);
    return angle::Result::Continue;
}

}  // namespace rx
