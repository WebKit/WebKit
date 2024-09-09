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
#import "MenuListButtonMac.h"

#if PLATFORM(MAC)

#import "ColorSpaceCG.h"
#import "FloatRoundedRect.h"
#import "GraphicsContext.h"
#import "MenuListButtonPart.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MenuListButtonMac);

MenuListButtonMac::MenuListButtonMac(MenuListButtonPart& owningPart, ControlFactoryMac& controlFactory)
    : ControlMac(owningPart, controlFactory)
{
}

static void interpolateGradient(const CGFloat* inData, CGFloat* outData, const float* dark, const float* light)
{
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

static void topGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static constexpr float dark[4] = { 1.0f, 1.0f, 1.0f, 0.4f };
    static constexpr float light[4] = { 1.0f, 1.0f, 1.0f, 0.15f };
    interpolateGradient(inData, outData, dark, light);
}

static void bottomGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static constexpr float dark[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
    static constexpr float light[4] = { 1.0f, 1.0f, 1.0f, 0.3f };
    interpolateGradient(inData, outData, dark, light);
}

static void mainGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static constexpr float dark[4] = { 0.0f, 0.0f, 0.0f, 0.15f };
    static constexpr float light[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    interpolateGradient(inData, outData, dark, light);
}

static void darkTopGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static constexpr float dark[4] = { 0.0f, 0.0f, 0.0f, 0.4f };
    static constexpr float light[4] = { 0.0f, 0.0f, 0.0f, 0.15f };
    interpolateGradient(inData, outData, dark, light);
}

static void darkBottomGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static constexpr float dark[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    static constexpr float light[4] = { 0.0f, 0.0f, 0.0f, 0.3f };
    interpolateGradient(inData, outData, dark, light);
}

static void darkMainGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static constexpr float dark[4] = { 1.0f, 1.0f, 1.0f, 0.15f };
    static constexpr float light[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
    interpolateGradient(inData, outData, dark, light);
}

static void drawMenuListBackground(GraphicsContext& context, const FloatRect& rect, const FloatRoundedRect& borderRect, const ControlStyle& style)
{
    CGContextRef cgContext = context.platformContext();

    const auto& radii = borderRect.radii();
    int radius = radii.topLeft().width();

    bool useDarkAppearance = style.states.contains(ControlStyle::State::DarkAppearance);

    CGColorSpaceRef cspace = sRGBColorSpaceRef();

    FloatRect topGradient(rect.x(), rect.y(), rect.width(), rect.height() / 2.0f);
    struct CGFunctionCallbacks topCallbacks = { 0, useDarkAppearance ? darkTopGradientInterpolate : topGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> topFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &topCallbacks));
    RetainPtr<CGShadingRef> topShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(topGradient.x(), topGradient.y()), CGPointMake(topGradient.x(), topGradient.maxY()), topFunction.get(), false, false));

    FloatRect bottomGradient(rect.x() + radius, rect.y() + rect.height() / 2.0f, rect.width() - 2.0f * radius, rect.height() / 2.0f);
    struct CGFunctionCallbacks bottomCallbacks = { 0, useDarkAppearance ? darkBottomGradientInterpolate : bottomGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> bottomFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &bottomCallbacks));
    RetainPtr<CGShadingRef> bottomShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(bottomGradient.x(),  bottomGradient.y()), CGPointMake(bottomGradient.x(), bottomGradient.maxY()), bottomFunction.get(), false, false));

    struct CGFunctionCallbacks mainCallbacks = { 0, useDarkAppearance ? darkMainGradientInterpolate : mainGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> mainFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &mainCallbacks));
    RetainPtr<CGShadingRef> mainShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(rect.x(),  rect.y()), CGPointMake(rect.x(), rect.maxY()), mainFunction.get(), false, false));

    RetainPtr<CGShadingRef> leftShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(rect.x(),  rect.y()), CGPointMake(rect.x() + radius, rect.y()), mainFunction.get(), false, false));

    RetainPtr<CGShadingRef> rightShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(rect.maxX(),  rect.y()), CGPointMake(rect.maxX() - radius, rect.y()), mainFunction.get(), false, false));

    {
        GraphicsContextStateSaver stateSaver(context);
        CGContextClipToRect(cgContext, rect);
        context.clipRoundedRect(borderRect);
        CGContextDrawShading(cgContext, mainShading.get());
    }

    {
        GraphicsContextStateSaver stateSaver(context);
        CGContextClipToRect(cgContext, topGradient);
        context.clipRoundedRect(FloatRoundedRect(enclosingIntRect(topGradient), radii.topLeft(), radii.topRight(), { }, { }));
        CGContextDrawShading(cgContext, topShading.get());
    }

    if (!bottomGradient.isEmpty()) {
        GraphicsContextStateSaver stateSaver(context);
        CGContextClipToRect(cgContext, bottomGradient);
        context.clipRoundedRect(FloatRoundedRect(enclosingIntRect(bottomGradient), { }, { }, radii.bottomLeft(), radii.bottomRight()));
        CGContextDrawShading(cgContext, bottomShading.get());
    }

    {
        GraphicsContextStateSaver stateSaver(context);
        CGContextClipToRect(cgContext, rect);
        context.clipRoundedRect(borderRect);
        CGContextDrawShading(cgContext, leftShading.get());
        CGContextDrawShading(cgContext, rightShading.get());
    }
}

void MenuListButtonMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle& style)
{
    auto bounds = borderRect.rect();
    bounds.contract(style.borderWidth);

    if (bounds.isEmpty())
        return;

    // Draw the gradients to give the styled popup menu a button appearance
    drawMenuListBackground(context, bounds, borderRect, style);

    static constexpr float baseFontSize = 11.0f;
    static constexpr float baseArrowHeight = 4.0f;
    static constexpr float baseArrowWidth = 5.0f;
    static constexpr float baseSpaceBetweenArrows = 2.0f;
    static constexpr int arrowPaddingBefore = 6;
    static constexpr int arrowPaddingAfter = 6;

    bool isVerticalWritingMode = style.states.contains(ControlStyle::State::VerticalWritingMode);
    auto logicalBounds = isVerticalWritingMode ? bounds.transposedRect() : bounds;

    // Since we actually know the size of the control here, we restrict the font scale to make sure the arrows will fit vertically in the bounds
    float fontScale = std::min(style.fontSize / baseFontSize, bounds.height() / (baseArrowHeight * 2 + baseSpaceBetweenArrows));
    float centerY = logicalBounds.y() + logicalBounds.height() / 2.0f;
    float arrowHeight = baseArrowHeight * fontScale;
    float arrowWidth = baseArrowWidth * fontScale;
    float spaceBetweenArrows = baseSpaceBetweenArrows * fontScale;

    if (bounds.width() < arrowWidth + arrowPaddingBefore * style.zoomFactor)
        return;

    bool isRightToLeft = style.states.contains(ControlStyle::State::RightToLeft);

    float leftEdge;
    if (isRightToLeft)
        leftEdge = logicalBounds.x() + arrowPaddingAfter * style.zoomFactor;
    else
        leftEdge = logicalBounds.maxX() - arrowPaddingAfter * style.zoomFactor - arrowWidth;

    GraphicsContextStateSaver stateSaver(context);

    context.setFillColor(style.textColor);
    context.setStrokeStyle(StrokeStyle::NoStroke);

    Vector<FloatPoint> topArrow = {
        { leftEdge, centerY - spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth, centerY - spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth / 2.0f, centerY - spaceBetweenArrows / 2.0f - arrowHeight }
    };

    Vector<FloatPoint> bottomArrow = {
        { leftEdge, centerY + spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth, centerY + spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth / 2.0f, centerY + spaceBetweenArrows / 2.0f + arrowHeight }
    };

    // Rotate the arrows for vertical writing mode since the popup appears to the side of the control instead of under it.
    if (isVerticalWritingMode) {
        auto transposePoint = [](const FloatPoint& point) {
            return point.transposedPoint();
        };

        topArrow = topArrow.map(transposePoint);
        bottomArrow = bottomArrow.map(transposePoint);
    }

    context.fillPath(Path(topArrow));
    context.fillPath(Path(bottomArrow));
}

} // namespace WebCore

#endif // PLATFORM(MAC)
