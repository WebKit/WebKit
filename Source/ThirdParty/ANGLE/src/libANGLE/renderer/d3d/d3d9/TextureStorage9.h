//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage9.h: Defines the abstract rx::TextureStorage9 class and its concrete derived
// classes TextureStorage9_2D and TextureStorage9_Cube, which act as the interface to the
// D3D9 texture.

#ifndef LIBANGLE_RENDERER_D3D_D3D9_TEXTURESTORAGE9_H_
#define LIBANGLE_RENDERER_D3D_D3D9_TEXTURESTORAGE9_H_

#include "libANGLE/renderer/d3d/TextureStorage.h"
#include "common/debug.h"

namespace rx
{
class EGLImageD3D;
class Renderer9;
class SwapChain9;
class RenderTargetD3D;
class RenderTarget9;

class TextureStorage9 : public TextureStorage
{
  public:
    ~TextureStorage9() override;

    static DWORD GetTextureUsage(GLenum internalformat, bool renderTarget);

    D3DPOOL getPool() const;
    DWORD getUsage() const;

    virtual gl::Error getSurfaceLevel(const gl::Context *context,
                                      GLenum target,
                                      int level,
                                      bool dirty,
                                      IDirect3DSurface9 **outSurface) = 0;
    virtual gl::Error getBaseTexture(const gl::Context *context,
                                     IDirect3DBaseTexture9 **outTexture) = 0;

    int getTopLevel() const override;
    bool isRenderTarget() const override;
    bool isManaged() const override;
    bool supportsNativeMipmapFunction() const override;
    int getLevelCount() const override;

    gl::Error setData(const gl::Context *context,
                      const gl::ImageIndex &index,
                      ImageD3D *image,
                      const gl::Box *destBox,
                      GLenum type,
                      const gl::PixelUnpackState &unpack,
                      const uint8_t *pixelData) override;

  protected:
    int mTopLevel;
    size_t mMipLevels;
    size_t mTextureWidth;
    size_t mTextureHeight;
    GLenum mInternalFormat;
    D3DFORMAT mTextureFormat;

    Renderer9 *mRenderer;

    TextureStorage9(Renderer9 *renderer, DWORD usage);

  private:
    const DWORD mD3DUsage;
    const D3DPOOL mD3DPool;
};

class TextureStorage9_2D : public TextureStorage9
{
  public:
    TextureStorage9_2D(Renderer9 *renderer, SwapChain9 *swapchain);
    TextureStorage9_2D(Renderer9 *renderer, GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels);
    ~TextureStorage9_2D() override;

    gl::Error getSurfaceLevel(const gl::Context *context,
                              GLenum target,
                              int level,
                              bool dirty,
                              IDirect3DSurface9 **outSurface) override;
    gl::Error getRenderTarget(const gl::Context *context,
                              const gl::ImageIndex &index,
                              RenderTargetD3D **outRT) override;
    gl::Error getBaseTexture(const gl::Context *context,
                             IDirect3DBaseTexture9 **outTexture) override;
    gl::Error generateMipmap(const gl::Context *context,
                             const gl::ImageIndex &sourceIndex,
                             const gl::ImageIndex &destIndex) override;
    gl::Error copyToStorage(const gl::Context *context, TextureStorage *destStorage) override;

  private:
    IDirect3DTexture9 *mTexture;
    std::vector<RenderTarget9 *> mRenderTargets;
};

class TextureStorage9_EGLImage final : public TextureStorage9
{
  public:
    TextureStorage9_EGLImage(Renderer9 *renderer, EGLImageD3D *image, RenderTarget9 *renderTarget9);
    ~TextureStorage9_EGLImage() override;

    gl::Error getSurfaceLevel(const gl::Context *context,
                              GLenum target,
                              int level,
                              bool dirty,
                              IDirect3DSurface9 **outSurface) override;
    gl::Error getRenderTarget(const gl::Context *context,
                              const gl::ImageIndex &index,
                              RenderTargetD3D **outRT) override;
    gl::Error getBaseTexture(const gl::Context *context,
                             IDirect3DBaseTexture9 **outTexture) override;
    gl::Error generateMipmap(const gl::Context *context,
                             const gl::ImageIndex &sourceIndex,
                             const gl::ImageIndex &destIndex) override;
    gl::Error copyToStorage(const gl::Context *context, TextureStorage *destStorage) override;

  private:
    EGLImageD3D *mImage;
};

class TextureStorage9_Cube : public TextureStorage9
{
  public:
    TextureStorage9_Cube(Renderer9 *renderer, GLenum internalformat, bool renderTarget, int size, int levels, bool hintLevelZeroOnly);
    ~TextureStorage9_Cube() override;

    gl::Error getSurfaceLevel(const gl::Context *context,
                              GLenum target,
                              int level,
                              bool dirty,
                              IDirect3DSurface9 **outSurface) override;
    gl::Error getRenderTarget(const gl::Context *context,
                              const gl::ImageIndex &index,
                              RenderTargetD3D **outRT) override;
    gl::Error getBaseTexture(const gl::Context *context,
                             IDirect3DBaseTexture9 **outTexture) override;
    gl::Error generateMipmap(const gl::Context *context,
                             const gl::ImageIndex &sourceIndex,
                             const gl::ImageIndex &destIndex) override;
    gl::Error copyToStorage(const gl::Context *context, TextureStorage *destStorage) override;

  private:
    IDirect3DCubeTexture9 *mTexture;
    RenderTarget9 *mRenderTarget[gl::CUBE_FACE_COUNT];
};

}

#endif // LIBANGLE_RENDERER_D3D_D3D9_TEXTURESTORAGE9_H_
