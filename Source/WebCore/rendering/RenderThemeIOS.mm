/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
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

#import "BitmapImage.h"
#import "CSSPrimitiveValue.h"
#import "CSSToLengthConversionData.h"
#import "CSSValueKey.h"
#import "CSSValueKeywords.h"
#import "ColorBlending.h"
#import "ColorIOS.h"
#import "DateComponents.h"
#import "Document.h"
#import "File.h"
#import "FloatRoundedRect.h"
#import "FontCache.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "FrameView.h"
#import "GeometryUtilities.h"
#import "Gradient.h"
#import "GraphicsContext.h"
#import "GraphicsContextCG.h"
#import "HTMLAttachmentElement.h"
#import "HTMLInputElement.h"
#import "HTMLMeterElement.h"
#import "HTMLNames.h"
#import "HTMLSelectElement.h"
#import "IOSurface.h"
#import "Icon.h"
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
#import "RuntimeEnabledFeatures.h"
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
#import <wtf/RefPtr.h>
#import <wtf/StdLibExtras.h>

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

static void contentSizeCategoryDidChange(CFNotificationCenterRef, void*, CFStringRef name, const void*, CFDictionaryRef)
{
    ASSERT_UNUSED(name, CFEqual(name, PAL::get_UIKit_UIContentSizeCategoryDidChangeNotification()));
    WebThreadRun(^{
        Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
    });
}

RenderThemeIOS::RenderThemeIOS()
{
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, contentSizeCategoryDidChange, (__bridge CFStringRef)PAL::get_UIKit_UIContentSizeCategoryDidChangeNotification(), 0, CFNotificationSuspensionBehaviorDeliverImmediately);
}

RenderTheme& RenderTheme::singleton()
{
    static NeverDestroyed<RenderThemeIOS> theme;
    return theme;
}

static String& _contentSizeCategory()
{
    static NeverDestroyed<String> _contentSizeCategory;
    return _contentSizeCategory.get();
}

CFStringRef RenderThemeIOS::contentSizeCategory() const
{
    if (!_contentSizeCategory().isNull())
        return (__bridge CFStringRef)static_cast<NSString*>(_contentSizeCategory());
    return (CFStringRef)[[PAL::getUIApplicationClass() sharedApplication] preferredContentSizeCategory];
}

void RenderThemeIOS::setContentSizeCategory(const String& contentSizeCategory)
{
    _contentSizeCategory() = contentSizeCategory;
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

void RenderThemeIOS::adjustCheckboxStyle(RenderStyle& style, const Element*) const
{
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
    CGContextSetStrokeColorWithColor(context, cachedCGColor(strokeColor));
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
    if (box.style().appearance() == CheckboxPart || box.style().appearance() == RadioPart) {
        float width = std::min(paintRect.width(), paintRect.height());
        float height = width;
        return enclosingLayoutRect(FloatRect(paintRect.x(), paintRect.y() + (box.height() - height) / 2, width, height)); // Vertically center the checkbox, like on desktop
    }

    return paintRect;
}

int RenderThemeIOS::baselinePosition(const RenderBox& box) const
{
    if (box.style().appearance() == CheckboxPart || box.style().appearance() == RadioPart)
        return box.marginTop() + box.height() - 2; // The baseline is 2px up from the bottom of the checkbox/radio in AppKit.
    if (box.style().appearance() == MenulistPart)
        return box.marginTop() + box.height() - 5; // This is to match AppKit. There might be a better way to calculate this though.
    return RenderTheme::baselinePosition(box);
}

bool RenderThemeIOS::isControlStyled(const RenderStyle& style, const RenderStyle& userAgentStyle) const
{
    // Buttons and MenulistButtons are styled if they contain a background image.
    if (style.appearance() == PushButtonPart || style.appearance() == MenulistButtonPart)
        return !style.visitedDependentColor(CSSPropertyBackgroundColor).isVisible() || style.backgroundLayers().hasImage();

    if (style.appearance() == TextFieldPart || style.appearance() == TextAreaPart)
        return style.backgroundLayers() != userAgentStyle.backgroundLayers();

    return RenderTheme::isControlStyled(style, userAgentStyle);
}

void RenderThemeIOS::adjustRadioStyle(RenderStyle& style, const Element*) const
{
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

void RenderThemeIOS::paintTextFieldDecorations(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (box.settings().iOSFormControlRefreshEnabled())
        return;
#endif

    auto& style = box.style();
    FloatPoint point(rect.x() + style.borderLeftWidth(), rect.y() + style.borderTopWidth());

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    paintInfo.context().clipRoundedRect(style.getRoundedBorderFor(LayoutRect(rect)).pixelSnappedRoundedRectForPainting(box.document().deviceScaleFactor()));

    // This gradient gets drawn black when printing.
    // Do not draw the gradient if there is no visible top border.
    bool topBorderIsInvisible = !style.hasBorder() || !style.borderTopWidth() || style.borderTopIsTransparent();
    if (!box.view().printing() && !topBorderIsInvisible)
        drawAxialGradient(paintInfo.context().platformContext(), gradientWithName(InsetGradient), point, FloatPoint(CGPointMake(point.x(), point.y() + 3.0f)), LinearInterpolation);
}

void RenderThemeIOS::paintTextAreaDecorations(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
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
        padding = emSize->computeLength<float>(CSSToLengthConversionData(&style, nullptr, nullptr, nullptr, 1.0, WTF::nullopt));
    }

    if (style.appearance() == MenulistButtonPart) {
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
    if (!canAdjustBorderRadiusForAppearance(style.appearance(), box) || style.backgroundLayers().hasImage())
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
    int pixels = emSize->computeLength<int>(CSSToLengthConversionData(&style, document.renderStyle(), nullptr, document.renderView(), document.frame() ? document.frame()->pageZoomFactor() : 1.));
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
    // Set the min-height to be at least MenuListMinHeight.
    if (style.height().isAuto())
        style.setMinHeight(Length(std::max(MenuListMinHeight, static_cast<int>(MenuListBaseHeight / MenuListBaseFontSize * style.fontDescription().computedSize())), LengthType::Fixed));
    else
        style.setMinHeight(Length(MenuListMinHeight, LengthType::Fixed));

    if (!element)
        return;

    adjustPressedStyle(style, *element);

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

    box.drawLineForBoxSide(paintInfo.context(), FloatRect(FloatPoint(separatorPosition - borderTopWidth, clip.y()), FloatPoint(separatorPosition, clip.maxY())), BoxSide::Right, style.visitedDependentColor(CSSPropertyBorderTopColor), style.borderTopStyle(), 0, 0);

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
    switch (style.appearance()) {
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
    if (style.appearance() != SliderThumbHorizontalPart && style.appearance() != SliderThumbVerticalPart)
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
    auto strokeGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1) });
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
    auto upperGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition + 0.5f), FloatPoint(rect.x(), verticalRenderingPosition + upperGradientHeight - 1.5) });
    upperGradient->addColorStop({ 0.0f, SRGBA<uint8_t> { 133, 133, 133, 188 } });
    upperGradient->addColorStop({ 1.0f, SRGBA<uint8_t> { 18, 18, 18, 51 } });
    context.setFillGradient(WTFMove(upperGradient));

    context.fillRect(FloatRect(rect.x(), verticalRenderingPosition, rect.width(), upperGradientHeight));

    const auto& renderProgress = downcast<RenderProgress>(renderer);
    if (renderProgress.isDeterminate()) {
        // 2) Draw the progress bar.
        double position = clampTo(renderProgress.position(), 0.0, 1.0);
        float barWidth = position * rect.width();
        auto barGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition + 0.5f), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1) });
        barGradient->addColorStop({ 0.0f, SRGBA<uint8_t> { 195, 217, 247 } });
        barGradient->addColorStop({ 0.45f, SRGBA<uint8_t> { 118, 164, 228 } });
        barGradient->addColorStop({ 0.49f, SRGBA<uint8_t> { 118, 164, 228 } });
        barGradient->addColorStop({ 0.51f, SRGBA<uint8_t> { 36, 114, 210 } });
        barGradient->addColorStop({ 0.55f, SRGBA<uint8_t> { 36, 114, 210 } });
        barGradient->addColorStop({ 1.0f, SRGBA<uint8_t> { 57, 142, 244 } });
        context.setFillGradient(WTFMove(barGradient));

        auto barStrokeGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1) });
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

void RenderThemeIOS::paintSearchFieldDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintTextFieldDecorations(box, paintInfo, rect);
}

// This value matches the opacity applied to UIKit controls.
constexpr auto pressedStateOpacity = 0.75f;

void RenderThemeIOS::adjustPressedStyle(RenderStyle& style, const Element& element) const
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (element.document().settings().iOSFormControlRefreshEnabled() && element.active() && !element.isDisabledFormControl()) {
        auto textColor = style.color();
        if (textColor.isValid())
            style.setColor(textColor.colorWithAlphaMultipliedBy(pressedStateOpacity));

        auto backgroundColor = style.backgroundColor();
        if (backgroundColor.isValid())
            style.setBackgroundColor(backgroundColor.colorWithAlphaMultipliedBy(pressedStateOpacity));
    }
#endif
}

void RenderThemeIOS::adjustButtonStyle(RenderStyle& style, const Element* element) const
{
    // If no size is specified, ensure the height of the button matches ControlBaseHeight scaled
    // with the font size. min-height is used rather than height to avoid clipping the contents of
    // the button in cases where the button contains more than one line of text.
    if (style.width().isIntrinsicOrAuto() || style.height().isAuto())
        style.setMinHeight(Length(ControlBaseHeight / ControlBaseFontSize * style.fontDescription().computedSize(), LengthType::Fixed));

#if ENABLE(INPUT_TYPE_COLOR)
    if (style.appearance() == ColorWellPart)
        return;
#endif

    // Set padding: 0 1.0em; on buttons.
    // CSSPrimitiveValue::computeLengthInt only needs the element's style to calculate em lengths.
    // Since the element might not be in a document, just pass nullptr for the root element style,
    // the parent element style, and the render view.
    auto emSize = CSSPrimitiveValue::create(1.0, CSSUnitType::CSS_EMS);
    int pixels = emSize->computeLength<int>(CSSToLengthConversionData(&style, nullptr, nullptr, nullptr, 1.0, WTF::nullopt));
    style.setPaddingBox(LengthBox(0, pixels, 0, pixels));

    if (!element)
        return;

    adjustPressedStyle(style, *element);

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
    auto [r, g, b, a] = backgroundColor.toSRGBALossy<float>();
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

const int kThumbnailBorderStrokeWidth = 1;
const int kThumbnailBorderCornerRadius = 1;
const int kVisibleBackgroundImageWidth = 1;
const int kMultipleThumbnailShrinkSize = 2;

void RenderThemeIOS::paintFileUploadIconDecorations(const RenderObject&, const RenderObject& buttonRenderer, const PaintInfo& paintInfo, const IntRect& rect, Icon* icon, FileUploadDecorations fileUploadDecorations)
{
    GraphicsContextStateSaver stateSaver(paintInfo.context());

    IntSize cornerSize(kThumbnailBorderCornerRadius, kThumbnailBorderCornerRadius);
    Color pictureFrameColor = buttonRenderer.style().visitedDependentColor(CSSPropertyBorderTopColor);

    IntRect thumbnailPictureFrameRect = rect;
    IntRect thumbnailRect = rect;
    thumbnailRect.contract(2 * kThumbnailBorderStrokeWidth, 2 * kThumbnailBorderStrokeWidth);
    thumbnailRect.move(kThumbnailBorderStrokeWidth, kThumbnailBorderStrokeWidth);

    if (fileUploadDecorations == MultipleFiles) {
        // Smaller thumbnails for multiple selection appearance.
        thumbnailPictureFrameRect.contract(kMultipleThumbnailShrinkSize, kMultipleThumbnailShrinkSize);
        thumbnailRect.contract(kMultipleThumbnailShrinkSize, kMultipleThumbnailShrinkSize);

        // Background picture frame and simple background icon with a gradient matching the button.
        Color backgroundImageColor = buttonRenderer.style().visitedDependentColor(CSSPropertyBackgroundColor);
        paintInfo.context().fillRoundedRect(FloatRoundedRect(thumbnailPictureFrameRect, cornerSize, cornerSize, cornerSize, cornerSize), pictureFrameColor);
        paintInfo.context().fillRect(thumbnailRect, backgroundImageColor);
        {
            GraphicsContextStateSaver stateSaver2(paintInfo.context());
            CGContextRef cgContext = paintInfo.context().platformContext();
            paintInfo.context().clip(thumbnailRect);
            if (shouldUseConvexGradient(backgroundImageColor))
                drawAxialGradient(cgContext, gradientWithName(ConvexGradient), thumbnailRect.location(), FloatPoint(thumbnailRect.x(), thumbnailRect.maxY()), LinearInterpolation);
            else {
                drawAxialGradient(cgContext, gradientWithName(ShadeGradient), thumbnailRect.location(), FloatPoint(thumbnailRect.x(), thumbnailRect.maxY()), LinearInterpolation);
                drawAxialGradient(cgContext, gradientWithName(ShineGradient), FloatPoint(thumbnailRect.x(), thumbnailRect.maxY()), thumbnailRect.location(), ExponentialInterpolation);
            }
        }

        // Move the rects for the Foreground picture frame and icon.
        int inset = kVisibleBackgroundImageWidth + kThumbnailBorderStrokeWidth;
        thumbnailPictureFrameRect.move(inset, inset);
        thumbnailRect.move(inset, inset);
    }

    // Foreground picture frame and icon.
    paintInfo.context().fillRoundedRect(FloatRoundedRect(thumbnailPictureFrameRect, cornerSize, cornerSize, cornerSize, cornerSize), pictureFrameColor);
    icon->paint(paintInfo.context(), thumbnailRect);
}

Color RenderThemeIOS::platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return Color::transparentBlack;
}

Color RenderThemeIOS::platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return Color::transparentBlack;
}

static Optional<Color>& cachedFocusRingColor()
{
    static NeverDestroyed<Optional<Color>> color;
    return color;
}

Color RenderThemeIOS::systemFocusRingColor()
{
    if (!cachedFocusRingColor().hasValue()) {
        // FIXME: Should be using -keyboardFocusIndicatorColor. For now, work around <rdar://problem/50838886>.
        cachedFocusRingColor() = colorFromUIColor([PAL::getUIColorClass() systemBlueColor]);
    }
    return *cachedFocusRingColor();
}

Color RenderThemeIOS::platformFocusRingColor(OptionSet<StyleColor::Options>) const
{
    return systemFocusRingColor();
}

#if ENABLE(APP_HIGHLIGHTS)
Color RenderThemeIOS::platformAppHighlightColor(OptionSet<StyleColor::Options>) const
{
    // FIXME: expose the real value from UIKit.
    return SRGBA<uint8_t> { 255, 238, 190 };
}
#endif

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
    switch (style.appearance()) {
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
            { CSSValueAppleSystemOpaqueSeparator, @selector(opaqueSeparatorColor) },
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

static inline Optional<Color> systemColorFromCSSValueSystemColorInformation(CSSValueSystemColorInformation systemColorInformation, bool useDarkAppearance)
{
    if (auto color = wtfObjCMsgSend<UIColor *>(PAL::getUIColorClass(), systemColorInformation.selector)) {
        Color systemColor = { color.CGColor, Color::Flags::Semantic };

        if (systemColorInformation.opacity < 1.0f)
            systemColor = systemColor.colorWithAlphaMultipliedBy(systemColorInformation.opacity);

        if (systemColorInformation.makeOpaque)
            return blendSourceOver(useDarkAppearance ? Color::black : Color::white, systemColor);

        return systemColor;
    }

    return WTF::nullopt;
}

static Optional<Color> systemColorFromCSSValueID(CSSValueID cssValueID, bool useDarkAppearance, bool useElevatedUserInterfaceLevel)
{
    LocalCurrentTraitCollection localTraitCollection(useDarkAppearance, useElevatedUserInterfaceLevel);

    for (auto& cssValueSystemColorInformation : cssValueSystemColorInformationList()) {
        if (cssValueSystemColorInformation.cssValueID == cssValueID)
            return systemColorFromCSSValueSystemColorInformation(cssValueSystemColorInformation, useDarkAppearance);
    }

    return WTF::nullopt;
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

Color RenderThemeIOS::systemColor(CSSValueID cssValueID, OptionSet<StyleColor::Options> options) const
{
    const bool forVisitedLink = options.contains(StyleColor::Options::ForVisitedLink);

    // The system color cache below can't handle visited links. The only color value
    // that cares about visited links is CSSValueWebkitLink, so handle it here by
    // calling through to RenderTheme's base implementation.
    if (forVisitedLink && cssValueID == CSSValueWebkitLink)
        return RenderTheme::systemColor(cssValueID, options);

    ASSERT(!forVisitedLink);

    auto& cache = colorCache(options);
    return cache.systemStyleColors.ensure(cssValueID, [this, cssValueID, options] () -> Color {
        const bool useDarkAppearance = options.contains(StyleColor::Options::UseDarkAppearance);
        const bool useElevatedUserInterfaceLevel = options.contains(StyleColor::Options::UseElevatedUserInterfaceLevel);
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

#if ENABLE(ATTACHMENT_ELEMENT)

const CGSize attachmentSize = { 160, 119 };

const CGFloat attachmentBorderRadius = 16;
constexpr auto attachmentBorderColor = SRGBA<uint8_t> { 204, 204, 204 };
static CGFloat attachmentBorderThickness = 1;

constexpr auto attachmentProgressColor = SRGBA<uint8_t> { 222, 222, 222 };
const CGFloat attachmentProgressBorderThickness = 3;

const CGFloat attachmentProgressSize = 36;
const CGFloat attachmentIconSize = 48;

const CGFloat attachmentItemMargin = 8;

const CGFloat attachmentWrappingTextMaximumWidth = 140;
const CFIndex attachmentWrappingTextMaximumLineCount = 2;

static RetainPtr<CTFontRef> attachmentActionFont()
{
    auto style = kCTUIFontTextStyleFootnote;
    auto size = RenderThemeIOS::singleton().contentSizeCategory();
    auto attributes = static_cast<CFDictionaryRef>(@{ (id)kCTFontTraitsAttribute: @{ (id)kCTFontSymbolicTrait: @(kCTFontTraitTightLeading | kCTFontTraitEmphasized) } });
#if HAVE(CTFONTDESCRIPTOR_CREATE_WITH_TEXT_STYLE_AND_ATTRIBUTES)
    auto emphasizedFontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyleAndAttributes(style, size, attributes));
#else
    auto fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(style, size, 0));
    auto emphasizedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(), attributes));
#endif

    return adoptCF(CTFontCreateWithFontDescriptor(emphasizedFontDescriptor.get(), 0, nullptr));
}

static UIColor *attachmentActionColor(const RenderAttachment& attachment)
{
    return [PAL::getUIColorClass() colorWithCGColor:cachedCGColor(attachment.style().visitedDependentColor(CSSPropertyColor))];
}

static RetainPtr<CTFontRef> attachmentTitleFont()
{
    auto fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(kCTUIFontTextStyleShortCaption1, RenderThemeIOS::singleton().contentSizeCategory(), 0));
    return adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), 0, nullptr));
}

static CGFloat shortCaptionPointSizeWithContentSizeCategory(CFStringRef contentSizeCategory)
{
    auto descriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(kCTUIFontTextStyleShortCaption1, contentSizeCategory, 0));
    auto pointSize = adoptCF(CTFontDescriptorCopyAttribute(descriptor.get(), kCTFontSizeAttribute));
    return [dynamic_objc_cast<NSNumber>((__bridge id)pointSize.get()) floatValue];
}

static CGFloat attachmentDynamicTypeScaleFactor()
{
    CGFloat fixedPointSize = shortCaptionPointSizeWithContentSizeCategory(kCTFontContentSizeCategoryL);
    CGFloat dynamicPointSize = shortCaptionPointSizeWithContentSizeCategory(RenderThemeIOS::singleton().contentSizeCategory());
    if (!dynamicPointSize || !fixedPointSize)
        return 1;
    return std::max<CGFloat>(1, dynamicPointSize / fixedPointSize);
}

static UIColor *attachmentTitleColor() { return [PAL::getUIColorClass() systemGrayColor]; }

static RetainPtr<CTFontRef> attachmentSubtitleFont() { return attachmentTitleFont(); }
static UIColor *attachmentSubtitleColor() { return [PAL::getUIColorClass() systemGrayColor]; }

struct RenderAttachmentInfo {
    explicit RenderAttachmentInfo(const RenderAttachment&);

    FloatRect iconRect;
    FloatRect attachmentRect;
    FloatRect progressRect;

    BOOL hasProgress { NO };
    float progress;

    RetainPtr<UIImage> icon;
    RefPtr<Image> thumbnailIcon;

    int baseline { 0 };

    struct LabelLine {
        FloatRect rect;
        RetainPtr<CTLineRef> line;
    };
    Vector<LabelLine> lines;

    CGFloat contentYOrigin { 0 };

private:
    void buildWrappedLines(const String&, CTFontRef, UIColor *, unsigned maximumLineCount);
    void buildSingleLine(const String&, CTFontRef, UIColor *);

    void addLine(CTLineRef);
};

void RenderAttachmentInfo::addLine(CTLineRef line)
{
    CGRect lineBounds = CTLineGetBoundsWithOptions(line, kCTLineBoundsExcludeTypographicLeading);
    CGFloat trailingWhitespaceWidth = CTLineGetTrailingWhitespaceWidth(line);
    CGFloat lineWidthIgnoringTrailingWhitespace = lineBounds.size.width - trailingWhitespaceWidth;
    CGFloat lineHeight = CGCeiling(lineBounds.size.height + lineBounds.origin.y);

    CGFloat xOffset = (attachmentRect.width() / 2) - (lineWidthIgnoringTrailingWhitespace / 2);
    LabelLine labelLine;
    labelLine.line = line;
    labelLine.rect = FloatRect(xOffset, 0, lineWidthIgnoringTrailingWhitespace, lineHeight);

    lines.append(labelLine);
}

void RenderAttachmentInfo::buildWrappedLines(const String& text, CTFontRef font, UIColor *color, unsigned maximumLineCount)
{
    if (text.isEmpty())
        return;

    NSDictionary *textAttributes = @{
        (id)kCTFontAttributeName: (id)font,
        (id)kCTForegroundColorAttributeName: color
    };
    RetainPtr<NSAttributedString> attributedText = adoptNS([[NSAttributedString alloc] initWithString:text attributes:textAttributes]);
    RetainPtr<CTFramesetterRef> framesetter = adoptCF(CTFramesetterCreateWithAttributedString((CFAttributedStringRef)attributedText.get()));

    CFRange fitRange;
    CGFloat wrappingWidth = attachmentWrappingTextMaximumWidth * attachmentDynamicTypeScaleFactor();
    CGSize textSize = CTFramesetterSuggestFrameSizeWithConstraints(framesetter.get(), CFRangeMake(0, 0), nullptr, CGSizeMake(wrappingWidth, CGFLOAT_MAX), &fitRange);

    RetainPtr<CGPathRef> textPath = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, textSize.width, textSize.height), nullptr));
    RetainPtr<CTFrameRef> textFrame = adoptCF(CTFramesetterCreateFrame(framesetter.get(), fitRange, textPath.get(), nullptr));

    CFArrayRef ctLines = CTFrameGetLines(textFrame.get());
    CFIndex lineCount = CFArrayGetCount(ctLines);
    if (!lineCount)
        return;

    // Lay out and record the first (maximumLineCount - 1) lines.
    CFIndex lineIndex = 0;
    CFIndex nonTruncatedLineCount = std::min<CFIndex>(maximumLineCount - 1, lineCount);
    for (; lineIndex < nonTruncatedLineCount; ++lineIndex)
        addLine((CTLineRef)CFArrayGetValueAtIndex(ctLines, lineIndex));

    if (lineIndex == lineCount)
        return;

    // We had text that didn't fit in the first (maximumLineCount - 1) lines.
    // Combine it into one last line, and center-truncate it.
    CTLineRef firstRemainingLine = (CTLineRef)CFArrayGetValueAtIndex(ctLines, lineIndex);
    CFIndex remainingRangeStart = CTLineGetStringRange(firstRemainingLine).location;
    CFRange remainingRange = CFRangeMake(remainingRangeStart, [attributedText length] - remainingRangeStart);
    RetainPtr<CGPathRef> remainingPath = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, CGFLOAT_MAX, CGFLOAT_MAX), nullptr));
    RetainPtr<CTFrameRef> remainingFrame = adoptCF(CTFramesetterCreateFrame(framesetter.get(), remainingRange, remainingPath.get(), nullptr));
    RetainPtr<NSAttributedString> ellipsisString = adoptNS([[NSAttributedString alloc] initWithString:@"\u2026" attributes:textAttributes]);
    RetainPtr<CTLineRef> ellipsisLine = adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)ellipsisString.get()));
    CTLineRef remainingLine = (CTLineRef)CFArrayGetValueAtIndex(CTFrameGetLines(remainingFrame.get()), 0);
    RetainPtr<CTLineRef> truncatedLine = adoptCF(CTLineCreateTruncatedLine(remainingLine, wrappingWidth, kCTLineTruncationMiddle, ellipsisLine.get()));

    if (!truncatedLine)
        truncatedLine = remainingLine;

    addLine(truncatedLine.get());
}

void RenderAttachmentInfo::buildSingleLine(const String& text, CTFontRef font, UIColor *color)
{
    if (text.isEmpty())
        return;

    NSDictionary *textAttributes = @{
        (id)kCTFontAttributeName: (id)font,
        (id)kCTForegroundColorAttributeName: color
    };
    RetainPtr<NSAttributedString> attributedText = adoptNS([[NSAttributedString alloc] initWithString:text attributes:textAttributes]);

    addLine(adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)attributedText.get())).get());
}

static BOOL getAttachmentProgress(const RenderAttachment& attachment, float& progress)
{
    auto& progressString = attachment.attachmentElement().attributeWithoutSynchronization(progressAttr);
    if (progressString.isEmpty())
        return NO;
    bool validProgress;
    progress = std::max<float>(std::min<float>(progressString.toFloat(&validProgress), 1), 0);
    return validProgress;
}

static RetainPtr<UIImage> iconForAttachment(const RenderAttachment& attachment, FloatSize& size)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto documentInteractionController = adoptNS([PAL::allocUIDocumentInteractionControllerInstance() init]);
    ALLOW_DEPRECATED_DECLARATIONS_END

    String fileName;
    if (File* file = attachment.attachmentElement().file())
        fileName = file->name();

    if (fileName.isEmpty())
        fileName = attachment.attachmentElement().attachmentTitle();
    [documentInteractionController setName:fileName];

    String attachmentType = attachment.attachmentElement().attachmentType();
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
        return nil;

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
    size = largestRectWithAspectRatioInsideRect(iconAspect, FloatRect(0, 0, attachmentIconSize, attachmentIconSize)).size();

    return result;
}

RenderAttachmentInfo::RenderAttachmentInfo(const RenderAttachment& attachment)
{
    attachmentRect = FloatRect(0, 0, attachment.width().toFloat(), attachment.height().toFloat());

    hasProgress = getAttachmentProgress(attachment, progress);

    String title = attachment.attachmentElement().attachmentTitleForDisplay();
    String action = attachment.attachmentElement().attributeWithoutSynchronization(actionAttr);
    String subtitle = attachment.attachmentElement().attributeWithoutSynchronization(subtitleAttr);

    CGFloat yOffset = 0;

    if (hasProgress) {
        progressRect = FloatRect((attachmentRect.width() / 2) - (attachmentProgressSize / 2), 0, attachmentProgressSize, attachmentProgressSize);
        yOffset += attachmentProgressSize + attachmentItemMargin;
    }

    if (action.isEmpty() && !hasProgress) {
        FloatSize iconSize;
        icon = iconForAttachment(attachment, iconSize);
        thumbnailIcon = attachment.attachmentElement().thumbnail();
        if (thumbnailIcon)
            iconSize = largestRectWithAspectRatioInsideRect(thumbnailIcon->size().aspectRatio(), FloatRect(0, 0, attachmentIconSize, attachmentIconSize)).size();
        
        if (thumbnailIcon || icon) {
            iconRect = FloatRect(FloatPoint((attachmentRect.width() / 2) - (iconSize.width() / 2), 0), iconSize);
            yOffset += iconRect.height() + attachmentItemMargin;
        }
    } else
        buildWrappedLines(action, attachmentActionFont().get(), attachmentActionColor(attachment), attachmentWrappingTextMaximumLineCount);

    bool forceSingleLineTitle = !action.isEmpty() || !subtitle.isEmpty() || hasProgress;
    buildWrappedLines(title, attachmentTitleFont().get(), attachmentTitleColor(), forceSingleLineTitle ? 1 : attachmentWrappingTextMaximumLineCount);
    buildSingleLine(subtitle, attachmentSubtitleFont().get(), attachmentSubtitleColor());

    if (!lines.isEmpty()) {
        for (auto& line : lines) {
            line.rect.setY(yOffset);
            yOffset += line.rect.height() + attachmentItemMargin;
        }
    }

    yOffset -= attachmentItemMargin;

    contentYOrigin = (attachmentRect.height() / 2) - (yOffset / 2);
}

LayoutSize RenderThemeIOS::attachmentIntrinsicSize(const RenderAttachment&) const
{
    return LayoutSize(FloatSize(attachmentSize) * attachmentDynamicTypeScaleFactor());
}

int RenderThemeIOS::attachmentBaseline(const RenderAttachment& attachment) const
{
    RenderAttachmentInfo info(attachment);
    return info.baseline;
}

static void paintAttachmentIcon(GraphicsContext& context, RenderAttachmentInfo& info)
{
    RefPtr<Image> iconImage;
    if (info.thumbnailIcon)
        iconImage = info.thumbnailIcon;
    else if (info.icon)
        iconImage = BitmapImage::create([info.icon CGImage]);
    
    context.drawImage(*iconImage, info.iconRect);
}

static void paintAttachmentText(GraphicsContext& context, RenderAttachmentInfo& info)
{
    for (const auto& line : info.lines) {
        GraphicsContextStateSaver saver(context);

        context.translate(toFloatSize(line.rect.minXMaxYCorner()));
        context.scale(FloatSize(1, -1));

        CGContextSetTextPosition(context.platformContext(), 0, 0);
        CTLineDraw(line.line.get(), context.platformContext());
    }
}

static void paintAttachmentProgress(GraphicsContext& context, RenderAttachmentInfo& info)
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

static Path attachmentBorderPath(RenderAttachmentInfo& info)
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

    RenderAttachmentInfo info(attachment);

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

    paintAttachmentText(context, info);

    return true;
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if PLATFORM(WATCHOS)

String RenderThemeIOS::extraDefaultStyleSheet()
{
    return "* { -webkit-text-size-adjust: auto; -webkit-hyphens: auto !important; }"_s;
}

#endif

#if USE(SYSTEM_PREVIEW)
static NSBundle *arKitBundle()
{
    static NSBundle *arKitBundle = []() {
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        dlopen("/System/Library/PrivateFrameworks/AssetViewer.framework/AssetViewer", RTLD_NOW);
        return [NSBundle bundleForClass:NSClassFromString(@"ASVThumbnailView")];
#else
        return [NSBundle bundleWithURL:[NSURL fileURLWithPath:@"/System/Library/PrivateFrameworks/AssetViewer.framework"]];
#endif
    }();

    return arKitBundle;
}

static RetainPtr<CGPDFPageRef> loadARKitPDFPage(NSString *imageName)
{
    NSURL *url = [arKitBundle() URLForResource:imageName withExtension:@"pdf"];

    if (!url)
        return nullptr;

    auto document = adoptCF(CGPDFDocumentCreateWithURL((CFURLRef)url));
    if (!document)
        return nullptr;

    if (!CGPDFDocumentGetNumberOfPages(document.get()))
        return nullptr;

    return CGPDFDocumentGetPage(document.get(), 1);
}

static CGPDFPageRef systemPreviewLogo()
{
    static CGPDFPageRef logoPage = loadARKitPDFPage(@"ARKitBadge").leakRef();
    return logoPage;
}

void RenderThemeIOS::paintSystemPreviewBadge(Image& image, const PaintInfo& paintInfo, const FloatRect& rect)
{
    static const int largeBadgeDimension = 70;
    static const int largeBadgeOffset = 20;

    static const int smallBadgeDimension = 35;
    static const int smallBadgeOffset = 8;

    static const int minimumSizeForLargeBadge = 240;

    bool useSmallBadge = rect.width() < minimumSizeForLargeBadge || rect.height() < minimumSizeForLargeBadge;
    int badgeOffset = useSmallBadge ? smallBadgeOffset : largeBadgeOffset;
    int badgeDimension = useSmallBadge ? smallBadgeDimension : largeBadgeDimension;

    int minimumDimension = badgeDimension + 2 * badgeOffset;
    if (rect.width() < minimumDimension || rect.height() < minimumDimension)
        return;

    CGRect absoluteBadgeRect = CGRectMake(rect.x() + rect.width() - badgeDimension - badgeOffset, rect.y() + badgeOffset, badgeDimension, badgeDimension);
    CGRect insetBadgeRect = CGRectMake(rect.width() - badgeDimension - badgeOffset, badgeOffset, badgeDimension, badgeDimension);
    CGRect badgeRect = CGRectMake(0, 0, badgeDimension, badgeDimension);

    CIImage *inputImage = [CIImage imageWithCGImage:image.nativeImage()->platformImage().get()];

    // Create a circle to be used for the clipping path in the badge, as well as the drop shadow.
    RetainPtr<CGPathRef> circle = adoptCF(CGPathCreateWithRoundedRect(absoluteBadgeRect, badgeDimension / 2, badgeDimension / 2, nullptr));

    auto& graphicsContext = paintInfo.context();
    if (graphicsContext.paintingDisabled())
        return;

    GraphicsContextStateSaver stateSaver(graphicsContext);

    CGContextRef ctx = graphicsContext.platformContext();
    if (!ctx)
        return;

    CGContextSaveGState(ctx);

    // Draw a drop shadow around the circle.
    // Use the GraphicsContext function, because it calculates the blur radius in context space,
    // rather than screen space.
    constexpr auto shadowColor = Color::black.colorWithAlphaByte(26);
    graphicsContext.setShadow(FloatSize { }, 16, shadowColor);

    // The circle must have an alpha channel value of 1 for the shadow color to appear.
    CGFloat circleColorComponents[4] = { 0, 0, 0, 1 };
    RetainPtr<CGColorRef> circleColor = adoptCF(CGColorCreate(sRGBColorSpaceRef(), circleColorComponents));
    CGContextSetFillColorWithColor(ctx, circleColor.get());

    // Clip out the circle to only show the shadow.
    CGContextBeginPath(ctx);
    CGContextAddRect(ctx, rect);
    CGContextAddPath(ctx, circle.get());
    CGContextClosePath(ctx);
    CGContextEOClip(ctx);

    // Draw a slightly smaller circle with a shadow, otherwise we'll see a fringe of the solid
    // black circle around the edges of the clipped path below.
    CGContextBeginPath(ctx);
    CGRect slightlySmallerAbsoluteBadgeRect = CGRectMake(absoluteBadgeRect.origin.x + 0.5, absoluteBadgeRect.origin.y + 0.5, badgeDimension - 1, badgeDimension - 1);
    RetainPtr<CGPathRef> slightlySmallerCircle = adoptCF(CGPathCreateWithRoundedRect(slightlySmallerAbsoluteBadgeRect, slightlySmallerAbsoluteBadgeRect.size.width / 2, slightlySmallerAbsoluteBadgeRect.size.height / 2, nullptr));
    CGContextAddPath(ctx, slightlySmallerCircle.get());
    CGContextClosePath(ctx);
    CGContextFillPath(ctx);

    CGContextRestoreGState(ctx);

    // Draw the blurred backdrop. Scale from intrinsic size to render size.
    CGAffineTransform transform = CGAffineTransformIdentity;
    transform = CGAffineTransformScale(transform, rect.width() / image.width(), rect.height() / image.height());
    CIImage *scaledImage = [inputImage imageByApplyingTransform:transform];

    // CoreImage coordinates are y-up, so we need to flip the badge rectangle within the image frame.
    CGRect flippedInsetBadgeRect = CGRectMake(insetBadgeRect.origin.x, rect.height() - insetBadgeRect.origin.y - insetBadgeRect.size.height, badgeDimension, badgeDimension);

    // Create a cropped region with pixel values extending outwards.
    CIImage *clampedImage = [scaledImage imageByClampingToRect:flippedInsetBadgeRect];

    // Blur.
    CIImage *blurredImage = [clampedImage imageByApplyingGaussianBlurWithSigma:10];

    // Saturate.
    CIFilter *saturationFilter = [CIFilter filterWithName:@"CIColorControls"];
    [saturationFilter setValue:blurredImage forKey:kCIInputImageKey];
    [saturationFilter setValue:@1.8 forKey:kCIInputSaturationKey];

    // Tint.
    CIFilter *tintFilter1 = [CIFilter filterWithName:@"CIConstantColorGenerator"];
    CIColor *tintColor1 = [CIColor colorWithRed:1 green:1 blue:1 alpha:0.18];
    [tintFilter1 setValue:tintColor1 forKey:kCIInputColorKey];

    // Blend the tint with the saturated output.
    CIFilter *sourceOverFilter = [CIFilter filterWithName:@"CISourceOverCompositing"];
    [sourceOverFilter setValue:tintFilter1.outputImage forKey:kCIInputImageKey];
    [sourceOverFilter setValue:saturationFilter.outputImage forKey:kCIInputBackgroundImageKey];

    if (!m_ciContext)
        m_ciContext = [CIContext context];

    RetainPtr<CGImageRef> cgImage;
#if HAVE(IOSURFACE_COREIMAGE_SUPPORT)
    // Crop the result to the badge location.
    CIImage *croppedImage = [sourceOverFilter.outputImage imageByCroppingToRect:flippedInsetBadgeRect];
    CIImage *translatedImage = [croppedImage imageByApplyingTransform:CGAffineTransformMakeTranslation(-flippedInsetBadgeRect.origin.x, -flippedInsetBadgeRect.origin.y)];
    IOSurfaceRef surface;
    if (useSmallBadge) {
        if (!m_smallBadgeSurface)
            m_smallBadgeSurface = IOSurface::create({ smallBadgeDimension, smallBadgeDimension }, sRGBColorSpaceRef());
        surface = m_smallBadgeSurface->surface();
    } else {
        if (!m_largeBadgeSurface)
            m_largeBadgeSurface = IOSurface::create({ largeBadgeDimension, largeBadgeDimension }, sRGBColorSpaceRef());
        surface = m_largeBadgeSurface->surface();
    }
    [m_ciContext.get() render:translatedImage toIOSurface:surface bounds:badgeRect colorSpace:sRGBColorSpaceRef()];
    cgImage = useSmallBadge ? m_smallBadgeSurface->createImage() : m_largeBadgeSurface->createImage();
#else
    cgImage = adoptCF([m_ciContext.get() createCGImage:sourceOverFilter.outputImage fromRect:flippedInsetBadgeRect]);
#endif

    // Before we render the result, we should clip to a circle around the badge rectangle.
    CGContextSaveGState(ctx);
    CGContextBeginPath(ctx);
    CGContextAddPath(ctx, circle.get());
    CGContextClosePath(ctx);
    CGContextClip(ctx);

    CGContextTranslateCTM(ctx, absoluteBadgeRect.origin.x, absoluteBadgeRect.origin.y);
    CGContextTranslateCTM(ctx, 0, badgeDimension);
    CGContextScaleCTM(ctx, 1, -1);
    CGContextDrawImage(ctx, badgeRect, cgImage.get());

    if (auto logo = systemPreviewLogo()) {
        CGSize pdfSize = CGPDFPageGetBoxRect(logo, kCGPDFMediaBox).size;
        CGFloat scaleX = badgeDimension / pdfSize.width;
        CGFloat scaleY = badgeDimension / pdfSize.height;
        CGContextScaleCTM(ctx, scaleX, scaleY);
        CGContextDrawPDFPage(ctx, logo);
    }

    CGContextFlush(ctx);
    CGContextRestoreGState(ctx);
}
#endif

#if ENABLE(IOS_FORM_CONTROL_REFRESH)

constexpr auto nativeControlBorderWidth = 1.0f;

Color RenderThemeIOS::checkboxRadioBackgroundColor(OptionSet<ControlStates::States> states, OptionSet<StyleColor::Options> styleColorOptions)
{
    if (!states.contains(ControlStates::States::Enabled))
        return systemColor(CSSValueAppleSystemOpaqueSecondaryFillDisabled, styleColorOptions);

    Color enabledBackgroundColor;
    if (states.containsAny({ ControlStates::States::Checked, ControlStates::States::Indeterminate }))
        enabledBackgroundColor = systemColor(CSSValueAppleSystemBlue, styleColorOptions);
    else
        enabledBackgroundColor = systemColor(CSSValueAppleSystemOpaqueSecondaryFill, styleColorOptions);

    if (states.contains(ControlStates::States::Pressed))
        return enabledBackgroundColor.colorWithAlphaMultipliedBy(pressedStateOpacity);

    return enabledBackgroundColor;
}

Color RenderThemeIOS::checkboxRadioIndicatorColor(OptionSet<ControlStates::States> states, OptionSet<StyleColor::Options> styleColorOptions)
{
    if (!states.contains(ControlStates::States::Enabled))
        return systemColor(CSSValueAppleSystemTertiaryLabel, styleColorOptions);

    Color enabledIndicatorColor = systemColor(CSSValueAppleSystemLabel, styleColorOptions | StyleColor::Options::UseDarkAppearance);
    if (states.contains(ControlStates::States::Pressed))
        return enabledIndicatorColor.colorWithAlphaMultipliedBy(pressedStateOpacity);

    return enabledIndicatorColor;
}

bool RenderThemeIOS::paintCheckbox(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!box.settings().iOSFormControlRefreshEnabled())
        return true;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver { context };

    constexpr auto checkboxHeight = 16.0f;
    constexpr auto checkboxCornerRadius = 5.0f;

    FloatRoundedRect checkboxRect(rect, FloatRoundedRect::Radii(checkboxCornerRadius * rect.height() / checkboxHeight));

    auto controlStates = extractControlStatesForRenderer(box);
    auto styleColorOptions = box.styleColorOptions();

    context.fillRoundedRect(checkboxRect, systemColor(CSSValueWebkitControlBackground, styleColorOptions));

    FloatRoundedRect checkboxInnerRoundedRect(checkboxRect);
    checkboxInnerRoundedRect.inflateWithRadii(-nativeControlBorderWidth);

    context.fillRoundedRect(checkboxInnerRoundedRect, checkboxRadioBackgroundColor(controlStates, styleColorOptions));

    FloatRect checkboxInnerRect(checkboxInnerRoundedRect.rect());

    bool checked = controlStates.contains(ControlStates::States::Checked);
    bool indeterminate = controlStates.contains(ControlStates::States::Indeterminate);

    if (!checked && !indeterminate)
        return false;

    Path path;
    if (checked) {
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
        float scale = (0.65f * checkboxInnerRect.width()) / checkmarkSize.width();

        AffineTransform transform;
        transform.translate(checkboxInnerRect.center() - (checkmarkSize * scale * 0.5f));
        transform.scale(scale);
        path.transform(transform);
    } else {
        const FloatSize indeterminateBarRoundingRadii(1.25f, 1.25f);
        constexpr float indeterminateBarPadding = 2.5f;
        float height = 0.12f * checkboxInnerRect.height();

        FloatRect indeterminateBarRect(checkboxInnerRect.x() + indeterminateBarPadding, checkboxInnerRect.center().y() - height / 2.0f, checkboxInnerRect.width() - indeterminateBarPadding * 2, height);
        path.addRoundedRect(indeterminateBarRect, indeterminateBarRoundingRadii);
    }

    context.setFillColor(checkboxRadioIndicatorColor(controlStates, styleColorOptions));
    context.fillPath(path);

    return false;
}

bool RenderThemeIOS::paintRadio(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!box.settings().iOSFormControlRefreshEnabled())
        return true;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto controlStates = extractControlStatesForRenderer(box);
    auto styleColorOptions = box.styleColorOptions();

    context.setFillColor(systemColor(CSSValueWebkitControlBackground, styleColorOptions));
    context.fillEllipse(rect);

    FloatRect radioRect(rect);
    radioRect.inflate(-nativeControlBorderWidth);

    context.setFillColor(checkboxRadioBackgroundColor(controlStates, styleColorOptions));
    context.fillEllipse(radioRect);

    if (controlStates.contains(ControlStates::States::Checked)) {
        // The inner circle is 6 / 14 the size of the surrounding circle,
        // leaving 8 / 14 around it. (8 / 14) / 2 = 2 / 7.
        constexpr float innerInverseRatio = 2 / 7.0f;

        FloatRect innerCircleRect(radioRect);
        innerCircleRect.inflateX(-innerCircleRect.width() * innerInverseRatio);
        innerCircleRect.inflateY(-innerCircleRect.height() * innerInverseRatio);

        context.setFillColor(checkboxRadioIndicatorColor(controlStates, styleColorOptions));
        context.fillEllipse(innerCircleRect);
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
    context.fillRoundedRect(FloatRoundedRect(barRect, barCornerRadii), systemColor(CSSValueAppleSystemBlue, styleColorOptions).colorWithAlphaMultipliedBy(alpha));

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
    auto element = makeRefPtr(renderMeter.meterElement());

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

    bool isHorizontal = box.style().appearance() == SliderHorizontalPart;
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
            context.fillRoundedRect(roundedTickRect, systemColor((value >= *optionValue) ? CSSValueAppleSystemBlue : CSSValueAppleSystemOpaqueSeparator, styleColorOptions));
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

    switch (box.style().appearance()) {
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
    context.fillRoundedRect(fillRect, systemColor(CSSValueAppleSystemBlue, styleColorOptions));

    return false;
}

#if ENABLE(INPUT_TYPE_COLOR)

String RenderThemeIOS::colorInputStyleSheet(const Settings& settings) const
{
    if (!settings.iOSFormControlRefreshEnabled())
        return RenderTheme::colorInputStyleSheet(settings);

    return "input[type=\"color\"] { -webkit-appearance: color-well; width: 28px; height: 28px; outline: none; border: initial; border-radius: 50%; } "_s;
}

void RenderThemeIOS::adjustColorWellStyle(RenderStyle& style, const Element* element) const
{
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

    auto gradient = Gradient::create(Gradient::ConicData { rect.center(), 0 });
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
    auto emPixels = emSize->computeLength<float>(CSSToLengthConversionData(&style, nullptr, nullptr, nullptr, 1.0, WTF::nullopt));
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

    CSSToLengthConversionData conversionData(&style, nullptr, nullptr, nullptr, 1.0, WTF::nullopt);

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
