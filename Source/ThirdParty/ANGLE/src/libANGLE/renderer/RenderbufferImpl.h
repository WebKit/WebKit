//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderbufferImpl.h: Defines the abstract class gl::RenderbufferImpl

#ifndef LIBANGLE_RENDERER_RENDERBUFFERIMPL_H_
#define LIBANGLE_RENDERER_RENDERBUFFERIMPL_H_

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/FramebufferAttachmentObjectImpl.h"

namespace gl
{
struct PixelPackState;
class RenderbufferState;
}  // namespace gl

namespace egl
{
class Image;
}  // namespace egl

namespace rx
{

class RenderbufferImpl : public FramebufferAttachmentObjectImpl
{
  public:
    RenderbufferImpl(const gl::RenderbufferState &state) : mState(state) {}
    ~RenderbufferImpl() override {}
    virtual void onDestroy(const gl::Context *context) {}

    virtual angle::Result setStorage(const gl::Context *context,
                                     GLenum internalformat,
                                     size_t width,
                                     size_t height)                   = 0;
    virtual angle::Result setStorageMultisample(const gl::Context *context,
                                                size_t samples,
                                                GLenum internalformat,
                                                size_t width,
                                                size_t height)        = 0;
    virtual angle::Result setStorageEGLImageTarget(const gl::Context *context,
                                                   egl::Image *image) = 0;

    virtual GLenum getColorReadFormat(const gl::Context *context);
    virtual GLenum getColorReadType(const gl::Context *context);

    virtual angle::Result getRenderbufferImage(const gl::Context *context,
                                               const gl::PixelPackState &packState,
                                               gl::Buffer *packBuffer,
                                               GLenum format,
                                               GLenum type,
                                               void *pixels);

    // Override if accurate native memory size information is available
    virtual GLint getMemorySize() const;

  protected:
    const gl::RenderbufferState &mState;
};

inline GLint RenderbufferImpl::getMemorySize() const
{
    return 0;
}

inline GLenum RenderbufferImpl::getColorReadFormat(const gl::Context *context)
{
    UNREACHABLE();
    return GL_NONE;
}

inline GLenum RenderbufferImpl::getColorReadType(const gl::Context *context)
{
    UNREACHABLE();
    return GL_NONE;
}

inline angle::Result RenderbufferImpl::getRenderbufferImage(const gl::Context *context,
                                                            const gl::PixelPackState &packState,
                                                            gl::Buffer *packBuffer,
                                                            GLenum format,
                                                            GLenum type,
                                                            void *pixels)
{
    UNREACHABLE();
    return angle::Result::Stop;
}
}  // namespace rx

#endif  // LIBANGLE_RENDERER_RENDERBUFFERIMPL_H_
