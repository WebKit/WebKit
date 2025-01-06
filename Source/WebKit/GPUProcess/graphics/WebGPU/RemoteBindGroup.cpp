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
#include "RemoteBindGroup.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteBindGroupMessages.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPUBindGroup.h>
#include <WebCore/WebGPUExternalTexture.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteBindGroup);

RemoteBindGroup::RemoteBindGroup(WebCore::WebGPU::BindGroup& bindGroup, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    : m_backing(bindGroup)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_gpu(gpu)
    , m_identifier(identifier)
{
    protectedStreamConnection()->startReceivingMessages(*this, Messages::RemoteBindGroup::messageReceiverName(), m_identifier.toUInt64());
}

RemoteBindGroup::~RemoteBindGroup() = default;

void RemoteBindGroup::destruct()
{
    protectedObjectHeap()->removeObject(m_identifier);
}

void RemoteBindGroup::updateExternalTextures(WebGPUIdentifier externalTextureIdentifier, CompletionHandler<void(bool)>&& completion)
{
    if (auto externalTexture = protectedObjectHeap()->convertExternalTextureFromBacking(externalTextureIdentifier); externalTexture.get())
        completion(protectedBacking()->updateExternalTextures(*externalTexture.get()));
    else
        completion(false);
}

void RemoteBindGroup::stopListeningForIPC()
{
    protectedStreamConnection()->stopReceivingMessages(Messages::RemoteBindGroup::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteBindGroup::setLabel(String&& label)
{
    protectedBacking()->setLabel(WTFMove(label));
}

Ref<WebCore::WebGPU::BindGroup> RemoteBindGroup::protectedBacking()
{
    return m_backing;
}

Ref<IPC::StreamServerConnection> RemoteBindGroup::protectedStreamConnection() const
{
    return m_streamConnection;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
