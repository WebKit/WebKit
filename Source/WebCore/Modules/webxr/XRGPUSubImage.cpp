/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XRGPUSubImage.h"

#if ENABLE(WEBXR_LAYERS)

#include "GPUTextureDescriptor.h"
#include "GPUTextureFormat.h"
#include "WebXRViewport.h"

namespace WebCore {

static constexpr auto gpuSubImageWidth = 1920;
static constexpr auto gpuSubImageHeight = 1824;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(XRGPUSubImage);

static auto makeTextureViewDescriptor(WebGPU::XREye eye)
{
    return GPUTextureViewDescriptor {
        .format = std::nullopt,
        .dimension = GPUTextureViewDimension::_2d,
        .aspect = GPUTextureAspect::All,
        .baseMipLevel = 0,
        .mipLevelCount = std::nullopt,
        .baseArrayLayer = (eye == WebGPU::XREye::Right ? 1u : 0u),
        .arrayLayerCount = 1u,
    };
}

XRGPUSubImage::XRGPUSubImage(Ref<WebGPU::XRSubImage>&& backing, WebGPU::XREye eye, GPUDevice& device)
    : m_backing(WTFMove(backing))
    , m_device(device)
    , m_descriptor(makeTextureViewDescriptor(eye))
    , m_viewport(WebXRViewport::create(IntRect { 0, 0, gpuSubImageWidth, gpuSubImageHeight }))
{
}

static GPUTextureDescriptor textureDescriptor(GPUTextureFormat format)
{
    return GPUTextureDescriptor {
        { "canvas backing"_s },
        GPUExtent3DDict { gpuSubImageWidth, gpuSubImageHeight, 1 },
        1,
        1,
        GPUTextureDimension::_2d,
        format,
        GPUTextureUsage::RENDER_ATTACHMENT,
        { }
    };
}

ExceptionOr<Ref<GPUTexture>> XRGPUSubImage::colorTexture()
{
    RefPtr texture = m_backing->colorTexture();
    if (!texture)
        return Exception { ExceptionCode::InvalidStateError };

    return GPUTexture::create(texture.releaseNonNull(), textureDescriptor(GPUTextureFormat::Bgra8unormSRGB), m_device);
}

RefPtr<GPUTexture> XRGPUSubImage::depthStencilTexture()
{
    RefPtr texture = m_backing->depthStencilTexture();
    if (!texture)
        return nullptr;

    return GPUTexture::create(texture.releaseNonNull(), textureDescriptor(GPUTextureFormat::Depth24plus), m_device);
}

RefPtr<GPUTexture> XRGPUSubImage::motionVectorTexture()
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

const GPUTextureViewDescriptor& XRGPUSubImage::getViewDescriptor() const
{
    return m_descriptor;
}

const WebXRViewport& XRGPUSubImage::viewport() const
{
    return m_viewport;
}

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
