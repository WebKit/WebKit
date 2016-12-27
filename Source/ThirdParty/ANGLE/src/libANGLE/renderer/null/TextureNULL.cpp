//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureNULL.cpp:
//    Implements the class methods for TextureNULL.
//

#include "libANGLE/renderer/null/TextureNULL.h"

#include "common/debug.h"

namespace rx
{

TextureNULL::TextureNULL(const gl::TextureState &state) : TextureImpl(state)
{
}

TextureNULL::~TextureNULL()
{
}

gl::Error TextureNULL::setImage(GLenum target,
                                size_t level,
                                GLenum internalFormat,
                                const gl::Extents &size,
                                GLenum format,
                                GLenum type,
                                const gl::PixelUnpackState &unpack,
                                const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error TextureNULL::setSubImage(GLenum target,
                                   size_t level,
                                   const gl::Box &area,
                                   GLenum format,
                                   GLenum type,
                                   const gl::PixelUnpackState &unpack,
                                   const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error TextureNULL::setCompressedImage(GLenum target,
                                          size_t level,
                                          GLenum internalFormat,
                                          const gl::Extents &size,
                                          const gl::PixelUnpackState &unpack,
                                          size_t imageSize,
                                          const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error TextureNULL::setCompressedSubImage(GLenum target,
                                             size_t level,
                                             const gl::Box &area,
                                             GLenum format,
                                             const gl::PixelUnpackState &unpack,
                                             size_t imageSize,
                                             const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error TextureNULL::copyImage(GLenum target,
                                 size_t level,
                                 const gl::Rectangle &sourceArea,
                                 GLenum internalFormat,
                                 const gl::Framebuffer *source)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error TextureNULL::copySubImage(GLenum target,
                                    size_t level,
                                    const gl::Offset &destOffset,
                                    const gl::Rectangle &sourceArea,
                                    const gl::Framebuffer *source)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error TextureNULL::setStorage(GLenum target,
                                  size_t levels,
                                  GLenum internalFormat,
                                  const gl::Extents &size)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error TextureNULL::setEGLImageTarget(GLenum target, egl::Image *image)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error TextureNULL::setImageExternal(GLenum target,
                                        egl::Stream *stream,
                                        const egl::Stream::GLTextureDescription &desc)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error TextureNULL::generateMipmap()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

void TextureNULL::setBaseLevel(GLuint baseLevel)
{
    UNIMPLEMENTED();
}

void TextureNULL::bindTexImage(egl::Surface *surface)
{
    UNIMPLEMENTED();
}

void TextureNULL::releaseTexImage()
{
    UNIMPLEMENTED();
}

void TextureNULL::syncState(const gl::Texture::DirtyBits &dirtyBits)
{
    UNIMPLEMENTED();
}

}  // namespace rx
