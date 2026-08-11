/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#include "RemoteFaceDetectorProxy.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"

#include "MessageSenderInlines.h"
#include "RemoteFaceDetectorMessages.h"
#include "RemoteRenderingBackendProxy.h"
#include "StreamClientConnection.h"
#include "WebProcess.h"
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/NativeImage.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::ShapeDetection {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteFaceDetectorProxy);

Ref<RemoteFaceDetectorProxy> RemoteFaceDetectorProxy::create(RemoteRenderingBackendProxy& renderingBackend)
{
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=275245): Does not work when GPUP crashes.
    return adoptRef(*new RemoteFaceDetectorProxy(renderingBackend));
}

RemoteFaceDetectorProxy::RemoteFaceDetectorProxy(RemoteRenderingBackendProxy& renderingBackend)
    : m_renderingBackend(renderingBackend)
{
}

RemoteFaceDetectorProxy::~RemoteFaceDetectorProxy()
{
    if (RefPtr renderingBackend = m_renderingBackend.get())
        renderingBackend->releaseFaceDetector(*this);
}

void RemoteFaceDetectorProxy::detect(const WebCore::NativeImage& image, CompletionHandler<void(Vector<WebCore::ShapeDetection::DetectedFace>&&)>&& completionHandler)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    RefPtr connection = renderingBackend ? renderingBackend->connection() : nullptr;
    if (!connection) [[unlikely]] {
        completionHandler({ });
        return;
    }
    if (!renderingBackend->remoteResourceCacheProxy().recordNativeImageUse(image, WebCore::DestinationColorSpace::SRGB())) {
        completionHandler({ });
        return;
    }
    connection->sendWithAsyncReply(Messages::RemoteFaceDetector::Detect(image.renderingResourceIdentifier()), WTF::move(completionHandler), identifier());
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
