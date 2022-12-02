/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "RemoteSwapChain.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteSurface.h"
#include "RemoteSwapChainMessages.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include "WebGPUSurfaceDescriptor.h"
#include <pal/graphics/WebGPU/WebGPUSurface.h>
#include <pal/graphics/WebGPU/WebGPUSurfaceDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUSwapChain.h>

namespace WebKit {

RemoteSwapChain::RemoteSwapChain(PAL::WebGPU::SwapChain& swapChain, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(swapChain)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_identifier(identifier)
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteSwapChain::messageReceiverName(), m_identifier.toUInt64());
}

RemoteSwapChain::~RemoteSwapChain() = default;

void RemoteSwapChain::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemoteSwapChain::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteSwapChain::destroy()
{
    m_backing->destroy();
}

void RemoteSwapChain::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

#if PLATFORM(COCOA)
void RemoteSwapChain::prepareForDisplay(CompletionHandler<void(MachSendRight&&)>&& completionHandler)
{
    m_backing->prepareForDisplay(WTFMove(completionHandler));
}
#else
void RemoteSwapChain::prepareForDisplay(CompletionHandler<void()>&&)
{

}
#endif // !PLATFORM(COCOA)


} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
