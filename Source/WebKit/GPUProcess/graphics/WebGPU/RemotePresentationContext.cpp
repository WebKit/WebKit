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

#include "GPUConnectionToWebProcess.h"
#include "RemotePresentationContextMessages.h"
#include "RemoteTexture.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPUCanvasConfiguration.h>
#include <WebCore/WebGPUPresentationContext.h>
#include <WebCore/WebGPUTexture.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemotePresentationContext);

RemotePresentationContext::RemotePresentationContext(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, WebCore::WebGPU::PresentationContext& presentationContext, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(presentationContext)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_identifier(identifier)
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_gpu(gpu)
{
    protectedStreamConnection()->startReceivingMessages(*this, Messages::RemotePresentationContext::messageReceiverName(), m_identifier.toUInt64());
}

RemotePresentationContext::~RemotePresentationContext() = default;

void RemotePresentationContext::stopListeningForIPC()
{
    protectedStreamConnection()->stopReceivingMessages(Messages::RemotePresentationContext::messageReceiverName(), m_identifier.toUInt64());
}

void RemotePresentationContext::configure(const WebGPU::CanvasConfiguration& canvasConfiguration)
{
    auto convertedConfiguration = m_objectHeap->convertFromBacking(canvasConfiguration);
    ASSERT(convertedConfiguration);
    if (!convertedConfiguration)
        return;

    bool success = protectedBacking()->configure(*convertedConfiguration);
    ASSERT_UNUSED(success, success);
}

void RemotePresentationContext::unconfigure()
{
    protectedBacking()->unconfigure();
}

void RemotePresentationContext::present()
{
    protectedBacking()->present();
}

void RemotePresentationContext::getCurrentTexture(WebGPUIdentifier identifier)
{
    auto texture = protectedBacking()->getCurrentTexture();
    ASSERT(texture);
    auto connection = m_gpuConnectionToWebProcess.get();
    if (!texture || !connection)
        return;

    // We're creating a new resource here, because we don't want the GetCurrentTexture message to be sync.
    // If the message is async, then the WebGPUIdentifier goes from the Web process to the GPU Process, which
    // means the Web Process is going to proceed and interact with the texture as-if it has this identifier.
    // So we need to make sure the texture has this identifier.
    // Maybe one day we could add the same texture into the ObjectHeap multiple times under multiple identifiers,
    // but for now let's just create a new RemoteTexture object with the expected identifier, just for simplicity.
    // The Web Process should already be caching these current textures internally, so it's unlikely that we'll
    // actually run into a problem here.
    Ref objectHeap = m_objectHeap.get();
    auto remoteTexture = RemoteTexture::create(*connection, protectedGPU(), *texture, objectHeap, m_streamConnection.copyRef(), identifier);
    objectHeap->addObject(identifier, remoteTexture);
}

Ref<WebCore::WebGPU::PresentationContext> RemotePresentationContext::protectedBacking()
{
    return m_backing;
}

Ref<WebGPU::ObjectHeap> RemotePresentationContext::protectedObjectHeap() const
{
    return m_objectHeap.get();
}

Ref<IPC::StreamServerConnection> RemotePresentationContext::protectedStreamConnection() const
{
    return m_streamConnection;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
