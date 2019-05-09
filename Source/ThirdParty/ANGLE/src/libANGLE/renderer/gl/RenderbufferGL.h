//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderbufferGL.h: Defines the class interface for RenderbufferGL.

#ifndef LIBANGLE_RENDERER_GL_RENDERBUFFERGL_H_
#define LIBANGLE_RENDERER_GL_RENDERBUFFERGL_H_

#include "libANGLE/renderer/RenderbufferImpl.h"

namespace gl
{
class TextureCapsMap;
}

namespace rx
{

class BlitGL;
class FunctionsGL;
class StateManagerGL;
struct WorkaroundsGL;

class RenderbufferGL : public RenderbufferImpl
{
  public:
    RenderbufferGL(const gl::RenderbufferState &state,
                   const FunctionsGL *functions,
                   const WorkaroundsGL &workarounds,
                   StateManagerGL *stateManager,
                   BlitGL *blitter,
                   const gl::TextureCapsMap &textureCaps);
    ~RenderbufferGL() override;

    angle::Result setStorage(const gl::Context *context,
                             GLenum internalformat,
                             size_t width,
                             size_t height) override;
    angle::Result setStorageMultisample(const gl::Context *context,
                                        size_t samples,
                                        GLenum internalformat,
                                        size_t width,
                                        size_t height) override;
    angle::Result setStorageEGLImageTarget(const gl::Context *context, egl::Image *image) override;

    angle::Result initializeContents(const gl::Context *context,
                                     const gl::ImageIndex &imageIndex) override;

    GLuint getRenderbufferID() const;
    GLenum getNativeInternalFormat() const;

  private:
    const FunctionsGL *mFunctions;
    const WorkaroundsGL &mWorkarounds;
    StateManagerGL *mStateManager;
    BlitGL *mBlitter;
    const gl::TextureCapsMap &mTextureCaps;

    GLuint mRenderbufferID;

    GLenum mNativeInternalFormat;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_RENDERBUFFERGL_H_
