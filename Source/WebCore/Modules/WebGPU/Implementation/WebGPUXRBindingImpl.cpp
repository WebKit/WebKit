/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "WebGPUXRBindingImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUConvertToBackingContext.h"
#include "WebGPUDevice.h"
#include "WebGPUTextureFormat.h"
#include "WebGPUXRProjectionLayerImpl.h"
#include "WebGPUXRSubImageImpl.h"

namespace WebCore::WebGPU {

XRBindingImpl::XRBindingImpl(WebGPUPtr<WGPUXRBinding>&& binding, ConvertToBackingContext& convertToBackingContext)
    : m_backing(WTFMove(binding))
    , m_convertToBackingContext(convertToBackingContext)
{
}

XRBindingImpl::~XRBindingImpl() = default;

RefPtr<XRProjectionLayer> XRBindingImpl::createProjectionLayer(const XRProjectionLayerInit& init)
{
    WGPUTextureFormat colorFormat = m_convertToBackingContext->convertToBacking(init.colorFormat);
    WGPUTextureFormat optionalDepthStencilFormat;
    if (init.depthStencilFormat)
        optionalDepthStencilFormat = m_convertToBackingContext->convertToBacking(*init.depthStencilFormat);
    WGPUTextureUsageFlags flags = m_convertToBackingContext->convertTextureUsageFlagsToBacking(init.textureUsage);
    return XRProjectionLayerImpl::create(adoptWebGPU(wgpuBindingCreateXRProjectionLayer(m_backing.get(), colorFormat, init.depthStencilFormat ? &optionalDepthStencilFormat : nullptr, flags, init.scaleFactor)), m_convertToBackingContext);
}

RefPtr<XRSubImage> XRBindingImpl::getSubImage(XRProjectionLayer&, WebCore::WebXRFrame&, std::optional<XREye>/* = "none"*/)
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<XRSubImage> XRBindingImpl::getViewSubImage(XRProjectionLayer& projectionLayer, XREye eye)
{
    UNUSED_PARAM(projectionLayer);
    return XRSubImageImpl::create(adoptWebGPU(wgpuBindingGetViewSubImage(m_backing.get(), m_convertToBackingContext->convertToBacking(eye))), m_convertToBackingContext);
}

TextureFormat XRBindingImpl::getPreferredColorFormat()
{
    return TextureFormat::Bgra8unorm;
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
