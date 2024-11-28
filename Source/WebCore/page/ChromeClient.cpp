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

#include "config.h"
#include "ChromeClient.h"

#include "BarcodeDetectorInterface.h"
#include "BarcodeDetectorOptionsInterface.h"
#include "BarcodeFormatInterface.h"
#include "FaceDetectorInterface.h"
#include "FaceDetectorOptionsInterface.h"
#include "ScrollbarsController.h"
#include "ScrollingCoordinator.h"
#include "TextDetectorInterface.h"
#include "WorkerClient.h"

#if ENABLE(WEBGL)
#include "GraphicsContextGL.h"
#endif

namespace WebCore {

ChromeClient::ChromeClient() = default;

ChromeClient::~ChromeClient() = default;

std::unique_ptr<WorkerClient> ChromeClient::createWorkerClient(SerialFunctionDispatcher&)
{
    return nullptr;
}

#if ENABLE(WEBGL)
RefPtr<GraphicsContextGL> ChromeClient::createGraphicsContextGL(const GraphicsContextGLAttributes& attributes) const
{
    return createWebProcessGraphicsContextGL(attributes);
}
#endif

RefPtr<ImageBuffer> ChromeClient::sinkIntoImageBuffer(std::unique_ptr<WebCore::SerializedImageBuffer> imageBuffer)
{
    return SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(imageBuffer));
}

void ChromeClient::ensureScrollbarsController(Page&, ScrollableArea& area, bool update) const
{
    if (update)
        return;

    area.ScrollableArea::createScrollbarsController();
}

RefPtr<ScrollingCoordinator> ChromeClient::createScrollingCoordinator(Page&) const
{
    return nullptr;
}

RefPtr<ShapeDetection::BarcodeDetector> ChromeClient::createBarcodeDetector(const ShapeDetection::BarcodeDetectorOptions&) const
{
    return nullptr;
}

void ChromeClient::getBarcodeDetectorSupportedFormats(CompletionHandler<void(Vector<ShapeDetection::BarcodeFormat>&&)>&& completionHandler) const
{
    completionHandler({ });
}

RefPtr<ShapeDetection::FaceDetector> ChromeClient::createFaceDetector(const ShapeDetection::FaceDetectorOptions&) const
{
    return nullptr;
}

RefPtr<ShapeDetection::TextDetector> ChromeClient::createTextDetector() const
{
    return nullptr;
}

} // namespace WebCore
