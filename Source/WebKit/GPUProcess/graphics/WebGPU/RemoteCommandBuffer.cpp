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
#include "RemoteCommandBuffer.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteCommandBufferMessages.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPUCommandBuffer.h>

namespace WebKit {

RemoteCommandBuffer::RemoteCommandBuffer(WebCore::WebGPU::CommandBuffer& commandBuffer, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(commandBuffer)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_identifier(identifier)
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteCommandBuffer::messageReceiverName(), m_identifier.toUInt64());
}

RemoteCommandBuffer::~RemoteCommandBuffer() = default;

void RemoteCommandBuffer::destruct()
{
    m_objectHeap.removeObject(m_identifier);
}

void RemoteCommandBuffer::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemoteCommandBuffer::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteCommandBuffer::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
