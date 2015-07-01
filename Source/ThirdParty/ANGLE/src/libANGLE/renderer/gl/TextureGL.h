//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureGL.h: Defines the class interface for TextureGL.

#ifndef LIBANGLE_RENDERER_GL_TEXTUREGL_H_
#define LIBANGLE_RENDERER_GL_TEXTUREGL_H_

#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/TextureImpl.h"

namespace rx
{

class FunctionsGL;
class StateManagerGL;

class TextureGL : public TextureImpl
{
  public:
    TextureGL(GLenum type, const FunctionsGL *functions, StateManagerGL *stateManager);
    ~TextureGL() override;

    void setUsage(GLenum usage) override;

    gl::Error setImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size, GLenum format, GLenum type,
                       const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;
    gl::Error setSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format, GLenum type,
                          const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;

    gl::Error setCompressedImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size,
                                 const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;
    gl::Error setCompressedSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format,
                                    const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;

    gl::Error copyImage(GLenum target, size_t level, const gl::Rectangle &sourceArea, GLenum internalFormat,
                        const gl::Framebuffer *source) override;
    gl::Error copySubImage(GLenum target, size_t level, const gl::Offset &destOffset, const gl::Rectangle &sourceArea,
                           const gl::Framebuffer *source) override;

    gl::Error setStorage(GLenum target, size_t levels, GLenum internalFormat, const gl::Extents &size) override;

    gl::Error generateMipmaps(const gl::SamplerState &samplerState) override;

    void bindTexImage(egl::Surface *surface) override;
    void releaseTexImage() override;

    void syncSamplerState(const gl::SamplerState &samplerState) const;
    GLuint getTextureID() const;

    gl::Error getAttachmentRenderTarget(const gl::FramebufferAttachment::Target &target,
                                        FramebufferAttachmentRenderTarget **rtOut) override
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Not supported on OpenGL");
    }

  private:
    GLenum mTextureType;

    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;

    mutable gl::SamplerState mAppliedSamplerState;
    GLuint mTextureID;
};

}

#endif // LIBANGLE_RENDERER_GL_TEXTUREGL_H_
