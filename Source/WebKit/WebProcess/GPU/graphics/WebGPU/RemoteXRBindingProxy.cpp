/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "RemoteXRBindingProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteGPUProxy.h"
#include "RemoteXRBindingMessages.h"
#include "RemoteXRProjectionLayerProxy.h"
#include "RemoteXRSubImageProxy.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/WebGPUTextureFormat.h>

namespace WebKit::WebGPU {

RemoteXRBindingProxy::RemoteXRBindingProxy(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteXRBindingProxy::~RemoteXRBindingProxy()
{
    auto sendResult = send(Messages::RemoteXRBinding::Destruct());
    UNUSED_VARIABLE(sendResult);
}

RefPtr<WebCore::WebGPU::XRProjectionLayer> RemoteXRBindingProxy::createProjectionLayer(const WebCore::WebGPU::XRProjectionLayerInit& descriptor)
{
    auto identifier = WebGPUIdentifier::generate();

    auto sendResult = send(Messages::RemoteXRBinding::CreateProjectionLayer(descriptor.colorFormat, descriptor.depthStencilFormat, descriptor.textureUsage, descriptor.scaleFactor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteXRProjectionLayerProxy::create(root(), m_convertToBackingContext, identifier);
    return result;
}

RefPtr<WebCore::WebGPU::XRSubImage> RemoteXRBindingProxy::getSubImage(WebCore::WebGPU::XRProjectionLayer&, WebCore::WebXRFrame&, std::optional<WebCore::WebGPU::XREye>/* = "none"*/)
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<WebCore::WebGPU::XRSubImage> RemoteXRBindingProxy::getViewSubImage(WebCore::WebGPU::XRProjectionLayer& projectionLayer, WebCore::WebGPU::XREye eye)
{
    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteXRBinding::GetViewSubImage(static_cast<RemoteXRProjectionLayerProxy&>(projectionLayer).backing(), eye, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteXRSubImageProxy::create(root(), m_convertToBackingContext, identifier);
    return result;
}

WebCore::WebGPU::TextureFormat RemoteXRBindingProxy::getPreferredColorFormat()
{
    return WebCore::WebGPU::TextureFormat::Bgra8unorm;
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
