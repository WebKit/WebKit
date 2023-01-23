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
#include "RemoteSwapChainProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteSurfaceProxy.h"
#include "RemoteSwapChainMessages.h"
#include "RemoteTextureProxy.h"
#include "RemoteTextureViewProxy.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPUSurfaceDescriptor.h"
#include <pal/graphics/WebGPU/WebGPUSurfaceDescriptor.h>

namespace WebKit::WebGPU {

RemoteSwapChainProxy::RemoteSwapChainProxy(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteSwapChainProxy::~RemoteSwapChainProxy()
{
}

void RemoteSwapChainProxy::clearCurrentTextureAndView()
{
    m_currentTexture = nullptr;
    m_currentTextureView = nullptr;
}

void RemoteSwapChainProxy::ensureCurrentTextureAndView()
{
    ASSERT(static_cast<bool>(m_currentTexture) == static_cast<bool>(m_currentTextureView));

    if (m_currentTexture && m_currentTextureView)
        return;

    {
        auto identifier = WebGPUIdentifier::generate();
        auto sendResult = send(Messages::RemoteSwapChain::GetCurrentTexture(identifier));
        UNUSED_VARIABLE(sendResult);

        m_currentTexture = RemoteTextureProxy::create(root(), m_convertToBackingContext, identifier);
    }

    {
        auto identifier = WebGPUIdentifier::generate();
        auto sendResult = send(Messages::RemoteSwapChain::GetCurrentTextureView(identifier));
        UNUSED_VARIABLE(sendResult);

        m_currentTextureView = RemoteTextureViewProxy::create(root(), m_convertToBackingContext, identifier);
    }
}

PAL::WebGPU::Texture& RemoteSwapChainProxy::getCurrentTexture()
{
    ensureCurrentTextureAndView();
    return *m_currentTexture;
}

PAL::WebGPU::TextureView& RemoteSwapChainProxy::getCurrentTextureView()
{
    ensureCurrentTextureAndView();
    return *m_currentTextureView;
}

void RemoteSwapChainProxy::present()
{
    auto sendResult = send(Messages::RemoteSwapChain::Present());
    UNUSED_VARIABLE(sendResult);
    clearCurrentTextureAndView();
}

void RemoteSwapChainProxy::destroy()
{
    auto sendResult = send(Messages::RemoteSwapChain::Destroy());
    UNUSED_VARIABLE(sendResult);
}

void RemoteSwapChainProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteSwapChain::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

#if PLATFORM(COCOA)
void RemoteSwapChainProxy::prepareForDisplay(CompletionHandler<void(MachSendRight&&)>&& completionHandler)
{
    MachSendRight emptyResult;
    auto sendResult = sendSync(Messages::RemoteSwapChain::PrepareForDisplay());
    if (!sendResult) {
        completionHandler(WTFMove(emptyResult));
        return;
    }

    auto [sendRight] = sendResult.takeReply();
    if (!sendRight) {
        completionHandler(WTFMove(emptyResult));
        return;
    }

    completionHandler(WTFMove(sendRight));
}
#endif // PLATFORM(COCOA)

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
