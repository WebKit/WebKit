//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage9.h: Defines the abstract rx::TextureStorage9 class and its concrete derived
// classes TextureStorage9_2D and TextureStorage9_Cube, which act as the interface to the
// D3D9 texture.

#ifndef LIBGLESV2_RENDERER_TEXTURESTORAGE9_H_
#define LIBGLESV2_RENDERER_TEXTURESTORAGE9_H_

#include "libGLESv2/renderer/TextureStorage.h"
#include "common/debug.h"

namespace rx
{
class Renderer9;
class SwapChain9;
class RenderTarget;
class RenderTarget9;

class TextureStorage9 : public TextureStorage
{
  public:
    virtual ~TextureStorage9();

    static TextureStorage9 *makeTextureStorage9(TextureStorage *storage);

    static DWORD GetTextureUsage(GLenum internalformat, Renderer9 *renderer, bool renderTarget);

    D3DPOOL getPool() const;
    DWORD getUsage() const;

    virtual IDirect3DBaseTexture9 *getBaseTexture() const = 0;
    virtual RenderTarget *getRenderTarget(int level) { return NULL; }
    virtual RenderTarget *getRenderTargetFace(GLenum faceTarget, int level) { return NULL; }
    virtual RenderTarget *getRenderTargetLayer(int mipLevel, int layer) { return NULL; }
    virtual void generateMipmap(int level) {};
    virtual void generateMipmap(int face, int level) {};

    virtual int getTopLevel() const;
    virtual bool isRenderTarget() const;
    virtual bool isManaged() const;
    virtual int getLevelCount() const;

  protected:
    int mTopLevel;
    Renderer9 *mRenderer;

    TextureStorage9(Renderer *renderer, DWORD usage);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage9);

    const DWORD mD3DUsage;
    const D3DPOOL mD3DPool;
};

class TextureStorage9_2D : public TextureStorage9
{
  public:
    TextureStorage9_2D(Renderer *renderer, SwapChain9 *swapchain);
    TextureStorage9_2D(Renderer *renderer, GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels);
    virtual ~TextureStorage9_2D();

    static TextureStorage9_2D *makeTextureStorage9_2D(TextureStorage *storage);

    IDirect3DSurface9 *getSurfaceLevel(int level, bool dirty);
    virtual RenderTarget *getRenderTarget(int level);
    virtual IDirect3DBaseTexture9 *getBaseTexture() const;
    virtual void generateMipmap(int level);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage9_2D);

    void initializeRenderTarget();

    IDirect3DTexture9 *mTexture;
    RenderTarget9 *mRenderTarget;
};

class TextureStorage9_Cube : public TextureStorage9
{
  public:
    TextureStorage9_Cube(Renderer *renderer, GLenum internalformat, bool renderTarget, int size, int levels);
    virtual ~TextureStorage9_Cube();

    static TextureStorage9_Cube *makeTextureStorage9_Cube(TextureStorage *storage);

    IDirect3DSurface9 *getCubeMapSurface(GLenum faceTarget, int level, bool dirty);
    virtual RenderTarget *getRenderTargetFace(GLenum faceTarget, int level);
    virtual IDirect3DBaseTexture9 *getBaseTexture() const;
    virtual void generateMipmap(int faceIndex, int level);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage9_Cube);

    void initializeRenderTarget();

    IDirect3DCubeTexture9 *mTexture;
    RenderTarget9 *mRenderTarget[6];
};

}

#endif // LIBGLESV2_RENDERER_TEXTURESTORAGE9_H_

