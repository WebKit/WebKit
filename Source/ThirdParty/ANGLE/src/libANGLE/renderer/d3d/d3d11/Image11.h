//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image11.h: Defines the rx::Image11 class, which acts as the interface to
// the actual underlying resources of a Texture

#ifndef LIBANGLE_RENDERER_D3D_D3D11_IMAGE11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_IMAGE11_H_

#include "common/debug.h"
#include "libANGLE/ImageIndex.h"
#include "libANGLE/renderer/d3d/ImageD3D.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace gl
{
class Framebuffer;
}

namespace rx
{
class Renderer11;
class TextureHelper11;
class TextureStorage11;
struct Renderer11DeviceCaps;

class Image11 : public ImageD3D
{
  public:
    Image11(Renderer11 *renderer);
    ~Image11() override;

    static gl::Error GenerateMipmap(const gl::Context *context,
                                    Image11 *dest,
                                    Image11 *src,
                                    const Renderer11DeviceCaps &rendererCaps);
    static gl::Error CopyImage(const gl::Context *context,
                               Image11 *dest,
                               Image11 *source,
                               const gl::Rectangle &sourceRect,
                               const gl::Offset &destOffset,
                               bool unpackFlipY,
                               bool unpackPremultiplyAlpha,
                               bool unpackUnmultiplyAlpha,
                               const Renderer11DeviceCaps &rendererCaps);

    bool isDirty() const override;

    gl::Error copyToStorage(const gl::Context *context,
                            TextureStorage *storage,
                            const gl::ImageIndex &index,
                            const gl::Box &region) override;

    bool redefine(GLenum target, GLenum internalformat, const gl::Extents &size, bool forceRelease) override;

    DXGI_FORMAT getDXGIFormat() const;

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

    gl::Error recoverFromAssociatedStorage(const gl::Context *context);
    void verifyAssociatedStorageValid(TextureStorage11 *textureStorage) const;
    void disassociateStorage();

  protected:
    gl::Error map(const gl::Context *context, D3D11_MAP mapType, D3D11_MAPPED_SUBRESOURCE *map);
    void unmap();

  private:
    gl::Error copyWithoutConversion(const gl::Offset &destOffset,
                                    const gl::Box &sourceArea,
                                    const TextureHelper11 &textureHelper,
                                    UINT sourceSubResource);

    gl::Error getStagingTexture(const TextureHelper11 **outStagingTexture,
                                unsigned int *outSubresourceIndex);
    gl::Error createStagingTexture();
    void releaseStagingTexture();

    Renderer11 *mRenderer;

    DXGI_FORMAT mDXGIFormat;
    TextureHelper11 mStagingTexture;
    unsigned int mStagingSubresource;

    bool mRecoverFromStorage;
    TextureStorage11 *mAssociatedStorage;
    gl::ImageIndex mAssociatedImageIndex;
    unsigned int mRecoveredFromStorageCount;
};

}

#endif // LIBANGLE_RENDERER_D3D_D3D11_IMAGE11_H_
