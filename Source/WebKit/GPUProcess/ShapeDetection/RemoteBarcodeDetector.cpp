/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "RemoteBarcodeDetector.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"
#include "RemoteRenderingBackend.h"
#include "RemoteResourceCache.h"
#include "ShapeDetectionObjectHeap.h"
#include <WebCore/BarcodeDetectorInterface.h>
#include <WebCore/DetectedBarcodeInterface.h>
#include <WebCore/ImageBuffer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteBarcodeDetector);

RemoteBarcodeDetector::RemoteBarcodeDetector(Ref<WebCore::ShapeDetection::BarcodeDetector>&& barcodeDetector, ShapeDetection::ObjectHeap& objectHeap, RemoteRenderingBackend& backend, ShapeDetectionIdentifier identifier, WebCore::ProcessIdentifier webProcessIdentifier)
    : m_backing(WTFMove(barcodeDetector))
    , m_objectHeap(objectHeap)
    , m_backend(backend)
    , m_identifier(identifier)
    , m_webProcessIdentifier(webProcessIdentifier)
{
}

RemoteBarcodeDetector::~RemoteBarcodeDetector() = default;

const SharedPreferencesForWebProcess& RemoteBarcodeDetector::sharedPreferencesForWebProcess() const
{
    return m_backend->sharedPreferencesForWebProcess();
}

Ref<WebCore::ShapeDetection::BarcodeDetector> RemoteBarcodeDetector::protectedBacking()
{
    return backing();
}

Ref<RemoteRenderingBackend> RemoteBarcodeDetector::protectedBackend()
{
    return m_backend.get();
}

void RemoteBarcodeDetector::detect(WebCore::RenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(Vector<WebCore::ShapeDetection::DetectedBarcode>&&)>&& completionHandler)
{
    auto sourceImage = protectedBackend()->imageBuffer(renderingResourceIdentifier);
    if (!sourceImage) {
        completionHandler({ });
        return;
    }

    protectedBacking()->detect(*sourceImage, WTFMove(completionHandler));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
