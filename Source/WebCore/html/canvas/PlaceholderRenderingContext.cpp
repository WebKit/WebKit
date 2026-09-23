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
#include "NativeImage.h"
#include "OffscreenCanvas.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaceholderLayerContents);

void PlaceholderLayerContents::copyFrame(ImageBuffer& imageBuffer, bool opaque, PlaceholderFrameIdentifier frame)
{
    Locker locker { m_lock };
    if (!m_delegate || frame <= m_frame)
        return;
    m_delegate->tryCopyToLayer(imageBuffer, opaque, frame);
    m_frame = frame;
}

void PlaceholderLayerContents::attach(GraphicsLayer& layer, ImageBuffer* buffer, bool opaque, PlaceholderFrameIdentifier frame)
{
    assertIsMainThread();
    Locker locker { m_lock };
    if (!(m_delegate = layer.createAsyncContentsDisplayDelegate(m_delegate.get())))
        return;
    if (buffer) {
        m_delegate->tryCopyToLayer(*buffer, opaque, frame);
        m_frame = frame;
    }
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(LocalPlaceholderRenderingContextSource);

Ref<LocalPlaceholderRenderingContextSource> LocalPlaceholderRenderingContextSource::create(PlaceholderRenderingContext& context)
{
    return adoptRef(*new LocalPlaceholderRenderingContextSource(context));
}

LocalPlaceholderRenderingContextSource::LocalPlaceholderRenderingContextSource(PlaceholderRenderingContext& placeholder)
    : PlaceholderRenderingContextSource(placeholder.identifier())
    , m_placeholder(placeholder)
    , m_layerContents(placeholder.layerContents())
{
}

void LocalPlaceholderRenderingContextSource::setPlaceholderBuffer(ImageBuffer& imageBuffer, bool originClean, bool opaque)
{
    auto frame = m_lastFrame.increment();
    m_layerContents->copyFrame(imageBuffer, opaque, frame);

    RefPtr clone = imageBuffer.clone();
    if (!clone)
        return;
    std::unique_ptr serializedClone = ImageBuffer::sinkIntoSerializedImageBuffer(WTF::move(clone));
    if (!serializedClone)
        return;
    callOnMainThread([weakPlaceholder = m_placeholder, buffer = WTF::move(serializedClone), frame, originClean, opaque] () mutable {
        assertIsMainThread();
        RefPtr placeholder = weakPlaceholder.get();
        if (!placeholder)
            return;
        RefPtr imageBuffer = SerializedImageBuffer::sinkIntoImageBuffer(WTF::move(buffer), protect(protect(placeholder->canvas())->scriptExecutionContext())->graphicsClient());
        if (!imageBuffer)
            return;
        // Compares the frames, so that a possibly already historical buffer in this main thread
        // task does not override the newest buffer that the worker thread already set.
        protect(placeholder->layerContents())->copyFrame(*imageBuffer, opaque, frame);
        placeholder->setPlaceholderBuffer(imageBuffer.releaseNonNull(), frame, originClean, opaque);
    });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaceholderRenderingContext);

std::unique_ptr<PlaceholderRenderingContext> PlaceholderRenderingContext::create(HTMLCanvasElement& element)
{
    return std::unique_ptr<PlaceholderRenderingContext> { new PlaceholderRenderingContext(element) };
}

PlaceholderRenderingContext::PlaceholderRenderingContext(HTMLCanvasElement& canvas)
    : CanvasRenderingContext(canvas, Type::Placeholder)
    , m_identifier(PlaceholderRenderingContextIdentifier::generate())
    , m_layerContents(PlaceholderLayerContents::create())
{
}

PlaceholderRenderingContext::~PlaceholderRenderingContext() = default;

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
    m_layerContents->attach(layer, m_buffer.get(), m_opaque, m_frame);
}

void PlaceholderRenderingContext::setPlaceholderBuffer(Ref<ImageBuffer>&& newBuffer, PlaceholderFrameIdentifier frame, bool originClean, bool opaque)
{
    m_frame = frame;
    IntSize newSize = newBuffer->truncatedLogicalSize();
    Ref canvas = this->canvas();
    canvas->willUpdateContents(FloatRect { { }, newSize }, ShouldApplyPostProcessingToDirtyRect::No);
    m_opaque = opaque;
    updateMemoryCost(newBuffer->memoryCost());
    m_buffer = WTF::move(newBuffer);
    m_bufferNativeImage = nullptr;
    canvas->setSizeForControllingContext(newSize);
    if (originClean)
        canvas->setOriginClean();
    else
        canvas->setOriginTainted();
}

PixelFormat PlaceholderRenderingContext::pixelFormat() const
{
    if (auto* buffer = m_buffer.get())
        return buffer->pixelFormat();
    return CanvasRenderingContext::pixelFormat();
}

RefPtr<ImageBuffer> PlaceholderRenderingContext::surfaceBufferToImageBuffer(SurfaceBuffer)
{
    if (!m_buffer) {
        // Transparent black bitmaps are not cached.
        return canvas().createTransparentBlackImageBuffer();
    }
    return m_buffer;
}

RefPtr<NativeImage> PlaceholderRenderingContext::surfaceBufferToNativeImage(SurfaceBuffer)
{
    if (m_bufferNativeImage)
        return m_bufferNativeImage;
    RefPtr buffer = m_buffer;
    if (!buffer) {
        // No frame has been committed yet, so the placeholder reads as transparent black.
        return ImageBuffer::sinkIntoNativeImage(canvas().createTransparentBlackImageBuffer());
    }
    m_bufferNativeImage = buffer->copyNativeImage();
    return m_bufferNativeImage;
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
