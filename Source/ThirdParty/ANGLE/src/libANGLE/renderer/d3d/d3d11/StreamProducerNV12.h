//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StreamProducerNV12.h: Interface for a NV12 texture stream producer

#ifndef LIBANGLE_RENDERER_D3D_D3D11_STREAM11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_STREAM11_H_

#include "libANGLE/renderer/StreamProducerImpl.h"

namespace rx
{
class Renderer11;

class StreamProducerNV12 : public StreamProducerImpl
{
  public:
    StreamProducerNV12(Renderer11 *renderer);
    ~StreamProducerNV12() override;

    egl::Error validateD3DNV12Texture(void *pointer) const override;
    void postD3DNV12Texture(void *pointer, const egl::AttributeMap &attributes) override;
    egl::Stream::GLTextureDescription getGLFrameDescription(int planeIndex) override;

    // Gets a pointer to the internal D3D texture
    ID3D11Texture2D *getD3DTexture();

    // Gets the slice index for the D3D texture that the frame is in
    UINT getArraySlice();

  private:
    Renderer11 *mRenderer;

    ID3D11Texture2D *mTexture;
    UINT mArraySlice;
    UINT mTextureWidth;
    UINT mTextureHeight;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D11_STREAM11_H_
