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
#include "RemoteImageBufferMessageHandlerProxy.h"

#if ENABLE(GPU_PROCESS)

#include "ImageDataReference.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxy.h"
#include <WebCore/DisplayListItems.h>
#include <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

RemoteImageBufferMessageHandlerProxy::RemoteImageBufferMessageHandlerProxy(const FloatSize& size, RenderingMode renderingMode, float resolutionScale, ColorSpace colorSpace, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    : m_remoteRenderingBackendProxy(makeWeakPtr(remoteRenderingBackendProxy))
{
    // Create the RemoteImageBufferProxy.
    m_remoteRenderingBackendProxy->send(Messages::RemoteRenderingBackend::CreateImageBuffer(size, renderingMode, resolutionScale, colorSpace, m_remoteResourceIdentifier), m_remoteRenderingBackendProxy->renderingBackendIdentifier());
}

RemoteImageBufferMessageHandlerProxy::~RemoteImageBufferMessageHandlerProxy()
{
    // Release the RemoteImageBufferProxy.
    if (!m_remoteRenderingBackendProxy)
        return;
    m_remoteRenderingBackendProxy->send(Messages::RemoteRenderingBackend::ReleaseRemoteResource(m_remoteResourceIdentifier), m_remoteRenderingBackendProxy->renderingBackendIdentifier());
}

RefPtr<ImageData> RemoteImageBufferMessageHandlerProxy::getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect) const
{
    IPC::ImageDataReference imageDataReference;
    m_remoteRenderingBackendProxy->sendSync(Messages::RemoteRenderingBackend::GetImageData(outputFormat, srcRect, m_remoteResourceIdentifier), Messages::RemoteRenderingBackend::GetImageData::Reply(imageDataReference), m_remoteRenderingBackendProxy->renderingBackendIdentifier(), 1_s);
    return imageDataReference.buffer();
}

void RemoteImageBufferMessageHandlerProxy::waitForImageBufferBackendWasCreated()
{
    if (m_remoteRenderingBackendProxy && !isBackendCreated())
        m_remoteRenderingBackendProxy->waitForImageBufferBackendWasCreated();
}

void RemoteImageBufferMessageHandlerProxy::waitForFlushDisplayListWasCommitted()
{
    if (m_remoteRenderingBackendProxy && isPendingFlush())
        m_remoteRenderingBackendProxy->waitForFlushDisplayListWasCommitted();
}

void RemoteImageBufferMessageHandlerProxy::flushDisplayList(WebCore::DisplayList::DisplayList& displayList)
{
    if (!m_remoteRenderingBackendProxy)
        return;
    
    TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);
    m_remoteRenderingBackendProxy->send(Messages::RemoteRenderingBackend::FlushDisplayList(displayList, m_remoteResourceIdentifier), m_remoteRenderingBackendProxy->renderingBackendIdentifier());
    displayList.clear();
}

void RemoteImageBufferMessageHandlerProxy::flushDisplayListAndWaitCommit(WebCore::DisplayList::DisplayList& displayList)
{
    if (!m_remoteRenderingBackendProxy)
        return;

    TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd, 1);
    m_sentFlushIdentifier = DisplayListFlushIdentifier::generate();
    m_remoteRenderingBackendProxy->send(Messages::RemoteRenderingBackend::FlushDisplayListAndCommit(displayList, m_sentFlushIdentifier, m_remoteResourceIdentifier), m_remoteRenderingBackendProxy->renderingBackendIdentifier());
    displayList.clear();
    waitForFlushDisplayListWasCommitted();
}

void RemoteImageBufferMessageHandlerProxy::commitFlushContext(DisplayListFlushIdentifier flushIdentifier)
{
    m_receivedFlushIdentifier = flushIdentifier;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
