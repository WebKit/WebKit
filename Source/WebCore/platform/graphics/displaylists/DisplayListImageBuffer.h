/*
 * Copyright (C) 2020-2021 Apple Inc.  All rights reserved.
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

#include "DisplayListDrawingContext.h"
#include "ImageBuffer.h"
#include "InMemoryDisplayList.h"

namespace WebCore {
namespace DisplayList {

class ImageBuffer final : public WebCore::ImageBuffer {
public:
    template<typename BackendType>
    static auto create(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, RenderingPurpose purpose, const WebCore::ImageBufferCreationContext& creationContext)
    {
        return WebCore::ImageBuffer::create<BackendType, ImageBuffer>(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
    }

    template<typename BackendType>
    static auto create(const FloatSize& size, const GraphicsContext& context, RenderingPurpose purpose)
    {
        return WebCore::ImageBuffer::create<BackendType, ImageBuffer>(size, context, purpose);
    }

    ImageBuffer(const ImageBufferBackend::Parameters& parameters, const ImageBufferBackend::Info& info, std::unique_ptr<ImageBufferBackend>&& backend)
        : WebCore::ImageBuffer(parameters, info, WTFMove(backend))
        , m_drawingContext(logicalSize(), baseTransform())
        , m_writingClient(makeUnique<InMemoryDisplayList::WritingClient>())
        , m_readingClient(makeUnique<InMemoryDisplayList::ReadingClient>())
    {
        m_drawingContext.displayList().setItemBufferWritingClient(m_writingClient.get());
        m_drawingContext.displayList().setItemBufferReadingClient(m_readingClient.get());
    }

    ImageBuffer(const ImageBufferBackend::Parameters& parameters, const ImageBufferBackend::Info& info)
        : WebCore::ImageBuffer(parameters, info)
        , m_drawingContext(logicalSize(), baseTransform())
        , m_writingClient(makeUnique<InMemoryDisplayList::WritingClient>())
        , m_readingClient(makeUnique<InMemoryDisplayList::ReadingClient>())
    {
        m_drawingContext.displayList().setItemBufferWritingClient(m_writingClient.get());
        m_drawingContext.displayList().setItemBufferReadingClient(m_readingClient.get());
    }

    ~ImageBuffer()
    {
        flushDrawingContext();
    }

    GraphicsContext& context() const final
    {
        return m_drawingContext.context();
    }

    void flushDrawingContext() final
    {
        if (!m_drawingContext.displayList().isEmpty())
            m_drawingContext.replayDisplayList(WebCore::ImageBuffer::context());
    }

protected:
    DrawingContext m_drawingContext;
    std::unique_ptr<ItemBufferWritingClient> m_writingClient;
    std::unique_ptr<ItemBufferReadingClient> m_readingClient;
};

} // DisplayList
} // namespace WebCore
