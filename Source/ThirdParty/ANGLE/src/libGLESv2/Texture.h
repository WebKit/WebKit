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

#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/RefCountObject.h"
#include "libGLESv2/utilities.h"
#include "common/debug.h"

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

class Texture : public RefCountObject
{
  public:
    explicit Texture(GLuint id);

    virtual ~Texture();

    virtual GLenum getTarget() const = 0;

    bool setMinFilter(GLenum filter);
    bool setMagFilter(GLenum filter);
    bool setWrapS(GLenum wrap);
    bool setWrapT(GLenum wrap);

    GLenum getMinFilter() const;
    GLenum getMagFilter() const;
    GLenum getWrapS() const;
    GLenum getWrapT() const;

    virtual GLsizei getWidth() const = 0;
    virtual GLsizei getHeight() const = 0;
    virtual GLenum getInternalFormat() const = 0;
    virtual GLenum getType() const = 0;
    virtual D3DFORMAT getD3DFormat() const = 0;

    virtual bool isComplete() const = 0;
    virtual bool isCompressed() const = 0;

    IDirect3DBaseTexture9 *getTexture();
    virtual Renderbuffer *getRenderbuffer(GLenum target) = 0;

    virtual void generateMipmaps() = 0;
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source) = 0;

    bool isDirtyParameter() const;
    bool isDirtyImage() const;
    void resetDirty();
    unsigned int getSerial() const;

    static const GLuint INCOMPLETE_TEXTURE_ID = static_cast<GLuint>(-1);   // Every texture takes an id at creation time. The value is arbitrary because it is never registered with the resource manager.

  protected:
    friend class Colorbuffer;

    // Helper structure representing a single image layer
    struct Image
    {
        Image();
        ~Image();

        bool isRenderable() const;
        D3DFORMAT getD3DFormat() const;

        GLsizei width;
        GLsizei height;
        GLenum format;
        GLenum type;

        bool dirty;

        IDirect3DSurface9 *surface;
    };

    void setImage(GLint unpackAlignment, const void *pixels, Image *image);
    bool subImage(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *image);
    void setCompressedImage(GLsizei imageSize, const void *pixels, Image *image);
    bool subImageCompressed(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels, Image *image);
    void copyToImage(Image *image, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, IDirect3DSurface9 *renderTarget);

    GLint creationLevels(GLsizei width, GLsizei height, GLint maxlevel) const;
    GLint creationLevels(GLsizei size, GLint maxlevel) const;

    virtual IDirect3DBaseTexture9 *getBaseTexture() const = 0;
    virtual void createTexture() = 0;
    virtual void updateTexture() = 0;
    virtual void convertToRenderTarget() = 0;
    virtual IDirect3DSurface9 *getRenderTarget(GLenum target) = 0;

    void createSurface(Image *image);

    Blit *getBlitter();

    int levelCount() const;

    GLenum mMinFilter;
    GLenum mMagFilter;
    GLenum mWrapS;
    GLenum mWrapT;
    bool mDirtyParameter;

    bool mDirtyImage;

    bool mIsRenderable;

  private:
    DISALLOW_COPY_AND_ASSIGN(Texture);

    void loadImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
                       GLint unpackAlignment, const void *input, std::size_t outputPitch, void *output, D3DSURFACE_DESC *description) const;

    void loadAlphaImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                            int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadAlphaFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                 int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadAlphaHalfFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                     int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadLuminanceImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                int inputPitch, const void *input, size_t outputPitch, void *output, bool native) const;
    void loadLuminanceFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                     int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadLuminanceHalfFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                         int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadLuminanceAlphaImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                     int inputPitch, const void *input, size_t outputPitch, void *output, bool native) const;
    void loadLuminanceAlphaFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                          int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadLuminanceAlphaHalfFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                              int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBUByteImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGB565ImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                             int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBHalfFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                   int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBAUByteImageDataSSE2(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                    int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBAUByteImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBA4444ImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBA5551ImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBAFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBAHalfFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                    int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadBGRAImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                           int inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadCompressedImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                 int inputPitch, const void *input, size_t outputPitch, void *output) const;

    static unsigned int issueSerial();

    const unsigned int mSerial;

    static unsigned int mCurrentSerial;
};

class Texture2D : public Texture
{
  public:
    explicit Texture2D(GLuint id);

    ~Texture2D();

    virtual GLenum getTarget() const;

    virtual GLsizei getWidth() const;
    virtual GLsizei getHeight() const;
    virtual GLenum getInternalFormat() const;
    virtual GLenum getType() const;
    virtual D3DFORMAT getD3DFormat() const;

    void setImage(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setCompressedImage(GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels);
    void subImage(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void subImageCompressed(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels);
    void copyImage(GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);
    void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source);

    virtual bool isComplete() const;
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

    void redefineTexture(GLint level, GLenum format, GLsizei width, GLsizei height, GLenum type, bool force);
    void commitRect(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);

    Image mImageArray[IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    IDirect3DTexture9 *mTexture;
    egl::Surface *mSurface;

    BindingPointer<Renderbuffer> mColorbufferProxy;
};

class TextureCubeMap : public Texture
{
  public:
    explicit TextureCubeMap(GLuint id);

    ~TextureCubeMap();

    virtual GLenum getTarget() const;
    
    virtual GLsizei getWidth() const;
    virtual GLsizei getHeight() const;
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

    virtual bool isComplete() const;
    virtual bool isCompressed() const;

    virtual void generateMipmaps();

    virtual Renderbuffer *getRenderbuffer(GLenum target);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureCubeMap);

    virtual IDirect3DBaseTexture9 *getBaseTexture() const;
    virtual void createTexture();
    virtual void updateTexture();
    virtual void convertToRenderTarget();
    virtual IDirect3DSurface9 *getRenderTarget(GLenum target);

    // face is one of the GL_TEXTURE_CUBE_MAP_* enumerants.
    // Returns NULL if the call underlying Direct3D call fails.
    IDirect3DSurface9 *getCubeMapSurface(GLenum face, unsigned int level);

    static unsigned int faceIndex(GLenum face);

    bool isCubeComplete() const;

    void setImage(int faceIndex, GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void commitRect(GLenum faceTarget, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);
    void redefineTexture(int faceIndex, GLint level, GLenum format, GLsizei width, GLsizei height, GLenum type);

    Image mImageArray[6][IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    IDirect3DCubeTexture9 *mTexture;

    BindingPointer<Renderbuffer> mFaceProxies[6];
};
}

#endif   // LIBGLESV2_TEXTURE_H_
