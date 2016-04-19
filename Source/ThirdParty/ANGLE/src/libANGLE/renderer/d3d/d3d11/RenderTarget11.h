//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderTarget11.h: Defines a DX11-specific wrapper for ID3D11View pointers
// retained by Renderbuffers.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_RENDERTARGET11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_RENDERTARGET11_H_

#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"

#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace rx
{
class SwapChain11;
class Renderer11;

class RenderTarget11 : public RenderTargetD3D
{
  public:
    RenderTarget11(d3d11::ANGLEFormat angleFormat);
    virtual ~RenderTarget11();

    virtual ID3D11Resource *getTexture() const = 0;
    virtual ID3D11RenderTargetView *getRenderTargetView() const = 0;
    virtual ID3D11DepthStencilView *getDepthStencilView() const = 0;
    virtual ID3D11ShaderResourceView *getShaderResourceView() const = 0;
    virtual ID3D11ShaderResourceView *getBlitShaderResourceView() const = 0;

    virtual unsigned int getSubresourceIndex() const = 0;

    void addDirtyCallback(const NotificationCallback *callback);
    void removeDirtyCallback(const NotificationCallback *callback);
    void signalDirty() override;

    d3d11::ANGLEFormat getANGLEFormat() const { return mANGLEFormat; }

  protected:
    NotificationSet mDirtyCallbacks;
    d3d11::ANGLEFormat mANGLEFormat;
};

class TextureRenderTarget11 : public RenderTarget11
{
  public:
    // TextureRenderTarget11 takes ownership of any D3D11 resources it is given and will AddRef them
    TextureRenderTarget11(ID3D11RenderTargetView *rtv,
                          ID3D11Resource *resource,
                          ID3D11ShaderResourceView *srv,
                          ID3D11ShaderResourceView *blitSRV,
                          GLenum internalFormat,
                          d3d11::ANGLEFormat angleFormat,
                          GLsizei width,
                          GLsizei height,
                          GLsizei depth,
                          GLsizei samples);
    TextureRenderTarget11(ID3D11DepthStencilView *dsv,
                          ID3D11Resource *resource,
                          ID3D11ShaderResourceView *srv,
                          GLenum internalFormat,
                          d3d11::ANGLEFormat angleFormat,
                          GLsizei width,
                          GLsizei height,
                          GLsizei depth,
                          GLsizei samples);
    virtual ~TextureRenderTarget11();

    GLsizei getWidth() const override;
    GLsizei getHeight() const override;
    GLsizei getDepth() const override;
    GLenum getInternalFormat() const override;
    GLsizei getSamples() const override;

    ID3D11Resource *getTexture() const override;
    ID3D11RenderTargetView *getRenderTargetView() const override;
    ID3D11DepthStencilView *getDepthStencilView() const override;
    ID3D11ShaderResourceView *getShaderResourceView() const override;
    ID3D11ShaderResourceView *getBlitShaderResourceView() const override;

    unsigned int getSubresourceIndex() const override;

  private:
    GLsizei mWidth;
    GLsizei mHeight;
    GLsizei mDepth;
    GLenum mInternalFormat;
    GLsizei mSamples;

    unsigned int mSubresourceIndex;
    ID3D11Resource *mTexture;
    ID3D11RenderTargetView *mRenderTarget;
    ID3D11DepthStencilView *mDepthStencil;
    ID3D11ShaderResourceView *mShaderResource;

    // Shader resource view to use with internal blit shaders. Not set for depth/stencil render
    // targets.
    ID3D11ShaderResourceView *mBlitShaderResource;
};

class SurfaceRenderTarget11 : public RenderTarget11
{
  public:
    SurfaceRenderTarget11(SwapChain11 *swapChain, Renderer11 *renderer, bool depth);
    virtual ~SurfaceRenderTarget11();

    GLsizei getWidth() const override;
    GLsizei getHeight() const override;
    GLsizei getDepth() const override;
    GLenum getInternalFormat() const override;
    GLsizei getSamples() const override;

    ID3D11Resource *getTexture() const override;
    ID3D11RenderTargetView *getRenderTargetView() const override;
    ID3D11DepthStencilView *getDepthStencilView() const override;
    ID3D11ShaderResourceView *getShaderResourceView() const override;
    ID3D11ShaderResourceView *getBlitShaderResourceView() const override;

    unsigned int getSubresourceIndex() const override;

  private:
    // The internal versions of the functions are needed so that they can be safely called
    // from the constructor.
    GLenum getInternalFormatInternal() const;

    SwapChain11 *mSwapChain;
    Renderer11 *mRenderer;
    bool mDepth;
};

}

#endif // LIBANGLE_RENDERER_D3D_D3D11_RENDERTARGET11_H_
