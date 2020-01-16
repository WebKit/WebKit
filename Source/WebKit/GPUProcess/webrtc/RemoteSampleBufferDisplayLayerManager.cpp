/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteSampleBufferDisplayLayerManager.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)

#include "Decoder.h"
#include "RemoteSampleBufferDisplayLayer.h"

namespace WebKit {

RemoteSampleBufferDisplayLayerManager::RemoteSampleBufferDisplayLayerManager(Ref<IPC::Connection>&& connection)
    : m_connection(WTFMove(connection))
{
}

RemoteSampleBufferDisplayLayerManager::~RemoteSampleBufferDisplayLayerManager() = default;

void RemoteSampleBufferDisplayLayerManager::didReceiveLayerMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (auto* layer = m_layers.get(makeObjectIdentifier<SampleBufferDisplayLayerIdentifierType>(decoder.destinationID())))
        layer->didReceiveMessage(connection, decoder);
}

// FIXME: We should refactor code to use an asynchronous IPC.
void RemoteSampleBufferDisplayLayerManager::createLayer(SampleBufferDisplayLayerIdentifier identifier, bool hideRootLayer, IntSize size, Messages::RemoteSampleBufferDisplayLayerManager::CreateLayerDelayedReply&& reply)
{
    ASSERT(!m_layers.contains(identifier));
    auto layer = RemoteSampleBufferDisplayLayer::create(identifier, m_connection.copyRef(), hideRootLayer, size);
    if (!layer)
        return reply({ }, { });

    reply(layer->contextID(), layer->bounds());
    m_layers.add(identifier, WTFMove(layer));
}

void RemoteSampleBufferDisplayLayerManager::releaseLayer(SampleBufferDisplayLayerIdentifier identifier)
{
    ASSERT(m_layers.contains(identifier));
    m_layers.remove(identifier);
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)
