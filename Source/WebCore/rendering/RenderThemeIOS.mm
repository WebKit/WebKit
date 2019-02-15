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
#import "CSSValueKeywords.h"
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
#import "HTMLNames.h"
#import "HTMLSelectElement.h"
#import "IOSurface.h"
#import "Icon.h"
#import "LocalizedDateCache.h"
#import "NodeRenderStyle.h"
#import "Page.h"
#import "PaintInfo.h"
#import "PathUtilities.h"
#import "PlatformLocale.h"
#import "RenderAttachment.h"
#import "RenderObject.h"
#import "RenderProgress.h"
#import "RenderStyle.h"
#import "RenderView.h"
#import "RuntimeEnabledFeatures.h"
#import "UTIUtilities.h"
#import "UserAgentScripts.h"
#import "UserAgentStyleSheets.h"
#import "WebCoreThreadRun.h"
#import <CoreGraphics/CoreGraphics.h>
#import <CoreImage/CoreImage.h>
#import <objc/runtime.h>
#import <pal/ios/UIKitSoftLink.h>
#import <pal/spi/cocoa/CoreTextSPI.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RefPtr.h>
#import <wtf/StdLibExtras.h>

@interface WebCoreRenderThemeBundle : NSObject
@end

@implementation WebCoreRenderThemeBundle
@end

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
    CGFunctionRef function = nullptr;

    static HashMap<IOSGradientRef, CGFunctionRef>* linearFunctionRefs;
    static HashMap<IOSGradientRef, CGFunctionRef>* exponentialFunctionRefs;

    if (interpolation == LinearInterpolation) {
        if (!linearFunctionRefs)
            linearFunctionRefs = new HashMap<IOSGradientRef, CGFunctionRef>;
        else
            function = linearFunctionRefs->get(gradient);
    
        if (!function) {
            static struct CGFunctionCallbacks linearFunctionCallbacks =  { 0, interpolateLinearGradient, 0 };
            linearFunctionRefs->set(gradient, function = CGFunctionCreate(gradient, 1, nullptr, 4, nullptr, &linearFunctionCallbacks));
        }

        return function;
    }

    if (!exponentialFunctionRefs)
        exponentialFunctionRefs = new HashMap<IOSGradientRef, CGFunctionRef>;
    else
        function = exponentialFunctionRefs->get(gradient);

    if (!function) {
        static struct CGFunctionCallbacks exponentialFunctionCallbacks =  { 0, interpolateExponentialGradient, 0 };
        exponentialFunctionRefs->set(gradient, function = CGFunctionCreate(gradient, 1, 0, 4, 0, &exponentialFunctionCallbacks));
    }

    return function;
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

CFStringRef RenderThemeIOS::contentSizeCategory()
{
    if (!_contentSizeCategory().isNull())
        return (__bridge CFStringRef)static_cast<NSString*>(_contentSizeCategory());
    return (CFStringRef)[[PAL::getUIApplicationClass() sharedApplication] preferredContentSizeCategory];
}

void RenderThemeIOS::setContentSizeCategory(const String& contentSizeCategory)
{
    _contentSizeCategory() = contentSizeCategory;
}

const Color& RenderThemeIOS::shadowColor() const
{
    static NeverDestroyed<Color> color(0.0f, 0.0f, 0.0f, 0.7f);
    return color;
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
        ASSERT(style.visitedDependentColor(CSSPropertyBorderTopColor).alpha() % 255 == 0);
        ASSERT(style.visitedDependentColor(CSSPropertyBorderRightColor).alpha() % 255 == 0);
        ASSERT(style.visitedDependentColor(CSSPropertyBorderBottomColor).alpha() % 255 == 0);
        ASSERT(style.visitedDependentColor(CSSPropertyBorderLeftColor).alpha() % 255 == 0);
    }

    return border.rect();
}

void RenderThemeIOS::adjustCheckboxStyle(StyleResolver&, RenderStyle& style, const Element*) const
{
    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    int size = std::max(style.computedFontPixelSize(), 10U);
    style.setWidth({ size, Fixed });
    style.setHeight({ size, Fixed });
}

static CGPoint shortened(CGPoint start, CGPoint end, float width)
{
    float x = end.x - start.x;
    float y = end.y - start.y;
    float ratio = (!x && !y) ? 0 : width / sqrtf(x * x + y * y);
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

bool RenderThemeIOS::paintCheckboxDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    GraphicsContextStateSaver stateSaver(paintInfo.context());
    FloatRect clip = addRoundedBorderClip(box, paintInfo.context(), rect);

    float width = clip.width();
    float height = clip.height();

    bool checked = isChecked(box);
    bool indeterminate = isIndeterminate(box);

    CGContextRef cgContext = paintInfo.context().platformContext();
    if (!checked && !indeterminate) {
        FloatPoint bottomCenter(clip.x() + clip.width() / 2.0f, clip.maxY());
        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
        drawRadialGradient(cgContext, gradientWithName(ShineGradient), bottomCenter, 0, bottomCenter, sqrtf((width * width) / 4.0f + height * height), ExponentialInterpolation);
        return false;
    }

    drawAxialGradient(cgContext, gradientWithName(ConcaveGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);

    static const float thicknessRatio = 2 / 14.0;
    static const CGSize size = { 14.0f, 14.0f };
    float lineWidth = std::min(width, height) * 2.0f * thicknessRatio;

    Vector<CGPoint> line;
    Vector<CGPoint> shadow;

    if (checked) {
        static const CGPoint pathRatios[3] = {
            { 2.5f / size.width, 7.5f / size.height },
            { 5.5f / size.width, 10.5f / size.height },
            { 11.5f / size.width, 2.5f / size.height }
        };

        line = {
            CGPointMake(clip.x() + width * pathRatios[0].x, clip.y() + height * pathRatios[0].y),
            CGPointMake(clip.x() + width * pathRatios[1].x, clip.y() + height * pathRatios[1].y),
            CGPointMake(clip.x() + width * pathRatios[2].x, clip.y() + height * pathRatios[2].y)
        };

        shadow = {
            shortened(line[0], line[1], lineWidth / 4.0f),
            line[1],
            shortened(line[2], line[1], lineWidth / 4.0f)
        };
    } else if (indeterminate) {
        line = {
            CGPointMake(clip.x() + 3.5, clip.center().y()),
            CGPointMake(clip.maxX() - 3.5, clip.center().y())
        };

        shadow = {
            shortened(line[0], line[1], lineWidth / 4.0f),
            shortened(line[1], line[0], lineWidth / 4.0f)
        };
    }

    lineWidth = std::max<float>(lineWidth, 1);
    drawJoinedLines(cgContext, shadow, kCGLineCapSquare, lineWidth, Color(0.0f, 0.0f, 0.0f, 0.7f));

    lineWidth = std::max<float>(std::min(clip.width(), clip.height()) * thicknessRatio, 1);
    drawJoinedLines(cgContext, line, kCGLineCapButt, lineWidth, Color(1.0f, 1.0f, 1.0f, 240 / 255.0f));

    return false;
}

int RenderThemeIOS::baselinePosition(const RenderBox& box) const
{
    if (box.style().appearance() == CheckboxPart || box.style().appearance() == RadioPart)
        return box.marginTop() + box.height() - 2; // The baseline is 2px up from the bottom of the checkbox/radio in AppKit.
    if (box.style().appearance() == MenulistPart)
        return box.marginTop() + box.height() - 5; // This is to match AppKit. There might be a better way to calculate this though.
    return RenderTheme::baselinePosition(box);
}

bool RenderThemeIOS::isControlStyled(const RenderStyle& style, const BorderData& border, const FillLayer& background, const Color& backgroundColor) const
{
    // Buttons and MenulistButtons are styled if they contain a background image.
    if (style.appearance() == PushButtonPart || style.appearance() == MenulistButtonPart)
        return !style.visitedDependentColor(CSSPropertyBackgroundColor).isVisible() || style.backgroundLayers().hasImage();

    if (style.appearance() == TextFieldPart || style.appearance() == TextAreaPart)
        return style.backgroundLayers() != background;

    return RenderTheme::isControlStyled(style, border, background, backgroundColor);
}

void RenderThemeIOS::adjustRadioStyle(StyleResolver&, RenderStyle& style, const Element*) const
{
    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    int size = std::max(style.computedFontPixelSize(), 10U);
    style.setWidth({ size, Fixed });
    style.setHeight({ size, Fixed });
    style.setBorderRadius({ size / 2, size / 2 });
}

bool RenderThemeIOS::paintRadioDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    GraphicsContextStateSaver stateSaver(paintInfo.context());
    FloatRect clip = addRoundedBorderClip(box, paintInfo.context(), rect);

    CGContextRef cgContext = paintInfo.context().platformContext();
    if (isChecked(box)) {
        drawAxialGradient(cgContext, gradientWithName(ConcaveGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);

        // The inner circle is 6 / 14 the size of the surrounding circle, 
        // leaving 8 / 14 around it. (8 / 14) / 2 = 2 / 7.

        static const float InnerInverseRatio = 2 / 7.0;

        clip.inflateX(-clip.width() * InnerInverseRatio);
        clip.inflateY(-clip.height() * InnerInverseRatio);

        paintInfo.context().drawRaisedEllipse(clip, Color::white, shadowColor());

        FloatSize radius(clip.width() / 2.0f, clip.height() / 2.0f);
        paintInfo.context().clipRoundedRect(FloatRoundedRect(clip, radius, radius, radius, radius));
    }
    FloatPoint bottomCenter(clip.x() + clip.width() / 2.0, clip.maxY());
    drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
    drawRadialGradient(cgContext, gradientWithName(ShineGradient), bottomCenter, 0, bottomCenter, std::max(clip.width(), clip.height()), ExponentialInterpolation);
    return false;
}

bool RenderThemeIOS::paintTextFieldDecorations(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    auto& style = box.style();
    FloatPoint point(rect.x() + style.borderLeftWidth(), rect.y() + style.borderTopWidth());

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    paintInfo.context().clipRoundedRect(style.getRoundedBorderFor(LayoutRect(rect)).pixelSnappedRoundedRectForPainting(box.document().deviceScaleFactor()));

    // This gradient gets drawn black when printing.
    // Do not draw the gradient if there is no visible top border.
    bool topBorderIsInvisible = !style.hasBorder() || !style.borderTopWidth() || style.borderTopIsTransparent();
    if (!box.view().printing() && !topBorderIsInvisible)
        drawAxialGradient(paintInfo.context().platformContext(), gradientWithName(InsetGradient), point, FloatPoint(CGPointMake(point.x(), point.y() + 3.0f)), LinearInterpolation);
    return false;
}

bool RenderThemeIOS::paintTextAreaDecorations(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintTextFieldDecorations(box, paintInfo, rect);
}

const int MenuListMinHeight = 15;

const float MenuListBaseHeight = 20;
const float MenuListBaseFontSize = 11;

const float MenuListArrowWidth = 7;
const float MenuListArrowHeight = 6;
const float MenuListButtonPaddingAfter = 19;

LengthBox RenderThemeIOS::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.appearance() == MenulistButtonPart) {
        if (style.direction() == TextDirection::RTL)
            return { 0, 0, 0, static_cast<int>(MenuListButtonPaddingAfter + style.borderTopWidth()) };
        return { 0, static_cast<int>(MenuListButtonPaddingAfter + style.borderTopWidth()), 0, 0 };
    }
    return { 0, 0, 0, 0 };
}

void RenderThemeIOS::adjustRoundBorderRadius(RenderStyle& style, RenderBox& box)
{
    if (style.appearance() == NoControlPart || style.backgroundLayers().hasImage())
        return;

    // FIXME: We should not be relying on border radius for the appearance of our controls <rdar://problem/7675493>.
    style.setBorderRadius({ { std::min(box.width(), box.height()) / 2, Fixed }, { box.height() / 2, Fixed } });
}

static void applyCommonButtonPaddingToStyle(RenderStyle& style, const Element& element)
{
    Document& document = element.document();
    auto emSize = CSSPrimitiveValue::create(0.5, CSSPrimitiveValue::CSS_EMS);
    int pixels = emSize->computeLength<int>(CSSToLengthConversionData(&style, document.renderStyle(), document.renderView(), document.frame()->pageZoomFactor()));
    style.setPaddingBox(LengthBox(0, pixels, 0, pixels));
}

static void adjustSelectListButtonStyle(RenderStyle& style, const Element& element)
{
    // Enforce "padding: 0 0.5em".
    applyCommonButtonPaddingToStyle(style, element);

    // Enforce "line-height: normal".
    style.setLineHeight(Length(-100.0, Percent));
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
    DateComponents::Type dateType = inputElement.dateType();
    if (dateType == DateComponents::Invalid || dateType == DateComponents::Week)
        return;

    // Enforce the width and set the box-sizing to content-box to not conflict with the padding.
    FontCascade font = style.fontCascade();
    
    float maximumWidth = localizedDateCache().maximumWidthForDateType(dateType, font, RenderThemeMeasureTextClient(font, style));

    ASSERT(maximumWidth >= 0);

    if (maximumWidth > 0) {
        int width = static_cast<int>(maximumWidth + MenuListButtonPaddingAfter);
        style.setWidth(Length(width, Fixed));
        style.setBoxSizing(BoxSizing::ContentBox);
    }
}

void RenderThemeIOS::adjustMenuListButtonStyle(StyleResolver&, RenderStyle& style, const Element* element) const
{
    // Set the min-height to be at least MenuListMinHeight.
    if (style.height().isAuto())
        style.setMinHeight(Length(std::max(MenuListMinHeight, static_cast<int>(MenuListBaseHeight / MenuListBaseFontSize * style.fontDescription().computedSize())), Fixed));
    else
        style.setMinHeight(Length(MenuListMinHeight, Fixed));

    if (!element)
        return;

    // Enforce some default styles in the case that this is a non-multiple <select> element,
    // or a date input. We don't force these if this is just an element with
    // "-webkit-appearance: menulist-button".
    if (is<HTMLSelectElement>(*element) && !element->hasAttributeWithoutSynchronization(HTMLNames::multipleAttr))
        adjustSelectListButtonStyle(style, *element);
    else if (is<HTMLInputElement>(*element))
        adjustInputElementButtonStyle(style, downcast<HTMLInputElement>(*element));
}

bool RenderThemeIOS::paintMenuListButtonDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
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

    box.drawLineForBoxSide(paintInfo.context(), FloatRect(FloatPoint(separatorPosition - borderTopWidth, clip.y()), FloatPoint(separatorPosition, clip.maxY())), BSRight, style.visitedDependentColor(CSSPropertyBorderTopColor), style.borderTopStyle(), 0, 0);

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
            paintInfo.context().drawRaisedEllipse(ellipse, Color::white, Color(0.0f, 0.0f, 0.0f, 0.5f));
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

        float opacity = isReadOnlyControl(box) ? 0.2 : 0.5;
        paintInfo.context().setStrokeColor(Color(0.0f, 0.0f, 0.0f, opacity));
        paintInfo.context().setFillColor(Color(0.0f, 0.0f, 0.0f, opacity));
        paintInfo.context().drawPath(Path::polygonPathFromPoints(shadow));

        paintInfo.context().setStrokeColor(Color::white);
        paintInfo.context().setFillColor(Color::white);
        paintInfo.context().drawPath(Path::polygonPathFromPoints(arrow));
    }

    return false;
}

const CGFloat kTrackThickness = 4.0;
const CGFloat kTrackRadius = kTrackThickness / 2.0;
const int kDefaultSliderThumbSize = 16;

void RenderThemeIOS::adjustSliderTrackStyle(StyleResolver& selector, RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSliderTrackStyle(selector, style, element);

    // FIXME: We should not be relying on border radius for the appearance of our controls <rdar://problem/7675493>.
    int radius = static_cast<int>(kTrackRadius);
    style.setBorderRadius({ { radius, Fixed }, { radius, Fixed } });
}

bool RenderThemeIOS::paintSliderTrack(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
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
            paintInfo.context().setStrokeColor(Color(178, 178, 178));
        else
            paintInfo.context().setStrokeColor(Color(76, 76, 76));

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
    style.setBorderRadius({ { 50, Percent }, { 50, Percent } });

    // Enforce a 16x16 size if no size is provided.
    if (style.width().isIntrinsicOrAuto() || style.height().isAuto()) {
        style.setWidth({ kDefaultSliderThumbSize, Fixed });
        style.setHeight({ kDefaultSliderThumbSize, Fixed });
    }
}

bool RenderThemeIOS::paintSliderThumbDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
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

    return false;
}

Seconds RenderThemeIOS::animationRepeatIntervalForProgressBar(RenderProgress&) const
{
    return 0_s;
}

Seconds RenderThemeIOS::animationDurationForProgressBar(RenderProgress&) const
{
    return 0_s;
}

bool RenderThemeIOS::paintProgressBar(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!is<RenderProgress>(renderer))
        return true;

    const int progressBarHeight = 9;
    const float verticalOffset = (rect.height() - progressBarHeight) / 2.0;

    GraphicsContextStateSaver stateSaver(paintInfo.context());
    if (rect.width() < 10 || rect.height() < 9) {
        // The rect is smaller than the standard progress bar. We clip to the element's rect to avoid
        // leaking pixels outside the repaint rect.
        paintInfo.context().clip(rect);
    }

    // 1) Draw the progress bar track.
    // 1.1) Draw the white background with grey gradient border.
    GraphicsContext& context = paintInfo.context();
    context.setStrokeThickness(0.68);
    context.setStrokeStyle(SolidStroke);

    const float verticalRenderingPosition = rect.y() + verticalOffset;
    auto strokeGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1) });
    strokeGradient->addColorStop(0.0, Color(0x8d, 0x8d, 0x8d));
    strokeGradient->addColorStop(0.45, Color(0xee, 0xee, 0xee));
    strokeGradient->addColorStop(0.55, Color(0xee, 0xee, 0xee));
    strokeGradient->addColorStop(1.0, Color(0x8d, 0x8d, 0x8d));
    context.setStrokeGradient(WTFMove(strokeGradient));

    context.setFillColor(Color(255, 255, 255));

    Path trackPath;
    FloatRect trackRect(rect.x() + 0.25, verticalRenderingPosition + 0.25, rect.width() - 0.5, progressBarHeight - 0.5);
    FloatSize roundedCornerRadius(5, 4);
    trackPath.addRoundedRect(trackRect, roundedCornerRadius);
    context.drawPath(trackPath);

    // 1.2) Draw top gradient on the upper half. It is supposed to overlay the fill from the background and darker the stroked path.
    FloatRect border(rect.x(), rect.y() + verticalOffset, rect.width(), progressBarHeight);
    paintInfo.context().clipRoundedRect(FloatRoundedRect(border, roundedCornerRadius, roundedCornerRadius, roundedCornerRadius, roundedCornerRadius));

    float upperGradientHeight = progressBarHeight / 2.;
    auto upperGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition + 0.5), FloatPoint(rect.x(), verticalRenderingPosition + upperGradientHeight - 1.5) });
    upperGradient->addColorStop(0.0, Color(133, 133, 133, 188));
    upperGradient->addColorStop(1.0, Color(18, 18, 18, 51));
    context.setFillGradient(WTFMove(upperGradient));

    context.fillRect(FloatRect(rect.x(), verticalRenderingPosition, rect.width(), upperGradientHeight));

    const auto& renderProgress = downcast<RenderProgress>(renderer);
    if (renderProgress.isDeterminate()) {
        // 2) Draw the progress bar.
        double position = clampTo(renderProgress.position(), 0.0, 1.0);
        double barWidth = position * rect.width();
        auto barGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition + 0.5), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1) });
        barGradient->addColorStop(0.0, Color(195, 217, 247));
        barGradient->addColorStop(0.45, Color(118, 164, 228));
        barGradient->addColorStop(0.49, Color(118, 164, 228));
        barGradient->addColorStop(0.51, Color(36, 114, 210));
        barGradient->addColorStop(0.55, Color(36, 114, 210));
        barGradient->addColorStop(1.0, Color(57, 142, 244));
        context.setFillGradient(WTFMove(barGradient));

        auto barStrokeGradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), verticalRenderingPosition), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1) });
        barStrokeGradient->addColorStop(0.0, Color(95, 107, 183));
        barStrokeGradient->addColorStop(0.5, Color(66, 106, 174, 240));
        barStrokeGradient->addColorStop(1.0, Color(38, 104, 166));
        context.setStrokeGradient(WTFMove(barStrokeGradient));

        Path barPath;
        int left = rect.x();
        if (!renderProgress.style().isLeftToRightDirection())
            left = rect.maxX() - barWidth;
        FloatRect barRect(left + 0.25, verticalRenderingPosition + 0.25, std::max(barWidth - 0.5, 0.0), progressBarHeight - 0.5);
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

void RenderThemeIOS::adjustSearchFieldStyle(StyleResolver& selector, RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSearchFieldStyle(selector, style, element);

    if (!element)
        return;

    if (!style.hasBorder())
        return;

    RenderBox* box = element->renderBox();
    if (!box)
        return;

    adjustRoundBorderRadius(style, *box);
}

bool RenderThemeIOS::paintSearchFieldDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintTextFieldDecorations(box, paintInfo, rect);
}

void RenderThemeIOS::adjustButtonStyle(StyleResolver& selector, RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustButtonStyle(selector, style, element);

#if ENABLE(INPUT_TYPE_COLOR)
    if (style.appearance() == ColorWellPart)
        return;
#endif

    // Set padding: 0 1.0em; on buttons.
    // CSSPrimitiveValue::computeLengthInt only needs the element's style to calculate em lengths.
    // Since the element might not be in a document, just pass nullptr for the root element style
    // and the render view.
    auto emSize = CSSPrimitiveValue::create(1.0, CSSPrimitiveValue::CSS_EMS);
    int pixels = emSize->computeLength<int>(CSSToLengthConversionData(&style, nullptr, nullptr, 1.0, false));
    style.setPaddingBox(LengthBox(0, pixels, 0, pixels));

    if (!element)
        return;

    RenderBox* box = element->renderBox();
    if (!box)
        return;

    adjustRoundBorderRadius(style, *box);
}

bool RenderThemeIOS::paintButtonDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintPushButtonDecorations(box, paintInfo, rect);
}

bool RenderThemeIOS::paintPushButtonDecorations(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    GraphicsContextStateSaver stateSaver(paintInfo.context());
    FloatRect clip = addRoundedBorderClip(box, paintInfo.context(), rect);

    CGContextRef cgContext = paintInfo.context().platformContext();
    if (box.style().visitedDependentColor(CSSPropertyBackgroundColor).isDark())
        drawAxialGradient(cgContext, gradientWithName(ConvexGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
    else {
        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
        drawAxialGradient(cgContext, gradientWithName(ShineGradient), FloatPoint(clip.x(), clip.maxY()), clip.location(), ExponentialInterpolation);
    }
    return false;
}

void RenderThemeIOS::setButtonSize(RenderStyle& style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    // Use the font size to determine the intrinsic width of the control.
    style.setHeight(Length(static_cast<int>(ControlBaseHeight / ControlBaseFontSize * style.fontDescription().computedSize()), Fixed));
}

const int kThumbnailBorderStrokeWidth = 1;
const int kThumbnailBorderCornerRadius = 1;
const int kVisibleBackgroundImageWidth = 1;
const int kMultipleThumbnailShrinkSize = 2;

bool RenderThemeIOS::paintFileUploadIconDecorations(const RenderObject&, const RenderObject& buttonRenderer, const PaintInfo& paintInfo, const IntRect& rect, Icon* icon, FileUploadDecorations fileUploadDecorations)
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
        Color backgroundImageColor = Color(buttonRenderer.style().visitedDependentColor(CSSPropertyBackgroundColor).rgb());
        paintInfo.context().fillRoundedRect(FloatRoundedRect(thumbnailPictureFrameRect, cornerSize, cornerSize, cornerSize, cornerSize), pictureFrameColor);
        paintInfo.context().fillRect(thumbnailRect, backgroundImageColor);
        {
            GraphicsContextStateSaver stateSaver2(paintInfo.context());
            CGContextRef cgContext = paintInfo.context().platformContext();
            paintInfo.context().clip(thumbnailRect);
            if (backgroundImageColor.isDark())
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

    return false;
}

Color RenderThemeIOS::platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return Color::transparent;
}

Color RenderThemeIOS::platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return Color::transparent;
}

#if ENABLE(FULL_KEYBOARD_ACCESS)
Color RenderThemeIOS::platformFocusRingColor(OptionSet<StyleColor::Options>) const
{
    return colorFromUIColor([PAL::getUIColorClass() keyboardFocusIndicatorColor]);
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

FontCascadeDescription& RenderThemeIOS::cachedSystemFontDescription(CSSValueID valueID) const
{
    static NeverDestroyed<FontCascadeDescription> systemFont;
    static NeverDestroyed<FontCascadeDescription> headlineFont;
    static NeverDestroyed<FontCascadeDescription> bodyFont;
    static NeverDestroyed<FontCascadeDescription> subheadlineFont;
    static NeverDestroyed<FontCascadeDescription> footnoteFont;
    static NeverDestroyed<FontCascadeDescription> caption1Font;
    static NeverDestroyed<FontCascadeDescription> caption2Font;
    static NeverDestroyed<FontCascadeDescription> shortHeadlineFont;
    static NeverDestroyed<FontCascadeDescription> shortBodyFont;
    static NeverDestroyed<FontCascadeDescription> shortSubheadlineFont;
    static NeverDestroyed<FontCascadeDescription> shortFootnoteFont;
    static NeverDestroyed<FontCascadeDescription> shortCaption1Font;
    static NeverDestroyed<FontCascadeDescription> tallBodyFont;
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    static NeverDestroyed<FontCascadeDescription> title0Font;
#endif
    static NeverDestroyed<FontCascadeDescription> title1Font;
    static NeverDestroyed<FontCascadeDescription> title2Font;
    static NeverDestroyed<FontCascadeDescription> title3Font;
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    static NeverDestroyed<FontCascadeDescription> title4Font;
#endif

    static CFStringRef userTextSize = contentSizeCategory();

    if (userTextSize != contentSizeCategory()) {
        userTextSize = contentSizeCategory();

        headlineFont.get().setIsAbsoluteSize(false);
        bodyFont.get().setIsAbsoluteSize(false);
        subheadlineFont.get().setIsAbsoluteSize(false);
        footnoteFont.get().setIsAbsoluteSize(false);
        caption1Font.get().setIsAbsoluteSize(false);
        caption2Font.get().setIsAbsoluteSize(false);
        shortHeadlineFont.get().setIsAbsoluteSize(false);
        shortBodyFont.get().setIsAbsoluteSize(false);
        shortSubheadlineFont.get().setIsAbsoluteSize(false);
        shortFootnoteFont.get().setIsAbsoluteSize(false);
        shortCaption1Font.get().setIsAbsoluteSize(false);
        tallBodyFont.get().setIsAbsoluteSize(false);
    }

    switch (valueID) {
    case CSSValueAppleSystemHeadline:
        return headlineFont;
    case CSSValueAppleSystemBody:
        return bodyFont;
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    case CSSValueAppleSystemTitle0:
        return title0Font;
#endif
    case CSSValueAppleSystemTitle1:
        return title1Font;
    case CSSValueAppleSystemTitle2:
        return title2Font;
    case CSSValueAppleSystemTitle3:
        return title3Font;
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    case CSSValueAppleSystemTitle4:
        return title4Font;
#endif
    case CSSValueAppleSystemSubheadline:
        return subheadlineFont;
    case CSSValueAppleSystemFootnote:
        return footnoteFont;
    case CSSValueAppleSystemCaption1:
        return caption1Font;
    case CSSValueAppleSystemCaption2:
        return caption2Font;
        // Short version.
    case CSSValueAppleSystemShortHeadline:
        return shortHeadlineFont;
    case CSSValueAppleSystemShortBody:
        return shortBodyFont;
    case CSSValueAppleSystemShortSubheadline:
        return shortSubheadlineFont;
    case CSSValueAppleSystemShortFootnote:
        return shortFootnoteFont;
    case CSSValueAppleSystemShortCaption1:
        return shortCaption1Font;
        // Tall version.
    case CSSValueAppleSystemTallBody:
        return tallBodyFont;
    default:
        return systemFont;
    }
}

static inline FontSelectionValue cssWeightOfSystemFont(CTFontRef font)
{
    RetainPtr<CFDictionaryRef> traits = adoptCF(CTFontCopyTraits(font));
    CFNumberRef resultRef = (CFNumberRef)CFDictionaryGetValue(traits.get(), kCTFontWeightTrait);
    float result = 0;
    CFNumberGetValue(resultRef, kCFNumberFloatType, &result);
    // These numbers were experimentally gathered from weights of the system font.
    static float weightThresholds[] = { -0.6, -0.365, -0.115, 0.130, 0.235, 0.350, 0.5, 0.7 };
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(weightThresholds); ++i) {
        if (result < weightThresholds[i])
            return FontSelectionValue((static_cast<int>(i) + 1) * 100);
    }
    return FontSelectionValue(900);
}

void RenderThemeIOS::updateCachedSystemFontDescription(CSSValueID valueID, FontCascadeDescription& fontDescription) const
{
    RetainPtr<CTFontDescriptorRef> fontDescriptor;
    CFStringRef textStyle;
    switch (valueID) {
    case CSSValueAppleSystemHeadline:
        textStyle = kCTUIFontTextStyleHeadline;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemBody:
        textStyle = kCTUIFontTextStyleBody;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    case CSSValueAppleSystemTitle0:
        textStyle = kCTUIFontTextStyleTitle0;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
#endif
    case CSSValueAppleSystemTitle1:
        textStyle = kCTUIFontTextStyleTitle1;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemTitle2:
        textStyle = kCTUIFontTextStyleTitle2;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemTitle3:
        textStyle = kCTUIFontTextStyleTitle3;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    case CSSValueAppleSystemTitle4:
        textStyle = kCTUIFontTextStyleTitle4;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
#endif
    case CSSValueAppleSystemSubheadline:
        textStyle = kCTUIFontTextStyleSubhead;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemFootnote:
        textStyle = kCTUIFontTextStyleFootnote;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemCaption1:
        textStyle = kCTUIFontTextStyleCaption1;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemCaption2:
        textStyle = kCTUIFontTextStyleCaption2;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;

    // Short version.
    case CSSValueAppleSystemShortHeadline:
        textStyle = kCTUIFontTextStyleShortHeadline;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemShortBody:
        textStyle = kCTUIFontTextStyleShortBody;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemShortSubheadline:
        textStyle = kCTUIFontTextStyleShortSubhead;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemShortFootnote:
        textStyle = kCTUIFontTextStyleShortFootnote;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemShortCaption1:
        textStyle = kCTUIFontTextStyleShortCaption1;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;

    // Tall version.
    case CSSValueAppleSystemTallBody:
        textStyle = kCTUIFontTextStyleTallBody;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;

    default:
        textStyle = kCTFontDescriptorTextStyleEmphasized;
        fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, 0, nullptr));
    }

    ASSERT(fontDescriptor);
    RetainPtr<CTFontRef> font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), 0, nullptr));
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setOneFamily(textStyle);
    fontDescription.setSpecifiedSize(CTFontGetSize(font.get()));
    fontDescription.setWeight(cssWeightOfSystemFont(font.get()));
    fontDescription.setItalic(normalItalicValue());
}

#if ENABLE(VIDEO)
String RenderThemeIOS::mediaControlsStyleSheet()
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (m_legacyMediaControlsStyleSheet.isEmpty())
        m_legacyMediaControlsStyleSheet = [NSString stringWithContentsOfFile:[[NSBundle bundleForClass:[WebCoreRenderThemeBundle class]] pathForResource:@"mediaControlsiOS" ofType:@"css"] encoding:NSUTF8StringEncoding error:nil];
    return m_legacyMediaControlsStyleSheet;
#else
    return emptyString();
#endif
}

String RenderThemeIOS::modernMediaControlsStyleSheet()
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (RuntimeEnabledFeatures::sharedFeatures().modernMediaControlsEnabled()) {
        if (m_mediaControlsStyleSheet.isEmpty())
            m_mediaControlsStyleSheet = [NSString stringWithContentsOfFile:[[NSBundle bundleForClass:[WebCoreRenderThemeBundle class]] pathForResource:@"modern-media-controls" ofType:@"css" inDirectory:@"modern-media-controls"] encoding:NSUTF8StringEncoding error:nil];
        return m_mediaControlsStyleSheet;
    }
    return emptyString();
#else
    return emptyString();
#endif
}

void RenderThemeIOS::purgeCaches()
{
    m_legacyMediaControlsScript.clearImplIfNotShared();
    m_mediaControlsScript.clearImplIfNotShared();
    m_legacyMediaControlsStyleSheet.clearImplIfNotShared();
    m_mediaControlsStyleSheet.clearImplIfNotShared();
}

String RenderThemeIOS::mediaControlsScript()
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (RuntimeEnabledFeatures::sharedFeatures().modernMediaControlsEnabled()) {
        if (m_mediaControlsScript.isEmpty()) {
            NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];

            StringBuilder scriptBuilder;
            scriptBuilder.append([NSString stringWithContentsOfFile:[bundle pathForResource:@"modern-media-controls-localized-strings" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil]);
            scriptBuilder.append([NSString stringWithContentsOfFile:[bundle pathForResource:@"modern-media-controls" ofType:@"js" inDirectory:@"modern-media-controls"] encoding:NSUTF8StringEncoding error:nil]);
            m_mediaControlsScript = scriptBuilder.toString();
        }
        return m_mediaControlsScript;
    }

    if (m_legacyMediaControlsScript.isEmpty()) {
        NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];

        StringBuilder scriptBuilder;
        scriptBuilder.append([NSString stringWithContentsOfFile:[bundle pathForResource:@"mediaControlsLocalizedStrings" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil]);
        scriptBuilder.append([NSString stringWithContentsOfFile:[bundle pathForResource:@"mediaControlsApple" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil]);
        scriptBuilder.append([NSString stringWithContentsOfFile:[bundle pathForResource:@"mediaControlsiOS" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil]);

        m_legacyMediaControlsScript = scriptBuilder.toString();
    }
    return m_legacyMediaControlsScript;
#else
    return emptyString();
#endif
}

String RenderThemeIOS::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (!RuntimeEnabledFeatures::sharedFeatures().modernMediaControlsEnabled())
        return emptyString();

    String directory = "modern-media-controls/images";
    NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
    return [[NSData dataWithContentsOfFile:[bundle pathForResource:iconName ofType:iconType inDirectory:directory]] base64EncodedStringWithOptions:0];
#else
    return emptyString();
#endif
}

#endif // ENABLE(VIDEO)

Color RenderThemeIOS::systemColor(CSSValueID cssValueID, OptionSet<StyleColor::Options> options) const
{
    const bool forVisitedLink = options.contains(StyleColor::Options::ForVisitedLink);

    // The system color cache below can't handle visited links. The only color value
    // that cares about visited links is CSSValueWebkitLink, so handle it here by
    // calling through to RenderTheme's base implementation.
    if (forVisitedLink && cssValueID == CSSValueWebkitLink)
        return RenderTheme::systemColor(cssValueID, options);

    ASSERT(!forVisitedLink);

    auto addResult = m_systemColorCache.add(cssValueID, Color());
    if (!addResult.isNewEntry)
        return addResult.iterator->value;

    Color color;
    switch (cssValueID) {
    case CSSValueAppleWirelessPlaybackTargetActive:
        color = [PAL::getUIColorClass() systemBlueColor].CGColor;
        break;
    case CSSValueAppleSystemBlue:
        color = [PAL::getUIColorClass() systemBlueColor].CGColor;
        break;
    case CSSValueAppleSystemGray:
        color = [PAL::getUIColorClass() systemGrayColor].CGColor;
        break;
    case CSSValueAppleSystemGreen:
        color = [PAL::getUIColorClass() systemGreenColor].CGColor;
        break;
    case CSSValueAppleSystemOrange:
        color = [PAL::getUIColorClass() systemOrangeColor].CGColor;
        break;
    case CSSValueAppleSystemPink:
        color = [PAL::getUIColorClass() systemPinkColor].CGColor;
        break;
    case CSSValueAppleSystemRed:
        color = [PAL::getUIColorClass() systemRedColor].CGColor;
        break;
    case CSSValueAppleSystemYellow:
        color = [PAL::getUIColorClass() systemYellowColor].CGColor;
        break;
    default:
        break;
    }

    if (!color.isValid())
        color = RenderTheme::systemColor(cssValueID, options);

    addResult.iterator->value = color;

    return addResult.iterator->value;
}

#if ENABLE(ATTACHMENT_ELEMENT)

const CGSize attachmentSize = { 160, 119 };

const CGFloat attachmentBorderRadius = 16;
static Color attachmentBorderColor() { return Color(204, 204, 204); }

static Color attachmentProgressColor() { return Color(222, 222, 222); }
const CGFloat attachmentProgressBorderThickness = 3;

const CGFloat attachmentProgressSize = 36;
const CGFloat attachmentIconSize = 48;

const CGFloat attachmentItemMargin = 8;

const CGFloat attachmentWrappingTextMaximumWidth = 140;
const CFIndex attachmentWrappingTextMaximumLineCount = 2;

static RetainPtr<CTFontRef> attachmentActionFont()
{
    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(kCTUIFontTextStyleShortFootnote, RenderThemeIOS::contentSizeCategory(), 0));
    RetainPtr<CTFontDescriptorRef> emphasizedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(),
        (CFDictionaryRef)@{
            (id)kCTFontDescriptorTextStyleAttribute: (id)kCTFontDescriptorTextStyleEmphasized
    }));
    return adoptCF(CTFontCreateWithFontDescriptor(emphasizedFontDescriptor.get(), 0, nullptr));
}

static UIColor *attachmentActionColor(const RenderAttachment& attachment)
{
    return [PAL::getUIColorClass() colorWithCGColor:cachedCGColor(attachment.style().visitedDependentColor(CSSPropertyColor))];
}

static RetainPtr<CTFontRef> attachmentTitleFont()
{
    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(kCTUIFontTextStyleShortCaption1, RenderThemeIOS::contentSizeCategory(), 0));
    return adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), 0, nullptr));
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
    CGSize textSize = CTFramesetterSuggestFrameSizeWithConstraints(framesetter.get(), CFRangeMake(0, 0), nullptr, CGSizeMake(attachmentWrappingTextMaximumWidth, CGFLOAT_MAX), &fitRange);

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
    RetainPtr<CTLineRef> truncatedLine = adoptCF(CTLineCreateTruncatedLine(remainingLine, attachmentWrappingTextMaximumWidth, kCTLineTruncationMiddle, ellipsisLine.get()));

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
        if (icon) {
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
    return LayoutSize(FloatSize(attachmentSize));
}

int RenderThemeIOS::attachmentBaseline(const RenderAttachment& attachment) const
{
    RenderAttachmentInfo info(attachment);
    return info.baseline;
}

static void paintAttachmentIcon(GraphicsContext& context, RenderAttachmentInfo& info)
{
    if (!info.icon)
        return;

    auto iconImage = BitmapImage::create([info.icon CGImage]);
    context.drawImage(iconImage.get(), info.iconRect);
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
    context.setStrokeColor(attachmentProgressColor());
    context.setFillColor(attachmentProgressColor());
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
    Path borderPath;
    borderPath.addRoundedRect(info.attachmentRect, FloatSize(attachmentBorderRadius, attachmentBorderRadius));
    return borderPath;
}

static void paintAttachmentBorder(GraphicsContext& context, Path& borderPath)
{
    context.setStrokeColor(attachmentBorderColor());
    context.setStrokeThickness(1);
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
    else if (info.icon)
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

    CIImage *inputImage = [CIImage imageWithCGImage:image.nativeImage().get()];

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
    Color shadowColor = Color { 0.f, 0.f, 0.f, 0.1f };
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
#if HAVE(IOSURFACE)
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

} // namespace WebCore

#endif //PLATFORM(IOS_FAMILY)
