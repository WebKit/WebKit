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
#include "libGLESv2/utilities.h"
#include "common/debug.h"

namespace gl
{
class Blit;

enum
{
    MAX_TEXTURE_SIZE = 2048,
    MAX_CUBE_MAP_TEXTURE_SIZE = 2048,

    MAX_TEXTURE_LEVELS = 12   // 1+log2 of MAX_TEXTURE_SIZE
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

    GLuint getWidth() const;
    GLuint getHeight() const;

    virtual GLenum getFormat() const = 0;
    virtual bool isComplete() const = 0;
    virtual bool isCompressed() const = 0;

    IDirect3DBaseTexture9 *getTexture();
    virtual Renderbuffer *getColorbuffer(GLenum target) = 0;

    virtual void generateMipmaps() = 0;

    bool isDirty() const;

    static const GLuint INCOMPLETE_TEXTURE_ID = static_cast<GLuint>(-1); // Every texture takes an id at creation time. The value is arbitrary because it is never registered with the resource manager.

  protected:
    class TextureColorbufferProxy;
    friend class TextureColorbufferProxy;
    class TextureColorbufferProxy : public Colorbuffer
    {
      public:
        TextureColorbufferProxy(Texture *texture, GLenum target);
            // target is a 2D-like texture target (GL_TEXTURE_2D or one of the cube face targets)

        virtual void addRef() const;
        virtual void release() const;

        virtual IDirect3DSurface9 *getRenderTarget();

        virtual int getWidth() const;
        virtual int getHeight() const;
        virtual GLenum getFormat() const;

      private:
        Texture *mTexture;
        GLenum mTarget;
    };

    // Helper structure representing a single image layer
    struct Image
    {
        Image();
        ~Image();

        GLsizei width;
        GLsizei height;
        GLenum format;

        bool dirty;

        IDirect3DSurface9 *surface;
    };

    static D3DFORMAT selectFormat(GLenum format);
    int imagePitch(const Image& img) const;

    void setImage(GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *img);
    bool subImage(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *img);
    void setCompressedImage(GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels, Image *img);
    bool subImageCompressed(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels, Image *img);

    void needRenderTarget();

    GLint creationLevels(GLsizei width, GLsizei height, GLint maxlevel) const;
    GLint creationLevels(GLsizei size, GLint maxlevel) const;

    // The pointer returned is weak and it is assumed the derived class will keep a strong pointer until the next createTexture() call.
    virtual IDirect3DBaseTexture9 *createTexture() = 0;
    virtual void updateTexture() = 0;
    virtual IDirect3DBaseTexture9 *convertToRenderTarget() = 0;
    virtual IDirect3DSurface9 *getRenderTarget(GLenum target) = 0;

    virtual bool dirtyImageData() const = 0;

    void dropTexture();
    void pushTexture(IDirect3DBaseTexture9 *newTexture, bool renderable);

    Blit *getBlitter();

    int levelCount() const;

    unsigned int mWidth;
    unsigned int mHeight;
    GLenum mMinFilter;
    GLenum mMagFilter;
    GLenum mWrapS;
    GLenum mWrapT;

  private:
    DISALLOW_COPY_AND_ASSIGN(Texture);

    void loadImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
                       GLint unpackAlignment, const void *input, std::size_t outputPitch, void *output) const;

    void loadAlphaImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                            size_t inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadLuminanceImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                size_t inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadLuminanceAlphaImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                     size_t inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBUByteImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               size_t inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGB565ImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                             size_t inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBAUByteImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                size_t inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBA4444ImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               size_t inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadRGBA5551ImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               size_t inputPitch, const void *input, size_t outputPitch, void *output) const;
    void loadBGRAImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                           size_t inputPitch, const void *input, size_t outputPitch, void *output) const;

    IDirect3DBaseTexture9 *mBaseTexture; // This is a weak pointer. The derived class is assumed to own a strong pointer.

    bool mDirty;
    bool mDirtyMetaData;
    bool mIsRenderable;

    void createSurface(GLsizei width, GLsizei height, GLenum format, Image *img);
};

class Texture2D : public Texture
{
  public:
    explicit Texture2D(GLuint id);

    ~Texture2D();

    GLenum getTarget() const;
    GLenum getFormat() const;

    void setImage(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setCompressedImage(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels);
    void subImage(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void subImageCompressed(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels);
    void copyImage(GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, RenderbufferStorage *source);
    void copySubImage(GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, RenderbufferStorage *source);

    bool isComplete() const;
    bool isCompressed() const;

    virtual void generateMipmaps();

    virtual Renderbuffer *getColorbuffer(GLenum target);

  private:
    DISALLOW_COPY_AND_ASSIGN(Texture2D);

    virtual IDirect3DBaseTexture9 *createTexture();
    virtual void updateTexture();
    virtual IDirect3DBaseTexture9 *convertToRenderTarget();
    virtual IDirect3DSurface9 *getRenderTarget(GLenum target);

    virtual bool dirtyImageData() const;

    bool redefineTexture(GLint level, GLenum internalFormat, GLsizei width, GLsizei height);
    void commitRect(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);

    Image mImageArray[MAX_TEXTURE_LEVELS];

    IDirect3DTexture9 *mTexture;

    Renderbuffer *mColorbufferProxy;
};

class TextureCubeMap : public Texture
{
  public:
    explicit TextureCubeMap(GLuint id);

    ~TextureCubeMap();

    GLenum getTarget() const;
    GLenum getFormat() const;

    void setImagePosX(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setImageNegX(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setImagePosY(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setImageNegY(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setImagePosZ(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void setImageNegZ(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);

    void setCompressedImage(GLenum face, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels);

    void subImage(GLenum face, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void subImageCompressed(GLenum face, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels);
    void copyImage(GLenum face, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, RenderbufferStorage *source);
    void copySubImage(GLenum face, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, RenderbufferStorage *source);

    bool isComplete() const;
    bool isCompressed() const;

    virtual void generateMipmaps();

    virtual Renderbuffer *getColorbuffer(GLenum target);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureCubeMap);

    virtual IDirect3DBaseTexture9 *createTexture();
    virtual void updateTexture();
    virtual IDirect3DBaseTexture9 *convertToRenderTarget();
    virtual IDirect3DSurface9 *getRenderTarget(GLenum target);

    virtual bool dirtyImageData() const;

    // faceIdentifier is 0-5 or one of the GL_TEXTURE_CUBE_MAP_* enumerants.
    // Returns NULL if the call underlying Direct3D call fails.
    IDirect3DSurface9 *getCubeMapSurface(unsigned int faceIdentifier, unsigned int level);

    static unsigned int faceIndex(GLenum face);

    bool isCubeComplete() const;

    void setImage(int face, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels);
    void commitRect(GLenum faceTarget, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);
    bool redefineTexture(GLint level, GLenum internalFormat, GLsizei width);

    Image mImageArray[6][MAX_TEXTURE_LEVELS];

    IDirect3DCubeTexture9 *mTexture;

    Renderbuffer *mFaceProxies[6];
};
}

#endif   // LIBGLESV2_TEXTURE_H_
