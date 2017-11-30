//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image9.h: Defines the rx::Image9 class, which acts as the interface to
// the actual underlying surfaces of a Texture.

#ifndef LIBANGLE_RENDERER_D3D_D3D9_IMAGE9_H_
#define LIBANGLE_RENDERER_D3D_D3D9_IMAGE9_H_

#include "libANGLE/renderer/d3d/ImageD3D.h"
#include "common/debug.h"

namespace gl
{
class Framebuffer;
}

namespace rx
{
class Renderer9;

class Image9 : public ImageD3D
{
  public:
    Image9(Renderer9 *renderer);
    ~Image9() override;

    static gl::Error generateMipmap(Image9 *dest, Image9 *source);
    static gl::Error generateMip(IDirect3DSurface9 *destSurface, IDirect3DSurface9 *sourceSurface);
    static gl::Error copyLockableSurfaces(IDirect3DSurface9 *dest, IDirect3DSurface9 *source);
    static gl::Error CopyImage(const gl::Context *context,
                               Image9 *dest,
                               Image9 *source,
                               const gl::Rectangle &sourceRect,
                               const gl::Offset &destOffset,
                               bool unpackFlipY,
                               bool unpackPremultiplyAlpha,
                               bool unpackUnmultiplyAlpha);

    bool redefine(GLenum target, GLenum internalformat, const gl::Extents &size, bool forceRelease) override;

    D3DFORMAT getD3DFormat() const;

    bool isDirty() const override;

    gl::Error setManagedSurface2D(const gl::Context *context,
                                  TextureStorage *storage,
                                  int level) override;
    gl::Error setManagedSurfaceCube(const gl::Context *context,
                                    TextureStorage *storage,
                                    int face,
                                    int level) override;
    gl::Error copyToStorage(const gl::Context *context,
                            TextureStorage *storage,
                            const gl::ImageIndex &index,
                            const gl::Box &region) override;

    gl::Error loadData(const gl::Context *context,
                       const gl::Box &area,
                       const gl::PixelUnpackState &unpack,
                       GLenum type,
                       const void *input,
                       bool applySkipImages) override;
    gl::Error loadCompressedData(const gl::Context *context,
                                 const gl::Box &area,
                                 const void *input) override;

    gl::Error copyFromTexStorage(const gl::Context *context,
                                 const gl::ImageIndex &imageIndex,
                                 TextureStorage *source) override;
    gl::Error copyFromFramebuffer(const gl::Context *context,
                                  const gl::Offset &destOffset,
                                  const gl::Rectangle &sourceArea,
                                  const gl::Framebuffer *source) override;

  private:
    gl::Error getSurface(IDirect3DSurface9 **outSurface);

    gl::Error createSurface();
    gl::Error setManagedSurface(IDirect3DSurface9 *surface);
    gl::Error copyToSurface(IDirect3DSurface9 *dest, const gl::Box &area);

    gl::Error lock(D3DLOCKED_RECT *lockedRect, const RECT &rect);
    void unlock();

    gl::Error copyFromRTInternal(const gl::Offset &destOffset,
                                 const gl::Rectangle &sourceArea,
                                 RenderTargetD3D *source);

    Renderer9 *mRenderer;

    D3DPOOL mD3DPool;   // can only be D3DPOOL_SYSTEMMEM or D3DPOOL_MANAGED since it needs to be lockable.
    D3DFORMAT mD3DFormat;

    IDirect3DSurface9 *mSurface;
};
}

#endif   // LIBANGLE_RENDERER_D3D_D3D9_IMAGE9_H_
