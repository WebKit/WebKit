/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#include "PlaceholderRenderingContext.h"

#if ENABLE(OFFSCREEN_CANVAS)

#include "ContextDestructionObserverInlines.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "HTMLCanvasElement.h"
#include "NodeInlines.h"
#include "OffscreenCanvas.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaceholderRenderingContextSource);

Ref<PlaceholderRenderingContextSource> PlaceholderRenderingContextSource::create(PlaceholderRenderingContext& context)
{
    return adoptRef(*new PlaceholderRenderingContextSource(context));
}

PlaceholderRenderingContextSource::PlaceholderRenderingContextSource(PlaceholderRenderingContext& placeholder)
    : m_placeholder(placeholder)
{
}

void PlaceholderRenderingContextSource::setPlaceholderBuffer(ImageBuffer& imageBuffer, bool originClean, bool opaque)
{
    auto bufferVersion = ++m_bufferVersion;
    {
        Locker locker { m_lock };
        if (m_delegate) {
            m_delegate->tryCopyToLayer(imageBuffer, opaque);
            m_delegateBufferVersion = bufferVersion;
        }
    }

    RefPtr clone = imageBuffer.clone();
    if (!clone)
        return;
    std::unique_ptr serializedClone = ImageBuffer::sinkIntoSerializedImageBuffer(WTF::move(clone));
    if (!serializedClone)
        return;
    callOnMainThread([weakPlaceholder = m_placeholder, buffer = WTF::move(serializedClone), bufferVersion, originClean, opaque] () mutable {
        assertIsMainThread();
        RefPtr placeholder = weakPlaceholder.get();
        if (!placeholder)
            return;
        RefPtr imageBuffer = SerializedImageBuffer::sinkIntoImageBuffer(WTF::move(buffer), protect(protect(placeholder->canvas())->scriptExecutionContext())->graphicsClient());
        if (!imageBuffer)
            return;
        Ref source = placeholder->source();
        {
            Locker locker { source->m_lock };
            if (source->m_delegate && source->m_delegateBufferVersion < bufferVersion) {
                // Compare the versions, so that possibly already historical buffer in this
                // main thread task does not override the newest buffer that the worker thread
                // already set.
                source->m_delegate->tryCopyToLayer(*imageBuffer, opaque);
                source->m_delegateBufferVersion = bufferVersion;
            }
        }

        placeholder->setPlaceholderBuffer(imageBuffer.releaseNonNull(), originClean, opaque);
        source->m_placeholderBufferVersion = bufferVersion;
    });
}

void PlaceholderRenderingContextSource::setContentsToLayer(GraphicsLayer& layer, ImageBuffer* buffer, bool opaque)
{
    assertIsMainThread();
    Locker locker { m_lock };
    if ((m_delegate = layer.createAsyncContentsDisplayDelegate(m_delegate.get()))) {
        if (buffer) {
            m_delegate->tryCopyToLayer(*buffer, opaque);
            m_delegateBufferVersion = m_placeholderBufferVersion;
        }
    }
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaceholderRenderingContext);

std::unique_ptr<PlaceholderRenderingContext> PlaceholderRenderingContext::create(HTMLCanvasElement& element)
{
    return std::unique_ptr<PlaceholderRenderingContext> { new PlaceholderRenderingContext(element) };
}

PlaceholderRenderingContext::PlaceholderRenderingContext(HTMLCanvasElement& canvas)
    : CanvasRenderingContext(canvas, Type::Placeholder)
    , m_source(PlaceholderRenderingContextSource::create(*this))
{
}

HTMLCanvasElement& PlaceholderRenderingContext::canvas() const
{
    return downcast<HTMLCanvasElement>(canvasBase());
}

IntSize PlaceholderRenderingContext::size() const
{
    return canvas().size();
}

void PlaceholderRenderingContext::setContentsToLayer(GraphicsLayer& layer)
{
    m_source->setContentsToLayer(layer, m_buffer.get(), m_opaque);
}

void PlaceholderRenderingContext::setPlaceholderBuffer(Ref<ImageBuffer>&& newBuffer, bool originClean, bool opaque)
{
    m_opaque = opaque;
    IntSize newSize = newBuffer->truncatedLogicalSize();
    updateMemoryCost(newBuffer->memoryCost());
    m_buffer = WTF::move(newBuffer);
    Ref canvas = this->canvas();
    canvas->setSizeForControllingContext(newSize);
    if (originClean)
        canvas->setOriginClean();
    else
        canvas->setOriginTainted();
    canvas->didDraw(FloatRect { { }, newSize }, ShouldApplyPostProcessingToDirtyRect::No);
}

PixelFormat PlaceholderRenderingContext::pixelFormat() const
{
    if (RefPtr buffer = m_buffer)
        return buffer->pixelFormat();
    return CanvasRenderingContext::pixelFormat();
}

RefPtr<ImageBuffer> PlaceholderRenderingContext::surfaceBufferToImageBuffer(SurfaceBuffer)
{
    return m_buffer;
}

bool PlaceholderRenderingContext::isSurfaceBufferTransparentBlack(SurfaceBuffer) const
{
    return !m_buffer;
}

void PlaceholderRenderingContext::didUpdateCanvasSizeProperties(bool)
{
}

}

#endif
