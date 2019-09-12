//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/d3d/d3d11/ExternalImageSiblingImpl11.h"

#include "libANGLE/Context.h"
#include "libANGLE/Error.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/d3d11/Context11.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"

namespace rx
{
ExternalImageSiblingImpl11::ExternalImageSiblingImpl11(Renderer11 *renderer,
                                                       EGLClientBuffer buffer,
                                                       const egl::AttributeMap &attribs)
    : mRenderer(renderer), mBuffer(buffer), mAttribs(attribs)
{}

ExternalImageSiblingImpl11::~ExternalImageSiblingImpl11() {}

egl::Error ExternalImageSiblingImpl11::initialize(const egl::Display *display)
{
    const angle::Format *angleFormat = nullptr;
    ANGLE_TRY(mRenderer->getD3DTextureInfo(nullptr, static_cast<IUnknown *>(mBuffer), mAttribs,
                                           &mWidth, &mHeight, &mSamples, &mFormat, &angleFormat));
    ID3D11Texture2D *texture =
        d3d11::DynamicCastComObject<ID3D11Texture2D>(static_cast<IUnknown *>(mBuffer));
    ASSERT(texture != nullptr);
    // TextureHelper11 will release texture on destruction.
    mTexture.set(texture, d3d11::Format::Get(angleFormat->glInternalFormat,
                                             mRenderer->getRenderer11DeviceCaps()));
    D3D11_TEXTURE2D_DESC textureDesc = {};
    mTexture.getDesc(&textureDesc);

    IDXGIResource *resource = d3d11::DynamicCastComObject<IDXGIResource>(mTexture.get());
    ASSERT(resource != nullptr);
    DXGI_USAGE resourceUsage = 0;
    resource->GetUsage(&resourceUsage);
    SafeRelease(resource);

    mIsRenderable = (textureDesc.BindFlags & D3D11_BIND_RENDER_TARGET) &&
                    (resourceUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) &&
                    !(resourceUsage & DXGI_USAGE_READ_ONLY);

    mIsTexturable = (textureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) &&
                    (resourceUsage & DXGI_USAGE_SHADER_INPUT);

    return egl::NoError();
}

gl::Format ExternalImageSiblingImpl11::getFormat() const
{
    return mFormat;
}

bool ExternalImageSiblingImpl11::isRenderable(const gl::Context *context) const
{
    return mIsRenderable;
}

bool ExternalImageSiblingImpl11::isTexturable(const gl::Context *context) const
{
    return mIsTexturable;
}

gl::Extents ExternalImageSiblingImpl11::getSize() const
{
    return gl::Extents(mWidth, mHeight, 1);
}

size_t ExternalImageSiblingImpl11::getSamples() const
{
    return mSamples;
}

angle::Result ExternalImageSiblingImpl11::getAttachmentRenderTarget(
    const gl::Context *context,
    GLenum binding,
    const gl::ImageIndex &imageIndex,
    GLsizei samples,
    FramebufferAttachmentRenderTarget **rtOut)
{
    ANGLE_TRY(createRenderTarget(context));
    *rtOut = mRenderTarget.get();
    return angle::Result::Continue;
}

angle::Result ExternalImageSiblingImpl11::initializeContents(const gl::Context *context,
                                                             const gl::ImageIndex &imageIndex)
{
    UNREACHABLE();
    return angle::Result::Stop;
}

angle::Result ExternalImageSiblingImpl11::createRenderTarget(const gl::Context *context)
{
    if (mRenderTarget)
        return angle::Result::Continue;

    Context11 *context11            = GetImplAs<Context11>(context);
    const d3d11::Format &formatInfo = mTexture.getFormatSet();

    d3d11::RenderTargetView rtv;
    if (mIsRenderable)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format = formatInfo.rtvFormat;
        rtvDesc.ViewDimension =
            mSamples == 0 ? D3D11_RTV_DIMENSION_TEXTURE2D : D3D11_RTV_DIMENSION_TEXTURE2DMS;
        rtvDesc.Texture2D.MipSlice = 0;

        ANGLE_TRY(mRenderer->allocateResource(context11, rtvDesc, mTexture.get(), &rtv));
        rtv.setDebugName("getAttachmentRenderTarget.RTV");
    }

    d3d11::SharedSRV srv;
    if (mIsTexturable)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = formatInfo.srvFormat;
        srvDesc.ViewDimension =
            mSamples == 0 ? D3D11_SRV_DIMENSION_TEXTURE2D : D3D11_SRV_DIMENSION_TEXTURE2DMS;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels       = 1;

        ANGLE_TRY(mRenderer->allocateResource(context11, srvDesc, mTexture.get(), &srv));
        srv.setDebugName("getAttachmentRenderTarget.SRV");
    }
    d3d11::SharedSRV blitSrv = srv.makeCopy();

    mRenderTarget = std::make_unique<TextureRenderTarget11>(
        std::move(rtv), mTexture, std::move(srv), std::move(blitSrv), mFormat.info->internalFormat,
        formatInfo, mWidth, mHeight, 1, mSamples);
    return angle::Result::Continue;
}

}  // namespace rx
