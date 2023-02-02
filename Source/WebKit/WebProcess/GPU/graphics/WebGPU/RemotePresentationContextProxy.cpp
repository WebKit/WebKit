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

namespace WebKit::WebGPU {

RemotePresentationContextProxy::RemotePresentationContextProxy(RemoteGPUProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemotePresentationContextProxy::~RemotePresentationContextProxy() = default;

void RemotePresentationContextProxy::configure(const PAL::WebGPU::CanvasConfiguration& canvasConfiguration)
{
    auto convertedConfiguration = m_convertToBackingContext->convertToBacking(canvasConfiguration);
    if (!convertedConfiguration) {
        // FIXME: Implement error handling.
        return;
    }

    auto sendResult = send(Messages::RemotePresentationContext::Configure(*convertedConfiguration));
    UNUSED_VARIABLE(sendResult);
}

void RemotePresentationContextProxy::unconfigure()
{
    auto sendResult = send(Messages::RemotePresentationContext::Unconfigure());
    UNUSED_VARIABLE(sendResult);
}

RefPtr<PAL::WebGPU::Texture> RemotePresentationContextProxy::getCurrentTexture()
{
    if (!m_currentTexture) {
        auto identifier = WebGPUIdentifier::generate();
        auto sendResult = send(Messages::RemotePresentationContext::GetCurrentTexture(identifier));
        UNUSED_VARIABLE(sendResult);

        m_currentTexture = RemoteTextureProxy::create(root(), m_convertToBackingContext, identifier);
    }

    return m_currentTexture;
}

void RemotePresentationContextProxy::present()
{
    auto sendResult = send(Messages::RemotePresentationContext::Present());
    UNUSED_VARIABLE(sendResult);
    m_currentTexture = nullptr;
}

#if PLATFORM(COCOA)
void RemotePresentationContextProxy::prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&& completionHandler)
{
    MachSendRight emptyResult;
    auto sendResult = sendSync(Messages::RemotePresentationContext::PrepareForDisplay());
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
#endif

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
