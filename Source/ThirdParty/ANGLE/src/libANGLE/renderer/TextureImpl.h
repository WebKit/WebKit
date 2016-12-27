//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureImpl.h: Defines the abstract rx::TextureImpl classes.

#ifndef LIBANGLE_RENDERER_TEXTUREIMPL_H_
#define LIBANGLE_RENDERER_TEXTUREIMPL_H_

#include <stdint.h>

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/ImageIndex.h"
#include "libANGLE/Stream.h"
#include "libANGLE/Texture.h"
#include "libANGLE/renderer/FramebufferAttachmentObjectImpl.h"

namespace egl
{
class Surface;
class Image;
}

namespace gl
{
struct Box;
struct Extents;
struct Offset;
struct Rectangle;
class Framebuffer;
struct PixelUnpackState;
struct TextureState;
}

namespace rx
{

class TextureImpl : public FramebufferAttachmentObjectImpl
{
  public:
    TextureImpl(const gl::TextureState &state);
    virtual ~TextureImpl();

    virtual gl::Error setImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size, GLenum format, GLenum type,
                               const gl::PixelUnpackState &unpack, const uint8_t *pixels) = 0;
    virtual gl::Error setSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format, GLenum type,
                                  const gl::PixelUnpackState &unpack, const uint8_t *pixels) = 0;

    virtual gl::Error setCompressedImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size,
                                         const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels) = 0;
    virtual gl::Error setCompressedSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format,
                                            const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels) = 0;

    virtual gl::Error copyImage(GLenum target, size_t level, const gl::Rectangle &sourceArea, GLenum internalFormat,
                                const gl::Framebuffer *source) = 0;
    virtual gl::Error copySubImage(GLenum target, size_t level, const gl::Offset &destOffset, const gl::Rectangle &sourceArea,
                                   const gl::Framebuffer *source) = 0;

    virtual gl::Error copyTexture(GLenum internalFormat,
                                  GLenum type,
                                  bool unpackFlipY,
                                  bool unpackPremultiplyAlpha,
                                  bool unpackUnmultiplyAlpha,
                                  const gl::Texture *source);
    virtual gl::Error copySubTexture(const gl::Offset &destOffset,
                                     const gl::Rectangle &sourceArea,
                                     bool unpackFlipY,
                                     bool unpackPremultiplyAlpha,
                                     bool unpackUnmultiplyAlpha,
                                     const gl::Texture *source);

    virtual gl::Error copyCompressedTexture(const gl::Texture *source);

    virtual gl::Error setStorage(GLenum target, size_t levels, GLenum internalFormat, const gl::Extents &size) = 0;

    virtual gl::Error setEGLImageTarget(GLenum target, egl::Image *image) = 0;

    virtual gl::Error setImageExternal(GLenum target,
                                       egl::Stream *stream,
                                       const egl::Stream::GLTextureDescription &desc) = 0;

    virtual gl::Error generateMipmap() = 0;

    virtual void setBaseLevel(GLuint baseLevel) = 0;

    virtual void bindTexImage(egl::Surface *surface) = 0;
    virtual void releaseTexImage() = 0;

    virtual void syncState(const gl::Texture::DirtyBits &dirtyBits) = 0;

  protected:
    const gl::TextureState &mState;
};

}

#endif // LIBANGLE_RENDERER_TEXTUREIMPL_H_
