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
#include "RemotePresentationContext.h"

#if ENABLE(GPU_PROCESS)

#include "RemotePresentationContextMessages.h"
#include "RemoteTexture.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <pal/graphics/WebGPU/WebGPUCanvasConfiguration.h>
#include <pal/graphics/WebGPU/WebGPUPresentationContext.h>
#include <pal/graphics/WebGPU/WebGPUTexture.h>

namespace WebKit {

RemotePresentationContext::RemotePresentationContext(PAL::WebGPU::PresentationContext& presentationContext, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(presentationContext)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_identifier(identifier)
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemotePresentationContext::messageReceiverName(), m_identifier.toUInt64());
}

RemotePresentationContext::~RemotePresentationContext() = default;

void RemotePresentationContext::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemotePresentationContext::messageReceiverName(), m_identifier.toUInt64());
}

void RemotePresentationContext::configure(const WebGPU::CanvasConfiguration& canvasConfiguration)
{
    auto convertedConfiguration = m_objectHeap.convertFromBacking(canvasConfiguration);
    ASSERT(convertedConfiguration);
    if (!convertedConfiguration)
        return;

    m_backing->configure(*convertedConfiguration);
}

void RemotePresentationContext::unconfigure()
{
    m_backing->unconfigure();
}

void RemotePresentationContext::getCurrentTexture(WebGPUIdentifier identifier)
{
    auto texture = m_backing->getCurrentTexture();
    // We're creating a new resource here, because we don't want the GetCurrentTexture message to be sync.
    // If the message is async, then the WebGPUIdentifier goes from the Web process to the GPU Process, which
    // means the Web Process is going to proceed and interact with the texture as-if it has this identifier.
    // So we need to make sure the texture has this identifier.
    // Maybe one day we could add the same texture into the ObjectHeap multiple times under multiple identifiers,
    // but for now let's just create a new RemoteTexture object with the expected identifier, just for simplicity.
    // The Web Process should already be caching these current textures internally, so it's unlikely that we'll
    // actually run into a problem here.
    // FIXME: Handle the situation where texture is nullptr.
    auto remoteTexture = RemoteTexture::create(*texture, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteTexture);
}

void RemotePresentationContext::present()
{
    m_backing->present();
}

#if PLATFORM(COCOA)
void RemotePresentationContext::prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&& completionHandler)
{
    m_backing->prepareForDisplay(WTFMove(completionHandler));
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
