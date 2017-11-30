//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureNULL.h:
//    Defines the class interface for TextureNULL, implementing TextureImpl.
//

#ifndef LIBANGLE_RENDERER_NULL_TEXTURENULL_H_
#define LIBANGLE_RENDERER_NULL_TEXTURENULL_H_

#include "libANGLE/renderer/TextureImpl.h"

namespace rx
{

class TextureNULL : public TextureImpl
{
  public:
    TextureNULL(const gl::TextureState &state);
    ~TextureNULL() override;

    gl::Error setImage(const gl::Context *context,
                       GLenum target,
                       size_t level,
                       GLenum internalFormat,
                       const gl::Extents &size,
                       GLenum format,
                       GLenum type,
                       const gl::PixelUnpackState &unpack,
                       const uint8_t *pixels) override;
    gl::Error setSubImage(const gl::Context *context,
                          GLenum target,
                          size_t level,
                          const gl::Box &area,
                          GLenum format,
                          GLenum type,
                          const gl::PixelUnpackState &unpack,
                          const uint8_t *pixels) override;

    gl::Error setCompressedImage(const gl::Context *context,
                                 GLenum target,
                                 size_t level,
                                 GLenum internalFormat,
                                 const gl::Extents &size,
                                 const gl::PixelUnpackState &unpack,
                                 size_t imageSize,
                                 const uint8_t *pixels) override;
    gl::Error setCompressedSubImage(const gl::Context *context,
                                    GLenum target,
                                    size_t level,
                                    const gl::Box &area,
                                    GLenum format,
                                    const gl::PixelUnpackState &unpack,
                                    size_t imageSize,
                                    const uint8_t *pixels) override;

    gl::Error copyImage(const gl::Context *context,
                        GLenum target,
                        size_t level,
                        const gl::Rectangle &sourceArea,
                        GLenum internalFormat,
                        const gl::Framebuffer *source) override;
    gl::Error copySubImage(const gl::Context *context,
                           GLenum target,
                           size_t level,
                           const gl::Offset &destOffset,
                           const gl::Rectangle &sourceArea,
                           const gl::Framebuffer *source) override;

    gl::Error setStorage(const gl::Context *context,
                         GLenum target,
                         size_t levels,
                         GLenum internalFormat,
                         const gl::Extents &size) override;

    gl::Error setEGLImageTarget(const gl::Context *context,
                                GLenum target,
                                egl::Image *image) override;

    gl::Error setImageExternal(const gl::Context *context,
                               GLenum target,
                               egl::Stream *stream,
                               const egl::Stream::GLTextureDescription &desc) override;

    gl::Error generateMipmap(const gl::Context *context) override;

    gl::Error setBaseLevel(const gl::Context *context, GLuint baseLevel) override;

    gl::Error bindTexImage(const gl::Context *context, egl::Surface *surface) override;
    gl::Error releaseTexImage(const gl::Context *context) override;

    void syncState(const gl::Texture::DirtyBits &dirtyBits) override;

    gl::Error setStorageMultisample(const gl::Context *context,
                                    GLenum target,
                                    GLsizei samples,
                                    GLint internalformat,
                                    const gl::Extents &size,
                                    bool fixedSampleLocations) override;

    gl::Error initializeContents(const gl::Context *context,
                                 const gl::ImageIndex &imageIndex) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_TEXTURENULL_H_
