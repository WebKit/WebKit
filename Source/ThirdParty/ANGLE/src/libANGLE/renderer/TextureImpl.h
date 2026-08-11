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
                                   const gl::OwnImageIndex &index,
                                   GLenum internalFormat,
                                   const gl::Extents &size,
                                   GLenum format,
                                   GLenum type,
                                   const gl::PixelUnpackState &unpack,
                                   gl::Buffer *unpackBuffer,
                                   const uint8_t *pixels)    = 0;
    virtual angle::Result setSubImage(const gl::Context *context,
                                      const gl::OwnImageIndex &index,
                                      const gl::Box &area,
                                      GLenum format,
                                      GLenum type,
                                      const gl::PixelUnpackState &unpack,
                                      gl::Buffer *unpackBuffer,
                                      const uint8_t *pixels) = 0;

    virtual angle::Result setCompressedImage(const gl::Context *context,
                                             const gl::OwnImageIndex &index,
                                             GLenum internalFormat,
                                             const gl::Extents &size,
                                             const gl::PixelUnpackState &unpack,
                                             size_t imageSize,
                                             const uint8_t *pixels)    = 0;
    virtual angle::Result setCompressedSubImage(const gl::Context *context,
                                                const gl::OwnImageIndex &index,
                                                const gl::Box &area,
                                                GLenum format,
                                                const gl::PixelUnpackState &unpack,
                                                size_t imageSize,
                                                const uint8_t *pixels) = 0;

    virtual angle::Result copyImage(const gl::Context *context,
                                    const gl::OwnImageIndex &index,
                                    const gl::Rectangle &sourceArea,
                                    GLenum internalFormat,
                                    gl::Framebuffer *source)    = 0;
    virtual angle::Result copySubImage(const gl::Context *context,
                                       const gl::OwnImageIndex &index,
                                       const gl::Offset &destOffset,
                                       const gl::Rectangle &sourceArea,
                                       gl::Framebuffer *source) = 0;

    virtual angle::Result copyTexture(const gl::Context *context,
                                      const gl::OwnImageIndex &index,
                                      GLenum internalFormat,
                                      GLenum type,
                                      gl::OwnLevel sourceLevel,
                                      bool unpackFlipY,
                                      bool unpackPremultiplyAlpha,
                                      bool unpackUnmultiplyAlpha,
                                      const gl::Texture *source);
    virtual angle::Result copySubTexture(const gl::Context *context,
                                         const gl::OwnImageIndex &index,
                                         const gl::Offset &destOffset,
                                         gl::OwnLevel sourceLevel,
                                         const gl::Box &sourceBox,
                                         bool unpackFlipY,
                                         bool unpackPremultiplyAlpha,
                                         bool unpackUnmultiplyAlpha,
                                         const gl::Texture *source);

    virtual angle::Result copyRenderbufferSubData(const gl::Context *context,
                                                  const gl::Renderbuffer *srcBuffer,
                                                  GLint srcX,
                                                  GLint srcY,
                                                  gl::OwnLevel dstLevel,
                                                  GLint dstX,
                                                  GLint dstY,
                                                  gl::OwnLayer dstZ,
                                                  GLsizei srcWidth,
                                                  GLsizei srcHeight);

    virtual angle::Result copyTextureSubData(const gl::Context *context,
                                             const gl::Texture *srcTexture,
                                             gl::OwnLevel srcLevel,
                                             GLint srcX,
                                             GLint srcY,
                                             gl::OwnLayer srcZ,
                                             gl::OwnLevel dstLevel,
                                             GLint dstX,
                                             GLint dstY,
                                             gl::OwnLayer dstZ,
                                             GLsizei srcWidth,
                                             GLsizei srcHeight,
                                             GLsizei srcDepth);

    virtual angle::Result copyCompressedTexture(const gl::Context *context,
                                                const gl::Texture *source);

    virtual angle::Result copy3DTexture(const gl::Context *context,
                                        gl::TextureTarget target,
                                        GLenum internalFormat,
                                        GLenum type,
                                        gl::OwnLevel sourceLevel,
                                        gl::OwnLevel destLevel,
                                        bool unpackFlipY,
                                        bool unpackPremultiplyAlpha,
                                        bool unpackUnmultiplyAlpha,
                                        const gl::Texture *source);
    virtual angle::Result copy3DSubTexture(const gl::Context *context,
                                           const gl::TextureTarget target,
                                           const gl::Offset &destOffset,
                                           gl::OwnLevel sourceLevel,
                                           gl::OwnLevel destLevel,
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

    virtual angle::Result setStorageAttribs(const gl::Context *context,
                                            gl::TextureType type,
                                            size_t levels,
                                            GLint internalformat,
                                            const gl::Extents &size,
                                            const GLint *attribList);

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

    virtual angle::Result setEGLImageTarget(const gl::Context *context,
                                            gl::TextureType type,
                                            egl::Image *image) = 0;

    virtual angle::Result setImageExternal(const gl::Context *context,
                                           gl::TextureType type,
                                           egl::Stream *stream,
                                           const egl::Stream::GLTextureDescription &desc) = 0;

    virtual angle::Result setBuffer(const gl::Context *context, GLenum internalFormat);

    virtual angle::Result generateMipmap(const gl::Context *context) = 0;

    virtual angle::Result clearImage(const gl::Context *context,
                                     gl::OwnLevel level,
                                     GLenum format,
                                     GLenum type,
                                     const uint8_t *data);
    virtual angle::Result clearSubImage(const gl::Context *context,
                                        gl::OwnLevel level,
                                        const gl::Box &area,
                                        GLenum format,
                                        GLenum type,
                                        const uint8_t *data);

    virtual angle::Result setBaseLevel(const gl::Context *context, GLuint baseLevel) = 0;

    virtual angle::Result bindTexImage(const gl::Context *context, egl::Surface *surface) = 0;
    virtual angle::Result releaseTexImage(const gl::Context *context)                     = 0;

    virtual angle::Result onLabelUpdate(const gl::Context *context);

    // Override if accurate native memory size information is available
    virtual GLint getMemorySize() const;
    virtual GLint getLevelMemorySize(gl::TextureTarget target, gl::OwnLevel level);

    virtual GLint getImageCompressionRate(const gl::Context *context);
    virtual GLint getFormatSupportedCompressionRates(const gl::Context *context,
                                                     GLenum internalformat,
                                                     GLsizei bufSize,
                                                     GLint *rates);

    virtual angle::Result syncState(const gl::Context *context,
                                    const gl::Texture::DirtyBits &dirtyBits,
                                    gl::Command source) = 0;

    virtual GLenum getColorReadFormat(const gl::Context *context);
    virtual GLenum getColorReadType(const gl::Context *context);

    virtual angle::Result getTexImage(const gl::Context *context,
                                      const gl::PixelPackState &packState,
                                      gl::Buffer *packBuffer,
                                      gl::TextureTarget target,
                                      gl::OwnLevel level,
                                      GLenum format,
                                      GLenum type,
                                      void *pixels);

    virtual angle::Result getCompressedTexImage(const gl::Context *context,
                                                const gl::PixelPackState &packState,
                                                gl::Buffer *packBuffer,
                                                gl::TextureTarget target,
                                                gl::OwnLevel level,
                                                void *pixels);

    virtual GLint getRequiredExternalTextureImageUnits(const gl::Context *context);

    const gl::TextureState &getState() const { return mState; }

    void setContentsObservers(gl::TextureBufferContentsObservers *observers)
    {
        mBufferContentsObservers = observers;
    }

  protected:
    const gl::TextureState &mState;
    gl::TextureBufferContentsObservers *mBufferContentsObservers = nullptr;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_TEXTUREIMPL_H_
