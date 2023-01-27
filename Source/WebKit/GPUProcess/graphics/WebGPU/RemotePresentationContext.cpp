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
#include <pal/graphics/WebGPU/WebGPUPresentationConfiguration.h>
#include <pal/graphics/WebGPU/WebGPUPresentationContext.h>

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

void RemotePresentationContext::configure(const WebGPU::PresentationConfiguration& presentationConfiguration)
{
    auto convertedConfiguration = m_objectHeap.convertFromBacking(presentationConfiguration);
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
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=250958 Figure out how the lifetime of these objects should behave.
}

#if PLATFORM(COCOA)
void RemotePresentationContext::prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&& completionHandler)
{
    m_backing->prepareForDisplay(WTFMove(completionHandler));
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
