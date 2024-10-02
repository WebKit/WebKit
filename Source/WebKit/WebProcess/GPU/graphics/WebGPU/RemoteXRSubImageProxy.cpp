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
#include "RemoteXRSubImageProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteGPUProxy.h"
#include "RemoteTextureProxy.h"
#include "RemoteXRSubImageMessages.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/WebGPUTextureFormat.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteXRSubImageProxy);

RemoteXRSubImageProxy::RemoteXRSubImageProxy(Ref<RemoteGPUProxy>&& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(WTFMove(parent))
{
}

RemoteXRSubImageProxy::~RemoteXRSubImageProxy()
{
    auto sendResult = send(Messages::RemoteXRSubImage::Destruct());
    UNUSED_VARIABLE(sendResult);
}

RefPtr<WebCore::WebGPU::Texture> RemoteXRSubImageProxy::colorTexture()
{
    if (m_currentTexture)
        return m_currentTexture;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteXRSubImage::GetColorTexture(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    m_currentTexture = RemoteTextureProxy::create(protectedRoot(), m_convertToBackingContext, identifier);
    return m_currentTexture;
}

RefPtr<WebCore::WebGPU::Texture> RemoteXRSubImageProxy::depthStencilTexture()
{
    if (m_currentDepthTexture)
        return m_currentDepthTexture;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteXRSubImage::GetDepthTexture(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    m_currentDepthTexture = RemoteTextureProxy::create(protectedRoot(), m_convertToBackingContext, identifier);
    return m_currentDepthTexture;
}

RefPtr<WebCore::WebGPU::Texture> RemoteXRSubImageProxy::motionVectorTexture()
{
    return nullptr;
}


} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
