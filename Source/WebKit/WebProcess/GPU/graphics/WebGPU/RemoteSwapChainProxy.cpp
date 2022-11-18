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
#include "RemoteSwapChainProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteSurfaceProxy.h"
#include "RemoteSwapChainMessages.h"
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
