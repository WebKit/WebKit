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
#include "RemoteXRSubImage.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "RemoteGPU.h"
#include "RemoteTexture.h"
#include "RemoteXRSubImageMessages.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPUTexture.h>
#include <WebCore/WebGPUXRSubImage.h>
#include <wtf/TZoneMalloc.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_streamConnection)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteXRSubImage);

RemoteXRSubImage::RemoteXRSubImage(GPUConnectionToWebProcess& gpuConnectionToWebProcess, WebCore::WebGPU::XRSubImage& xrSubImage, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    : m_backing(xrSubImage)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_identifier(identifier)
    , m_gpu(gpu)
{
    protectedStreamConnection()->startReceivingMessages(*this, Messages::RemoteXRSubImage::messageReceiverName(), m_identifier.toUInt64());
}

RemoteXRSubImage::~RemoteXRSubImage() = default;

Ref<IPC::StreamServerConnection> RemoteXRSubImage::protectedStreamConnection()
{
    return m_streamConnection;
}

Ref<WebCore::WebGPU::XRSubImage> RemoteXRSubImage::protectedBacking()
{
    return m_backing;
}

Ref<RemoteGPU> RemoteXRSubImage::protectedGPU() const
{
    return m_gpu.get();
}

void RemoteXRSubImage::destruct()
{
    Ref { m_objectHeap.get() }->removeObject(m_identifier);
}

void RemoteXRSubImage::getColorTexture(WebGPUIdentifier identifier)
{
    auto texture = protectedBacking()->colorTexture();
    ASSERT(texture);
    auto connection = m_gpuConnectionToWebProcess.get();
    if (!texture || !connection)
        return;

    Ref objectHeap = m_objectHeap.get();
    auto remoteTexture = RemoteTexture::create(*connection, protectedGPU(), *texture, objectHeap, protectedStreamConnection(), identifier);
    objectHeap->addObject(identifier, remoteTexture);
}

void RemoteXRSubImage::getDepthTexture(WebGPUIdentifier identifier)
{
    auto texture = protectedBacking()->depthStencilTexture();
    ASSERT(texture);
    auto connection = m_gpuConnectionToWebProcess.get();
    if (!texture || !connection)
        return;

    Ref objectHeap = m_objectHeap.get();
    auto remoteTexture = RemoteTexture::create(*connection, protectedGPU(), *texture, objectHeap, protectedStreamConnection(), identifier);
    objectHeap->addObject(identifier, remoteTexture);
}

void RemoteXRSubImage::stopListeningForIPC()
{
    protectedStreamConnection()->stopReceivingMessages(Messages::RemoteXRSubImage::messageReceiverName(), m_identifier.toUInt64());
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
