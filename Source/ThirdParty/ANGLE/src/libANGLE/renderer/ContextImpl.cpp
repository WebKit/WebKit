//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextImpl:
//   Implementation-specific functionality associated with a GL Context.
//

#include "libANGLE/renderer/ContextImpl.h"

namespace rx
{

ContextImpl::ContextImpl(const gl::ContextState &state)
    : mState(state), mMemoryProgramCache(nullptr)
{
}

ContextImpl::~ContextImpl()
{
}

void ContextImpl::stencilFillPath(const gl::Path *path, GLenum fillMode, GLuint mask)
{
    UNREACHABLE();
}

void ContextImpl::stencilStrokePath(const gl::Path *path, GLint reference, GLuint mask)
{
    UNREACHABLE();
}

void ContextImpl::coverFillPath(const gl::Path *path, GLenum coverMode)
{
    UNREACHABLE();
}

void ContextImpl::coverStrokePath(const gl::Path *path, GLenum coverMode)
{
    UNREACHABLE();
}

void ContextImpl::stencilThenCoverFillPath(const gl::Path *path,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum coverMode)
{
    UNREACHABLE();
}

void ContextImpl::stencilThenCoverStrokePath(const gl::Path *path,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum coverMode)
{
    UNREACHABLE();
}

void ContextImpl::coverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                         GLenum coverMode,
                                         GLenum transformType,
                                         const GLfloat *transformValues)
{
    UNREACHABLE();
}

void ContextImpl::coverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                           GLenum coverMode,
                                           GLenum transformType,
                                           const GLfloat *transformValues)
{
    UNREACHABLE();
}

void ContextImpl::stencilFillPathInstanced(const std::vector<gl::Path *> &paths,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum transformType,
                                           const GLfloat *transformValues)
{
    UNREACHABLE();
}

void ContextImpl::stencilStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum transformType,
                                             const GLfloat *transformValues)
{
    UNREACHABLE();
}

void ContextImpl::stencilThenCoverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                                    GLenum coverMode,
                                                    GLenum fillMode,
                                                    GLuint mask,
                                                    GLenum transformType,
                                                    const GLfloat *transformValues)
{
    UNREACHABLE();
}

void ContextImpl::stencilThenCoverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                                      GLenum coverMode,
                                                      GLint reference,
                                                      GLuint mask,
                                                      GLenum transformType,
                                                      const GLfloat *transformValues)
{
    UNREACHABLE();
}

void ContextImpl::setMemoryProgramCache(gl::MemoryProgramCache *memoryProgramCache)
{
    mMemoryProgramCache = memoryProgramCache;
}

}  // namespace rx
