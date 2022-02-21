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
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/FramebufferAttachmentObjectImpl.h"

namespace egl
{
class Surface;
class Image;
}  // namespace egl

namespace gl
{
class Framebuffer;
class MemoryObject;
struct PixelUnpackState;
class TextureState;
}  // namespace gl

namespace rx
{
class ContextImpl;

class TextureImpl : public FramebufferAttachmentObjectImpl
{
  public:
    TextureImpl(const gl::TextureState &state);
    ~TextureImpl() override;

    virtual void onDestroy(const gl::Context *context);

    virtual angle::Result setImage(const gl::Context *context,
                                   const gl::ImageIndex &index,
                                   GLenum internalFormat,
                                   const gl::Extents &size,
                                   GLenum format,
                                   GLenum type,
                                   const gl::PixelUnpackState &unpack,
                                   gl::Buffer *unpackBuffer,
                                   const uint8_t *pixels)    = 0;
    virtual angle::Result setSubImage(const gl::Context *context,
                                      const gl::ImageIndex &index,
                                      const gl::Box &area,
                                      GLenum format,
                                      GLenum type,
                                      const gl::PixelUnpackState &unpack,
                                      gl::Buffer *unpackBuffer,
                                      const uint8_t *pixels) = 0;

    virtual angle::Result setCompressedImage(const gl::Context *context,
                                             const gl::ImageIndex &index,
                                             GLenum internalFormat,
                                             const gl::Extents &size,
                                             const gl::PixelUnpackState &unpack,
                                             size_t imageSize,
                                             const uint8_t *pixels)    = 0;
    virtual angle::Result setCompressedSubImage(const gl::Context *context,
                                                const gl::ImageIndex &index,
                                                const gl::Box &area,
                                                GLenum format,
                                                const gl::PixelUnpackState &unpack,
                                                size_t imageSize,
                                                const uint8_t *pixels) = 0;

    virtual angle::Result copyImage(const gl::Context *context,
                                    const gl::ImageIndex &index,
                                    const gl::Rectangle &sourceArea,
                                    GLenum internalFormat,
                                    gl::Framebuffer *source)    = 0;
    virtual angle::Result copySubImage(const gl::Context *context,
                                       const gl::ImageIndex &index,
                                       const gl::Offset &destOffset,
                                       const gl::Rectangle &sourceArea,
                                       gl::Framebuffer *source) = 0;

    virtual angle::Result copyTexture(const gl::Context *context,
                                      const gl::ImageIndex &index,
                                      GLenum internalFormat,
                                      GLenum type,
                                      GLint sourceLevel,
                                      bool unpackFlipY,
                                      bool unpackPremultiplyAlpha,
                                      bool unpackUnmultiplyAlpha,
                                      const gl::Texture *source);
    virtual angle::Result copySubTexture(const gl::Context *context,
                                         const gl::ImageIndex &index,
                                         const gl::Offset &destOffset,
                                         GLint sourceLevel,
                                         const gl::Box &sourceBox,
                                         bool unpackFlipY,
                                         bool unpackPremultiplyAlpha,
                                         bool unpackUnmultiplyAlpha,
                                         const gl::Texture *source);

    virtual angle::Result copyRenderbufferSubData(const gl::Context *context,
                                                  const gl::Renderbuffer *srcBuffer,
                                                  GLint srcLevel,
                                                  GLint srcX,
                                                  GLint srcY,
                                                  GLint srcZ,
                                                  GLint dstLevel,
                                                  GLint dstX,
                                                  GLint dstY,
                                                  GLint dstZ,
                                                  GLsizei srcWidth,
                                                  GLsizei srcHeight,
                                                  GLsizei srcDepth);

    virtual angle::Result copyTextureSubData(const gl::Context *context,
                                             const gl::Texture *srcTexture,
                                             GLint srcLevel,
                                             GLint srcX,
                                             GLint srcY,
                                             GLint srcZ,
                                             GLint dstLevel,
                                             GLint dstX,
                                             GLint dstY,
                                             GLint dstZ,
                                             GLsizei srcWidth,
                                             GLsizei srcHeight,
                                             GLsizei srcDepth);

    virtual angle::Result copyCompressedTexture(const gl::Context *context,
                                                const gl::Texture *source);

    virtual angle::Result copy3DTexture(const gl::Context *context,
                                        gl::TextureTarget target,
                                        GLenum internalFormat,
                                        GLenum type,
                                        GLint sourceLevel,
                                        GLint destLevel,
                                        bool unpackFlipY,
                                        bool unpackPremultiplyAlpha,
                                        bool unpackUnmultiplyAlpha,
                                        const gl::Texture *source);
    virtual angle::Result copy3DSubTexture(const gl::Context *context,
                                           const gl::TextureTarget target,
                                           const gl::Offset &destOffset,
                                           GLint sourceLevel,
                                           GLint destLevel,
                                           const gl::Box &srcBox,
                                           bool unpackFlipY,
                                           bool unpackPremultiplyAlpha,
                                           bool unpackUnmultiplyAlpha,
                                           const gl::Texture *source);

    virtual angle::Result setStorage(const gl::Context *context,
                                     gl::TextureType type,
                                     size_t levels,
                                     GLenum internalFormat,
                                     const gl::Extents &size) = 0;

    virtual angle::Result setStorageMultisample(const gl::Context *context,
                                                gl::TextureType type,
                                                GLsizei samples,
                                                GLint internalformat,
                                                const gl::Extents &size,
                                                bool fixedSampleLocations) = 0;

    virtual angle::Result setStorageExternalMemory(const gl::Context *context,
                                                   gl::TextureType type,
                                                   size_t levels,
                                                   GLenum internalFormat,
                                                   const gl::Extents &size,
                                                   gl::MemoryObject *memoryObject,
                                                   GLuint64 offset,
                                                   GLbitfield createFlags,
                                                   GLbitfield usageFlags,
                                                   const void *imageCreateInfoPNext) = 0;

    virtual angle::Result setImageExternal(const gl::Context *context,
                                           const gl::ImageIndex &index,
                                           GLenum internalFormat,
                                           const gl::Extents &size,
                                           GLenum format,
                                           GLenum type);

    virtual angle::Result setEGLImageTarget(const gl::Context *context,
                                            gl::TextureType type,
                                            egl::Image *image) = 0;

    virtual angle::Result setImageExternal(const gl::Context *context,
                                           gl::TextureType type,
                                           egl::Stream *stream,
                                           const egl::Stream::GLTextureDescription &desc) = 0;

    virtual angle::Result setBuffer(const gl::Context *context, GLenum internalFormat);

    virtual angle::Result generateMipmap(const gl::Context *context) = 0;

    virtual angle::Result setBaseLevel(const gl::Context *context, GLuint baseLevel) = 0;

    virtual angle::Result bindTexImage(const gl::Context *context, egl::Surface *surface) = 0;
    virtual angle::Result releaseTexImage(const gl::Context *context)                     = 0;

    virtual void onLabelUpdate() {}

    // Override if accurate native memory size information is available
    virtual GLint getMemorySize() const;
    virtual GLint getLevelMemorySize(gl::TextureTarget target, GLint level);

    virtual GLint getNativeID() const;

    virtual angle::Result syncState(const gl::Context *context,
                                    const gl::Texture::DirtyBits &dirtyBits,
                                    gl::Command source) = 0;

    virtual GLenum getColorReadFormat(const gl::Context *context);
    virtual GLenum getColorReadType(const gl::Context *context);

    virtual angle::Result getTexImage(const gl::Context *context,
                                      const gl::PixelPackState &packState,
                                      gl::Buffer *packBuffer,
                                      gl::TextureTarget target,
                                      GLint level,
                                      GLenum format,
                                      GLenum type,
                                      void *pixels);

    virtual angle::Result getCompressedTexImage(const gl::Context *context,
                                                const gl::PixelPackState &packState,
                                                gl::Buffer *packBuffer,
                                                gl::TextureTarget target,
                                                GLint level,
                                                void *pixels);

    virtual GLint getRequiredExternalTextureImageUnits(const gl::Context *context);

  protected:
    const gl::TextureState &mState;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_TEXTUREIMPL_H_
