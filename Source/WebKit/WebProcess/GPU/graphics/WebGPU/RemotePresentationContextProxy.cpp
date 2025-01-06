/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#include "RemotePresentationContextProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemotePresentationContextMessages.h"
#include "RemoteTextureProxy.h"
#include "WebGPUCanvasConfiguration.h"
#include "WebGPUConvertToBackingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemotePresentationContextProxy);

RemotePresentationContextProxy::RemotePresentationContextProxy(RemoteGPUProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemotePresentationContextProxy::~RemotePresentationContextProxy() = default;

bool RemotePresentationContextProxy::configure(const WebCore::WebGPU::CanvasConfiguration& canvasConfiguration)
{
    auto convertedConfiguration = protectedConvertToBackingContext()->convertToBacking(canvasConfiguration);
    if (!convertedConfiguration)
        return false;

    auto sendResult = send(Messages::RemotePresentationContext::Configure(*convertedConfiguration));
    return sendResult == IPC::Error::NoError;
}

void RemotePresentationContextProxy::unconfigure()
{
    for (size_t i = 0; i < textureCount; ++i)
        m_currentTexture[i] = nullptr;

    auto sendResult = send(Messages::RemotePresentationContext::Unconfigure());
    UNUSED_VARIABLE(sendResult);
}

RefPtr<WebCore::WebGPU::Texture> RemotePresentationContextProxy::getCurrentTexture(uint32_t frameIndex)
{
    if (frameIndex >= textureCount)
        return nullptr;

    if (!m_currentTexture[frameIndex]) {
        auto identifier = WebGPUIdentifier::generate();
        auto sendResult = send(Messages::RemotePresentationContext::GetCurrentTexture(identifier, frameIndex));
        if (sendResult != IPC::Error::NoError)
            return nullptr;

        m_currentTexture[frameIndex] = RemoteTextureProxy::create(protectedRoot(), protectedConvertToBackingContext(), identifier, true);
    } else
        RefPtr { m_currentTexture[frameIndex] }->undestroy();

    return m_currentTexture[frameIndex];
}

void RemotePresentationContextProxy::present(uint32_t frameIndex, bool presentToGPUProcess)
{
    if (presentToGPUProcess) {
        auto sendResult = send(Messages::RemotePresentationContext::Present(frameIndex));
        UNUSED_VARIABLE(sendResult);
    }
}

RefPtr<WebCore::NativeImage> RemotePresentationContextProxy::getMetalTextureAsNativeImage(uint32_t, bool&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

Ref<ConvertToBackingContext> RemotePresentationContextProxy::protectedConvertToBackingContext() const
{
    return m_convertToBackingContext;
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
