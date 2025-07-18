/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include "CSSAppleColorFilterPropertyValue.h"
#include "CSSBasicShapeValue.h"
#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSCounterValue.h"
#include "CSSEasingFunctionValue.h"
#include "CSSFilterPropertyValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSPathValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "CSSPropertyParserConsumer+Anchor.h"
#include "CSSQuadValue.h"
#include "CSSRatioValue.h"
#include "CSSRayValue.h"
#include "CSSRectValue.h"
#include "CSSReflectValue.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSScrollValue.h"
#include "CSSSerializationContext.h"
#include "CSSTransformListValue.h"
#include "CSSURLValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "CSSViewValue.h"
#include "ContainerNodeInlines.h"
#include "ContentData.h"
#include "CursorList.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "FontSelectionValueInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "Length.h"
#include "PathOperation.h"
#include "PerspectiveTransformOperation.h"
#include "QuotesData.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderObjectInlines.h"
#include "RenderStyleInlines.h"
#include "SVGRenderStyle.h"
#include "ScrollTimeline.h"
#include "SkewTransformOperation.h"
#include "StyleAppleColorFilterProperty.h"
#include "StyleClipPath.h"
#include "StyleColor.h"
#include "StyleColorScheme.h"
#include "StyleCornerShapeValue.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleEasingFunction.h"
#include "StyleExtractorState.h"
#include "StyleFilterProperty.h"
#include "StyleFlexBasis.h"
#include "StyleInset.h"
#include "StyleLineBoxContain.h"
#include "StyleMargin.h"
#include "StyleMaximumSize.h"
#include "StyleMinimumSize.h"
#include "StyleOffsetPath.h"
#include "StylePadding.h"
#include "StylePathData.h"
#include "StylePerspective.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleReflection.h"
#include "StyleRotate.h"
#include "StyleScale.h"
#include "StyleScrollMargin.h"
#include "StyleScrollPadding.h"
#include "StyleTranslate.h"
#include "TimelineRange.h"
#include "TransformOperationData.h"
#include "ViewTimeline.h"
#include "WebAnimationUtilities.h"
#include <wtf/IteratorRange.h>

namespace WebCore {
namespace Style {

class ExtractorConverter {
public:
    // MARK: Strong value conversions

    template<typename T, typename... Rest> static Ref<CSSValue> convertStyleType(ExtractorState&, const T&, Rest&&...);

    // MARK: Primitive conversions

    template<typename ConvertibleType>
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, const ConvertibleType&);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, double);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, float);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, unsigned);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, int);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, unsigned short);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, short);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, const ScopedName&);

    static Ref<CSSPrimitiveValue> convertLength(ExtractorState&, const WebCore::Length&);
    static Ref<CSSPrimitiveValue> convertLength(const RenderStyle&, const WebCore::Length&);
    static Ref<CSSPrimitiveValue> convertLengthAllowingNumber(ExtractorState&, const WebCore::Length&);
    static Ref<CSSPrimitiveValue> convertLengthOrAuto(ExtractorState&, const WebCore::Length&);

    template<typename T> static Ref<CSSPrimitiveValue> convertNumber(ExtractorState&, T);
    template<typename T> static Ref<CSSPrimitiveValue> convertNumberAsPixels(ExtractorState&, T);
    template<typename T> static Ref<CSSPrimitiveValue> convertComputedLength(ExtractorState&, T);
    template<typename T, CSSValueID> static Ref<CSSValue> convertNumberOrKeyword(ExtractorState&, T);
    template<typename T> static Ref<CSSPrimitiveValue> convertLineWidth(ExtractorState&, T lineWidth);

    template<CSSValueID> static Ref<CSSPrimitiveValue> convertStringOrKeyword(ExtractorState&, const String&);
    template<CSSValueID> static Ref<CSSPrimitiveValue> convertCustomIdentOrKeyword(ExtractorState&, const String&);
    template<CSSValueID> static Ref<CSSPrimitiveValue> convertStringAtomOrKeyword(ExtractorState&, const AtomString&);
    template<CSSValueID> static Ref<CSSPrimitiveValue> convertCustomIdentAtomOrKeyword(ExtractorState&, const AtomString&);

    // MARK: SVG conversions

    static Ref<CSSValue> convertSVGURIReference(ExtractorState&, const URL&);

    // MARK: Transform conversions

    static Ref<CSSValue> convertTransformationMatrix(ExtractorState&, const TransformationMatrix&);
    static Ref<CSSValue> convertTransformationMatrix(const RenderStyle&, const TransformationMatrix&);
    static Ref<CSSValue> convertTransformOperation(ExtractorState&, const TransformOperation&);
    static Ref<CSSValue> convertTransformOperation(const RenderStyle&, const TransformOperation&);

    // MARK: Shared conversions

    static Ref<CSSValue> convertOpacity(ExtractorState&, float);
    static Ref<CSSValue> convertImageOrNone(ExtractorState&, const StyleImage*);
    static Ref<CSSValue> convertGlyphOrientation(ExtractorState&, GlyphOrientation);
    static Ref<CSSValue> convertGlyphOrientationOrAuto(ExtractorState&, GlyphOrientation);
    static Ref<CSSValue> convertMarginTrim(ExtractorState&, OptionSet<MarginTrimType>);
    static Ref<CSSValue> convertShapeValue(ExtractorState&, const ShapeValue*);
    static Ref<CSSValue> convertDPath(ExtractorState&, const StylePathData*);
    static Ref<CSSValue> convertStrokeDashArray(ExtractorState&, const FixedVector<WebCore::Length>&);
    static Ref<CSSValue> convertTextStrokeWidth(ExtractorState&, float);
    static Ref<CSSValue> convertFilterOperations(ExtractorState&, const FilterOperations&);
    static Ref<CSSValue> convertAppleColorFilterOperations(ExtractorState&, const FilterOperations&);
    static Ref<CSSValue> convertWebkitTextCombine(ExtractorState&, TextCombine);
    static Ref<CSSValue> convertImageOrientation(ExtractorState&, ImageOrientation);
    static Ref<CSSValue> convertLineClamp(ExtractorState&, const LineClampValue&);
    static Ref<CSSValue> convertContain(ExtractorState&, OptionSet<Containment>);
    static Ref<CSSValue> convertMaxLines(ExtractorState&, size_t);
    static Ref<CSSValue> convertSmoothScrolling(ExtractorState&, bool);
    static Ref<CSSValue> convertInitialLetter(ExtractorState&, IntSize);
    static Ref<CSSValue> convertTextSpacingTrim(ExtractorState&, TextSpacingTrim);
    static Ref<CSSValue> convertTextAutospace(ExtractorState&, TextAutospace);
    static Ref<CSSValue> convertReflection(ExtractorState&, const StyleReflection*);
    static Ref<CSSValue> convertLineFitEdge(ExtractorState&, const TextEdge&);
    static Ref<CSSValue> convertTextBoxEdge(ExtractorState&, const TextEdge&);
    static Ref<CSSValue> convertQuotes(ExtractorState&, const QuotesData*);
    static Ref<CSSValue> convertViewTransitionName(ExtractorState&, const ViewTransitionName&);
    static Ref<CSSValue> convertPositionTryFallbacks(ExtractorState&, const FixedVector<PositionTryFallback>&);
    static Ref<CSSValue> convertWillChange(ExtractorState&, const WillChangeData*);
    static Ref<CSSValue> convertBlockStepSize(ExtractorState&, std::optional<WebCore::Length>);
    static Ref<CSSValue> convertTabSize(ExtractorState&, const TabSize&);
    static Ref<CSSValue> convertScrollSnapType(ExtractorState&, const ScrollSnapType&);
    static Ref<CSSValue> convertScrollSnapAlign(ExtractorState&, const ScrollSnapAlign&);
    static Ref<CSSValue> convertScrollbarColor(ExtractorState&, std::optional<ScrollbarColor>);
    static Ref<CSSValue> convertLineBoxContain(ExtractorState&, OptionSet<Style::LineBoxContain>);
    static Ref<CSSValue> convertWebkitRubyPosition(ExtractorState&, RubyPosition);
    static Ref<CSSValue> convertPosition(ExtractorState&, const LengthPoint&);
    static Ref<CSSValue> convertTouchAction(ExtractorState&, OptionSet<TouchAction>);
    static Ref<CSSValue> convertTextTransform(ExtractorState&, OptionSet<TextTransform>);
    static Ref<CSSValue> convertTextDecorationLine(ExtractorState&, OptionSet<TextDecorationLine>);
    static Ref<CSSValue> convertTextUnderlinePosition(ExtractorState&, OptionSet<TextUnderlinePosition>);
    static Ref<CSSValue> convertTextDecorationThickness(ExtractorState&, const TextDecorationThickness&);
    static Ref<CSSValue> convertTextEmphasisPosition(ExtractorState&, OptionSet<TextEmphasisPosition>);
    static Ref<CSSValue> convertSpeakAs(ExtractorState&, OptionSet<SpeakAs>);
    static Ref<CSSValue> convertHangingPunctuation(ExtractorState&, OptionSet<HangingPunctuation>);
    static Ref<CSSValue> convertPageBreak(ExtractorState&, BreakBetween);
    static Ref<CSSValue> convertPageBreak(ExtractorState&, BreakInside);
    static Ref<CSSValue> convertWebkitColumnBreak(ExtractorState&, BreakBetween);
    static Ref<CSSValue> convertWebkitColumnBreak(ExtractorState&, BreakInside);
    static Ref<CSSValue> convertSelfOrDefaultAlignmentData(ExtractorState&, const StyleSelfAlignmentData&);
    static Ref<CSSValue> convertContentAlignmentData(ExtractorState&, const StyleContentAlignmentData&);
    static Ref<CSSValue> convertPaintOrder(ExtractorState&, PaintOrder);
    static Ref<CSSValue> convertPositionAnchor(ExtractorState&, const std::optional<ScopedName>&);
    static Ref<CSSValue> convertPositionArea(ExtractorState&, const PositionArea&);
    static Ref<CSSValue> convertPositionArea(ExtractorState&, const std::optional<PositionArea>&);
    static Ref<CSSValue> convertNameScope(ExtractorState&, const NameScope&);
    static Ref<CSSValue> convertPositionVisibility(ExtractorState&, OptionSet<PositionVisibility>);
#if ENABLE(TEXT_AUTOSIZING)
    static Ref<CSSValue> convertWebkitTextSizeAdjust(ExtractorState&, const TextSizeAdjustment&);
#endif
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    static Ref<CSSValue> convertOverflowScrolling(ExtractorState&, bool);
#endif
#if PLATFORM(IOS_FAMILY)
    static Ref<CSSValue> convertTouchCallout(ExtractorState&, bool);
#endif

    // MARK: FillLayer conversions

    static Ref<CSSValue> convertFillLayerAttachment(ExtractorState&, FillAttachment);
    static Ref<CSSValue> convertFillLayerBlendMode(ExtractorState&, BlendMode);
    static Ref<CSSValue> convertFillLayerClip(ExtractorState&, FillBox);
    static Ref<CSSValue> convertFillLayerOrigin(ExtractorState&, FillBox);
    static Ref<CSSValue> convertFillLayerRepeat(ExtractorState&, FillRepeatXY);
    static Ref<CSSValue> convertFillLayerBackgroundSize(ExtractorState&, FillSize);
    static Ref<CSSValue> convertFillLayerMaskSize(ExtractorState&, FillSize);
    static Ref<CSSValue> convertFillLayerMaskComposite(ExtractorState&, CompositeOperator);
    static Ref<CSSValue> convertFillLayerWebkitMaskComposite(ExtractorState&, CompositeOperator);
    static Ref<CSSValue> convertFillLayerMaskMode(ExtractorState&, MaskMode);
    static Ref<CSSValue> convertFillLayerWebkitMaskSourceType(ExtractorState&, MaskMode);
    static Ref<CSSValue> convertFillLayerImage(ExtractorState&, const StyleImage*);

    // MARK: Font conversions

    static Ref<CSSValue> convertFontFamily(ExtractorState&, const AtomString&);
    static Ref<CSSValue> convertFontSizeAdjust(ExtractorState&, const FontSizeAdjust&);
    static Ref<CSSValue> convertFontPalette(ExtractorState&, const FontPalette&);
    static Ref<CSSValue> convertFontWeight(ExtractorState&, FontSelectionValue);
    static Ref<CSSValue> convertFontWidth(ExtractorState&, FontSelectionValue);
    static Ref<CSSValue> convertFontFeatureSettings(ExtractorState&, const FontFeatureSettings&);
    static Ref<CSSValue> convertFontVariationSettings(ExtractorState&, const FontVariationSettings&);

    // MARK: NinePieceImage conversions

    static Ref<CSSValue> convertNinePieceImageQuad(ExtractorState&, const LengthBox&);
    static Ref<CSSValue> convertNinePieceImageSlices(ExtractorState&, const NinePieceImage&);
    static Ref<CSSValue> convertNinePieceImageRepeat(ExtractorState&, const NinePieceImage&);
    static Ref<CSSValue> convertNinePieceImage(ExtractorState&, const NinePieceImage&);

    // MARK: Animation/Transition conversions

    static Ref<CSSValue> convertAnimationName(ExtractorState&, const ScopedName&, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationProperty(ExtractorState&, const Animation::TransitionProperty&, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationAllowsDiscreteTransitions(ExtractorState&, bool, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationDuration(ExtractorState&, Markable<double>, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationDelay(ExtractorState&, double, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationIterationCount(ExtractorState&, double, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationDirection(ExtractorState&, Animation::Direction, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationFillMode(ExtractorState&, AnimationFillMode, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationCompositeOperation(ExtractorState&, CompositeOperation, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationPlayState(ExtractorState&, AnimationPlayState, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationTimeline(ExtractorState&, const Animation::Timeline&, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationTimingFunction(ExtractorState&, const TimingFunction&, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertAnimationTimingFunction(ExtractorState&, const TimingFunction*, const Animation*, const AnimationList*);
    static Ref<CSSValueList> convertAnimationSingleRange(ExtractorState&, const SingleTimelineRange&, SingleTimelineRange::Type);
    static Ref<CSSValueList> convertAnimationRangeStart(ExtractorState&, const SingleTimelineRange&, const Animation*, const AnimationList*);
    static Ref<CSSValueList> convertAnimationRangeEnd(ExtractorState&, const SingleTimelineRange&, const Animation*, const AnimationList*);
    static Ref<CSSValueList> convertAnimationRange(ExtractorState&, const TimelineRange&, const Animation*, const AnimationList*);
    static Ref<CSSValue> convertSingleAnimation(ExtractorState&, const Animation&);
    static Ref<CSSValue> convertSingleTransition(ExtractorState&, const Animation&);

    // MARK: Grid conversions

    static Ref<CSSValue> convertGridAutoFlow(ExtractorState&, GridAutoFlow);
};

// MARK: - Strong value conversions

template<typename T, typename... Rest> Ref<CSSValue> ExtractorConverter::convertStyleType(ExtractorState& state, const T& value, Rest&&... rest)
{
    return createCSSValue(state.pool, state.style, value, std::forward<Rest>(rest)...);
}

// MARK: - Primitive conversions

template<typename ConvertibleType>
Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, const ConvertibleType& value)
{
    return CSSPrimitiveValue::create(toCSSValueID(value));
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, double value)
{
    return CSSPrimitiveValue::create(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, float value)
{
    return CSSPrimitiveValue::create(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, unsigned value)
{
    return CSSPrimitiveValue::createInteger(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, int value)
{
    return CSSPrimitiveValue::createInteger(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, unsigned short value)
{
    return CSSPrimitiveValue::createInteger(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, short value)
{
    return CSSPrimitiveValue::createInteger(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, const ScopedName& scopedName)
{
    if (scopedName.isIdentifier)
        return CSSPrimitiveValue::createCustomIdent(scopedName.name);
    return CSSPrimitiveValue::create(scopedName.name);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convertLength(ExtractorState& state, const WebCore::Length& length)
{
    return convertLength(state.style, length);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convertLength(const RenderStyle& style, const WebCore::Length& length)
{
    if (length.isFixed())
        return CSSPrimitiveValue::create(adjustFloatForAbsoluteZoom(length.value(), style), CSSUnitType::CSS_PX);
    return CSSPrimitiveValue::create(length, style);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convertLengthAllowingNumber(ExtractorState& state, const WebCore::Length& length)
{
    return convertLength(state, length);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convertLengthOrAuto(ExtractorState& state, const WebCore::Length& length)
{
    if (length.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return convertLength(state, length);
}

template<typename T> Ref<CSSPrimitiveValue> ExtractorConverter::convertNumber(ExtractorState& state, T number)
{
    return convert(state, number);
}

template<typename T> Ref<CSSPrimitiveValue> ExtractorConverter::convertNumberAsPixels(ExtractorState& state, T number)
{
    return CSSPrimitiveValue::create(adjustFloatForAbsoluteZoom(number, state.style), CSSUnitType::CSS_PX);
}

template<typename T> Ref<CSSPrimitiveValue> ExtractorConverter::convertComputedLength(ExtractorState& state, T number)
{
    return convertNumberAsPixels(state, number);
}

template<typename T, CSSValueID keyword> Ref<CSSValue> ExtractorConverter::convertNumberOrKeyword(ExtractorState&, T number)
{
    if (number < 0)
        return CSSPrimitiveValue::create(keyword);
    return CSSPrimitiveValue::create(number);
}

template<typename T> Ref<CSSPrimitiveValue> ExtractorConverter::convertLineWidth(ExtractorState& state, T lineWidth)
{
    return convertNumberAsPixels(state, lineWidth);
}

template<CSSValueID keyword> Ref<CSSPrimitiveValue> ExtractorConverter::convertStringOrKeyword(ExtractorState&, const String& string)
{
    if (string.isNull())
        return CSSPrimitiveValue::create(keyword);
    return CSSPrimitiveValue::create(string);
}

template<CSSValueID keyword> Ref<CSSPrimitiveValue> ExtractorConverter::convertCustomIdentOrKeyword(ExtractorState&, const String& string)
{
    if (string.isNull())
        return CSSPrimitiveValue::create(keyword);
    return CSSPrimitiveValue::createCustomIdent(string);
}

template<CSSValueID keyword> Ref<CSSPrimitiveValue> ExtractorConverter::convertStringAtomOrKeyword(ExtractorState&, const AtomString& string)
{
    if (string.isNull())
        return CSSPrimitiveValue::create(keyword);
    return CSSPrimitiveValue::create(string);
}

template<CSSValueID keyword> Ref<CSSPrimitiveValue> ExtractorConverter::convertCustomIdentAtomOrKeyword(ExtractorState&, const AtomString& string)
{
    if (string.isNull())
        return CSSPrimitiveValue::create(keyword);
    return CSSPrimitiveValue::createCustomIdent(string);
}

// MARK: - SVG conversions

inline Ref<CSSValue> ExtractorConverter::convertSVGURIReference(ExtractorState& state, const URL& marker)
{
    if (marker.isNone())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSURLValue::create(toCSS(marker, state.style));
}

// MARK: - Transform conversions

inline Ref<CSSValue> ExtractorConverter::convertTransformationMatrix(ExtractorState& state, const TransformationMatrix& transform)
{
    return convertTransformationMatrix(state.style, transform);
}

inline Ref<CSSValue> ExtractorConverter::convertTransformationMatrix(const RenderStyle& style, const TransformationMatrix& transform)
{
    auto zoom = style.usedZoom();
    if (transform.isAffine()) {
        double values[] = { transform.a(), transform.b(), transform.c(), transform.d(), transform.e() / zoom, transform.f() / zoom };
        CSSValueListBuilder arguments;
        for (auto value : values)
            arguments.append(CSSPrimitiveValue::create(value));
        return CSSFunctionValue::create(CSSValueMatrix, WTFMove(arguments));
    }

    double values[] = {
        transform.m11(), transform.m12(), transform.m13(), transform.m14() * zoom,
        transform.m21(), transform.m22(), transform.m23(), transform.m24() * zoom,
        transform.m31(), transform.m32(), transform.m33(), transform.m34() * zoom,
        transform.m41() / zoom, transform.m42() / zoom, transform.m43() / zoom, transform.m44()
    };
    CSSValueListBuilder arguments;
    for (auto value : values)
        arguments.append(CSSPrimitiveValue::create(value));
    return CSSFunctionValue::create(CSSValueMatrix3d, WTFMove(arguments));
}

inline Ref<CSSValue> ExtractorConverter::convertTransformOperation(ExtractorState& state, const TransformOperation& operation)
{
    return convertTransformOperation(state.style, operation);
}

inline Ref<CSSValue> ExtractorConverter::convertTransformOperation(const RenderStyle& style, const TransformOperation& operation)
{
    auto translateLength = [&](const auto& length) -> Ref<CSSPrimitiveValue> {
        if (length.isZero())
            return CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
        return convertLength(style, length);
    };

    auto includeLength = [](const auto& length) -> bool {
        return !length.isZero() || length.isPercent();
    };

    switch (operation.type()) {
    case TransformOperation::Type::TranslateX:
        return CSSFunctionValue::create(CSSValueTranslateX, translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).x()));
    case TransformOperation::Type::TranslateY:
        return CSSFunctionValue::create(CSSValueTranslateY, translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).y()));
    case TransformOperation::Type::TranslateZ:
        return CSSFunctionValue::create(CSSValueTranslateZ, translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).z()));
    case TransformOperation::Type::Translate:
    case TransformOperation::Type::Translate3D: {
        auto& translate = uncheckedDowncast<TranslateTransformOperation>(operation);
        if (!translate.is3DOperation()) {
            if (!includeLength(translate.y()))
                return CSSFunctionValue::create(CSSValueTranslate, translateLength(translate.x()));
            return CSSFunctionValue::create(CSSValueTranslate, translateLength(translate.x()),
                translateLength(translate.y()));
        }
        return CSSFunctionValue::create(CSSValueTranslate3d,
            translateLength(translate.x()),
            translateLength(translate.y()),
            translateLength(translate.z()));
    }
    case TransformOperation::Type::ScaleX:
        return CSSFunctionValue::create(CSSValueScaleX, CSSPrimitiveValue::create(uncheckedDowncast<ScaleTransformOperation>(operation).x()));
    case TransformOperation::Type::ScaleY:
        return CSSFunctionValue::create(CSSValueScaleY, CSSPrimitiveValue::create(uncheckedDowncast<ScaleTransformOperation>(operation).y()));
    case TransformOperation::Type::ScaleZ:
        return CSSFunctionValue::create(CSSValueScaleZ, CSSPrimitiveValue::create(uncheckedDowncast<ScaleTransformOperation>(operation).z()));
    case TransformOperation::Type::Scale:
    case TransformOperation::Type::Scale3D: {
        auto& scale = uncheckedDowncast<ScaleTransformOperation>(operation);
        if (!scale.is3DOperation()) {
            if (scale.x() == scale.y())
                return CSSFunctionValue::create(CSSValueScale, CSSPrimitiveValue::create(scale.x()));
            return CSSFunctionValue::create(CSSValueScale, CSSPrimitiveValue::create(scale.x()),
                CSSPrimitiveValue::create(scale.y()));
        }
        return CSSFunctionValue::create(CSSValueScale3d,
            CSSPrimitiveValue::create(scale.x()),
            CSSPrimitiveValue::create(scale.y()),
            CSSPrimitiveValue::create(scale.z()));
    }
    case TransformOperation::Type::RotateX:
        return CSSFunctionValue::create(CSSValueRotateX, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::RotateY:
        return CSSFunctionValue::create(CSSValueRotateY, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::RotateZ:
        return CSSFunctionValue::create(CSSValueRotateZ, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::Rotate:
        return CSSFunctionValue::create(CSSValueRotate, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::Rotate3D: {
        auto& rotate = uncheckedDowncast<RotateTransformOperation>(operation);
        return CSSFunctionValue::create(CSSValueRotate3d, CSSPrimitiveValue::create(rotate.x()), CSSPrimitiveValue::create(rotate.y()), CSSPrimitiveValue::create(rotate.z()), CSSPrimitiveValue::create(rotate.angle(), CSSUnitType::CSS_DEG));
    }
    case TransformOperation::Type::SkewX:
        return CSSFunctionValue::create(CSSValueSkewX, CSSPrimitiveValue::create(uncheckedDowncast<SkewTransformOperation>(operation).angleX(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::SkewY:
        return CSSFunctionValue::create(CSSValueSkewY, CSSPrimitiveValue::create(uncheckedDowncast<SkewTransformOperation>(operation).angleY(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::Skew: {
        auto& skew = uncheckedDowncast<SkewTransformOperation>(operation);
        if (!skew.angleY())
            return CSSFunctionValue::create(CSSValueSkew, CSSPrimitiveValue::create(skew.angleX(), CSSUnitType::CSS_DEG));
        return CSSFunctionValue::create(CSSValueSkew, CSSPrimitiveValue::create(skew.angleX(), CSSUnitType::CSS_DEG),
            CSSPrimitiveValue::create(skew.angleY(), CSSUnitType::CSS_DEG));
    }
    case TransformOperation::Type::Perspective:
        if (auto perspective = uncheckedDowncast<PerspectiveTransformOperation>(operation).perspective())
            return CSSFunctionValue::create(CSSValuePerspective, convertLength(style, *perspective));
        return CSSFunctionValue::create(CSSValuePerspective, CSSPrimitiveValue::create(CSSValueNone));
    case TransformOperation::Type::Matrix:
    case TransformOperation::Type::Matrix3D: {
        TransformationMatrix transform;
        operation.apply(transform, { });
        return convertTransformationMatrix(style, transform);
    }
    case TransformOperation::Type::Identity:
    case TransformOperation::Type::None:
        break;
    }

    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueNone);
}

// MARK: - Shared conversions

inline Ref<CSSValue> ExtractorConverter::convertOpacity(ExtractorState& state, float opacity)
{
    return convert(state, opacity);
}

inline Ref<CSSValue> ExtractorConverter::convertImageOrNone(ExtractorState& state, const StyleImage* image)
{
    if (image)
        return image->computedStyleValue(state.style);
    return CSSPrimitiveValue::create(CSSValueNone);
}

inline Ref<CSSValue> ExtractorConverter::convertGlyphOrientation(ExtractorState&, GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        return CSSPrimitiveValue::create(0.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees90:
        return CSSPrimitiveValue::create(90.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees180:
        return CSSPrimitiveValue::create(180.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees270:
        return CSSPrimitiveValue::create(270.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Auto:
        ASSERT_NOT_REACHED();
        return CSSPrimitiveValue::create(0.0f, CSSUnitType::CSS_DEG);
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorConverter::convertGlyphOrientationOrAuto(ExtractorState&, GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        return CSSPrimitiveValue::create(0.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees90:
        return CSSPrimitiveValue::create(90.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees180:
        return CSSPrimitiveValue::create(180.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees270:
        return CSSPrimitiveValue::create(270.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Auto:
        return CSSPrimitiveValue::create(CSSValueAuto);
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorConverter::convertMarginTrim(ExtractorState&, OptionSet<MarginTrimType> marginTrim)
{
    if (marginTrim.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    // Try to serialize into one of the "block" or "inline" shorthands
    if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd }) && !marginTrim.containsAny({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd }))
        return CSSPrimitiveValue::create(CSSValueBlock);
    if (marginTrim.containsAll({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd }) && !marginTrim.containsAny({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd }))
        return CSSPrimitiveValue::create(CSSValueInline);
    if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd, MarginTrimType::InlineStart, MarginTrimType::InlineEnd }))
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueBlock), CSSPrimitiveValue::create(CSSValueInline));

    CSSValueListBuilder list;
    if (marginTrim.contains(MarginTrimType::BlockStart))
        list.append(CSSPrimitiveValue::create(CSSValueBlockStart));
    if (marginTrim.contains(MarginTrimType::InlineStart))
        list.append(CSSPrimitiveValue::create(CSSValueInlineStart));
    if (marginTrim.contains(MarginTrimType::BlockEnd))
        list.append(CSSPrimitiveValue::create(CSSValueBlockEnd));
    if (marginTrim.contains(MarginTrimType::InlineEnd))
        list.append(CSSPrimitiveValue::create(CSSValueInlineEnd));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertShapeValue(ExtractorState& state, const ShapeValue* shapeValue)
{
    if (!shapeValue)
        return CSSPrimitiveValue::create(CSSValueNone);

    if (shapeValue->type() == ShapeValue::Type::Box)
        return convert(state, shapeValue->cssBox());

    if (shapeValue->type() == ShapeValue::Type::Image)
        return convertImageOrNone(state, shapeValue->image());

    ASSERT(shapeValue->type() == ShapeValue::Type::Shape);
    if (shapeValue->cssBox() == CSSBoxType::BoxMissing)
        return CSSValueList::createSpaceSeparated(convertStyleType(state, *shapeValue->shape()));
    return CSSValueList::createSpaceSeparated(convertStyleType(state, *shapeValue->shape()), convert(state, shapeValue->cssBox()));
}

inline Ref<CSSValue> ExtractorConverter::convertDPath(ExtractorState& state, const StylePathData* path)
{
    if (!path)
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSPathValue::create(toCSS(Ref { *path }->path(), state.style, PathConversion::ForceAbsolute));
}

inline Ref<CSSValue> ExtractorConverter::convertStrokeDashArray(ExtractorState& state, const FixedVector<WebCore::Length>& dashes)
{
    if (dashes.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    CSSValueListBuilder list;
    for (auto& dash : dashes)
        list.append(convertLength(state, dash));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertTextStrokeWidth(ExtractorState& state, float textStrokeWidth)
{
    return convertNumberAsPixels(state, textStrokeWidth);
}

inline Ref<CSSValue> ExtractorConverter::convertFilterOperations(ExtractorState& state, const FilterOperations& filterOperations)
{
    return CSSFilterPropertyValue::create(toCSSFilterProperty(filterOperations, state.style));
}

inline Ref<CSSValue> ExtractorConverter::convertAppleColorFilterOperations(ExtractorState& state, const FilterOperations& filterOperations)
{
    return CSSAppleColorFilterPropertyValue::create(toCSSAppleColorFilterProperty(filterOperations, state.style));
}

inline Ref<CSSValue> ExtractorConverter::convertWebkitTextCombine(ExtractorState& state, TextCombine textCombine)
{
    if (textCombine == TextCombine::All)
        return CSSPrimitiveValue::create(CSSValueHorizontal);
    return convert(state, textCombine);
}

inline Ref<CSSValue> ExtractorConverter::convertImageOrientation(ExtractorState&, ImageOrientation imageOrientation)
{
    if (imageOrientation == ImageOrientation::Orientation::FromImage)
        return CSSPrimitiveValue::create(CSSValueFromImage);
    return CSSPrimitiveValue::create(CSSValueNone);
}

inline Ref<CSSValue> ExtractorConverter::convertLineClamp(ExtractorState&, const LineClampValue& lineClamp)
{
    if (lineClamp.isNone())
        return CSSPrimitiveValue::create(CSSValueNone);
    if (lineClamp.isPercentage())
        return CSSPrimitiveValue::create(lineClamp.value(), CSSUnitType::CSS_PERCENTAGE);
    return CSSPrimitiveValue::createInteger(lineClamp.value());
}

inline Ref<CSSValue> ExtractorConverter::convertContain(ExtractorState&, OptionSet<Containment> containment)
{
    if (!containment)
        return CSSPrimitiveValue::create(CSSValueNone);
    if (containment == RenderStyle::strictContainment())
        return CSSPrimitiveValue::create(CSSValueStrict);
    if (containment == RenderStyle::contentContainment())
        return CSSPrimitiveValue::create(CSSValueContent);
    CSSValueListBuilder list;
    if (containment & Containment::Size)
        list.append(CSSPrimitiveValue::create(CSSValueSize));
    if (containment & Containment::InlineSize)
        list.append(CSSPrimitiveValue::create(CSSValueInlineSize));
    if (containment & Containment::Layout)
        list.append(CSSPrimitiveValue::create(CSSValueLayout));
    if (containment & Containment::Style)
        list.append(CSSPrimitiveValue::create(CSSValueStyle));
    if (containment & Containment::Paint)
        list.append(CSSPrimitiveValue::create(CSSValuePaint));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertMaxLines(ExtractorState&, size_t maxLines)
{
    if (!maxLines)
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSPrimitiveValue::create(maxLines);
}

inline Ref<CSSValue> ExtractorConverter::convertSmoothScrolling(ExtractorState&, bool useSmoothScrolling)
{
    if (useSmoothScrolling)
        return CSSPrimitiveValue::create(CSSValueSmooth);
    return CSSPrimitiveValue::create(CSSValueAuto);
}

inline Ref<CSSValue> ExtractorConverter::convertInitialLetter(ExtractorState&, IntSize initialLetter)
{
    return CSSValuePair::create(
        !initialLetter.width() ? CSSPrimitiveValue::create(CSSValueNormal) : CSSPrimitiveValue::create(initialLetter.width()),
        !initialLetter.height() ? CSSPrimitiveValue::create(CSSValueNormal) : CSSPrimitiveValue::create(initialLetter.height())
    );
}

inline Ref<CSSValue> ExtractorConverter::convertTextSpacingTrim(ExtractorState&, TextSpacingTrim textSpacingTrim)
{
    switch (textSpacingTrim.type()) {
    case TextSpacingTrim::TrimType::SpaceAll:
        return CSSPrimitiveValue::create(CSSValueSpaceAll);
    case TextSpacingTrim::TrimType::Auto:
        return CSSPrimitiveValue::create(CSSValueAuto);
    case TextSpacingTrim::TrimType::TrimAll:
        return CSSPrimitiveValue::create(CSSValueTrimAll);
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return CSSPrimitiveValue::create(CSSValueSpaceAll);
}

inline Ref<CSSValue> ExtractorConverter::convertTextAutospace(ExtractorState&, TextAutospace textAutospace)
{
    if (textAutospace.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (textAutospace.isNoAutospace())
        return CSSPrimitiveValue::create(CSSValueNoAutospace);
    if (textAutospace.isNormal())
        return CSSPrimitiveValue::create(CSSValueNormal);

    CSSValueListBuilder list;
    if (textAutospace.hasIdeographAlpha())
        list.append(CSSPrimitiveValue::create(CSSValueIdeographAlpha));
    if (textAutospace.hasIdeographNumeric())
        list.append(CSSPrimitiveValue::create(CSSValueIdeographNumeric));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertReflection(ExtractorState& state, const StyleReflection* reflection)
{
    if (!reflection)
        return CSSPrimitiveValue::create(CSSValueNone);

    // FIXME: Consider omitting 0px when the mask is null.

    auto offset = [&] -> Ref<CSSPrimitiveValue> {
        auto& reflectionOffset = reflection->offset();
        if (reflectionOffset.isPercentOrCalculated())
            return CSSPrimitiveValue::create(reflectionOffset.percent(), CSSUnitType::CSS_PERCENTAGE);
        return convertNumberAsPixels(state, reflectionOffset.value());
    }();

    auto mask = [&] -> RefPtr<CSSValue> {
        auto& reflectionMask = reflection->mask();
        RefPtr reflectionMaskImageSource = reflectionMask.image();
        if (!reflectionMaskImageSource)
            return CSSPrimitiveValue::create(CSSValueNone);
        if (reflectionMask.overridesBorderWidths())
            return nullptr;
        return convertNinePieceImage(state, reflectionMask);
    }();

    return CSSReflectValue::create(
        toCSSValueID(reflection->direction()),
        WTFMove(offset),
        WTFMove(mask)
    );
}

inline Ref<CSSValue> ExtractorConverter::convertLineFitEdge(ExtractorState& state, const TextEdge& textEdge)
{
    if (textEdge.over == TextEdgeType::Leading && textEdge.under == TextEdgeType::Leading)
        return convert(state, textEdge.over);

    // https://www.w3.org/TR/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeUnderEdge = [&] {
        if (textEdge.over == TextEdgeType::CapHeight || textEdge.over == TextEdgeType::ExHeight)
            return textEdge.under != TextEdgeType::Text;
        return textEdge.over != textEdge.under;
    }();

    if (!shouldSerializeUnderEdge)
        return convert(state, textEdge.over);

    return CSSValuePair::create(convert(state, textEdge.over), convert(state, textEdge.under));
}

inline Ref<CSSValue> ExtractorConverter::convertTextBoxEdge(ExtractorState& state, const TextEdge& textEdge)
{
    if (textEdge.over == TextEdgeType::Auto && textEdge.under == TextEdgeType::Auto)
        return convert(state, textEdge.over);

    // https://www.w3.org/TR/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeUnderEdge = [&] {
        if (textEdge.over == TextEdgeType::CapHeight || textEdge.over == TextEdgeType::ExHeight)
            return textEdge.under != TextEdgeType::Text;
        return textEdge.over != textEdge.under;
    }();

    if (!shouldSerializeUnderEdge)
        return convert(state, textEdge.over);

    return CSSValuePair::create(convert(state, textEdge.over), convert(state, textEdge.under));
}

inline Ref<CSSValue> ExtractorConverter::convertQuotes(ExtractorState&, const QuotesData* quotes)
{
    if (!quotes)
        return CSSPrimitiveValue::create(CSSValueAuto);
    unsigned size = quotes->size();
    if (!size)
        return CSSPrimitiveValue::create(CSSValueNone);
    CSSValueListBuilder list;
    for (unsigned i = 0; i < size; ++i) {
        list.append(CSSPrimitiveValue::create(quotes->openQuote(i)));
        list.append(CSSPrimitiveValue::create(quotes->closeQuote(i)));
    }
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertViewTransitionName(ExtractorState&, const ViewTransitionName& viewTransitionName)
{
    if (viewTransitionName.isNone())
        return CSSPrimitiveValue::create(CSSValueNone);
    if (viewTransitionName.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSPrimitiveValue::createCustomIdent(viewTransitionName.customIdent());
}


inline Ref<CSSValue> ExtractorConverter::convertPositionTryFallbacks(ExtractorState& state, const FixedVector<PositionTryFallback>& fallbacks)
{
    if (fallbacks.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& fallback : fallbacks) {
        if (fallback.positionAreaProperties) {
            auto areaValue = fallback.positionAreaProperties->getPropertyCSSValue(CSSPropertyPositionArea);
            if (areaValue)
                list.append(*areaValue);
            continue;
        }

        CSSValueListBuilder singleFallbackList;
        if (fallback.positionTryRuleName)
            singleFallbackList.append(convert(state, *fallback.positionTryRuleName));
        for (auto& tactic : fallback.tactics)
            singleFallbackList.append(convert(state, tactic));
        list.append(CSSValueList::createSpaceSeparated(singleFallbackList));
    }

    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertWillChange(ExtractorState&, const WillChangeData* willChangeData)
{
    if (!willChangeData || !willChangeData->numFeatures())
        return CSSPrimitiveValue::create(CSSValueAuto);

    CSSValueListBuilder list;
    for (size_t i = 0; i < willChangeData->numFeatures(); ++i) {
        auto feature = willChangeData->featureAt(i);
        switch (feature.first) {
        case WillChangeData::Feature::ScrollPosition:
            list.append(CSSPrimitiveValue::create(CSSValueScrollPosition));
            break;
        case WillChangeData::Feature::Contents:
            list.append(CSSPrimitiveValue::create(CSSValueContents));
            break;
        case WillChangeData::Feature::Property:
            list.append(CSSPrimitiveValue::create(feature.second));
            break;
        case WillChangeData::Feature::Invalid:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertBlockStepSize(ExtractorState& state, std::optional<WebCore::Length> blockStepSize)
{
    if (blockStepSize)
        return convertLength(state, *blockStepSize);
    return CSSPrimitiveValue::create(CSSValueNone);
}

inline Ref<CSSValue> ExtractorConverter::convertTabSize(ExtractorState&, const TabSize& tabSize)
{
    return CSSPrimitiveValue::create(tabSize.widthInPixels(1.0), tabSize.isSpaces() ? CSSUnitType::CSS_NUMBER : CSSUnitType::CSS_PX);
}

inline Ref<CSSValue> ExtractorConverter::convertScrollSnapType(ExtractorState& state, const ScrollSnapType& type)
{
    if (type.strictness == ScrollSnapStrictness::None)
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueNone));
    if (type.strictness == ScrollSnapStrictness::Proximity)
        return CSSValueList::createSpaceSeparated(convert(state, type.axis));
    return CSSValueList::createSpaceSeparated(convert(state, type.axis), convert(state, type.strictness));
}

inline Ref<CSSValue> ExtractorConverter::convertScrollSnapAlign(ExtractorState& state, const ScrollSnapAlign& alignment)
{
    return CSSValuePair::create(
        convert(state, alignment.blockAlign),
        convert(state, alignment.inlineAlign)
    );
}

inline Ref<CSSValue> ExtractorConverter::convertScrollbarColor(ExtractorState& state, std::optional<ScrollbarColor> scrollbarColor)
{
    if (!scrollbarColor)
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSValuePair::createNoncoalescing(
        convertStyleType(state, scrollbarColor->thumbColor),
        convertStyleType(state, scrollbarColor->trackColor)
    );
}

inline Ref<CSSValue> ExtractorConverter::convertLineBoxContain(ExtractorState&, OptionSet<Style::LineBoxContain> lineBoxContain)
{
    if (!lineBoxContain)
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    if (lineBoxContain.contains(LineBoxContain::Block))
        list.append(CSSPrimitiveValue::create(CSSValueBlock));
    if (lineBoxContain.contains(LineBoxContain::Inline))
        list.append(CSSPrimitiveValue::create(CSSValueInline));
    if (lineBoxContain.contains(LineBoxContain::Font))
        list.append(CSSPrimitiveValue::create(CSSValueFont));
    if (lineBoxContain.contains(LineBoxContain::Glyphs))
        list.append(CSSPrimitiveValue::create(CSSValueGlyphs));
    if (lineBoxContain.contains(LineBoxContain::Replaced))
        list.append(CSSPrimitiveValue::create(CSSValueReplaced));
    if (lineBoxContain.contains(LineBoxContain::InlineBox))
        list.append(CSSPrimitiveValue::create(CSSValueInlineBox));
    if (lineBoxContain.contains(LineBoxContain::InitialLetter))
        list.append(CSSPrimitiveValue::create(CSSValueInitialLetter));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertWebkitRubyPosition(ExtractorState&, RubyPosition position)
{
    return CSSPrimitiveValue::create([&] {
        switch (position) {
        case RubyPosition::Over:
            return CSSValueBefore;
        case RubyPosition::Under:
            return CSSValueAfter;
        case RubyPosition::InterCharacter:
        case RubyPosition::LegacyInterCharacter:
            return CSSValueInterCharacter;
        }
        return CSSValueBefore;
    }());
}

inline Ref<CSSValue> ExtractorConverter::convertPosition(ExtractorState& state, const LengthPoint& position)
{
    return CSSValueList::createSpaceSeparated(
        convertLength(state, position.x),
        convertLength(state, position.y)
    );
}

inline Ref<CSSValue> ExtractorConverter::convertTouchAction(ExtractorState&, OptionSet<TouchAction> touchActions)
{
    if (touchActions & TouchAction::Auto)
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (touchActions & TouchAction::None)
        return CSSPrimitiveValue::create(CSSValueNone);
    if (touchActions & TouchAction::Manipulation)
        return CSSPrimitiveValue::create(CSSValueManipulation);

    CSSValueListBuilder list;
    if (touchActions & TouchAction::PanX)
        list.append(CSSPrimitiveValue::create(CSSValuePanX));
    if (touchActions & TouchAction::PanY)
        list.append(CSSPrimitiveValue::create(CSSValuePanY));
    if (touchActions & TouchAction::PinchZoom)
        list.append(CSSPrimitiveValue::create(CSSValuePinchZoom));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertTextTransform(ExtractorState&, OptionSet<TextTransform> textTransform)
{
    CSSValueListBuilder list;
    if (textTransform.contains(TextTransform::Capitalize))
        list.append(CSSPrimitiveValue::create(CSSValueCapitalize));
    else if (textTransform.contains(TextTransform::Uppercase))
        list.append(CSSPrimitiveValue::create(CSSValueUppercase));
    else if (textTransform.contains(TextTransform::Lowercase))
        list.append(CSSPrimitiveValue::create(CSSValueLowercase));

    if (textTransform.contains(TextTransform::FullWidth))
        list.append(CSSPrimitiveValue::create(CSSValueFullWidth));

    if (textTransform.contains(TextTransform::FullSizeKana))
        list.append(CSSPrimitiveValue::create(CSSValueFullSizeKana));

    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertTextDecorationLine(ExtractorState&, OptionSet<TextDecorationLine> textDecorationLine)
{
    // Blink value is ignored.
    CSSValueListBuilder list;
    if (textDecorationLine & TextDecorationLine::Underline)
        list.append(CSSPrimitiveValue::create(CSSValueUnderline));
    if (textDecorationLine & TextDecorationLine::Overline)
        list.append(CSSPrimitiveValue::create(CSSValueOverline));
    if (textDecorationLine & TextDecorationLine::LineThrough)
        list.append(CSSPrimitiveValue::create(CSSValueLineThrough));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertTextUnderlinePosition(ExtractorState&, OptionSet<TextUnderlinePosition> textUnderlinePosition)
{
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::FromFont) && (textUnderlinePosition & TextUnderlinePosition::Under)));
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::Left) && (textUnderlinePosition & TextUnderlinePosition::Right)));

    if (textUnderlinePosition.isEmpty())
        return CSSPrimitiveValue::create(CSSValueAuto);
    bool isFromFont = textUnderlinePosition.contains(TextUnderlinePosition::FromFont);
    bool isUnder = textUnderlinePosition.contains(TextUnderlinePosition::Under);
    bool isLeft = textUnderlinePosition.contains(TextUnderlinePosition::Left);
    bool isRight = textUnderlinePosition.contains(TextUnderlinePosition::Right);

    auto metric = isUnder ? CSSValueUnder : CSSValueFromFont;
    auto side = isLeft ? CSSValueLeft : CSSValueRight;
    if (!isFromFont && !isUnder)
        return CSSPrimitiveValue::create(side);
    if (!isLeft && !isRight)
        return CSSPrimitiveValue::create(metric);
    return CSSValuePair::create(CSSPrimitiveValue::create(metric), CSSPrimitiveValue::create(side));
}

inline Ref<CSSValue> ExtractorConverter::convertTextDecorationThickness(ExtractorState& state, const TextDecorationThickness& textDecorationThickness)
{
    if (textDecorationThickness.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (textDecorationThickness.isFromFont())
        return CSSPrimitiveValue::create(CSSValueFromFont);

    ASSERT(textDecorationThickness.isLength());
    auto& length = textDecorationThickness.length();
    if (length.isPercent())
        return CSSPrimitiveValue::create(length.percent(), CSSUnitType::CSS_PERCENTAGE);
    return CSSPrimitiveValue::create(length, state.style);
}

inline Ref<CSSValue> ExtractorConverter::convertTextEmphasisPosition(ExtractorState&, OptionSet<TextEmphasisPosition> textEmphasisPosition)
{
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Over) && (textEmphasisPosition & TextEmphasisPosition::Under)));
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Left) && (textEmphasisPosition & TextEmphasisPosition::Right)));
    ASSERT((textEmphasisPosition & TextEmphasisPosition::Over) || (textEmphasisPosition & TextEmphasisPosition::Under));

    CSSValueListBuilder list;
    if (textEmphasisPosition & TextEmphasisPosition::Over)
        list.append(CSSPrimitiveValue::create(CSSValueOver));
    if (textEmphasisPosition & TextEmphasisPosition::Under)
        list.append(CSSPrimitiveValue::create(CSSValueUnder));
    if (textEmphasisPosition & TextEmphasisPosition::Left)
        list.append(CSSPrimitiveValue::create(CSSValueLeft));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertSpeakAs(ExtractorState&, OptionSet<SpeakAs> speakAs)
{
    CSSValueListBuilder list;
    if (speakAs & SpeakAs::SpellOut)
        list.append(CSSPrimitiveValue::create(CSSValueSpellOut));
    if (speakAs & SpeakAs::Digits)
        list.append(CSSPrimitiveValue::create(CSSValueDigits));
    if (speakAs & SpeakAs::LiteralPunctuation)
        list.append(CSSPrimitiveValue::create(CSSValueLiteralPunctuation));
    if (speakAs & SpeakAs::NoPunctuation)
        list.append(CSSPrimitiveValue::create(CSSValueNoPunctuation));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNormal);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertHangingPunctuation(ExtractorState&, OptionSet<HangingPunctuation> hangingPunctuation)
{
    CSSValueListBuilder list;
    if (hangingPunctuation & HangingPunctuation::First)
        list.append(CSSPrimitiveValue::create(CSSValueFirst));
    if (hangingPunctuation & HangingPunctuation::AllowEnd)
        list.append(CSSPrimitiveValue::create(CSSValueAllowEnd));
    if (hangingPunctuation & HangingPunctuation::ForceEnd)
        list.append(CSSPrimitiveValue::create(CSSValueForceEnd));
    if (hangingPunctuation & HangingPunctuation::Last)
        list.append(CSSPrimitiveValue::create(CSSValueLast));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertPageBreak(ExtractorState&, BreakBetween value)
{
    if (value == BreakBetween::Page || value == BreakBetween::LeftPage || value == BreakBetween::RightPage
        || value == BreakBetween::RectoPage || value == BreakBetween::VersoPage)
        return CSSPrimitiveValue::create(CSSValueAlways); // CSS 2.1 allows us to map these to always.
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidPage)
        return CSSPrimitiveValue::create(CSSValueAvoid);
    return CSSPrimitiveValue::create(CSSValueAuto);
}

inline Ref<CSSValue> ExtractorConverter::convertPageBreak(ExtractorState&, BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidPage)
        return CSSPrimitiveValue::create(CSSValueAvoid);
    return CSSPrimitiveValue::create(CSSValueAuto);
}

inline Ref<CSSValue> ExtractorConverter::convertWebkitColumnBreak(ExtractorState&, BreakBetween value)
{
    if (value == BreakBetween::Column)
        return CSSPrimitiveValue::create(CSSValueAlways);
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidColumn)
        return CSSPrimitiveValue::create(CSSValueAvoid);
    return CSSPrimitiveValue::create(CSSValueAuto);
}

inline Ref<CSSValue> ExtractorConverter::convertWebkitColumnBreak(ExtractorState&, BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidColumn)
        return CSSPrimitiveValue::create(CSSValueAvoid);
    return CSSPrimitiveValue::create(CSSValueAuto);
}

inline Ref<CSSValue> ExtractorConverter::convertSelfOrDefaultAlignmentData(ExtractorState& state, const StyleSelfAlignmentData& data)
{
    CSSValueListBuilder list;
    if (data.positionType() == ItemPositionType::Legacy)
        list.append(CSSPrimitiveValue::create(CSSValueLegacy));
    if (data.position() == ItemPosition::Baseline)
        list.append(CSSPrimitiveValue::create(CSSValueBaseline));
    else if (data.position() == ItemPosition::LastBaseline) {
        list.append(CSSPrimitiveValue::create(CSSValueLast));
        list.append(CSSPrimitiveValue::create(CSSValueBaseline));
    } else {
        if (data.position() >= ItemPosition::Center && data.overflow() != OverflowAlignment::Default)
            list.append(convert(state, data.overflow()));
        if (data.position() == ItemPosition::Legacy)
            list.append(CSSPrimitiveValue::create(CSSValueNormal));
        else
            list.append(convert(state, data.position()));
    }
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertContentAlignmentData(ExtractorState& state, const StyleContentAlignmentData& data)
{
    CSSValueListBuilder list;

    // Handle content-distribution values
    if (data.distribution() != ContentDistribution::Default)
        list.append(convert(state, data.distribution()));

    // Handle content-position values (either as fallback or actual value)
    switch (data.position()) {
    case ContentPosition::Normal:
        // Handle 'normal' value, not valid as content-distribution fallback.
        if (data.distribution() == ContentDistribution::Default)
            list.append(CSSPrimitiveValue::create(CSSValueNormal));
        break;
    case ContentPosition::LastBaseline:
        list.append(CSSPrimitiveValue::create(CSSValueLast));
        list.append(CSSPrimitiveValue::create(CSSValueBaseline));
        break;
    default:
        // Handle overflow-alignment (only allowed for content-position values)
        if ((data.position() >= ContentPosition::Center || data.distribution() != ContentDistribution::Default) && data.overflow() != OverflowAlignment::Default)
            list.append(convert(state, data.overflow()));
        list.append(convert(state, data.position()));
    }

    ASSERT(list.size() > 0);
    ASSERT(list.size() <= 3);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertPaintOrder(ExtractorState&, PaintOrder paintOrder)
{
    if (paintOrder == PaintOrder::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    CSSValueListBuilder paintOrderList;
    switch (paintOrder) {
    case PaintOrder::Normal:
        ASSERT_NOT_REACHED();
        break;
    case PaintOrder::Fill:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueFill));
        break;
    case PaintOrder::FillMarkers:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueFill));
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueMarkers));
        break;
    case PaintOrder::Stroke:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueStroke));
        break;
    case PaintOrder::StrokeMarkers:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueStroke));
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueMarkers));
        break;
    case PaintOrder::Markers:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueMarkers));
        break;
    case PaintOrder::MarkersStroke:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueMarkers));
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueStroke));
        break;
    }
    return CSSValueList::createSpaceSeparated(WTFMove(paintOrderList));
}

inline Ref<CSSValue> ExtractorConverter::convertPositionAnchor(ExtractorState& state, const std::optional<ScopedName>& positionAnchor)
{
    if (!positionAnchor)
        return CSSPrimitiveValue::create(CSSValueAuto);
    return convert(state, *positionAnchor);
}

inline Ref<CSSValue> ExtractorConverter::convertPositionArea(ExtractorState&, const PositionArea& positionArea)
{
    auto keywordForPositionAreaSpan = [](const PositionAreaSpan span) -> CSSValueID {
        auto axis = span.axis();
        auto track = span.track();
        auto self = span.self();

        switch (axis) {
        case PositionAreaAxis::Horizontal:
            ASSERT(self == PositionAreaSelf::No);
            switch (track) {
            case PositionAreaTrack::Start:
                return CSSValueLeft;
            case PositionAreaTrack::SpanStart:
                return CSSValueSpanLeft;
            case PositionAreaTrack::End:
                return CSSValueRight;
            case PositionAreaTrack::SpanEnd:
                return CSSValueSpanRight;
            case PositionAreaTrack::Center:
                return CSSValueCenter;
            case PositionAreaTrack::SpanAll:
                return CSSValueSpanAll;
            default:
                ASSERT_NOT_REACHED();
                return CSSValueLeft;
            }

        case PositionAreaAxis::Vertical:
            ASSERT(self == PositionAreaSelf::No);
            switch (track) {
            case PositionAreaTrack::Start:
                return CSSValueTop;
            case PositionAreaTrack::SpanStart:
                return CSSValueSpanTop;
            case PositionAreaTrack::End:
                return CSSValueBottom;
            case PositionAreaTrack::SpanEnd:
                return CSSValueSpanBottom;
            case PositionAreaTrack::Center:
                return CSSValueCenter;
            case PositionAreaTrack::SpanAll:
                return CSSValueSpanAll;
            default:
                ASSERT_NOT_REACHED();
                return CSSValueTop;
            }

        case PositionAreaAxis::X:
            switch (track) {
            case PositionAreaTrack::Start:
                return self == PositionAreaSelf::No ? CSSValueXStart : CSSValueXSelfStart;
            case PositionAreaTrack::SpanStart:
                return self == PositionAreaSelf::No ? CSSValueSpanXStart : CSSValueSpanXSelfStart;
            case PositionAreaTrack::End:
                return self == PositionAreaSelf::No ? CSSValueXEnd : CSSValueXSelfEnd;
            case PositionAreaTrack::SpanEnd:
                return self == PositionAreaSelf::No ? CSSValueSpanXEnd : CSSValueSpanXSelfEnd;
            case PositionAreaTrack::Center:
                return CSSValueCenter;
            case PositionAreaTrack::SpanAll:
                return CSSValueSpanAll;
            default:
                ASSERT_NOT_REACHED();
                return CSSValueXStart;
            }

        case PositionAreaAxis::Y:
            switch (track) {
            case PositionAreaTrack::Start:
                return self == PositionAreaSelf::No ? CSSValueYStart : CSSValueYSelfStart;
            case PositionAreaTrack::SpanStart:
                return self == PositionAreaSelf::No ? CSSValueSpanYStart : CSSValueSpanYSelfStart;
            case PositionAreaTrack::End:
                return self == PositionAreaSelf::No ? CSSValueYEnd : CSSValueYSelfEnd;
            case PositionAreaTrack::SpanEnd:
                return self == PositionAreaSelf::No ? CSSValueSpanYEnd : CSSValueSpanYSelfEnd;
            case PositionAreaTrack::Center:
                return CSSValueCenter;
            case PositionAreaTrack::SpanAll:
                return CSSValueSpanAll;
            default:
                ASSERT_NOT_REACHED();
                return CSSValueYStart;
            }

        case PositionAreaAxis::Block:
            switch (track) {
            case PositionAreaTrack::Start:
                return self == PositionAreaSelf::No ? CSSValueBlockStart : CSSValueSelfBlockStart;
            case PositionAreaTrack::SpanStart:
                return self == PositionAreaSelf::No ? CSSValueSpanBlockStart : CSSValueSpanSelfBlockStart;
            case PositionAreaTrack::End:
                return self == PositionAreaSelf::No ? CSSValueBlockEnd : CSSValueSelfBlockEnd;
            case PositionAreaTrack::SpanEnd:
                return self == PositionAreaSelf::No ? CSSValueSpanBlockEnd : CSSValueSpanSelfBlockEnd;
            case PositionAreaTrack::Center:
                return CSSValueCenter;
            case PositionAreaTrack::SpanAll:
                return CSSValueSpanAll;
            default:
                ASSERT_NOT_REACHED();
                return CSSValueBlockStart;
            }

        case PositionAreaAxis::Inline:
            switch (track) {
            case PositionAreaTrack::Start:
                return self == PositionAreaSelf::No ? CSSValueInlineStart : CSSValueSelfInlineStart;
            case PositionAreaTrack::SpanStart:
                return self == PositionAreaSelf::No ? CSSValueSpanInlineStart : CSSValueSpanSelfInlineStart;
            case PositionAreaTrack::End:
                return self == PositionAreaSelf::No ? CSSValueInlineEnd : CSSValueSelfInlineEnd;
            case PositionAreaTrack::SpanEnd:
                return self == PositionAreaSelf::No ? CSSValueSpanInlineEnd : CSSValueSpanSelfInlineEnd;
            case PositionAreaTrack::Center:
                return CSSValueCenter;
            case PositionAreaTrack::SpanAll:
                return CSSValueSpanAll;
            default:
                ASSERT_NOT_REACHED();
                return CSSValueInlineStart;
            }
        }

        ASSERT_NOT_REACHED();
        return CSSValueLeft;
    };

    auto blockOrXAxisKeyword = keywordForPositionAreaSpan(positionArea.blockOrXAxis());
    auto inlineOrYAxisKeyword = keywordForPositionAreaSpan(positionArea.inlineOrYAxis());

    return CSSPropertyParserHelpers::valueForPositionArea(blockOrXAxisKeyword, inlineOrYAxisKeyword).releaseNonNull();
}

inline Ref<CSSValue> ExtractorConverter::convertPositionArea(ExtractorState& state, const std::optional<PositionArea>& positionArea)
{
    if (!positionArea)
        return CSSPrimitiveValue::create(CSSValueNone);
    return convertPositionArea(state, *positionArea);
}

inline Ref<CSSValue> ExtractorConverter::convertNameScope(ExtractorState&, const NameScope& scope)
{
    switch (scope.type) {
    case NameScope::Type::None:
        return CSSPrimitiveValue::create(CSSValueNone);

    case NameScope::Type::All:
        return CSSPrimitiveValue::create(CSSValueAll);

    case NameScope::Type::Ident:
        if (scope.names.isEmpty())
            return CSSPrimitiveValue::create(CSSValueNone);

        CSSValueListBuilder list;
        for (auto& name : scope.names) {
            ASSERT(!name.isNull());
            list.append(CSSPrimitiveValue::createCustomIdent(name));
        }

        return CSSValueList::createCommaSeparated(WTFMove(list));
    }

    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueNone);
}

inline Ref<CSSValue> ExtractorConverter::convertPositionVisibility(ExtractorState&, OptionSet<PositionVisibility> positionVisibility)
{
    CSSValueListBuilder list;
    if (positionVisibility & PositionVisibility::AnchorsValid)
        list.append(CSSPrimitiveValue::create(CSSValueAnchorsValid));
    if (positionVisibility & PositionVisibility::AnchorsVisible)
        list.append(CSSPrimitiveValue::create(CSSValueAnchorsVisible));
    if (positionVisibility & PositionVisibility::NoOverflow)
        list.append(CSSPrimitiveValue::create(CSSValueNoOverflow));

    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueAlways);

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

#if ENABLE(TEXT_AUTOSIZING)
inline Ref<CSSValue> ExtractorConverter::convertWebkitTextSizeAdjust(ExtractorState&, const TextSizeAdjustment& textSizeAdjust)
{
    if (textSizeAdjust.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (textSizeAdjust.isNone())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSPrimitiveValue::create(textSizeAdjust.percentage(), CSSUnitType::CSS_PERCENTAGE);
}
#endif

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
inline Ref<CSSValue> ExtractorConverter::convertOverflowScrolling(ExtractorState&, bool useTouchOverflowScrolling)
{
    return CSSPrimitiveValue::create(useTouchOverflowScrolling ? CSSValueTouch : CSSValueAuto);
}
#endif

#if PLATFORM(IOS_FAMILY)
inline Ref<CSSValue> ExtractorConverter::convertTouchCallout(ExtractorState&, bool touchCalloutEnabled)
{
    return CSSPrimitiveValue::create(touchCalloutEnabled ? CSSValueDefault : CSSValueNone);
}
#endif

// MARK: - FillLayer conversions

inline Ref<CSSValue> ExtractorConverter::convertFillLayerAttachment(ExtractorState& state, FillAttachment attachment)
{
    return convert(state, attachment);
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerBlendMode(ExtractorState& state, BlendMode blendMode)
{
    return convert(state, blendMode);
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerClip(ExtractorState& state, FillBox clip)
{
    return convert(state, clip);
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerOrigin(ExtractorState& state, FillBox origin)
{
    return convert(state, origin);
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerRepeat(ExtractorState& state, FillRepeatXY repeat)
{
    if (repeat.x == repeat.y)
        return convert(state, repeat.x);

    if (repeat.x == FillRepeat::Repeat && repeat.y == FillRepeat::NoRepeat)
        return CSSPrimitiveValue::create(CSSValueRepeatX);

    if (repeat.x == FillRepeat::NoRepeat && repeat.y == FillRepeat::Repeat)
        return CSSPrimitiveValue::create(CSSValueRepeatY);

    return CSSValueList::createSpaceSeparated(
        convert(state, repeat.x),
        convert(state, repeat.y)
    );
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerBackgroundSize(ExtractorState& state, FillSize size)
{
    if (size.type == FillSizeType::Contain)
        return CSSPrimitiveValue::create(CSSValueContain);

    if (size.type == FillSizeType::Cover)
        return CSSPrimitiveValue::create(CSSValueCover);

    if (size.size.height.isAuto() && size.size.width.isAuto())
        return convertLength(state, size.size.width);

    return CSSValueList::createSpaceSeparated(
        convertLength(state, size.size.width),
        convertLength(state, size.size.height)
    );
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerMaskSize(ExtractorState& state, FillSize size)
{
    if (size.type == FillSizeType::Contain)
        return CSSPrimitiveValue::create(CSSValueContain);

    if (size.type == FillSizeType::Cover)
        return CSSPrimitiveValue::create(CSSValueCover);

    if (size.size.height.isAuto())
        return convertLength(state, size.size.width);

    return CSSValueList::createSpaceSeparated(
        convertLength(state, size.size.width),
        convertLength(state, size.size.height)
    );
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerMaskComposite(ExtractorState&, CompositeOperator composite)
{
    return CSSPrimitiveValue::create(toCSSValueID(composite, CSSPropertyMaskComposite));
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerWebkitMaskComposite(ExtractorState&, CompositeOperator composite)
{
    return CSSPrimitiveValue::create(toCSSValueID(composite, CSSPropertyWebkitMaskComposite));
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerMaskMode(ExtractorState&, MaskMode maskMode)
{
    switch (maskMode) {
    case MaskMode::Alpha:
        return CSSPrimitiveValue::create(CSSValueAlpha);
    case MaskMode::Luminance:
        return CSSPrimitiveValue::create(CSSValueLuminance);
    case MaskMode::MatchSource:
        return CSSPrimitiveValue::create(CSSValueMatchSource);
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueMatchSource);
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerWebkitMaskSourceType(ExtractorState&, MaskMode maskMode)
{
    switch (maskMode) {
    case MaskMode::Alpha:
        return CSSPrimitiveValue::create(CSSValueAlpha);
    case MaskMode::Luminance:
        return CSSPrimitiveValue::create(CSSValueLuminance);
    case MaskMode::MatchSource:
        // MatchSource is only available in the mask-mode property.
        return CSSPrimitiveValue::create(CSSValueAlpha);
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueAlpha);
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerImage(ExtractorState& state, const StyleImage* image)
{
    return convertImageOrNone(state, image);
}

// MARK: - Font conversions

inline Ref<CSSValue> ExtractorConverter::convertFontFamily(ExtractorState& state, const AtomString& family)
{
    auto identifierForFamily = [](const auto& family) {
        if (family == cursiveFamily)
            return CSSValueCursive;
        if (family == fantasyFamily)
            return CSSValueFantasy;
        if (family == monospaceFamily)
            return CSSValueMonospace;
        if (family == pictographFamily)
            return CSSValueWebkitPictograph;
        if (family == sansSerifFamily)
            return CSSValueSansSerif;
        if (family == serifFamily)
            return CSSValueSerif;
        if (family == systemUiFamily)
            return CSSValueSystemUi;
        return CSSValueInvalid;
    };

    if (auto familyIdentifier = identifierForFamily(family))
        return CSSPrimitiveValue::create(familyIdentifier);
    return state.pool.createFontFamilyValue(family);
}

inline Ref<CSSValue> ExtractorConverter::convertFontSizeAdjust(ExtractorState& state, const FontSizeAdjust& fontSizeAdjust)
{
    if (fontSizeAdjust.isNone())
        return CSSPrimitiveValue::create(CSSValueNone);

    auto metric = fontSizeAdjust.metric;
    auto value = fontSizeAdjust.shouldResolveFromFont() ? fontSizeAdjust.resolve(state.style.computedFontSize(), state.style.metricsOfPrimaryFont()) : fontSizeAdjust.value.asOptional();
    if (!value)
        return CSSPrimitiveValue::create(CSSValueNone);

    if (metric == FontSizeAdjust::Metric::ExHeight)
        return CSSPrimitiveValue::create(*value);

    return CSSValuePair::create(convert(state, metric), CSSPrimitiveValue::create(*value));
}

inline Ref<CSSValue> ExtractorConverter::convertFontPalette(ExtractorState&, const FontPalette& fontPalette)
{
    switch (fontPalette.type) {
    case FontPalette::Type::Normal:
        return CSSPrimitiveValue::create(CSSValueNormal);
    case FontPalette::Type::Light:
        return CSSPrimitiveValue::create(CSSValueLight);
    case FontPalette::Type::Dark:
        return CSSPrimitiveValue::create(CSSValueDark);
    case FontPalette::Type::Custom:
        return CSSPrimitiveValue::createCustomIdent(fontPalette.identifier);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorConverter::convertFontWeight(ExtractorState&, FontSelectionValue fontWeight)
{
    return CSSPrimitiveValue::create(static_cast<float>(fontWeight));
}

inline Ref<CSSValue> ExtractorConverter::convertFontWidth(ExtractorState&, FontSelectionValue fontWidth)
{
    return CSSPrimitiveValue::create(static_cast<float>(fontWidth), CSSUnitType::CSS_PERCENTAGE);
}

inline Ref<CSSValue> ExtractorConverter::convertFontFeatureSettings(ExtractorState& state, const FontFeatureSettings& fontFeatureSettings)
{
    if (!fontFeatureSettings.size())
        return CSSPrimitiveValue::create(CSSValueNormal);
    CSSValueListBuilder list;
    for (auto& feature : fontFeatureSettings)
        list.append(CSSFontFeatureValue::create(FontTag(feature.tag()), convert(state, feature.value())));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertFontVariationSettings(ExtractorState& state, const FontVariationSettings& fontVariationSettings)
{
    if (fontVariationSettings.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNormal);
    CSSValueListBuilder list;
    for (auto& feature : fontVariationSettings)
        list.append(CSSFontVariationValue::create(feature.tag(), convert(state, feature.value())));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

// MARK: - NinePieceImage conversions

inline Ref<CSSValue> ExtractorConverter::convertNinePieceImageQuad(ExtractorState& state, const LengthBox& box)
{
    RefPtr<CSSPrimitiveValue> top;
    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;

    if (box.top().isRelative())
        top = CSSPrimitiveValue::create(box.top().value());
    else
        top = CSSPrimitiveValue::create(box.top(), state.style);

    if (box.right() == box.top() && box.bottom() == box.top() && box.left() == box.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (box.right().isRelative())
            right = CSSPrimitiveValue::create(box.right().value());
        else
            right = CSSPrimitiveValue::create(box.right(), state.style);

        if (box.bottom() == box.top() && box.right() == box.left()) {
            bottom = top;
            left = right;
        } else {
            if (box.bottom().isRelative())
                bottom = CSSPrimitiveValue::create(box.bottom().value());
            else
                bottom = CSSPrimitiveValue::create(box.bottom(), state.style);

            if (box.left() == box.right())
                left = right;
            else {
                if (box.left().isRelative())
                    left = CSSPrimitiveValue::create(box.left().value());
                else
                    left = CSSPrimitiveValue::create(box.left(), state.style);
            }
        }
    }

    return CSSQuadValue::create({
        top.releaseNonNull(),
        right.releaseNonNull(),
        bottom.releaseNonNull(),
        left.releaseNonNull()
    });
}

inline Ref<CSSValue> ExtractorConverter::convertNinePieceImageSlices(ExtractorState&, const NinePieceImage& image)
{
    auto sliceSide = [](const WebCore::Length& length) -> Ref<CSSPrimitiveValue> {
        // These values can be percentages or numbers.
        if (length.isPercent())
            return CSSPrimitiveValue::create(length.percent(), CSSUnitType::CSS_PERCENTAGE);
        ASSERT(length.isFixed());
        return CSSPrimitiveValue::create(length.value());
    };

    auto& slices = image.imageSlices();

    RefPtr<CSSPrimitiveValue> top = sliceSide(slices.top());
    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;
    if (slices.right() == slices.top() && slices.bottom() == slices.top() && slices.left() == slices.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        right = sliceSide(slices.right());
        if (slices.bottom() == slices.top() && slices.right() == slices.left()) {
            bottom = top;
            left = right;
        } else {
            bottom = sliceSide(slices.bottom());
            if (slices.left() == slices.right())
                left = right;
            else
                left = sliceSide(slices.left());
        }
    }

    return CSSBorderImageSliceValue::create({
        top.releaseNonNull(),
        right.releaseNonNull(),
        bottom.releaseNonNull(),
        left.releaseNonNull()
    }, image.fill());
}

inline Ref<CSSValue> ExtractorConverter::convertNinePieceImageRepeat(ExtractorState&, const NinePieceImage& image)
{
    auto valueID = [](NinePieceImageRule rule) -> CSSValueID {
        switch (rule) {
        case NinePieceImageRule::Repeat:
            return CSSValueRepeat;
        case NinePieceImageRule::Round:
            return CSSValueRound;
        case NinePieceImageRule::Space:
            return CSSValueSpace;
        default:
            return CSSValueStretch;
        }
    };

    auto horizontalRepeat = CSSPrimitiveValue::create(valueID(image.horizontalRule()));
    RefPtr<CSSPrimitiveValue> verticalRepeat;
    if (image.horizontalRule() == image.verticalRule())
        verticalRepeat = horizontalRepeat.copyRef();
    else
        verticalRepeat = CSSPrimitiveValue::create(valueID(image.verticalRule()));
    return CSSValuePair::create(WTFMove(horizontalRepeat), verticalRepeat.releaseNonNull());
}

inline Ref<CSSValue> ExtractorConverter::convertNinePieceImage(ExtractorState& state, const NinePieceImage& image)
{
    return createBorderImageValue({
        .source = image.image()->computedStyleValue(state.style),
        .slice = convertNinePieceImageSlices(state, image),
        .width = convertNinePieceImageQuad(state, image.borderSlices()),
        .outset = convertNinePieceImageQuad(state, image.outset()),
        .repeat = convertNinePieceImageRepeat(state, image),
    });
}

// MARK: - Animation/Transition conversions

inline Ref<CSSValue> ExtractorConverter::convertAnimationName(ExtractorState& state, const ScopedName& name, const Animation*, const AnimationList*)
{
    return convert(state, name);
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationProperty(ExtractorState&, const Animation::TransitionProperty& property, const Animation*, const AnimationList*)
{
    switch (property.mode) {
    case Animation::TransitionMode::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case Animation::TransitionMode::All:
        return CSSPrimitiveValue::create(CSSValueAll);
    case Animation::TransitionMode::SingleProperty:
    case Animation::TransitionMode::UnknownProperty:
        return CSSPrimitiveValue::createCustomIdent(animatablePropertyAsString(property.animatableProperty));
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueNone);
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationAllowsDiscreteTransitions(ExtractorState&, bool allowsDiscreteTransitions, const Animation*, const AnimationList*)
{
    return CSSPrimitiveValue::create(allowsDiscreteTransitions ? CSSValueAllowDiscrete : CSSValueNormal);
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationDuration(ExtractorState&, Markable<double> duration, const Animation* animation, const AnimationList* animationList)
{
    auto animationListHasMultipleExplicitTimelines = [&] {
        if (!animationList || animationList->size() <= 1)
            return false;
        auto explicitTimelines = 0;
        for (auto& animation : *animationList) {
            if (animation->isTimelineSet())
                ++explicitTimelines;
            if (explicitTimelines > 1)
                return true;
        }
        return false;
    };

    auto animationHasExplicitNonAutoTimeline = [&] {
        if (!animation || !animation->isTimelineSet())
            return false;
        auto* timelineKeyword = std::get_if<Animation::TimelineKeyword>(&animation->timeline());
        return !timelineKeyword || *timelineKeyword != Animation::TimelineKeyword::Auto;
    };

    // https://drafts.csswg.org/css-animations-2/#animation-duration
    // For backwards-compatibility with Level 1, when the computed value of animation-timeline is auto
    // (i.e. only one list value, and that value being auto), the resolved value of auto for
    // animation-duration is 0s whenever its used value would also be 0s.
    if (!duration && (animationListHasMultipleExplicitTimelines() || animationHasExplicitNonAutoTimeline()))
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSPrimitiveValue::create(duration.value_or(0), CSSUnitType::CSS_S);
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationDelay(ExtractorState&, double delay, const Animation*, const AnimationList*)
{
    return CSSPrimitiveValue::create(delay, CSSUnitType::CSS_S);
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationIterationCount(ExtractorState&, double iterationCount, const Animation*, const AnimationList*)
{
    if (iterationCount == Animation::IterationCountInfinite)
        return CSSPrimitiveValue::create(CSSValueInfinite);
    return CSSPrimitiveValue::create(iterationCount);
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationDirection(ExtractorState&, Animation::Direction direction, const Animation*, const AnimationList*)
{
    switch (direction) {
    case Animation::Direction::Normal:
        return CSSPrimitiveValue::create(CSSValueNormal);
    case Animation::Direction::Alternate:
        return CSSPrimitiveValue::create(CSSValueAlternate);
    case Animation::Direction::Reverse:
        return CSSPrimitiveValue::create(CSSValueReverse);
    case Animation::Direction::AlternateReverse:
        return CSSPrimitiveValue::create(CSSValueAlternateReverse);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationFillMode(ExtractorState&, AnimationFillMode fillMode, const Animation*, const AnimationList*)
{
    switch (fillMode) {
    case AnimationFillMode::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case AnimationFillMode::Forwards:
        return CSSPrimitiveValue::create(CSSValueForwards);
    case AnimationFillMode::Backwards:
        return CSSPrimitiveValue::create(CSSValueBackwards);
    case AnimationFillMode::Both:
        return CSSPrimitiveValue::create(CSSValueBoth);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationCompositeOperation(ExtractorState&, CompositeOperation operation, const Animation*, const AnimationList*)
{
    switch (operation) {
    case CompositeOperation::Add:
        return CSSPrimitiveValue::create(CSSValueAdd);
    case CompositeOperation::Accumulate:
        return CSSPrimitiveValue::create(CSSValueAccumulate);
    case CompositeOperation::Replace:
        return CSSPrimitiveValue::create(CSSValueReplace);
    }
    RELEASE_ASSERT_NOT_REACHED();
}


inline Ref<CSSValue> ExtractorConverter::convertAnimationPlayState(ExtractorState&, AnimationPlayState playState, const Animation*, const AnimationList*)
{
    switch (playState) {
    case AnimationPlayState::Playing:
        return CSSPrimitiveValue::create(CSSValueRunning);
    case AnimationPlayState::Paused:
        return CSSPrimitiveValue::create(CSSValuePaused);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationTimeline(ExtractorState& state, const Animation::Timeline& timeline, const Animation*, const AnimationList*)
{
    auto valueForAnonymousScrollTimeline = [&](auto& anonymousScrollTimeline) {
        auto scroller = [&] {
            switch (anonymousScrollTimeline.scroller) {
            case Scroller::Nearest:
                return CSSValueNearest;
            case Scroller::Root:
                return CSSValueRoot;
            case Scroller::Self:
                return CSSValueSelf;
            default:
                ASSERT_NOT_REACHED();
                return CSSValueNearest;
            }
        }();
        return CSSScrollValue::create(
            CSSPrimitiveValue::create(scroller),
            convert(state, anonymousScrollTimeline.axis)
        );
    };

    auto valueForAnonymousViewTimeline = [&](auto& anonymousViewTimeline) {
        auto insetCSSValue = [&](auto& inset) -> RefPtr<CSSValue> {
            if (!inset)
                return nullptr;
            return CSSPrimitiveValue::create(*inset, state.style);
        };
        return CSSViewValue::create(
            convert(state, anonymousViewTimeline.axis),
            insetCSSValue(anonymousViewTimeline.insets.start),
            insetCSSValue(anonymousViewTimeline.insets.end)
        );
    };

    return WTF::switchOn(timeline,
        [&](Animation::TimelineKeyword keyword) -> Ref<CSSValue> {
            return CSSPrimitiveValue::create(keyword == Animation::TimelineKeyword::None ? CSSValueNone : CSSValueAuto);
        },
        [&](const AtomString& customIdent) -> Ref<CSSValue> {
            return CSSPrimitiveValue::createCustomIdent(customIdent);
        },
        [&](const Animation::AnonymousScrollTimeline& anonymousScrollTimeline) -> Ref<CSSValue> {
            return valueForAnonymousScrollTimeline(anonymousScrollTimeline);
        },
        [&](const Animation::AnonymousViewTimeline& anonymousViewTimeline) -> Ref<CSSValue> {
            return valueForAnonymousViewTimeline(anonymousViewTimeline);
        }
    );
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationTimingFunction(ExtractorState& state, const TimingFunction& timingFunction, const Animation*, const AnimationList*)
{
    return CSSEasingFunctionValue::create(toCSSEasingFunction(timingFunction, state.style));
}

inline Ref<CSSValue> ExtractorConverter::convertAnimationTimingFunction(ExtractorState& state, const TimingFunction* timingFunction, const Animation*, const AnimationList*)
{
    return CSSEasingFunctionValue::create(toCSSEasingFunction(*timingFunction, state.style));
}

inline Ref<CSSValueList> ExtractorConverter::convertAnimationSingleRange(ExtractorState& state, const SingleTimelineRange& range, SingleTimelineRange::Type type)
{
    CSSValueListBuilder list;
    if (range.name != SingleTimelineRange::Name::Omitted)
        list.append(CSSPrimitiveValue::create(SingleTimelineRange::valueID(range.name)));
    if (!SingleTimelineRange::isDefault(range.offset, type))
        list.append(convertLength(state, range.offset));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValueList> ExtractorConverter::convertAnimationRangeStart(ExtractorState& state, const SingleTimelineRange& range, const Animation*, const AnimationList*)
{
    return convertAnimationSingleRange(state, range, SingleTimelineRange::Type::Start);
}

inline Ref<CSSValueList> ExtractorConverter::convertAnimationRangeEnd(ExtractorState& state, const SingleTimelineRange& range, const Animation*, const AnimationList*)
{
    return convertAnimationSingleRange(state, range, SingleTimelineRange::Type::End);
}

inline Ref<CSSValueList> ExtractorConverter::convertAnimationRange(ExtractorState& state, const TimelineRange& range, const Animation*, const AnimationList*)
{
    CSSValueListBuilder list;
    auto rangeStart = range.start;
    auto rangeEnd = range.end;

    Ref startValue = convertAnimationSingleRange(state, rangeStart, SingleTimelineRange::Type::Start);
    Ref endValue = convertAnimationSingleRange(state, rangeEnd, SingleTimelineRange::Type::End);
    bool endValueEqualsStart = startValue->equals(endValue);

    if (startValue->length())
        list.append(WTFMove(startValue));

    bool isNormal = rangeEnd.name == SingleTimelineRange::Name::Normal;
    bool isDefaultAndSameNameAsStart = rangeStart.name == rangeEnd.name && SingleTimelineRange::isDefault(rangeEnd.offset, SingleTimelineRange::Type::End);
    if (endValue->length() && !endValueEqualsStart && !isNormal && !isDefaultAndSameNameAsStart)
        list.append(WTFMove(endValue));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertSingleAnimation(ExtractorState& state, const Animation& animation)
{
    static NeverDestroyed<Ref<TimingFunction>> initialTimingFunction(Animation::initialTimingFunction());
    static NeverDestroyed<String> alternate { "alternate"_s };
    static NeverDestroyed<String> alternateReverse { "alternate-reverse"_s };
    static NeverDestroyed<String> backwards { "backwards"_s };
    static NeverDestroyed<String> both { "both"_s };
    static NeverDestroyed<String> ease { "ease"_s };
    static NeverDestroyed<String> easeIn { "ease-in"_s };
    static NeverDestroyed<String> easeInOut { "ease-in-out"_s };
    static NeverDestroyed<String> easeOut { "ease-out"_s };
    static NeverDestroyed<String> forwards { "forwards"_s };
    static NeverDestroyed<String> infinite { "infinite"_s };
    static NeverDestroyed<String> linear { "linear"_s };
    static NeverDestroyed<String> normal { "normal"_s };
    static NeverDestroyed<String> paused { "paused"_s };
    static NeverDestroyed<String> reverse { "reverse"_s };
    static NeverDestroyed<String> running { "running"_s };
    static NeverDestroyed<String> stepEnd { "step-end"_s };
    static NeverDestroyed<String> stepStart { "step-start"_s };

    // If we have an animation-delay but no animation-duration set, we must serialize
    // the animation-duration because they're both <time> values and animation-delay
    // comes first.
    auto showsDelay = animation.delay() != Animation::initialDelay();
    auto showsDuration = showsDelay || animation.duration() != Animation::initialDuration();

    auto showsTimingFunction = [&] {
        RefPtr timingFunction = animation.timingFunction();
        if (timingFunction && *timingFunction != initialTimingFunction.get())
            return true;
        auto& name = animation.name().name;
        return name == ease || name == easeIn || name == easeInOut || name == easeOut || name == linear || name == stepEnd || name == stepStart;
    };

    auto showsIterationCount = [&] {
        if (animation.iterationCount() != Animation::initialIterationCount())
            return true;
        return animation.name().name == infinite;
    };

    auto showsDirection = [&] {
        if (animation.direction() != Animation::initialDirection())
            return true;
        auto& name = animation.name().name;
        return name == normal || name == reverse || name == alternate || name == alternateReverse;
    };

    auto showsFillMode = [&] {
        if (animation.fillMode() != Animation::initialFillMode())
            return true;
        auto& name = animation.name().name;
        return name == forwards || name == backwards || name == both;
    };

    auto showsPlaysState = [&] {
        if (animation.playState() != Animation::initialPlayState())
            return true;
        auto& name = animation.name().name;
        return name == running || name == paused;
    };

    CSSValueListBuilder list;
    if (showsDuration)
        list.append(ExtractorConverter::convertAnimationDuration(state, animation.duration(), nullptr, nullptr));
    if (showsTimingFunction())
        list.append(ExtractorConverter::convertAnimationTimingFunction(state, *animation.timingFunction(), nullptr, nullptr));
    if (showsDelay)
        list.append(ExtractorConverter::convertAnimationDelay(state, animation.delay(), nullptr, nullptr));
    if (showsIterationCount())
        list.append(ExtractorConverter::convertAnimationIterationCount(state, animation.iterationCount(), nullptr, nullptr));
    if (showsDirection())
        list.append(ExtractorConverter::convertAnimationDirection(state, animation.direction(), nullptr, nullptr));
    if (showsFillMode())
        list.append(ExtractorConverter::convertAnimationFillMode(state, animation.fillMode(), nullptr, nullptr));
    if (showsPlaysState())
        list.append(ExtractorConverter::convertAnimationPlayState(state, animation.playState(), nullptr, nullptr));
    if (animation.name() != Animation::initialName())
        list.append(ExtractorConverter::convertAnimationName(state, animation.name(), nullptr, nullptr));
    if (animation.timeline() != Animation::initialTimeline())
        list.append(ExtractorConverter::convertAnimationTimeline(state, animation.timeline(), nullptr, nullptr));
    if (animation.compositeOperation() != Animation::initialCompositeOperation())
        list.append(ExtractorConverter::convertAnimationCompositeOperation(state, animation.compositeOperation(), nullptr, nullptr));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertSingleTransition(ExtractorState& state, const Animation& transition)
{
    static NeverDestroyed<Ref<TimingFunction>> initialTimingFunction(Animation::initialTimingFunction());

    // If we have a transition-delay but no transition-duration set, we must serialize
    // the transition-duration because they're both <time> values and transition-delay
    // comes first.
    auto showsDelay = transition.delay() != Animation::initialDelay();
    auto showsDuration = showsDelay || transition.duration() != Animation::initialDuration();

    CSSValueListBuilder list;
    if (transition.property() != Animation::initialProperty())
        list.append(convertAnimationProperty(state, transition.property(), nullptr, nullptr));
    if (showsDuration)
        list.append(convertAnimationDuration(state, transition.duration(), nullptr, nullptr));
    if (RefPtr timingFunction = transition.timingFunction(); *timingFunction != initialTimingFunction.get())
        list.append(convertAnimationTimingFunction(state, *timingFunction, nullptr, nullptr));
    if (showsDelay)
        list.append(convertAnimationDelay(state, transition.delay(), nullptr, nullptr));
    if (transition.allowsDiscreteTransitions() != Animation::initialAllowsDiscreteTransitions())
        list.append(convertAnimationAllowsDiscreteTransitions(state, transition.allowsDiscreteTransitions(), nullptr, nullptr));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueAll);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

// MARK: - Grid conversions

inline Ref<CSSValue> ExtractorConverter::convertGridAutoFlow(ExtractorState&, GridAutoFlow gridAutoFlow)
{
    ASSERT(gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionRow) || gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionColumn));

    CSSValueListBuilder list;
    if (gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionColumn))
        list.append(CSSPrimitiveValue::create(CSSValueColumn));
    else if (!(gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowAlgorithmDense)))
        list.append(CSSPrimitiveValue::create(CSSValueRow));

    if (gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowAlgorithmDense))
        list.append(CSSPrimitiveValue::create(CSSValueDense));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

} // namespace Style
} // namespace WebCore
