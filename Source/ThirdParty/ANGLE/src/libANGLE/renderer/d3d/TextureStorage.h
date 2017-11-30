//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage.h: Defines the abstract rx::TextureStorage class.

#ifndef LIBANGLE_RENDERER_D3D_TEXTURESTORAGE_H_
#define LIBANGLE_RENDERER_D3D_TEXTURESTORAGE_H_

#include "common/debug.h"
#include "libANGLE/angletypes.h"

#include <GLES2/gl2.h>
#include <stdint.h>

namespace gl
{
class Context;
struct ImageIndex;
struct Box;
struct PixelUnpackState;
}  // namespace gl

namespace rx
{
class SwapChainD3D;
class RenderTargetD3D;
class ImageD3D;

class TextureStorage : angle::NonCopyable
{
  public:
    TextureStorage() {}
    virtual ~TextureStorage() {}

    virtual gl::Error onDestroy(const gl::Context *context);

    virtual int getTopLevel() const = 0;
    virtual bool isRenderTarget() const = 0;
    virtual bool isManaged() const = 0;
    virtual bool supportsNativeMipmapFunction() const = 0;
    virtual int getLevelCount() const = 0;

    virtual gl::Error getRenderTarget(const gl::Context *context,
                                      const gl::ImageIndex &index,
                                      RenderTargetD3D **outRT) = 0;
    virtual gl::Error generateMipmap(const gl::Context *context,
                                     const gl::ImageIndex &sourceIndex,
                                     const gl::ImageIndex &destIndex) = 0;

    virtual gl::Error copyToStorage(const gl::Context *context, TextureStorage *destStorage) = 0;
    virtual gl::Error setData(const gl::Context *context,
                              const gl::ImageIndex &index,
                              ImageD3D *image,
                              const gl::Box *destBox,
                              GLenum type,
                              const gl::PixelUnpackState &unpack,
                              const uint8_t *pixelData) = 0;

    // This is a no-op for most implementations of TextureStorage. Some (e.g. TextureStorage11_2D) might override it.
    virtual gl::Error useLevelZeroWorkaroundTexture(const gl::Context *context,
                                                    bool useLevelZeroTexture);
};

inline gl::Error TextureStorage::onDestroy(const gl::Context *context)
{
    return gl::NoError();
}

inline gl::Error TextureStorage::useLevelZeroWorkaroundTexture(const gl::Context *context,
                                                               bool useLevelZeroTexture)
{
    return gl::NoError();
}

using TexStoragePointer = angle::UniqueObjectPointer<TextureStorage, gl::Context>;

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_TEXTURESTORAGE_H_
