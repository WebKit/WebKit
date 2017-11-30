//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Context9:
//   D3D9-specific functionality associated with a GL Context.
//

#include "libANGLE/renderer/d3d/d3d9/Context9.h"

#include "common/string_utils.h"
#include "libANGLE/renderer/d3d/CompilerD3D.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/RenderbufferD3D.h"
#include "libANGLE/renderer/d3d/SamplerD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/renderer/d3d/d3d9/Buffer9.h"
#include "libANGLE/renderer/d3d/d3d9/Fence9.h"
#include "libANGLE/renderer/d3d/d3d9/Framebuffer9.h"
#include "libANGLE/renderer/d3d/d3d9/Query9.h"
#include "libANGLE/renderer/d3d/d3d9/Renderer9.h"
#include "libANGLE/renderer/d3d/d3d9/StateManager9.h"
#include "libANGLE/renderer/d3d/d3d9/VertexArray9.h"

namespace rx
{

Context9::Context9(const gl::ContextState &state, Renderer9 *renderer)
    : ContextImpl(state), mRenderer(renderer)
{
}

Context9::~Context9()
{
}

gl::Error Context9::initialize()
{
    return gl::NoError();
}

CompilerImpl *Context9::createCompiler()
{
    return new CompilerD3D(SH_HLSL_3_0_OUTPUT);
}

ShaderImpl *Context9::createShader(const gl::ShaderState &data)
{
    return new ShaderD3D(data, mRenderer->getWorkarounds(), mRenderer->getNativeExtensions());
}

ProgramImpl *Context9::createProgram(const gl::ProgramState &data)
{
    return new ProgramD3D(data, mRenderer);
}

FramebufferImpl *Context9::createFramebuffer(const gl::FramebufferState &data)
{
    return new Framebuffer9(data, mRenderer);
}

TextureImpl *Context9::createTexture(const gl::TextureState &state)
{
    switch (state.getTarget())
    {
        case GL_TEXTURE_2D:
            return new TextureD3D_2D(state, mRenderer);
        case GL_TEXTURE_CUBE_MAP:
            return new TextureD3D_Cube(state, mRenderer);
        case GL_TEXTURE_EXTERNAL_OES:
            return new TextureD3D_External(state, mRenderer);
        default:
            UNREACHABLE();
    }
    return nullptr;
}

RenderbufferImpl *Context9::createRenderbuffer()
{
    return new RenderbufferD3D(mRenderer);
}

BufferImpl *Context9::createBuffer(const gl::BufferState &state)
{
    return new Buffer9(state, mRenderer);
}

VertexArrayImpl *Context9::createVertexArray(const gl::VertexArrayState &data)
{
    return new VertexArray9(data);
}

QueryImpl *Context9::createQuery(GLenum type)
{
    return new Query9(mRenderer, type);
}

FenceNVImpl *Context9::createFenceNV()
{
    return new FenceNV9(mRenderer);
}

SyncImpl *Context9::createSync()
{
    // D3D9 doesn't support ES 3.0 and its sync objects.
    UNREACHABLE();
    return nullptr;
}

TransformFeedbackImpl *Context9::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    UNREACHABLE();
    return nullptr;
}

SamplerImpl *Context9::createSampler(const gl::SamplerState &state)
{
    return new SamplerD3D(state);
}

ProgramPipelineImpl *Context9::createProgramPipeline(const gl::ProgramPipelineState &data)
{
    UNREACHABLE();
    return nullptr;
}

std::vector<PathImpl *> Context9::createPaths(GLsizei)
{
    return std::vector<PathImpl *>();
}

gl::Error Context9::flush(const gl::Context *context)
{
    return mRenderer->flush();
}

gl::Error Context9::finish(const gl::Context *context)
{
    return mRenderer->finish();
}

gl::Error Context9::drawArrays(const gl::Context *context, GLenum mode, GLint first, GLsizei count)
{
    return mRenderer->genericDrawArrays(context, mode, first, count, 0);
}

gl::Error Context9::drawArraysInstanced(const gl::Context *context,
                                        GLenum mode,
                                        GLint first,
                                        GLsizei count,
                                        GLsizei instanceCount)
{
    return mRenderer->genericDrawArrays(context, mode, first, count, instanceCount);
}

gl::Error Context9::drawElements(const gl::Context *context,
                                 GLenum mode,
                                 GLsizei count,
                                 GLenum type,
                                 const void *indices)
{
    return mRenderer->genericDrawElements(context, mode, count, type, indices, 0);
}

gl::Error Context9::drawElementsInstanced(const gl::Context *context,
                                          GLenum mode,
                                          GLsizei count,
                                          GLenum type,
                                          const void *indices,
                                          GLsizei instances)
{
    return mRenderer->genericDrawElements(context, mode, count, type, indices, instances);
}

gl::Error Context9::drawRangeElements(const gl::Context *context,
                                      GLenum mode,
                                      GLuint start,
                                      GLuint end,
                                      GLsizei count,
                                      GLenum type,
                                      const void *indices)
{
    return mRenderer->genericDrawElements(context, mode, count, type, indices, 0);
}

gl::Error Context9::drawArraysIndirect(const gl::Context *context,
                                       GLenum mode,
                                       const void *indirect)
{
    UNREACHABLE();
    return gl::InternalError() << "D3D9 doesn't support ES 3.1 DrawArraysIndirect API";
}

gl::Error Context9::drawElementsIndirect(const gl::Context *context,
                                         GLenum mode,
                                         GLenum type,
                                         const void *indirect)
{
    UNREACHABLE();
    return gl::InternalError() << "D3D9 doesn't support ES 3.1 DrawElementsIndirect API";
}

GLenum Context9::getResetStatus()
{
    return mRenderer->getResetStatus();
}

std::string Context9::getVendorString() const
{
    return mRenderer->getVendorString();
}

std::string Context9::getRendererDescription() const
{
    return mRenderer->getRendererDescription();
}

void Context9::insertEventMarker(GLsizei length, const char *marker)
{
    auto optionalString = angle::WidenString(static_cast<size_t>(length), marker);
    if (optionalString.valid())
    {
        mRenderer->getAnnotator()->setMarker(optionalString.value().data());
    }
}

void Context9::pushGroupMarker(GLsizei length, const char *marker)
{
    auto optionalString = angle::WidenString(static_cast<size_t>(length), marker);
    if (optionalString.valid())
    {
        mRenderer->getAnnotator()->beginEvent(optionalString.value().data());
    }
}

void Context9::popGroupMarker()
{
    mRenderer->getAnnotator()->endEvent();
}

void Context9::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message)
{
    // Fall through to the EXT_debug_marker functions
    pushGroupMarker(length, message);
}

void Context9::popDebugGroup()
{
    // Fall through to the EXT_debug_marker functions
    popGroupMarker();
}

void Context9::syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits)
{
    mRenderer->getStateManager()->syncState(mState.getState(), dirtyBits);
}

GLint Context9::getGPUDisjoint()
{
    return mRenderer->getGPUDisjoint();
}

GLint64 Context9::getTimestamp()
{
    return mRenderer->getTimestamp();
}

void Context9::onMakeCurrent(const gl::Context *context)
{
}

const gl::Caps &Context9::getNativeCaps() const
{
    return mRenderer->getNativeCaps();
}

const gl::TextureCapsMap &Context9::getNativeTextureCaps() const
{
    return mRenderer->getNativeTextureCaps();
}

const gl::Extensions &Context9::getNativeExtensions() const
{
    return mRenderer->getNativeExtensions();
}

const gl::Limitations &Context9::getNativeLimitations() const
{
    return mRenderer->getNativeLimitations();
}

gl::Error Context9::dispatchCompute(const gl::Context *context,
                                    GLuint numGroupsX,
                                    GLuint numGroupsY,
                                    GLuint numGroupsZ)
{
    UNREACHABLE();
    return gl::InternalError() << "D3D9 doesn't support ES 3.1 DispatchCompute API";
}

}  // namespace rx
