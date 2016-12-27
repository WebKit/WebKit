//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextNULL.cpp:
//    Implements the class methods for ContextNULL.
//

#include "libANGLE/renderer/null/ContextNULL.h"

#include "common/debug.h"

namespace rx
{

ContextNULL::ContextNULL(const gl::ContextState &state) : ContextImpl(state)
{
}

ContextNULL::~ContextNULL()
{
}

gl::Error ContextNULL::initialize()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextNULL::flush()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextNULL::finish()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextNULL::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextNULL::drawArraysInstanced(GLenum mode,
                                           GLint first,
                                           GLsizei count,
                                           GLsizei instanceCount)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextNULL::drawElements(GLenum mode,
                                    GLsizei count,
                                    GLenum type,
                                    const GLvoid *indices,
                                    const gl::IndexRange &indexRange)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextNULL::drawElementsInstanced(GLenum mode,
                                             GLsizei count,
                                             GLenum type,
                                             const GLvoid *indices,
                                             GLsizei instances,
                                             const gl::IndexRange &indexRange)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ContextNULL::drawRangeElements(GLenum mode,
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

CompilerImpl *ContextNULL::createCompiler()
{
    UNIMPLEMENTED();
    return static_cast<CompilerImpl *>(0);
}

ShaderImpl *ContextNULL::createShader(const gl::ShaderState &data)
{
    UNIMPLEMENTED();
    return static_cast<ShaderImpl *>(0);
}

ProgramImpl *ContextNULL::createProgram(const gl::ProgramState &data)
{
    UNIMPLEMENTED();
    return static_cast<ProgramImpl *>(0);
}

FramebufferImpl *ContextNULL::createFramebuffer(const gl::FramebufferState &data)
{
    UNIMPLEMENTED();
    return static_cast<FramebufferImpl *>(0);
}

TextureImpl *ContextNULL::createTexture(const gl::TextureState &state)
{
    UNIMPLEMENTED();
    return static_cast<TextureImpl *>(0);
}

RenderbufferImpl *ContextNULL::createRenderbuffer()
{
    UNIMPLEMENTED();
    return static_cast<RenderbufferImpl *>(0);
}

BufferImpl *ContextNULL::createBuffer()
{
    UNIMPLEMENTED();
    return static_cast<BufferImpl *>(0);
}

VertexArrayImpl *ContextNULL::createVertexArray(const gl::VertexArrayState &data)
{
    UNIMPLEMENTED();
    return static_cast<VertexArrayImpl *>(0);
}

QueryImpl *ContextNULL::createQuery(GLenum type)
{
    UNIMPLEMENTED();
    return static_cast<QueryImpl *>(0);
}

FenceNVImpl *ContextNULL::createFenceNV()
{
    UNIMPLEMENTED();
    return static_cast<FenceNVImpl *>(0);
}

FenceSyncImpl *ContextNULL::createFenceSync()
{
    UNIMPLEMENTED();
    return static_cast<FenceSyncImpl *>(0);
}

TransformFeedbackImpl *ContextNULL::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    UNIMPLEMENTED();
    return static_cast<TransformFeedbackImpl *>(0);
}

SamplerImpl *ContextNULL::createSampler()
{
    UNIMPLEMENTED();
    return static_cast<SamplerImpl *>(0);
}

std::vector<PathImpl *> ContextNULL::createPaths(GLsizei range)
{
    UNIMPLEMENTED();
    return std::vector<PathImpl *>();
}

}  // namespace rx
