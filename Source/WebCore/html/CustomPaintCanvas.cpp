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

#include "BitmapImage.h"
#include "CSSParserContext.h"
#include "CanvasRenderingContext.h"
#include "ImageBitmap.h"
#include "PaintRenderingContext2D.h"
#include "ScriptExecutionContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CustomPaintCanvas);

Ref<CustomPaintCanvas> CustomPaintCanvas::create(ScriptExecutionContext& context, unsigned width, unsigned height)
{
    return adoptRef(*new CustomPaintCanvas(context, width, height));
}

CustomPaintCanvas::CustomPaintCanvas(ScriptExecutionContext& context, unsigned width, unsigned height)
    : CanvasBase(IntSize(width, height), context)
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
    if (!m_context)
        m_context = PaintRenderingContext2D::create(*this);
    return m_context.get();
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
    if (m_context)
        m_context->replayDisplayList(imageTarget);
    target.drawImageBuffer(*image, clipBounds);
}

Image* CustomPaintCanvas::copiedImage() const
{
    if (!width() || !height())
        return nullptr;
    m_copiedImage = nullptr;
    auto buffer = ImageBuffer::create(size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (buffer) {
        if (m_context)
            m_context->replayDisplayList(buffer->context());
        m_copiedImage = BitmapImage::create(ImageBuffer::sinkIntoNativeImage(buffer));
    }
    return m_copiedImage.get();
}

void CustomPaintCanvas::clearCopiedImage() const
{
    m_copiedImage = nullptr;
}

const CSSParserContext& CustomPaintCanvas::cssParserContext() const
{
    // FIXME: Rather than using a default CSSParserContext, there should be one exposed via ScriptExecutionContext.
    if (!m_cssParserContext)
        m_cssParserContext = WTF::makeUnique<CSSParserContext>(HTMLStandardMode);
    return *m_cssParserContext;
}

} // namespace WebCore
