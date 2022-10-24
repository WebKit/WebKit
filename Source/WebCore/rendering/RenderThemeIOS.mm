/*
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RenderThemeIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "ARKitBadgeSystemImage.h"
#import "BitmapImage.h"
#import "BorderPainter.h"
#import "CSSPrimitiveValue.h"
#import "CSSToLengthConversionData.h"
#import "CSSValueKey.h"
#import "CSSValueKeywords.h"
#import "ColorBlending.h"
#import "ColorCocoa.h"
#import "ColorTypes.h"
#import "DateComponents.h"
#import "DeprecatedGlobalSettings.h"
#import "Document.h"
#import "DrawGlyphsRecorder.h"
#import "File.h"
#import "FloatRoundedRect.h"
#import "FontCache.h"
#import "FontCacheCoreText.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "FrameView.h"
#import "GeometryUtilities.h"
#import "Gradient.h"
#import "GraphicsContext.h"
#import "GraphicsContextCG.h"
#import "HTMLAttachmentElement.h"
#import "HTMLButtonElement.h"
#import "HTMLInputElement.h"
#import "HTMLMeterElement.h"
#import "HTMLNames.h"
#import "HTMLSelectElement.h"
#import "HTMLTextAreaElement.h"
#import "IOSurface.h"
#import "LocalCurrentTraitCollection.h"
#import "LocalizedDateCache.h"
#import "NodeRenderStyle.h"
#import "Page.h"
#import "PaintInfo.h"
#import "PathUtilities.h"
#import "PlatformLocale.h"
#import "RenderAttachment.h"
#import "RenderButton.h"
#import "RenderMenuList.h"
#import "RenderMeter.h"
#import "RenderObject.h"
#import "RenderProgress.h"
#import "RenderSlider.h"
#import "RenderStyle.h"
#import "RenderView.h"
#import "Settings.h"
#import "Theme.h"
#import "UTIUtilities.h"
#import "WebCoreThreadRun.h"
#import <CoreGraphics/CoreGraphics.h>
#import <CoreImage/CoreImage.h>
#import <objc/runtime.h>
#import <pal/spi/cf/CoreTextSPI.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if ENABLE(DATALIST_ELEMENT)
#include "HTMLDataListElement.h"
#include "HTMLOptionElement.h"
#endif

#import <pal/ios/UIKitSoftLink.h>

namespace WebCore {

using namespace HTMLNames;

const float ControlBaseHeight = 20;
const float ControlBaseFontSize = 11;

struct IOSGradient {
    float* start; // points to static float[4]
    float* end; // points to static float[4]
    IOSGradient(float start[4], float end[4])
        : start(start)
        , end(end)
    {
    }
};

typedef IOSGradient* IOSGradientRef;

enum Interpolation
{
    LinearInterpolation,
    ExponentialInterpolation
};

static void interpolateLinearGradient(void *info, const CGFloat *inData, CGFloat *outData)
{
    IOSGradientRef gradient = static_cast<IOSGradientRef>(info);
    float alpha = inData[0];
    float inverse = 1.0f - alpha;

    outData[0] = inverse * gradient->start[0] + alpha * gradient->end[0];
    outData[1] = inverse * gradient->start[1] + alpha * gradient->end[1];
    outData[2] = inverse * gradient->start[2] + alpha * gradient->end[2];
    outData[3] = inverse * gradient->start[3] + alpha * gradient->end[3];
}

static void interpolateExponentialGradient(void *info, const CGFloat *inData, CGFloat *outData)
{
    IOSGradientRef gradient = static_cast<IOSGradientRef>(info);
    float a = inData[0];
    for (int paintInfo = 0; paintInfo < 4; ++paintInfo) {
        float end = logf(std::max(gradient->end[paintInfo], 0.01f));
        float start = logf(std::max(gradient->start[paintInfo], 0.01f));
        outData[paintInfo] = expf(start - (end + start) * a);
    }
}

static CGFunctionRef getSharedFunctionRef(IOSGradientRef gradient, Interpolation interpolation)
{
    static NeverDestroyed<HashMap<IOSGradientRef, RetainPtr<CGFunctionRef>>> linearFunctionRefs;
    static NeverDestroyed<HashMap<IOSGradientRef, RetainPtr<CGFunctionRef>>> exponentialFunctionRefs;

    if (interpolation == LinearInterpolation) {
        auto function = linearFunctionRefs->get(gradient);
        if (!function) {
            static struct CGFunctionCallbacks linearFunctionCallbacks =  { 0, interpolateLinearGradient, 0 };
            linearFunctionRefs->set(gradient, function = adoptCF(CGFunctionCreate(gradient, 1, nullptr, 4, nullptr, &linearFunctionCallbacks)));
        }

        return function.get();
    }

    auto function = exponentialFunctionRefs->get(gradient);
    if (!function) {
        static struct CGFunctionCallbacks exponentialFunctionCallbacks =  { 0, interpolateExponentialGradient, 0 };
        exponentialFunctionRefs->set(gradient, function = adoptCF(CGFunctionCreate(gradient, 1, 0, 4, 0, &exponentialFunctionCallbacks)));
    }
    return function.get();
}

static void drawAxialGradient(CGContextRef context, IOSGradientRef gradient, const FloatPoint& startPoint, const FloatPoint& stopPoint, Interpolation interpolation)
{
    RetainPtr<CGShadingRef> shading = adoptCF(CGShadingCreateAxial(sRGBColorSpaceRef(), startPoint, stopPoint, getSharedFunctionRef(gradient, interpolation), false, false));
    CGContextDrawShading(context, shading.get());
}

static void drawRadialGradient(CGContextRef context, IOSGradientRef gradient, const FloatPoint& startPoint, float startRadius, const FloatPoint& stopPoint, float stopRadius, Interpolation interpolation)
{
    RetainPtr<CGShadingRef> shading = adoptCF(CGShadingCreateRadial(sRGBColorSpaceRef(), startPoint, startRadius, stopPoint, stopRadius, getSharedFunctionRef(gradient, interpolation), false, false));
    CGContextDrawShading(context, shading.get());
}

enum IOSGradientType {
    InsetGradient,
    ShineGradient,
    ShadeGradient,
    ConvexGradient,
    ConcaveGradient,
    SliderTrackGradient,
    ReadonlySliderTrackGradient,
    SliderThumbOpaquePressedGradient,
};

static IOSGradientRef getInsetGradient()
{
    static float end[4] = { 0 / 255.0, 0 / 255.0, 0 / 255.0, 0 };
    static float start[4] = { 0 / 255.0, 0 / 255.0, 0 / 255.0, 0.2 };
    static NeverDestroyed<IOSGradient> gradient(start, end);
    return &gradient.get();
}

static IOSGradientRef getShineGradient()
{
    static float end[4] = { 1, 1, 1, 0.8 };
    static float start[4] = { 1, 1, 1, 0 };
    static NeverDestroyed<IOSGradient> gradient(start, end);
    return &gradient.get();
}

static IOSGradientRef getShadeGradient()
{
    static float end[4] = { 178 / 255.0, 178 / 255.0, 178 / 255.0, 0.65 };
    static float start[4] = { 252 / 255.0, 252 / 255.0, 252 / 255.0, 0.65 };
    static NeverDestroyed<IOSGradient> gradient(start, end);
    return &gradient.get();
}

static IOSGradientRef getConvexGradient()
{
    static float end[4] = { 255 / 255.0, 255 / 255.0, 255 / 255.0, 0.05 };
    static float start[4] = { 255 / 255.0, 255 / 255.0, 255 / 255.0, 0.43 };
    static NeverDestroyed<IOSGradient> gradient(start, end);
    return &gradient.get();
}

static IOSGradientRef getConcaveGradient()
{
    static float end[4] = { 255 / 255.0, 255 / 255.0, 255 / 255.0, 0.46 };
    static float start[4] = { 255 / 255.0, 255 / 255.0, 255 / 255.0, 0 };
    static NeverDestroyed<IOSGradient> gradient(start, end);
    return &gradient.get();
}

static IOSGradientRef getSliderTrackGradient()
{
    static float end[4] = { 132 / 255.0, 132 / 255.0, 132 / 255.0, 1 };
    static float start[4] = { 74 / 255.0, 77 / 255.0, 80 / 255.0, 1 };
    static NeverDestroyed<IOSGradient> gradient(start, end);
    return &gradient.get();
}

static IOSGradientRef getReadonlySliderTrackGradient()
{
    static float end[4] = { 132 / 255.0, 132 / 255.0, 132 / 255.0, 0.4 };
    static float start[4] = { 74 / 255.0, 77 / 255.0, 80 /255.0, 0.4 };
    static NeverDestroyed<IOSGradient> gradient(start, end);
    return &gradient.get();
}

static IOSGradientRef getSliderThumbOpaquePressedGradient()
{
    static float end[4] = { 144 / 255.0, 144 / 255.0, 144 / 255.0, 1};
    static float start[4] = { 55 / 255.0, 55 / 255.0, 55 / 255.0, 1 };
    static NeverDestroyed<IOSGradient> gradient(start, end);
    return &gradient.get();
}

static IOSGradientRef gradientWithName(IOSGradientType gradientType)
{
    switch (gradientType) {
    case InsetGradient:
        return getInsetGradient();
    case ShineGradient:
        return getShineGradient();
    case ShadeGradient:
        return getShadeGradient();
    case ConvexGradient:
        return getConvexGradient();
    case ConcaveGradient:
        return getConcaveGradient();
    case SliderTrackGradient:
        return getSliderTrackGradient();
    case ReadonlySliderTrackGradient:
        return getReadonlySliderTrackGradient();
    case SliderThumbOpaquePressedGradient:
        return getSliderThumbOpaquePressedGradient();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

RenderThemeIOS::RenderThemeIOS() = default;

RenderTheme& RenderTheme::singleton()
{
    static NeverDestroyed<RenderThemeIOS> theme;
    return theme;
}

FloatRect RenderThemeIOS::addRoundedBorderClip(const RenderObject& box, GraphicsContext& context, const IntRect& rect)
{
    // To fix inner border bleeding issues <rdar://problem/9812507>, we clip to the outer border and assert that
    // the border is opaque or transparent, unless we're checked because checked radio/checkboxes show no bleeding.
    auto& style = box.style();
    RoundedRect border = isChecked(box) ? style.getRoundedInnerBorderFor(rect) : style.getRoundedBorderFor(rect);

    if (border.isRounded())
        context.clipRoundedRect(FloatRoundedRect(border));
    else
        context.clip(border.rect());

    if (isChecked(box)) {
        ASSERT(style.visitedDependentColor(CSSPropertyBorderTopColor).alphaByte() % 255 == 0);
        ASSERT(style.visitedDependentColor(CSSPropertyBorderRightColor).alphaByte() % 255 == 0);
        ASSERT(style.visitedDependentColor(CSSPropertyBorderBottomColor).alphaByte() % 255 == 0);
        ASSERT(style.visitedDependentColor(CSSPropertyBorderLeftColor).alphaByte() % 255 == 0);
    }

    return border.rect();
}

void RenderThemeIOS::adjustStyleForAlternateFormControlDesignTransition(RenderStyle& style, const Element* element) const
{
    if (!element)
        return;

    if (!element->document().settings().alternateFormControlDesignEnabled())
        return;

#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
    // FIXME: We need to find a way to not do this for any running transition, only the UA-owned transition.
    style.setTransformStyle3D(element->hasRunningTransitionForProperty(PseudoId::None, CSSPropertyID::CSSPropertyTranslate) || element->hovered() ? TransformStyle3D::Optimized3D : TransformStyle3D::Flat);
#else
    UNUSED_PARAM(style);
#endif
}

void RenderThemeIOS::adjustCheckboxStyle(RenderStyle& style, const Element* element) const
{
    adjustStyleForAlternateFormControlDesignTransition(style, element);

    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    int size = std::max(style.computedFontPixelSize(), 10U);
    style.setWidth({ size, LengthType::Fixed });
    style.setHeight({ size, LengthType::Fixed });
}

static CGPoint shortened(CGPoint start, CGPoint end, float width)
{
    float x = end.x - start.x;
    float y = end.y - start.y;
    float ratio = (!x && !y) ? 0 : width / std::hypot(x, y);
    return CGPointMake(start.x + x * ratio, start.y + y * ratio);
}

static void drawJoinedLines(CGContextRef context, const Vector<CGPoint>& points, CGLineCap lineCap, float lineWidth, Color strokeColor)
{
    CGContextSetLineWidth(context, lineWidth);
    CGContextSetStrokeColorWithColor(context, cachedCGColor(strokeColor).get());
    CGContextSetShouldAntialias(context, true);
    CGContextBeginPath(context);
    CGContextSetLineCap(context, lineCap);
    CGContextMoveToPoint(context, points[0].x, points[0].y);
    
    for (unsigned i = 1; i < points.size(); ++i)
        CGContextAddLineToPoint(context, points[i].x, points[i].y);

    CGContextStrokePath(context);
}

bool RenderThemeIOS::canPaint(const PaintInfo& paintInfo, const Settings& settings) const
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (settings.iOSFormControlRefreshEnabled())
        return true;
#else
    UNUSED_PARAM(settings);
#endif
    return paintInfo.context().hasPlatformContext();
}

void RenderThemeIOS::paintCheckboxDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (box.settings().iOSFormControlRefreshEnabled())
        return;
#endif

    bool checked = isChecked(box);
    bool indeterminate = isIndeterminate(box);
    CGContextRef cgContext = paintInfo.context().platformContext();
    GraphicsContextStateSaver stateSaver { paintInfo.context() };

    if (checked || indeterminate) {
        auto border = box.style().getRoundedBorderFor(rect);
        paintInfo.context().fillRoundedRect(border.pixelSnappedRoundedRectForPainting(box.document().deviceScaleFactor()), Color::black.colorWithAlphaByte(204));

        auto clip = addRoundedBorderClip(box, paintInfo.context(), rect);
        auto width = clip.width();
        auto height = clip.height();
        drawAxialGradient(cgContext, gradientWithName(ConcaveGradient), clip.location(), FloatPoint { clip.x(), clip.maxY() }, LinearInterpolation);

        constexpr float thicknessRatio = 2 / 14.0;
        float lineWidth = std::min(width, height) * 2.0f * thicknessRatio;

        Vector<CGPoint, 3> line;
        Vector<CGPoint, 3> shadow;
        if (checked) {
            constexpr CGSize size { 14.0f, 14.0f };
            constexpr CGPoint pathRatios[] = {
                { 2.5f / size.width, 7.5f / size.height },
                { 5.5f / size.width, 10.5f / size.height },
                { 11.5f / size.width, 2.5f / size.height }
            };

            line.uncheckedAppend(CGPointMake(clip.x() + width * pathRatios[0].x, clip.y() + height * pathRatios[0].y));
            line.uncheckedAppend(CGPointMake(clip.x() + width * pathRatios[1].x, clip.y() + height * pathRatios[1].y));
            line.uncheckedAppend(CGPointMake(clip.x() + width * pathRatios[2].x, clip.y() + height * pathRatios[2].y));

            shadow.uncheckedAppend(shortened(line[0], line[1], lineWidth / 4.0f));
            shadow.uncheckedAppend(line[1]);
            shadow.uncheckedAppend(shortened(line[2], line[1], lineWidth / 4.0f));
        } else {
            line.uncheckedAppend(CGPointMake(clip.x() + 3.5, clip.center().y()));
            line.uncheckedAppend(CGPointMake(clip.maxX() - 3.5, clip.center().y()));

            shadow.uncheckedAppend(shortened(line[0], line[1], lineWidth / 4.0f));
            shadow.uncheckedAppend(shortened(line[1], line[0], lineWidth / 4.0f));
        }

        lineWidth = std::max<float>(lineWidth, 1);
        drawJoinedLines(cgContext, Vector<CGPoint> { WTFMove(shadow) }, kCGLineCapSquare, lineWidth, Color::black.colorWithAlphaByte(179));

        lineWidth = std::max<float>(std::min(width, height) * thicknessRatio, 1);
        drawJoinedLines(cgContext, Vector<CGPoint> { WTFMove(line) }, kCGLineCapButt, lineWidth, Color::white.colorWithAlphaByte(240));
    } else {
        auto clip = addRoundedBorderClip(box, paintInfo.context(), rect);
        auto width = clip.width();
        auto height = clip.height();
        FloatPoint bottomCenter { clip.x() + width / 2.0f, clip.maxY() };

        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint { clip.x(), clip.maxY() }, LinearInterpolation);
        drawRadialGradient(cgContext, gradientWithName(ShineGradient), bottomCenter, 0, bottomCenter, sqrtf((width * width) / 4.0f + height * height), ExponentialInterpolation);
    }
}

LayoutRect RenderThemeIOS::adjustedPaintRect(const RenderBox& box, const LayoutRect& paintRect) const
{
    // Workaround for <rdar://problem/6209763>. Force the painting bounds of checkboxes and radio controls to be square.
    if (box.style().effectiveAppearance() == CheckboxPart || box.style().effectiveAppearance() == RadioPart) {
        float width = std::min(paintRect.width(), paintRect.height());
        float height = width;
        return enclosingLayoutRect(FloatRect(paintRect.x(), paintRect.y() + (box.height() - height) / 2, width, height)); // Vertically center the checkbox, like on desktop
    }

    return paintRect;
}

int RenderThemeIOS::baselinePosition(const RenderBox& box) const
{
    if (box.style().effectiveAppearance() == CheckboxPart || box.style().effectiveAppearance() == RadioPart)
        return box.marginTop() + box.height() - 2; // The baseline is 2px up from the bottom of the checkbox/radio in AppKit.
    if (box.style().effectiveAppearance() == MenulistPart)
        return box.marginTop() + box.height() - 5; // This is to match AppKit. There might be a better way to calculate this though.
    return RenderTheme::baselinePosition(box);
}

bool RenderThemeIOS::isControlStyled(const RenderStyle& style, const RenderStyle& userAgentStyle) const
{
    // Buttons and MenulistButtons are styled if they contain a background image.
    if (style.effectiveAppearance() == PushButtonPart || style.effectiveAppearance() == MenulistButtonPart)
        return !style.visitedDependentColor(CSSPropertyBackgroundColor).isVisible() || style.backgroundLayers().hasImage();

    if (style.effectiveAppearance() == TextFieldPart || style.effectiveAppearance() == TextAreaPart)
        return style.backgroundLayers() != userAgentStyle.backgroundLayers();

#if ENABLE(DATALIST_ELEMENT)
    if (style.effectiveAppearance() == ListButtonPart)
        return style.hasContent() || style.hasEffectiveContentNone();
#endif

    return RenderTheme::isControlStyled(style, userAgentStyle);
}

void RenderThemeIOS::adjustRadioStyle(RenderStyle& style, const Element* element) const
{
    adjustStyleForAlternateFormControlDesignTransition(style, element);

    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    int size = std::max(style.computedFontPixelSize(), 10U);
    style.setWidth({ size, LengthType::Fixed });
    style.setHeight({ size, LengthType::Fixed });
    style.setBorderRadius({ size / 2, size / 2 });
}

void RenderThemeIOS::paintRadioDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (box.settings().iOSFormControlRefreshEnabled())
        return;
#endif

    GraphicsContextStateSaver stateSaver(paintInfo.context());
    CGContextRef cgContext = paintInfo.context().platformContext();

    auto drawShadeAndShineGradients = [&](auto clip) {
        FloatPoint bottomCenter(clip.x() + clip.width() / 2.0, clip.maxY());
        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
        drawRadialGradient(cgContext, gradientWithName(ShineGradient), bottomCenter, 0, bottomCenter, std::max(clip.width(), clip.height()), ExponentialInterpolation);
    };

    if (isChecked(box)) {
        auto border = box.style().getRoundedBorderFor(rect);
        paintInfo.context().fillRoundedRect(border.pixelSnappedRoundedRectForPainting(box.document().deviceScaleFactor()), Color::black.colorWithAlphaByte(204));

        auto clip = addRoundedBorderClip(box, paintInfo.context(), rect);
        drawAxialGradient(cgContext, gradientWithName(ConcaveGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);

        // The inner circle is 6 / 14 the size of the surrounding circle, 
        // leaving 8 / 14 around it. (8 / 14) / 2 = 2 / 7.

        static const float InnerInverseRatio = 2 / 7.0;

        clip.inflateX(-clip.width() * InnerInverseRatio);
        clip.inflateY(-clip.height() * InnerInverseRatio);
        
        constexpr auto shadowColor = Color::black.colorWithAlphaByte(179);
        paintInfo.context().drawRaisedEllipse(clip, Color::white, shadowColor);

        FloatSize radius(clip.width() / 2.0f, clip.height() / 2.0f);
        paintInfo.context().clipRoundedRect(FloatRoundedRect(clip, radius, radius, radius, radius));
        drawShadeAndShineGradients(clip);
    } else {
        auto clip = addRoundedBorderClip(box, paintInfo.context(), rect);
        drawShadeAndShineGradients(clip);
    }
}

void RenderThemeIOS::adjustTextFieldStyle(RenderStyle& style, const Element* element) const
{
    if (!element)
        return;

    bool hasTextfieldAppearance = false;
    // Do not force a background color for elements that have a textfield
    // appearance by default, so that their background color can be styled.
    if (is<HTMLInputElement>(*element)) {
        auto& input = downcast<HTMLInputElement>(*element);
        // <input type=search> is the only TextFieldInputType that has a
        // non-textfield appearance value.
        hasTextfieldAppearance = input.isTextField() && !input.isSearchField();
    }

    auto adjustBackgroundColor = [&] {
        auto styleColorOptions = element->document().styleColorOptions(&style);
        if (!style.backgroundColorEqualsToColorIgnoringVisited(systemColor(CSSValueAppleSystemOpaqueTertiaryFill, styleColorOptions)))
            return;

        style.setBackgroundColor(systemColor(CSSValueWebkitControlBackground, styleColorOptions));
    };

    bool useAlternateDesign = element->document().settings().alternateFormControlDesignEnabled();
    if (useAlternateDesign) {
        if (hasTextfieldAppearance)
            style.setBackgroundColor(Color::transparentBlack);
        else
            adjustBackgroundColor();
        style.resetBorderExceptRadius();
        return;
    }

    if (hasTextfieldAppearance)
        return;

    adjustBackgroundColor();
}

void RenderThemeIOS::paintTextFieldInnerShadow(const PaintInfo& paintInfo, const FloatRoundedRect& roundedRect)
{
    auto& context = paintInfo.context();

    const FloatSize innerShadowOffset { 0, 5 };
    constexpr auto innerShadowBlur = 10.0f;
    auto innerShadowColor = DisplayP3<float> { 0, 0, 0, 0.04f };
    context.setShadow(innerShadowOffset, innerShadowBlur, innerShadowColor);
    context.setFillColor(Color::black);

    Path innerShadowPath;
    FloatRect innerShadowRect = roundedRect.rect();
    innerShadowRect.inflate(std::max<float>(innerShadowOffset.width(), innerShadowOffset.height()) + innerShadowBlur);
    innerShadowPath.addRect(innerShadowRect);

    FloatRoundedRect innerShadowHoleRect = roundedRect;
    // FIXME: This is not from the spec; but without it we get antialiasing fringe from the fill; we need a better solution.
    innerShadowHoleRect.inflate(0.5);
    innerShadowPath.addRoundedRect(innerShadowHoleRect);

    context.setFillRule(WindRule::EvenOdd);
    context.fillPath(innerShadowPath);
}

void RenderThemeIOS::paintTextFieldDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto& style = box.style();
    auto roundedRect = style.getRoundedBorderFor(LayoutRect(rect)).pixelSnappedRoundedRectForPainting(box.document().deviceScaleFactor());

#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (box.settings().iOSFormControlRefreshEnabled()) {
        bool shouldPaintFillAndInnerShadow = false;
        auto element = box.element();
        if (is<HTMLInputElement>(*element)) {
            auto& input = downcast<HTMLInputElement>(*element);
            if (input.isTextField() && !input.isSearchField())
                shouldPaintFillAndInnerShadow = true;
        } else if (is<HTMLTextAreaElement>(*element))
            shouldPaintFillAndInnerShadow = true;

        bool useAlternateDesign = box.settings().alternateFormControlDesignEnabled();
        if (useAlternateDesign && shouldPaintFillAndInnerShadow) {
            Path path;
            path.addRoundedRect(roundedRect);

            context.setFillColor(Color::black.colorWithAlphaByte(10));
            context.drawPath(path);
            context.clipPath(path);
            paintTextFieldInnerShadow(paintInfo, roundedRect);
        }

        return;
    }
#endif

    context.clipRoundedRect(roundedRect);

    // This gradient gets drawn black when printing.
    // Do not draw the gradient if there is no visible top border.
    bool topBorderIsInvisible = !style.hasBorder() || !style.borderTopWidth() || style.borderTopIsTransparent();
    if (!box.view().printing() && !topBorderIsInvisible) {
        FloatPoint point(rect.x() + style.borderLeftWidth(), rect.y() + style.borderTopWidth());
        drawAxialGradient(context.platformContext(), gradientWithName(InsetGradient), point, FloatPoint(CGPointMake(point.x(), point.y() + 3.0f)), LinearInterpolation);
    }
}

void RenderThemeIOS::adjustTextAreaStyle(RenderStyle& style, const Element* element) const
{
    if (!element)
        return;

    if (!element->document().settings().alternateFormControlDesignEnabled())
        return;

    style.setBackgroundColor(Color::transparentBlack);
    style.resetBorderExceptRadius();
}

void RenderThemeIOS::paintTextAreaDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    paintTextFieldDecorations(box, paintInfo, rect);
}

// These values are taken from the UIKit button system.
constexpr auto largeButtonSize = 45;
constexpr auto largeButtonBorderRadiusRatio = 0.35f / 2;

const int MenuListMinHeight = 15;

const float MenuListBaseHeight = 20;
const float MenuListBaseFontSize = 11;

const float MenuListArrowWidth = 7;
const float MenuListArrowHeight = 6;
const float MenuListButtonPaddingAfter = 19;

LengthBox RenderThemeIOS::popupInternalPaddingBox(const RenderStyle& style, const Settings& settings) const
{
    float padding = MenuListButtonPaddingAfter;
    if (settings.iOSFormControlRefreshEnabled()) {
        auto emSize = CSSPrimitiveValue::create(1.0, CSSUnitType::CSS_EMS);
        padding = emSize->computeLength<float>({ style, nullptr, nullptr, nullptr });
    }

    if (style.effectiveAppearance() == MenulistButtonPart) {
        if (style.direction() == TextDirection::RTL)
            return { 0, 0, 0, static_cast<int>(padding + style.borderTopWidth()) };
        return { 0, static_cast<int>(padding + style.borderTopWidth()), 0, 0 };
    }
    return { 0, 0, 0, 0 };
}

static inline bool canAdjustBorderRadiusForAppearance(ControlPart appearance, const RenderBox& box)
{
    switch (appearance) {
    case NoControlPart:
#if ENABLE(APPLE_PAY)
    case ApplePayButtonPart:
#endif
        return false;
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    case SearchFieldPart:
        return !box.settings().iOSFormControlRefreshEnabled();
    case MenulistButtonPart:
        return !box.style().hasExplicitlySetBorderRadius() && box.settings().iOSFormControlRefreshEnabled();
#endif
    default:
        return true;
    };
}

void RenderThemeIOS::adjustRoundBorderRadius(RenderStyle& style, RenderBox& box)
{
    if (!canAdjustBorderRadiusForAppearance(style.effectiveAppearance(), box) || style.backgroundLayers().hasImage())
        return;

    if ((is<RenderButton>(box) || is<RenderMenuList>(box)) && box.height() >= largeButtonSize) {
        auto largeButtonBorderRadius = std::min(box.width(), box.height()) * largeButtonBorderRadiusRatio;
        style.setBorderRadius({ { largeButtonBorderRadius, LengthType::Fixed }, { largeButtonBorderRadius, LengthType::Fixed } });
        return;
    }

    // FIXME: We should not be relying on border radius for the appearance of our controls <rdar://problem/7675493>.
    style.setBorderRadius({ { std::min(box.width(), box.height()) / 2, LengthType::Fixed }, { box.height() / 2, LengthType::Fixed } });
}

static void applyCommonButtonPaddingToStyle(RenderStyle& style, const Element& element)
{
    Document& document = element.document();
    auto emSize = CSSPrimitiveValue::create(0.5, CSSUnitType::CSS_EMS);
    // We don't need this element's parent style to calculate `em` units, so it's okay to pass nullptr for it here.
    int pixels = emSize->computeLength<int>({ style, document.renderStyle(), nullptr, document.renderView() });
    style.setPaddingBox(LengthBox(0, pixels, 0, pixels));
}

static void adjustSelectListButtonStyle(RenderStyle& style, const Element& element)
{
    // Enforce "padding: 0 0.5em".
    applyCommonButtonPaddingToStyle(style, element);

    // Enforce "line-height: normal".
    style.setLineHeight(Length(-100.0, LengthType::Percent));
}
    
class RenderThemeMeasureTextClient : public MeasureTextClient {
public:
    RenderThemeMeasureTextClient(const FontCascade& font, const RenderStyle& style)
        : m_font(font)
        , m_style(style)
    {
    }
    float measureText(const String& string) const override
    {
        TextRun run = RenderBlock::constructTextRun(string, m_style);
        return m_font.width(run);
    }
private:
    const FontCascade& m_font;
    const RenderStyle& m_style;
};

static void adjustInputElementButtonStyle(RenderStyle& style, const HTMLInputElement& inputElement)
{
    // Always Enforce "padding: 0 0.5em".
    applyCommonButtonPaddingToStyle(style, inputElement);

    // Don't adjust the style if the width is specified.
    if (style.width().isFixed() && style.width().value() > 0)
        return;

    // Don't adjust for unsupported date input types.
    DateComponentsType dateType = inputElement.dateType();
    if (dateType == DateComponentsType::Invalid || dateType == DateComponentsType::Week)
        return;

    // Enforce the width and set the box-sizing to content-box to not conflict with the padding.
    FontCascade font = style.fontCascade();
    
    float maximumWidth = localizedDateCache().maximumWidthForDateType(dateType, font, RenderThemeMeasureTextClient(font, style));

    ASSERT(maximumWidth >= 0);

    if (maximumWidth > 0) {
        int width = static_cast<int>(maximumWidth + MenuListButtonPaddingAfter);
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
        if (inputElement.document().settings().iOSFormControlRefreshEnabled())
            width = static_cast<int>(std::ceil(maximumWidth));
#endif
        style.setWidth(Length(width, LengthType::Fixed));
        style.setBoxSizing(BoxSizing::ContentBox);
    }
}

void RenderThemeIOS::adjustMenuListButtonStyle(RenderStyle& style, const Element* element) const
{
    adjustStyleForAlternateFormControlDesignTransition(style, element);

    // Set the min-height to be at least MenuListMinHeight.
    if (style.height().isAuto())
        style.setMinHeight(Length(std::max(MenuListMinHeight, static_cast<int>(MenuListBaseHeight / MenuListBaseFontSize * style.fontDescription().computedSize())), LengthType::Fixed));
    else
        style.setMinHeight(Length(MenuListMinHeight, LengthType::Fixed));

    if (!element)
        return;

    adjustButtonLikeControlStyle(style, *element);

    // Enforce some default styles in the case that this is a non-multiple <select> element,
    // or a date input. We don't force these if this is just an element with
    // "-webkit-appearance: menulist-button".
    if (is<HTMLSelectElement>(*element) && !element->hasAttributeWithoutSynchronization(HTMLNames::multipleAttr))
        adjustSelectListButtonStyle(style, *element);
    else if (is<HTMLInputElement>(*element))
        adjustInputElementButtonStyle(style, downcast<HTMLInputElement>(*element));
}

void RenderThemeIOS::paintMenuListButtonDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (box.settings().iOSFormControlRefreshEnabled()) {
        paintMenuListButtonDecorationsWithFormControlRefresh(box, paintInfo, rect);
        return;
    }
#endif

    auto& style = box.style();
    bool isRTL = style.direction() == TextDirection::RTL;
    float borderTopWidth = style.borderTopWidth();
    FloatRect clip(rect.x() + style.borderLeftWidth(), rect.y() + style.borderTopWidth(), rect.width() - style.borderLeftWidth() - style.borderRightWidth(), rect.height() - style.borderTopWidth() - style.borderBottomWidth());
    CGContextRef cgContext = paintInfo.context().platformContext();

    float adjustLeft = 0.5;
    float adjustRight = 0.5;
    float adjustTop = 0.5;
    float adjustBottom = 0.5;

    // Paint title portion.
    {
        float leftInset = isRTL ? MenuListButtonPaddingAfter : 0;
        FloatRect titleClip(clip.x() + leftInset - adjustLeft, clip.y() - adjustTop, clip.width() - MenuListButtonPaddingAfter + adjustLeft, clip.height() + adjustTop + adjustBottom);

        GraphicsContextStateSaver stateSaver(paintInfo.context());

        FloatSize topLeftRadius;
        FloatSize topRightRadius;
        FloatSize bottomLeftRadius;
        FloatSize bottomRightRadius;

        if (isRTL) {
            topRightRadius = FloatSize(valueForLength(style.borderTopRightRadius().width, rect.width()) - style.borderRightWidth(), valueForLength(style.borderTopRightRadius().height, rect.height()) - style.borderTopWidth());
            bottomRightRadius = FloatSize(valueForLength(style.borderBottomRightRadius().width, rect.width()) - style.borderRightWidth(), valueForLength(style.borderBottomRightRadius().height, rect.height()) - style.borderBottomWidth());
        } else {
            topLeftRadius = FloatSize(valueForLength(style.borderTopLeftRadius().width, rect.width()) - style.borderLeftWidth(), valueForLength(style.borderTopLeftRadius().height, rect.height()) - style.borderTopWidth());
            bottomLeftRadius = FloatSize(valueForLength(style.borderBottomLeftRadius().width, rect.width()) - style.borderLeftWidth(), valueForLength(style.borderBottomLeftRadius().height, rect.height()) - style.borderBottomWidth());
        }

        paintInfo.context().clipRoundedRect(FloatRoundedRect(titleClip,
            topLeftRadius, topRightRadius,
            bottomLeftRadius, bottomRightRadius));

        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), titleClip.location(), FloatPoint(titleClip.x(), titleClip.maxY()), LinearInterpolation);
        drawAxialGradient(cgContext, gradientWithName(ShineGradient), FloatPoint(titleClip.x(), titleClip.maxY()), titleClip.location(), ExponentialInterpolation);
    }

    // Draw the separator after the initial padding.

    float separatorPosition = isRTL ? (clip.x() + MenuListButtonPaddingAfter) : (clip.maxX() - MenuListButtonPaddingAfter);

    BorderPainter::drawLineForBoxSide(paintInfo.context(), box.document(), FloatRect(FloatPoint(separatorPosition - borderTopWidth, clip.y()), FloatPoint(separatorPosition, clip.maxY())), BoxSide::Right, style.visitedDependentColor(CSSPropertyBorderTopColor), style.borderTopStyle(), 0, 0);

    FloatRect buttonClip;
    if (isRTL)
        buttonClip = FloatRect(clip.x() - adjustTop, clip.y() - adjustTop, MenuListButtonPaddingAfter + adjustTop + adjustLeft, clip.height() + adjustTop + adjustBottom);
    else
        buttonClip = FloatRect(separatorPosition - adjustTop, clip.y() - adjustTop, MenuListButtonPaddingAfter + adjustTop + adjustRight, clip.height() + adjustTop + adjustBottom);

    // Now paint the button portion.
    {
        GraphicsContextStateSaver stateSaver(paintInfo.context());

        FloatSize topLeftRadius;
        FloatSize topRightRadius;
        FloatSize bottomLeftRadius;
        FloatSize bottomRightRadius;

        if (isRTL) {
            topLeftRadius = FloatSize(valueForLength(style.borderTopLeftRadius().width, rect.width()) - style.borderLeftWidth(), valueForLength(style.borderTopLeftRadius().height, rect.height()) - style.borderTopWidth());
            bottomLeftRadius = FloatSize(valueForLength(style.borderBottomLeftRadius().width, rect.width()) - style.borderLeftWidth(), valueForLength(style.borderBottomLeftRadius().height, rect.height()) - style.borderBottomWidth());
        } else {
            topRightRadius = FloatSize(valueForLength(style.borderTopRightRadius().width, rect.width()) - style.borderRightWidth(), valueForLength(style.borderTopRightRadius().height, rect.height()) - style.borderTopWidth());
            bottomRightRadius = FloatSize(valueForLength(style.borderBottomRightRadius().width, rect.width()) - style.borderRightWidth(), valueForLength(style.borderBottomRightRadius().height, rect.height()) - style.borderBottomWidth());
        }

        paintInfo.context().clipRoundedRect(FloatRoundedRect(buttonClip,
            topLeftRadius, topRightRadius,
            bottomLeftRadius, bottomRightRadius));

        paintInfo.context().fillRect(buttonClip, style.visitedDependentColor(CSSPropertyBorderTopColor));

        drawAxialGradient(cgContext, gradientWithName(isFocused(box) && !isReadOnlyControl(box) ? ConcaveGradient : ConvexGradient), buttonClip.location(), FloatPoint(buttonClip.x(), buttonClip.maxY()), LinearInterpolation);
    }

    // Paint Indicators.

    if (box.isMenuList() && downcast<HTMLSelectElement>(box.element())->multiple()) {
        int size = 2;
        int count = 3;
        int padding = 3;

        FloatRect ellipse(buttonClip.x() + (buttonClip.width() - count * (size + padding) + padding) / 2.0, buttonClip.maxY() - 10.0, size, size);

        for (int i = 0; i < count; ++i) {
            paintInfo.context().drawRaisedEllipse(ellipse, Color::white, Color::black.colorWithAlphaByte(128));
            ellipse.move(size + padding, 0);
        }
    }  else {
        float centerX = floorf(buttonClip.x() + buttonClip.width() / 2.0) - 0.5;
        float centerY = floorf(buttonClip.y() + buttonClip.height() * 3.0 / 8.0);

        Vector<FloatPoint> arrow = {
            { centerX - MenuListArrowWidth / 2, centerY },
            { centerX + MenuListArrowWidth / 2, centerY },
            { centerX, centerY + MenuListArrowHeight }
        };

        Vector<FloatPoint> shadow = {
            { arrow[0].x(), arrow[0].y() + 1 },
            { arrow[1].x(), arrow[1].y() + 1 },
            { arrow[2].x(), arrow[2].y() + 1 }
        };

        uint8_t opacity = isReadOnlyControl(box) ? 51 : 128;
        paintInfo.context().setStrokeColor(Color::black.colorWithAlphaByte(opacity));
        paintInfo.context().setFillColor(Color::black.colorWithAlphaByte(opacity));
        paintInfo.context().drawPath(Path::polygonPathFromPoints(shadow));

        paintInfo.context().setStrokeColor(Color::white);
        paintInfo.context().setFillColor(Color::white);
        paintInfo.context().drawPath(Path::polygonPathFromPoints(arrow));
    }
}

const CGFloat kTrackThickness = 4.0;
const CGFloat kTrackRadius = kTrackThickness / 2.0;
const int kDefaultSliderThumbSize = 16;

void RenderThemeIOS::adjustSliderTrackStyle(RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSliderTrackStyle(style, element);

    // FIXME: We should not be relying on border radius for the appearance of our controls <rdar://problem/7675493>.
    int radius = static_cast<int>(kTrackRadius);
    style.setBorderRadius({ { radius, LengthType::Fixed }, { radius, LengthType::Fixed } });
}

bool RenderThemeIOS::paintSliderTrack(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (box.settings().iOSFormControlRefreshEnabled())
        return paintSliderTrackWithFormControlRefresh(box, paintInfo, rect);
#endif

    IntRect trackClip = rect;
    auto& style = box.style();

    bool isHorizontal = true;
    switch (style.effectiveAppearance()) {
    case SliderHorizontalPart:
        isHorizontal = true;
        // Inset slightly so the thumb covers the edge.
        if (trackClip.width() > 2) {
            trackClip.setWidth(trackClip.width() - 2);
            trackClip.setX(trackClip.x() + 1);
        }
        trackClip.setHeight(static_cast<int>(kTrackThickness));
        trackClip.setY(rect.y() + rect.height() / 2 - kTrackThickness / 2);
        break;
    case SliderVerticalPart:
        isHorizontal = false;
        // Inset slightly so the thumb covers the edge.
        if (trackClip.height() > 2) {
            trackClip.setHeight(trackClip.height() - 2);
            trackClip.setY(trackClip.y() + 1);
        }
        trackClip.setWidth(kTrackThickness);
        trackClip.setX(rect.x() + rect.width() / 2 - kTrackThickness / 2);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    ASSERT(trackClip.width() >= 0);
    ASSERT(trackClip.height() >= 0);
    CGFloat cornerWidth = trackClip.width() < kTrackThickness ? trackClip.width() / 2.0f : kTrackRadius;
    CGFloat cornerHeight = trackClip.height() < kTrackThickness ? trackClip.height() / 2.0f : kTrackRadius;

    bool readonly = isReadOnlyControl(box);

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(box, paintInfo, trackClip);
#endif

    // Draw the track gradient.
    {
        GraphicsContextStateSaver stateSaver(paintInfo.context());

        IntSize cornerSize(cornerWidth, cornerHeight);
        FloatRoundedRect innerBorder(trackClip, cornerSize, cornerSize, cornerSize, cornerSize);
        paintInfo.context().clipRoundedRect(innerBorder);

        CGContextRef cgContext = paintInfo.context().platformContext();
        IOSGradientRef gradient = readonly ? gradientWithName(ReadonlySliderTrackGradient) : gradientWithName(SliderTrackGradient);
        if (isHorizontal)
            drawAxialGradient(cgContext, gradient, trackClip.location(), FloatPoint(trackClip.x(), trackClip.maxY()), LinearInterpolation);
        else
            drawAxialGradient(cgContext, gradient, trackClip.location(), FloatPoint(trackClip.maxX(), trackClip.y()), LinearInterpolation);
    }

    // Draw the track border.
    {
        GraphicsContextStateSaver stateSaver(paintInfo.context());

        CGContextRef cgContext = paintInfo.context().platformContext();
        if (readonly)
            paintInfo.context().setStrokeColor(SRGBA<uint8_t> { 178, 178, 178 });
        else
            paintInfo.context().setStrokeColor(SRGBA<uint8_t> { 76, 76, 76 });

        RetainPtr<CGMutablePathRef> roundedRectPath = adoptCF(CGPathCreateMutable());
        CGPathAddRoundedRect(roundedRectPath.get(), 0, trackClip, cornerWidth, cornerHeight);
        CGContextAddPath(cgContext, roundedRectPath.get());
        CGContextSetLineWidth(cgContext, 1);
        CGContextStrokePath(cgContext);
    }

    return false;
}

void RenderThemeIOS::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    if (style.effectiveAppearance() != SliderThumbHorizontalPart && style.effectiveAppearance() != SliderThumbVerticalPart)
        return;

    // Enforce "border-radius: 50%".
    style.setBorderRadius({ { 50, LengthType::Percent }, { 50, LengthType::Percent } });

    // Enforce a 16x16 size if no size is provided.
    if (style.width().isIntrinsicOrAuto() || style.height().isAuto()) {
        style.setWidth({ kDefaultSliderThumbSize, LengthType::Fixed });
        style.setHeight({ kDefaultSliderThumbSize, LengthType::Fixed });
    }
}

void RenderThemeIOS::paintSliderThumbDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (box.settings().iOSFormControlRefreshEnabled())
        return;
#endif

    GraphicsContextStateSaver stateSaver(paintInfo.context());
    FloatRect clip = addRoundedBorderClip(box, paintInfo.context(), rect);

    CGContextRef cgContext = paintInfo.context().platformContext();
    FloatPoint bottomCenter(clip.x() + clip.width() / 2.0f, clip.maxY());
    if (isPressed(box))
        drawAxialGradient(cgContext, gradientWithName(SliderThumbOpaquePressedGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
    else {
        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
        drawRadialGradient(cgContext, gradientWithName(ShineGradient), bottomCenter, 0.0f, bottomCenter, std::max(clip.width(), clip.height()), ExponentialInterpolation);
    }
}

bool RenderThemeIOS::paintProgressBar(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (renderer.settings().iOSFormControlRefreshEnabled())
        return paintProgressBarWithFormControlRefresh(renderer, paintInfo, rect);
#endif

    if (!is<RenderProgress>(renderer))
        return true;

    const int progressBarHeight = 9;
    const float verticalOffset = (rect.height() - progressBarHeight) / 2.0f;

    GraphicsContextStateSaver stateSaver(paintInfo.context());
    if (rect.width() < 10 || rect.height() < 9) {
        // The rect is smaller than the standard progress bar. We clip to the element's rect to avoid
        // leaking pixels outside the repaint rect.
        paintInfo.context().clip(rect);
    }

    // 1) Draw the progress bar track.
    // 1.1) Draw the white background with grey gradient border.
    GraphicsContext& context = paintInfo.context();
    context.setStrokeThickness(0.68f);
    context.setStrokeStyle(SolidStroke);

    const float verticalRenderingPosition = rect.y() + verticalOffset;
    auto strokeGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1) }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    strokeGradient->addColorStop({ 0.0f, SRGBA<uint8_t> { 141, 141, 141 } });
    strokeGradient->addColorStop({ 0.45f, SRGBA<uint8_t> { 238, 238, 238 } });
    strokeGradient->addColorStop({ 0.55f, SRGBA<uint8_t> { 238, 238, 238 } });
    strokeGradient->addColorStop({ 1.0f, SRGBA<uint8_t> { 141, 141, 141 } });
    context.setStrokeGradient(WTFMove(strokeGradient));

    context.setFillColor(Color::black);

    Path trackPath;
    FloatRect trackRect(rect.x() + 0.25f, verticalRenderingPosition + 0.25f, rect.width() - 0.5f, progressBarHeight - 0.5f);
    FloatSize roundedCornerRadius(5, 4);
    trackPath.addRoundedRect(trackRect, roundedCornerRadius);
    context.drawPath(trackPath);

    // 1.2) Draw top gradient on the upper half. It is supposed to overlay the fill from the background and darker the stroked path.
    FloatRect border(rect.x(), rect.y() + verticalOffset, rect.width(), progressBarHeight);
    paintInfo.context().clipRoundedRect(FloatRoundedRect(border, roundedCornerRadius, roundedCornerRadius, roundedCornerRadius, roundedCornerRadius));

    float upperGradientHeight = progressBarHeight / 2.;
    auto upperGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition + 0.5f), FloatPoint(rect.x(), verticalRenderingPosition + upperGradientHeight - 1.5) }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    upperGradient->addColorStop({ 0.0f, SRGBA<uint8_t> { 133, 133, 133, 188 } });
    upperGradient->addColorStop({ 1.0f, SRGBA<uint8_t> { 18, 18, 18, 51 } });
    context.setFillGradient(WTFMove(upperGradient));

    context.fillRect(FloatRect(rect.x(), verticalRenderingPosition, rect.width(), upperGradientHeight));

    const auto& renderProgress = downcast<RenderProgress>(renderer);
    if (renderProgress.isDeterminate()) {
        // 2) Draw the progress bar.
        double position = clampTo(renderProgress.position(), 0.0, 1.0);
        float barWidth = position * rect.width();
        auto barGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition + 0.5f), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1) }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
        barGradient->addColorStop({ 0.0f, SRGBA<uint8_t> { 195, 217, 247 } });
        barGradient->addColorStop({ 0.45f, SRGBA<uint8_t> { 118, 164, 228 } });
        barGradient->addColorStop({ 0.49f, SRGBA<uint8_t> { 118, 164, 228 } });
        barGradient->addColorStop({ 0.51f, SRGBA<uint8_t> { 36, 114, 210 } });
        barGradient->addColorStop({ 0.55f, SRGBA<uint8_t> { 36, 114, 210 } });
        barGradient->addColorStop({ 1.0f, SRGBA<uint8_t> { 57, 142, 244 } });
        context.setFillGradient(WTFMove(barGradient));

        auto barStrokeGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1) }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
        barStrokeGradient->addColorStop({ 0.0f, SRGBA<uint8_t> { 95, 107, 183 } });
        barStrokeGradient->addColorStop({ 0.5f, SRGBA<uint8_t> { 66, 106, 174, 240 } });
        barStrokeGradient->addColorStop({ 1.0f, SRGBA<uint8_t> { 38, 104, 166 } });
        context.setStrokeGradient(WTFMove(barStrokeGradient));

        Path barPath;
        int left = rect.x();
        if (!renderProgress.style().isLeftToRightDirection())
            left = rect.maxX() - barWidth;
        FloatRect barRect(left + 0.25f, verticalRenderingPosition + 0.25f, std::max(barWidth - 0.5f, 0.0f), progressBarHeight - 0.5f);
        barPath.addRoundedRect(barRect, roundedCornerRadius);
        context.drawPath(barPath);
    }

    return false;
}

#if ENABLE(DATALIST_ELEMENT)
IntSize RenderThemeIOS::sliderTickSize() const
{
    // FIXME: <rdar://problem/12271791> MERGEBOT: Correct values for slider tick of <input type="range"> elements (requires ENABLE_DATALIST_ELEMENT)
    return IntSize(1, 3);
}

int RenderThemeIOS::sliderTickOffsetFromTrackCenter() const
{
    // FIXME: <rdar://problem/12271791> MERGEBOT: Correct values for slider tick of <input type="range"> elements (requires ENABLE_DATALIST_ELEMENT)
    return -9;
}
#endif

void RenderThemeIOS::adjustSearchFieldStyle(RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSearchFieldStyle(style, element);

    if (!element)
        return;

    if (!style.hasBorder())
        return;

    RenderBox* box = element->renderBox();
    if (!box)
        return;

    adjustRoundBorderRadius(style, *box);
}

void RenderThemeIOS::paintSearchFieldDecorations(const RenderBox& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintTextFieldDecorations(box, paintInfo, rect);
}

// This value matches the opacity applied to UIKit controls.
constexpr auto pressedStateOpacity = 0.75f;

bool RenderThemeIOS::isSubmitStyleButton(const Element& element) const
{
    if (is<HTMLInputElement>(element) && downcast<HTMLInputElement>(element).isSubmitButton())
        return true;

    if (is<HTMLButtonElement>(element) && downcast<HTMLButtonElement>(element).isExplicitlySetSubmitButton())
        return true;

    return false;
}

void RenderThemeIOS::adjustButtonLikeControlStyle(RenderStyle& style, const Element& element) const
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (!element.document().settings().iOSFormControlRefreshEnabled())
        return;

    // FIXME: Implement button-like control adjustments for the alternate design.
    if (element.document().settings().alternateFormControlDesignEnabled())
        return;

    if (element.isDisabledFormControl())
        return;

    if (!style.hasAutoAccentColor()) {
        auto tintColor = style.effectiveAccentColor();
        if (isSubmitStyleButton(element))
            style.setBackgroundColor(tintColor);
        else
            style.setColor(tintColor);
    }

    if (!element.active())
        return;

    auto textColor = style.color();
    if (textColor.isValid())
        style.setColor(textColor.colorWithAlphaMultipliedBy(pressedStateOpacity));

    auto backgroundColor = style.colorResolvingCurrentColor(style.backgroundColor());
    if (backgroundColor.isValid())
        style.setBackgroundColor(backgroundColor.colorWithAlphaMultipliedBy(pressedStateOpacity));
#endif
}

void RenderThemeIOS::adjustButtonStyle(RenderStyle& style, const Element* element) const
{
    adjustStyleForAlternateFormControlDesignTransition(style, element);

    // If no size is specified, ensure the height of the button matches ControlBaseHeight scaled
    // with the font size. min-height is used rather than height to avoid clipping the contents of
    // the button in cases where the button contains more than one line of text.
    if (style.width().isIntrinsicOrAuto() || style.height().isAuto())
        style.setMinHeight(Length(ControlBaseHeight / ControlBaseFontSize * style.fontDescription().computedSize(), LengthType::Fixed));

#if ENABLE(INPUT_TYPE_COLOR)
    if (style.effectiveAppearance() == ColorWellPart)
        return;
#endif

    // Set padding: 0 1.0em; on buttons.
    // CSSPrimitiveValue::computeLengthInt only needs the element's style to calculate em lengths.
    // Since the element might not be in a document, just pass nullptr for the root element style,
    // the parent element style, and the render view.
    auto emSize = CSSPrimitiveValue::create(1.0, CSSUnitType::CSS_EMS);
    int pixels = emSize->computeLength<int>({ style, nullptr, nullptr, nullptr });
    style.setPaddingBox(LengthBox(0, pixels, 0, pixels));

    if (!element)
        return;

    adjustButtonLikeControlStyle(style, *element);

    RenderBox* box = element->renderBox();
    if (!box)
        return;

    adjustRoundBorderRadius(style, *box);
}

void RenderThemeIOS::paintButtonDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintPushButtonDecorations(box, paintInfo, rect);
}

static bool shouldUseConvexGradient(const Color& backgroundColor)
{
    // FIXME: This should probably be using luminance.
    auto [r, g, b, a] = backgroundColor.toColorTypeLossy<SRGBA<float>>().resolved();
    float largestNonAlphaChannel = std::max({ r, g, b });
    return a > 0.5 && largestNonAlphaChannel < 0.5;
}

void RenderThemeIOS::paintPushButtonDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (box.settings().iOSFormControlRefreshEnabled())
        return;
#endif

    GraphicsContextStateSaver stateSaver(paintInfo.context());
    FloatRect clip = addRoundedBorderClip(box, paintInfo.context(), rect);

    CGContextRef cgContext = paintInfo.context().platformContext();
    if (shouldUseConvexGradient(box.style().visitedDependentColor(CSSPropertyBackgroundColor)))
        drawAxialGradient(cgContext, gradientWithName(ConvexGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
    else {
        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
        drawAxialGradient(cgContext, gradientWithName(ShineGradient), FloatPoint(clip.x(), clip.maxY()), clip.location(), ExponentialInterpolation);
    }
}

Color RenderThemeIOS::platformActiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const
{
    return Color::transparentBlack;
}

Color RenderThemeIOS::platformInactiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const
{
    return Color::transparentBlack;
}

static std::optional<Color>& cachedFocusRingColor()
{
    static NeverDestroyed<std::optional<Color>> color;
    return color;
}

Color RenderThemeIOS::systemFocusRingColor()
{
    if (!cachedFocusRingColor().has_value()) {
        // FIXME: Should be using -keyboardFocusIndicatorColor. For now, work around <rdar://problem/50838886>.
        cachedFocusRingColor() = colorFromCocoaColor([PAL::getUIColorClass() systemBlueColor]);
    }
    return *cachedFocusRingColor();
}

Color RenderThemeIOS::platformFocusRingColor(OptionSet<StyleColorOptions>) const
{
    return systemFocusRingColor();
}

Color RenderThemeIOS::platformAnnotationHighlightColor(OptionSet<StyleColorOptions>) const
{
    // FIXME: expose the real value from UIKit.
    return SRGBA<uint8_t> { 255, 238, 190 };
}

bool RenderThemeIOS::shouldHaveSpinButton(const HTMLInputElement&) const
{
    return false;
}

bool RenderThemeIOS::supportsFocusRing(const RenderStyle&) const
{
    return false;
}

bool RenderThemeIOS::supportsBoxShadow(const RenderStyle& style) const
{
    // FIXME: See if additional native controls can support box shadows.
    switch (style.effectiveAppearance()) {
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
        return true;
    default:
        return false;
    }
}

struct CSSValueSystemColorInformation {
    CSSValueID cssValueID;
    SEL selector;
    bool makeOpaque { false };
    float opacity { 1.0f };
};

static const Vector<CSSValueSystemColorInformation>& cssValueSystemColorInformationList()
{
    static NeverDestroyed<Vector<CSSValueSystemColorInformation>> cssValueSystemColorInformationList;

    static std::once_flag initializeOnce;
    std::call_once(
        initializeOnce,
        [] {
        cssValueSystemColorInformationList.get() = Vector(std::initializer_list<CSSValueSystemColorInformation> {
            { CSSValueCanvas, @selector(systemBackgroundColor) },
            { CSSValueCanvastext, @selector(labelColor) },
            { CSSValueText, @selector(labelColor) },
            { CSSValueWebkitControlBackground, @selector(systemBackgroundColor) },
            { CSSValueAppleSystemBlue, @selector(systemBlueColor) },
            { CSSValueAppleSystemBrown, @selector(systemBrownColor) },
            { CSSValueAppleSystemGray, @selector(systemGrayColor) },
            { CSSValueAppleSystemGreen, @selector(systemGreenColor) },
            { CSSValueAppleSystemIndigo, @selector(systemIndigoColor) },
            { CSSValueAppleSystemOrange, @selector(systemOrangeColor) },
            { CSSValueAppleSystemPink, @selector(systemPinkColor) },
            { CSSValueAppleSystemPurple, @selector(systemPurpleColor) },
            { CSSValueAppleSystemRed, @selector(systemRedColor) },
            { CSSValueAppleSystemTeal, @selector(systemTealColor) },
            { CSSValueAppleSystemYellow, @selector(systemYellowColor) },
            { CSSValueAppleSystemBackground, @selector(systemBackgroundColor) },
            { CSSValueAppleSystemSecondaryBackground, @selector(secondarySystemBackgroundColor) },
            { CSSValueAppleSystemTertiaryBackground, @selector(tertiarySystemBackgroundColor) },
            { CSSValueAppleSystemOpaqueFill, @selector(systemFillColor), true },
            { CSSValueAppleSystemOpaqueSecondaryFill, @selector(secondarySystemFillColor), true },
            // FIXME: <rdar://problem/75538507> UIKit should expose this color so that we maintain parity with system buttons.
            { CSSValueAppleSystemOpaqueSecondaryFillDisabled, @selector(secondarySystemFillColor), true, 0.75f },
            { CSSValueAppleSystemOpaqueTertiaryFill, @selector(tertiarySystemFillColor), true },
            { CSSValueAppleSystemGroupedBackground, @selector(systemGroupedBackgroundColor) },
            { CSSValueAppleSystemSecondaryGroupedBackground, @selector(secondarySystemGroupedBackgroundColor) },
            { CSSValueAppleSystemTertiaryGroupedBackground, @selector(tertiarySystemGroupedBackgroundColor) },
            { CSSValueAppleSystemLabel, @selector(labelColor) },
            { CSSValueAppleSystemSecondaryLabel, @selector(secondaryLabelColor) },
            { CSSValueAppleSystemTertiaryLabel, @selector(tertiaryLabelColor) },
            { CSSValueAppleSystemQuaternaryLabel, @selector(quaternaryLabelColor) },
            { CSSValueAppleSystemPlaceholderText, @selector(placeholderTextColor) },
            { CSSValueAppleSystemSeparator, @selector(separatorColor) },
            // FIXME: <rdar://problem/79471528> Adopt [UIColor opaqueSeparatorColor] once it has a high contrast variant.
            { CSSValueAppleSystemOpaqueSeparator, @selector(separatorColor), true },
            { CSSValueAppleSystemContainerBorder, @selector(separatorColor) },
            { CSSValueAppleSystemControlBackground, @selector(systemBackgroundColor) },
            { CSSValueAppleSystemGrid, @selector(separatorColor) },
            { CSSValueAppleSystemHeaderText, @selector(labelColor) },
            { CSSValueAppleSystemSelectedContentBackground, @selector(tableCellDefaultSelectionTintColor) },
            { CSSValueAppleSystemTextBackground, @selector(systemBackgroundColor) },
            { CSSValueAppleSystemUnemphasizedSelectedContentBackground, @selector(tableCellDefaultSelectionTintColor) },
            { CSSValueAppleWirelessPlaybackTargetActive, @selector(systemBlueColor) },
        });
    });

    return cssValueSystemColorInformationList;
}

static inline std::optional<Color> systemColorFromCSSValueSystemColorInformation(CSSValueSystemColorInformation systemColorInformation, bool useDarkAppearance)
{
    UIColor *color = wtfObjCMsgSend<UIColor *>(PAL::getUIColorClass(), systemColorInformation.selector);
    if (!color)
        return std::nullopt;

    Color systemColor(roundAndClampToSRGBALossy(color.CGColor), Color::Flags::Semantic);

    if (systemColorInformation.opacity < 1.0f)
        systemColor = systemColor.colorWithAlphaMultipliedBy(systemColorInformation.opacity);

    if (systemColorInformation.makeOpaque)
        return blendSourceOver(useDarkAppearance ? Color::black : Color::white, systemColor);

    return systemColor;
}

static std::optional<Color> systemColorFromCSSValueID(CSSValueID cssValueID, bool useDarkAppearance, bool useElevatedUserInterfaceLevel)
{
    LocalCurrentTraitCollection localTraitCollection(useDarkAppearance, useElevatedUserInterfaceLevel);

    for (auto& cssValueSystemColorInformation : cssValueSystemColorInformationList()) {
        if (cssValueSystemColorInformation.cssValueID == cssValueID)
            return systemColorFromCSSValueSystemColorInformation(cssValueSystemColorInformation, useDarkAppearance);
    }

    return std::nullopt;
}

static RenderThemeIOS::CSSValueToSystemColorMap& globalCSSValueToSystemColorMap()
{
    static NeverDestroyed<RenderThemeIOS::CSSValueToSystemColorMap> colorMap;
    return colorMap;
}

const RenderThemeIOS::CSSValueToSystemColorMap& RenderThemeIOS::cssValueToSystemColorMap()
{
    ASSERT(RunLoop::isMain());
    static const NeverDestroyed<CSSValueToSystemColorMap> colorMap = [] {
        CSSValueToSystemColorMap map;
        for (bool useDarkAppearance : { false, true }) {
            for (bool useElevatedUserInterfaceLevel : { false, true }) {
                LocalCurrentTraitCollection localTraitCollection(useDarkAppearance, useElevatedUserInterfaceLevel);
                for (auto& cssValueSystemColorInformation : cssValueSystemColorInformationList()) {
                    if (auto color = systemColorFromCSSValueSystemColorInformation(cssValueSystemColorInformation, useDarkAppearance))
                        map.add(CSSValueKey { cssValueSystemColorInformation.cssValueID, useDarkAppearance, useElevatedUserInterfaceLevel }, WTFMove(*color));
                }
            }
        }
        return map;
    }();
    return colorMap;
}

void RenderThemeIOS::setCSSValueToSystemColorMap(CSSValueToSystemColorMap&& colorMap)
{
    globalCSSValueToSystemColorMap() = WTFMove(colorMap);
}

void RenderThemeIOS::setFocusRingColor(const Color& color)
{
    cachedFocusRingColor() = color;
}

Color RenderThemeIOS::systemColor(CSSValueID cssValueID, OptionSet<StyleColorOptions> options) const
{
    const bool forVisitedLink = options.contains(StyleColorOptions::ForVisitedLink);

    // The system color cache below can't handle visited links. The only color value
    // that cares about visited links is CSSValueWebkitLink, so handle it here by
    // calling through to RenderTheme's base implementation.
    if (forVisitedLink && cssValueID == CSSValueWebkitLink)
        return RenderTheme::systemColor(cssValueID, options);

    ASSERT(!forVisitedLink);

    auto& cache = colorCache(options);
    return cache.systemStyleColors.ensure(cssValueID, [this, cssValueID, options] () -> Color {
        const bool useDarkAppearance = options.contains(StyleColorOptions::UseDarkAppearance);
        const bool useElevatedUserInterfaceLevel = options.contains(StyleColorOptions::UseElevatedUserInterfaceLevel);
        if (!globalCSSValueToSystemColorMap().isEmpty()) {
            auto it = globalCSSValueToSystemColorMap().find(CSSValueKey { cssValueID, useDarkAppearance, useElevatedUserInterfaceLevel });
            if (it == globalCSSValueToSystemColorMap().end())
                return RenderTheme::systemColor(cssValueID, options);
            return it->value.semanticColor();
        }
        auto color = systemColorFromCSSValueID(cssValueID, useDarkAppearance, useElevatedUserInterfaceLevel);
        if (color)
            return *color;
        return RenderTheme::systemColor(cssValueID, options);
    }).iterator->value;
}

Color RenderThemeIOS::pictureFrameColor(const RenderObject& buttonRenderer)
{
    return buttonRenderer.style().visitedDependentColor(CSSPropertyBorderTopColor);
}

Color RenderThemeIOS::controlTintColor(const RenderStyle& style, OptionSet<StyleColorOptions> options) const
{
    if (!style.hasAutoAccentColor())
        return style.effectiveAccentColor();

    return systemColor(CSSValueAppleSystemBlue, options);
}

#if ENABLE(ATTACHMENT_ELEMENT)

RenderThemeIOS::IconAndSize RenderThemeIOS::iconForAttachment(const String& fileName, const String& attachmentType, const String& title)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto documentInteractionController = adoptNS([PAL::allocUIDocumentInteractionControllerInstance() init]);
    ALLOW_DEPRECATED_DECLARATIONS_END

    [documentInteractionController setName:fileName.isEmpty() ? title : fileName];

    if (!attachmentType.isEmpty()) {
        String UTI;
        if (isDeclaredUTI(attachmentType))
            UTI = attachmentType;
        else
            UTI = UTIFromMIMEType(attachmentType);

#if PLATFORM(IOS)
        [documentInteractionController setUTI:static_cast<NSString *>(UTI)];
#endif
    }

    RetainPtr<UIImage> result;
#if PLATFORM(IOS)
    NSArray *icons = [documentInteractionController icons];
    if (!icons.count)
        return IconAndSize { nil, FloatSize() };

    result = icons.lastObject;

    BOOL useHeightForClosestMatch = [result size].height > [result size].width;
    CGFloat bestMatchRatio = -1;

    for (UIImage *icon in icons) {
        CGFloat iconSize = useHeightForClosestMatch ? icon.size.height : icon.size.width;

        CGFloat matchRatio = (attachmentIconSize / iconSize) - 1.0f;
        if (matchRatio < 0.3f) {
            matchRatio = CGFAbs(matchRatio);
            if ((bestMatchRatio == -1) || (matchRatio < bestMatchRatio)) {
                result = icon;
                bestMatchRatio = matchRatio;
            }
        }
    }
#endif
    CGFloat iconAspect = [result size].width / [result size].height;
    auto size = largestRectWithAspectRatioInsideRect(iconAspect, FloatRect(0, 0, attachmentIconSize, attachmentIconSize)).size();

    return IconAndSize { result, size };
}

LayoutSize RenderThemeIOS::attachmentIntrinsicSize(const RenderAttachment&) const
{
    return LayoutSize(FloatSize(attachmentSize) * attachmentDynamicTypeScaleFactor());
}

static void paintAttachmentIcon(GraphicsContext& context, AttachmentLayout& info)
{
    RefPtr<Image> iconImage;
    if (info.thumbnailIcon)
        iconImage = info.thumbnailIcon;
    else if (info.icon)
        iconImage = info.icon;
    
    context.drawImage(*iconImage, info.iconRect);
}

static void paintAttachmentProgress(GraphicsContext& context, AttachmentLayout& info)
{
    GraphicsContextStateSaver saver(context);

    context.setStrokeThickness(attachmentProgressBorderThickness);
    context.setStrokeColor(attachmentProgressColor);
    context.setFillColor(attachmentProgressColor);
    context.strokeEllipse(info.progressRect);

    FloatPoint center = info.progressRect.center();

    Path progressPath;
    progressPath.moveTo(center);
    progressPath.addLineTo(FloatPoint(center.x(), info.progressRect.y()));
    progressPath.addArc(center, info.progressRect.width() / 2, -M_PI_2, info.progress * 2 * M_PI - M_PI_2, 0);
    progressPath.closeSubpath();
    context.fillPath(progressPath);
}

static Path attachmentBorderPath(AttachmentLayout& info)
{
    auto insetAttachmentRect = info.attachmentRect;
    insetAttachmentRect.inflate(-attachmentBorderThickness / 2);

    Path borderPath;
    borderPath.addRoundedRect(insetAttachmentRect, FloatSize(attachmentBorderRadius, attachmentBorderRadius));
    return borderPath;
}

static void paintAttachmentBorder(GraphicsContext& context, Path& borderPath)
{
    context.setStrokeColor(attachmentBorderColor);
    context.setStrokeThickness(attachmentBorderThickness);
    context.strokePath(borderPath);
}

bool RenderThemeIOS::paintAttachment(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& paintRect)
{
    if (!is<RenderAttachment>(renderer))
        return false;

    const RenderAttachment& attachment = downcast<RenderAttachment>(renderer);

    AttachmentLayout info(attachment);

    GraphicsContext& context = paintInfo.context();
    GraphicsContextStateSaver saver(context);

    context.translate(toFloatSize(paintRect.location()));

    if (attachment.shouldDrawBorder()) {
        auto borderPath = attachmentBorderPath(info);
        paintAttachmentBorder(context, borderPath);
        context.clipPath(borderPath);
    }

    context.translate(FloatSize(0, info.contentYOrigin));

    if (info.hasProgress)
        paintAttachmentProgress(context, info);
    else if (info.icon || info.thumbnailIcon)
        paintAttachmentIcon(context, info);

    paintAttachmentText(context, &info);

    return true;
}

String RenderThemeIOS::attachmentStyleSheet() const
{
    ASSERT(DeprecatedGlobalSettings::attachmentElementEnabled());
    return "attachment { appearance: auto; color: -apple-system-blue; }"_s;
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if PLATFORM(WATCHOS)

String RenderThemeIOS::extraDefaultStyleSheet()
{
    return "* { -webkit-text-size-adjust: auto; -webkit-hyphens: auto !important; }"_s;
}

#endif

#if USE(SYSTEM_PREVIEW)
void RenderThemeIOS::paintSystemPreviewBadge(Image& image, const PaintInfo& paintInfo, const FloatRect& rect)
{
    paintInfo.context().drawSystemImage(ARKitBadgeSystemImage::create(image), rect);
}
#endif

#if ENABLE(IOS_FORM_CONTROL_REFRESH)

constexpr auto nativeControlBorderWidth = 1.0f;

constexpr auto checkboxRadioBorderWidth = 1.5f;
constexpr auto checkboxRadioBorderDisabledOpacity = 0.3f;

Color RenderThemeIOS::checkboxRadioBorderColor(OptionSet<ControlStates::States> states, OptionSet<StyleColorOptions> styleColorOptions)
{
    auto defaultBorderColor = systemColor(CSSValueAppleSystemSecondaryLabel, styleColorOptions);

    if (!states.contains(ControlStates::States::Enabled))
        return defaultBorderColor.colorWithAlphaMultipliedBy(checkboxRadioBorderDisabledOpacity);

    if (states.contains(ControlStates::States::Pressed))
        return defaultBorderColor.colorWithAlphaMultipliedBy(pressedStateOpacity);

    return defaultBorderColor;
}

Color RenderThemeIOS::checkboxRadioBackgroundColor(bool useAlternateDesign, const RenderStyle& style, OptionSet<ControlStates::States> states, OptionSet<StyleColorOptions> styleColorOptions)
{
    bool isEmpty = !states.containsAny({ ControlStates::States::Checked, ControlStates::States::Indeterminate });
    bool isEnabled = states.contains(ControlStates::States::Enabled);
    bool isPressed = states.contains(ControlStates::States::Pressed);

    if (useAlternateDesign) {
        // FIXME (rdar://problem/83895064): The disabled state for the alternate appearance is currently unspecified; this is just a guess.
        if (!isEnabled)
            return systemColor(isEmpty ? CSSValueWebkitControlBackground : CSSValueAppleSystemOpaqueTertiaryFill, styleColorOptions);

        if (isPressed)
            return isEmpty ? Color(DisplayP3<float> { 0.773, 0.773, 0.773 }) : Color(DisplayP3<float> { 0.067, 0.38, 0.953 });

        return isEmpty ? Color(DisplayP3<float> { 0.835, 0.835, 0.835 }) : Color(DisplayP3<float> { 0.203, 0.47, 0.964 });
    }

    if (!isEnabled)
        return systemColor(isEmpty ? CSSValueWebkitControlBackground : CSSValueAppleSystemOpaqueTertiaryFill, styleColorOptions);

    auto enabledBackgroundColor = isEmpty ? systemColor(CSSValueWebkitControlBackground, styleColorOptions) : controlTintColor(style, styleColorOptions);
    if (isPressed)
        return enabledBackgroundColor.colorWithAlphaMultipliedBy(pressedStateOpacity);

    return enabledBackgroundColor;
}

RefPtr<Gradient> RenderThemeIOS::checkboxRadioBackgroundGradient(const FloatRect& rect, OptionSet<ControlStates::States> states)
{
    bool isPressed = states.contains(ControlStates::States::Pressed);
    if (isPressed)
        return nullptr;

    bool isEmpty = !states.containsAny({ ControlStates::States::Checked, ControlStates::States::Indeterminate });
    auto gradient = Gradient::create(Gradient::LinearData { rect.minXMinYCorner(), rect.maxXMaxYCorner() }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    gradient->addColorStop({ 0.0f, DisplayP3<float> { 0, 0, 0, isEmpty ? 0.05f : 0.125f }});
    gradient->addColorStop({ 1.0f, DisplayP3<float> { 0, 0, 0, 0 }});
    return gradient;
}

Color RenderThemeIOS::checkboxRadioIndicatorColor(OptionSet<ControlStates::States> states, OptionSet<StyleColorOptions> styleColorOptions)
{
    if (!states.contains(ControlStates::States::Enabled))
        return systemColor(CSSValueAppleSystemTertiaryLabel, styleColorOptions);

    Color enabledIndicatorColor = systemColor(CSSValueAppleSystemLabel, styleColorOptions | StyleColorOptions::UseDarkAppearance);
    if (states.contains(ControlStates::States::Pressed))
        return enabledIndicatorColor.colorWithAlphaMultipliedBy(pressedStateOpacity);

    return enabledIndicatorColor;
}

void RenderThemeIOS::paintCheckboxRadioInnerShadow(const PaintInfo& paintInfo, const FloatRoundedRect& roundedRect, OptionSet<ControlStates::States> states)
{
    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver { context };

    if (auto gradient = checkboxRadioBackgroundGradient(roundedRect.rect(), states)) {
        context.setFillGradient(*gradient);

        Path path;
        path.addRoundedRect(roundedRect);
        context.fillPath(path);
    }

    const FloatSize innerShadowOffset { 2, 2 };
    constexpr auto innerShadowBlur = 3.0f;

    bool isEmpty = !states.containsAny({ ControlStates::States::Checked, ControlStates::States::Indeterminate });
    auto firstShadowColor = DisplayP3<float> { 0, 0, 0, isEmpty ? 0.05f : 0.1f };
    context.setShadow(innerShadowOffset, innerShadowBlur, firstShadowColor);
    context.setFillColor(Color::black);

    Path innerShadowPath;
    FloatRect innerShadowRect = roundedRect.rect();
    innerShadowRect.inflate(std::max<float>(innerShadowOffset.width(), innerShadowOffset.height()) + innerShadowBlur);
    innerShadowPath.addRect(innerShadowRect);

    FloatRoundedRect innerShadowHoleRect = roundedRect;
    // FIXME: This is not from the spec; but without it we get antialiasing fringe from the fill; we need a better solution.
    innerShadowHoleRect.inflate(0.5);
    innerShadowPath.addRoundedRect(innerShadowHoleRect);

    context.setFillRule(WindRule::EvenOdd);
    context.fillPath(innerShadowPath);

    constexpr auto secondShadowColor = DisplayP3<float> { 1, 1, 1, 0.5f };
    context.setShadow(FloatSize { 0, 0 }, 1, secondShadowColor);

    context.fillPath(innerShadowPath);
}

bool RenderThemeIOS::paintCheckbox(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!box.settings().iOSFormControlRefreshEnabled())
        return true;

    bool useAlternateDesign = box.settings().alternateFormControlDesignEnabled();

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver { context };

    constexpr auto checkboxHeight = 16.0f;
    constexpr auto checkboxCornerRadius = 5.0f;

    FloatRoundedRect checkboxRect(rect, FloatRoundedRect::Radii(checkboxCornerRadius * rect.height() / checkboxHeight));

    auto controlStates = extractControlStatesForRenderer(box);
    auto styleColorOptions = box.styleColorOptions();

    auto backgroundColor = checkboxRadioBackgroundColor(useAlternateDesign, box.style(), controlStates, styleColorOptions);

    bool checked = controlStates.contains(ControlStates::States::Checked);
    bool indeterminate = controlStates.contains(ControlStates::States::Indeterminate);
    bool empty = !checked && !indeterminate;

    if (empty) {
        Path path;
        path.addRoundedRect(checkboxRect);
        if (!useAlternateDesign) {
            context.setStrokeColor(checkboxRadioBorderColor(controlStates, styleColorOptions));
            context.setStrokeThickness(checkboxRadioBorderWidth * 2);
            context.setStrokeStyle(SolidStroke);
        }
            
        context.setFillColor(backgroundColor);
        context.clipPath(path);
        context.drawPath(path);

        if (useAlternateDesign)
            paintCheckboxRadioInnerShadow(paintInfo, checkboxRect, controlStates);

        return false;
    }

    context.fillRoundedRect(checkboxRect, backgroundColor);

    if (useAlternateDesign) {
        context.clipRoundedRect(checkboxRect);
        paintCheckboxRadioInnerShadow(paintInfo, checkboxRect, controlStates);
    }

    Path path;
    if (indeterminate) {
        const FloatSize indeterminateBarRoundingRadii(1.25f, 1.25f);
        constexpr float indeterminateBarPadding = 2.5f;
        float height = 0.12f * rect.height();

        FloatRect indeterminateBarRect(rect.x() + indeterminateBarPadding, rect.center().y() - height / 2.0f, rect.width() - indeterminateBarPadding * 2, height);
        path.addRoundedRect(indeterminateBarRect, indeterminateBarRoundingRadii);
    } else {
        path.moveTo({ 28.174f, 68.652f });
        path.addBezierCurveTo({ 31.006f, 68.652f }, { 33.154f, 67.578f }, { 34.668f, 65.332f });
        path.addLineTo({ 70.02f, 11.28f });
        path.addBezierCurveTo({ 71.094f, 9.62f }, { 71.582f, 8.107f }, { 71.582f, 6.642f });
        path.addBezierCurveTo({ 71.582f, 2.784f }, { 68.652f, 0.001f }, { 64.697f, 0.001f });
        path.addBezierCurveTo({ 62.012f, 0.001f }, { 60.352f, 0.978f }, { 58.691f, 3.565f });
        path.addLineTo({ 28.027f, 52.1f });
        path.addLineTo({ 12.354f, 32.52f });
        path.addBezierCurveTo({ 10.84f, 30.664f }, { 9.18f, 29.834f }, { 6.884f, 29.834f });
        path.addBezierCurveTo({ 2.882f, 29.834f }, { 0.0f, 32.666f }, { 0.0f, 36.572f });
        path.addBezierCurveTo({ 0.0f, 38.282f }, { 0.537f, 39.795f }, { 2.002f, 41.504f });
        path.addLineTo({ 21.826f, 65.625f });
        path.addBezierCurveTo({ 23.536f, 67.675f }, { 25.536f, 68.652f }, { 28.174f, 68.652f });

        const FloatSize checkmarkSize(72.0f, 69.0f);
        float scale = (0.65f * rect.width()) / checkmarkSize.width();

        AffineTransform transform;
        transform.translate(rect.center() - (checkmarkSize * scale * 0.5f));
        transform.scale(scale);
        path.transform(transform);
    }

    context.setFillColor(checkboxRadioIndicatorColor(controlStates, styleColorOptions));
    context.fillPath(path);

    return false;
}

bool RenderThemeIOS::paintRadio(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!box.settings().iOSFormControlRefreshEnabled())
        return true;

    bool useAlternateDesign = box.settings().alternateFormControlDesignEnabled();

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto controlStates = extractControlStatesForRenderer(box);
    auto styleColorOptions = box.styleColorOptions();

    auto backgroundColor = checkboxRadioBackgroundColor(useAlternateDesign, box.style(), controlStates, styleColorOptions);

    FloatRoundedRect radioRect { rect, FloatRoundedRect::Radii(rect.width() / 2, rect.height() / 2) };

    if (controlStates.contains(ControlStates::States::Checked)) {
        context.setFillColor(backgroundColor);
        context.fillEllipse(rect);

        if (useAlternateDesign) {
            context.clipRoundedRect(radioRect);
            paintCheckboxRadioInnerShadow(paintInfo, radioRect, controlStates);
        }

        // The inner circle is 6 / 14 the size of the surrounding circle,
        // leaving 8 / 14 around it. (8 / 14) / 2 = 2 / 7.
        constexpr float innerInverseRatio = 2 / 7.0f;

        FloatRect innerCircleRect(rect);
        innerCircleRect.inflateX(-innerCircleRect.width() * innerInverseRatio);
        innerCircleRect.inflateY(-innerCircleRect.height() * innerInverseRatio);

        context.setFillColor(checkboxRadioIndicatorColor(controlStates, styleColorOptions));
        context.fillEllipse(innerCircleRect);
    } else {
        Path path;
        path.addEllipse(rect);
        if (!useAlternateDesign) {
            context.setStrokeColor(checkboxRadioBorderColor(controlStates, styleColorOptions));
            context.setStrokeThickness(checkboxRadioBorderWidth * 2);
            context.setStrokeStyle(SolidStroke);
        }
        context.setFillColor(backgroundColor);
        context.clipPath(path);
        context.drawPath(path);

        if (useAlternateDesign)
            paintCheckboxRadioInnerShadow(paintInfo, radioRect, controlStates);
    }

    return false;
}

constexpr Seconds progressAnimationRepeatInterval = 16_ms;

constexpr auto reducedMotionProgressAnimationMinOpacity = 0.3f;
constexpr auto reducedMotionProgressAnimationMaxOpacity = 0.6f;

Seconds RenderThemeIOS::animationRepeatIntervalForProgressBar(const RenderProgress& renderProgress) const
{
    if (!renderProgress.settings().iOSFormControlRefreshEnabled())
        return RenderTheme::animationRepeatIntervalForProgressBar(renderProgress);

    return progressAnimationRepeatInterval;
}

bool RenderThemeIOS::paintProgressBarWithFormControlRefresh(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!is<RenderProgress>(renderer))
        return true;
    auto& renderProgress = downcast<RenderProgress>(renderer);

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto styleColorOptions = renderer.styleColorOptions();

    constexpr auto barHeight = 4.0f;
    FloatRoundedRect::Radii barCornerRadii(2.5f, 1.5f);

    if (rect.height() < barHeight) {
        // The rect is smaller than the standard progress bar. We clip to the
        // element's rect to avoid leaking pixels outside the repaint rect.
        context.clip(rect);
    }

    float barTop = rect.y() + (rect.height() - barHeight) / 2.0f;

    FloatRect trackRect(rect.x() + nativeControlBorderWidth, barTop, rect.width() - 2 * nativeControlBorderWidth, barHeight);
    FloatRoundedRect roundedTrackRect(trackRect, barCornerRadii);

    FloatRoundedRect roundedTrackBorderRect(roundedTrackRect);
    roundedTrackBorderRect.inflateWithRadii(nativeControlBorderWidth);
    context.fillRoundedRect(roundedTrackBorderRect, systemColor(CSSValueWebkitControlBackground, styleColorOptions));

    context.fillRoundedRect(roundedTrackRect, systemColor(CSSValueAppleSystemOpaqueFill, styleColorOptions));

    float barWidth;
    float barLeft = trackRect.x();
    float alpha = 1.0f;

    if (renderProgress.isDeterminate()) {
        barWidth = clampTo<float>(renderProgress.position(), 0.0f, 1.0f) * trackRect.width();

        if (!renderProgress.style().isLeftToRightDirection())
            barLeft = trackRect.maxX() - barWidth;
    } else {
        Seconds elapsed = MonotonicTime::now() - renderProgress.animationStartTime();
        float position = fmodf(elapsed.value(), 1.0f);
        bool reverseDirection = static_cast<int>(elapsed.value()) % 2;

        if (Theme::singleton().userPrefersReducedMotion()) {
            barWidth = trackRect.width();

            float difference = position * (reducedMotionProgressAnimationMaxOpacity - reducedMotionProgressAnimationMinOpacity);
            if (reverseDirection)
                alpha = reducedMotionProgressAnimationMaxOpacity - difference;
            else
                alpha = reducedMotionProgressAnimationMinOpacity + difference;
        } else {
            barWidth = 0.25f * trackRect.width();

            float offset = position * (trackRect.width() + barWidth);
            if (reverseDirection)
                barLeft = trackRect.maxX() - offset;
            else
                barLeft -= barWidth - offset;

            context.clipRoundedRect(roundedTrackRect);
        }
    }

    FloatRect barRect(barLeft, barTop, barWidth, barHeight);
    context.fillRoundedRect(FloatRoundedRect(barRect, barCornerRadii), controlTintColor(renderer.style(), styleColorOptions).colorWithAlphaMultipliedBy(alpha));

    return false;
}

bool RenderThemeIOS::supportsMeter(ControlPart part, const HTMLMeterElement& element) const
{
    if (part == MeterPart)
        return element.document().settings().iOSFormControlRefreshEnabled();

    return false;
}

bool RenderThemeIOS::paintMeter(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderer.settings().iOSFormControlRefreshEnabled() || !is<RenderMeter>(renderer))
        return true;

    auto& renderMeter = downcast<RenderMeter>(renderer);
    RefPtr element = renderMeter.meterElement();

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto styleColorOptions = renderer.styleColorOptions();

    float cornerRadius = std::min(rect.width(), rect.height()) / 2.0f;
    FloatRoundedRect roundedFillRect(rect, FloatRoundedRect::Radii(cornerRadius));
    context.fillRoundedRect(roundedFillRect, systemColor(CSSValueWebkitControlBackground, styleColorOptions));

    roundedFillRect.inflateWithRadii(-nativeControlBorderWidth);
    context.fillRoundedRect(roundedFillRect, systemColor(CSSValueAppleSystemOpaqueTertiaryFill, styleColorOptions));

    context.clipRoundedRect(roundedFillRect);

    FloatRect fillRect(roundedFillRect.rect());
    if (renderMeter.style().isLeftToRightDirection())
        fillRect.move(fillRect.width() * (element->valueRatio() - 1), 0);
    else
        fillRect.move(fillRect.width() * (1 - element->valueRatio()), 0);
    roundedFillRect.setRect(fillRect);

    switch (element->gaugeRegion()) {
    case HTMLMeterElement::GaugeRegionOptimum:
        context.fillRoundedRect(roundedFillRect, systemColor(CSSValueAppleSystemGreen, styleColorOptions));
        break;
    case HTMLMeterElement::GaugeRegionSuboptimal:
        context.fillRoundedRect(roundedFillRect, systemColor(CSSValueAppleSystemYellow, styleColorOptions));
        break;
    case HTMLMeterElement::GaugeRegionEvenLessGood:
        context.fillRoundedRect(roundedFillRect, systemColor(CSSValueAppleSystemRed, styleColorOptions));
        break;
    }

    return false;
}

#if ENABLE(DATALIST_ELEMENT)

bool RenderThemeIOS::paintListButton(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto& style = box.style();

    float paddingTop = floatValueForLength(style.paddingTop(), rect.height());
    float paddingRight = floatValueForLength(style.paddingRight(), rect.width());
    float paddingBottom = floatValueForLength(style.paddingBottom(), rect.height());
    float paddingLeft = floatValueForLength(style.paddingLeft(), rect.width());

    FloatRect indicatorRect = rect;
    indicatorRect.move(paddingLeft, paddingTop);
    indicatorRect.contract(paddingLeft + paddingRight, paddingTop + paddingBottom);

    Path path;
    path.moveTo({ 35.48, 38.029 });
    path.addBezierCurveTo({ 36.904, 38.029 }, { 38.125, 37.5 }, { 39.223, 36.361 });
    path.addLineTo({ 63.352, 11.987 });
    path.addBezierCurveTo({ 64.206, 11.092 }, { 64.695, 9.993 }, { 64.695, 8.691 });
    path.addBezierCurveTo({ 64.695, 6.046 }, { 62.579, 3.971 }, { 59.975, 3.971 });
    path.addBezierCurveTo({ 58.714, 3.971 }, { 57.493, 4.5 }, { 56.557, 5.436 });
    path.addLineTo({ 35.52, 26.839 });
    path.addLineTo({ 14.443, 5.436 });
    path.addBezierCurveTo({ 13.507, 4.5 }, { 12.327, 3.971 }, { 10.984, 3.971 });
    path.addBezierCurveTo({ 8.38, 3.971 }, { 6.305, 6.046 }, { 6.305, 8.691 });
    path.addBezierCurveTo({ 6.305, 9.993 }, { 6.753, 11.092 }, { 7.648, 11.987 });
    path.addLineTo({ 31.777, 36.36 });
    path.addBezierCurveTo({ 32.916, 37.499 }, { 34.096, 38.028 }, { 35.48, 38.028 });

    const FloatSize indicatorSize(71.0f, 42.0f);
    float scale = indicatorRect.width() / indicatorSize.width();

    AffineTransform transform;
    transform.translate(rect.center() - (indicatorSize * scale * 0.5f));
    transform.scale(scale);
    path.transform(transform);

    context.setFillColor(controlTintColor(style, box.styleColorOptions()));
    context.fillPath(path);

    return false;
}

void RenderThemeIOS::paintSliderTicks(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!box.settings().iOSFormControlRefreshEnabled()) {
        RenderTheme::paintSliderTicks(box, paintInfo, rect);
        return;
    }

    if (!is<HTMLInputElement>(box.node()))
        return;

    auto& input = downcast<HTMLInputElement>(*box.node());
    if (!input.isRangeControl())
        return;

    auto dataList = input.dataList();
    if (!dataList)
        return;

    double min = input.minimum();
    double max = input.maximum();
    if (min >= max)
        return;

    constexpr int tickWidth = 2;
    constexpr int tickHeight = 8;
    constexpr int tickCornerRadius = 1;

    FloatRect tickRect;
    FloatRoundedRect::Radii tickCornerRadii(tickCornerRadius);

    bool isHorizontal = box.style().effectiveAppearance() == SliderHorizontalPart;
    if (isHorizontal) {
        tickRect.setWidth(tickWidth);
        tickRect.setHeight(tickHeight);
        tickRect.setY(rect.center().y() - tickRect.height() / 2.0f);
    } else {
        tickRect.setWidth(tickHeight);
        tickRect.setHeight(tickWidth);
        tickRect.setX(rect.center().x() - tickRect.width() / 2.0f);
    }

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto value = input.valueAsNumber();
    auto deviceScaleFactor = box.document().deviceScaleFactor();
    auto styleColorOptions = box.styleColorOptions();

    for (auto& optionElement : dataList->suggestions()) {
        if (auto optionValue = input.listOptionValueAsDouble(optionElement)) {
            auto tickFraction = (*optionValue - min) / (max - min);
            auto tickRatio = isHorizontal && box.style().isLeftToRightDirection() ? tickFraction : 1.0 - tickFraction;
            if (isHorizontal)
                tickRect.setX(rect.x() + tickRatio * (rect.width() - tickRect.width()));
            else
                tickRect.setY(rect.y() + tickRatio * (rect.height() - tickRect.height()));

            FloatRoundedRect roundedTickRect(snapRectToDevicePixels(LayoutRect(tickRect), deviceScaleFactor), tickCornerRadii);
            context.fillRoundedRect(roundedTickRect, (value >= *optionValue) ? controlTintColor(box.style(), styleColorOptions) : systemColor(CSSValueAppleSystemOpaqueSeparator, styleColorOptions));
        }
    }
}

#endif // ENABLE(DATALIST_ELEMENT)

bool RenderThemeIOS::paintSliderTrackWithFormControlRefresh(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!is<RenderSlider>(box))
        return true;
    auto& renderSlider = downcast<RenderSlider>(box);

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    bool isHorizontal = true;
    FloatRect trackClip = rect;

    switch (box.style().effectiveAppearance()) {
    case SliderHorizontalPart:
        // Inset slightly so the thumb covers the edge.
        if (trackClip.width() > 2) {
            trackClip.setWidth(trackClip.width() - 2);
            trackClip.setX(trackClip.x() + 1);
        }
        trackClip.setHeight(kTrackThickness);
        trackClip.setY(rect.y() + rect.height() / 2 - kTrackThickness / 2);
        break;
    case SliderVerticalPart:
        isHorizontal = false;
        // Inset slightly so the thumb covers the edge.
        if (trackClip.height() > 2) {
            trackClip.setHeight(trackClip.height() - 2);
            trackClip.setY(trackClip.y() + 1);
        }
        trackClip.setWidth(kTrackThickness);
        trackClip.setX(rect.x() + rect.width() / 2 - kTrackThickness / 2);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    auto styleColorOptions = box.styleColorOptions();

    auto cornerWidth = trackClip.width() < kTrackThickness ? trackClip.width() / 2.0f : kTrackRadius;
    auto cornerHeight = trackClip.height() < kTrackThickness ? trackClip.height() / 2.0f : kTrackRadius;

    FloatRoundedRect::Radii cornerRadii(cornerWidth, cornerHeight);
    FloatRoundedRect innerBorder(trackClip, cornerRadii);

    FloatRoundedRect outerBorder(innerBorder);
    outerBorder.inflateWithRadii(nativeControlBorderWidth);
    context.fillRoundedRect(outerBorder, systemColor(CSSValueWebkitControlBackground, styleColorOptions));

    context.fillRoundedRect(innerBorder, systemColor(CSSValueAppleSystemOpaqueFill, styleColorOptions));

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(box, paintInfo, trackClip);
#endif

    double valueRatio = renderSlider.valueRatio();
    if (isHorizontal) {
        double newWidth = trackClip.width() * valueRatio;

        if (!box.style().isLeftToRightDirection())
            trackClip.move(trackClip.width() - newWidth, 0);

        trackClip.setWidth(newWidth);
    } else {
        float height = trackClip.height();
        trackClip.setHeight(height * valueRatio);
        trackClip.setY(trackClip.y() + height - trackClip.height());
    }

    FloatRoundedRect fillRect(trackClip, cornerRadii);
    context.fillRoundedRect(fillRect, controlTintColor(box.style(), styleColorOptions));

    return false;
}

#if ENABLE(INPUT_TYPE_COLOR)

String RenderThemeIOS::colorInputStyleSheet(const Settings& settings) const
{
    if (!settings.iOSFormControlRefreshEnabled())
        return RenderTheme::colorInputStyleSheet(settings);

    return "input[type=\"color\"] { appearance: auto; width: 28px; height: 28px; box-sizing: border-box; outline: none; border: initial; border-radius: 50%; } "_s;
}

void RenderThemeIOS::adjustColorWellStyle(RenderStyle& style, const Element* element) const
{
    adjustStyleForAlternateFormControlDesignTransition(style, element);

    if (!element || element->document().settings().iOSFormControlRefreshEnabled())
        return;

    RenderTheme::adjustColorWellStyle(style, element);
}

bool RenderThemeIOS::paintColorWell(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!box.settings().iOSFormControlRefreshEnabled())
        return RenderTheme::paintColorWell(box, paintInfo, rect);

    return true;
}

void RenderThemeIOS::paintColorWellDecorations(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!box.settings().iOSFormControlRefreshEnabled()) {
        RenderTheme::paintColorWellDecorations(box, paintInfo, rect);
        return;
    }

    constexpr int strokeThickness = 3;
    constexpr DisplayP3<float> colorStops[] = {
        { 1, 1, 0, 1 },
        { 1, 0.5, 0, 1 },
        { 1, 0, 0, 1 },
        { 1, 0, 1, 1},
        { 0, 0, 1, 1 },
        { 0, 1, 1, 1 },
        { 0, 1, 0, 1},
        { 0.63, 0.88, 0.03, 1 },
        { 1, 1, 0, 1 }
    };
    constexpr int numColorStops = std::size(colorStops);

    auto gradient = Gradient::create(Gradient::ConicData { rect.center(), 0 }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    for (int i = 0; i < numColorStops; ++i)
        gradient->addColorStop({ i * 1.0f / (numColorStops - 1), colorStops[i] });

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    FloatRect strokeRect = rect;
    strokeRect.inflate(-strokeThickness / 2.0f);

    context.setStrokeThickness(strokeThickness);
    context.setStrokeStyle(SolidStroke);
    context.setStrokeGradient(WTFMove(gradient));
    context.strokeEllipse(strokeRect);
}

#endif // ENABLE(INPUT_TYPE_COLOR)

void RenderThemeIOS::paintMenuListButtonDecorationsWithFormControlRefresh(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (is<HTMLInputElement>(box.element()))
        return;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto& style = box.style();

    Path glyphPath;
    FloatSize glyphSize;

    if (box.isMenuList() && downcast<HTMLSelectElement>(box.element())->multiple()) {
        constexpr int length = 18;
        constexpr int count = 3;
        constexpr int padding = 12;

        FloatRect ellipse(0, 0, length, length);

        for (int i = 0; i < count; ++i) {
            glyphPath.addEllipse(ellipse);
            ellipse.move(length + padding, 0);
        }

        glyphSize = { length * count + padding * (count - 1), length };
    } else {
        constexpr int glyphWidth = 63;
        constexpr int glyphHeight = 73;
        glyphSize = { glyphWidth, glyphHeight };

        glyphPath.moveTo({ 31.8593f, 1.0f });
        glyphPath.addBezierCurveTo({ 30.541f, 1.0f }, { 29.418f, 1.586f }, { 28.0507f, 2.66f });
        glyphPath.addLineTo({ 2.5625f, 23.168f });
        glyphPath.addBezierCurveTo({ 1.5859f, 23.998f }, { 1.0f, 25.2188f }, { 1.0f, 26.7325f });
        glyphPath.addBezierCurveTo({ 1.0f, 29.6133f }, { 3.246f, 31.7129f }, { 5.9316f, 31.7129f });
        glyphPath.addBezierCurveTo({ 7.1523f, 31.7129f }, { 8.3242f, 31.2246f }, { 9.5449f, 30.248f });
        glyphPath.addLineTo({ 31.8593f, 12.377f });
        glyphPath.addLineTo({ 54.2226f, 30.248f });
        glyphPath.addBezierCurveTo({ 55.3945f, 31.2246f }, { 56.6152f, 31.7129f }, { 57.7871f, 31.7129 });
        glyphPath.addBezierCurveTo({ 60.4726f, 31.7129f }, { 62.7187f, 29.6133f }, { 62.7187f, 26.7325 });
        glyphPath.addBezierCurveTo({ 62.7187f, 25.2188f }, { 62.1327f, 23.9981f }, { 61.1562f, 23.168 });
        glyphPath.addLineTo({ 35.6679f, 2.6602f });
        glyphPath.addBezierCurveTo({ 34.3496f, 1.586f }, { 33.1777f, 1.0f }, { 31.8593f, 1.0f });
        glyphPath.moveTo({ 31.8593f, 72.3867f });
        glyphPath.addBezierCurveTo({ 33.1777f, 72.3867f }, { 34.3496f, 71.8007f }, { 35.6679f, 70.7266f });
        glyphPath.addLineTo({ 61.1562f, 50.2188f });
        glyphPath.addBezierCurveTo({ 62.1328f, 49.3888f }, { 62.7187f, 48.168f }, { 62.7187f, 46.6543f });
        glyphPath.addBezierCurveTo({ 62.7187f, 43.7735f }, { 60.4726f, 41.6739f }, { 57.7871f, 41.6739f });
        glyphPath.addBezierCurveTo({ 56.6151f, 41.6739f }, { 55.3945f, 42.162f }, { 54.2226f, 43.09f });
        glyphPath.addLineTo({ 31.8593f, 61.01f });
        glyphPath.addLineTo({ 9.545f, 43.0898f });
        glyphPath.addBezierCurveTo({ 8.3243f, 42.1619f }, { 7.1524f, 41.6738f }, { 5.9317f, 41.6738f });
        glyphPath.addBezierCurveTo({ 3.246f, 41.6739f }, { 1.0f, 43.7735f }, { 1.0f, 46.6543f });
        glyphPath.addBezierCurveTo({ 1.0f, 48.168f }, { 1.5859, 49.3887 }, { 2.5625, 50.2188f });
        glyphPath.addLineTo({ 28.0507f, 70.7266f });
        glyphPath.addBezierCurveTo({ 29.4179f, 71.8f }, { 30.541f, 72.3867f }, { 31.8593f, 72.3867 });
    }

    auto emSize = CSSPrimitiveValue::create(1.0, CSSUnitType::CSS_EMS);
    auto emPixels = emSize->computeLength<float>({ style, nullptr, nullptr, nullptr });
    auto glyphScale = 0.65f * emPixels / glyphSize.width();
    glyphSize = glyphScale * glyphSize;

    AffineTransform transform;
    if (style.isLeftToRightDirection())
        transform.translate(rect.maxX() - glyphSize.width() - box.style().borderEndWidth() - valueForLength(box.style().paddingEnd(), rect.width()), rect.center().y() - glyphSize.height() / 2.0f);
    else
        transform.translate(rect.x() + box.style().borderEndWidth() + valueForLength(box.style().paddingEnd(), rect.width()), rect.center().y() - glyphSize.height() / 2.0f);
    transform.scale(glyphScale);
    glyphPath.transform(transform);

    if (isEnabled(box))
        context.setFillColor(style.color());
    else
        context.setFillColor(systemColor(CSSValueAppleSystemTertiaryLabel, box.styleColorOptions()));

    context.fillPath(glyphPath);
}

void RenderThemeIOS::adjustSearchFieldDecorationPartStyle(RenderStyle& style, const Element* element) const
{
    if (!element || !element->document().settings().iOSFormControlRefreshEnabled())
        return;

    constexpr int searchFieldDecorationEmSize = 1;
    constexpr int searchFieldDecorationMargin = 4;

    CSSToLengthConversionData conversionData(style, nullptr, nullptr, nullptr);

    auto emSize = CSSPrimitiveValue::create(searchFieldDecorationEmSize, CSSUnitType::CSS_EMS);
    auto size = emSize->computeLength<float>(conversionData);

    style.setWidth({ size, LengthType::Fixed });
    style.setHeight({ size, LengthType::Fixed });
    style.setMarginEnd({ searchFieldDecorationMargin, LengthType::Fixed });
}

bool RenderThemeIOS::paintSearchFieldDecorationPart(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!box.settings().iOSFormControlRefreshEnabled())
        return RenderTheme::paintSearchFieldDecorationPart(box, paintInfo, rect);

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    const FloatSize glyphSize(73.0f, 73.0f);

    Path glyphPath;
    glyphPath.moveTo({ 29.6875f, 59.375f });
    glyphPath.addBezierCurveTo({ 35.9863f, 59.375f }, { 41.7969f, 57.422f }, { 46.6309f, 54.0528f });
    glyphPath.addLineTo({ 63.9649f, 71.3868f });
    glyphPath.addBezierCurveTo({ 64.8926f, 72.3145f }, { 66.1133f, 72.754f }, { 67.3829f, 72.754f });
    glyphPath.addBezierCurveTo({ 70.1172f, 72.754f }, { 72.1191f, 70.6544f }, { 72.1191f, 67.9688f });
    glyphPath.addBezierCurveTo({ 72.1191f, 66.6993f }, { 71.6797f, 65.4786f }, { 70.7519f, 64.5508f });
    glyphPath.addLineTo({ 53.5644f, 47.3145f });
    glyphPath.addBezierCurveTo({ 57.2266f, 42.3829f }, { 59.375f, 36.2793f }, { 59.375f, 29.6875f });
    glyphPath.addBezierCurveTo({ 59.375f, 13.3301f }, { 46.045f, 0.0f }, { 29.6875f, 0.0f });
    glyphPath.addBezierCurveTo({ 13.3301f, 0.0f }, { 0.0f, 13.3301f }, { 0.0f, 29.6875f });
    glyphPath.addBezierCurveTo({ 0.0f, 46.045f }, { 13.33f, 59.375f }, { 29.6875f, 59.375f });
    glyphPath.moveTo({ 29.6875f, 52.0997f });
    glyphPath.addBezierCurveTo({ 17.4316f, 52.0997f }, { 7.2754f, 41.9434f }, { 7.2754f, 29.6875f });
    glyphPath.addBezierCurveTo({ 7.2754f, 17.3829f }, { 17.4316f, 7.2754f }, { 29.6875f, 7.2754f });
    glyphPath.addBezierCurveTo({ 41.9922f, 7.2754f }, { 52.1f, 17.3829f }, { 52.1f, 29.6875f });
    glyphPath.addBezierCurveTo({ 52.1f, 41.9435f }, { 41.9922f, 52.0997f }, { 29.6875f, 52.0997f });

    FloatRect paintRect(rect);
    float scale = paintRect.width() / glyphSize.width();

    AffineTransform transform;
    transform.translate(paintRect.center() - (glyphSize * scale * 0.5f));
    transform.scale(scale);
    glyphPath.transform(transform);

    context.setFillColor(systemColor(CSSValueAppleSystemSecondaryLabel, box.styleColorOptions()));
    context.fillPath(glyphPath);

    return false;
}

void RenderThemeIOS::adjustSearchFieldResultsDecorationPartStyle(RenderStyle& style, const Element* element) const
{
    adjustSearchFieldDecorationPartStyle(style, element);
}

bool RenderThemeIOS::paintSearchFieldResultsDecorationPart(const RenderBox& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintSearchFieldDecorationPart(box, paintInfo, rect);
}

void RenderThemeIOS::adjustSearchFieldResultsButtonStyle(RenderStyle& style, const Element* element) const
{
    adjustSearchFieldDecorationPartStyle(style, element);
}

bool RenderThemeIOS::paintSearchFieldResultsButton(const RenderBox& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintSearchFieldDecorationPart(box, paintInfo, rect);
}

#endif // ENABLE(IOS_FORM_CONTROL_REFRESH)

} // namespace WebCore

#endif //PLATFORM(IOS_FAMILY)
