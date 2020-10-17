/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS)

#include "DisplayListFlushIdentifier.h"
#include "ImageBufferBackendHandle.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/DisplayList.h>
#include <WebCore/FloatSize.h>
#include <WebCore/RemoteResourceIdentifier.h>
#include <WebCore/RenderingMode.h>

namespace WebCore {
class ImageData;
enum class AlphaPremultiplication : uint8_t;
}

namespace WebKit {

class RemoteRenderingBackendProxy;

class RemoteImageBufferMessageHandlerProxy {
public:
    virtual ~RemoteImageBufferMessageHandlerProxy();

    WebCore::RemoteResourceIdentifier remoteResourceIdentifier() const { return m_remoteResourceIdentifier; }

    // Messages to be received. See RemoteRenderingBackend.messages.in.
    virtual void createBackend(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& backendSize, float resolutionScale, WebCore::ColorSpace, ImageBufferBackendHandle) = 0;

    void commitFlushContext(DisplayListFlushIdentifier);

protected:
    RemoteImageBufferMessageHandlerProxy(const WebCore::FloatSize&, WebCore::RenderingMode, float resolutionScale, WebCore::ColorSpace, RemoteRenderingBackendProxy&);

    virtual bool isBackendCreated() const = 0;
    bool isPendingFlush() const { return m_sentFlushIdentifier != m_receivedFlushIdentifier; }

    RefPtr<WebCore::ImageData> getImageData(WebCore::AlphaPremultiplication outputFormat, const WebCore::IntRect& srcRect) const;

    void waitForImageBufferBackendWasCreated();
    void waitForFlushDisplayListWasCommitted();

    // Messages to be sent. See RemoteRenderingBackendProxy.messages.in.
    void flushDisplayList(WebCore::DisplayList::DisplayList&);
    void flushDisplayListAndWaitCommit(WebCore::DisplayList::DisplayList&);

private:
    WeakPtr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    WebCore::RemoteResourceIdentifier m_remoteResourceIdentifier { WebCore::RemoteResourceIdentifier::generate() };

    DisplayListFlushIdentifier m_sentFlushIdentifier;
    DisplayListFlushIdentifier m_receivedFlushIdentifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
