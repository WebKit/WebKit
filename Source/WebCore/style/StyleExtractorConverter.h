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
#include "PathOperation.h"
#include "PerspectiveTransformOperation.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderStyleInlines.h"
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
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, const ScopedName&);

    template<typename T> static Ref<CSSPrimitiveValue> convertNumberAsPixels(ExtractorState&, T);

    template<CSSValueID> static Ref<CSSPrimitiveValue> convertCustomIdentAtomOrKeyword(ExtractorState&, const AtomString&);

    // MARK: Transform conversions

    static Ref<CSSValue> convertTransformationMatrix(ExtractorState&, const TransformationMatrix&);
    static Ref<CSSValue> convertTransformationMatrix(const RenderStyle&, const TransformationMatrix&);

    // MARK: Shared conversions

    static Ref<CSSValue> convertMarginTrim(ExtractorState&, OptionSet<MarginTrimType>);
    static Ref<CSSValue> convertWebkitTextCombine(ExtractorState&, TextCombine);
    static Ref<CSSValue> convertImageOrientation(ExtractorState&, ImageOrientation);
    static Ref<CSSValue> convertContain(ExtractorState&, OptionSet<Containment>);
    static Ref<CSSValue> convertTextSpacingTrim(ExtractorState&, TextSpacingTrim);
    static Ref<CSSValue> convertTextAutospace(ExtractorState&, TextAutospace);
    static Ref<CSSValue> convertPositionTryFallbacks(ExtractorState&, const FixedVector<PositionTryFallback>&);
    static Ref<CSSValue> convertWillChange(ExtractorState&, const WillChangeData*);
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

template<typename T> Ref<CSSPrimitiveValue> ExtractorConverter::convertNumberAsPixels(ExtractorState& state, T number)
{
    return CSSPrimitiveValue::create(adjustFloatForAbsoluteZoom(number, state.style), CSSUnitType::CSS_PX);
}

template<CSSValueID keyword> Ref<CSSPrimitiveValue> ExtractorConverter::convertCustomIdentAtomOrKeyword(ExtractorState&, const AtomString& string)
{
    if (string.isNull())
        return CSSPrimitiveValue::create(keyword);
    return CSSPrimitiveValue::createCustomIdent(string);
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

// MARK: - Shared conversions

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


inline Ref<CSSValue> ExtractorConverter::convertPositionTryFallbacks(ExtractorState& state, const FixedVector<PositionTryFallback>& fallbacks)
{
    if (fallbacks.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& fallback : fallbacks) {
        if (RefPtr positionAreaProperties = fallback.positionAreaProperties) {
            auto areaValue = positionAreaProperties->getPropertyCSSValue(CSSPropertyPositionArea);
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
                return self == PositionAreaSelf::No ? CSSValueXStart : CSSValueSelfXStart;
            case PositionAreaTrack::SpanStart:
                return self == PositionAreaSelf::No ? CSSValueSpanXStart : CSSValueSpanSelfXStart;
            case PositionAreaTrack::End:
                return self == PositionAreaSelf::No ? CSSValueXEnd : CSSValueSelfXEnd;
            case PositionAreaTrack::SpanEnd:
                return self == PositionAreaSelf::No ? CSSValueSpanXEnd : CSSValueSpanSelfXEnd;
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
                return self == PositionAreaSelf::No ? CSSValueYStart : CSSValueSelfYStart;
            case PositionAreaTrack::SpanStart:
                return self == PositionAreaSelf::No ? CSSValueSpanYStart : CSSValueSpanSelfYStart;
            case PositionAreaTrack::End:
                return self == PositionAreaSelf::No ? CSSValueYEnd : CSSValueSelfYEnd;
            case PositionAreaTrack::SpanEnd:
                return self == PositionAreaSelf::No ? CSSValueSpanYEnd : CSSValueSpanSelfYEnd;
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

// MARK: - FillLayer conversions

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
        if (family == mathFamily)
            return CSSValueMath;
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
