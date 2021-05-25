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

#include "ConcreteImageBuffer.h"
#include "DisplayListDrawingContext.h"
#include "InMemoryDisplayList.h"

namespace WebCore {
namespace DisplayList {

template<typename BackendType>
class ImageBuffer : public ConcreteImageBuffer<BackendType> {
    using BaseConcreteImageBuffer = ConcreteImageBuffer<BackendType>;
    using BaseConcreteImageBuffer::logicalSize;
    using BaseConcreteImageBuffer::baseTransform;

public:
    static auto create(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, const HostWindow* hostWindow)
    {
        return BaseConcreteImageBuffer::template create<ImageBuffer>(size, resolutionScale, colorSpace, pixelFormat, hostWindow);
    }

    static auto create(const FloatSize& size, const GraphicsContext& context)
    {
        return BaseConcreteImageBuffer::template create<ImageBuffer>(size, context);
    }

    ImageBuffer(const ImageBufferBackend::Parameters& parameters, std::unique_ptr<BackendType>&& backend)
        : BaseConcreteImageBuffer(parameters, WTFMove(backend))
        , m_drawingContext(logicalSize(), baseTransform())
        , m_writingClient(WTF::makeUnique<InMemoryDisplayList::WritingClient>())
        , m_readingClient(WTF::makeUnique<InMemoryDisplayList::ReadingClient>())
    {
        m_drawingContext.displayList().setItemBufferWritingClient(m_writingClient.get());
        m_drawingContext.displayList().setItemBufferReadingClient(m_readingClient.get());
    }

    ImageBuffer(const ImageBufferBackend::Parameters& parameters, Recorder::Delegate* delegate = nullptr)
        : BaseConcreteImageBuffer(parameters)
        , m_drawingContext(logicalSize(), baseTransform(), delegate)
        , m_writingClient(WTF::makeUnique<InMemoryDisplayList::WritingClient>())
        , m_readingClient(WTF::makeUnique<InMemoryDisplayList::ReadingClient>())
    {
        m_drawingContext.displayList().setItemBufferWritingClient(m_writingClient.get());
        m_drawingContext.displayList().setItemBufferReadingClient(m_readingClient.get());
    }

    ~ImageBuffer()
    {
        flushDrawingContext();
    }

    GraphicsContext& context() const override
    {
        return m_drawingContext.context();
    }

    DrawingContext* drawingContext() override { return &m_drawingContext; }

    void flushDrawingContext() override
    {
        if (!m_drawingContext.displayList().isEmpty())
            m_drawingContext.replayDisplayList(BaseConcreteImageBuffer::context());
    }

    void clearBackend() override
    {
        m_drawingContext.displayList().clear();
        BaseConcreteImageBuffer::clearBackend();
    }

protected:
    DrawingContext m_drawingContext;
    std::unique_ptr<ItemBufferWritingClient> m_writingClient;
    std::unique_ptr<ItemBufferReadingClient> m_readingClient;
};

} // DisplayList
} // namespace WebCore
