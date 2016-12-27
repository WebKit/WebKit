//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextVk.cpp:
//    Implements the class methods for ContextVk.
//

#include "libANGLE/renderer/vulkan/ContextVk.h"

#include "common/debug.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/CompilerVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DeviceVk.h"
#include "libANGLE/renderer/vulkan/FenceNVVk.h"
#include "libANGLE/renderer/vulkan/FenceSyncVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ImageVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/QueryVk.h"
#include "libANGLE/renderer/vulkan/RenderbufferVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SamplerVk.h"
#include "libANGLE/renderer/vulkan/ShaderVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"
#include "libANGLE/renderer/vulkan/VertexArrayVk.h"

namespace rx
{

ContextVk::ContextVk(const gl::ContextState &state, RendererVk *renderer)
    : ContextImpl(state), mRenderer(renderer)
{
}

ContextVk::~ContextVk()
{
}

gl::Error ContextVk::initialize()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextVk::flush()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextVk::finish()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextVk::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextVk::drawArraysInstanced(GLenum mode,
                                         GLint first,
                                         GLsizei count,
                                         GLsizei instanceCount)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextVk::drawElements(GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const GLvoid *indices,
                                  const gl::IndexRange &indexRange)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextVk::drawElementsInstanced(GLenum mode,
                                           GLsizei count,
                                           GLenum type,
                                           const GLvoid *indices,
                                           GLsizei instances,
                                           const gl::IndexRange &indexRange)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextVk::drawRangeElements(GLenum mode,
                                       GLuint start,
                                       GLuint end,
                                       GLsizei count,
                                       GLenum type,
                                       const GLvoid *indices,
                                       const gl::IndexRange &indexRange)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

GLenum ContextVk::getResetStatus()
{
    UNIMPLEMENTED();
    return GL_NO_ERROR;
}

std::string ContextVk::getVendorString() const
{
    UNIMPLEMENTED();
    return std::string();
}

std::string ContextVk::getRendererDescription() const
{
    UNIMPLEMENTED();
    return std::string();
}

void ContextVk::insertEventMarker(GLsizei length, const char *marker)
{
    UNIMPLEMENTED();
}

void ContextVk::pushGroupMarker(GLsizei length, const char *marker)
{
    UNIMPLEMENTED();
}

void ContextVk::popGroupMarker()
{
    UNIMPLEMENTED();
}

void ContextVk::syncState(const gl::State &state, const gl::State::DirtyBits &dirtyBits)
{
    UNIMPLEMENTED();
}

GLint ContextVk::getGPUDisjoint()
{
    UNIMPLEMENTED();
    return GLint();
}

GLint64 ContextVk::getTimestamp()
{
    UNIMPLEMENTED();
    return GLint64();
}

void ContextVk::onMakeCurrent(const gl::ContextState &data)
{
    UNIMPLEMENTED();
}

const gl::Caps &ContextVk::getNativeCaps() const
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
    return new FramebufferVk(state);
}

TextureImpl *ContextVk::createTexture(const gl::TextureState &state)
{
    return new TextureVk(state);
}

RenderbufferImpl *ContextVk::createRenderbuffer()
{
    return new RenderbufferVk();
}

BufferImpl *ContextVk::createBuffer()
{
    return new BufferVk();
}

VertexArrayImpl *ContextVk::createVertexArray(const gl::VertexArrayState &state)
{
    return new VertexArrayVk(state);
}

QueryImpl *ContextVk::createQuery(GLenum type)
{
    return new QueryVk(type);
}

FenceNVImpl *ContextVk::createFenceNV()
{
    return new FenceNVVk();
}

FenceSyncImpl *ContextVk::createFenceSync()
{
    return new FenceSyncVk();
}

TransformFeedbackImpl *ContextVk::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedbackVk(state);
}

SamplerImpl *ContextVk::createSampler()
{
    return new SamplerVk();
}

std::vector<PathImpl *> ContextVk::createPaths(GLsizei)
{
    return std::vector<PathImpl *>();
}

}  // namespace rx
