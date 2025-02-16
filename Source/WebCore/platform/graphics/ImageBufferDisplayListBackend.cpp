/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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
#include "ImageBufferDisplayListBackend.h"

#include "ImageBuffer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

std::unique_ptr<ImageBufferDisplayListBackend> ImageBufferDisplayListBackend::create(const Parameters& parameters, const ImageBufferCreationContext&)
{
    return std::unique_ptr<ImageBufferDisplayListBackend>(new ImageBufferDisplayListBackend(parameters));
}

ImageBufferDisplayListBackend::ImageBufferDisplayListBackend(const Parameters& parameters)
    : ImageBufferBackend(parameters)
    , m_drawingContext(parameters.backendSize)
{
}

GraphicsContext& ImageBufferDisplayListBackend::context()
{
    return m_drawingContext.context();
}

RefPtr<NativeImage> ImageBufferDisplayListBackend::copyNativeImage()
{
    RefPtr buffer = ImageBuffer::create(size(), RenderingMode::Unaccelerated, RenderingPurpose::Snapshot, 1, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (!buffer)
        return nullptr;

    auto& context = buffer->context();
    m_drawingContext.replayDisplayList(context);

    return ImageBuffer::sinkIntoNativeImage(WTFMove(buffer));
}

RefPtr<SharedBuffer> ImageBufferDisplayListBackend::sinkIntoPDFDocument()
{
    RefPtr buffer = ImageBuffer::create(size(), RenderingMode::PDFDocument, RenderingPurpose::Snapshot, 1, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (!buffer)
        return nullptr;

    auto& context = buffer->context();
    m_drawingContext.replayDisplayList(context);

    return ImageBuffer::sinkIntoPDFDocument(WTFMove(buffer));
}

String ImageBufferDisplayListBackend::debugDescription() const
{
    TextStream stream;
    stream << "ImageBufferDisplayListBackend " << this;
    return stream.release();
}

} // namespace WebCore
