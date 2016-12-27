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
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/RenderbufferD3D.h"
#include "libANGLE/renderer/d3d/SamplerD3D.h"
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
    return new ShaderD3D(data, mRenderer->getWorkarounds());
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

BufferImpl *Context9::createBuffer()
{
    return new Buffer9(mRenderer);
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

FenceSyncImpl *Context9::createFenceSync()
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

SamplerImpl *Context9::createSampler()
{
    return new SamplerD3D();
}

std::vector<PathImpl *> Context9::createPaths(GLsizei)
{
    return std::vector<PathImpl *>();
}

gl::Error Context9::flush()
{
    return mRenderer->flush();
}

gl::Error Context9::finish()
{
    return mRenderer->finish();
}

gl::Error Context9::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    return mRenderer->genericDrawArrays(this, mode, first, count, 0);
}

gl::Error Context9::drawArraysInstanced(GLenum mode,
                                        GLint first,
                                        GLsizei count,
                                        GLsizei instanceCount)
{
    return mRenderer->genericDrawArrays(this, mode, first, count, instanceCount);
}

gl::Error Context9::drawElements(GLenum mode,
                                 GLsizei count,
                                 GLenum type,
                                 const GLvoid *indices,
                                 const gl::IndexRange &indexRange)
{
    return mRenderer->genericDrawElements(this, mode, count, type, indices, 0, indexRange);
}

gl::Error Context9::drawElementsInstanced(GLenum mode,
                                          GLsizei count,
                                          GLenum type,
                                          const GLvoid *indices,
                                          GLsizei instances,
                                          const gl::IndexRange &indexRange)
{
    return mRenderer->genericDrawElements(this, mode, count, type, indices, instances, indexRange);
}

gl::Error Context9::drawRangeElements(GLenum mode,
                                      GLuint start,
                                      GLuint end,
                                      GLsizei count,
                                      GLenum type,
                                      const GLvoid *indices,
                                      const gl::IndexRange &indexRange)
{
    return mRenderer->genericDrawElements(this, mode, count, type, indices, 0, indexRange);
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

void Context9::syncState(const gl::State &state, const gl::State::DirtyBits &dirtyBits)
{
    mRenderer->getStateManager()->syncState(state, dirtyBits);
}

GLint Context9::getGPUDisjoint()
{
    return mRenderer->getGPUDisjoint();
}

GLint64 Context9::getTimestamp()
{
    return mRenderer->getTimestamp();
}

void Context9::onMakeCurrent(const gl::ContextState &data)
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

}  // namespace rx
