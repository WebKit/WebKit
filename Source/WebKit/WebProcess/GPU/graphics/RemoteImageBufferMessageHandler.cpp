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
#include "RemoteImageBufferMessageHandler.h"

#if ENABLE(GPU_PROCESS)

#include "ImageDataReference.h"
#include "RemoteRenderingBackend.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include <WebCore/DisplayListItems.h>

namespace WebKit {
using namespace WebCore;

RemoteImageBufferMessageHandler::RemoteImageBufferMessageHandler(const FloatSize& size, RenderingMode renderingMode, float resolutionScale, ColorSpace colorSpace, RemoteRenderingBackend& remoteRenderingBackend)
    : m_remoteRenderingBackend(makeWeakPtr(remoteRenderingBackend))
{
    // Create the RemoteImageBufferProxy.
    m_remoteRenderingBackend->send(Messages::RemoteRenderingBackendProxy::CreateImageBuffer(size, renderingMode, resolutionScale, colorSpace, m_imageBufferIdentifier), m_remoteRenderingBackend->renderingBackendIdentifier());
}

RemoteImageBufferMessageHandler::~RemoteImageBufferMessageHandler()
{
    // Release the RemoteImageBufferProxy.
    if (!m_remoteRenderingBackend)
        return;
    m_remoteRenderingBackend->send(Messages::RemoteRenderingBackendProxy::ReleaseImageBuffer(m_imageBufferIdentifier), m_remoteRenderingBackend->renderingBackendIdentifier());
}

RefPtr<ImageData> RemoteImageBufferMessageHandler::getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect) const
{
    IPC::ImageDataReference imageDataReference;
    m_remoteRenderingBackend->sendSync(Messages::RemoteRenderingBackendProxy::GetImageData(outputFormat, srcRect, m_imageBufferIdentifier), Messages::RemoteRenderingBackendProxy::GetImageData::Reply(imageDataReference), m_remoteRenderingBackend->renderingBackendIdentifier(), 1_s);
    return imageDataReference.buffer();
}

void RemoteImageBufferMessageHandler::waitForCreateImageBufferBackend()
{
    if (m_remoteRenderingBackend && !isBackendCreated())
        m_remoteRenderingBackend->waitForCreateImageBufferBackend();
}

void RemoteImageBufferMessageHandler::waitForCommitImageBufferFlushContext()
{
    if (m_remoteRenderingBackend && isPendingFlush())
        m_remoteRenderingBackend->waitForCommitImageBufferFlushContext();
}

void RemoteImageBufferMessageHandler::flushDrawingContext(WebCore::DisplayList::DisplayList& displayList)
{
    if (!m_remoteRenderingBackend)
        return;
    
    m_remoteRenderingBackend->send(Messages::RemoteRenderingBackendProxy::FlushImageBufferDrawingContext(displayList, m_imageBufferIdentifier), m_remoteRenderingBackend->renderingBackendIdentifier());
    displayList.clear();
}

void RemoteImageBufferMessageHandler::flushDrawingContextAndWaitCommit(WebCore::DisplayList::DisplayList& displayList)
{
    if (!m_remoteRenderingBackend)
        return;
    m_sentFlushIdentifier = ImageBufferFlushIdentifier::generate();
    m_remoteRenderingBackend->send(Messages::RemoteRenderingBackendProxy::FlushImageBufferDrawingContextAndCommit(displayList, m_sentFlushIdentifier, m_imageBufferIdentifier), m_remoteRenderingBackend->renderingBackendIdentifier());
    displayList.clear();
    waitForCommitImageBufferFlushContext();
}

void RemoteImageBufferMessageHandler::commitFlushContext(ImageBufferFlushIdentifier flushIdentifier)
{
    m_receivedFlushIdentifier = flushIdentifier;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
