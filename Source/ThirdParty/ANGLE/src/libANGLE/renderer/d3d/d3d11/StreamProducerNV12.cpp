//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StreamProducerNV12.cpp: Implements the stream producer for NV12 textures

#include "libANGLE/renderer/d3d/d3d11/StreamProducerNV12.h"

#include "common/utilities.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace rx
{

StreamProducerNV12::StreamProducerNV12(Renderer11 *renderer)
    : mRenderer(renderer), mTexture(nullptr), mArraySlice(0), mTextureWidth(0), mTextureHeight(0)
{
}

StreamProducerNV12::~StreamProducerNV12()
{
    SafeRelease(mTexture);
}

egl::Error StreamProducerNV12::validateD3DNV12Texture(void *pointer) const
{
    ID3D11Texture2D *textureD3D = static_cast<ID3D11Texture2D *>(pointer);

    // Check that the texture originated from our device
    ID3D11Device *device;
    textureD3D->GetDevice(&device);
    if (device != mRenderer->getDevice())
    {
        return egl::Error(EGL_BAD_PARAMETER, "Texture not created on ANGLE D3D device");
    }

    // Get the description and validate it
    D3D11_TEXTURE2D_DESC desc;
    textureD3D->GetDesc(&desc);
    if (desc.Format != DXGI_FORMAT_NV12)
    {
        return egl::Error(EGL_BAD_PARAMETER, "Texture format not DXGI_FORMAT_NV12");
    }
    if (desc.Width < 1 || desc.Height < 1)
    {
        return egl::Error(EGL_BAD_PARAMETER, "Texture is of size 0");
    }
    if ((desc.Width % 2) != 0 || (desc.Height % 2) != 0)
    {
        return egl::Error(EGL_BAD_PARAMETER, "Texture dimensions are not even");
    }
    return egl::Error(EGL_SUCCESS);
}

void StreamProducerNV12::postD3DNV12Texture(void *pointer, const egl::AttributeMap &attributes)
{
    ASSERT(pointer != nullptr);
    ID3D11Texture2D *textureD3D = static_cast<ID3D11Texture2D *>(pointer);

    // Check that the texture originated from our device
    ID3D11Device *device;
    textureD3D->GetDevice(&device);

    // Get the description
    D3D11_TEXTURE2D_DESC desc;
    textureD3D->GetDesc(&desc);

    // Release the previous texture if there is one
    SafeRelease(mTexture);

    mTexture = textureD3D;
    mTexture->AddRef();
    mTextureWidth  = desc.Width;
    mTextureHeight = desc.Height;
    mArraySlice    = static_cast<UINT>(attributes.get(EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, 0));
}

egl::Stream::GLTextureDescription StreamProducerNV12::getGLFrameDescription(int planeIndex)
{
    // The UV plane of NV12 textures has half the width/height of the Y plane
    egl::Stream::GLTextureDescription desc;
    desc.width          = (planeIndex == 0) ? mTextureWidth : (mTextureWidth / 2);
    desc.height         = (planeIndex == 0) ? mTextureHeight : (mTextureHeight / 2);
    desc.internalFormat = (planeIndex == 0) ? GL_R8 : GL_RG8;
    desc.mipLevels      = 0;
    return desc;
}

ID3D11Texture2D *StreamProducerNV12::getD3DTexture()
{
    return mTexture;
}

UINT StreamProducerNV12::getArraySlice()
{
    return mArraySlice;
}

}  // namespace rx
