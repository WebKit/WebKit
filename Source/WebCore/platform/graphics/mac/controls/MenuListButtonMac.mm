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
#import "LocalCurrentGraphicsContext.h"
#import "MenuListButtonPart.h"

namespace WebCore {

MenuListButtonMac::MenuListButtonMac(MenuListButtonPart& owningPart, ControlFactoryMac& controlFactory)
    : ControlMac(owningPart, controlFactory)
{
}

static void topGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static const float dark[4] = { 1.0f, 1.0f, 1.0f, 0.4f };
    static const float light[4] = { 1.0f, 1.0f, 1.0f, 0.15f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

static void bottomGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static const float dark[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
    static const float light[4] = { 1.0f, 1.0f, 1.0f, 0.3f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

static void mainGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static const float dark[4] = { 0.0f, 0.0f, 0.0f, 0.15f };
    static const float light[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

static void drawMenuListBackground(GraphicsContext& context, const FloatRect& rect, const FloatRoundedRect& borderRect, const ControlStyle&)
{
    ContextContainer cgContextContainer(context);
    CGContextRef cgContext = cgContextContainer.context();

    const auto& radii = borderRect.radii();
    int radius = radii.topLeft().width();

    CGColorSpaceRef cspace = sRGBColorSpaceRef();

    FloatRect topGradient(rect.x(), rect.y(), rect.width(), rect.height() / 2.0f);
    struct CGFunctionCallbacks topCallbacks = { 0, topGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> topFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &topCallbacks));
    RetainPtr<CGShadingRef> topShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(topGradient.x(), topGradient.y()), CGPointMake(topGradient.x(), topGradient.maxY()), topFunction.get(), false, false));

    FloatRect bottomGradient(rect.x() + radius, rect.y() + rect.height() / 2.0f, rect.width() - 2.0f * radius, rect.height() / 2.0f);
    struct CGFunctionCallbacks bottomCallbacks = { 0, bottomGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> bottomFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &bottomCallbacks));
    RetainPtr<CGShadingRef> bottomShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(bottomGradient.x(),  bottomGradient.y()), CGPointMake(bottomGradient.x(), bottomGradient.maxY()), bottomFunction.get(), false, false));

    struct CGFunctionCallbacks mainCallbacks = { 0, mainGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> mainFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &mainCallbacks));
    RetainPtr<CGShadingRef> mainShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(rect.x(),  rect.y()), CGPointMake(rect.x(), rect.maxY()), mainFunction.get(), false, false));

    RetainPtr<CGShadingRef> leftShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(rect.x(),  rect.y()), CGPointMake(rect.x() + radius, rect.y()), mainFunction.get(), false, false));

    RetainPtr<CGShadingRef> rightShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(rect.maxX(),  rect.y()), CGPointMake(rect.maxX() - radius, rect.y()), mainFunction.get(), false, false));

    {
        GraphicsContextStateSaver stateSaver(context);
        CGContextClipToRect(cgContext, rect);
        context.clipRoundedRect(borderRect);
        cgContext = cgContextContainer.context();
        CGContextDrawShading(cgContext, mainShading.get());
    }

    {
        GraphicsContextStateSaver stateSaver(context);
        CGContextClipToRect(cgContext, topGradient);
        context.clipRoundedRect(FloatRoundedRect(enclosingIntRect(topGradient), radii.topLeft(), radii.topRight(), { }, { }));
        cgContext = cgContextContainer.context();
        CGContextDrawShading(cgContext, topShading.get());
    }

    if (!bottomGradient.isEmpty()) {
        GraphicsContextStateSaver stateSaver(context);
        CGContextClipToRect(cgContext, bottomGradient);
        context.clipRoundedRect(FloatRoundedRect(enclosingIntRect(bottomGradient), { }, { }, radii.bottomLeft(), radii.bottomRight()));
        cgContext = cgContextContainer.context();
        CGContextDrawShading(cgContext, bottomShading.get());
    }

    {
        GraphicsContextStateSaver stateSaver(context);
        CGContextClipToRect(cgContext, rect);
        context.clipRoundedRect(borderRect);
        cgContext = cgContextContainer.context();
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

    // Since we actually know the size of the control here, we restrict the font scale to make sure the arrows will fit vertically in the bounds
    float fontScale = std::min(style.fontSize / baseFontSize, bounds.height() / (baseArrowHeight * 2 + baseSpaceBetweenArrows));
    float centerY = bounds.y() + bounds.height() / 2.0f;
    float arrowHeight = baseArrowHeight * fontScale;
    float arrowWidth = baseArrowWidth * fontScale;
    float spaceBetweenArrows = baseSpaceBetweenArrows * fontScale;

    if (bounds.width() < arrowWidth + arrowPaddingBefore * style.zoomFactor)
        return;

    bool isRightToLeft = style.states.contains(ControlStyle::State::RightToLeft);

    float leftEdge;
    if (isRightToLeft)
        leftEdge = bounds.x() + arrowPaddingAfter * style.zoomFactor;
    else
        leftEdge = bounds.maxX() - arrowPaddingAfter * style.zoomFactor - arrowWidth;

    GraphicsContextStateSaver stateSaver(context);

    context.setFillColor(style.textColor);
    context.setStrokeStyle(StrokeStyle::NoStroke);

    // Draw the top arrow
    Vector<FloatPoint> arrow1 = {
        { leftEdge, centerY - spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth, centerY - spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth / 2.0f, centerY - spaceBetweenArrows / 2.0f - arrowHeight }
    };
    context.fillPath(Path::polygonPathFromPoints(arrow1));

    // Draw the bottom arrow
    Vector<FloatPoint> arrow2 = {
        { leftEdge, centerY + spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth, centerY + spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth / 2.0f, centerY + spaceBetweenArrows / 2.0f + arrowHeight }
    };
    context.fillPath(Path::polygonPathFromPoints(arrow2));

    constexpr auto leftSeparatorColor = Color::black.colorWithAlphaByte(40);
    constexpr auto rightSeparatorColor = Color::white.colorWithAlphaByte(40);

    // FIXME: Should the separator thickness and space be scaled up by fontScale?
    int separatorSpace = 2; // Deliberately ignores zoom since it looks nicer if it stays thin.
    int leftEdgeOfSeparator;
    if (isRightToLeft)
        leftEdgeOfSeparator = static_cast<int>(roundf(leftEdge + arrowWidth + arrowPaddingBefore * style.zoomFactor));
    else
        leftEdgeOfSeparator = static_cast<int>(roundf(leftEdge - arrowPaddingBefore * style.zoomFactor));

    // Draw the separator to the left of the arrows
    context.setStrokeThickness(1); // Deliberately ignores zoom since it looks nicer if it stays thin.
    context.setStrokeStyle(StrokeStyle::SolidStroke);
    context.setStrokeColor(leftSeparatorColor);
    context.drawLine(IntPoint(leftEdgeOfSeparator, bounds.y()), IntPoint(leftEdgeOfSeparator, bounds.maxY()));

    context.setStrokeColor(rightSeparatorColor);
    context.drawLine(IntPoint(leftEdgeOfSeparator + separatorSpace, bounds.y()), IntPoint(leftEdgeOfSeparator + separatorSpace, bounds.maxY()));
}

} // namespace WebCore

#endif // PLATFORM(MAC)
