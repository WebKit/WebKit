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

#include "ConcreteImageBuffer.h"
#include "DisplayListDrawingContext.h"

namespace WebCore {
namespace DisplayList {

template<typename BackendType>
class ImageBuffer : public ConcreteImageBuffer<BackendType>, public Recorder::Observer {
    using BaseConcreteImageBuffer = ConcreteImageBuffer<BackendType>;

public:
    static auto create(const FloatSize& size, float resolutionScale, ColorSpace colorSpace, const HostWindow* hostWindow)
    {
        return BaseConcreteImageBuffer::template create<ImageBuffer>(size, resolutionScale, colorSpace, hostWindow, size);
    }

    static auto create(const FloatSize& size, const GraphicsContext& context)
    {
        return BaseConcreteImageBuffer::template create<ImageBuffer>(size, context, size);
    }

    ImageBuffer(std::unique_ptr<BackendType>&& dataBackend, const FloatSize& size)
        : BaseConcreteImageBuffer(WTFMove(dataBackend))
        , m_drawingContext(size, this)
    {
    }

    ImageBuffer(const FloatSize& size)
        : m_drawingContext(size, this)
    {
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
        m_drawingContext.replayDisplayList(BaseConcreteImageBuffer::context());
    }

    DrawingContext m_drawingContext;
};

} // DisplayList
} // namespace WebCore
