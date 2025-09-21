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

#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSCounterValue.h"
#include "CSSEasingFunctionValue.h"
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
#include "FontCascade.h"
#include "FontSelectionValueInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "Length.h"
#include "PathOperation.h"
#include "PerspectiveTransformOperation.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderStyleInlines.h"
#include "SVGRenderStyle.h"
#include "ScrollTimeline.h"
#include "SkewTransformOperation.h"
#include "StyleClipPath.h"
#include "StyleColor.h"
#include "StyleColorScheme.h"
#include "StyleCornerShapeValue.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleEasingFunction.h"
#include "StyleExtractorState.h"
#include "StyleFlexBasis.h"
#include "StyleInset.h"
#include "StyleLineBoxContain.h"
#include "StyleMargin.h"
#include "StyleMaximumSize.h"
#include "StyleMinimumSize.h"
#include "StyleOffsetPath.h"
#include "StylePadding.h"
#include "StylePerspective.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleRotate.h"
#include "StyleScale.h"
#include "StyleScrollMargin.h"
#include "StyleScrollPadding.h"
#include "StyleTranslate.h"
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

    static Ref<CSSPrimitiveValue> convertLength(ExtractorState&, const WebCore::Length&);
    static Ref<CSSPrimitiveValue> convertLength(CSSValuePool&, const RenderStyle&, const WebCore::Length&);
    template<typename T> static Ref<CSSPrimitiveValue> convertNumberAsPixels(ExtractorState&, T);

    static Ref<CSSValue> convertCustomIdentAtomOrAuto(ExtractorState&, const AtomString&);

    // MARK: SVG conversions

    static Ref<CSSValue> convertSVGURIReference(ExtractorState&, const URL&);

    // MARK: Transform conversions

    static Ref<CSSValue> convertTransformationMatrix(ExtractorState&, const TransformationMatrix&);
    static Ref<CSSValue> convertTransformationMatrix(CSSValuePool&, const RenderStyle&, const TransformationMatrix&);

    // MARK: Shared conversions

    static Ref<CSSValue> convertGlyphOrientation(ExtractorState&, GlyphOrientation);
    static Ref<CSSValue> convertGlyphOrientationOrAuto(ExtractorState&, GlyphOrientation);
    static Ref<CSSValue> convertMarginTrim(ExtractorState&, OptionSet<MarginTrimType>);
    static Ref<CSSValue> convertWebkitTextCombine(ExtractorState&, TextCombine);
    static Ref<CSSValue> convertImageOrientation(ExtractorState&, ImageOrientation);
    static Ref<CSSValue> convertContain(ExtractorState&, OptionSet<Containment>);
    static Ref<CSSValue> convertTextSpacingTrim(ExtractorState&, TextSpacingTrim);
    static Ref<CSSValue> convertTextAutospace(ExtractorState&, TextAutospace);
    static Ref<CSSValue> convertLineFitEdge(ExtractorState&, const TextEdge&);
    static Ref<CSSValue> convertTextBoxEdge(ExtractorState&, const TextEdge&);
    static Ref<CSSValue> convertPositionTryFallbacks(ExtractorState&, const FixedVector<PositionTryFallback>&);
    static Ref<CSSValue> convertWillChange(ExtractorState&, const WillChangeData*);
    static Ref<CSSValue> convertTabSize(ExtractorState&, const TabSize&);
    static Ref<CSSValue> convertScrollSnapType(ExtractorState&, const ScrollSnapType&);
    static Ref<CSSValue> convertScrollSnapAlign(ExtractorState&, const ScrollSnapAlign&);
    static Ref<CSSValue> convertLineBoxContain(ExtractorState&, OptionSet<Style::LineBoxContain>);
    static Ref<CSSValue> convertWebkitRubyPosition(ExtractorState&, RubyPosition);
    static Ref<CSSValue> convertTouchAction(ExtractorState&, OptionSet<TouchAction>);
    static Ref<CSSValue> convertTextTransform(ExtractorState&, OptionSet<TextTransform>);
    static Ref<CSSValue> convertTextUnderlinePosition(ExtractorState&, OptionSet<TextUnderlinePosition>);
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

    // MARK: FillLayer conversions

    static Ref<CSSValue> convertFillLayerMaskComposite(ExtractorState&, CompositeOperator);
    static Ref<CSSValue> convertFillLayerWebkitMaskComposite(ExtractorState&, CompositeOperator);
    static Ref<CSSValue> convertFillLayerMaskMode(ExtractorState&, MaskMode);
    static Ref<CSSValue> convertFillLayerWebkitMaskSourceType(ExtractorState&, MaskMode);

    // MARK: Font conversions

    static Ref<CSSValue> convertFontFamily(ExtractorState&, const AtomString&);
    static Ref<CSSValue> convertFontSizeAdjust(ExtractorState&, const FontSizeAdjust&);
    static Ref<CSSValue> convertFontPalette(ExtractorState&, const FontPalette&);
    static Ref<CSSValue> convertFontWeight(ExtractorState&, FontSelectionValue);
    static Ref<CSSValue> convertFontWidth(ExtractorState&, FontSelectionValue);
    static Ref<CSSValue> convertFontFeatureSettings(ExtractorState&, const FontFeatureSettings&);
    static Ref<CSSValue> convertFontVariationSettings(ExtractorState&, const FontVariationSettings&);

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
    return CSSPrimitiveValue::create(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, int value)
{
    return CSSPrimitiveValue::create(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, unsigned short value)
{
    return CSSPrimitiveValue::create(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, short value)
{
    return CSSPrimitiveValue::create(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convertLength(ExtractorState& state, const WebCore::Length& length)
{
    return convertLength(state.pool, state.style, length);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convertLength(CSSValuePool&, const RenderStyle& style, const WebCore::Length& length)
{
    if (length.isFixed())
        return CSSPrimitiveValue::create(adjustFloatForAbsoluteZoom(length.value(), style), CSSUnitType::CSS_PX);
    return CSSPrimitiveValue::create(length, style);
}

template<typename T> Ref<CSSPrimitiveValue> ExtractorConverter::convertNumberAsPixels(ExtractorState& state, T number)
{
    return CSSPrimitiveValue::create(adjustFloatForAbsoluteZoom(number, state.style), CSSUnitType::CSS_PX);
}

inline Ref<CSSValue> ExtractorConverter::convertCustomIdentAtomOrAuto(ExtractorState& state, const AtomString& string)
{
    if (string.isNull())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
    return createCSSValue(state.pool, state.style, CustomIdentifier { string });
}

// MARK: - SVG conversions

inline Ref<CSSValue> ExtractorConverter::convertSVGURIReference(ExtractorState& state, const URL& marker)
{
    if (marker.isNone())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
    return CSSURLValue::create(toCSS(marker, state.style));
}

// MARK: - Transform conversions

inline Ref<CSSValue> ExtractorConverter::convertTransformationMatrix(ExtractorState& state, const TransformationMatrix& transform)
{
    return convertTransformationMatrix(state.pool, state.style, transform);
}

inline Ref<CSSValue> ExtractorConverter::convertTransformationMatrix(CSSValuePool& pool, const RenderStyle& style, const TransformationMatrix& transform)
{
    auto zoom = style.usedZoom();
    if (transform.isAffine()) {
        double values[] = { transform.a(), transform.b(), transform.c(), transform.d(), transform.e() / zoom, transform.f() / zoom };
        CSSValueListBuilder arguments;
        for (auto value : values)
            arguments.append(createCSSValue(pool, style, Number<> { value }));
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
        arguments.append(createCSSValue(pool, style, Number<> { value }));
    return CSSFunctionValue::create(CSSValueMatrix3d, WTFMove(arguments));
}

// MARK: - Shared conversions

inline Ref<CSSValue> ExtractorConverter::convertGlyphOrientation(ExtractorState& state, GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        return createCSSValue(state.pool, state.style, Angle<> { 0.0 });
    case GlyphOrientation::Degrees90:
        return createCSSValue(state.pool, state.style, Angle<> { 90.0 });
    case GlyphOrientation::Degrees180:
        return createCSSValue(state.pool, state.style, Angle<> { 180.0 });
    case GlyphOrientation::Degrees270:
        return createCSSValue(state.pool, state.style, Angle<> { 270.0 });
    case GlyphOrientation::Auto:
        ASSERT_NOT_REACHED();
        return createCSSValue(state.pool, state.style, Angle<> { 0.0 });
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorConverter::convertGlyphOrientationOrAuto(ExtractorState& state, GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        return createCSSValue(state.pool, state.style, Angle<> { 0.0 });
    case GlyphOrientation::Degrees90:
        return createCSSValue(state.pool, state.style, Angle<> { 90.0 });
    case GlyphOrientation::Degrees180:
        return createCSSValue(state.pool, state.style, Angle<> { 180.0 });
    case GlyphOrientation::Degrees270:
        return createCSSValue(state.pool, state.style, Angle<> { 270.0 });
    case GlyphOrientation::Auto:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorConverter::convertMarginTrim(ExtractorState& state, OptionSet<MarginTrimType> marginTrim)
{
    if (marginTrim.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    // Try to serialize into one of the "block" or "inline" shorthands
    if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd }) && !marginTrim.containsAny({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd }))
        return createCSSValue(state.pool, state.style, CSS::Keyword::Block { });
    if (marginTrim.containsAll({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd }) && !marginTrim.containsAny({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd }))
        return createCSSValue(state.pool, state.style, CSS::Keyword::Inline { });
    if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd, MarginTrimType::InlineStart, MarginTrimType::InlineEnd }))
        return CSSValueList::createSpaceSeparated(createCSSValue(state.pool, state.style, CSS::Keyword::Block { }), createCSSValue(state.pool, state.style, CSS::Keyword::Inline { }));

    CSSValueListBuilder list;
    if (marginTrim.contains(MarginTrimType::BlockStart))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::BlockStart { }));
    if (marginTrim.contains(MarginTrimType::InlineStart))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::InlineStart { }));
    if (marginTrim.contains(MarginTrimType::BlockEnd))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::BlockEnd { }));
    if (marginTrim.contains(MarginTrimType::InlineEnd))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::InlineEnd { }));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertWebkitTextCombine(ExtractorState& state, TextCombine textCombine)
{
    if (textCombine == TextCombine::All)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Horizontal { });
    return createCSSValue(state.pool, state.style, textCombine);
}

inline Ref<CSSValue> ExtractorConverter::convertImageOrientation(ExtractorState& state, ImageOrientation imageOrientation)
{
    if (imageOrientation == ImageOrientation::Orientation::FromImage)
        return createCSSValue(state.pool, state.style, CSS::Keyword::FromImage { });
    return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
}

inline Ref<CSSValue> ExtractorConverter::convertContain(ExtractorState& state, OptionSet<Containment> containment)
{
    if (!containment)
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
    if (containment == RenderStyle::strictContainment())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Strict { });
    if (containment == RenderStyle::contentContainment())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Content { });
    CSSValueListBuilder list;
    if (containment & Containment::Size)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Size { }));
    if (containment & Containment::InlineSize)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::InlineSize { }));
    if (containment & Containment::Layout)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Layout { }));
    if (containment & Containment::Style)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Style { }));
    if (containment & Containment::Paint)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Paint { }));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertTextSpacingTrim(ExtractorState& state, TextSpacingTrim textSpacingTrim)
{
    switch (textSpacingTrim.type()) {
    case TextSpacingTrim::TrimType::SpaceAll:
        return createCSSValue(state.pool, state.style, CSS::Keyword::SpaceAll { });
    case TextSpacingTrim::TrimType::Auto:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
    case TextSpacingTrim::TrimType::TrimAll:
        return createCSSValue(state.pool, state.style, CSS::Keyword::TrimAll { });
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return createCSSValue(state.pool, state.style, CSS::Keyword::SpaceAll { });
}

inline Ref<CSSValue> ExtractorConverter::convertTextAutospace(ExtractorState& state, TextAutospace textAutospace)
{
    if (textAutospace.isAuto())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
    if (textAutospace.isNoAutospace())
        return createCSSValue(state.pool, state.style, CSS::Keyword::NoAutospace { });
    if (textAutospace.isNormal())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Normal { });

    CSSValueListBuilder list;
    if (textAutospace.hasIdeographAlpha())
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::IdeographAlpha { }));
    if (textAutospace.hasIdeographNumeric())
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::IdeographNumeric { }));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertLineFitEdge(ExtractorState& state, const TextEdge& textEdge)
{
    if (textEdge.over == TextEdgeType::Leading && textEdge.under == TextEdgeType::Leading)
        return createCSSValue(state.pool, state.style, textEdge.over);

    // https://www.w3.org/TR/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeUnderEdge = [&] {
        if (textEdge.over == TextEdgeType::CapHeight || textEdge.over == TextEdgeType::ExHeight)
            return textEdge.under != TextEdgeType::Text;
        return textEdge.over != textEdge.under;
    }();

    if (!shouldSerializeUnderEdge)
        return createCSSValue(state.pool, state.style, textEdge.over);

    return CSSValuePair::create(createCSSValue(state.pool, state.style, textEdge.over), createCSSValue(state.pool, state.style, textEdge.under));
}

inline Ref<CSSValue> ExtractorConverter::convertTextBoxEdge(ExtractorState& state, const TextEdge& textEdge)
{
    if (textEdge.over == TextEdgeType::Auto && textEdge.under == TextEdgeType::Auto)
        return createCSSValue(state.pool, state.style, textEdge.over);

    // https://www.w3.org/TR/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeUnderEdge = [&] {
        if (textEdge.over == TextEdgeType::CapHeight || textEdge.over == TextEdgeType::ExHeight)
            return textEdge.under != TextEdgeType::Text;
        return textEdge.over != textEdge.under;
    }();

    if (!shouldSerializeUnderEdge)
        return createCSSValue(state.pool, state.style, textEdge.over);

    return CSSValuePair::create(createCSSValue(state.pool, state.style, textEdge.over), createCSSValue(state.pool, state.style, textEdge.under));
}

inline Ref<CSSValue> ExtractorConverter::convertPositionTryFallbacks(ExtractorState& state, const FixedVector<PositionTryFallback>& fallbacks)
{
    if (fallbacks.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

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
            singleFallbackList.append(createCSSValue(state.pool, state.style, *fallback.positionTryRuleName));
        for (auto& tactic : fallback.tactics)
            singleFallbackList.append(createCSSValue(state.pool, state.style, tactic));
        list.append(CSSValueList::createSpaceSeparated(singleFallbackList));
    }

    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertWillChange(ExtractorState& state, const WillChangeData* willChangeData)
{
    if (!willChangeData || !willChangeData->numFeatures())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });

    CSSValueListBuilder list;
    for (size_t i = 0; i < willChangeData->numFeatures(); ++i) {
        auto feature = willChangeData->featureAt(i);
        switch (feature.first) {
        case WillChangeData::Feature::ScrollPosition:
            list.append(createCSSValue(state.pool, state.style, CSS::Keyword::ScrollPosition { }));
            break;
        case WillChangeData::Feature::Contents:
            list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Contents { }));
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

inline Ref<CSSValue> ExtractorConverter::convertTabSize(ExtractorState& state, const TabSize& tabSize)
{
    auto value = tabSize.widthInPixels(1.0);
    if (tabSize.isSpaces())
        return createCSSValue(state.pool, state.style, Number<> { value });
    else
        return createCSSValue(state.pool, state.style, Length<> { value });
}

inline Ref<CSSValue> ExtractorConverter::convertScrollSnapType(ExtractorState& state, const ScrollSnapType& type)
{
    if (type.strictness == ScrollSnapStrictness::None)
        return CSSValueList::createSpaceSeparated(createCSSValue(state.pool, state.style, CSS::Keyword::None { }));
    if (type.strictness == ScrollSnapStrictness::Proximity)
        return CSSValueList::createSpaceSeparated(createCSSValue(state.pool, state.style, type.axis));
    return CSSValueList::createSpaceSeparated(createCSSValue(state.pool, state.style, type.axis), createCSSValue(state.pool, state.style, type.strictness));
}

inline Ref<CSSValue> ExtractorConverter::convertScrollSnapAlign(ExtractorState& state, const ScrollSnapAlign& alignment)
{
    return CSSValuePair::create(
        createCSSValue(state.pool, state.style, alignment.blockAlign),
        createCSSValue(state.pool, state.style, alignment.inlineAlign)
    );
}

inline Ref<CSSValue> ExtractorConverter::convertLineBoxContain(ExtractorState& state, OptionSet<Style::LineBoxContain> lineBoxContain)
{
    if (!lineBoxContain)
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    CSSValueListBuilder list;
    if (lineBoxContain.contains(LineBoxContain::Block))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Block { }));
    if (lineBoxContain.contains(LineBoxContain::Inline))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Inline { }));
    if (lineBoxContain.contains(LineBoxContain::Font))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Font { }));
    if (lineBoxContain.contains(LineBoxContain::Glyphs))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Glyphs { }));
    if (lineBoxContain.contains(LineBoxContain::Replaced))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Replaced { }));
    if (lineBoxContain.contains(LineBoxContain::InlineBox))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::InlineBox { }));
    if (lineBoxContain.contains(LineBoxContain::InitialLetter))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::InitialLetter { }));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertWebkitRubyPosition(ExtractorState& state, RubyPosition position)
{
    switch (position) {
    case RubyPosition::Over:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Before { });
    case RubyPosition::Under:
        return createCSSValue(state.pool, state.style, CSS::Keyword::After { });
    case RubyPosition::InterCharacter:
    case RubyPosition::LegacyInterCharacter:
        return createCSSValue(state.pool, state.style, CSS::Keyword::InterCharacter { });
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorConverter::convertTouchAction(ExtractorState& state, OptionSet<TouchAction> touchActions)
{
    if (touchActions & TouchAction::Auto)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
    if (touchActions & TouchAction::None)
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
    if (touchActions & TouchAction::Manipulation)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Manipulation { });

    CSSValueListBuilder list;
    if (touchActions & TouchAction::PanX)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::PanX { }));
    if (touchActions & TouchAction::PanY)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::PanY { }));
    if (touchActions & TouchAction::PinchZoom)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::PinchZoom { }));
    if (list.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertTextTransform(ExtractorState& state, OptionSet<TextTransform> textTransform)
{
    CSSValueListBuilder list;
    if (textTransform.contains(TextTransform::Capitalize))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Capitalize { }));
    else if (textTransform.contains(TextTransform::Uppercase))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Uppercase { }));
    else if (textTransform.contains(TextTransform::Lowercase))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Lowercase { }));

    if (textTransform.contains(TextTransform::FullWidth))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::FullWidth { }));

    if (textTransform.contains(TextTransform::FullSizeKana))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::FullSizeKana { }));

    if (list.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertTextUnderlinePosition(ExtractorState& state, OptionSet<TextUnderlinePosition> textUnderlinePosition)
{
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::FromFont) && (textUnderlinePosition & TextUnderlinePosition::Under)));
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::Left) && (textUnderlinePosition & TextUnderlinePosition::Right)));

    if (textUnderlinePosition.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
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

inline Ref<CSSValue> ExtractorConverter::convertTextEmphasisPosition(ExtractorState& state, OptionSet<TextEmphasisPosition> textEmphasisPosition)
{
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Over) && (textEmphasisPosition & TextEmphasisPosition::Under)));
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Left) && (textEmphasisPosition & TextEmphasisPosition::Right)));
    ASSERT((textEmphasisPosition & TextEmphasisPosition::Over) || (textEmphasisPosition & TextEmphasisPosition::Under));

    CSSValueListBuilder list;
    if (textEmphasisPosition & TextEmphasisPosition::Over)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Over { }));
    if (textEmphasisPosition & TextEmphasisPosition::Under)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Under { }));
    if (textEmphasisPosition & TextEmphasisPosition::Left)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Left { }));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertSpeakAs(ExtractorState& state, OptionSet<SpeakAs> speakAs)
{
    CSSValueListBuilder list;
    if (speakAs & SpeakAs::SpellOut)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::SpellOut { }));
    if (speakAs & SpeakAs::Digits)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Digits { }));
    if (speakAs & SpeakAs::LiteralPunctuation)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::LiteralPunctuation { }));
    if (speakAs & SpeakAs::NoPunctuation)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::NoPunctuation { }));
    if (list.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Normal { });
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertHangingPunctuation(ExtractorState& state, OptionSet<HangingPunctuation> hangingPunctuation)
{
    CSSValueListBuilder list;
    if (hangingPunctuation & HangingPunctuation::First)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::First { }));
    if (hangingPunctuation & HangingPunctuation::AllowEnd)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::AllowEnd { }));
    if (hangingPunctuation & HangingPunctuation::ForceEnd)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::ForceEnd { }));
    if (hangingPunctuation & HangingPunctuation::Last)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Last { }));
    if (list.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertPageBreak(ExtractorState& state, BreakBetween value)
{
    if (value == BreakBetween::Page || value == BreakBetween::LeftPage || value == BreakBetween::RightPage
        || value == BreakBetween::RectoPage || value == BreakBetween::VersoPage)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Always { }); // CSS 2.1 allows us to map these to always.
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidPage)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Avoid { });
    return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
}

inline Ref<CSSValue> ExtractorConverter::convertPageBreak(ExtractorState& state, BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidPage)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Avoid { });
    return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
}

inline Ref<CSSValue> ExtractorConverter::convertWebkitColumnBreak(ExtractorState& state, BreakBetween value)
{
    if (value == BreakBetween::Column)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Always { });
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidColumn)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Avoid { });
    return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
}

inline Ref<CSSValue> ExtractorConverter::convertWebkitColumnBreak(ExtractorState& state, BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidColumn)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Avoid { });
    return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
}

inline Ref<CSSValue> ExtractorConverter::convertSelfOrDefaultAlignmentData(ExtractorState& state, const StyleSelfAlignmentData& data)
{
    CSSValueListBuilder list;
    if (data.positionType() == ItemPositionType::Legacy)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Legacy { }));
    if (data.position() == ItemPosition::Baseline)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Baseline { }));
    else if (data.position() == ItemPosition::LastBaseline) {
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Last { }));
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Baseline { }));
    } else {
        if (data.position() >= ItemPosition::Center && data.overflow() != OverflowAlignment::Default)
            list.append(createCSSValue(state.pool, state.style, data.overflow()));
        if (data.position() == ItemPosition::Legacy)
            list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Normal { }));
        else
            list.append(createCSSValue(state.pool, state.style, data.position()));
    }
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertContentAlignmentData(ExtractorState& state, const StyleContentAlignmentData& data)
{
    CSSValueListBuilder list;

    // Handle content-distribution values
    if (data.distribution() != ContentDistribution::Default)
        list.append(createCSSValue(state.pool, state.style, data.distribution()));

    // Handle content-position values (either as fallback or actual value)
    switch (data.position()) {
    case ContentPosition::Normal:
        // Handle 'normal' value, not valid as content-distribution fallback.
        if (data.distribution() == ContentDistribution::Default)
            list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Normal { }));
        break;
    case ContentPosition::LastBaseline:
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Last { }));
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Baseline { }));
        break;
    default:
        // Handle overflow-alignment (only allowed for content-position values)
        if ((data.position() >= ContentPosition::Center || data.distribution() != ContentDistribution::Default) && data.overflow() != OverflowAlignment::Default)
            list.append(createCSSValue(state.pool, state.style, data.overflow()));
        list.append(createCSSValue(state.pool, state.style, data.position()));
    }

    ASSERT(list.size() > 0);
    ASSERT(list.size() <= 3);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertPaintOrder(ExtractorState& state, PaintOrder paintOrder)
{
    if (paintOrder == PaintOrder::Normal)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Normal { });

    CSSValueListBuilder paintOrderList;
    switch (paintOrder) {
    case PaintOrder::Normal:
        ASSERT_NOT_REACHED();
        break;
    case PaintOrder::Fill:
        paintOrderList.append(createCSSValue(state.pool, state.style, CSS::Keyword::Fill { }));
        break;
    case PaintOrder::FillMarkers:
        paintOrderList.append(createCSSValue(state.pool, state.style, CSS::Keyword::Fill { }));
        paintOrderList.append(createCSSValue(state.pool, state.style, CSS::Keyword::Markers { }));
        break;
    case PaintOrder::Stroke:
        paintOrderList.append(createCSSValue(state.pool, state.style, CSS::Keyword::Stroke { }));
        break;
    case PaintOrder::StrokeMarkers:
        paintOrderList.append(createCSSValue(state.pool, state.style, CSS::Keyword::Stroke { }));
        paintOrderList.append(createCSSValue(state.pool, state.style, CSS::Keyword::Markers { }));
        break;
    case PaintOrder::Markers:
        paintOrderList.append(createCSSValue(state.pool, state.style, CSS::Keyword::Markers { }));
        break;
    case PaintOrder::MarkersStroke:
        paintOrderList.append(createCSSValue(state.pool, state.style, CSS::Keyword::Markers { }));
        paintOrderList.append(createCSSValue(state.pool, state.style, CSS::Keyword::Stroke { }));
        break;
    }
    return CSSValueList::createSpaceSeparated(WTFMove(paintOrderList));
}

inline Ref<CSSValue> ExtractorConverter::convertPositionAnchor(ExtractorState& state, const std::optional<ScopedName>& positionAnchor)
{
    if (!positionAnchor)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
    return createCSSValue(state.pool, state.style, *positionAnchor);
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

    return CSSPropertyParserHelpers::valueForPositionArea(blockOrXAxisKeyword, inlineOrYAxisKeyword, CSSPropertyParserHelpers::ValueType::Computed).releaseNonNull();
}

inline Ref<CSSValue> ExtractorConverter::convertPositionArea(ExtractorState& state, const std::optional<PositionArea>& positionArea)
{
    if (!positionArea)
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
    return convertPositionArea(state, *positionArea);
}

inline Ref<CSSValue> ExtractorConverter::convertNameScope(ExtractorState& state, const NameScope& scope)
{
    switch (scope.type) {
    case NameScope::Type::None:
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    case NameScope::Type::All:
        return createCSSValue(state.pool, state.style, CSS::Keyword::All { });

    case NameScope::Type::Ident:
        if (scope.names.isEmpty())
            return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

        CSSValueListBuilder list;
        for (auto& name : scope.names) {
            ASSERT(!name.isNull());
            list.append(createCSSValue(state.pool, state.style, CustomIdentifier { name }));
        }

        return CSSValueList::createCommaSeparated(WTFMove(list));
    }

    ASSERT_NOT_REACHED();
    return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
}

inline Ref<CSSValue> ExtractorConverter::convertPositionVisibility(ExtractorState& state, OptionSet<PositionVisibility> positionVisibility)
{
    CSSValueListBuilder list;
    if (positionVisibility & PositionVisibility::AnchorsValid)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::AnchorsValid { }));
    if (positionVisibility & PositionVisibility::AnchorsVisible)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::AnchorsVisible { }));
    if (positionVisibility & PositionVisibility::NoOverflow)
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::NoOverflow { }));

    if (list.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Always { });

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

// MARK: - FillLayer conversions

inline Ref<CSSValue> ExtractorConverter::convertFillLayerMaskComposite(ExtractorState&, CompositeOperator composite)
{
    return CSSPrimitiveValue::create(toCSSValueID(composite, CSSPropertyMaskComposite));
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerWebkitMaskComposite(ExtractorState&, CompositeOperator composite)
{
    return CSSPrimitiveValue::create(toCSSValueID(composite, CSSPropertyWebkitMaskComposite));
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerMaskMode(ExtractorState& state, MaskMode maskMode)
{
    switch (maskMode) {
    case MaskMode::Alpha:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Alpha { });
    case MaskMode::Luminance:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Luminance { });
    case MaskMode::MatchSource:
        return createCSSValue(state.pool, state.style, CSS::Keyword::MatchSource { });
    }
    ASSERT_NOT_REACHED();
    return createCSSValue(state.pool, state.style, CSS::Keyword::MatchSource { });
}

inline Ref<CSSValue> ExtractorConverter::convertFillLayerWebkitMaskSourceType(ExtractorState& state, MaskMode maskMode)
{
    switch (maskMode) {
    case MaskMode::Alpha:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Alpha { });
    case MaskMode::Luminance:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Luminance { });
    case MaskMode::MatchSource:
        // MatchSource is only available in the mask-mode property.
        return createCSSValue(state.pool, state.style, CSS::Keyword::Alpha { });
    }
    ASSERT_NOT_REACHED();
    return createCSSValue(state.pool, state.style, CSS::Keyword::Alpha { });
}

// MARK: - Font conversions

inline Ref<CSSValue> ExtractorConverter::convertFontFamily(ExtractorState& state, const AtomString& family)
{
    if (family == cursiveFamily)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Cursive { });
    if (family == fantasyFamily)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Fantasy { });
    if (family == monospaceFamily)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Monospace { });
    if (family == mathFamily)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Math { });
    if (family == pictographFamily)
        return createCSSValue(state.pool, state.style, CSS::Keyword::WebkitPictograph { });
    if (family == sansSerifFamily)
        return createCSSValue(state.pool, state.style, CSS::Keyword::SansSerif { });
    if (family == serifFamily)
        return createCSSValue(state.pool, state.style, CSS::Keyword::Serif { });
    if (family == systemUiFamily)
        return createCSSValue(state.pool, state.style, CSS::Keyword::SystemUi { });

    return state.pool.createFontFamilyValue(family);
}

inline Ref<CSSValue> ExtractorConverter::convertFontSizeAdjust(ExtractorState& state, const FontSizeAdjust& fontSizeAdjust)
{
    if (fontSizeAdjust.isNone())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    auto metric = fontSizeAdjust.metric;
    auto value = fontSizeAdjust.shouldResolveFromFont() ? fontSizeAdjust.resolve(state.style.computedFontSize(), state.style.metricsOfPrimaryFont()) : fontSizeAdjust.value.asOptional();
    if (!value)
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    if (metric == FontSizeAdjust::Metric::ExHeight)
        return createCSSValue(state.pool, state.style, Number<> { *value });

    return CSSValuePair::create(createCSSValue(state.pool, state.style, metric), createCSSValue(state.pool, state.style, Number<> { *value }));
}

inline Ref<CSSValue> ExtractorConverter::convertFontPalette(ExtractorState& state, const FontPalette& fontPalette)
{
    switch (fontPalette.type) {
    case FontPalette::Type::Normal:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Normal { });
    case FontPalette::Type::Light:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Light { });
    case FontPalette::Type::Dark:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Dark { });
    case FontPalette::Type::Custom:
        return createCSSValue(state.pool, state.style, CustomIdentifier { fontPalette.identifier });
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorConverter::convertFontWeight(ExtractorState& state, FontSelectionValue fontWeight)
{
    return createCSSValue(state.pool, state.style, Number<> { static_cast<float>(fontWeight) });
}

inline Ref<CSSValue> ExtractorConverter::convertFontWidth(ExtractorState& state, FontSelectionValue fontWidth)
{
    return createCSSValue(state.pool, state.style, Percentage<> { static_cast<float>(fontWidth) });
}

inline Ref<CSSValue> ExtractorConverter::convertFontFeatureSettings(ExtractorState& state, const FontFeatureSettings& fontFeatureSettings)
{
    if (!fontFeatureSettings.size())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Normal { });
    CSSValueListBuilder list;
    for (auto& feature : fontFeatureSettings)
        list.append(CSSFontFeatureValue::create(FontTag(feature.tag()), convert(state, feature.value())));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorConverter::convertFontVariationSettings(ExtractorState& state, const FontVariationSettings& fontVariationSettings)
{
    if (fontVariationSettings.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::Normal { });
    CSSValueListBuilder list;
    for (auto& feature : fontVariationSettings)
        list.append(CSSFontVariationValue::create(feature.tag(), convert(state, feature.value())));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

// MARK: - Grid conversions

inline Ref<CSSValue> ExtractorConverter::convertGridAutoFlow(ExtractorState& state, GridAutoFlow gridAutoFlow)
{
    ASSERT(gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionRow) || gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionColumn));

    CSSValueListBuilder list;
    if (gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionColumn))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Column { }));
    else if (!(gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowAlgorithmDense)))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Row { }));

    if (gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowAlgorithmDense))
        list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Dense { }));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

} // namespace Style
} // namespace WebCore
