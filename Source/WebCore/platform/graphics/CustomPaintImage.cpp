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
#include "CustomPaintImage.h"

#if ENABLE(CSS_PAINTING_API)

#include "CustomPaintCanvas.h"
#include "GraphicsContext.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "JSCSSPaintCallback.h"
#include "PaintRenderingContext2D.h"

namespace WebCore {

CustomPaintImage::CustomPaintImage(const PaintWorkletGlobalScope::PaintDefinition& definition, const FloatSize& size)
    : m_paintCallback(definition.paintCallback.get())
{
    setContainerSize(size);
}

CustomPaintImage::~CustomPaintImage() = default;

ImageDrawResult CustomPaintImage::doCustomPaint(GraphicsContext& destContext, const FloatSize& destSize)
{
    JSCSSPaintCallback& callback = static_cast<JSCSSPaintCallback&>(m_paintCallback.get());
    auto* scriptExecutionContext = callback.scriptExecutionContext();
    if (!scriptExecutionContext)
        return ImageDrawResult::DidNothing;

    auto canvas = CustomPaintCanvas::create(*scriptExecutionContext, destSize.width(), destSize.height());
    ExceptionOr<RefPtr<PaintRenderingContext2D>> contextOrException = canvas->getContext();

    if (contextOrException.hasException())
        return ImageDrawResult::DidNothing;
    auto context = contextOrException.releaseReturnValue();

    auto result = m_paintCallback->handleEvent(*context);
    if (result.type() != CallbackResultType::Success)
        return ImageDrawResult::DidNothing;

    auto image = canvas->copiedImage();
    if (!image)
        return ImageDrawResult::DidNothing;

    destContext.drawImage(*image, FloatPoint());

    return ImageDrawResult::DidDraw;
}

ImageDrawResult CustomPaintImage::draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator compositeOp, BlendMode blendMode, DecodingMode, ImageOrientationDescription)
{
    GraphicsContextStateSaver stateSaver(destContext);
    destContext.setCompositeOperation(compositeOp, blendMode);
    destContext.clip(destRect);
    destContext.translate(destRect.location());
    if (destRect.size() != srcRect.size())
        destContext.scale(destRect.size() / srcRect.size());
    destContext.translate(-srcRect.location());
    return doCustomPaint(destContext, size());
}

void CustomPaintImage::drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform,
    const FloatPoint& phase, const FloatSize& spacing, CompositeOperator compositeOp, BlendMode blendMode)
{
    // Allow the generator to provide visually-equivalent tiling parameters for better performance.
    FloatSize adjustedSize = size();
    FloatRect adjustedSrcRect = srcRect;

    // Factor in the destination context's scale to generate at the best resolution
    AffineTransform destContextCTM = destContext.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    double xScale = fabs(destContextCTM.xScale());
    double yScale = fabs(destContextCTM.yScale());
    AffineTransform adjustedPatternCTM = patternTransform;
    adjustedPatternCTM.scale(1.0 / xScale, 1.0 / yScale);
    adjustedSrcRect.scale(xScale, yScale);

    auto buffer = ImageBuffer::createCompatibleBuffer(adjustedSize, ColorSpaceSRGB, destContext);
    if (!buffer)
        return;
    doCustomPaint(buffer->context(), adjustedSize);

    if (destContext.drawLuminanceMask())
        buffer->convertToLuminanceMask();

    buffer->drawPattern(destContext, destRect, adjustedSrcRect, adjustedPatternCTM, phase, spacing, compositeOp, blendMode);
    destContext.setDrawLuminanceMask(false);
}

}
#endif
