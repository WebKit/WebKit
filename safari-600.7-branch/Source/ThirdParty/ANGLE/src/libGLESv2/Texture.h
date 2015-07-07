//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Texture.h: Defines the abstract gl::Texture class and its concrete derived
// classes Texture2D and TextureCubeMap. Implements GL texture objects and
// related functionality. [OpenGL ES 2.0.24] section 3.7 page 63.

#ifndef LIBGLESV2_TEXTURE_H_
#define LIBGLESV2_TEXTURE_H_

#include <vector>

#define GL_APICALL
#include <GLES3/gl3.h>
#include <GLES2/gl2.h>

#include "common/debug.h"
#include "common/RefCountObject.h"
#include "libGLESv2/angletypes.h"
#include "libGLESv2/RenderbufferProxySet.h"

namespace egl
{
class Surface;
}

namespace rx
{
class Renderer;
class TextureStorageInterface;
class TextureStorageInterface2D;
class TextureStorageInterfaceCube;
class TextureStorageInterface3D;
class TextureStorageInterface2DArray;
class RenderTarget;
class Image;
}

namespace gl
{
class Framebuffer;
class Renderbuffer;

enum
{
    // These are the maximums the implementation can support
    // The actual GL caps are limited by the device caps
    // and should be queried from the Context
    IMPLEMENTATION_MAX_2D_TEXTURE_SIZE = 16384,
    IMPLEMENTATION_MAX_CUBE_MAP_TEXTURE_SIZE = 16384,
    IMPLEMENTATION_MAX_3D_TEXTURE_SIZE = 2048,
    IMPLEMENTATION_MAX_2D_ARRAY_TEXTURE_LAYERS = 2048,

    IMPLEMENTATION_MAX_TEXTURE_LEVELS = 15   // 1+log2 of MAX_TEXTURE_SIZE
};

bool IsMipmapFiltered(const SamplerState &samplerState);

class Texture : public RefCountObject
{
  public:
    Texture(rx::Renderer *renderer, GLuint id, GLenum target);

    virtual ~Texture();

    void addProxyRef(const Renderbuffer *proxy);
    void releaseProxy(const Renderbuffer *proxy);

    GLenum getTarget() const;

    void setMinFilter(GLenum filter);
    void setMagFilter(GLenum filter);
    void setWrapS(GLenum wrap);
    void setWrapT(GLenum wrap);
    void setWrapR(GLenum wrap);
    void setMaxAnisotropy(float textureMaxAnisotropy, float contextMaxAnisotropy);
    void setCompareMode(GLenum mode);
    void setCompareFunc(GLenum func);
    void setSwizzleRed(GLenum swizzle);
    void setSwizzleGreen(GLenum swizzle);
    void setSwizzleBlue(GLenum swizzle);
    void setSwizzleAlpha(GLenum swizzle);
    void setBaseLevel(GLint baseLevel);
    void setMaxLevel(GLint maxLevel);
    void setMinLod(GLfloat minLod);
    void setMaxLod(GLfloat maxLod);
    void setUsage(GLenum usage);

    GLenum getMinFilter() const;
    GLenum getMagFilter() const;
    GLenum getWrapS() const;
    GLenum getWrapT() const;
    GLenum getWrapR() const;
    float getMaxAnisotropy() const;
    GLenum getSwizzleRed() const;
    GLenum getSwizzleGreen() const;
    GLenum getSwizzleBlue() const;
    GLenum getSwizzleAlpha() const;
    GLint getBaseLevel() const;
    GLint getMaxLevel() const;
    GLfloat getMinLod() const;
    GLfloat getMaxLod() const;
    bool isSwizzled() const;
    void getSamplerState(SamplerState *sampler);
    GLenum getUsage() const;

    GLint getBaseLevelWidth() const;
    GLint getBaseLevelHeight() const;
    GLint getBaseLevelDepth() const;
    GLenum getBaseLevelInternalFormat() const;

    virtual bool isSamplerComplete(const SamplerState &samplerState) const = 0;

    rx::TextureStorageInterface *getNativeTexture();

    virtual void generateMipmaps() = 0;
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source) = 0;

    bool hasDirtyParameters() const;
    bool hasDirtyImages() const;
    void resetDirty();
    unsigned int getTextureSerial();

    bool isImmutable() const;
    int immutableLevelCount();

    static const GLuint INCOMPLETE_TEXTURE_ID = static_cast<GLuint>(-1);   // Every texture takes an id at creation time. The value is arbitrary because it is never registered with the resource manager.

  protected:
    void setImage(const PixelUnpackState &unpack, GLenum type, const void *pixels, rx::Image *image);
    bool subImage(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                  GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels, rx::Image *image);
    void setCompressedImage(GLsizei imageSize, const void *pixels, rx::Image *image);
    bool subImageCompressed(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                            GLenum format, GLsizei imageSize, const void *pixels, rx::Image *image);
    bool isFastUnpackable(const PixelUnpackState &unpack, GLenum sizedInternalFormat);
    bool fastUnpackPixels(const PixelUnpackState &unpack, const void *pixels, const Box &destArea,
                          GLenum sizedInternalFormat, GLenum type, rx::RenderTarget *destRenderTarget);

    GLint creationLevels(GLsizei width, GLsizei height, GLsizei depth) const;
    int mipLevels() const;

    virtual void initializeStorage(bool renderTarget) = 0;
    virtual void updateStorage() = 0;
    virtual bool ensureRenderTarget() = 0;

    rx::Renderer *mRenderer;

    SamplerState mSamplerState;
    GLenum mUsage;

    bool mDirtyImages;

    bool mImmutable;

    GLenum mTarget;

    // A specific internal reference count is kept for colorbuffer proxy references,
    // because, as the renderbuffer acting as proxy will maintain a binding pointer
    // back to this texture, there would be a circular reference if we used a binding
    // pointer here. This reference count will cause the pointer to be set to NULL if
    // the count drops to zero, but will not cause deletion of the Renderbuffer.
    RenderbufferProxySet mRenderbufferProxies;

  private:
    DISALLOW_COPY_AND_ASSIGN(Texture);

    virtual rx::TextureStorageInterface *getBaseLevelStorage() = 0;
    virtual const rx::Image *getBaseLevelImage() const = 0;
};

class Texture2D : public Texture
{
  public:
    Texture2D(rx::Renderer *renderer, GLuint id);

    ~Texture2D();

    GLsizei getWidth(GLint level) const;
    GLsizei getHeight(GLint level) const;
    GLenum getInternalFormat(GLint level) const;
    GLenum getActualFormat(GLint level) const;
    bool isCompressed(GLint level) const;
    bool isDepth(GLint level) const;

    void setImage(GLint level, GLsizei width, GLsizei height, GLenum internalFormat, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void setCompressedImage(GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels);
    void subImage(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void subImageCompressed(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels);
    void copyImage(GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);
    void storage(GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);

    virtual bool isSamplerComplete(const SamplerState &samplerState) const;
    virtual void bindTexImage(egl::Surface *surface);
    virtual void releaseTexImage();

    virtual void generateMipmaps();

    Renderbuffer *getRenderbuffer(GLint level);
    unsigned int getRenderTargetSerial(GLint level);

  protected:
    friend class RenderbufferTexture2D;
    rx::RenderTarget *getRenderTarget(GLint level);
    rx::RenderTarget *getDepthSencil(GLint level);

  private:
    DISALLOW_COPY_AND_ASSIGN(Texture2D);

    virtual void initializeStorage(bool renderTarget);
    rx::TextureStorageInterface2D *createCompleteStorage(bool renderTarget) const;
    void setCompleteTexStorage(rx::TextureStorageInterface2D *newCompleteTexStorage);

    virtual void updateStorage();
    virtual bool ensureRenderTarget();
    virtual rx::TextureStorageInterface *getBaseLevelStorage();
    virtual const rx::Image *getBaseLevelImage() const;

    bool isMipmapComplete() const;
    bool isValidLevel(int level) const;
    bool isLevelComplete(int level) const;
    void updateStorageLevel(int level);

    void redefineImage(GLint level, GLenum internalformat, GLsizei width, GLsizei height);
    void commitRect(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);

    rx::Image *mImageArray[IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    rx::TextureStorageInterface2D *mTexStorage;
    egl::Surface *mSurface;
};

class TextureCubeMap : public Texture
{
  public:
    TextureCubeMap(rx::Renderer *renderer, GLuint id);

    ~TextureCubeMap();

    GLsizei getWidth(GLenum target, GLint level) const;
    GLsizei getHeight(GLenum target, GLint level) const;
    GLenum getInternalFormat(GLenum target, GLint level) const;
    GLenum getActualFormat(GLenum target, GLint level) const;
    bool isCompressed(GLenum target, GLint level) const;
    bool isDepth(GLenum target, GLint level) const;

    void setImagePosX(GLint level, GLsizei width, GLsizei height, GLenum internalFormat, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void setImageNegX(GLint level, GLsizei width, GLsizei height, GLenum internalFormat, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void setImagePosY(GLint level, GLsizei width, GLsizei height, GLenum internalFormat, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void setImageNegY(GLint level, GLsizei width, GLsizei height, GLenum internalFormat, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void setImagePosZ(GLint level, GLsizei width, GLsizei height, GLenum internalFormat, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void setImageNegZ(GLint level, GLsizei width, GLsizei height, GLenum internalFormat, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);

    void setCompressedImage(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels);

    void subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels);
    void copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);
    void storage(GLsizei levels, GLenum internalformat, GLsizei size);

    virtual bool isSamplerComplete(const SamplerState &samplerState) const;
    bool isCubeComplete() const;

    virtual void generateMipmaps();

    Renderbuffer *getRenderbuffer(GLenum target, GLint level);
    unsigned int getRenderTargetSerial(GLenum target, GLint level);

    static int targetToIndex(GLenum target);

  protected:
    friend class RenderbufferTextureCubeMap;
    rx::RenderTarget *getRenderTarget(GLenum target, GLint level);
    rx::RenderTarget *getDepthStencil(GLenum target, GLint level);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureCubeMap);

    virtual void initializeStorage(bool renderTarget);
    rx::TextureStorageInterfaceCube *createCompleteStorage(bool renderTarget) const;
    void setCompleteTexStorage(rx::TextureStorageInterfaceCube *newCompleteTexStorage);

    virtual void updateStorage();
    virtual bool ensureRenderTarget();
    virtual rx::TextureStorageInterface *getBaseLevelStorage();
    virtual const rx::Image *getBaseLevelImage() const;

    bool isMipmapCubeComplete() const;
    bool isValidFaceLevel(int faceIndex, int level) const;
    bool isFaceLevelComplete(int faceIndex, int level) const;
    void updateStorageFaceLevel(int faceIndex, int level);

    void setImage(int faceIndex, GLint level, GLsizei width, GLsizei height, GLenum internalFormat, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void commitRect(int faceIndex, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);
    void redefineImage(int faceIndex, GLint level, GLenum internalformat, GLsizei width, GLsizei height);

    rx::Image *mImageArray[6][IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    rx::TextureStorageInterfaceCube *mTexStorage;
};

class Texture3D : public Texture
{
  public:
    Texture3D(rx::Renderer *renderer, GLuint id);

    ~Texture3D();

    GLsizei getWidth(GLint level) const;
    GLsizei getHeight(GLint level) const;
    GLsizei getDepth(GLint level) const;
    GLenum getInternalFormat(GLint level) const;
    GLenum getActualFormat(GLint level) const;
    bool isCompressed(GLint level) const;
    bool isDepth(GLint level) const;

    void setImage(GLint level, GLsizei width, GLsizei height, GLsizei depth, GLenum internalFormat, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void setCompressedImage(GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLsizei imageSize, const void *pixels);
    void subImage(GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void subImageCompressed(GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *pixels);
    void storage(GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

    virtual void generateMipmaps();
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);

    virtual bool isSamplerComplete(const SamplerState &samplerState) const;
    virtual bool isMipmapComplete() const;

    Renderbuffer *getRenderbuffer(GLint level, GLint layer);
    unsigned int getRenderTargetSerial(GLint level, GLint layer);

  protected:
    friend class RenderbufferTexture3DLayer;
    rx::RenderTarget *getRenderTarget(GLint level);
    rx::RenderTarget *getRenderTarget(GLint level, GLint layer);
    rx::RenderTarget *getDepthStencil(GLint level, GLint layer);

  private:
    DISALLOW_COPY_AND_ASSIGN(Texture3D);

    virtual void initializeStorage(bool renderTarget);
    rx::TextureStorageInterface3D *createCompleteStorage(bool renderTarget) const;
    void setCompleteTexStorage(rx::TextureStorageInterface3D *newCompleteTexStorage);

    virtual void updateStorage();
    virtual bool ensureRenderTarget();

    virtual rx::TextureStorageInterface *getBaseLevelStorage();
    virtual const rx::Image *getBaseLevelImage() const;

    void redefineImage(GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
    void commitRect(GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth);

    bool isValidLevel(int level) const;
    bool isLevelComplete(int level) const;
    void updateStorageLevel(int level);

    rx::Image *mImageArray[IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    rx::TextureStorageInterface3D *mTexStorage;
};

class Texture2DArray : public Texture
{
  public:
    Texture2DArray(rx::Renderer *renderer, GLuint id);

    ~Texture2DArray();

    GLsizei getWidth(GLint level) const;
    GLsizei getHeight(GLint level) const;
    GLsizei getLayers(GLint level) const;
    GLenum getInternalFormat(GLint level) const;
    GLenum getActualFormat(GLint level) const;
    bool isCompressed(GLint level) const;
    bool isDepth(GLint level) const;

    void setImage(GLint level, GLsizei width, GLsizei height, GLsizei depth, GLenum internalFormat, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void setCompressedImage(GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLsizei imageSize, const void *pixels);
    void subImage(GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const PixelUnpackState &unpack, const void *pixels);
    void subImageCompressed(GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *pixels);
    void storage(GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

    virtual void generateMipmaps();
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);

    virtual bool isSamplerComplete(const SamplerState &samplerState) const;
    virtual bool isMipmapComplete() const;

    Renderbuffer *getRenderbuffer(GLint level, GLint layer);
    unsigned int getRenderTargetSerial(GLint level, GLint layer);

  protected:
    friend class RenderbufferTexture2DArrayLayer;
    rx::RenderTarget *getRenderTarget(GLint level, GLint layer);
    rx::RenderTarget *getDepthStencil(GLint level, GLint layer);

  private:
    DISALLOW_COPY_AND_ASSIGN(Texture2DArray);

    virtual void initializeStorage(bool renderTarget);
    rx::TextureStorageInterface2DArray *createCompleteStorage(bool renderTarget) const;
    void setCompleteTexStorage(rx::TextureStorageInterface2DArray *newCompleteTexStorage);

    virtual void updateStorage();
    virtual bool ensureRenderTarget();

    virtual rx::TextureStorageInterface *getBaseLevelStorage();
    virtual const rx::Image *getBaseLevelImage() const;

    void deleteImages();
    void redefineImage(GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
    void commitRect(GLint level, GLint xoffset, GLint yoffset, GLint layerTarget, GLsizei width, GLsizei height);

    bool isValidLevel(int level) const;
    bool isLevelComplete(int level) const;
    void updateStorageLevel(int level);

    // Storing images as an array of single depth textures since D3D11 treats each array level of a
    // Texture2D object as a separate subresource.  Each layer would have to be looped over
    // to update all the texture layers since they cannot all be updated at once and it makes the most
    // sense for the Image class to not have to worry about layer subresource as well as mip subresources.
    GLsizei mLayerCounts[IMPLEMENTATION_MAX_TEXTURE_LEVELS];
    rx::Image **mImageArray[IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    rx::TextureStorageInterface2DArray *mTexStorage;
};

}

#endif   // LIBGLESV2_TEXTURE_H_
