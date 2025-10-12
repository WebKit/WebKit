/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2023 ChangSeok Oh <changseok@webkit.org>
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include "AnchorPositionEvaluator.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSDynamicRangeLimitValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPathValue.h"
#include "CSSPositionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSSubgridValue.h"
#include "CSSURLValue.h"
#include "CSSValuePair.h"
#include "CalculationValue.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "FontSelectionValueInlines.h"
#include "FontSizeAdjust.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "RenderStyleInlines.h"
#include "RotateTransformOperation.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "ScaleTransformOperation.h"
#include "ScopedName.h"
#include "ScrollAxis.h"
#include "Settings.h"
#include "StyleBasicShape.h"
#include "StyleBuilderChecking.h"
#include "StyleClipPath.h"
#include "StyleColorScheme.h"
#include "StyleCornerShapeValue.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleEasingFunction.h"
#include "StyleFlexBasis.h"
#include "StyleInset.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StyleLineBoxContain.h"
#include "StyleMargin.h"
#include "StyleMaximumSize.h"
#include "StyleMinimumSize.h"
#include "StyleOffsetAnchor.h"
#include "StyleOffsetDistance.h"
#include "StyleOffsetPath.h"
#include "StyleOffsetPosition.h"
#include "StyleOffsetRotate.h"
#include "StylePadding.h"
#include "StylePerspective.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleRayFunction.h"
#include "StyleResolveForFont.h"
#include "StyleRotate.h"
#include "StyleSVGPaint.h"
#include "StyleScale.h"
#include "StyleScrollMargin.h"
#include "StyleScrollPadding.h"
#include "StyleTextEdge+CSSValueConversion.h"
#include "StyleTranslate.h"
#include "StyleURL.h"
#include "StyleValueTypes+CSSValueConversion.h"
#include "TextSpacing.h"
#include "TouchAction.h"
#include "ViewTimeline.h"
#include "WillChangeData.h"
#include <ranges>
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace Style {

// FIXME: Some of those functions assume the CSS parser only allows valid CSSValue types.
// This might not be true if we pass the CSSValue from js via CSS Typed OM.

class BuilderConverter {
public:
    template<typename T, typename... Rest> static T convertStyleType(BuilderState&, const CSSValue&, Rest&&...);

    static OptionSet<TextTransform> convertTextTransform(BuilderState&, const CSSValue&);
    static ImageOrientation convertImageOrientation(BuilderState&, const CSSValue&);
    template<CSSValueID> static AtomString convertCustomIdentAtomOrKeyword(BuilderState&, const CSSValue&);

    static OptionSet<TextEmphasisPosition> convertTextEmphasisPosition(BuilderState&, const CSSValue&);
    static TextAlignMode convertTextAlign(BuilderState&, const CSSValue&);
    static TextAlignLast convertTextAlignLast(BuilderState&, const CSSValue&);
    static Resize convertResize(BuilderState&, const CSSValue&);
    static OptionSet<TextUnderlinePosition> convertTextUnderlinePosition(BuilderState&, const CSSValue&);
    static OptionSet<LineBoxContain> convertLineBoxContain(BuilderState&, const CSSValue&);
    // scrollbar-width converter is only needed for quirking.
    static ScrollbarWidth convertScrollbarWidth(BuilderState&, const CSSValue&);
    static GridAutoFlow convertGridAutoFlow(BuilderState&, const CSSValue&);
    static OptionSet<TouchAction> convertTouchAction(BuilderState&, const CSSValue&);

    static PaintOrder convertPaintOrder(BuilderState&, const CSSValue&);
    static StyleSelfAlignmentData convertSelfOrDefaultAlignmentData(BuilderState&, const CSSValue&);
    static StyleContentAlignmentData convertContentAlignmentData(BuilderState&, const CSSValue&);

    static OptionSet<HangingPunctuation> convertHangingPunctuation(BuilderState&, const CSSValue&);

    static OptionSet<SpeakAs> convertSpeakAs(BuilderState&, const CSSValue&);

    static OptionSet<Containment> convertContain(BuilderState&, const CSSValue&);

    static OptionSet<MarginTrimType> convertMarginTrim(BuilderState&, const CSSValue&);

    static TextSpacingTrim convertTextSpacingTrim(BuilderState&, const CSSValue&);
    static TextAutospace convertTextAutospace(BuilderState&, const CSSValue&);

    static RefPtr<WillChangeData> convertWillChange(BuilderState&, const CSSValue&);

    static std::optional<ScopedName> convertPositionAnchor(BuilderState&, const CSSValue&);
    static std::optional<PositionArea> convertPositionArea(BuilderState&, const CSSValue&);
    static OptionSet<PositionVisibility> convertPositionVisibility(BuilderState&, const CSSValue&);

    static NameScope convertNameScope(BuilderState&, const CSSValue&);

    static FixedVector<PositionTryFallback> convertPositionTryFallbacks(BuilderState&, const CSSValue&);

    static MaskMode convertFillLayerMaskMode(BuilderState&, const CSSValue&);
};

template<typename T, typename... Rest> inline T BuilderConverter::convertStyleType(BuilderState& builderState, const CSSValue& value, Rest&&... rest)
{
    return toStyleFromCSSValue<T>(builderState, value, std::forward<Rest>(rest)...);
}

inline OptionSet<TextTransform> BuilderConverter::convertTextTransform(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialTextTransform();
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            result.add(fromCSSValue<TextTransform>(currentValue));
    }
    return result;
}

inline ImageOrientation BuilderConverter::convertImageOrientation(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    if (primitiveValue->valueID() == CSSValueFromImage)
        return ImageOrientation::Orientation::FromImage;
    return ImageOrientation::Orientation::None;
}

template<CSSValueID keyword> inline AtomString BuilderConverter::convertCustomIdentAtomOrKeyword(BuilderState& builderState, const CSSValue& value)
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return nullAtom();

    if (primitiveValue->valueID() == keyword)
        return nullAtom();
    return AtomString { primitiveValue->stringValue() };
}

inline static OptionSet<TextEmphasisPosition> valueToEmphasisPosition(const CSSPrimitiveValue& primitiveValue)
{
    ASSERT(primitiveValue.isValueID());

    switch (primitiveValue.valueID()) {
    case CSSValueOver:
        return TextEmphasisPosition::Over;
    case CSSValueUnder:
        return TextEmphasisPosition::Under;
    case CSSValueLeft:
        return TextEmphasisPosition::Left;
    case CSSValueRight:
        return TextEmphasisPosition::Right;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return RenderStyle::initialTextEmphasisPosition();
}

inline OptionSet<TextEmphasisPosition> BuilderConverter::convertTextEmphasisPosition(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return valueToEmphasisPosition(*primitiveValue);

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    OptionSet<TextEmphasisPosition> position;
    for (auto& currentValue : *list)
        position.add(valueToEmphasisPosition(currentValue));
    return position;
}

inline TextAlignMode BuilderConverter::convertTextAlign(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    ASSERT(primitiveValue->isValueID());

    const auto& parentStyle = builderState.parentStyle();

    // User agents are expected to have a rule in their user agent stylesheet that matches th elements that have a parent
    // node whose computed value for the 'text-align' property is its initial value, whose declaration block consists of
    // just a single declaration that sets the 'text-align' property to the value 'center'.
    // https://html.spec.whatwg.org/multipage/rendering.html#rendering
    if (primitiveValue->valueID() == CSSValueInternalThCenter) {
        if (parentStyle.textAlign() == RenderStyle::initialTextAlign())
            return TextAlignMode::Center;
        return parentStyle.textAlign();
    }

    if (primitiveValue->valueID() == CSSValueWebkitMatchParent || primitiveValue->valueID() == CSSValueMatchParent) {
        const auto* element = builderState.element();

        if (element && element == builderState.document().documentElement())
            return TextAlignMode::Start;
        if (parentStyle.textAlign() == TextAlignMode::Start)
            return parentStyle.writingMode().isBidiLTR() ? TextAlignMode::Left : TextAlignMode::Right;
        if (parentStyle.textAlign() == TextAlignMode::End)
            return parentStyle.writingMode().isBidiLTR() ? TextAlignMode::Right : TextAlignMode::Left;

        return parentStyle.textAlign();
    }

    return fromCSSValue<TextAlignMode>(value);
}

inline TextAlignLast BuilderConverter::convertTextAlignLast(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    ASSERT(primitiveValue->isValueID());

    if (primitiveValue->valueID() != CSSValueMatchParent)
        return fromCSSValue<TextAlignLast>(value);

    auto& parentStyle = builderState.parentStyle();
    if (parentStyle.textAlignLast() == TextAlignLast::Start)
        return parentStyle.writingMode().isBidiLTR() ? TextAlignLast::Left : TextAlignLast::Right;
    if (parentStyle.textAlignLast() == TextAlignLast::End)
        return parentStyle.writingMode().isBidiLTR() ? TextAlignLast::Right : TextAlignLast::Left;
    return parentStyle.textAlignLast();
}

inline Resize BuilderConverter::convertResize(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    auto resize = Resize::None;
    if (primitiveValue->valueID() == CSSValueInternalTextareaAuto)
        resize = builderState.document().settings().textAreasAreResizable() ? Resize::Both : Resize::None;
    else
        resize = fromCSSValue<Resize>(value);

    return resize;
}

inline static OptionSet<TextUnderlinePosition> valueToUnderlinePosition(const CSSPrimitiveValue& primitiveValue)
{
    ASSERT(primitiveValue.isValueID());

    switch (primitiveValue.valueID()) {
    case CSSValueFromFont:
        return TextUnderlinePosition::FromFont;
    case CSSValueUnder:
        return TextUnderlinePosition::Under;
    case CSSValueLeft:
        return TextUnderlinePosition::Left;
    case CSSValueRight:
        return TextUnderlinePosition::Right;
    case CSSValueAuto:
        return RenderStyle::initialTextUnderlinePosition();
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return RenderStyle::initialTextUnderlinePosition();
}

inline OptionSet<TextUnderlinePosition> BuilderConverter::convertTextUnderlinePosition(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return valueToUnderlinePosition(*primitiveValue);

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(builderState, value);
    if (!pair)
        return { };

    auto position = valueToUnderlinePosition(pair->first);
    position.add(valueToUnderlinePosition(pair->second));
    return position;
}

inline OptionSet<LineBoxContain> BuilderConverter::convertLineBoxContain(BuilderState& builderState, const CSSValue& value)
{
    if (RefPtr primitive = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitive->valueID()) {
        case CSSValueNone:
            return { };
        case CSSValueBlock:
            return LineBoxContain::Block;
        case CSSValueInline:
            return LineBoxContain::Inline;
        case CSSValueFont:
            return LineBoxContain::Font;
        case CSSValueGlyphs:
            return LineBoxContain::Glyphs;
        case CSSValueReplaced:
            return LineBoxContain::Replaced;
        case CSSValueInlineBox:
            return LineBoxContain::InlineBox;
        case CSSValueInitialLetter:
            return LineBoxContain::InitialLetter;
        default:
            builderState.setCurrentPropertyInvalidAtComputedValueTime();
            return { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    OptionSet<LineBoxContain> result;
    for (Ref primitive : *list) {
        switch (primitive->valueID()) {
        case CSSValueBlock:
            result.add(LineBoxContain::Block);
            break;
        case CSSValueInline:
            result.add(LineBoxContain::Inline);
            break;
        case CSSValueFont:
            result.add(LineBoxContain::Font);
            break;
        case CSSValueGlyphs:
            result.add(LineBoxContain::Glyphs);
            break;
        case CSSValueReplaced:
            result.add(LineBoxContain::Replaced);
            break;
        case CSSValueInlineBox:
            result.add(LineBoxContain::InlineBox);
            break;
        case CSSValueInitialLetter:
            result.add(LineBoxContain::InitialLetter);
            break;
        default:
            builderState.setCurrentPropertyInvalidAtComputedValueTime();
            return { };
        }
    }
    return result;
}

inline ScrollbarWidth BuilderConverter::convertScrollbarWidth(BuilderState& builderState, const CSSValue& value)
{
    auto scrollbarWidth = fromCSSValue<ScrollbarWidth>(value);
    if (scrollbarWidth == ScrollbarWidth::Thin && builderState.document().quirks().needsScrollbarWidthThinDisabledQuirk())
        return ScrollbarWidth::Auto;

    return scrollbarWidth;
}

inline GridAutoFlow BuilderConverter::convertGridAutoFlow(BuilderState& builderState, const CSSValue& value)
{
    ASSERT(!is<CSSPrimitiveValue>(value) || downcast<CSSPrimitiveValue>(value).isValueID());

    auto* list = dynamicDowncast<CSSValueList>(value);
    if (list && !list->size())
        return RenderStyle::initialGridAutoFlow();

    auto* first = requiredDowncast<CSSPrimitiveValue>(builderState, list ? *(list->item(0)) : value);
    if (!first)
        return { };
    auto* second = dynamicDowncast<CSSPrimitiveValue>(list && list->size() == 2 ? list->item(1) : nullptr);

    GridAutoFlow autoFlow;
    switch (first->valueID()) {
    case CSSValueRow:
        if (second && second->valueID() == CSSValueDense)
            autoFlow = AutoFlowRowDense;
        else
            autoFlow = AutoFlowRow;
        break;
    case CSSValueColumn:
        if (second && second->valueID() == CSSValueDense)
            autoFlow = AutoFlowColumnDense;
        else
            autoFlow = AutoFlowColumn;
        break;
    case CSSValueDense:
        if (second && second->valueID() == CSSValueColumn)
            autoFlow = AutoFlowColumnDense;
        else
            autoFlow = AutoFlowRowDense;
        break;
    default:
        ASSERT_NOT_REACHED();
        autoFlow = RenderStyle::initialGridAutoFlow();
        break;
    }

    return autoFlow;
}

inline float zoomWithTextZoomFactor(BuilderState& builderState)
{
    if (auto* frame = builderState.document().frame()) {
        float textZoomFactor = builderState.style().textZoom() != TextZoom::Reset ? frame->textZoomFactor() : 1.0f;
        return builderState.style().usedZoom() * textZoomFactor;
    }
    return builderState.cssToLengthConversionData().zoom();
}

inline OptionSet<TouchAction> BuilderConverter::convertTouchAction(BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return fromCSSValue<TouchAction>(value);

    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        OptionSet<TouchAction> touchActions;
        for (auto& currentValue : *list) {
            auto valueID = currentValue.valueID();
            if (valueID != CSSValuePanX && valueID != CSSValuePanY && valueID != CSSValuePinchZoom)
                return RenderStyle::initialTouchActions();
            touchActions.add(fromCSSValueID<TouchAction>(valueID));
        }
        return touchActions;
    }

    return RenderStyle::initialTouchActions();
}

inline PaintOrder BuilderConverter::convertPaintOrder(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNormal);
        return PaintOrder::Normal;
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    switch (list->item(0).valueID()) {
    case CSSValueFill:
        return list->size() > 1 ? PaintOrder::FillMarkers : PaintOrder::Fill;
    case CSSValueStroke:
        return list->size() > 1 ? PaintOrder::StrokeMarkers : PaintOrder::Stroke;
    case CSSValueMarkers:
        return list->size() > 1 ? PaintOrder::MarkersStroke : PaintOrder::Markers;
    default:
        ASSERT_NOT_REACHED();
        return PaintOrder::Normal;
    }
}

// Get the "opposite" ItemPosition to the provided ItemPosition.
// e.g: start -> end, end -> start, self-start -> self-end.
// Position that doesn't have an opposite value is returned as-is.
inline ItemPosition oppositeItemPosition(ItemPosition position)
{
    switch (position) {
    case ItemPosition::Legacy:
    case ItemPosition::Auto:
    case ItemPosition::Normal:
    case ItemPosition::Stretch:
    case ItemPosition::Baseline:
    case ItemPosition::LastBaseline:
    case ItemPosition::Center:
    case ItemPosition::AnchorCenter:
    case ItemPosition::Dialog:
        return position;

    case ItemPosition::Start:
        return ItemPosition::End;
    case ItemPosition::End:
        return ItemPosition::Start;

    case ItemPosition::SelfStart:
        return ItemPosition::SelfEnd;
    case ItemPosition::SelfEnd:
        return ItemPosition::SelfStart;

    case ItemPosition::FlexStart:
        return ItemPosition::FlexEnd;
    case ItemPosition::FlexEnd:
        return ItemPosition::FlexStart;

    case ItemPosition::Left:
        return ItemPosition::Right;
    case ItemPosition::Right:
        return ItemPosition::Left;
    }

    ASSERT_NOT_REACHED();
    return position;
}

inline StyleSelfAlignmentData BuilderConverter::convertSelfOrDefaultAlignmentData(BuilderState& builderState, const CSSValue& value)
{
    auto alignmentData = RenderStyle::initialSelfAlignment();

    if (value.isPair()) {
        if (value.first().valueID() == CSSValueLegacy) {
            alignmentData.setPositionType(ItemPositionType::Legacy);
            alignmentData.setPosition(fromCSSValue<ItemPosition>(value.second()));
        } else if (value.first().valueID() == CSSValueFirst)
            alignmentData.setPosition(ItemPosition::Baseline);
        else if (value.first().valueID() == CSSValueLast)
            alignmentData.setPosition(ItemPosition::LastBaseline);
        else {
            alignmentData.setOverflow(fromCSSValue<OverflowAlignment>(value.first()));
            alignmentData.setPosition(fromCSSValue<ItemPosition>(value.second()));
        }
    } else
        alignmentData.setPosition(fromCSSValue<ItemPosition>(value));

    // Flip the position according to position-try fallback, if specified.
    if (auto positionTryFallback = builderState.positionTryFallback()) {
        for (auto tactic : positionTryFallback->tactics) {
            switch (tactic) {
            case PositionTryFallback::Tactic::FlipBlock:
                if (builderState.cssPropertyID() == CSSPropertyAlignSelf)
                    alignmentData.setPosition(oppositeItemPosition(alignmentData.position()));
                break;

            case PositionTryFallback::Tactic::FlipInline:
                if (builderState.cssPropertyID() == CSSPropertyJustifySelf)
                    alignmentData.setPosition(oppositeItemPosition(alignmentData.position()));
                break;

            case PositionTryFallback::Tactic::FlipStart:
                // justify-self additionally takes left/right, align-self doesn't. When
                // applying flip-start, justify-self gets swapped with align-self. So if
                // we're resolving justify-self (which later gets swapped with align-self),
                // and the position is 'left' or 'right', resolve it to self-start/self-end.
                if (builderState.cssPropertyID() == CSSPropertyJustifySelf) {
                    auto writingMode = builderState.style().writingMode();

                    switch (alignmentData.position()) {
                    case ItemPosition::Left:
                        alignmentData.setPosition(writingMode.bidiDirection() == TextDirection::LTR ? ItemPosition::SelfStart : ItemPosition::SelfEnd);
                        break;
                    case ItemPosition::Right:
                        alignmentData.setPosition(writingMode.bidiDirection() == TextDirection::LTR ? ItemPosition::SelfEnd : ItemPosition::SelfStart);
                        break;
                    default:
                        break;
                    }
                }

                break;
            }
        }
    }

    return alignmentData;
}

inline StyleContentAlignmentData BuilderConverter::convertContentAlignmentData(BuilderState&, const CSSValue& value)
{
    StyleContentAlignmentData alignmentData = RenderStyle::initialContentAlignment();
    auto* contentValue = dynamicDowncast<CSSContentDistributionValue>(value);
    if (!contentValue)
        return alignmentData;
    if (contentValue->distribution() != CSSValueInvalid)
        alignmentData.setDistribution(fromCSSValueID<ContentDistribution>(contentValue->distribution()));
    if (contentValue->position() != CSSValueInvalid)
        alignmentData.setPosition(fromCSSValueID<ContentPosition>(contentValue->position()));
    if (contentValue->overflow() != CSSValueInvalid)
        alignmentData.setOverflow(fromCSSValueID<OverflowAlignment>(contentValue->overflow()));
    return alignmentData;
}

inline OptionSet<SpeakAs> BuilderConverter::convertSpeakAs(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialSpeakAs();
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list) {
            if (!isValueID(currentValue, CSSValueNormal))
                result.add(fromCSSValue<SpeakAs>(currentValue));
        }
    }
    return result;
}

inline OptionSet<HangingPunctuation> BuilderConverter::convertHangingPunctuation(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialHangingPunctuation();
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            result.add(fromCSSValue<HangingPunctuation>(currentValue));
    }
    return result;
}

inline OptionSet<MarginTrimType> BuilderConverter::convertMarginTrim(BuilderState&, const CSSValue& value)
{
    // See if value is "block" or "inline" before trying to parse a list
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueBlock)
            return { MarginTrimType::BlockStart, MarginTrimType::BlockEnd };
        if (primitiveValue->valueID() == CSSValueInline)
            return { MarginTrimType::InlineStart, MarginTrimType::InlineEnd };
    }
    auto list = dynamicDowncast<CSSValueList>(value);
    if (!list || !list->size())
        return RenderStyle::initialMarginTrim();
    OptionSet<MarginTrimType> marginTrim;
    for (auto& item : *list) {
        if (item.valueID() == CSSValueBlock)
            marginTrim.add({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd });
        if (item.valueID() == CSSValueInline)
            marginTrim.add({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd });
    }
    if (!marginTrim.isEmpty())
        return marginTrim;
    for (auto& item : *list) {
        if (item.valueID() == CSSValueBlockStart)
            marginTrim.add(MarginTrimType::BlockStart);
        if (item.valueID() == CSSValueBlockEnd)
            marginTrim.add(MarginTrimType::BlockEnd);
        if (item.valueID() == CSSValueInlineStart)
            marginTrim.add(MarginTrimType::InlineStart);
        if (item.valueID() == CSSValueInlineEnd)
            marginTrim.add(MarginTrimType::InlineEnd);
    }
    ASSERT(list->size() <= 4);
    return marginTrim;
}

inline TextSpacingTrim BuilderConverter::convertTextSpacingTrim(BuilderState&, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueSpaceAll:
            return TextSpacingTrim::TrimType::SpaceAll;
        case CSSValueTrimAll:
            return TextSpacingTrim::TrimType::TrimAll;
        case CSSValueAuto:
            return TextSpacingTrim::TrimType::Auto;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return { };
}

inline TextAutospace BuilderConverter::convertTextAutospace(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNoAutospace)
            return { };
        if (primitiveValue->valueID() == CSSValueAuto)
            return { TextAutospace::Type::Auto };
        if (primitiveValue->valueID() == CSSValueNormal)
            return { TextAutospace::Type::Normal };
    }

    TextAutospace::Options options;

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    for (auto& value : *list) {
        switch (value.valueID()) {
        case CSSValueIdeographAlpha:
            options.add(TextAutospace::Type::IdeographAlpha);
            break;
        case CSSValueIdeographNumeric:
            options.add(TextAutospace::Type::IdeographNumeric);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return options;
}

inline OptionSet<Containment> BuilderConverter::convertContain(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        if (value.valueID() == CSSValueNone)
            return RenderStyle::initialContainment();
        if (value.valueID() == CSSValueStrict)
            return RenderStyle::strictContainment();
        return RenderStyle::contentContainment();
    }

    OptionSet<Containment> containment;

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    for (auto& value : *list) {
        switch (value.valueID()) {
        case CSSValueSize:
            containment.add(Containment::Size);
            break;
        case CSSValueInlineSize:
            containment.add(Containment::InlineSize);
            break;
        case CSSValueLayout:
            containment.add(Containment::Layout);
            break;
        case CSSValuePaint:
            containment.add(Containment::Paint);
            break;
        case CSSValueStyle:
            containment.add(Containment::Style);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        };
    }
    return containment;
}

inline RefPtr<WillChangeData> BuilderConverter::convertWillChange(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        return nullptr;

    auto willChange = WillChangeData::create();
    auto processSingleValue = [&](const CSSValue& item) {
        auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(item);
        if (!primitiveValue)
            return;
        switch (primitiveValue->valueID()) {
        case CSSValueScrollPosition:
            willChange->addFeature(WillChangeData::Feature::ScrollPosition);
            break;
        case CSSValueContents:
            willChange->addFeature(WillChangeData::Feature::Contents);
            break;
        default:
            if (primitiveValue->isPropertyID()) {
                if (!isExposed(primitiveValue->propertyID(), &builderState.document().settings()))
                    break;
                willChange->addFeature(WillChangeData::Feature::Property, primitiveValue->propertyID());
            }
            break;
        }
    };
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& item : *list)
            processSingleValue(item);
    } else
        processSingleValue(value);
    return willChange;
}

inline std::optional<ScopedName> BuilderConverter::convertPositionAnchor(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        return { };
    return convertStyleType<ScopedName>(builderState, value);
}

static std::optional<PositionAreaAxis> positionAreaKeywordToAxis(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueLeft:
    case CSSValueSpanLeft:
    case CSSValueRight:
    case CSSValueSpanRight:
        return PositionAreaAxis::Horizontal;

    case CSSValueTop:
    case CSSValueSpanTop:
    case CSSValueBottom:
    case CSSValueSpanBottom:
        return PositionAreaAxis::Vertical;

    case CSSValueXStart:
    case CSSValueSpanXStart:
    case CSSValueSelfXStart:
    case CSSValueSpanSelfXStart:
    case CSSValueXEnd:
    case CSSValueSpanXEnd:
    case CSSValueSelfXEnd:
    case CSSValueSpanSelfXEnd:
        return PositionAreaAxis::X;

    case CSSValueYStart:
    case CSSValueSpanYStart:
    case CSSValueSelfYStart:
    case CSSValueSpanSelfYStart:
    case CSSValueYEnd:
    case CSSValueSpanYEnd:
    case CSSValueSelfYEnd:
    case CSSValueSpanSelfYEnd:
        return PositionAreaAxis::Y;

    case CSSValueBlockStart:
    case CSSValueSpanBlockStart:
    case CSSValueSelfBlockStart:
    case CSSValueSpanSelfBlockStart:
    case CSSValueBlockEnd:
    case CSSValueSpanBlockEnd:
    case CSSValueSelfBlockEnd:
    case CSSValueSpanSelfBlockEnd:
        return PositionAreaAxis::Block;

    case CSSValueInlineStart:
    case CSSValueSpanInlineStart:
    case CSSValueSelfInlineStart:
    case CSSValueSpanSelfInlineStart:
    case CSSValueInlineEnd:
    case CSSValueSpanInlineEnd:
    case CSSValueSelfInlineEnd:
    case CSSValueSpanSelfInlineEnd:
        return PositionAreaAxis::Inline;

    case CSSValueStart:
    case CSSValueSpanStart:
    case CSSValueSelfStart:
    case CSSValueSpanSelfStart:
    case CSSValueEnd:
    case CSSValueSpanEnd:
    case CSSValueSelfEnd:
    case CSSValueSpanSelfEnd:
    case CSSValueCenter:
    case CSSValueSpanAll:
        return { };

    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

static PositionAreaTrack positionAreaKeywordToTrack(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueLeft:
    case CSSValueTop:
    case CSSValueXStart:
    case CSSValueSelfXStart:
    case CSSValueYStart:
    case CSSValueSelfYStart:
    case CSSValueBlockStart:
    case CSSValueSelfBlockStart:
    case CSSValueInlineStart:
    case CSSValueSelfInlineStart:
    case CSSValueStart:
    case CSSValueSelfStart:
        return PositionAreaTrack::Start;

    case CSSValueSpanLeft:
    case CSSValueSpanTop:
    case CSSValueSpanXStart:
    case CSSValueSpanSelfXStart:
    case CSSValueSpanYStart:
    case CSSValueSpanSelfYStart:
    case CSSValueSpanBlockStart:
    case CSSValueSpanSelfBlockStart:
    case CSSValueSpanInlineStart:
    case CSSValueSpanSelfInlineStart:
    case CSSValueSpanStart:
    case CSSValueSpanSelfStart:
        return PositionAreaTrack::SpanStart;

    case CSSValueRight:
    case CSSValueBottom:
    case CSSValueXEnd:
    case CSSValueSelfXEnd:
    case CSSValueYEnd:
    case CSSValueSelfYEnd:
    case CSSValueBlockEnd:
    case CSSValueSelfBlockEnd:
    case CSSValueInlineEnd:
    case CSSValueSelfInlineEnd:
    case CSSValueEnd:
    case CSSValueSelfEnd:
        return PositionAreaTrack::End;

    case CSSValueSpanRight:
    case CSSValueSpanBottom:
    case CSSValueSpanXEnd:
    case CSSValueSpanSelfXEnd:
    case CSSValueSpanYEnd:
    case CSSValueSpanSelfYEnd:
    case CSSValueSpanBlockEnd:
    case CSSValueSpanSelfBlockEnd:
    case CSSValueSpanInlineEnd:
    case CSSValueSpanSelfInlineEnd:
    case CSSValueSpanEnd:
    case CSSValueSpanSelfEnd:
        return PositionAreaTrack::SpanEnd;

    case CSSValueCenter:
        return PositionAreaTrack::Center;
    case CSSValueSpanAll:
        return PositionAreaTrack::SpanAll;

    default:
        ASSERT_NOT_REACHED();
        return PositionAreaTrack::Start;
    }
}

static PositionAreaSelf positionAreaKeywordToSelf(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueLeft:
    case CSSValueSpanLeft:
    case CSSValueRight:
    case CSSValueSpanRight:
    case CSSValueTop:
    case CSSValueSpanTop:
    case CSSValueBottom:
    case CSSValueSpanBottom:
    case CSSValueXStart:
    case CSSValueSpanXStart:
    case CSSValueXEnd:
    case CSSValueSpanXEnd:
    case CSSValueYStart:
    case CSSValueSpanYStart:
    case CSSValueYEnd:
    case CSSValueSpanYEnd:
    case CSSValueBlockStart:
    case CSSValueSpanBlockStart:
    case CSSValueBlockEnd:
    case CSSValueSpanBlockEnd:
    case CSSValueInlineStart:
    case CSSValueSpanInlineStart:
    case CSSValueInlineEnd:
    case CSSValueSpanInlineEnd:
    case CSSValueStart:
    case CSSValueSpanStart:
    case CSSValueEnd:
    case CSSValueSpanEnd:
    case CSSValueCenter:
    case CSSValueSpanAll:
        return PositionAreaSelf::No;

    case CSSValueSelfXStart:
    case CSSValueSpanSelfXStart:
    case CSSValueSelfXEnd:
    case CSSValueSpanSelfXEnd:
    case CSSValueSelfYStart:
    case CSSValueSpanSelfYStart:
    case CSSValueSelfYEnd:
    case CSSValueSpanSelfYEnd:
    case CSSValueSelfBlockStart:
    case CSSValueSpanSelfBlockStart:
    case CSSValueSelfBlockEnd:
    case CSSValueSpanSelfBlockEnd:
    case CSSValueSelfInlineStart:
    case CSSValueSpanSelfInlineStart:
    case CSSValueSelfInlineEnd:
    case CSSValueSpanSelfInlineEnd:
    case CSSValueSelfStart:
    case CSSValueSpanSelfStart:
    case CSSValueSelfEnd:
    case CSSValueSpanSelfEnd:
        return PositionAreaSelf::Yes;

    default:
        ASSERT_NOT_REACHED();
        return PositionAreaSelf::No;
    }
}

// Expand a one keyword position-area to the equivalent keyword pair value.
static std::pair<CSSValueID, CSSValueID> positionAreaExpandKeyword(CSSValueID dim)
{
    auto maybeAxis = positionAreaKeywordToAxis(dim);
    if (maybeAxis) {
        // Keyword is axis unambiguous, second keyword is span-all.

        // Y/inline axis keyword goes after in the pair.
        auto axis = *maybeAxis;
        if (axis == PositionAreaAxis::Vertical || axis == PositionAreaAxis::Y || axis == PositionAreaAxis::Inline)
            return { CSSValueSpanAll, dim };

        return { dim, CSSValueSpanAll };
    }

    // Keyword is axis ambiguous, it's repeated.
    return { dim, dim };
}


// Flip a PositionArea across a logical axis (block or inline), given the current writing mode.
inline PositionArea flipPositionAreaByLogicalAxis(LogicalBoxAxis flipAxis, PositionArea area, WritingMode writingMode)
{
    auto blockOrXSpan = area.blockOrXAxis();
    auto inlineOrYSpan = area.inlineOrYAxis();

    // blockOrXSpan is on the flip axis, so flip its track and keep inlineOrYSpan intact.
    if (mapPositionAreaAxisToLogicalAxis(blockOrXSpan.axis(), writingMode) == flipAxis) {
        return {
            { blockOrXSpan.axis(), flipPositionAreaTrack(blockOrXSpan.track()), blockOrXSpan.self() },
            inlineOrYSpan
        };
    }

    // The two spans are orthogonal in axis, so if blockOrXSpan isn't on the flip axis,
    // inlineOrYSpan must be. In this case, flip the track of inlineOrYSpan, and
    // keep blockOrXSpan intact.
    return {
        blockOrXSpan,
        { inlineOrYSpan.axis(), flipPositionAreaTrack(inlineOrYSpan.track()), inlineOrYSpan.self() }
    };
}

// Flip a PositionArea as specified by flip-start tactic.
// Intuitively, this mirrors the PositionArea across a diagonal line drawn from the
// block-start/inline-start corner to the block-end/inline-end corner. This is done
// by flipping the axes of the spans in the PositionArea, while keeping their track
// and self properties intact. Because this turns a block/X span into an inline/Y
// span and vice versa, this function also swaps the order of the spans, so
// that the block/X span goes before the inline/Y span.
inline PositionArea mirrorPositionAreaAcrossDiagonal(PositionArea area)
{
    auto blockOrXSpan = area.blockOrXAxis();
    auto inlineOrYSpan = area.inlineOrYAxis();

    return {
        { oppositePositionAreaAxis(inlineOrYSpan.axis()), inlineOrYSpan.track(), inlineOrYSpan.self() },
        { oppositePositionAreaAxis(blockOrXSpan.axis()), blockOrXSpan.track(), blockOrXSpan.self() }
    };
}

inline std::optional<PositionArea> BuilderConverter::convertPositionArea(BuilderState& builderState, const CSSValue& value)
{
    std::pair<CSSValueID, CSSValueID> dimPair;

    if (value.isValueID()) {
        if (value.valueID() == CSSValueNone)
            return { };

        dimPair = positionAreaExpandKeyword(value.valueID());
    } else if (const auto* pair = dynamicDowncast<CSSValuePair>(value)) {
        const auto& first = pair->first();
        const auto& second = pair->second();
        ASSERT(first.isValueID() && second.isValueID());

        // The parsing logic guarantees the keyword pair is in the correct order
        // (horizontal/x/block axis before vertical/Y/inline axis)

        dimPair = { first.valueID(), second.valueID() };
    } else {
        // value MUST be a single ValueID or a pair of ValueIDs, as returned by the parsing logic.
        ASSERT_NOT_REACHED();
        return { };
    }

    auto dim1Axis = positionAreaKeywordToAxis(dimPair.first);
    auto dim2Axis = positionAreaKeywordToAxis(dimPair.second);

    // If both keyword axes are ambiguous, the first one is block axis and second one
    // is inline axis. If only one keyword axis is ambiguous, its axis is the opposite
    // of the other keyword's axis.
    if (!dim1Axis && !dim2Axis) {
        dim1Axis = PositionAreaAxis::Block;
        dim2Axis = PositionAreaAxis::Inline;
    } else if (!dim1Axis)
        dim1Axis = oppositePositionAreaAxis(*dim2Axis);
    else if (!dim2Axis)
        dim2Axis = oppositePositionAreaAxis(*dim1Axis);

    PositionArea area {
        { *dim1Axis, positionAreaKeywordToTrack(dimPair.first), positionAreaKeywordToSelf(dimPair.first) },
        { *dim2Axis, positionAreaKeywordToTrack(dimPair.second), positionAreaKeywordToSelf(dimPair.second) }
    };

    // Flip according to position-try-fallbacks, if specified.
    if (const auto& positionTryFallback = builderState.positionTryFallback()) {
        for (auto tactic : positionTryFallback->tactics) {
            switch (tactic) {
            case PositionTryFallback::Tactic::FlipBlock:
                area = flipPositionAreaByLogicalAxis(LogicalBoxAxis::Block, area, builderState.style().writingMode());
                break;
            case PositionTryFallback::Tactic::FlipInline:
                area = flipPositionAreaByLogicalAxis(LogicalBoxAxis::Inline, area, builderState.style().writingMode());
                break;
            case PositionTryFallback::Tactic::FlipStart:
                area = mirrorPositionAreaAcrossDiagonal(area);
                break;
            }
        }
    }

    return area;
}

inline OptionSet<PositionVisibility> BuilderConverter::convertPositionVisibility(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAlways)
        return { };

    auto maybeList = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!maybeList)
        return { };
    auto list = *maybeList;

    OptionSet<PositionVisibility> result;
    for (const auto& value : list)
        result.add(fromCSSValue<PositionVisibility>(value));

    return result;
}

inline NameScope BuilderConverter::convertNameScope(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNone:
            return { };
        case CSSValueAll:
            return { NameScope::Type::All, { }, builderState.styleScopeOrdinal() };
        default:
            return { NameScope::Type::Ident, { AtomString { primitiveValue->stringValue() } }, builderState.styleScopeOrdinal() };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    ListHashSet<AtomString> nameHashSet;
    for (auto& name : *list)
        nameHashSet.add(AtomString { name.stringValue() });

    return { NameScope::Type::Ident, WTFMove(nameHashSet), builderState.styleScopeOrdinal() };
}

inline FixedVector<PositionTryFallback> BuilderConverter::convertPositionTryFallbacks(BuilderState& builderState, const CSSValue& value)
{
    // FIXME: SaferCPP analysis reports that 'builderState' is an unsafe capture, even though this lambda does not escape.
    SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE auto convertFallback = [&](const CSSValue& fallbackValue) -> std::optional<PositionTryFallback> {
        auto* valueList = dynamicDowncast<CSSValueList>(fallbackValue);
        if (!valueList) {
            // Turn the inlined position-area fallback into properties object that can be applied similarly to @position-try declarations.
            auto property = CSSProperty { CSSPropertyPositionArea, Ref { const_cast<CSSValue&>(fallbackValue) } };
            return PositionTryFallback {
                .positionAreaProperties = ImmutableStyleProperties::createDeduplicating(std::span { &property, 1 }, HTMLStandardMode)
            };
        }

        if (valueList->separator() != CSSValueList::SpaceSeparator)
            return { };

        auto fallback = PositionTryFallback { };

        for (auto& item : *valueList) {
            if (item.isCustomIdent())
                fallback.positionTryRuleName = ScopedName { AtomString { item.customIdent() }, builderState.styleScopeOrdinal() };
            else {
                auto tacticValue = fromCSSValueID<PositionTryFallback::Tactic>(item.valueID());
                if (fallback.tactics.contains(tacticValue)) {
                    ASSERT_NOT_REACHED();
                    return { };
                }

                fallback.tactics.append(tacticValue);
            }
        }
        return fallback;
    };

    if (value.valueID() == CSSValueNone)
        return { };

    if (auto fallback = convertFallback(value))
        return { *fallback };

    auto* list = dynamicDowncast<CSSValueList>(value);
    if (!list)
        return { };

    return FixedVector<PositionTryFallback>::map(*list, [&](auto& item) {
        auto fallback = convertFallback(item);
        return fallback ? *fallback : PositionTryFallback { };
    });
}

inline MaskMode BuilderConverter::convertFillLayerMaskMode(BuilderState& builderState, const CSSValue& value)
{
    switch (value.valueID()) {
    case CSSValueAlpha:
        return MaskMode::Alpha;
    case CSSValueLuminance:
        return MaskMode::Luminance;
    case CSSValueMatchSource:
        return MaskMode::MatchSource;
    case CSSValueAuto: // -webkit-mask-source-type
        return MaskMode::MatchSource;
    default:
        builderState.setCurrentPropertyInvalidAtComputedValueTime();
        return MaskMode::MatchSource;
    }
}

} // namespace Style
} // namespace WebCore
