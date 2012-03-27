//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
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
#include <GLES2/gl2.h>
#include <d3d9.h>

#include "common/debug.h"
#include "common/RefCountObject.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/utilities.h"

namespace egl
{
class Surface;
}

namespace gl
{
class Blit;
class Framebuffer;

enum
{
    // These are the maximums the implementation can support
    // The actual GL caps are limited by the device caps
    // and should be queried from the Context
    IMPLEMENTATION_MAX_TEXTURE_SIZE = 16384,
    IMPLEMENTATION_MAX_CUBE_MAP_TEXTURE_SIZE = 16384,

    IMPLEMENTATION_MAX_TEXTURE_LEVELS = 15   // 1+log2 of MAX_TEXTURE_SIZE
};

class Image
{
  public:
    Image();
    ~Image();

    bool redefine(GLenum format, GLsizei width, GLsizei height, GLenum type, bool forceRelease);
    void markDirty() {mDirty = true;}
    void markClean() {mDirty = false;}

    HRESULT lock(D3DLOCKED_RECT *lockedRect, const RECT *rect);
    void unlock();

    bool isRenderableFormat() const;
    D3DFORMAT getD3DFormat() const;

    GLsizei getWidth() const {return mWidth;}
    GLsizei getHeight() const {return mHeight;}
    GLenum getFormat() const {return mFormat;}
    GLenum getType() const {return mType;}
    bool isDirty() const {return mSurface && mDirty;}
    IDirect3DSurface9 *getSurface();

    void setManagedSurface(IDirect3DSurface9 *surface);
    void updateSurface(IDirect3DSurface9 *dest, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);

    void loadData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum type,
                  GLint unpackAlignment, const void *input, std::size_t outputPitch, void *output) const;

    void loadAlphaData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                       int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadAlphaFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                            int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadAlphaHalfFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadLuminanceData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                           int inputPitch, const void *input, size_t outputPitch, void *output, bool native) const;
    void loadLuminanceFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadLuminanceHalfFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                    int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadLuminanceAlphaData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                int inputPitch, const void *input, size_t outputPitch, void *output, bool native) const;
    void loadLuminanceAlphaFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                     int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadLuminanceAlphaHalfFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                         int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBUByteData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                          int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGB565Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                        int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                          int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBHalfFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                              int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBAUByteDataSSE2(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBAUByteData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                           int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBA4444Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                          int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBA5551Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                          int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBAFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                           int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBAHalfFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadBGRAData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                      int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadCompressedData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                            int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadDXT1Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                      int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadDXT3Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                      int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadDXT5Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                      int inputPitch, const void *input, size_t outputPitch, void *output) const;

    void copy(GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, IDirect3DSurface9 *renderTarget);

  private:
    DISALLOW_COPY_AND_ASSIGN(Image);

    void createSurface();

    GLsizei mWidth;
    GLsizei mHeight;
    GLenum mFormat;
    GLenum mType;

    bool mDirty;

    D3DPOOL mD3DPool;   // can only be D3DPOOL_SYSTEMMEM or D3DPOOL_MANAGED since it needs to be lockable.
    D3DFORMAT mD3DFormat;

    IDirect3DSurface9 *mSurface;
};

class TextureStorage
{
  public:
    explicit TextureStorage(bool renderTarget);

    virtual ~TextureStorage();

    bool isRenderTarget() const;
    bool isManaged() const;
    D3DPOOL getPool() const;
    unsigned int getTextureSerial() const;
    virtual unsigned int getRenderTargetSerial(GLenum target) const = 0;

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage);

    const bool mRenderTarget;
    const D3DPOOL mD3DPool;

    const unsigned int mTextureSerial;
    static unsigned int issueTextureSerial();

    static unsigned int mCurrentTextureSerial;
};

class Texture : public RefCountObject
{
  public:
    explicit Texture(GLuint id);

    virtual ~Texture();

    virtual void addProxyRef(const Renderbuffer *proxy) = 0;
    virtual void releaseProxy(const Renderbuffer *proxy) = 0;

    virtual GLenum getTarget() const = 0;

    bool setMinFilter(GLenum filter);
    bool setMagFilter(GLenum filter);
    bool setWrapS(GLenum wrap);
    bool setWrapT(GLenum wrap);
    bool setUsage(GLenum usage);

    GLenum getMinFilter() const;
    GLenum getMagFilter() const;
    GLenum getWrapS() const;
    GLenum getWrapT() const;
    GLenum getUsage() const;

    virtual GLsizei getWidth(GLint level) const = 0;
    virtual GLsizei getHeight(GLint level) const = 0;
    virtual GLenum getInternalFormat() const = 0;
    virtual GLenum getType() const = 0;
    virtual D3DFORMAT getD3DFormat() const = 0;

    virtual bool isSamplerComplete() const = 0;
    virtual bool isCompressed() const = 0;

    IDirect3DBaseTexture9 *getTexture();
    virtual Renderbuffer *getRenderbuffer(GLenum target) = 0;

    virtual void generateMipmaps() = 0;
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source) = 0;

    bool hasDirtyParameters() const;
    bool hasDirtyImages() const;
    void resetDirty();
    unsigned int getTextureSerial();
    unsigned int getRenderTargetSerial(GLenum target);

    bool isImmutable() const;

    static const GLuint INCOMPLETE_TEXTURE_ID = static_cast<GLuint>(-1);   // Every texture takes an id at creation time. The value is arbitrary because it is never registered with the resource manager.

  protected:
    friend class RenderbufferTexture;

    void setImage(GLint unpackAlignment, const void *pixels, Image *image);
    bool subImage(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *image);
    void setCompressedImage(GLsizei imageSize, const void *pixels, Image *image);
    bool subImageCompressed(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels, Image *image);

    GLint creationLevels(GLsizei width, GLsizei height) const;
    GLint creationLevels(GLsizei size) const;

    virtual IDirect3DBaseTexture9 *getBaseTexture() const = 0;
    virtual void createTexture() = 0;
    virtual void updateTexture() = 0;
    virtual void convertToRenderTarget() = 0;
    virtual IDirect3DSurface9 *getRenderTarget(GLenum target) = 0;

    int levelCount() const;

    static Blit *getBlitter();
    static bool copyToRenderTarget(IDirect3DSurface9 *dest, IDirect3DSurface9 *source, bool fromManaged);

    GLenum mMinFilter;
    GLenum mMagFilter;
    GLenum mWrapS;
    GLenum mWrapT;
    bool mDirtyParameters;
    GLenum mUsage;

    bool mDirtyImages;

    bool mImmutable;

  private:
    DISALLOW_COPY_AND_ASSIGN(Texture);

    virtual TextureStorage *getStorage(bool renderTarget) = 0;
};

class TextureStorage2D : public TextureStorage
{
  public:
    explicit TextureStorage2D(IDirect3DTexture9 *surfaceTexture);
    TextureStorage2D(int levels, D3DFORMAT format, int width, int height, bool renderTarget);

    virtual ~TextureStorage2D();

    IDirect3DSurface9 *getSurfaceLevel(int level);
    IDirect3DBaseTexture9 *getBaseTexture() const;

    virtual unsigned int getRenderTargetSerial(GLenum target) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage2D);

    IDirect3DTexture9 *mTexture;
    const unsigned int mRenderTargetSerial;
};

class Texture2D : public Texture
{
  public:
    explicit Texture2D(GLuint id);

    ~Texture2D();

    void addProxyRef(const Renderbuffer *proxy);
    void releaseProxy(const Renderbuffer *proxy);

    virtual GLenum getTarget() const;

    virtual GLsizei getWidth(GLint level) const;
    virtual GLsizei getHeight(GLint level) const;
    virtual GLenum getInternalFormat() const;
    virtual GLenum getType() const;
    virtual D3DFORMAT getD3DFormat() const;

    void setImage(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setCompressedImage(GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels);
    void subImage(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void subImageCompressed(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels);
    void copyImage(GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);
    void storage(GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);

    virtual bool isSamplerComplete() const;
    virtual bool isCompressed() const;
    virtual void bindTexImage(egl::Surface *surface);
    virtual void releaseTexImage();

    virtual void generateMipmaps();

    virtual Renderbuffer *getRenderbuffer(GLenum target);

  private:
    DISALLOW_COPY_AND_ASSIGN(Texture2D);

    virtual IDirect3DBaseTexture9 *getBaseTexture() const;
    virtual void createTexture();
    virtual void updateTexture();
    virtual void convertToRenderTarget();
    virtual IDirect3DSurface9 *getRenderTarget(GLenum target);
    virtual TextureStorage *getStorage(bool renderTarget);

    bool isMipmapComplete() const;

    void redefineImage(GLint level, GLenum format, GLsizei width, GLsizei height, GLenum type);
    void commitRect(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);

    Image mImageArray[IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    TextureStorage2D *mTexStorage;
    egl::Surface *mSurface;

    // A specific internal reference count is kept for colorbuffer proxy references,
    // because, as the renderbuffer acting as proxy will maintain a binding pointer
    // back to this texture, there would be a circular reference if we used a binding
    // pointer here. This reference count will cause the pointer to be set to NULL if
    // the count drops to zero, but will not cause deletion of the Renderbuffer.
    Renderbuffer *mColorbufferProxy;
    unsigned int mProxyRefs;
};

class TextureStorageCubeMap : public TextureStorage
{
  public:
    TextureStorageCubeMap(int levels, D3DFORMAT format, int size, bool renderTarget);

    virtual ~TextureStorageCubeMap();

    IDirect3DSurface9 *getCubeMapSurface(GLenum faceTarget, int level);
    IDirect3DBaseTexture9 *getBaseTexture() const;

    virtual unsigned int getRenderTargetSerial(GLenum target) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorageCubeMap);

    IDirect3DCubeTexture9 *mTexture;
    const unsigned int mFirstRenderTargetSerial;
};

class TextureCubeMap : public Texture
{
  public:
    explicit TextureCubeMap(GLuint id);

    ~TextureCubeMap();

    void addProxyRef(const Renderbuffer *proxy);
    void releaseProxy(const Renderbuffer *proxy);

    virtual GLenum getTarget() const;
    
    virtual GLsizei getWidth(GLint level) const;
    virtual GLsizei getHeight(GLint level) const;
    virtual GLenum getInternalFormat() const;
    virtual GLenum getType() const;
    virtual D3DFORMAT getD3DFormat() const;

    void setImagePosX(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setImageNegX(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setImagePosY(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setImageNegY(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setImagePosZ(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setImageNegZ(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);

    void setCompressedImage(GLenum face, GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels);

    void subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels);
    void copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);
    void storage(GLsizei levels, GLenum internalformat, GLsizei size);

    virtual bool isSamplerComplete() const;
    virtual bool isCompressed() const;

    virtual void generateMipmaps();

    virtual Renderbuffer *getRenderbuffer(GLenum target);

    static unsigned int faceIndex(GLenum face);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureCubeMap);

    virtual IDirect3DBaseTexture9 *getBaseTexture() const;
    virtual void createTexture();
    virtual void updateTexture();
    virtual void convertToRenderTarget();
    virtual IDirect3DSurface9 *getRenderTarget(GLenum target);
    virtual TextureStorage *getStorage(bool renderTarget);

    bool isCubeComplete() const;
    bool isMipmapCubeComplete() const;

    void setImage(int faceIndex, GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void commitRect(int faceIndex, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);
    void redefineImage(int faceIndex, GLint level, GLenum format, GLsizei width, GLsizei height, GLenum type);

    Image mImageArray[6][IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    TextureStorageCubeMap *mTexStorage;

    // A specific internal reference count is kept for colorbuffer proxy references,
    // because, as the renderbuffer acting as proxy will maintain a binding pointer
    // back to this texture, there would be a circular reference if we used a binding
    // pointer here. This reference count will cause the pointer to be set to NULL if
    // the count drops to zero, but will not cause deletion of the Renderbuffer.
    Renderbuffer *mFaceProxies[6];
    unsigned int *mFaceProxyRefs[6];
};
}

#endif   // LIBGLESV2_TEXTURE_H_