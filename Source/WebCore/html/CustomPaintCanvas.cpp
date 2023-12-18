/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "CustomPaintCanvas.h"

#if ENABLE(CSS_PAINTING_API)

#include "BitmapImage.h"
#include "CanvasRenderingContext.h"
#include "DisplayListDrawingContext.h"
#include "DisplayListRecorder.h"
#include "DisplayListReplayer.h"
#include "ImageBitmap.h"
#include "PaintRenderingContext2D.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

Ref<CustomPaintCanvas> CustomPaintCanvas::create(ScriptExecutionContext& context, unsigned width, unsigned height)
{
    return adoptRef(*new CustomPaintCanvas(context, width, height));
}

CustomPaintCanvas::CustomPaintCanvas(ScriptExecutionContext& context, unsigned width, unsigned height)
    : CanvasBase(IntSize(width, height), context.noiseInjectionHashSalt())
    , ContextDestructionObserver(&context)
{
}

CustomPaintCanvas::~CustomPaintCanvas()
{
    notifyObserversCanvasDestroyed();

    m_context = nullptr; // Ensure this goes away before the ImageBuffer.
    setImageBuffer(nullptr);
}

RefPtr<PaintRenderingContext2D> CustomPaintCanvas::getContext()
{
    if (m_context)
        return &downcast<PaintRenderingContext2D>(*m_context);

    auto context = PaintRenderingContext2D::create(*this);
    auto* contextPtr = context.get();
    m_context = WTFMove(context);
    return contextPtr;
}

void CustomPaintCanvas::replayDisplayList(GraphicsContext& target)
{
    if (!width() || !height())
        return;
    // FIXME: Using an intermediate buffer is not needed if there are no composite operations.
    auto clipBounds = target.clipBounds();
    auto image = target.createAlignedImageBuffer(clipBounds.size());
    if (!image)
        return;
    auto& imageTarget = image->context();
    imageTarget.translate(-clipBounds.location());
    replayDisplayListImpl(imageTarget);
    target.drawImageBuffer(*image, clipBounds);
}

AffineTransform CustomPaintCanvas::baseTransform() const
{
    // The base transform of the display list.
    // FIXME: this is actually correct, but the display list will not behave correctly with respect to
    // playback. The GraphicsContext should be fixed to start at identity transform, and the
    // device transform should be a separate concept that the display list or context2d cannot reset.
    return { };
}

Image* CustomPaintCanvas::copiedImage() const
{
    if (!width() || !height())
        return nullptr;
    m_copiedImage = nullptr;
    auto buffer = ImageBuffer::create(size(), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (buffer) {
        replayDisplayListImpl(buffer->context());
        m_copiedImage = BitmapImage::create(ImageBuffer::sinkIntoNativeImage(buffer));
    }
    return m_copiedImage.get();
}

void CustomPaintCanvas::clearCopiedImage() const
{
    m_copiedImage = nullptr;
}

GraphicsContext* CustomPaintCanvas::drawingContext() const
{
    if (!m_recordingContext)
        m_recordingContext = makeUnique<DisplayList::DrawingContext>(size());
    return &m_recordingContext->context();
}

GraphicsContext* CustomPaintCanvas::existingDrawingContext() const
{
    return m_recordingContext ? &m_recordingContext->context() : nullptr;
}

void CustomPaintCanvas::replayDisplayListImpl(GraphicsContext& target) const
{
    if (!m_recordingContext)
        return;
    auto& displayList = m_recordingContext->displayList();
    if (!displayList.isEmpty()) {
        DisplayList::Replayer replayer(target, displayList);
        replayer.replay(FloatRect { { }, size() });
        displayList.clear();
    }
}

}
#endif
