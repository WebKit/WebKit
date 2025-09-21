/*
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSMarkup.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "StyleExtractorConverter.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace Style {

class ExtractorSerializer {
public:
    // MARK: Strong value conversions

    template<typename T, typename... Rest> static void serializeStyleType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const T&, Rest&&...);

    // MARK: Primitive serializations

    template<typename ConvertibleType>
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ConvertibleType&);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, double);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, float);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, unsigned);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, int);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, unsigned short);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, short);

    static void serializeLength(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    static void serializeLength(const RenderStyle&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    template<typename T> static void serializeNumberAsPixels(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);

    static void serializeCustomIdentAtomOrAuto(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const AtomString&);

    // MARK: SVG serializations

    static void serializeSVGURIReference(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const URL&);

    // MARK: Transform serializations

    static void serializeTransformationMatrix(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TransformationMatrix&);
    static void serializeTransformationMatrix(const RenderStyle&, StringBuilder&, const CSS::SerializationContext&, const TransformationMatrix&);

    // MARK: Shared serializations

    static void serializeGlyphOrientation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, GlyphOrientation);
    static void serializeGlyphOrientationOrAuto(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, GlyphOrientation);
    static void serializeMarginTrim(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<MarginTrimType>);
    static void serializeWebkitTextCombine(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, TextCombine);
    static void serializeImageOrientation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, ImageOrientation);
    static void serializeContain(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<Containment>);
    static void serializeTextSpacingTrim(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, TextSpacingTrim);
    static void serializeTextAutospace(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, TextAutospace);
    static void serializeLineFitEdge(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TextEdge&);
    static void serializeTextBoxEdge(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TextEdge&);
    static void serializePositionTryFallbacks(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<PositionTryFallback>&);
    static void serializeWillChange(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WillChangeData*);
    static void serializeTabSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TabSize&);
    static void serializeScrollSnapType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScrollSnapType&);
    static void serializeScrollSnapAlign(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScrollSnapAlign&);
    static void serializeLineBoxContain(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<Style::LineBoxContain>);
    static void serializeWebkitRubyPosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, RubyPosition);
    static void serializeTouchAction(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TouchAction>);
    static void serializeTextTransform(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextTransform>);
    static void serializeTextUnderlinePosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextUnderlinePosition>);
    static void serializeTextEmphasisPosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextEmphasisPosition>);
    static void serializeSpeakAs(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<SpeakAs>);
    static void serializeHangingPunctuation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<HangingPunctuation>);
    static void serializePageBreak(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, BreakBetween);
    static void serializePageBreak(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, BreakInside);
    static void serializeWebkitColumnBreak(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, BreakBetween);
    static void serializeWebkitColumnBreak(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, BreakInside);
    static void serializeSelfOrDefaultAlignmentData(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StyleSelfAlignmentData&);
    static void serializeContentAlignmentData(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StyleContentAlignmentData&);
    static void serializePaintOrder(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, PaintOrder);
    static void serializePositionAnchor(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const std::optional<ScopedName>&);
    static void serializePositionArea(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const std::optional<PositionArea>&);
    static void serializeNameScope(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const NameScope&);
    static void serializePositionVisibility(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<PositionVisibility>);

    // MARK: FillLayer serializations

    static void serializeFillLayerMaskComposite(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, CompositeOperator);
    static void serializeFillLayerWebkitMaskComposite(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, CompositeOperator);
    static void serializeFillLayerMaskMode(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, MaskMode);
    static void serializeFillLayerWebkitMaskSourceType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, MaskMode);

    // MARK: Font serializations

    static void serializeFontFamily(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const AtomString&);
    static void serializeFontSizeAdjust(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontSizeAdjust&);
    static void serializeFontPalette(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontPalette&);
    static void serializeFontWeight(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FontSelectionValue);
    static void serializeFontWidth(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FontSelectionValue);
    static void serializeFontFeatureSettings(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontFeatureSettings&);
    static void serializeFontVariationSettings(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontVariationSettings&);

    // MARK: Grid serializations

    static void serializeGridAutoFlow(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, GridAutoFlow);
};

// MARK: - Strong value serializations

template<typename T, typename... Rest> void ExtractorSerializer::serializeStyleType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const T& value, Rest&&... rest)
{
    serializationForCSS(builder, context, state.style, value, std::forward<Rest>(rest)...);
}

// MARK: - Primitive serializations

template<typename ConvertibleType>
void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ConvertibleType& value)
{
    serializationForCSS(builder, context, state.style, value);
}

inline void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, double value)
{
    serializationForCSS(builder, context, state.style, Number<> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, float value)
{
    serializationForCSS(builder, context, state.style, Number<> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, unsigned value)
{
    serializationForCSS(builder, context, state.style, Integer<CSS::All, unsigned> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, int value)
{
    serializationForCSS(builder, context, state.style, Integer<CSS::All, int> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, unsigned short value)
{
    serializationForCSS(builder, context, state.style, Integer<CSS::All, unsigned short> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, short value)
{
    serializationForCSS(builder, context, state.style, Integer<CSS::All, short> { value });
}

inline void ExtractorSerializer::serializeLength(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& length)
{
    serializeLength(state.style, builder, context, length);
}

inline void ExtractorSerializer::serializeLength(const RenderStyle& style, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& length)
{
    switch (length.type()) {
    case LengthType::Auto:
        serializationForCSS(builder, context, style, CSS::Keyword::Auto { });
        return;
    case LengthType::Content:
        serializationForCSS(builder, context, style, CSS::Keyword::Content { });
        return;
    case LengthType::FillAvailable:
        serializationForCSS(builder, context, style, CSS::Keyword::WebkitFillAvailable { });
        return;
    case LengthType::FitContent:
        serializationForCSS(builder, context, style, CSS::Keyword::FitContent { });
        return;
    case LengthType::Intrinsic:
        serializationForCSS(builder, context, style, CSS::Keyword::Intrinsic { });
        return;
    case LengthType::MinIntrinsic:
        serializationForCSS(builder, context, style, CSS::Keyword::MinIntrinsic { });
        return;
    case LengthType::MinContent:
        serializationForCSS(builder, context, style, CSS::Keyword::MinContent { });
        return;
    case LengthType::MaxContent:
        serializationForCSS(builder, context, style, CSS::Keyword::MaxContent { });
        return;
    case LengthType::Normal:
        serializationForCSS(builder, context, style, CSS::Keyword::Normal { });
        return;
    case LengthType::Fixed:
        serializationForCSS(builder, context, style, Length<> { length.value() });
        return;
    case LengthType::Percent:
        serializationForCSS(builder, context, style, Percentage<> { length.value() });
        return;
    case LengthType::Calculated:
        builder.append(CSSCalcValue::create(length.protectedCalculationValue(), style)->customCSSText(context));
        return;
    case LengthType::Relative:
    case LengthType::Undefined:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename T> void ExtractorSerializer::serializeNumberAsPixels(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    serializationForCSS(builder, context, state.style, Length<CSS::All, T> { number });
}

inline void ExtractorSerializer::serializeCustomIdentAtomOrAuto(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const AtomString& string)
{
    if (string.isNull()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    serializationForCSS(builder, context, state.style, CustomIdentifier { string });
}

// MARK: - SVG serializations

inline void ExtractorSerializer::serializeSVGURIReference(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const URL& marker)
{
    if (marker.isNone()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    serializationForCSS(builder, context, state.style, marker);
}

// MARK: - Transform serializations

inline void ExtractorSerializer::serializeTransformationMatrix(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TransformationMatrix& transform)
{
    serializeTransformationMatrix(state.style, builder, context, transform);
}

inline void ExtractorSerializer::serializeTransformationMatrix(const RenderStyle& style, StringBuilder& builder, const CSS::SerializationContext& context, const TransformationMatrix& transform)
{
    auto zoom = style.usedZoom();
    if (transform.isAffine()) {
        std::array values { transform.a(), transform.b(), transform.c(), transform.d(), transform.e() / zoom, transform.f() / zoom };
        builder.append(nameLiteral(CSSValueMatrix), '(', interleave(values, [&](auto& builder, auto& value) {
            serializationForCSS(builder, context, style, Number<> { value });
        }, ", "_s), ')');
        return;
    }

    std::array values {
        transform.m11(), transform.m12(), transform.m13(), transform.m14() * zoom,
        transform.m21(), transform.m22(), transform.m23(), transform.m24() * zoom,
        transform.m31(), transform.m32(), transform.m33(), transform.m34() * zoom,
        transform.m41() / zoom, transform.m42() / zoom, transform.m43() / zoom, transform.m44()
    };
    builder.append(nameLiteral(CSSValueMatrix3d), '(', interleave(values, [&](auto& builder, auto& value) {
        serializationForCSS(builder, context, style, Number<> { value });
    }, ", "_s), ')');
}

// MARK: - Shared serializations

inline void ExtractorSerializer::serializeGlyphOrientation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        serializationForCSS(builder, context, state.style, Angle<> { 0_css_deg });
        return;
    case GlyphOrientation::Degrees90:
        serializationForCSS(builder, context, state.style, Angle<> { 90_css_deg });
        return;
    case GlyphOrientation::Degrees180:
        serializationForCSS(builder, context, state.style, Angle<> { 180_css_deg });
        return;
    case GlyphOrientation::Degrees270:
        serializationForCSS(builder, context, state.style, Angle<> { 270_css_deg });
        return;
    case GlyphOrientation::Auto:
        ASSERT_NOT_REACHED();
        serializationForCSS(builder, context, state.style, Angle<> { 0_css_deg });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeGlyphOrientationOrAuto(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        serializationForCSS(builder, context, state.style, Angle<> { 0_css_deg });
        return;
    case GlyphOrientation::Degrees90:
        serializationForCSS(builder, context, state.style, Angle<> { 90_css_deg });
        return;
    case GlyphOrientation::Degrees180:
        serializationForCSS(builder, context, state.style, Angle<> { 180_css_deg });
        return;
    case GlyphOrientation::Degrees270:
        serializationForCSS(builder, context, state.style, Angle<> { 270_css_deg });
        return;
    case GlyphOrientation::Auto:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeMarginTrim(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<MarginTrimType> marginTrim)
{
    if (marginTrim.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    // Try to serialize into one of the "block" or "inline" shorthands
    if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd }) && !marginTrim.containsAny({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd })) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Block { });
        return;
    }
    if (marginTrim.containsAll({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd }) && !marginTrim.containsAny({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd })) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Inline { });
        return;
    }
    if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd, MarginTrimType::InlineStart, MarginTrimType::InlineEnd })) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Block { });
        builder.append(' ');
        serializationForCSS(builder, context, state.style, CSS::Keyword::Inline { });
        return;
    }

    bool listEmpty = true;
    auto appendOption = [&](MarginTrimType test, CSSValueID value) {
        if (marginTrim.contains(test)) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(MarginTrimType::BlockStart, CSSValueBlockStart);
    appendOption(MarginTrimType::InlineStart, CSSValueInlineStart);
    appendOption(MarginTrimType::BlockEnd, CSSValueBlockEnd);
    appendOption(MarginTrimType::InlineEnd, CSSValueInlineEnd);
}


inline void ExtractorSerializer::serializeWebkitTextCombine(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, TextCombine textCombine)
{
    if (textCombine == TextCombine::All) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Horizontal { });
        return;
    }
    serializationForCSS(builder, context, state.style, textCombine);
}

inline void ExtractorSerializer::serializeImageOrientation(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, ImageOrientation imageOrientation)
{
    builder.append(nameLiteralForSerialization(imageOrientation == ImageOrientation::Orientation::FromImage ? CSSValueFromImage : CSSValueNone));
}

inline void ExtractorSerializer::serializeContain(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<Containment> containment)
{
    if (!containment) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }
    if (containment == RenderStyle::strictContainment()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Strict { });
        return;
    }
    if (containment == RenderStyle::contentContainment()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Content { });
        return;
    }

    bool listEmpty = true;
    auto appendOption = [&](Containment test, CSSValueID value) {
        if (containment & test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(Containment::Size, CSSValueSize);
    appendOption(Containment::InlineSize, CSSValueInlineSize);
    appendOption(Containment::Layout, CSSValueLayout);
    appendOption(Containment::Style, CSSValueStyle);
    appendOption(Containment::Paint, CSSValuePaint);
}

inline void ExtractorSerializer::serializeTextSpacingTrim(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, TextSpacingTrim textSpacingTrim)
{
    switch (textSpacingTrim.type()) {
    case TextSpacingTrim::TrimType::SpaceAll:
        serializationForCSS(builder, context, state.style, CSS::Keyword::SpaceAll { });
        return;
    case TextSpacingTrim::TrimType::Auto:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    case TextSpacingTrim::TrimType::TrimAll:
        serializationForCSS(builder, context, state.style, CSS::Keyword::TrimAll { });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeTextAutospace(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, TextAutospace textAutospace)
{
    if (textAutospace.isAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    if (textAutospace.isNoAutospace()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::NoAutospace { });
        return;
    }

    if (textAutospace.isNormal()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    }

    if (textAutospace.hasIdeographAlpha() && textAutospace.hasIdeographNumeric()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::IdeographAlpha { });
        builder.append(' ');
        serializationForCSS(builder, context, state.style, CSS::Keyword::IdeographNumeric { });
        return;
    }

    if (textAutospace.hasIdeographAlpha()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::IdeographAlpha { });
        return;
    }

    if (textAutospace.hasIdeographNumeric()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::IdeographNumeric { });
        return;
    }
}

inline void ExtractorSerializer::serializeLineFitEdge(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TextEdge& textEdge)
{
    if (textEdge.over == TextEdgeType::Leading && textEdge.under == TextEdgeType::Leading) {
        serializationForCSS(builder, context, state.style, textEdge.over);
        return;
    }

    // https://www.w3.org/TR/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeUnderEdge = [&] {
        if (textEdge.over == TextEdgeType::CapHeight || textEdge.over == TextEdgeType::ExHeight)
            return textEdge.under != TextEdgeType::Text;
        return textEdge.over != textEdge.under;
    }();

    if (!shouldSerializeUnderEdge) {
        serializationForCSS(builder, context, state.style, textEdge.over);
        return;
    }

    serializationForCSS(builder, context, state.style, textEdge.over);
    builder.append(' ');
    serializationForCSS(builder, context, state.style, textEdge.under);
}

inline void ExtractorSerializer::serializeTextBoxEdge(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TextEdge& textEdge)
{
    if (textEdge.over == TextEdgeType::Auto && textEdge.under == TextEdgeType::Auto) {
        serializationForCSS(builder, context, state.style, textEdge.over);
        return;
    }

    // https://www.w3.org/TR/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeUnderEdge = [&] {
        if (textEdge.over == TextEdgeType::CapHeight || textEdge.over == TextEdgeType::ExHeight)
            return textEdge.under != TextEdgeType::Text;
        return textEdge.over != textEdge.under;
    }();

    if (!shouldSerializeUnderEdge) {
        serializationForCSS(builder, context, state.style, textEdge.over);
        return;
    }

    serializationForCSS(builder, context, state.style, textEdge.over);
    builder.append(' ');
    serializationForCSS(builder, context, state.style, textEdge.under);
}

inline void ExtractorSerializer::serializePositionTryFallbacks(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<PositionTryFallback>& fallbacks)
{
    if (fallbacks.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    bool needsComma = false;
    for (auto& fallback : fallbacks) {
        if (needsComma) {
            builder.append(", "_s);
            needsComma = false;
        }

        if (fallback.positionAreaProperties) {
            RefPtr areaValue = fallback.positionAreaProperties->getPropertyCSSValue(CSSPropertyPositionArea);
            if (areaValue) {
                builder.append(areaValue->cssText(context));
                needsComma = true;
            }
            continue;
        }

        bool needsSpace = false;
        if (fallback.positionTryRuleName) {
            serializationForCSS(builder, context, state.style, *fallback.positionTryRuleName);
            needsSpace = true;
        }
        for (auto tactic : fallback.tactics) {
            if (needsSpace)
                builder.append(' ');
            serializationForCSS(builder, context, state.style, tactic);
            needsSpace = true;
        }

        needsComma = true;
    }
}

inline void ExtractorSerializer::serializeWillChange(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WillChangeData* willChangeData)
{
    if (!willChangeData || !willChangeData->numFeatures()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

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
    builder.append(CSSValueList::createCommaSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeTabSize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TabSize& tabSize)
{
    auto value = tabSize.widthInPixels(1.0);
    if (tabSize.isSpaces())
        serializationForCSS(builder, context, state.style, Number<> { value });
    else
        serializationForCSS(builder, context, state.style, Length<> { value });
}

inline void ExtractorSerializer::serializeScrollSnapType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScrollSnapType& type)
{
    if (type.strictness == ScrollSnapStrictness::None) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    if (type.strictness == ScrollSnapStrictness::Proximity) {
        serializationForCSS(builder, context, state.style, type.axis);
        return;
    }

    serializationForCSS(builder, context, state.style, type.axis);
    builder.append(' ');
    serializationForCSS(builder, context, state.style, type.strictness);
}

inline void ExtractorSerializer::serializeScrollSnapAlign(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScrollSnapAlign& alignment)
{
    if (alignment.blockAlign == alignment.inlineAlign) {
        serializationForCSS(builder, context, state.style, alignment.blockAlign);
        return;
    }

    serializationForCSS(builder, context, state.style, alignment.blockAlign);
    builder.append(' ');
    serializationForCSS(builder, context, state.style, alignment.inlineAlign);
}

inline void ExtractorSerializer::serializeLineBoxContain(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<Style::LineBoxContain> lineBoxContain)
{
    if (!lineBoxContain) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    bool listEmpty = true;
    auto appendOption = [&](LineBoxContain test, CSSValueID value) {
        if (lineBoxContain.contains(test)) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(LineBoxContain::Block, CSSValueBlock);
    appendOption(LineBoxContain::Inline, CSSValueInline);
    appendOption(LineBoxContain::Font, CSSValueFont);
    appendOption(LineBoxContain::Glyphs, CSSValueGlyphs);
    appendOption(LineBoxContain::Replaced, CSSValueReplaced);
    appendOption(LineBoxContain::InlineBox, CSSValueInlineBox);
    appendOption(LineBoxContain::InitialLetter, CSSValueInitialLetter);
}

inline void ExtractorSerializer::serializeWebkitRubyPosition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, RubyPosition position)
{
    switch (position) {
    case RubyPosition::Over:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Before { });
        return;
    case RubyPosition::Under:
        serializationForCSS(builder, context, state.style, CSS::Keyword::After { });
        return;
    case RubyPosition::InterCharacter:
    case RubyPosition::LegacyInterCharacter:
        serializationForCSS(builder, context, state.style, CSS::Keyword::InterCharacter { });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeTouchAction(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<TouchAction> touchActions)
{
    if (touchActions & TouchAction::Auto) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }
    if (touchActions & TouchAction::None) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }
    if (touchActions & TouchAction::Manipulation) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Manipulation { });
        return;
    }

    bool listEmpty = true;
    auto appendOption = [&](TouchAction test, CSSValueID value) {
        if (touchActions & test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(TouchAction::PanX, CSSValuePanX);
    appendOption(TouchAction::PanY, CSSValuePanY);
    appendOption(TouchAction::PinchZoom, CSSValuePinchZoom);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
}

inline void ExtractorSerializer::serializeTextTransform(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<TextTransform> textTransform)
{
    bool listEmpty = true;

    if (textTransform.contains(TextTransform::Capitalize)) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Capitalize { });
        listEmpty = false;
    } else if (textTransform.contains(TextTransform::Uppercase)) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Uppercase { });
        listEmpty = false;
    } else if (textTransform.contains(TextTransform::Lowercase)) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Lowercase { });
        listEmpty = false;
    }

    auto appendOption = [&](TextTransform test, CSSValueID value) {
        if (textTransform.contains(test)) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(TextTransform::FullWidth, CSSValueFullWidth);
    appendOption(TextTransform::FullSizeKana, CSSValueFullSizeKana);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
}

inline void ExtractorSerializer::serializeTextUnderlinePosition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<TextUnderlinePosition> textUnderlinePosition)
{
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::FromFont) && (textUnderlinePosition & TextUnderlinePosition::Under)));
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::Left) && (textUnderlinePosition & TextUnderlinePosition::Right)));

    if (textUnderlinePosition.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    bool isFromFont = textUnderlinePosition.contains(TextUnderlinePosition::FromFont);
    bool isUnder = textUnderlinePosition.contains(TextUnderlinePosition::Under);
    bool isLeft = textUnderlinePosition.contains(TextUnderlinePosition::Left);
    bool isRight = textUnderlinePosition.contains(TextUnderlinePosition::Right);

    auto metric = isUnder ? CSSValueUnder : CSSValueFromFont;
    auto side = isLeft ? CSSValueLeft : CSSValueRight;
    if (!isFromFont && !isUnder) {
        builder.append(nameLiteralForSerialization(side));
        return;
    }
    if (!isLeft && !isRight) {
        builder.append(nameLiteralForSerialization(metric));
        return;
    }

    builder.append(nameLiteralForSerialization(metric), ' ', nameLiteralForSerialization(side));
}

inline void ExtractorSerializer::serializeTextEmphasisPosition(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, OptionSet<TextEmphasisPosition> textEmphasisPosition)
{
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Over) && (textEmphasisPosition & TextEmphasisPosition::Under)));
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Left) && (textEmphasisPosition & TextEmphasisPosition::Right)));
    ASSERT((textEmphasisPosition & TextEmphasisPosition::Over) || (textEmphasisPosition & TextEmphasisPosition::Under));

    bool listEmpty = true;
    auto appendOption = [&](TextEmphasisPosition test, CSSValueID value) {
        if (textEmphasisPosition &  test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(TextEmphasisPosition::Over, CSSValueOver);
    appendOption(TextEmphasisPosition::Under, CSSValueUnder);
    appendOption(TextEmphasisPosition::Left, CSSValueLeft);
}

inline void ExtractorSerializer::serializeSpeakAs(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<SpeakAs> speakAs)
{
    bool listEmpty = true;
    auto appendOption = [&](SpeakAs test, CSSValueID value) {
        if (speakAs &  test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(SpeakAs::SpellOut, CSSValueSpellOut);
    appendOption(SpeakAs::Digits, CSSValueDigits);
    appendOption(SpeakAs::LiteralPunctuation, CSSValueLiteralPunctuation);
    appendOption(SpeakAs::NoPunctuation, CSSValueNoPunctuation);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
}

inline void ExtractorSerializer::serializeHangingPunctuation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<HangingPunctuation> hangingPunctuation)
{
    bool listEmpty = true;
    auto appendOption = [&](HangingPunctuation test, CSSValueID value) {
        if (hangingPunctuation &  test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(HangingPunctuation::First, CSSValueFirst);
    appendOption(HangingPunctuation::AllowEnd, CSSValueAllowEnd);
    appendOption(HangingPunctuation::ForceEnd, CSSValueForceEnd);
    appendOption(HangingPunctuation::Last, CSSValueLast);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
}

inline void ExtractorSerializer::serializePageBreak(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, BreakBetween value)
{
    if (value == BreakBetween::Page || value == BreakBetween::LeftPage || value == BreakBetween::RightPage
        || value == BreakBetween::RectoPage || value == BreakBetween::VersoPage) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Always { }); // CSS 2.1 allows us to map these to always.
        return;
    }
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidPage) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Avoid { });
        return;
    }
    serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
}

inline void ExtractorSerializer::serializePageBreak(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidPage) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Avoid { });
        return;
    }
    serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
}

inline void ExtractorSerializer::serializeWebkitColumnBreak(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, BreakBetween value)
{
    if (value == BreakBetween::Column) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Always { });
        return;
    }
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidColumn) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Avoid { });
        return;
    }
    serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
}

inline void ExtractorSerializer::serializeWebkitColumnBreak(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidColumn) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Avoid { });
        return;
    }
    serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
}

inline void ExtractorSerializer::serializeSelfOrDefaultAlignmentData(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleSelfAlignmentData& data)
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
            list.append(ExtractorConverter::convertStyleType(state, data.overflow()));
        if (data.position() == ItemPosition::Legacy)
            list.append(CSSPrimitiveValue::create(CSSValueNormal));
        else
            list.append(ExtractorConverter::convertStyleType(state, data.position()));
    }
    builder.append(CSSValueList::createSpaceSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeContentAlignmentData(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleContentAlignmentData& data)
{
    CSSValueListBuilder list;

    // Handle content-distribution values
    if (data.distribution() != ContentDistribution::Default)
        list.append(ExtractorConverter::convertStyleType(state, data.distribution()));

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
            list.append(ExtractorConverter::convertStyleType(state, data.overflow()));
        list.append(ExtractorConverter::convertStyleType(state, data.position()));
    }

    ASSERT(list.size() > 0);
    ASSERT(list.size() <= 3);
    builder.append(CSSValueList::createSpaceSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializePaintOrder(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, PaintOrder paintOrder)
{
    if (paintOrder == PaintOrder::Normal) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    }

    auto appendOne = [&](auto a) {
        builder.append(nameLiteralForSerialization(a));
    };

    auto appendTwo = [&](auto a, auto b) {
        builder.append(nameLiteralForSerialization(a), ' ', nameLiteralForSerialization(b));
    };

    switch (paintOrder) {
    case PaintOrder::Normal:
        ASSERT_NOT_REACHED();
        return;
    case PaintOrder::Fill:
        appendOne(CSSValueFill);
        return;
    case PaintOrder::FillMarkers:
        appendTwo(CSSValueFill, CSSValueMarkers);
        return;
    case PaintOrder::Stroke:
        appendOne(CSSValueStroke);
        return;
    case PaintOrder::StrokeMarkers:
        appendTwo(CSSValueStroke, CSSValueMarkers);
        return;
    case PaintOrder::Markers:
        appendOne(CSSValueMarkers);
        return;
    case PaintOrder::MarkersStroke:
        appendTwo(CSSValueMarkers, CSSValueStroke);
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializePositionAnchor(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const std::optional<ScopedName>& positionAnchor)
{
    if (!positionAnchor) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    serializationForCSS(builder, context, state.style, *positionAnchor);
}

inline void ExtractorSerializer::serializePositionArea(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const std::optional<PositionArea>& positionArea)
{
    if (!positionArea) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(ExtractorConverter::convertPositionArea(state, *positionArea)->cssText(context));
}

inline void ExtractorSerializer::serializeNameScope(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const NameScope& scope)
{
    switch (scope.type) {
    case NameScope::Type::None:
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    case NameScope::Type::All:
        serializationForCSS(builder, context, state.style, CSS::Keyword::All { });
        return;
    case NameScope::Type::Ident:
        if (scope.names.isEmpty()) {
            serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
            return;
        }

        builder.append(interleave(scope.names, [&](auto& builder, auto& name) {
            serializationForCSS(builder, context, state.style, CustomIdentifier { name });
        }, ", "_s));
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializePositionVisibility(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<PositionVisibility> positionVisibility)
{
    bool listEmpty = true;
    auto appendOption = [&](PositionVisibility test, CSSValueID value) {
        if (positionVisibility & test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(PositionVisibility::AnchorsValid, CSSValueAnchorsValid);
    appendOption(PositionVisibility::AnchorsVisible, CSSValueAnchorsVisible);
    appendOption(PositionVisibility::NoOverflow, CSSValueNoOverflow);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::Always { });
}

// MARK: - FillLayer serializations

inline void ExtractorSerializer::serializeFillLayerMaskComposite(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, CompositeOperator composite)
{
    builder.append(nameLiteralForSerialization(toCSSValueID(composite, CSSPropertyMaskComposite)));
}

inline void ExtractorSerializer::serializeFillLayerWebkitMaskComposite(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, CompositeOperator composite)
{
    builder.append(nameLiteralForSerialization(toCSSValueID(composite, CSSPropertyWebkitMaskComposite)));
}

inline void ExtractorSerializer::serializeFillLayerMaskMode(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, MaskMode maskMode)
{
    switch (maskMode) {
    case MaskMode::Alpha:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alpha { });
        return;
    case MaskMode::Luminance:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Luminance { });
        return;
    case MaskMode::MatchSource:
        serializationForCSS(builder, context, state.style, CSS::Keyword::MatchSource { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeFillLayerWebkitMaskSourceType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, MaskMode maskMode)
{
    switch (maskMode) {
    case MaskMode::Alpha:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alpha { });
        return;
    case MaskMode::Luminance:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Luminance { });
        return;
    case MaskMode::MatchSource:
        // MatchSource is only available in the mask-mode property.
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alpha { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Font serializations

inline void ExtractorSerializer::serializeFontFamily(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, const AtomString& family)
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
        builder.append(nameLiteralForSerialization(familyIdentifier));
    else
        builder.append(WebCore::serializeFontFamily(family));
}

inline void ExtractorSerializer::serializeFontSizeAdjust(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FontSizeAdjust& fontSizeAdjust)
{
    if (fontSizeAdjust.isNone()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    auto metric = fontSizeAdjust.metric;
    auto value = fontSizeAdjust.shouldResolveFromFont() ? fontSizeAdjust.resolve(state.style.computedFontSize(), state.style.metricsOfPrimaryFont()) : fontSizeAdjust.value.asOptional();

    if (!value) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    if (metric == FontSizeAdjust::Metric::ExHeight) {
        serializationForCSS(builder, context, state.style, Number<> { *value });
        return;
    }

    serializationForCSS(builder, context, state.style, metric);
    builder.append(' ');
    serializationForCSS(builder, context, state.style, Number<> { *value });
}

inline void ExtractorSerializer::serializeFontPalette(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FontPalette& fontPalette)
{
    switch (fontPalette.type) {
    case FontPalette::Type::Normal:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    case FontPalette::Type::Light:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Light { });
        return;
    case FontPalette::Type::Dark:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Dark { });
        return;
    case FontPalette::Type::Custom:
        serializationForCSS(builder, context, state.style, CustomIdentifier { fontPalette.identifier });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeFontWeight(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FontSelectionValue fontWeight)
{
    serializationForCSS(builder, context, state.style, Number<> { static_cast<float>(fontWeight) });
}

inline void ExtractorSerializer::serializeFontWidth(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FontSelectionValue fontWidth)
{
    serializationForCSS(builder, context, state.style, Percentage<> { static_cast<float>(fontWidth) });
}

inline void ExtractorSerializer::serializeFontFeatureSettings(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FontFeatureSettings& fontFeatureSettings)
{
    if (!fontFeatureSettings.size()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    }

    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.

    CSSValueListBuilder list;
    for (auto& feature : fontFeatureSettings)
        list.append(CSSFontFeatureValue::create(FontTag(feature.tag()), ExtractorConverter::convert(state, feature.value())));
    builder.append(CSSValueList::createCommaSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeFontVariationSettings(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FontVariationSettings& fontVariationSettings)
{
    if (fontVariationSettings.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    }

    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.

    CSSValueListBuilder list;
    for (auto& feature : fontVariationSettings)
        list.append(CSSFontVariationValue::create(feature.tag(), ExtractorConverter::convert(state, feature.value())));
    builder.append(CSSValueList::createCommaSeparated(WTFMove(list))->cssText(context));
}

// MARK: - Grid serializations

inline void ExtractorSerializer::serializeGridAutoFlow(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, GridAutoFlow gridAutoFlow)
{
    ASSERT(gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionRow) || gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionColumn));

    bool needsSpace = false;

    if (gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionColumn)) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Column { });
        needsSpace = true;
    } else if (!(gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowAlgorithmDense))) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Row { });
        needsSpace = true;
    }

    if (gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowAlgorithmDense)) {
        if (needsSpace)
            builder.append(' ');
        serializationForCSS(builder, context, state.style, CSS::Keyword::Dense { });
    }
}

} // namespace Style
} // namespace WebCore
