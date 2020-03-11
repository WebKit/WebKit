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
#include "RemoteRenderingBackendProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "PlatformRemoteImageBufferProxy.h"
#include "RemoteRenderingBackendProxyMessages.h"

namespace WebKit {
using namespace WebCore;

std::unique_ptr<RemoteRenderingBackendProxy> RemoteRenderingBackendProxy::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RenderingBackendIdentifier renderingBackendIdentifier)
{
    return std::unique_ptr<RemoteRenderingBackendProxy>(new RemoteRenderingBackendProxy(gpuConnectionToWebProcess, renderingBackendIdentifier));
}

RemoteRenderingBackendProxy::RemoteRenderingBackendProxy(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RenderingBackendIdentifier renderingBackendIdentifier)
    : m_gpuConnectionToWebProcess(makeWeakPtr(gpuConnectionToWebProcess))
    , m_renderingBackendIdentifier(renderingBackendIdentifier)
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        gpuConnectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteRenderingBackendProxy::messageReceiverName(), renderingBackendIdentifier.toUInt64(), *this);
}

RemoteRenderingBackendProxy::~RemoteRenderingBackendProxy()
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        gpuConnectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteRenderingBackendProxy::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
}

IPC::Connection* RemoteRenderingBackendProxy::messageSenderConnection() const
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        return &gpuConnectionToWebProcess->connection();
    return nullptr;
}

uint64_t RemoteRenderingBackendProxy::messageSenderDestinationID() const
{
    return m_renderingBackendIdentifier.toUInt64();
}

void RemoteRenderingBackendProxy::createImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, float resolutionScale, ColorSpace colorSpace, ImageBufferIdentifier imageBufferIdentifier)
{
    auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get();
    if (!gpuConnectionToWebProcess)
        return;

    if (m_imageBufferMessageHandlerMap.contains(imageBufferIdentifier)) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (renderingMode == RenderingMode::RemoteAccelerated) {
        if (auto imageBuffer = AcceleratedRemoteImageBufferProxy::create(logicalSize, resolutionScale, colorSpace, *this, imageBufferIdentifier)) {
            m_imageBufferMessageHandlerMap.add(imageBufferIdentifier, WTFMove(imageBuffer));
            return;
        }
    }

    if (renderingMode == RenderingMode::RemoteAccelerated || renderingMode == RenderingMode::RemoteUnaccelerated) {
        if (auto imageBuffer = UnacceleratedRemoteImageBufferProxy::create(logicalSize, resolutionScale, colorSpace, *this, imageBufferIdentifier)) {
            m_imageBufferMessageHandlerMap.add(imageBufferIdentifier, WTFMove(imageBuffer));
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

void RemoteRenderingBackendProxy::releaseImageBuffer(ImageBufferIdentifier imageBufferIdentifier)
{
    // CreateImageBuffer message should have been received before this one.
    bool found = m_imageBufferMessageHandlerMap.remove(imageBufferIdentifier);
    ASSERT_UNUSED(found, found);
}

void RemoteRenderingBackendProxy::getImageData(WebCore::AlphaPremultiplication outputFormat, WebCore::IntRect srcRect, ImageBufferIdentifier imageBufferIdentifier, CompletionHandler<void(IPC::ImageDataReference&&)>&& completionHandler)
{
    if (auto imageBuffer = m_imageBufferMessageHandlerMap.get(imageBufferIdentifier)) {
        auto imageData = imageBuffer->getImageData(outputFormat, srcRect);
        completionHandler(IPC::ImageDataReference(WTFMove(imageData)));
    }
}

void RemoteRenderingBackendProxy::flushImageBufferDrawingContext(const WebCore::DisplayList::DisplayList& displayList, ImageBufferIdentifier imageBufferIdentifier)
{
    if (auto imageBuffer = m_imageBufferMessageHandlerMap.get(imageBufferIdentifier))
        imageBuffer->flushDrawingContext(displayList);
}

void RemoteRenderingBackendProxy::flushImageBufferDrawingContextAndCommit(const WebCore::DisplayList::DisplayList& displayList, ImageBufferFlushIdentifier flushIdentifier, ImageBufferIdentifier imageBufferIdentifier)
{
    if (auto imageBuffer = m_imageBufferMessageHandlerMap.get(imageBufferIdentifier))
        imageBuffer->flushDrawingContextAndCommit(displayList, flushIdentifier);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
