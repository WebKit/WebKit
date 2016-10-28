//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureD3D.h: Implementations of the Texture interfaces shared betweeen the D3D backends.

#ifndef LIBANGLE_RENDERER_D3D_TEXTURED3D_H_
#define LIBANGLE_RENDERER_D3D_TEXTURED3D_H_

#include "libANGLE/renderer/TextureImpl.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Stream.h"

namespace gl
{
class Framebuffer;
}

namespace rx
{
class EGLImageD3D;
class ImageD3D;
class RendererD3D;
class RenderTargetD3D;
class TextureStorage;

class TextureD3D : public TextureImpl
{
  public:
    TextureD3D(const gl::TextureState &data, RendererD3D *renderer);
    virtual ~TextureD3D();

    gl::Error getNativeTexture(TextureStorage **outStorage);

    bool hasDirtyImages() const { return mDirtyImages; }
    void resetDirty() { mDirtyImages = false; }

    virtual ImageD3D *getImage(const gl::ImageIndex &index) const = 0;
    virtual GLsizei getLayerCount(int level) const = 0;

    GLint getBaseLevelWidth() const;
    GLint getBaseLevelHeight() const;
    GLenum getBaseLevelInternalFormat() const;

    bool isImmutable() const { return mImmutable; }

    virtual gl::Error getRenderTarget(const gl::ImageIndex &index, RenderTargetD3D **outRT) = 0;

    // Returns an iterator over all "Images" for this particular Texture.
    virtual gl::ImageIndexIterator imageIterator() const = 0;

    // Returns an ImageIndex for a particular "Image". 3D Textures do not have images for
    // slices of their depth texures, so 3D textures ignore the layer parameter.
    virtual gl::ImageIndex getImageIndex(GLint mip, GLint layer) const = 0;
    virtual bool isValidIndex(const gl::ImageIndex &index) const = 0;

    virtual gl::Error setImageExternal(GLenum target,
                                       egl::Stream *stream,
                                       const egl::Stream::GLTextureDescription &desc) override;
    gl::Error generateMipmap() override;
    TextureStorage *getStorage();
    ImageD3D *getBaseLevelImage() const;

    gl::Error getAttachmentRenderTarget(const gl::FramebufferAttachment::Target &target,
                                        FramebufferAttachmentRenderTarget **rtOut) override;

    void setBaseLevel(GLuint baseLevel) override;

    void syncState(const gl::Texture::DirtyBits &dirtyBits) override;

  protected:
    gl::Error setImageImpl(const gl::ImageIndex &index,
                           GLenum type,
                           const gl::PixelUnpackState &unpack,
                           const uint8_t *pixels,
                           ptrdiff_t layerOffset);
    gl::Error subImage(const gl::ImageIndex &index, const gl::Box &area, GLenum format, GLenum type,
                       const gl::PixelUnpackState &unpack, const uint8_t *pixels, ptrdiff_t layerOffset);
    gl::Error setCompressedImageImpl(const gl::ImageIndex &index,
                                     const gl::PixelUnpackState &unpack,
                                     const uint8_t *pixels,
                                     ptrdiff_t layerOffset);
    gl::Error subImageCompressed(const gl::ImageIndex &index, const gl::Box &area, GLenum format,
                                 const gl::PixelUnpackState &unpack, const uint8_t *pixels, ptrdiff_t layerOffset);
    bool isFastUnpackable(const gl::PixelUnpackState &unpack, GLenum sizedInternalFormat);
    gl::Error fastUnpackPixels(const gl::PixelUnpackState &unpack, const uint8_t *pixels, const gl::Box &destArea,
                               GLenum sizedInternalFormat, GLenum type, RenderTargetD3D *destRenderTarget);

    GLint getLevelZeroWidth() const;
    GLint getLevelZeroHeight() const;
    virtual GLint getLevelZeroDepth() const;

    GLint creationLevels(GLsizei width, GLsizei height, GLsizei depth) const;
    virtual void initMipmapImages() = 0;
    bool isBaseImageZeroSize() const;
    virtual bool isImageComplete(const gl::ImageIndex &index) const = 0;

    bool canCreateRenderTargetForImage(const gl::ImageIndex &index) const;
    virtual gl::Error ensureRenderTarget();

    virtual gl::Error createCompleteStorage(bool renderTarget, TextureStorage **outTexStorage) const = 0;
    virtual gl::Error setCompleteTexStorage(TextureStorage *newCompleteTexStorage) = 0;
    gl::Error commitRegion(const gl::ImageIndex &index, const gl::Box &region);

    GLuint getBaseLevel() const { return mBaseLevel; };

    virtual void markAllImagesDirty() = 0;

    GLint getBaseLevelDepth() const;

    RendererD3D *mRenderer;

    bool mDirtyImages;

    bool mImmutable;
    TextureStorage *mTexStorage;

  private:
    virtual gl::Error initializeStorage(bool renderTarget) = 0;

    virtual gl::Error updateStorage() = 0;

    bool shouldUseSetData(const ImageD3D *image) const;

    gl::Error generateMipmapUsingImages(const GLuint maxLevel);

    GLuint mBaseLevel;
};

class TextureD3D_2D : public TextureD3D
{
  public:
    TextureD3D_2D(const gl::TextureState &data, RendererD3D *renderer);
    virtual ~TextureD3D_2D();

    virtual ImageD3D *getImage(int level, int layer) const;
    virtual ImageD3D *getImage(const gl::ImageIndex &index) const;
    virtual GLsizei getLayerCount(int level) const;

    GLsizei getWidth(GLint level) const;
    GLsizei getHeight(GLint level) const;
    GLenum getInternalFormat(GLint level) const;
    bool isDepth(GLint level) const;

    gl::Error setImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size, GLenum format, GLenum type,
                       const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;
    gl::Error setSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format, GLenum type,
                          const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;

    gl::Error setCompressedImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size,
                                 const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels) override;
    gl::Error setCompressedSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format,
                                    const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels) override;

    gl::Error copyImage(GLenum target, size_t level, const gl::Rectangle &sourceArea, GLenum internalFormat,
                        const gl::Framebuffer *source) override;
    gl::Error copySubImage(GLenum target, size_t level, const gl::Offset &destOffset, const gl::Rectangle &sourceArea,
                           const gl::Framebuffer *source) override;

    gl::Error copyTexture(GLenum internalFormat,
                          GLenum type,
                          bool unpackFlipY,
                          bool unpackPremultiplyAlpha,
                          bool unpackUnmultiplyAlpha,
                          const gl::Texture *source) override;
    gl::Error copySubTexture(const gl::Offset &destOffset,
                             const gl::Rectangle &sourceArea,
                             bool unpackFlipY,
                             bool unpackPremultiplyAlpha,
                             bool unpackUnmultiplyAlpha,
                             const gl::Texture *source) override;
    gl::Error copyCompressedTexture(const gl::Texture *source) override;

    gl::Error setStorage(GLenum target, size_t levels, GLenum internalFormat, const gl::Extents &size) override;

    virtual void bindTexImage(egl::Surface *surface);
    virtual void releaseTexImage();

    gl::Error setEGLImageTarget(GLenum target, egl::Image *image) override;

    virtual gl::Error getRenderTarget(const gl::ImageIndex &index, RenderTargetD3D **outRT);

    virtual gl::ImageIndexIterator imageIterator() const;
    virtual gl::ImageIndex getImageIndex(GLint mip, GLint layer) const;
    virtual bool isValidIndex(const gl::ImageIndex &index) const;

  protected:
    void markAllImagesDirty() override;

  private:
    virtual gl::Error initializeStorage(bool renderTarget);
    virtual gl::Error createCompleteStorage(bool renderTarget, TextureStorage **outTexStorage) const;
    virtual gl::Error setCompleteTexStorage(TextureStorage *newCompleteTexStorage);

    virtual gl::Error updateStorage();
    virtual void initMipmapImages();

    bool isValidLevel(int level) const;
    bool isLevelComplete(int level) const;
    virtual bool isImageComplete(const gl::ImageIndex &index) const;

    gl::Error updateStorageLevel(int level);

    void redefineImage(size_t level,
                       GLenum internalformat,
                       const gl::Extents &size,
                       bool forceRelease);

    bool mEGLImageTarget;
    ImageD3D *mImageArray[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
};

class TextureD3D_Cube : public TextureD3D
{
  public:
    TextureD3D_Cube(const gl::TextureState &data, RendererD3D *renderer);
    virtual ~TextureD3D_Cube();

    virtual ImageD3D *getImage(int level, int layer) const;
    virtual ImageD3D *getImage(const gl::ImageIndex &index) const;
    virtual GLsizei getLayerCount(int level) const;

    virtual bool hasDirtyImages() const { return mDirtyImages; }
    virtual void resetDirty() { mDirtyImages = false; }

    GLenum getInternalFormat(GLint level, GLint layer) const;
    bool isDepth(GLint level, GLint layer) const;

    gl::Error setImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size, GLenum format, GLenum type,
                       const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;
    gl::Error setSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format, GLenum type,
                          const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;

    gl::Error setCompressedImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size,
                                 const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels) override;
    gl::Error setCompressedSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format,
                                    const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels) override;

    gl::Error copyImage(GLenum target, size_t level, const gl::Rectangle &sourceArea, GLenum internalFormat,
                        const gl::Framebuffer *source) override;
    gl::Error copySubImage(GLenum target, size_t level, const gl::Offset &destOffset, const gl::Rectangle &sourceArea,
                           const gl::Framebuffer *source) override;

    gl::Error setStorage(GLenum target, size_t levels, GLenum internalFormat, const gl::Extents &size) override;

    virtual void bindTexImage(egl::Surface *surface);
    virtual void releaseTexImage();

    gl::Error setEGLImageTarget(GLenum target, egl::Image *image) override;

    virtual gl::Error getRenderTarget(const gl::ImageIndex &index, RenderTargetD3D **outRT);

    virtual gl::ImageIndexIterator imageIterator() const;
    virtual gl::ImageIndex getImageIndex(GLint mip, GLint layer) const;
    virtual bool isValidIndex(const gl::ImageIndex &index) const;

  protected:
    void markAllImagesDirty() override;

  private:
    virtual gl::Error initializeStorage(bool renderTarget);
    virtual gl::Error createCompleteStorage(bool renderTarget, TextureStorage **outTexStorage) const;
    virtual gl::Error setCompleteTexStorage(TextureStorage *newCompleteTexStorage);

    virtual gl::Error updateStorage();
    void initMipmapImages() override;

    bool isValidFaceLevel(int faceIndex, int level) const;
    bool isFaceLevelComplete(int faceIndex, int level) const;
    bool isCubeComplete() const;
    virtual bool isImageComplete(const gl::ImageIndex &index) const;
    gl::Error updateStorageFaceLevel(int faceIndex, int level);

    void redefineImage(int faceIndex, GLint level, GLenum internalformat, const gl::Extents &size);

    ImageD3D *mImageArray[6][gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
};

class TextureD3D_3D : public TextureD3D
{
  public:
    TextureD3D_3D(const gl::TextureState &data, RendererD3D *renderer);
    virtual ~TextureD3D_3D();

    virtual ImageD3D *getImage(int level, int layer) const;
    virtual ImageD3D *getImage(const gl::ImageIndex &index) const;
    virtual GLsizei getLayerCount(int level) const;

    GLsizei getWidth(GLint level) const;
    GLsizei getHeight(GLint level) const;
    GLsizei getDepth(GLint level) const;
    GLenum getInternalFormat(GLint level) const;
    bool isDepth(GLint level) const;

    gl::Error setImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size, GLenum format, GLenum type,
                       const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;
    gl::Error setSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format, GLenum type,
                          const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;

    gl::Error setCompressedImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size,
                                 const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels) override;
    gl::Error setCompressedSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format,
                                    const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels) override;

    gl::Error copyImage(GLenum target, size_t level, const gl::Rectangle &sourceArea, GLenum internalFormat,
                        const gl::Framebuffer *source) override;
    gl::Error copySubImage(GLenum target, size_t level, const gl::Offset &destOffset, const gl::Rectangle &sourceArea,
                           const gl::Framebuffer *source) override;

    gl::Error setStorage(GLenum target, size_t levels, GLenum internalFormat, const gl::Extents &size) override;

    virtual void bindTexImage(egl::Surface *surface);
    virtual void releaseTexImage();

    gl::Error setEGLImageTarget(GLenum target, egl::Image *image) override;

    virtual gl::Error getRenderTarget(const gl::ImageIndex &index, RenderTargetD3D **outRT);

    virtual gl::ImageIndexIterator imageIterator() const;
    virtual gl::ImageIndex getImageIndex(GLint mip, GLint layer) const;
    virtual bool isValidIndex(const gl::ImageIndex &index) const;

  protected:
    void markAllImagesDirty() override;
    GLint getLevelZeroDepth() const override;

  private:
    virtual gl::Error initializeStorage(bool renderTarget);
    virtual gl::Error createCompleteStorage(bool renderTarget, TextureStorage **outStorage) const;
    virtual gl::Error setCompleteTexStorage(TextureStorage *newCompleteTexStorage);

    virtual gl::Error updateStorage();
    void initMipmapImages() override;

    bool isValidLevel(int level) const;
    bool isLevelComplete(int level) const;
    virtual bool isImageComplete(const gl::ImageIndex &index) const;
    gl::Error updateStorageLevel(int level);

    void redefineImage(GLint level, GLenum internalformat, const gl::Extents &size);

    ImageD3D *mImageArray[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
};

class TextureD3D_2DArray : public TextureD3D
{
  public:
    TextureD3D_2DArray(const gl::TextureState &data, RendererD3D *renderer);
    virtual ~TextureD3D_2DArray();

    virtual ImageD3D *getImage(int level, int layer) const;
    virtual ImageD3D *getImage(const gl::ImageIndex &index) const;
    virtual GLsizei getLayerCount(int level) const;

    GLsizei getWidth(GLint level) const;
    GLsizei getHeight(GLint level) const;
    GLenum getInternalFormat(GLint level) const;
    bool isDepth(GLint level) const;

    gl::Error setImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size, GLenum format, GLenum type,
                       const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;
    gl::Error setSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format, GLenum type,
                          const gl::PixelUnpackState &unpack, const uint8_t *pixels) override;

    gl::Error setCompressedImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size,
                                 const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels) override;
    gl::Error setCompressedSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format,
                                    const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels) override;

    gl::Error copyImage(GLenum target, size_t level, const gl::Rectangle &sourceArea, GLenum internalFormat,
                        const gl::Framebuffer *source) override;
    gl::Error copySubImage(GLenum target, size_t level, const gl::Offset &destOffset, const gl::Rectangle &sourceArea,
                           const gl::Framebuffer *source) override;

    gl::Error setStorage(GLenum target, size_t levels, GLenum internalFormat, const gl::Extents &size) override;

    virtual void bindTexImage(egl::Surface *surface);
    virtual void releaseTexImage();

    gl::Error setEGLImageTarget(GLenum target, egl::Image *image) override;

    virtual gl::Error getRenderTarget(const gl::ImageIndex &index, RenderTargetD3D **outRT);

    virtual gl::ImageIndexIterator imageIterator() const;
    virtual gl::ImageIndex getImageIndex(GLint mip, GLint layer) const;
    virtual bool isValidIndex(const gl::ImageIndex &index) const;

  protected:
    void markAllImagesDirty() override;

  private:
    virtual gl::Error initializeStorage(bool renderTarget);
    virtual gl::Error createCompleteStorage(bool renderTarget, TextureStorage **outStorage) const;
    virtual gl::Error setCompleteTexStorage(TextureStorage *newCompleteTexStorage);

    virtual gl::Error updateStorage();
    void initMipmapImages() override;

    bool isValidLevel(int level) const;
    bool isLevelComplete(int level) const;
    virtual bool isImageComplete(const gl::ImageIndex &index) const;
    gl::Error updateStorageLevel(int level);

    void deleteImages();
    void redefineImage(GLint level, GLenum internalformat, const gl::Extents &size);

    // Storing images as an array of single depth textures since D3D11 treats each array level of a
    // Texture2D object as a separate subresource.  Each layer would have to be looped over
    // to update all the texture layers since they cannot all be updated at once and it makes the most
    // sense for the Image class to not have to worry about layer subresource as well as mip subresources.
    GLsizei mLayerCounts[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
    ImageD3D **mImageArray[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
};

class TextureD3D_External : public TextureD3D
{
  public:
    TextureD3D_External(const gl::TextureState &data, RendererD3D *renderer);
    ~TextureD3D_External() override;

    ImageD3D *getImage(const gl::ImageIndex &index) const override;
    GLsizei getLayerCount(int level) const override;

    gl::Error setImage(GLenum target,
                       size_t level,
                       GLenum internalFormat,
                       const gl::Extents &size,
                       GLenum format,
                       GLenum type,
                       const gl::PixelUnpackState &unpack,
                       const uint8_t *pixels) override;
    gl::Error setSubImage(GLenum target,
                          size_t level,
                          const gl::Box &area,
                          GLenum format,
                          GLenum type,
                          const gl::PixelUnpackState &unpack,
                          const uint8_t *pixels) override;

    gl::Error setCompressedImage(GLenum target,
                                 size_t level,
                                 GLenum internalFormat,
                                 const gl::Extents &size,
                                 const gl::PixelUnpackState &unpack,
                                 size_t imageSize,
                                 const uint8_t *pixels) override;
    gl::Error setCompressedSubImage(GLenum target,
                                    size_t level,
                                    const gl::Box &area,
                                    GLenum format,
                                    const gl::PixelUnpackState &unpack,
                                    size_t imageSize,
                                    const uint8_t *pixels) override;

    gl::Error copyImage(GLenum target,
                        size_t level,
                        const gl::Rectangle &sourceArea,
                        GLenum internalFormat,
                        const gl::Framebuffer *source) override;
    gl::Error copySubImage(GLenum target,
                           size_t level,
                           const gl::Offset &destOffset,
                           const gl::Rectangle &sourceArea,
                           const gl::Framebuffer *source) override;

    gl::Error setStorage(GLenum target,
                         size_t levels,
                         GLenum internalFormat,
                         const gl::Extents &size) override;

    gl::Error setImageExternal(GLenum target,
                               egl::Stream *stream,
                               const egl::Stream::GLTextureDescription &desc) override;

    void bindTexImage(egl::Surface *surface) override;
    void releaseTexImage() override;

    gl::Error setEGLImageTarget(GLenum target, egl::Image *image) override;

    gl::Error getRenderTarget(const gl::ImageIndex &index, RenderTargetD3D **outRT) override;

    gl::ImageIndexIterator imageIterator() const override;
    gl::ImageIndex getImageIndex(GLint mip, GLint layer) const override;
    bool isValidIndex(const gl::ImageIndex &index) const override;

  protected:
    void markAllImagesDirty() override;

  private:
    gl::Error initializeStorage(bool renderTarget) override;
    gl::Error createCompleteStorage(bool renderTarget,
                                    TextureStorage **outTexStorage) const override;
    gl::Error setCompleteTexStorage(TextureStorage *newCompleteTexStorage) override;

    gl::Error updateStorage() override;
    void initMipmapImages() override;

    bool isImageComplete(const gl::ImageIndex &index) const override;
};
}

#endif // LIBANGLE_RENDERER_D3D_TEXTURED3D_H_
