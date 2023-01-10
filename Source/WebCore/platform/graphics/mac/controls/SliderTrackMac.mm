/*
 * Copyright (C) 2023 Apple Inc. All Rights Reserved.
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

#import "config.h"
#import "SliderTrackMac.h"

#if PLATFORM(MAC)

#import "ColorSpaceCG.h"
#import "FloatRoundedRect.h"
#import "LocalCurrentGraphicsContext.h"

namespace WebCore {

SliderTrackMac::SliderTrackMac(SliderTrackPart& part, ControlFactoryMac& controlFactory)
    : ControlMac(part, controlFactory)
{
}

FloatRect SliderTrackMac::rectForBounds(const FloatRect& bounds, const ControlStyle& style) const
{
    static constexpr int sliderTrackWidth = 5;
    float trackWidth = sliderTrackWidth * style.zoomFactor;

    auto& sliderTrackPart = owningSliderTrackPart();
    auto rect = bounds;
    
    // Set the height/width and align the location in the center of the difference.
    if (sliderTrackPart.type() == StyleAppearance::SliderHorizontal) {
        rect.setHeight(trackWidth);
        rect.setY(rect.y() + (bounds.height() - rect.height()) / 2);
    } else {
        rect.setWidth(trackWidth);
        rect.setX(rect.x() + (bounds.width() - rect.width()) / 2);
    }

    return rect;
}

static void trackGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static const float dark[4] = { 0.0f, 0.0f, 0.0f, 0.678f };
    static const float light[4] = { 0.0f, 0.0f, 0.0f, 0.13f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

void SliderTrackMac::draw(GraphicsContext& context, const FloatRect& rect, float, const ControlStyle& style)
{
    static constexpr int sliderTrackRadius = 2;
    static constexpr IntSize sliderRadius(sliderTrackRadius, sliderTrackRadius);

    LocalCurrentGraphicsContext localContext(context);
    CGContextRef cgContext = localContext.cgContext();
    CGColorSpaceRef cspace = sRGBColorSpaceRef();

    auto& sliderTrackPart = owningSliderTrackPart();

#if ENABLE(DATALIST_ELEMENT)
    sliderTrackPart.drawTicks(context, rect, style);
#endif

    GraphicsContextStateSaver stateSaver(context);

    auto logicalRect = rectForBounds(rect, style);
    CGContextClipToRect(cgContext, logicalRect);

    struct CGFunctionCallbacks mainCallbacks = { 0, trackGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> mainFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &mainCallbacks));
    RetainPtr<CGShadingRef> mainShading;
    if (sliderTrackPart.type() == StyleAppearance::SliderVertical)
        mainShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(logicalRect.x(),  logicalRect.maxY()), CGPointMake(logicalRect.maxX(), logicalRect.maxY()), mainFunction.get(), false, false));
    else
        mainShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(logicalRect.x(),  logicalRect.y()), CGPointMake(logicalRect.x(), logicalRect.maxY()), mainFunction.get(), false, false));

    context.clipRoundedRect(FloatRoundedRect(logicalRect, sliderRadius, sliderRadius, sliderRadius, sliderRadius));
    cgContext = localContext.cgContext();
    CGContextDrawShading(cgContext, mainShading.get());
}

} // namespace WebCore

#endif // PLATFORM(MAC)
