//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage.h: Defines the abstract rx::TextureStorageInterface class and its concrete derived
// classes TextureStorageInterface2D and TextureStorageInterfaceCube, which act as the interface to the
// GPU-side texture.

#ifndef LIBGLESV2_RENDERER_TEXTURESTORAGE_H_
#define LIBGLESV2_RENDERER_TEXTURESTORAGE_H_

#include "common/debug.h"

namespace rx
{
class Renderer;
class SwapChain;
class RenderTarget;

class TextureStorage
{
  public:
    TextureStorage() {};
    virtual ~TextureStorage() {};

    virtual int getTopLevel() const = 0;
    virtual bool isRenderTarget() const = 0;
    virtual bool isManaged() const = 0;
    virtual int getLevelCount() const = 0;

    virtual RenderTarget *getRenderTarget(int level) = 0;
    virtual RenderTarget *getRenderTargetFace(GLenum faceTarget, int level) = 0;
    virtual RenderTarget *getRenderTargetLayer(int mipLevel, int layer) = 0;
    virtual void generateMipmap(int level) = 0;
    virtual void generateMipmap(int face, int level) = 0;

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage);

};

class TextureStorageInterface
{
  public:
    TextureStorageInterface();
    virtual ~TextureStorageInterface();

    TextureStorage *getStorageInstance() { return mInstance; }

    unsigned int getTextureSerial() const;

    virtual int getTopLevel() const;
    virtual bool isRenderTarget() const;
    virtual bool isManaged() const;
    virtual int getLevelCount() const;

  protected:
    TextureStorage *mInstance;

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorageInterface);

    const unsigned int mTextureSerial;
    static unsigned int issueTextureSerial();

    static unsigned int mCurrentTextureSerial;
};

class TextureStorageInterface2D : public TextureStorageInterface
{
  public:
    TextureStorageInterface2D(Renderer *renderer, SwapChain *swapchain);
    TextureStorageInterface2D(Renderer *renderer, GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels);
    virtual ~TextureStorageInterface2D();

    void generateMipmap(int level);
    RenderTarget *getRenderTarget(GLint level) const;

    unsigned int getRenderTargetSerial(GLint level) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorageInterface2D);

    unsigned int mFirstRenderTargetSerial;
};

class TextureStorageInterfaceCube : public TextureStorageInterface
{
  public:
    TextureStorageInterfaceCube(Renderer *renderer, GLenum internalformat, bool renderTarget, int size, int levels);
    virtual ~TextureStorageInterfaceCube();

    void generateMipmap(int faceIndex, int level);
    RenderTarget *getRenderTarget(GLenum faceTarget, GLint level) const;

    virtual unsigned int getRenderTargetSerial(GLenum target, GLint level) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorageInterfaceCube);

    unsigned int mFirstRenderTargetSerial;
};

class TextureStorageInterface3D : public TextureStorageInterface
{
  public:
    TextureStorageInterface3D(Renderer *renderer, GLenum internalformat, bool renderTarget,
                              GLsizei width, GLsizei height, GLsizei depth, int levels);
    virtual ~TextureStorageInterface3D();

    void generateMipmap(int level);
    RenderTarget *getRenderTarget(GLint level) const;
    RenderTarget *getRenderTarget(GLint level, GLint layer) const;

    virtual unsigned int getRenderTargetSerial(GLint level, GLint layer) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorageInterface3D);

    unsigned int mFirstRenderTargetSerial;
};

class TextureStorageInterface2DArray : public TextureStorageInterface
{
  public:
    TextureStorageInterface2DArray(Renderer *renderer, GLenum internalformat, bool renderTarget,
                                   GLsizei width, GLsizei height, GLsizei depth, int levels);
    virtual ~TextureStorageInterface2DArray();

    void generateMipmap(int level);
    RenderTarget *getRenderTarget(GLint level, GLint layer) const;

    virtual unsigned int getRenderTargetSerial(GLint level, GLint layer) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorageInterface2DArray);

    unsigned int mFirstRenderTargetSerial;
};

}

#endif // LIBGLESV2_RENDERER_TEXTURESTORAGE_H_
