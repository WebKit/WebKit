//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferGL.h: Defines the class interface for BufferGL.

#ifndef LIBANGLE_RENDERER_GL_BUFFERGL_H_
#define LIBANGLE_RENDERER_GL_BUFFERGL_H_

#include "common/MemoryBuffer.h"
#include "libANGLE/renderer/BufferImpl.h"

namespace rx
{

class FunctionsGL;
class StateManagerGL;

class BufferGL : public BufferImpl
{
  public:
    BufferGL(const gl::BufferState &state,
             const FunctionsGL *functions,
             StateManagerGL *stateManager);
    ~BufferGL() override;

    gl::Error setData(ContextImpl *context,
                      GLenum target,
                      const void *data,
                      size_t size,
                      GLenum usage) override;
    gl::Error setSubData(ContextImpl *context,
                         GLenum target,
                         const void *data,
                         size_t size,
                         size_t offset) override;
    gl::Error copySubData(ContextImpl *contextImpl,
                          BufferImpl *source,
                          GLintptr sourceOffset,
                          GLintptr destOffset,
                          GLsizeiptr size) override;
    gl::Error map(ContextImpl *contextImpl, GLenum access, GLvoid **mapPtr) override;
    gl::Error mapRange(ContextImpl *contextImpl,
                       size_t offset,
                       size_t length,
                       GLbitfield access,
                       GLvoid **mapPtr) override;
    gl::Error unmap(ContextImpl *contextImpl, GLboolean *result) override;

    gl::Error getIndexRange(GLenum type,
                            size_t offset,
                            size_t count,
                            bool primitiveRestartEnabled,
                            gl::IndexRange *outRange) override;

    GLuint getBufferID() const;

  private:
    bool mIsMapped;
    size_t mMapOffset;
    size_t mMapSize;

    bool mShadowBufferData;
    angle::MemoryBuffer mShadowCopy;

    size_t mBufferSize;

    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;

    GLuint mBufferID;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_GL_BUFFERGL_H_
