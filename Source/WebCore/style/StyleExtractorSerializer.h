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

#include "CSSDynamicRangeLimitMix.h"
#include "CSSMarkup.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSValueTypes.h"
#include "ColorSerialization.h"
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
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScopedName&);

    static void serializeLength(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    static void serializeLength(const RenderStyle&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    static void serializeLengthAllowingNumber(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    static void serializeLengthOrAuto(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);

    template<typename T> static void serializeNumber(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);
    template<typename T> static void serializeNumberAsPixels(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);
    template<typename T> static void serializeComputedLength(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);
    template<typename T> static void serializeLineWidth(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T lineWidth);

    template<CSSValueID> static void serializeCustomIdentAtomOrKeyword(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const AtomString&);

    // MARK: SVG serializations

    static void serializeSVGURIReference(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const URL&);

    // MARK: Transform serializations

    static void serializeTransformationMatrix(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TransformationMatrix&);
    static void serializeTransformationMatrix(const RenderStyle&, StringBuilder&, const CSS::SerializationContext&, const TransformationMatrix&);
    static void serializeTransformOperation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TransformOperation&);
    static void serializeTransformOperation(const RenderStyle&, StringBuilder&, const CSS::SerializationContext&, const TransformOperation&);

    // MARK: Shared serializations

    static void serializeImageOrNone(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StyleImage*);
    static void serializeGlyphOrientation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, GlyphOrientation);
    static void serializeGlyphOrientationOrAuto(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, GlyphOrientation);
    static void serializeMarginTrim(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<MarginTrimType>);
    static void serializeDPath(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StylePathData*);
    static void serializeStrokeDashArray(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<WebCore::Length>&);
    static void serializeFilterOperations(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FilterOperations&);
    static void serializeAppleColorFilterOperations(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FilterOperations&);
    static void serializeWebkitTextCombine(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, TextCombine);
    static void serializeImageOrientation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, ImageOrientation);
    static void serializeLineClamp(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const LineClampValue&);
    static void serializeContain(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<Containment>);
    static void serializeSmoothScrolling(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, bool);
    static void serializeInitialLetter(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FloatSize);
    static void serializeTextSpacingTrim(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, TextSpacingTrim);
    static void serializeTextAutospace(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, TextAutospace);
    static void serializeReflection(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StyleReflection*);
    static void serializeLineFitEdge(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TextEdge&);
    static void serializeTextBoxEdge(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TextEdge&);
    static void serializePositionTryFallbacks(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<PositionTryFallback>&);
    static void serializeWillChange(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WillChangeData*);
    static void serializeTabSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TabSize&);
    static void serializeScrollSnapType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScrollSnapType&);
    static void serializeScrollSnapAlign(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScrollSnapAlign&);
    static void serializeLineBoxContain(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<Style::LineBoxContain>);
    static void serializeWebkitRubyPosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, RubyPosition);
    static void serializePosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const LengthPoint&);
    static void serializeTouchAction(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TouchAction>);
    static void serializeTextTransform(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextTransform>);
    static void serializeTextDecorationLine(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextDecorationLine>);
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

    static void serializeFillLayerAttachment(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillAttachment);
    static void serializeFillLayerBlendMode(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, BlendMode);
    static void serializeFillLayerClip(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillBox);
    static void serializeFillLayerOrigin(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillBox);
    static void serializeFillLayerRepeat(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillRepeatXY);
    static void serializeFillLayerBackgroundSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillSize);
    static void serializeFillLayerMaskSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillSize);
    static void serializeFillLayerMaskComposite(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, CompositeOperator);
    static void serializeFillLayerWebkitMaskComposite(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, CompositeOperator);
    static void serializeFillLayerMaskMode(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, MaskMode);
    static void serializeFillLayerWebkitMaskSourceType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, MaskMode);
    static void serializeFillLayerImage(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StyleImage*);

    // MARK: Font serializations

    static void serializeFontFamily(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const AtomString&);
    static void serializeFontSizeAdjust(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontSizeAdjust&);
    static void serializeFontPalette(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontPalette&);
    static void serializeFontWeight(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FontSelectionValue);
    static void serializeFontWidth(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FontSelectionValue);
    static void serializeFontFeatureSettings(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontFeatureSettings&);
    static void serializeFontVariationSettings(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontVariationSettings&);

    // MARK: Animation/Transition serializations

    static void serializeAnimationName(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScopedName&, const Animation*, const AnimationList*);
    static void serializeAnimationProperty(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const Animation::TransitionProperty&, const Animation*, const AnimationList*);
    static void serializeAnimationAllowsDiscreteTransitions(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, bool, const Animation*, const AnimationList*);
    static void serializeAnimationDuration(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, Markable<double>, const Animation*, const AnimationList*);
    static void serializeAnimationDelay(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, double, const Animation*, const AnimationList*);
    static void serializeAnimationIterationCount(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, double, const Animation*, const AnimationList*);
    static void serializeAnimationDirection(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, Animation::Direction, const Animation*, const AnimationList*);
    static void serializeAnimationFillMode(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, AnimationFillMode, const Animation*, const AnimationList*);
    static void serializeAnimationCompositeOperation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, CompositeOperation, const Animation*, const AnimationList*);
    static void serializeAnimationPlayState(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, AnimationPlayState, const Animation*, const AnimationList*);
    static void serializeAnimationTimeline(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const Animation::Timeline&, const Animation*, const AnimationList*);
    static void serializeAnimationTimingFunction(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TimingFunction&, const Animation*, const AnimationList*);
    static void serializeAnimationTimingFunction(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TimingFunction*, const Animation*, const AnimationList*);
    static void serializeAnimationSingleRange(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const SingleTimelineRange&, SingleTimelineRange::Type);
    static void serializeAnimationRangeStart(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const SingleTimelineRange&, const Animation*, const AnimationList*);
    static void serializeAnimationRangeEnd(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const SingleTimelineRange&, const Animation*, const AnimationList*);
    static void serializeAnimationRange(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TimelineRange&, const Animation*, const AnimationList*);
    static void serializeSingleAnimation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const Animation&);
    static void serializeSingleTransition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const Animation&);

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

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, double value)
{
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, float value)
{
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, unsigned value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, unsigned> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, int value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, int> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, unsigned short value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, unsigned short> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, short value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, short> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScopedName& scopedName)
{
    if (scopedName.isIdentifier)
        serializationForCSS(builder, context, state.style, CustomIdentifier { scopedName.name });
    else
        serializationForCSS(builder, context, state.style, scopedName.name);
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
        CSS::serializationForCSS(builder, context, CSS::LengthRaw<> { CSS::LengthUnit::Px, adjustFloatForAbsoluteZoom(length.value(), style) });
        return;
    case LengthType::Percent:
        CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { length.value() });
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

inline void ExtractorSerializer::serializeLengthAllowingNumber(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& length)
{
    serializeLength(state, builder, context, length);
}

inline void ExtractorSerializer::serializeLengthOrAuto(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& length)
{
    serializeLength(state, builder, context, length);
}

template<typename T> void ExtractorSerializer::serializeNumber(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    serialize(state, builder, context, number);
}

template<typename T> void ExtractorSerializer::serializeNumberAsPixels(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    CSS::serializationForCSS(builder, context, CSS::LengthRaw<> { CSS::LengthUnit::Px, adjustFloatForAbsoluteZoom(number, state.style) });
}

template<typename T> void ExtractorSerializer::serializeComputedLength(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    serializeNumberAsPixels(state, builder, context, number);
}

template<typename T> void ExtractorSerializer::serializeLineWidth(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T lineWidth)
{
    serializeNumberAsPixels(state, builder, context, lineWidth);
}

template<CSSValueID keyword> void ExtractorSerializer::serializeCustomIdentAtomOrKeyword(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const AtomString& string)
{
    if (string.isNull()) {
        serializationForCSS(builder, context, state.style, Constant<keyword> { });
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
            CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
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
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
    }, ", "_s), ')');
}

inline void ExtractorSerializer::serializeTransformOperation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TransformOperation& operation)
{
    serializeTransformOperation(state.style, builder, context, operation);
}

inline void ExtractorSerializer::serializeTransformOperation(const RenderStyle& style, StringBuilder& builder, const CSS::SerializationContext& context, const TransformOperation& operation)
{
    auto translateLength = [&](const auto& length) {
        if (length.isZero()) {
            builder.append("0px"_s);
            return;
        }
        serializeLength(style, builder, context, length);
    };

    auto translateAngle = [&](auto angle) {
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { CSS::AngleUnit::Deg, angle });
    };

    auto translateNumber = [&](auto number) {
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { number });
    };


    auto includeLength = [](const auto& length) -> bool {
        return !length.isZero() || length.isPercent();
    };

    switch (operation.type()) {
    case TransformOperation::Type::TranslateX:
        builder.append(nameLiteral(CSSValueTranslateX), '(');
        translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).x());
        builder.append(')');
        return;
    case TransformOperation::Type::TranslateY:
        builder.append(nameLiteral(CSSValueTranslateY), '(');
        translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).y());
        builder.append(')');
        return;
    case TransformOperation::Type::TranslateZ:
        builder.append(nameLiteral(CSSValueTranslateZ), '(');
        translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).z());
        builder.append(')');
        return;
    case TransformOperation::Type::Translate:
    case TransformOperation::Type::Translate3D: {
        auto& translate = uncheckedDowncast<TranslateTransformOperation>(operation);
        if (!translate.is3DOperation()) {
            if (!includeLength(translate.y())) {
                builder.append(nameLiteral(CSSValueTranslate), '(');
                translateLength(translate.x());
                builder.append(')');
                return;
            }
            builder.append(nameLiteral(CSSValueTranslate), '(');
            translateLength(translate.x());
            builder.append(", "_s);
            translateLength(translate.y());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueTranslate3d), '(');
        translateLength(translate.x());
        builder.append(", "_s);
        translateLength(translate.y());
        builder.append(", "_s);
        translateLength(translate.z());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::ScaleX:
        builder.append(nameLiteral(CSSValueScaleX), '(');
        translateNumber(uncheckedDowncast<ScaleTransformOperation>(operation).x());
        builder.append(')');
        return;
    case TransformOperation::Type::ScaleY:
        builder.append(nameLiteral(CSSValueScaleY), '(');
        translateNumber(uncheckedDowncast<ScaleTransformOperation>(operation).y());
        builder.append(')');
        return;
    case TransformOperation::Type::ScaleZ:
        builder.append(nameLiteral(CSSValueScaleZ), '(');
        translateNumber(uncheckedDowncast<ScaleTransformOperation>(operation).z());
        builder.append(')');
        return;
    case TransformOperation::Type::Scale:
    case TransformOperation::Type::Scale3D: {
        auto& scale = uncheckedDowncast<ScaleTransformOperation>(operation);
        if (!scale.is3DOperation()) {
            if (scale.x() == scale.y()) {
                builder.append(nameLiteral(CSSValueScale), '(');
                translateNumber(scale.x());
                builder.append(')');
                return;
            }
            builder.append(nameLiteral(CSSValueScale), '(');
            translateNumber(scale.x());
            builder.append(", "_s);
            translateNumber(scale.y());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueScale3d), '(');
        translateNumber(scale.x());
        builder.append(", "_s);
        translateNumber(scale.y());
        builder.append(", "_s);
        translateNumber(scale.z());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::RotateX:
        builder.append(nameLiteral(CSSValueRotateX), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::RotateY:
        builder.append(nameLiteral(CSSValueRotateY), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::RotateZ:
        builder.append(nameLiteral(CSSValueRotateZ), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::Rotate:
        builder.append(nameLiteral(CSSValueRotate), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::Rotate3D: {
        auto& rotate = uncheckedDowncast<RotateTransformOperation>(operation);
        builder.append(nameLiteral(CSSValueRotate3d), '(');
        translateNumber(rotate.x());
        builder.append(", "_s);
        translateNumber(rotate.y());
        builder.append(", "_s);
        translateNumber(rotate.z());
        builder.append(", "_s);
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::SkewX:
        builder.append(nameLiteral(CSSValueSkewX), '(');
        translateAngle(uncheckedDowncast<SkewTransformOperation>(operation).angleX());
        builder.append(')');
        return;
    case TransformOperation::Type::SkewY:
        builder.append(nameLiteral(CSSValueSkewY), '(');
        translateAngle(uncheckedDowncast<SkewTransformOperation>(operation).angleY());
        builder.append(')');
        return;
    case TransformOperation::Type::Skew: {
        auto& skew = uncheckedDowncast<SkewTransformOperation>(operation);
        if (!skew.angleY()) {
            builder.append(nameLiteral(CSSValueSkew), '(');
            translateAngle(skew.angleX());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueSkew), '(');
        translateAngle(skew.angleX());
        builder.append(", "_s);
        translateAngle(skew.angleY());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::Perspective:
        if (auto perspective = uncheckedDowncast<PerspectiveTransformOperation>(operation).perspective()) {
            builder.append(nameLiteral(CSSValuePerspective), '(');
            serializeLength(style, builder, context, *perspective);
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValuePerspective), '(', nameLiteralForSerialization(CSSValueNone), ')');
        return;
    case TransformOperation::Type::Matrix:
    case TransformOperation::Type::Matrix3D: {
        TransformationMatrix transform;
        operation.apply(transform, { });
        serializeTransformationMatrix(style, builder, context, transform);
        return;
    }
    case TransformOperation::Type::Identity:
    case TransformOperation::Type::None:
        ASSERT_NOT_REACHED();
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Shared serializations

inline void ExtractorSerializer::serializeImageOrNone(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleImage* image)
{
    if (!image) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    builder.append(image->computedStyleValue(state.style)->cssText(context));
}

inline void ExtractorSerializer::serializeGlyphOrientation(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 0_css_deg });
        return;
    case GlyphOrientation::Degrees90:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 90_css_deg });
        return;
    case GlyphOrientation::Degrees180:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 180_css_deg });
        return;
    case GlyphOrientation::Degrees270:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 270_css_deg });
        return;
    case GlyphOrientation::Auto:
        ASSERT_NOT_REACHED();
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 0_css_deg });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeGlyphOrientationOrAuto(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 0_css_deg });
        return;
    case GlyphOrientation::Degrees90:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 90_css_deg });
        return;
    case GlyphOrientation::Degrees180:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 180_css_deg });
        return;
    case GlyphOrientation::Degrees270:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 270_css_deg });
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

inline void ExtractorSerializer::serializeDPath(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StylePathData* path)
{
    if (!path) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    serializationForCSS(builder, context, state.style, Ref { *path }->path(), PathConversion::ForceAbsolute);
}

inline void ExtractorSerializer::serializeStrokeDashArray(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<WebCore::Length>& dashes)
{
    if (dashes.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    builder.append(interleave(dashes, [&](auto& builder, auto& dash) {
        serializeLength(state, builder, context, dash);
    }, ", "_s));
}

inline void ExtractorSerializer::serializeFilterOperations(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FilterOperations& filterOperations)
{
    CSS::serializationForCSS(builder, context, toCSSFilterProperty(filterOperations, state.style));
}

inline void ExtractorSerializer::serializeAppleColorFilterOperations(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FilterOperations& filterOperations)
{
    CSS::serializationForCSS(builder, context, toCSSAppleColorFilterProperty(filterOperations, state.style));
}

inline void ExtractorSerializer::serializeWebkitTextCombine(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, TextCombine textCombine)
{
    if (textCombine == TextCombine::All) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Horizontal { });
        return;
    }
    serialize(state, builder, context, textCombine);
}

inline void ExtractorSerializer::serializeImageOrientation(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, ImageOrientation imageOrientation)
{
    builder.append(nameLiteralForSerialization(imageOrientation == ImageOrientation::Orientation::FromImage ? CSSValueFromImage : CSSValueNone));
}

inline void ExtractorSerializer::serializeLineClamp(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const LineClampValue& lineClamp)
{
    if (lineClamp.isNone()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }
    if (lineClamp.isPercentage()) {
        CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { lineClamp.value() });
        return;
    }
    serialize(state, builder, context, lineClamp.value());
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

inline void ExtractorSerializer::serializeInitialLetter(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FloatSize initialLetter)
{
    auto append = [&](auto axis) {
        if (!axis)
            serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        else
            CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { axis });
    };

    if (initialLetter.width() == initialLetter.height()) {
        append(initialLetter.width());
        return;
    }

    append(initialLetter.width());
    builder.append(' ');
    append(initialLetter.height());
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

inline void ExtractorSerializer::serializeReflection(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleReflection* reflection)
{
    if (!reflection) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    // FIXME: Consider omitting 0px when the mask is null.

    auto offset = [&] -> Ref<CSSPrimitiveValue> {
        auto& reflectionOffset = reflection->offset();
        if (reflectionOffset.isPercentOrCalculated())
            return CSSPrimitiveValue::create(reflectionOffset.percent(), CSSUnitType::CSS_PERCENTAGE);
        return ExtractorConverter::convertNumberAsPixels(state, reflectionOffset.value());
    }();

    auto mask = [&] -> RefPtr<CSSValue> {
        auto& reflectionMask = reflection->mask();
        if (reflectionMask.source().isNone())
            return CSSPrimitiveValue::create(CSSValueNone);
        return createCSSValue(state.pool, state.style, reflectionMask);
    }();

    builder.append(CSSReflectValue::create(
        toCSSValueID(reflection->direction()),
        WTFMove(offset),
        WTFMove(mask)
    )->cssText(context));
}

inline void ExtractorSerializer::serializeLineFitEdge(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TextEdge& textEdge)
{
    if (textEdge.over == TextEdgeType::Leading && textEdge.under == TextEdgeType::Leading) {
        serialize(state, builder, context, textEdge.over);
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
        serialize(state, builder, context, textEdge.over);
        return;
    }

    serialize(state, builder, context, textEdge.over);
    builder.append(' ');
    serialize(state, builder, context, textEdge.under);
}

inline void ExtractorSerializer::serializeTextBoxEdge(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TextEdge& textEdge)
{
    if (textEdge.over == TextEdgeType::Auto && textEdge.under == TextEdgeType::Auto) {
        serialize(state, builder, context, textEdge.over);
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
        serialize(state, builder, context, textEdge.over);
        return;
    }

    serialize(state, builder, context, textEdge.over);
    builder.append(' ');
    serialize(state, builder, context, textEdge.under);
}

inline void ExtractorSerializer::serializePositionTryFallbacks(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<PositionTryFallback>& fallbacks)
{
    if (fallbacks.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

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
            singleFallbackList.append(ExtractorConverter::convert(state, *fallback.positionTryRuleName));
        for (auto& tactic : fallback.tactics)
            singleFallbackList.append(ExtractorConverter::convert(state, tactic));
        list.append(CSSValueList::createSpaceSeparated(singleFallbackList));
    }

    builder.append(CSSValueList::createCommaSeparated(WTFMove(list))->cssText(context));
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

inline void ExtractorSerializer::serializeTabSize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, const TabSize& tabSize)
{
    auto value = tabSize.widthInPixels(1.0);
    if (tabSize.isSpaces())
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
    else
        CSS::serializationForCSS(builder, context, CSS::LengthRaw<> { CSS::LengthUnit::Px, value });
}

inline void ExtractorSerializer::serializeScrollSnapType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScrollSnapType& type)
{
    if (type.strictness == ScrollSnapStrictness::None) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    if (type.strictness == ScrollSnapStrictness::Proximity) {
        serialize(state, builder, context, type.axis);
        return;
    }

    serialize(state, builder, context, type.axis);
    builder.append(' ');
    serialize(state, builder, context, type.strictness);
}

inline void ExtractorSerializer::serializeScrollSnapAlign(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScrollSnapAlign& alignment)
{
    if (alignment.blockAlign == alignment.inlineAlign) {
        serialize(state, builder, context, alignment.blockAlign);
        return;
    }

    serialize(state, builder, context, alignment.blockAlign);
    builder.append(' ');
    serialize(state, builder, context, alignment.inlineAlign);
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

inline void ExtractorSerializer::serializePosition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const LengthPoint& position)
{
    serializeLength(state, builder, context, position.x);
    builder.append(' ');
    serializeLength(state, builder, context, position.y);
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

inline void ExtractorSerializer::serializeTextDecorationLine(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<TextDecorationLine> textDecorationLine)
{
    if (textDecorationLine.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }
    if (textDecorationLine & TextDecorationLine::SpellingError) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::SpellingError { });
        return;
    }
    bool listEmpty = true;
    auto appendOption = [&](TextDecorationLine test, CSSValueID value) {
        if (textDecorationLine & test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(TextDecorationLine::Underline, CSSValueUnderline);
    appendOption(TextDecorationLine::Overline, CSSValueOverline);
    appendOption(TextDecorationLine::LineThrough, CSSValueLineThrough);
    // Blink value is ignored for rendering but not for the computed value.
    appendOption(TextDecorationLine::Blink, CSSValueBlink);

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
            list.append(ExtractorConverter::convert(state, data.overflow()));
        if (data.position() == ItemPosition::Legacy)
            list.append(CSSPrimitiveValue::create(CSSValueNormal));
        else
            list.append(ExtractorConverter::convert(state, data.position()));
    }
    builder.append(CSSValueList::createSpaceSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeContentAlignmentData(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleContentAlignmentData& data)
{
    CSSValueListBuilder list;

    // Handle content-distribution values
    if (data.distribution() != ContentDistribution::Default)
        list.append(ExtractorConverter::convert(state, data.distribution()));

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
            list.append(ExtractorConverter::convert(state, data.overflow()));
        list.append(ExtractorConverter::convert(state, data.position()));
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

    serialize(state, builder, context, *positionAnchor);
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

inline void ExtractorSerializer::serializeFillLayerAttachment(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillAttachment attachment)
{
    serialize(state, builder, context, attachment);
}

inline void ExtractorSerializer::serializeFillLayerBlendMode(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, BlendMode blendMode)
{
    serialize(state, builder, context, blendMode);
}

inline void ExtractorSerializer::serializeFillLayerClip(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillBox clip)
{
    serialize(state, builder, context, clip);
}

inline void ExtractorSerializer::serializeFillLayerOrigin(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillBox origin)
{
    serialize(state, builder, context, origin);
}

inline void ExtractorSerializer::serializeFillLayerRepeat(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillRepeatXY repeat)
{
    if (repeat.x == repeat.y) {
        serialize(state, builder, context, repeat.x);
        return;
    }

    if (repeat.x == FillRepeat::Repeat && repeat.y == FillRepeat::NoRepeat) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::RepeatX { });
        return;
    }

    if (repeat.x == FillRepeat::NoRepeat && repeat.y == FillRepeat::Repeat) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::RepeatY { });
        return;
    }

    serialize(state, builder, context, repeat.x);
    builder.append(' ');
    serialize(state, builder, context, repeat.y);
}

inline void ExtractorSerializer::serializeFillLayerBackgroundSize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillSize size)
{
    if (size.type == FillSizeType::Contain) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Contain { });
        return;
    }

    if (size.type == FillSizeType::Cover) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Cover { });
        return;
    }

    if (size.size.height.isAuto() && size.size.width.isAuto()) {
        serializeLength(state, builder, context, size.size.width);
        return;
    }

    serializeLength(state, builder, context, size.size.width);
    builder.append(' ');
    serializeLength(state, builder, context, size.size.height);
}

inline void ExtractorSerializer::serializeFillLayerMaskSize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillSize size)
{
    if (size.type == FillSizeType::Contain) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Contain { });
        return;
    }

    if (size.type == FillSizeType::Cover) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Cover { });
        return;
    }

    if (size.size.height.isAuto()) {
        serializeLength(state, builder, context, size.size.width);
        return;
    }

    serializeLength(state, builder, context, size.size.width);
    builder.append(' ');
    serializeLength(state, builder, context, size.size.height);
}

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

inline void ExtractorSerializer::serializeFillLayerImage(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleImage* image)
{
    serializeImageOrNone(state, builder, context, image);
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
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { *value });
        return;
    }

    serialize(state, builder, context, metric);
    builder.append(' ');
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { *value });
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

inline void ExtractorSerializer::serializeFontWeight(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, FontSelectionValue fontWeight)
{
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { static_cast<float>(fontWeight) });
}

inline void ExtractorSerializer::serializeFontWidth(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, FontSelectionValue fontWidth)
{
    CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { static_cast<float>(fontWidth) });
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

// MARK: - Animation/Transition serializations

inline void ExtractorSerializer::serializeAnimationName(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScopedName& name, const Animation*, const AnimationList*)
{
    serialize(state, builder, context, name);
}

inline void ExtractorSerializer::serializeAnimationProperty(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const Animation::TransitionProperty& property, const Animation*, const AnimationList*)
{
    switch (property.mode) {
    case Animation::TransitionMode::None:
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    case Animation::TransitionMode::All:
        serializationForCSS(builder, context, state.style, CSS::Keyword::All { });
        return;
    case Animation::TransitionMode::SingleProperty:
    case Animation::TransitionMode::UnknownProperty:
        serializationForCSS(builder, context, state.style, CustomIdentifier { animatablePropertyAsString(property.animatableProperty) });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeAnimationAllowsDiscreteTransitions(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, bool allowsDiscreteTransitions, const Animation*, const AnimationList*)
{
    builder.append(nameLiteralForSerialization(allowsDiscreteTransitions ? CSSValueAllowDiscrete : CSSValueNormal));
}

inline void ExtractorSerializer::serializeAnimationDuration(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, Markable<double> duration, const Animation* animation, const AnimationList* animationList)
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
    if (!duration && (animationListHasMultipleExplicitTimelines() || animationHasExplicitNonAutoTimeline())) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    CSS::serializationForCSS(builder, context, CSS::TimeRaw<> { CSS::TimeUnit::S, duration.value_or(0) });
}

inline void ExtractorSerializer::serializeAnimationDelay(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, double delay, const Animation*, const AnimationList*)
{
    CSS::serializationForCSS(builder, context, CSS::TimeRaw<> { CSS::TimeUnit::S, delay });
}

inline void ExtractorSerializer::serializeAnimationIterationCount(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, double iterationCount, const Animation*, const AnimationList*)
{
    if (iterationCount == Animation::IterationCountInfinite)
        serializationForCSS(builder, context, state.style, CSS::Keyword::Infinite { });
    else
        serialize(state, builder, context, iterationCount);
}

inline void ExtractorSerializer::serializeAnimationDirection(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, Animation::Direction direction, const Animation*, const AnimationList*)
{
    switch (direction) {
    case Animation::Direction::Normal:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    case Animation::Direction::Alternate:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alternate { });
        return;
    case Animation::Direction::Reverse:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Reverse { });
        return;
    case Animation::Direction::AlternateReverse:
        serializationForCSS(builder, context, state.style, CSS::Keyword::AlternateReverse { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeAnimationFillMode(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, AnimationFillMode fillMode, const Animation*, const AnimationList*)
{
    switch (fillMode) {
    case AnimationFillMode::None:
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    case AnimationFillMode::Forwards:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Forwards { });
        return;
    case AnimationFillMode::Backwards:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Backwards { });
        return;
    case AnimationFillMode::Both:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Both { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeAnimationCompositeOperation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, CompositeOperation operation, const Animation*, const AnimationList*)
{
    switch (operation) {
    case CompositeOperation::Add:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Add { });
        return;
    case CompositeOperation::Accumulate:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Accumulate { });
        return;
    case CompositeOperation::Replace:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Replace { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeAnimationPlayState(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, AnimationPlayState playState, const Animation*, const AnimationList*)
{
    switch (playState) {
    case AnimationPlayState::Playing:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Running { });
        return;
    case AnimationPlayState::Paused:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Paused { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeAnimationTimeline(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const Animation::Timeline& timeline, const Animation*, const AnimationList*)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.

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
            ExtractorConverter::convert(state, anonymousScrollTimeline.axis)
        );
    };

    auto valueForAnonymousViewTimeline = [&](auto& anonymousViewTimeline) {
        auto insetCSSValue = [&](auto& inset) -> RefPtr<CSSValue> {
            if (!inset)
                return nullptr;
            return CSSPrimitiveValue::create(*inset, state.style);
        };
        return CSSViewValue::create(
            ExtractorConverter::convert(state, anonymousViewTimeline.axis),
            insetCSSValue(anonymousViewTimeline.insets.start),
            insetCSSValue(anonymousViewTimeline.insets.end)
        );
    };

    WTF::switchOn(timeline,
        [&](Animation::TimelineKeyword keyword) {
            builder.append(nameLiteralForSerialization(keyword == Animation::TimelineKeyword::None ? CSSValueNone : CSSValueAuto));
        },
        [&](const AtomString& customIdent) {
            serializationForCSS(builder, context, state.style, CustomIdentifier { customIdent });
        },
        [&](const Animation::AnonymousScrollTimeline& anonymousScrollTimeline) {
            builder.append(valueForAnonymousScrollTimeline(anonymousScrollTimeline)->cssText(context));
        },
        [&](const Animation::AnonymousViewTimeline& anonymousViewTimeline) {
            builder.append(valueForAnonymousViewTimeline(anonymousViewTimeline)->cssText(context));
        }
    );
}

inline void ExtractorSerializer::serializeAnimationTimingFunction(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TimingFunction& timingFunction, const Animation*, const AnimationList*)
{
    // FIXME: Optimize by avoiding CSSEasingFunction conversion.
    CSS::serializationForCSS(builder, context, toCSSEasingFunction(timingFunction, state.style));
}

inline void ExtractorSerializer::serializeAnimationTimingFunction(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TimingFunction* timingFunction, const Animation*, const AnimationList*)
{
    // FIXME: Optimize by avoiding CSSEasingFunction conversion.
    CSS::serializationForCSS(builder, context, toCSSEasingFunction(*timingFunction, state.style));
}

inline void ExtractorSerializer::serializeAnimationSingleRange(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const SingleTimelineRange& range, SingleTimelineRange::Type type)
{
    bool listEmpty = true;

    if (range.name != SingleTimelineRange::Name::Omitted) {
        builder.append(nameLiteralForSerialization(SingleTimelineRange::valueID(range.name)));
        listEmpty = false;
    }
    if (!SingleTimelineRange::isDefault(range.offset, type)) {
        if (!listEmpty)
            builder.append(' ');
        serializeLength(state, builder, context, range.offset);
    }
}

inline void ExtractorSerializer::serializeAnimationRangeStart(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const SingleTimelineRange& range, const Animation*, const AnimationList*)
{
    serializeAnimationSingleRange(state, builder, context, range, SingleTimelineRange::Type::Start);
}

inline void ExtractorSerializer::serializeAnimationRangeEnd(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const SingleTimelineRange& range, const Animation*, const AnimationList*)
{
    serializeAnimationSingleRange(state, builder, context, range, SingleTimelineRange::Type::End);
}

inline void ExtractorSerializer::serializeAnimationRange(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TimelineRange& range, const Animation*, const AnimationList*)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.

    CSSValueListBuilder list;
    auto rangeStart = range.start;
    auto rangeEnd = range.end;

    Ref startValue = ExtractorConverter::convertAnimationSingleRange(state, rangeStart, SingleTimelineRange::Type::Start);
    Ref endValue = ExtractorConverter::convertAnimationSingleRange(state, rangeEnd, SingleTimelineRange::Type::End);
    bool endValueEqualsStart = startValue->equals(endValue);

    if (startValue->length())
        list.append(WTFMove(startValue));

    bool isNormal = rangeEnd.name == SingleTimelineRange::Name::Normal;
    bool isDefaultAndSameNameAsStart = rangeStart.name == rangeEnd.name && SingleTimelineRange::isDefault(rangeEnd.offset, SingleTimelineRange::Type::End);
    if (endValue->length() && !endValueEqualsStart && !isNormal && !isDefaultAndSameNameAsStart)
        list.append(WTFMove(endValue));

    builder.append(CSSValueList::createSpaceSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeSingleAnimation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const Animation& animation)
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

    bool listEmpty = true;

    if (showsDuration) {
        serializeAnimationDuration(state, builder, context, animation.duration(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsTimingFunction()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationTimingFunction(state, builder, context, *animation.timingFunction(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsDelay) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationDelay(state, builder, context, animation.delay(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsIterationCount()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationIterationCount(state, builder, context, animation.iterationCount(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsDirection()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationDirection(state, builder, context, animation.direction(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsFillMode()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationFillMode(state, builder, context, animation.fillMode(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsPlaysState()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationPlayState(state, builder, context, animation.playState(), nullptr, nullptr);
        listEmpty = false;
    }
    if (animation.name() != Animation::initialName()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationName(state, builder, context, animation.name(), nullptr, nullptr);
        listEmpty = false;
    }
    if (animation.timeline() != Animation::initialTimeline()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationTimeline(state, builder, context, animation.timeline(), nullptr, nullptr);
        listEmpty = false;
    }
    if (animation.compositeOperation() != Animation::initialCompositeOperation()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationCompositeOperation(state, builder, context, animation.compositeOperation(), nullptr, nullptr);
        listEmpty = false;
    }
    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
}

inline void ExtractorSerializer::serializeSingleTransition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const Animation& transition)
{
    static NeverDestroyed<Ref<TimingFunction>> initialTimingFunction(Animation::initialTimingFunction());

    // If we have a transition-delay but no transition-duration set, we must serialize
    // the transition-duration because they're both <time> values and transition-delay
    // comes first.
    auto showsDelay = transition.delay() != Animation::initialDelay();
    auto showsDuration = showsDelay || transition.duration() != Animation::initialDuration();

    bool listEmpty = true;

    if (transition.property() != Animation::initialProperty()) {
        serializeAnimationProperty(state, builder, context, transition.property(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsDuration) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationDuration(state, builder, context, transition.duration(), nullptr, nullptr);
        listEmpty = false;
    }
    if (RefPtr timingFunction = transition.timingFunction(); *timingFunction != initialTimingFunction.get()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationTimingFunction(state, builder, context, *timingFunction, nullptr, nullptr);
        listEmpty = false;
    }
    if (showsDelay) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationDelay(state, builder, context, transition.delay(), nullptr, nullptr);
        listEmpty = false;
    }
    if (transition.allowsDiscreteTransitions() != Animation::initialAllowsDiscreteTransitions()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationAllowsDiscreteTransitions(state, builder, context, transition.allowsDiscreteTransitions(), nullptr, nullptr);
        listEmpty = false;
    }

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::All { });
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
