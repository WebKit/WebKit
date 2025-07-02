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

#include "StyleExtractorConverter.h"
#include "StyleExtractorSerializer.h"
#include "StyleInterpolation.h"
#include "StyleOrderedNamedLinesCollector.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"

namespace WebCore {
namespace Style {

// Custom handling of computed value extraction.
class ExtractorCustom {
public:
    static Ref<CSSValue> extractAspectRatio(ExtractorState&);
    static Ref<CSSValue> extractDirection(ExtractorState&);
    static Ref<CSSValue> extractWritingMode(ExtractorState&);
    static Ref<CSSValue> extractFloat(ExtractorState&);
    static Ref<CSSValue> extractClip(ExtractorState&);
    static Ref<CSSValue> extractContent(ExtractorState&);
    static Ref<CSSValue> extractCursor(ExtractorState&);
    static Ref<CSSValue> extractBaselineShift(ExtractorState&);
    static Ref<CSSValue> extractVerticalAlign(ExtractorState&);
    static Ref<CSSValue> extractTextEmphasisStyle(ExtractorState&);
    static Ref<CSSValue> extractTextIndent(ExtractorState&);
    static Ref<CSSValue> extractLetterSpacing(ExtractorState&);
    static Ref<CSSValue> extractWordSpacing(ExtractorState&);
    static Ref<CSSValue> extractLineHeight(ExtractorState&);
    static Ref<CSSValue> extractFontFamily(ExtractorState&);
    static Ref<CSSValue> extractFontSize(ExtractorState&);
    static Ref<CSSValue> extractFontStyle(ExtractorState&);
    static Ref<CSSValue> extractFontVariantLigatures(ExtractorState&);
    static Ref<CSSValue> extractFontVariantNumeric(ExtractorState&);
    static Ref<CSSValue> extractFontVariantAlternates(ExtractorState&);
    static Ref<CSSValue> extractFontVariantEastAsian(ExtractorState&);
    static Ref<CSSValue> extractTop(ExtractorState&);
    static Ref<CSSValue> extractRight(ExtractorState&);
    static Ref<CSSValue> extractBottom(ExtractorState&);
    static Ref<CSSValue> extractLeft(ExtractorState&);
    static Ref<CSSValue> extractMarginTop(ExtractorState&);
    static Ref<CSSValue> extractMarginRight(ExtractorState&);
    static Ref<CSSValue> extractMarginBottom(ExtractorState&);
    static Ref<CSSValue> extractMarginLeft(ExtractorState&);
    static Ref<CSSValue> extractPaddingTop(ExtractorState&);
    static Ref<CSSValue> extractPaddingRight(ExtractorState&);
    static Ref<CSSValue> extractPaddingBottom(ExtractorState&);
    static Ref<CSSValue> extractPaddingLeft(ExtractorState&);
    static Ref<CSSValue> extractHeight(ExtractorState&);
    static Ref<CSSValue> extractWidth(ExtractorState&);
    static Ref<CSSValue> extractMaxHeight(ExtractorState&);
    static Ref<CSSValue> extractMaxWidth(ExtractorState&);
    static Ref<CSSValue> extractMinHeight(ExtractorState&);
    static Ref<CSSValue> extractMinWidth(ExtractorState&);
    static Ref<CSSValue> extractCounterIncrement(ExtractorState&);
    static Ref<CSSValue> extractCounterReset(ExtractorState&);
    static Ref<CSSValue> extractCounterSet(ExtractorState&);
    static Ref<CSSValue> extractContainIntrinsicHeight(ExtractorState&);
    static Ref<CSSValue> extractContainIntrinsicWidth(ExtractorState&);
    static Ref<CSSValue> extractBorderImageOutset(ExtractorState&);
    static Ref<CSSValue> extractBorderImageRepeat(ExtractorState&);
    static Ref<CSSValue> extractBorderImageSlice(ExtractorState&);
    static RefPtr<CSSValue> extractBorderImageWidth(ExtractorState&);
    static Ref<CSSValue> extractMaskBorderOutset(ExtractorState&);
    static Ref<CSSValue> extractMaskBorderRepeat(ExtractorState&);
    static Ref<CSSValue> extractMaskBorderSlice(ExtractorState&);
    static Ref<CSSValue> extractMaskBorderWidth(ExtractorState&);
    static Ref<CSSValue> extractTransform(ExtractorState&);
    static Ref<CSSValue> extractTranslate(ExtractorState&);
    static Ref<CSSValue> extractScale(ExtractorState&);
    static Ref<CSSValue> extractRotate(ExtractorState&);
    static Ref<CSSValue> extractGridAutoFlow(ExtractorState&);
    static Ref<CSSValue> extractGridTemplateAreas(ExtractorState&);
    static Ref<CSSValue> extractGridTemplateColumns(ExtractorState&);
    static Ref<CSSValue> extractGridTemplateRows(ExtractorState&);

    // MARK: Shorthands

    static RefPtr<CSSValue> extractAnimationShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractAnimationRangeShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractBackgroundShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractBackgroundPositionShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractBlockStepShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractBorderShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractBorderBlockShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractBorderImageShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractBorderInlineShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractBorderRadiusShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractColumnsShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractContainerShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractFlexFlowShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractFontShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractFontSynthesisShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractFontVariantShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractLineClampShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractMaskShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractMaskBorderShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractMaskPositionShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractOffsetShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractOverscrollBehaviorShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractPageBreakAfterShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractPageBreakBeforeShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractPageBreakInsideShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractPerspectiveOriginShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractPositionTryShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractScrollTimelineShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractTextBoxShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractTextDecorationSkipShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractTextWrapShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractTransformOriginShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractTransitionShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractViewTimelineShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractWhiteSpaceShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractWebkitBorderImageShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractWebkitBorderRadiusShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractWebkitColumnBreakAfterShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractWebkitColumnBreakBeforeShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractWebkitColumnBreakInsideShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractWebkitMaskBoxImageShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractWebkitMaskPositionShorthand(ExtractorState&);

    // MARK: Custom Serialization

    static void extractAspectRatioSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractDirectionSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWritingModeSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFloatSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractClipSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractContentSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractCursorSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBaselineShiftSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractVerticalAlignSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTextEmphasisStyleSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTextIndentSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractLetterSpacingSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWordSpacingSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractLineHeightSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontFamilySerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontSizeSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontStyleSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontVariantLigaturesSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontVariantNumericSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontVariantAlternatesSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontVariantEastAsianSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTopSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractRightSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBottomSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractLeftSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMarginTopSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMarginRightSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMarginBottomSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMarginLeftSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractPaddingTopSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractPaddingRightSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractPaddingBottomSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractPaddingLeftSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractHeightSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWidthSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMaxHeightSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMaxWidthSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMinHeightSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMinWidthSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractCounterIncrementSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractCounterResetSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractCounterSetSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractContainIntrinsicHeightSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractContainIntrinsicWidthSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBorderImageOutsetSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBorderImageRepeatSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBorderImageSliceSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBorderImageWidthSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMaskBorderOutsetSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMaskBorderRepeatSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMaskBorderSliceSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMaskBorderWidthSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTransformSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTranslateSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractScaleSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractRotateSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractGridAutoFlowSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractGridTemplateAreasSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractGridTemplateColumnsSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractGridTemplateRowsSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);

    static void extractAnimationShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractAnimationRangeShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBackgroundShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBackgroundPositionShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBlockStepShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBorderShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBorderBlockShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBorderImageShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBorderInlineShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractBorderRadiusShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractColumnsShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractContainerShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFlexFlowShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontSynthesisShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontVariantShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractLineClampShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMaskShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMaskBorderShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractMaskPositionShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractOffsetShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractOverscrollBehaviorShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractPageBreakAfterShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractPageBreakBeforeShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractPageBreakInsideShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractPerspectiveOriginShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractPositionTryShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractScrollTimelineShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTextBoxShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTextDecorationSkipShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTextWrapShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTransformOriginShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTransitionShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractViewTimelineShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWhiteSpaceShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitBorderImageShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitBorderRadiusShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitColumnBreakAfterShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitColumnBreakBeforeShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitColumnBreakInsideShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitMaskBoxImageShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitMaskPositionShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
};

template<CSSPropertyID> struct PropertyExtractorAdaptor;

template<> struct PropertyExtractorAdaptor<CSSPropertyRotate> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        if (is<RenderInline>(state.renderer))
            return functor(CSS::Keyword::None { });
        return functor(state.style.rotate());
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyScale> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        if (is<RenderInline>(state.renderer))
            return functor(CSS::Keyword::None { });
        return functor(state.style.scale());
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyTranslate> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        if (is<RenderInline>(state.renderer))
            return functor(CSS::Keyword::None { });
        return functor(state.style.translate());
    }
};

template<CSSPropertyID propertyID> Ref<CSSValue> extractCSSValue(ExtractorState& state)
{
    return PropertyExtractorAdaptor<propertyID> { }.computedValue(state, [&](auto&& value) {
        return createCSSValue(state.pool, state.style, value);
    });
}

template<CSSPropertyID propertyID> void extractSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    PropertyExtractorAdaptor<propertyID> { }.computedValue(state, [&](auto&& value) {
        serializationForCSS(builder, context, state.style, value);
    });
}

// MARK: - Utilities

template<typename MappingFunctor> Ref<CSSValue> extractFillLayerValue(ExtractorState& state, const FillLayer& layers, MappingFunctor&& mapper)
{
    if (!layers.next())
        return mapper(state, layers);
    CSSValueListBuilder list;
    for (auto* layer = &layers; layer; layer = layer->next())
        list.append(mapper(state, *layer));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

template<typename MappingFunctor> void extractFillLayerValueSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FillLayer& layers, MappingFunctor&& mapper)
{
    bool includeComma = false;

    if (!layers.next()) {
        mapper(state, builder, context, includeComma, layers);
        return;
    }

    for (auto* layer = &layers; layer; layer = layer->next()) {
        mapper(state, builder, context, includeComma, *layer);
        includeComma = true;
    }
}

template<typename MappingFunctor> Ref<CSSValue> extractAnimationOrTransitionValue(ExtractorState& state, const AnimationList* animationList, MappingFunctor&& mapper)
{
    CSSValueListBuilder list;
    if (animationList) {
        for (auto& animation : *animationList) {
            if (auto mappedValue = mapper(state, animation.ptr(), animationList))
                list.append(mappedValue.releaseNonNull());
        }
    } else {
        if (auto mappedValue = mapper(state, nullptr, nullptr))
            list.append(mappedValue.releaseNonNull());
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

template<typename MappingFunctor> void extractAnimationOrTransitionValueSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const AnimationList* animationList, MappingFunctor&& mapper)
{
    bool includeComma = false;
    if (animationList) {
        for (auto& animation : *animationList) {
            auto lengthBefore = builder.length();
            mapper(state, builder, context, includeComma, animation.ptr(), animationList);
            if (builder.length() != lengthBefore)
                includeComma = true;
        }
    } else
        mapper(state, builder, context, includeComma, nullptr, nullptr);
}

template<CSSPropertyID propertyID, typename InsetEdgeApplier, typename NumberAsPixelsApplier, typename ValueIDApplier> decltype(auto) extractZoomAdjustedInset(ExtractorState& state, InsetEdgeApplier&& insetEdgeApplier, NumberAsPixelsApplier&& numberAsPixelsApplier, ValueIDApplier&& valueIDApplier)
{
    auto valueFromStyle = [&](const RenderStyle& style) -> const Style::InsetEdge& {
        // If specified as a length, the corresponding absolute length; if specified as
        // a percentage, the specified value; otherwise, 'auto'. Hence, we can just
        // return the value in the style.
        //
        // See http://www.w3.org/TR/CSS21/cascade.html#computed-value

        if constexpr (propertyID == CSSPropertyTop)
            return style.top();
        else if constexpr (propertyID == CSSPropertyRight)
            return style.right();
        else if constexpr (propertyID == CSSPropertyBottom)
            return style.bottom();
        else if constexpr (propertyID == CSSPropertyLeft)
            return style.left();
    };

    auto& inset = valueFromStyle(state.style);

    // If the element is not displayed; return the "computed value".
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (!box)
        return insetEdgeApplier(inset);

    auto* containingBlock = box->containingBlock();

    // Resolve a "computed value" percentage if the element is positioned.
    if (containingBlock && inset.isPercentOrCalculated() && box->isPositioned()) {
        constexpr bool isVerticalProperty = (propertyID == CSSPropertyTop || propertyID == CSSPropertyBottom);

        LayoutUnit containingBlockSize;
        if (box->isStickilyPositioned()) {
            auto& enclosingClippingBox = box->enclosingClippingBoxForStickyPosition().first;
            if (isVerticalProperty == enclosingClippingBox.isHorizontalWritingMode())
                containingBlockSize = enclosingClippingBox.contentBoxLogicalHeight();
            else
                containingBlockSize = enclosingClippingBox.contentBoxLogicalWidth();
        } else {
            if (isVerticalProperty == containingBlock->isHorizontalWritingMode()) {
                containingBlockSize = box->isOutOfFlowPositioned()
                    ? box->containingBlockLogicalHeightForPositioned(*containingBlock, false)
                    : box->containingBlockLogicalHeightForContent(AvailableLogicalHeightType::ExcludeMarginBorderPadding);
            } else {
                containingBlockSize = box->isOutOfFlowPositioned()
                    ? box->containingBlockLogicalWidthForPositioned(*containingBlock, false)
                    : box->containingBlockLogicalWidthForContent();
            }
        }
        return numberAsPixelsApplier(Style::evaluate(inset, containingBlockSize));
    }

    // Return a "computed value" length.
    if (!inset.isAuto())
        return insetEdgeApplier(inset);

    auto insetUsedStyleRelative = [&](const RenderBox& box) -> LayoutUnit {
        // For relatively positioned boxes, the inset is with respect to the top edges
        // of the box itself. This ties together top/bottom and left/right to be
        // opposites of each other.
        //
        // See http://www.w3.org/TR/CSS2/visuren.html#relative-positioning
        //
        // Specifically;
        //   Since boxes are not split or stretched as a result of 'left' or
        //   'right', the used values are always: left = -right.
        // and
        //   Since boxes are not split or stretched as a result of 'top' or
        //   'bottom', the used values are always: top = -bottom.

        if constexpr (propertyID == CSSPropertyTop)
            return box.relativePositionOffset().height();
        else if constexpr (propertyID == CSSPropertyRight)
            return -(box.relativePositionOffset().width());
        else if constexpr (propertyID == CSSPropertyBottom)
            return -(box.relativePositionOffset().height());
        else if constexpr (propertyID == CSSPropertyLeft)
            return box.relativePositionOffset().width();
    };

    // The property won't be over-constrained if its computed value is "auto", so the "used value" can be returned.
    if (box->isRelativelyPositioned())
        return numberAsPixelsApplier(insetUsedStyleRelative(*box));

    auto insetUsedStyleOutOfFlowPositioned = [&](const RenderBlock& container, const RenderBox& box) {
        // For out-of-flow positioned boxes, the inset is how far an box's margin
        // edge is inset below the edge of the box's containing block.
        // See http://www.w3.org/TR/CSS2/visuren.html#position-props
        //
        // Margins are included in offsetTop/offsetLeft so we need to remove them here.

        if constexpr (propertyID == CSSPropertyTop)
            return box.offsetTop() - box.marginTop();
        else if constexpr (propertyID == CSSPropertyRight)
            return container.clientWidth() - (box.offsetLeft() + box.offsetWidth()) - box.marginRight();
        else if constexpr (propertyID == CSSPropertyBottom)
            return container.clientHeight() - (box.offsetTop() + box.offsetHeight()) - box.marginBottom();
        else if constexpr (propertyID == CSSPropertyLeft)
            return box.offsetLeft() - box.marginLeft();
    };

    if (containingBlock && box->isOutOfFlowPositioned())
        return numberAsPixelsApplier(insetUsedStyleOutOfFlowPositioned(*containingBlock, *box));

    return valueIDApplier(CSSValueAuto);
}

template<CSSPropertyID propertyID> Ref<CSSValue> extractZoomAdjustedInsetValue(ExtractorState& state)
{
    return extractZoomAdjustedInset<propertyID>(state,
        [&](const auto& edge)   -> Ref<CSSValue> { return ExtractorConverter::convertStyleType(state, edge); },
        [&](const auto& number) -> Ref<CSSValue> { return ExtractorConverter::convertNumberAsPixels(state, number); },
        [&](const auto& value)  -> Ref<CSSValue> { return CSSPrimitiveValue::create(value); }
    );
}

template<CSSPropertyID propertyID> void extractZoomAdjustedInsetSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedInset<propertyID>(state,
        [&](const auto& edge)   { ExtractorSerializer::serializeStyleType(state, builder, context, edge); },
        [&](const auto& number) { ExtractorSerializer::serializeNumberAsPixels(state, builder, context, number); },
        [&](const auto& value)  { builder.append(nameLiteralForSerialization(value)); }
    );
}

using PhysicalDirection = BoxSide;
using FlowRelativeDirection = LogicalBoxSide;

inline MarginTrimType toMarginTrimType(const RenderBox& renderer, PhysicalDirection direction)
{
    auto formattingContextRootStyle = [](const RenderBox& renderer) -> const RenderStyle& {
        if (auto* ancestorToUse = (renderer.isFlexItem() || renderer.isGridItem()) ? renderer.parent() : renderer.containingBlock())
            return ancestorToUse->style();
        ASSERT_NOT_REACHED();
        return renderer.style();
    };

    switch (mapSidePhysicalToLogical(formattingContextRootStyle(renderer).writingMode(), direction)) {
    case FlowRelativeDirection::BlockStart:
        return MarginTrimType::BlockStart;
    case FlowRelativeDirection::BlockEnd:
        return MarginTrimType::BlockEnd;
    case FlowRelativeDirection::InlineStart:
        return MarginTrimType::InlineStart;
    case FlowRelativeDirection::InlineEnd:
        return MarginTrimType::InlineEnd;
    default:
        ASSERT_NOT_REACHED();
        return MarginTrimType::BlockStart;
    }
}

inline bool rendererCanHaveTrimmedMargin(const RenderBox& renderer, MarginTrimType marginTrimType)
{
    // A renderer will have a specific margin marked as trimmed by setting its rare data bit if:
    // 1.) The layout system the box is in has this logic (setting the rare data bit for this
    // specific margin) implemented
    // 2.) The block container/flexbox/grid has this margin specified in its margin-trim style
    // If marginTrimType is empty we will check if any of the supported margins are in the style
    if (renderer.isFlexItem() || renderer.isGridItem())
        return renderer.parent()->style().marginTrim().contains(marginTrimType);

    // Even though margin-trim is not inherited, it is possible for nested block level boxes
    // to get placed at the block-start of an containing block ancestor which does have margin-trim.
    // In this case it is not enough to simply check the immediate containing block of the child. It is
    // also probably too expensive to perform an arbitrary walk up the tree to check for the existence
    // of an ancestor containing block with the property, so we will just return true and let
    // the rest of the logic in RenderBox::hasTrimmedMargin to determine if the rare data bit
    // were set at some point during layout
    if (renderer.isBlockLevelBox()) {
        auto containingBlock = renderer.containingBlock();
        return containingBlock && containingBlock->isHorizontalWritingMode();
    }
    return false;
}

template<auto styleGetter, auto computedCSSValueGetter> Ref<CSSValue> extractZoomAdjustedMarginValue(ExtractorState& state)
{
    auto* renderBox = dynamicDowncast<RenderBox>(state.renderer);
    if (!renderBox)
        return ExtractorConverter::convertStyleType(state, (state.style.*styleGetter)());
    return ExtractorConverter::convertNumberAsPixels(state, (renderBox->*computedCSSValueGetter)());
}

template<auto styleGetter, auto computedCSSValueGetter> void extractZoomAdjustedMarginSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto* renderBox = dynamicDowncast<RenderBox>(state.renderer);
    if (!renderBox) {
        ExtractorSerializer::serializeStyleType(state, builder, context, (state.style.*styleGetter)());
        return;
    }

    ExtractorSerializer::serializeNumberAsPixels(state, builder, context, (renderBox->*computedCSSValueGetter)());
}

template<auto styleGetter, auto computedCSSValueGetter> Ref<CSSValue> extractZoomAdjustedPaddingValue(ExtractorState& state)
{
    auto& paddingEdge  = (state.style.*styleGetter)();
    auto* renderBox = dynamicDowncast<RenderBox>(state.renderer);
    if (!renderBox || paddingEdge.isFixed())
        return ExtractorConverter::convertStyleType(state, paddingEdge);
    return ExtractorConverter::convertNumberAsPixels(state, (renderBox->*computedCSSValueGetter)());
}

template<auto styleGetter, auto computedCSSValueGetter> void extractZoomAdjustedPaddingSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto paddingEdge = (state.style.*styleGetter)();
    auto* renderBox = dynamicDowncast<RenderBox>(state.renderer);
    if (!renderBox || paddingEdge.isFixed()) {
        ExtractorSerializer::serializeStyleType(state, builder, context, paddingEdge);
        return;
    }
    ExtractorSerializer::serializeNumberAsPixels(state, builder, context, (renderBox->*computedCSSValueGetter)());
}

template<auto styleGetter, auto boxGetter> Ref<CSSValue> extractZoomAdjustedPreferredSizeValue(ExtractorState& state)
{
    auto sizingBox = [](auto& renderer) -> LayoutRect {
        auto* box = dynamicDowncast<RenderBox>(renderer);
        if (!box)
            return LayoutRect();
        return box->style().boxSizing() == BoxSizing::BorderBox ? box->borderBoxRect() : box->computedCSSContentBoxRect();
    };

    auto isNonReplacedInline = [](auto& renderer) {
        return renderer.isInline() && !renderer.isBlockLevelReplacedOrAtomicInline();
    };

    if (state.renderer && !state.renderer->isRenderOrLegacyRenderSVGModelObject()) {
        // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
        // the "height" property does not apply for non-replaced inline elements.
        if (!isNonReplacedInline(*state.renderer))
            return ExtractorConverter::convertNumberAsPixels(state, (sizingBox(*state.renderer).*boxGetter)());
    }
    return ExtractorConverter::convertStyleType(state, (state.style.*styleGetter)());
}

template<auto styleGetter, auto boxGetter> void extractZoomAdjustedPreferredSizeSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto sizingBox = [](auto& renderer) -> LayoutRect {
        auto* box = dynamicDowncast<RenderBox>(renderer);
        if (!box)
            return LayoutRect();
        return box->style().boxSizing() == BoxSizing::BorderBox ? box->borderBoxRect() : box->computedCSSContentBoxRect();
    };

    auto isNonReplacedInline = [](auto& renderer) {
        return renderer.isInline() && !renderer.isBlockLevelReplacedOrAtomicInline();
    };

    if (state.renderer && !state.renderer->isRenderOrLegacyRenderSVGModelObject()) {
        // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
        // the "height" property does not apply for non-replaced inline elements.
        if (!isNonReplacedInline(*state.renderer)) {
            ExtractorSerializer::serializeNumberAsPixels(state, builder, context, (sizingBox(*state.renderer).*boxGetter)());
            return;
        }
    }

    ExtractorSerializer::serializeStyleType(state, builder, context, (state.style.*styleGetter)());
}

template<auto styleGetter> Ref<CSSValue> extractZoomAdjustedMaxSizeValue(ExtractorState& state)
{
    auto unzoomedLength = (state.style.*styleGetter)();
    if (unzoomedLength.isNone())
        return CSSPrimitiveValue::create(CSSValueNone);
    return ExtractorConverter::convertStyleType(state, unzoomedLength);
}

template<auto styleGetter> void extractZoomAdjustedMaxSizeSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto unzoomedLength = (state.style.*styleGetter)();
    if (unzoomedLength.isNone()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    ExtractorSerializer::serializeStyleType(state, builder, context, unzoomedLength);
}

template<auto styleGetter> Ref<CSSValue> extractZoomAdjustedMinSizeValue(ExtractorState& state)
{
    auto isFlexOrGridItem = [](auto renderer) {
        auto* box = dynamicDowncast<RenderBox>(renderer);
        return box && (box->isFlexItem() || box->isGridItem());
    };

    auto unzoomedLength = (state.style.*styleGetter)();
    if (unzoomedLength.isAuto()) {
        if (isFlexOrGridItem(state.renderer))
            return CSSPrimitiveValue::create(CSSValueAuto);
        return ExtractorConverter::convertNumberAsPixels(state, 0);
    }
    return ExtractorConverter::convertStyleType(state, unzoomedLength);
}

template<auto styleGetter> void extractZoomAdjustedMinSizeSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto isFlexOrGridItem = [](auto renderer) {
        auto* box = dynamicDowncast<RenderBox>(renderer);
        return box && (box->isFlexItem() || box->isGridItem());
    };

    auto unzoomedLength = (state.style.*styleGetter)();
    if (unzoomedLength.isAuto()) {
        if (isFlexOrGridItem(state.renderer)) {
            CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
            return;
        }

        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, 0);
        return;
    }

    ExtractorSerializer::serializeStyleType(state, builder, context, unzoomedLength);
}

template<CSSPropertyID propertyID> Ref<CSSValue> extractCounterValue(ExtractorState& state)
{
    auto& map = state.style.counterDirectives().map;
    if (map.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& keyValue : map) {
        auto number = [&]() -> std::optional<int> {
            if constexpr (propertyID == CSSPropertyCounterIncrement)
                return keyValue.value.incrementValue;
            else if constexpr (propertyID == CSSPropertyCounterReset)
                return keyValue.value.resetValue;
            else if constexpr (propertyID == CSSPropertyCounterSet)
                return keyValue.value.setValue;
        }();
        if (number) {
            list.append(CSSPrimitiveValue::createCustomIdent(keyValue.key));
            list.append(CSSPrimitiveValue::createInteger(*number));
        }
    }
    if (!list.isEmpty())
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    return CSSPrimitiveValue::create(CSSValueNone);
}

template<CSSPropertyID propertyID> void extractCounterSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& map = state.style.counterDirectives().map;
    if (map.isEmpty()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    bool listEmpty = true;

    for (auto& keyValue : map) {
        auto number = [&]() -> std::optional<int> {
            if constexpr (propertyID == CSSPropertyCounterIncrement)
                return keyValue.value.incrementValue;
            else if constexpr (propertyID == CSSPropertyCounterReset)
                return keyValue.value.resetValue;
            else if constexpr (propertyID == CSSPropertyCounterSet)
                return keyValue.value.setValue;
        }();
        if (number) {
            if (!listEmpty)
                builder.append(' ');

            CSS::serializationForCSS(builder, context, CustomIdentifier { keyValue.key });
            builder.append(' ');
            CSS::serializationForCSS(builder, context, CSS::IntegerRaw<> { *number });

            listEmpty = false;
        }
    }

    if (listEmpty)
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
}

template<GridTrackSizingDirection direction> Ref<CSSValue> extractGridTemplateValue(ExtractorState& state)
{
    constexpr bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;

    auto addValuesForNamedGridLinesAtIndex = [](auto& list, auto& collector, auto i, auto renderEmpty) {
        if (collector.isEmpty() && !renderEmpty)
            return;

        Vector<String> lineNames;
        collector.collectLineNamesForIndex(lineNames, i);
        if (!lineNames.isEmpty() || renderEmpty)
            list.append(CSSGridLineNamesValue::create(lineNames));
    };

    auto* renderGrid = dynamicDowncast<RenderGrid>(state.renderer);
    bool isSubgrid = isRowAxis ? state.style.gridSubgridColumns() : state.style.gridSubgridRows();
    auto& trackSizes = isRowAxis ? state.style.gridColumnTrackSizes() : state.style.gridRowTrackSizes();
    auto& autoRepeatTrackSizes = isRowAxis ? state.style.gridAutoRepeatColumns() : state.style.gridAutoRepeatRows();

    if ((direction == GridTrackSizingDirection::ForRows && state.style.gridMasonryRows())
        || (direction == GridTrackSizingDirection::ForColumns && state.style.gridMasonryColumns()))
        return CSSPrimitiveValue::create(CSSValueMasonry);

    // Handle the 'none' case.
    bool trackListIsEmpty = trackSizes.isEmpty() && autoRepeatTrackSizes.isEmpty();
    if (renderGrid && trackListIsEmpty) {
        // For grids we should consider every listed track, whether implicitly or explicitly
        // created. Empty grids have a sole grid line per axis.
        auto& positions = isRowAxis ? renderGrid->columnPositions() : renderGrid->rowPositions();
        trackListIsEmpty = positions.size() == 1;
    }

    if (trackListIsEmpty && !isSubgrid)
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;

    // If the element is a grid container, the resolved value is the used value,
    // specifying track sizes in pixels and expanding the repeat() notation.
    // If subgrid was specified, but the element isn't a subgrid (due to not having
    // an appropriate grid parent), then we fall back to using the specified value.
    if (renderGrid && (!isSubgrid || renderGrid->isSubgrid(direction))) {
        if (isSubgrid) {
            list.append(CSSPrimitiveValue::create(CSSValueSubgrid));

            OrderedNamedLinesCollectorInSubgridLayout collector(state, isRowAxis, renderGrid->numTracks(direction));
            for (int i = 0; i < collector.namedGridLineCount(); i++)
                addValuesForNamedGridLinesAtIndex(list, collector, i, true);
            return CSSValueList::createSpaceSeparated(WTFMove(list));
        }
        OrderedNamedLinesCollectorInGridLayout collector(state, isRowAxis, renderGrid->autoRepeatCountForDirection(direction), autoRepeatTrackSizes.size());

        auto tracks = renderGrid->trackSizesForComputedStyle(direction);
        // Named grid line indices are relative to the explicit grid, but we are including all tracks.
        // So we need to subtract the number of leading implicit tracks in order to get the proper line index.
        int offset = -renderGrid->explicitGridStartForDirection(direction);

        int start = 0;
        int end = tracks.size();
        ASSERT(start <= end);
        ASSERT(static_cast<unsigned>(end) <= tracks.size());
        for (int i = start; i < end; ++i) {
            if (i + offset >= 0)
                addValuesForNamedGridLinesAtIndex(list, collector, i + offset, false);
            list.append(ExtractorConverter::convertNumberAsPixels(state, tracks[i]));
        }
        if (end + offset >= 0)
            addValuesForNamedGridLinesAtIndex(list, collector, end + offset, false);
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    }

    // Otherwise, the resolved value is the computed value, preserving repeat().
    auto& computedTracks = (isRowAxis ? state.style.gridColumnList() : state.style.gridRowList()).list;

    auto repeatVisitor = [&](CSSValueListBuilder& list, const RepeatEntry& entry) {
        if (std::holds_alternative<Vector<String>>(entry)) {
            const auto& names = std::get<Vector<String>>(entry);
            if (names.isEmpty() && !isSubgrid)
                return;
            list.append(CSSGridLineNamesValue::create(names));
        } else
            list.append(ExtractorConverter::convertGridTrackSize(state, std::get<GridTrackSize>(entry)));
    };

    for (auto& entry : computedTracks) {
        WTF::switchOn(entry,
            [&](const GridTrackSize& size) {
                list.append(ExtractorConverter::convertGridTrackSize(state, size));
            },
            [&](const Vector<String>& names) {
                // Subgrids don't have track sizes specified, so empty line names sets
                // need to be serialized, as they are meaningful placeholders.
                if (names.isEmpty() && !isSubgrid)
                    return;
                list.append(CSSGridLineNamesValue::create(names));
            },
            [&](const GridTrackEntryRepeat& repeat) {
                CSSValueListBuilder repeatedValues;
                for (auto& entry : repeat.list)
                    repeatVisitor(repeatedValues, entry);
                list.append(CSSGridIntegerRepeatValue::create(CSSPrimitiveValue::createInteger(repeat.repeats), WTFMove(repeatedValues)));
            },
            [&](const GridTrackEntryAutoRepeat& repeat) {
                CSSValueListBuilder repeatedValues;
                for (auto& entry : repeat.list)
                    repeatVisitor(repeatedValues, entry);
                list.append(CSSGridAutoRepeatValue::create(repeat.type == AutoRepeatType::Fill ? CSSValueAutoFill : CSSValueAutoFit, WTFMove(repeatedValues)));
            },
            [&](const GridTrackEntrySubgrid&) {
                list.append(CSSPrimitiveValue::create(CSSValueSubgrid));
            },
            [&](const GridTrackEntryMasonry&) {
                list.append(CSSPrimitiveValue::create(CSSValueMasonry));
            }
        );
    }

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

template<GridTrackSizingDirection direction> void extractGridTemplateSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractGridTemplateValue<direction>(state)->cssText(context));
}

// MARK: Shorthand Utilities

inline Ref<CSSValue> extractSingleShorthand(ExtractorState& state, const StylePropertyShorthand& shorthand)
{
    ASSERT(shorthand.length() == 1);
    return ExtractorGenerated::extractValue(state, *shorthand.begin()).releaseNonNull();
}

inline void extractSingleShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StylePropertyShorthand& shorthand)
{
    ASSERT(shorthand.length() == 1);
    ExtractorGenerated::extractValueSerialization(state, builder, context, *shorthand.begin());
}

inline Ref<CSSValueList> extractStandardSpaceSeparatedShorthand(ExtractorState& state, const StylePropertyShorthand& shorthand)
{
    CSSValueListBuilder list;
    for (auto longhand : shorthand)
        list.append(ExtractorGenerated::extractValue(state, longhand).releaseNonNull());
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline void extractStandardSpaceSeparatedShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StylePropertyShorthand& shorthand)
{
    builder.append(interleave(shorthand, [&](auto& builder, const auto& longhand) {
        ExtractorGenerated::extractValueSerialization(state, builder, context, longhand);
    }, ' '));
}

inline Ref<CSSValue> extractStandardSlashSeparatedShorthand(ExtractorState& state, const StylePropertyShorthand& shorthand)
{
    CSSValueListBuilder builder;
    for (auto longhand : shorthand)
        builder.append(ExtractorGenerated::extractValue(state, longhand).releaseNonNull());
    return CSSValueList::createSlashSeparated(WTFMove(builder));
}

inline void extractStandardSlashSeparatedShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StylePropertyShorthand& shorthand)
{
    builder.append(interleave(shorthand, [&](auto& builder, const auto& longhand) {
        ExtractorGenerated::extractValueSerialization(state, builder, context, longhand);
    }, " / "_s));
}

inline RefPtr<CSSValue> extractCoalescingPairShorthand(ExtractorState& state, const StylePropertyShorthand& shorthand)
{
    // Assume the properties are in the usual order start, end.
    auto longhands = shorthand.properties();
    auto startValue = ExtractorGenerated::extractValue(state, longhands[0]);
    auto endValue = ExtractorGenerated::extractValue(state, longhands[1]);

    // All 2 properties must be specified.
    if (!startValue || !endValue)
        return nullptr;

    return CSSValuePair::create(startValue.releaseNonNull(), endValue.releaseNonNull());
}

inline void extractCoalescingPairShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StylePropertyShorthand& shorthand)
{
    auto longhands = shorthand.properties();

    auto offsetBeforeFirst = builder.length();
    ExtractorGenerated::extractValueSerialization(state, builder, context, longhands[0]);
    auto offsetAfterFirst = builder.length();

    if (offsetBeforeFirst == offsetAfterFirst)
        return;

    builder.append(' ');

    auto offsetBeforeSecond = builder.length();
    ExtractorGenerated::extractValueSerialization(state, builder, context, longhands[1]);
    auto offsetAfterSecond = builder.length();

    if (offsetBeforeSecond == offsetAfterSecond) {
        builder.shrink(offsetBeforeFirst);
        return;
    }

    StringView stringView = builder;
    StringView stringViewFirst = stringView.substring(offsetBeforeFirst, offsetAfterFirst - offsetBeforeFirst);
    StringView stringViewSecond = stringView.substring(offsetBeforeSecond, offsetAfterSecond - offsetBeforeSecond);

    // If the two longhands serialized to the same value, shrink the builder to right after the first longhand.
    if (stringViewFirst == stringViewSecond)
        builder.shrink(offsetAfterFirst);
}

inline RefPtr<CSSValue> extractCoalescingQuadShorthand(ExtractorState& state, const StylePropertyShorthand& shorthand)
{
    // Assume the properties are in the usual order top, right, bottom, left.
    auto longhands = shorthand.properties();
    auto topValue = ExtractorGenerated::extractValue(state, longhands[0]);
    auto rightValue = ExtractorGenerated::extractValue(state, longhands[1]);
    auto bottomValue = ExtractorGenerated::extractValue(state, longhands[2]);
    auto leftValue = ExtractorGenerated::extractValue(state, longhands[3]);

    // All 4 properties must be specified.
    if (!topValue || !rightValue || !bottomValue || !leftValue)
        return nullptr;

    bool showLeft = !compareCSSValuePtr(rightValue, leftValue);
    bool showBottom = !compareCSSValuePtr(topValue, bottomValue) || showLeft;
    bool showRight = !compareCSSValuePtr(topValue, rightValue) || showBottom;

    CSSValueListBuilder list;
    list.append(topValue.releaseNonNull());
    if (showRight)
        list.append(rightValue.releaseNonNull());
    if (showBottom)
        list.append(bottomValue.releaseNonNull());
    if (showLeft)
        list.append(leftValue.releaseNonNull());
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline void extractCoalescingQuadShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StylePropertyShorthand& shorthand)
{
    auto longhands = shorthand.properties();

    // All 4 properties must be specified.

    auto offsetBeforeTop = builder.length();
    ExtractorGenerated::extractValueSerialization(state, builder, context, longhands[0]);
    auto offsetAfterTop = builder.length();
    if (offsetBeforeTop == offsetAfterTop)
        return;

    builder.append(' ');

    auto offsetBeforeRight = builder.length();
    ExtractorGenerated::extractValueSerialization(state, builder, context, longhands[1]);
    auto offsetAfterRight = builder.length();
    if (offsetBeforeRight == offsetAfterRight) {
        builder.shrink(offsetBeforeTop);
        return;
    }

    builder.append(' ');

    auto offsetBeforeBottom = builder.length();
    ExtractorGenerated::extractValueSerialization(state, builder, context, longhands[2]);
    auto offsetAfterBottom = builder.length();
    if (offsetBeforeBottom == offsetAfterBottom) {
        builder.shrink(offsetBeforeTop);
        return;
    }

    builder.append(' ');

    auto offsetBeforeLeft = builder.length();
    ExtractorGenerated::extractValueSerialization(state, builder, context, longhands[3]);
    auto offsetAfterLeft = builder.length();
    if (offsetBeforeLeft == offsetAfterLeft) {
        builder.shrink(offsetBeforeTop);
        return;
    }

    StringView stringView = builder;
    StringView stringViewTop = stringView.substring(offsetBeforeTop, offsetAfterTop - offsetBeforeTop);
    StringView stringViewRight = stringView.substring(offsetBeforeRight, offsetAfterRight - offsetBeforeRight);
    StringView stringViewBottom = stringView.substring(offsetBeforeBottom, offsetAfterBottom - offsetBeforeBottom);
    StringView stringViewLeft = stringView.substring(offsetBeforeLeft, offsetAfterLeft - offsetBeforeLeft);

    // Include everything.
    if (stringViewRight != stringViewLeft)
        return;

    // Shrink to include top, right and bottom.
    if (stringViewBottom != stringViewTop) {
        builder.shrink(offsetAfterBottom);
        return;
    }

    // Shrink to include top and right.
    if (stringViewRight != stringViewTop) {
        builder.shrink(offsetAfterRight);
        return;
    }

    // Shrink to just include top.
    builder.shrink(offsetAfterTop);
}

inline RefPtr<CSSValue> extractBorderShorthand(ExtractorState& state, std::span<const CSSPropertyID> sections)
{
    auto value = ExtractorGenerated::extractValue(state, sections[0]);
    for (auto& section : sections.subspan(1)) {
        if (!compareCSSValuePtr<CSSValue>(value, ExtractorGenerated::extractValue(state, section)))
            return nullptr;
    }
    return value;
}

inline void extractBorderShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, std::span<const CSSPropertyID> sections)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    if (auto value = extractBorderShorthand(state, sections))
        builder.append(value->cssText(context));
}

inline Ref<CSSValue> extractBorderRadiusShorthand(ExtractorState& state, CSSPropertyID propertyID)
{
    auto itemsEqual = [](const CSSValueListBuilder& a, const CSSValueListBuilder& b) -> bool {
        auto size = a.size();
        if (size != b.size())
            return false;
        for (unsigned i = 0; i < size; ++i) {
            if (!a[i]->equals(b[i]))
                return false;
        }
        return true;
    };

    auto extractBorderRadiusCornerValues = [&](auto& state, const auto& radius) {
        auto x = ExtractorConverter::convertStyleType(state, radius.width());
        auto y = radius.width() == radius.height() ? x.copyRef() : ExtractorConverter::convertStyleType(state, radius.height());
        return std::pair<Ref<CSSValue>, Ref<CSSValue>> { WTFMove(x), WTFMove(y) };
    };

    bool showHorizontalBottomLeft = state.style.borderTopRightRadius().width() != state.style.borderBottomLeftRadius().width();
    bool showHorizontalBottomRight = showHorizontalBottomLeft || (state.style.borderBottomRightRadius().width() != state.style.borderTopLeftRadius().width());
    bool showHorizontalTopRight = showHorizontalBottomRight || (state.style.borderTopRightRadius().width() != state.style.borderTopLeftRadius().width());

    bool showVerticalBottomLeft = state.style.borderTopRightRadius().height() != state.style.borderBottomLeftRadius().height();
    bool showVerticalBottomRight = showVerticalBottomLeft || (state.style.borderBottomRightRadius().height() != state.style.borderTopLeftRadius().height());
    bool showVerticalTopRight = showVerticalBottomRight || (state.style.borderTopRightRadius().height() != state.style.borderTopLeftRadius().height());

    auto [topLeftRadiusX, topLeftRadiusY] = extractBorderRadiusCornerValues(state, state.style.borderTopLeftRadius());
    auto [topRightRadiusX, topRightRadiusY] = extractBorderRadiusCornerValues(state, state.style.borderTopRightRadius());
    auto [bottomRightRadiusX, bottomRightRadiusY] = extractBorderRadiusCornerValues(state, state.style.borderBottomRightRadius());
    auto [bottomLeftRadiusX, bottomLeftRadiusY] = extractBorderRadiusCornerValues(state, state.style.borderBottomLeftRadius());

    CSSValueListBuilder horizontalRadii;
    horizontalRadii.append(WTFMove(topLeftRadiusX));
    if (showHorizontalTopRight)
        horizontalRadii.append(WTFMove(topRightRadiusX));
    if (showHorizontalBottomRight)
        horizontalRadii.append(WTFMove(bottomRightRadiusX));
    if (showHorizontalBottomLeft)
        horizontalRadii.append(WTFMove(bottomLeftRadiusX));

    CSSValueListBuilder verticalRadii;
    verticalRadii.append(WTFMove(topLeftRadiusY));
    if (showVerticalTopRight)
        verticalRadii.append(WTFMove(topRightRadiusY));
    if (showVerticalBottomRight)
        verticalRadii.append(WTFMove(bottomRightRadiusY));
    if (showVerticalBottomLeft)
        verticalRadii.append(WTFMove(bottomLeftRadiusY));

    bool includeVertical = false;
    if (!itemsEqual(horizontalRadii, verticalRadii))
        includeVertical = true;
    else if (propertyID == CSSPropertyWebkitBorderRadius && showHorizontalTopRight && !showHorizontalBottomRight)
        horizontalRadii.append(WTFMove(bottomRightRadiusX));

    if (!includeVertical)
        return CSSValueList::createSlashSeparated(CSSValueList::createSpaceSeparated(WTFMove(horizontalRadii)));
    return CSSValueList::createSlashSeparated(CSSValueList::createSpaceSeparated(WTFMove(horizontalRadii)), CSSValueList::createSpaceSeparated(WTFMove(verticalRadii)));
}

inline void extractBorderRadiusShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, CSSPropertyID propertyID)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractBorderRadiusShorthand(state, propertyID)->cssText(context));
}

inline Ref<CSSValue> extractFillLayerPropertyShorthand(ExtractorState& state, CSSPropertyID property, const StylePropertyShorthand& propertiesBeforeSlashSeparator, const StylePropertyShorthand& propertiesAfterSlashSeparator, CSSPropertyID lastLayerProperty)
{
    ASSERT(property == CSSPropertyBackground || property == CSSPropertyMask);

    auto computeRenderStyle = [&](std::unique_ptr<RenderStyle>& ownedStyle) -> const RenderStyle* {
        if (auto renderer = state.element->renderer(); renderer && renderer->isComposited() && Style::Interpolation::isAccelerated(property, state.element->document().settings())) {
            ownedStyle = renderer->animatedStyle();
            if (state.pseudoElementIdentifier) {
                // FIXME: This cached pseudo style will only exist if the animation has been run at least once.
                return ownedStyle->getCachedPseudoStyle(*state.pseudoElementIdentifier);
            }
            return ownedStyle.get();
        }

        return state.element->computedStyle(state.pseudoElementIdentifier);
    };

    auto layerCount = [&] -> size_t {
        // FIXME: Why does this not use state.style?

        std::unique_ptr<RenderStyle> ownedStyle;
        auto style = computeRenderStyle(ownedStyle);
        if (!style)
            return 0;

        auto& layers = property == CSSPropertyMask ? style->maskLayers() : style->backgroundLayers();

        size_t layerCount = 0;
        for (auto* layer = &layers; layer; layer = layer->next())
            layerCount++;
        if (layerCount == 1 && property == CSSPropertyMask && !layers.image())
            return 0;
        return layerCount;
    }();
    if (!layerCount) {
        ASSERT(property == CSSPropertyMask);
        return CSSPrimitiveValue::create(CSSValueNone);
    }

    auto lastValue = lastLayerProperty != CSSPropertyInvalid ? ExtractorGenerated::extractValue(state, lastLayerProperty) : nullptr;
    auto before = extractStandardSpaceSeparatedShorthand(state, propertiesBeforeSlashSeparator);
    auto after = extractStandardSpaceSeparatedShorthand(state, propertiesAfterSlashSeparator);

    // The computed properties are returned as lists of properties, with a list of layers in each.
    // We want to swap that around to have a list of layers, with a list of properties in each.

    CSSValueListBuilder layers;
    for (size_t i = 0; i < layerCount; i++) {
        CSSValueListBuilder beforeList;
        if (i == layerCount - 1 && lastValue)
            beforeList.append(*lastValue);
        for (size_t j = 0; j < propertiesBeforeSlashSeparator.length(); j++) {
            auto& value = *before->item(j);
            beforeList.append(const_cast<CSSValue&>(layerCount == 1 ? value : *downcast<CSSValueList>(value).item(i)));
        }
        CSSValueListBuilder afterList;
        for (size_t j = 0; j < propertiesAfterSlashSeparator.length(); j++) {
            auto& value = *after->item(j);
            afterList.append(const_cast<CSSValue&>(layerCount == 1 ? value : *downcast<CSSValueList>(value).item(i)));
        }
        auto list = CSSValueList::createSlashSeparated(CSSValueList::createSpaceSeparated(WTFMove(beforeList)), CSSValueList::createSpaceSeparated(WTFMove(afterList)));
        if (layerCount == 1)
            return list;
        layers.append(WTFMove(list));
    }
    return CSSValueList::createCommaSeparated(WTFMove(layers));
}

inline void extractFillLayerPropertyShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, CSSPropertyID property, const StylePropertyShorthand& propertiesBeforeSlashSeparator, const StylePropertyShorthand& propertiesAfterSlashSeparator, CSSPropertyID lastLayerProperty)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractFillLayerPropertyShorthand(state, property, propertiesBeforeSlashSeparator, propertiesAfterSlashSeparator, lastLayerProperty)->cssText(context));
}

// MARK: - Custom Extractors

inline Ref<CSSValue> ExtractorCustom::extractAspectRatio(ExtractorState& state)
{
    switch (state.style.aspectRatioType()) {
    case AspectRatioType::Auto:
        return CSSPrimitiveValue::create(CSSValueAuto);
    case AspectRatioType::AutoZero:
    case AspectRatioType::Ratio:
        return CSSRatioValue::create(CSS::Ratio { state.style.aspectRatioWidth(), state.style.aspectRatioHeight() });
    case AspectRatioType::AutoAndRatio:
        return CSSValueList::createSpaceSeparated(
            CSSPrimitiveValue::create(CSSValueAuto),
            CSSRatioValue::create(CSS::Ratio { state.style.aspectRatioWidth(), state.style.aspectRatioHeight() })
        );
    }
    RELEASE_ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueAuto);
}

inline void ExtractorCustom::extractAspectRatioSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    switch (state.style.aspectRatioType()) {
    case AspectRatioType::Auto:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
        return;
    case AspectRatioType::AutoZero:
    case AspectRatioType::Ratio:
        CSS::serializationForCSS(builder, context, CSS::Ratio { state.style.aspectRatioWidth(), state.style.aspectRatioHeight() });
        return;
    case AspectRatioType::AutoAndRatio:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
        builder.append(' ');
        CSS::serializationForCSS(builder, context, CSS::Ratio { state.style.aspectRatioWidth(), state.style.aspectRatioHeight() });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline CSSValueID extractDirectionValueID(ExtractorState& state)
{
    if (state.element.ptr() == state.element->document().documentElement() && !state.style.hasExplicitlySetDirection())
        return toCSSValueID(RenderStyle::initialDirection());
    return toCSSValueID(state.style.writingMode().computedTextDirection());
}

inline Ref<CSSValue> ExtractorCustom::extractDirection(ExtractorState& state)
{
    return CSSPrimitiveValue::create(extractDirectionValueID(state));
}

inline void ExtractorCustom::extractDirectionSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext&)
{
    builder.append(nameLiteralForSerialization(extractDirectionValueID(state)));
}

inline CSSValueID extractWritingModeValueID(ExtractorState& state)
{
    if (state.element.ptr() == state.element->document().documentElement() && !state.style.hasExplicitlySetWritingMode())
        return toCSSValueID(RenderStyle::initialWritingMode());
    return toCSSValueID(state.style.writingMode().computedWritingMode());
}

inline Ref<CSSValue> ExtractorCustom::extractWritingMode(ExtractorState& state)
{
    return CSSPrimitiveValue::create(extractWritingModeValueID(state));
}

inline void ExtractorCustom::extractWritingModeSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext&)
{
    builder.append(nameLiteralForSerialization(extractWritingModeValueID(state)));
}

inline Ref<CSSValue> ExtractorCustom::extractFloat(ExtractorState& state)
{
    if (state.style.hasOutOfFlowPosition())
        return CSSPrimitiveValue::create(CSSValueNone);
    return ExtractorConverter::convert(state, state.style.floating());
}

inline void ExtractorCustom::extractFloatSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (state.style.hasOutOfFlowPosition()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    ExtractorSerializer::serialize(state, builder, context, state.style.floating());
}

inline Ref<CSSValue> ExtractorCustom::extractClip(ExtractorState& state)
{
    if (!state.style.hasClip())
        return CSSPrimitiveValue::create(CSSValueAuto);

    auto& clip = state.style.clip();

    if (clip.allOf([](auto& side) { return side.isAuto(); }))
        return CSSPrimitiveValue::create(CSSValueAuto);

    return CSSRectValue::create({
        ExtractorConverter::convertLengthOrAuto(state, clip.top()),
        ExtractorConverter::convertLengthOrAuto(state, clip.right()),
        ExtractorConverter::convertLengthOrAuto(state, clip.bottom()),
        ExtractorConverter::convertLengthOrAuto(state, clip.left())
    });
}

inline void ExtractorCustom::extractClipSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (!state.style.hasClip()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
        return;
    }

    auto& clip = state.style.clip();

    if (clip.allOf([](auto& side) { return side.isAuto(); })) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
        return;
    }

    builder.append(nameLiteral(CSSValueRect), '(');
    ExtractorSerializer::serializeLengthOrAuto(state, builder, context, clip.top());
    builder.append(", "_s);
    ExtractorSerializer::serializeLengthOrAuto(state, builder, context, clip.right());
    builder.append(", "_s);
    ExtractorSerializer::serializeLengthOrAuto(state, builder, context, clip.bottom());
    builder.append(", "_s);
    ExtractorSerializer::serializeLengthOrAuto(state, builder, context, clip.left());
    builder.append(')');
}

inline Ref<CSSValue> ExtractorCustom::extractContent(ExtractorState& state)
{
    CSSValueListBuilder list;
    for (auto* contentData = state.style.contentData(); contentData; contentData = contentData->next()) {
        if (auto* counterContentData = dynamicDowncast<CounterContentData>(*contentData))
            list.append(CSSCounterValue::create(counterContentData->counter().identifier(), counterContentData->counter().separator(), CSSPrimitiveValue::createCustomIdent(counterContentData->counter().listStyleType().identifier)));
        else if (auto* imageContentData = dynamicDowncast<ImageContentData>(*contentData))
            list.append(imageContentData->image().computedStyleValue(state.style));
        else if (auto* quoteContentData = dynamicDowncast<QuoteContentData>(*contentData))
            list.append(ExtractorConverter::convert(state, quoteContentData->quote()));
        else if (auto* textContentData = dynamicDowncast<TextContentData>(*contentData))
            list.append(CSSPrimitiveValue::create(textContentData->text()));
        else {
            ASSERT_NOT_REACHED();
            continue;
        }
    }
    if (list.isEmpty())
        list.append(CSSPrimitiveValue::create(state.style.hasUsedContentNone() ? CSSValueNone : CSSValueNormal));
    else if (auto& altText = state.style.contentAltText(); !altText.isNull())
        return CSSValuePair::createSlashSeparated(CSSValueList::createSpaceSeparated(WTFMove(list)), CSSPrimitiveValue::create(altText));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractContentSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractContent(state)->cssText(context));
}

inline Ref<CSSValue> ExtractorCustom::extractCursor(ExtractorState& state)
{
    auto value = ExtractorConverter::convert(state, state.style.cursor());
    auto* cursors = state.style.cursors();
    if (!cursors || !cursors->size())
        return value;
    CSSValueListBuilder list;
    for (unsigned i = 0; i < cursors->size(); ++i) {
        if (auto* image = cursors->at(i).image())
            list.append(image->computedStyleValue(state.style));
    }
    list.append(WTFMove(value));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractCursorSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractCursor(state)->cssText(context));
}

inline Ref<CSSValue> ExtractorCustom::extractBaselineShift(ExtractorState& state)
{
    switch (state.style.svgStyle().baselineShift()) {
    case BaselineShift::Baseline:
        return CSSPrimitiveValue::create(CSSValueBaseline);
    case BaselineShift::Super:
        return CSSPrimitiveValue::create(CSSValueSuper);
    case BaselineShift::Sub:
        return CSSPrimitiveValue::create(CSSValueSub);
    case BaselineShift::Length:
        return ExtractorConverter::convertLength(state, state.style.svgStyle().baselineShiftValue());
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorCustom::extractBaselineShiftSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    switch (state.style.svgStyle().baselineShift()) {
    case BaselineShift::Baseline:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Baseline { });
        return;
    case BaselineShift::Super:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Super { });
        return;
    case BaselineShift::Sub:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Sub { });
        return;
    case BaselineShift::Length:
        ExtractorSerializer::serializeLength(state, builder, context, state.style.svgStyle().baselineShiftValue());
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorCustom::extractVerticalAlign(ExtractorState& state)
{
    switch (state.style.verticalAlign()) {
    case VerticalAlign::Baseline:
        return CSSPrimitiveValue::create(CSSValueBaseline);
    case VerticalAlign::Middle:
        return CSSPrimitiveValue::create(CSSValueMiddle);
    case VerticalAlign::Sub:
        return CSSPrimitiveValue::create(CSSValueSub);
    case VerticalAlign::Super:
        return CSSPrimitiveValue::create(CSSValueSuper);
    case VerticalAlign::TextTop:
        return CSSPrimitiveValue::create(CSSValueTextTop);
    case VerticalAlign::TextBottom:
        return CSSPrimitiveValue::create(CSSValueTextBottom);
    case VerticalAlign::Top:
        return CSSPrimitiveValue::create(CSSValueTop);
    case VerticalAlign::Bottom:
        return CSSPrimitiveValue::create(CSSValueBottom);
    case VerticalAlign::BaselineMiddle:
        return CSSPrimitiveValue::create(CSSValueWebkitBaselineMiddle);
    case VerticalAlign::Length:
        return ExtractorConverter::convertLength(state, state.style.verticalAlignLength());
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorCustom::extractVerticalAlignSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    switch (state.style.verticalAlign()) {
    case VerticalAlign::Baseline:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Baseline { });
        return;
    case VerticalAlign::Middle:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Middle { });
        return;
    case VerticalAlign::Sub:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Sub { });
        return;
    case VerticalAlign::Super:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Super { });
        return;
    case VerticalAlign::TextTop:
        CSS::serializationForCSS(builder, context, CSS::Keyword::TextTop { });
        return;
    case VerticalAlign::TextBottom:
        CSS::serializationForCSS(builder, context, CSS::Keyword::TextBottom { });
        return;
    case VerticalAlign::Top:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Top { });
        return;
    case VerticalAlign::Bottom:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Bottom { });
        return;
    case VerticalAlign::BaselineMiddle:
        CSS::serializationForCSS(builder, context, CSS::Keyword::WebkitBaselineMiddle { });
        return;
    case VerticalAlign::Length:
        ExtractorSerializer::serializeLength(state, builder, context, state.style.verticalAlignLength());
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorCustom::extractTextEmphasisStyle(ExtractorState& state)
{
    switch (state.style.textEmphasisMark()) {
    case TextEmphasisMark::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case TextEmphasisMark::Custom:
        return CSSPrimitiveValue::create(state.style.textEmphasisCustomMark());
    case TextEmphasisMark::Auto:
        ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
        [[fallthrough]];
#endif
    case TextEmphasisMark::Dot:
    case TextEmphasisMark::Circle:
    case TextEmphasisMark::DoubleCircle:
    case TextEmphasisMark::Triangle:
    case TextEmphasisMark::Sesame:
        if (state.style.textEmphasisFill() == TextEmphasisFill::Filled)
            return CSSValueList::createSpaceSeparated(ExtractorConverter::convert(state, state.style.textEmphasisMark()));
        return CSSValueList::createSpaceSeparated(
            ExtractorConverter::convert(state, state.style.textEmphasisFill()),
            ExtractorConverter::convert(state, state.style.textEmphasisMark())
        );
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorCustom::extractTextEmphasisStyleSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    switch (state.style.textEmphasisMark()) {
    case TextEmphasisMark::None:
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    case TextEmphasisMark::Custom:
        serializeString(state.style.textEmphasisCustomMark(), builder);
        return;
    case TextEmphasisMark::Auto:
        ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
        [[fallthrough]];
#endif
    case TextEmphasisMark::Dot:
    case TextEmphasisMark::Circle:
    case TextEmphasisMark::DoubleCircle:
    case TextEmphasisMark::Triangle:
    case TextEmphasisMark::Sesame:
        if (state.style.textEmphasisFill() == TextEmphasisFill::Filled) {
            ExtractorSerializer::serialize(state, builder, context, state.style.textEmphasisMark());
            return;
        }

        ExtractorSerializer::serialize(state, builder, context, state.style.textEmphasisFill());
        builder.append(' ');
        ExtractorSerializer::serialize(state, builder, context, state.style.textEmphasisMark());
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Ref<CSSValue> ExtractorCustom::extractTextIndent(ExtractorState& state)
{
    auto textIndent = ExtractorConverter::convertLength(state, state.style.textIndent());
    auto textIndentLine = state.style.textIndentLine();
    auto textIndentType = state.style.textIndentType();
    if (textIndentLine == TextIndentLine::EachLine || textIndentType == TextIndentType::Hanging) {
        CSSValueListBuilder list;
        list.append(WTFMove(textIndent));
        if (textIndentType == TextIndentType::Hanging)
            list.append(CSSPrimitiveValue::create(CSSValueHanging));
        if (textIndentLine == TextIndentLine::EachLine)
            list.append(CSSPrimitiveValue::create(CSSValueEachLine));
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    }
    return textIndent;
}

inline void ExtractorCustom::extractTextIndentSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeLength(state, builder, context, state.style.textIndent());

    if (state.style.textIndentType() == TextIndentType::Hanging) {
        builder.append(' ');
        CSS::serializationForCSS(builder, context, CSS::Keyword::Hanging { });
    }

    if (state.style.textIndentLine() == TextIndentLine::EachLine) {
        builder.append(' ');
        CSS::serializationForCSS(builder, context, CSS::Keyword::EachLine { });
    }
}

inline Ref<CSSValue> ExtractorCustom::extractLetterSpacing(ExtractorState& state)
{
    auto& spacing = state.style.computedLetterSpacing();
    if (spacing.isFixed() && spacing.isZero())
        return CSSPrimitiveValue::create(CSSValueNormal);
    return ExtractorConverter::convertLength(state, spacing);
}

inline void ExtractorCustom::extractLetterSpacingSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& spacing = state.style.computedLetterSpacing();
    if (spacing.isFixed() && spacing.isZero()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::Normal { });
        return;
    }
    ExtractorSerializer::serializeLength(state, builder, context, spacing);
}

inline Ref<CSSValue> ExtractorCustom::extractWordSpacing(ExtractorState& state)
{
    return ExtractorConverter::convertLength(state, state.style.computedWordSpacing());
}

inline void ExtractorCustom::extractWordSpacingSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeLength(state, builder, context, state.style.computedWordSpacing());
}

inline Ref<CSSValue> ExtractorCustom::extractLineHeight(ExtractorState& state)
{
    auto& length = state.style.lineHeight();
    if (length.isNormal())
        return CSSPrimitiveValue::create(CSSValueNormal);
    if (length.isPercent()) {
        // BuilderConverter::convertLineHeight() will convert a percentage value to a fixed value,
        // and a number value to a percentage value. To be able to roundtrip a number value, we thus
        // look for a percent value and convert it back to a number.
        if (state.valueType == ExtractorState::PropertyValueType::Computed)
            return CSSPrimitiveValue::create(length.value() / 100);

        // This is imperfect, because it doesn't include the zoom factor and the real computation
        // for how high to be in pixels does include things like minimum font size and the zoom factor.
        // On the other hand, since font-size doesn't include the zoom factor, we really can't do
        // that here either.
        return ExtractorConverter::convertNumberAsPixels(state, static_cast<double>(length.percent() * state.style.fontDescription().computedSize()) / 100);
    }
    return ExtractorConverter::convertNumberAsPixels(state, floatValueForLength(length, 0));
}

inline void ExtractorCustom::extractLineHeightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& length = state.style.lineHeight();
    if (length.isNormal()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::Normal { });
        return;
    }
    if (length.isPercent()) {
        // BuilderConverter::convertLineHeight() will convert a percentage value to a fixed value,
        // and a number value to a percentage value. To be able to roundtrip a number value, we thus
        // look for a percent value and convert it back to a number.
        if (state.valueType == ExtractorState::PropertyValueType::Computed) {
            ExtractorSerializer::serializeNumber(state, builder, context, length.value() / 100);
            return;
        }

        // This is imperfect, because it doesn't include the zoom factor and the real computation
        // for how high to be in pixels does include things like minimum font size and the zoom factor.
        // On the other hand, since font-size doesn't include the zoom factor, we really can't do
        // that here either.
        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, static_cast<double>(length.percent() * state.style.fontDescription().computedSize()) / 100);
        return;
    }

    ExtractorSerializer::serializeNumberAsPixels(state, builder, context, floatValueForLength(length, 0));
}

inline Ref<CSSValue> ExtractorCustom::extractFontFamily(ExtractorState& state)
{
    if (state.style.fontCascade().familyCount() == 1)
        return ExtractorConverter::convertFontFamily(state, state.style.fontCascade().familyAt(0));

    CSSValueListBuilder list;
    for (unsigned i = 0; i < state.style.fontCascade().familyCount(); ++i)
        list.append(ExtractorConverter::convertFontFamily(state, state.style.fontCascade().familyAt(i)));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractFontFamilySerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    builder.append(interleave(state.style.fontCascade().fontDescription().families(), [&](auto& builder, auto& family) {
        ExtractorSerializer::serializeFontFamily(state, builder, context, family);
    }, ", "_s));
}

inline Ref<CSSValue> ExtractorCustom::extractFontSize(ExtractorState& state)
{
    return ExtractorConverter::convertNumberAsPixels(state, state.style.fontDescription().computedSize());
}

inline void ExtractorCustom::extractFontSizeSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeNumberAsPixels(state, builder, context, state.style.fontDescription().computedSize());
}

inline Ref<CSSValue> ExtractorCustom::extractFontStyle(ExtractorState& state)
{
    auto italic = state.style.fontDescription().italic();
    if (auto keyword = fontStyleKeyword(italic, state.style.fontDescription().fontStyleAxis()))
        return CSSPrimitiveValue::create(*keyword);
    return CSSFontStyleWithAngleValue::create({ CSS::AngleUnit::Deg, static_cast<float>(*italic) });
}

inline void ExtractorCustom::extractFontStyleSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto italic = state.style.fontDescription().italic();
    if (auto keyword = fontStyleKeyword(italic, state.style.fontDescription().fontStyleAxis())) {
        builder.append(nameLiteralForSerialization(*keyword));
        return;
    }

    float angle = *italic;
    if (!angle) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::Normal { });
        return;
    }

    CSS::serializationForCSS(builder, context, CSS::Keyword::Oblique { });
    builder.append(' ');
    CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { CSS::AngleUnit::Deg, angle });
}

inline Ref<CSSValue> ExtractorCustom::extractFontVariantLigatures(ExtractorState& state)
{
    auto common = state.style.fontDescription().variantCommonLigatures();
    auto discretionary = state.style.fontDescription().variantDiscretionaryLigatures();
    auto historical = state.style.fontDescription().variantHistoricalLigatures();
    auto contextualAlternates = state.style.fontDescription().variantContextualAlternates();

    if (common == FontVariantLigatures::No && discretionary == FontVariantLigatures::No && historical == FontVariantLigatures::No && contextualAlternates == FontVariantLigatures::No)
        return CSSPrimitiveValue::create(CSSValueNone);
    if (common == FontVariantLigatures::Normal && discretionary == FontVariantLigatures::Normal && historical == FontVariantLigatures::Normal && contextualAlternates == FontVariantLigatures::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    auto appendLigaturesValue = [](auto& list, auto value, auto yesValue, auto noValue) {
        switch (value) {
        case FontVariantLigatures::Normal:
            return;
        case FontVariantLigatures::No:
            list.append(CSSPrimitiveValue::create(noValue));
            return;
        case FontVariantLigatures::Yes:
            list.append(CSSPrimitiveValue::create(yesValue));
            return;
        }
        ASSERT_NOT_REACHED();
    };

    CSSValueListBuilder valueList;
    appendLigaturesValue(valueList, common, CSSValueCommonLigatures, CSSValueNoCommonLigatures);
    appendLigaturesValue(valueList, discretionary, CSSValueDiscretionaryLigatures, CSSValueNoDiscretionaryLigatures);
    appendLigaturesValue(valueList, historical, CSSValueHistoricalLigatures, CSSValueNoHistoricalLigatures);
    appendLigaturesValue(valueList, contextualAlternates, CSSValueContextual, CSSValueNoContextual);
    return CSSValueList::createSpaceSeparated(WTFMove(valueList));
}

inline void ExtractorCustom::extractFontVariantLigaturesSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractFontVariantLigatures(state)->cssText(context));
}

inline Ref<CSSValue> ExtractorCustom::extractFontVariantNumeric(ExtractorState& state)
{
    auto figure = state.style.fontDescription().variantNumericFigure();
    auto spacing = state.style.fontDescription().variantNumericSpacing();
    auto fraction = state.style.fontDescription().variantNumericFraction();
    auto ordinal = state.style.fontDescription().variantNumericOrdinal();
    auto slashedZero = state.style.fontDescription().variantNumericSlashedZero();

    if (figure == FontVariantNumericFigure::Normal && spacing == FontVariantNumericSpacing::Normal && fraction == FontVariantNumericFraction::Normal && ordinal == FontVariantNumericOrdinal::Normal && slashedZero == FontVariantNumericSlashedZero::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    CSSValueListBuilder valueList;
    switch (figure) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        valueList.append(CSSPrimitiveValue::create(CSSValueLiningNums));
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        valueList.append(CSSPrimitiveValue::create(CSSValueOldstyleNums));
        break;
    }

    switch (spacing) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        valueList.append(CSSPrimitiveValue::create(CSSValueProportionalNums));
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        valueList.append(CSSPrimitiveValue::create(CSSValueTabularNums));
        break;
    }

    switch (fraction) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        valueList.append(CSSPrimitiveValue::create(CSSValueDiagonalFractions));
        break;
    case FontVariantNumericFraction::StackedFractions:
        valueList.append(CSSPrimitiveValue::create(CSSValueStackedFractions));
        break;
    }

    if (ordinal == FontVariantNumericOrdinal::Yes)
        valueList.append(CSSPrimitiveValue::create(CSSValueOrdinal));
    if (slashedZero == FontVariantNumericSlashedZero::Yes)
        valueList.append(CSSPrimitiveValue::create(CSSValueSlashedZero));

    return CSSValueList::createSpaceSeparated(WTFMove(valueList));
}

inline void ExtractorCustom::extractFontVariantNumericSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractFontVariantNumeric(state)->cssText(context));
}

inline Ref<CSSValue> ExtractorCustom::extractFontVariantAlternates(ExtractorState& state)
{
    auto alternates = state.style.fontDescription().variantAlternates();
    if (alternates.isNormal())
        return CSSPrimitiveValue::create(CSSValueNormal);

    CSSValueListBuilder valueList;

    if (!alternates.values().stylistic.isNull())
        valueList.append(CSSFunctionValue::create(CSSValueStylistic, CSSPrimitiveValue::createCustomIdent(alternates.values().stylistic)));

    if (alternates.values().historicalForms)
        valueList.append(CSSPrimitiveValue::create(CSSValueHistoricalForms));

    if (!alternates.values().styleset.isEmpty()) {
        CSSValueListBuilder stylesetArguments;
        for (auto& argument : alternates.values().styleset)
            stylesetArguments.append(CSSPrimitiveValue::createCustomIdent(argument));
        valueList.append(CSSFunctionValue::create(CSSValueStyleset, WTFMove(stylesetArguments)));
    }

    if (!alternates.values().characterVariant.isEmpty()) {
        CSSValueListBuilder characterVariantArguments;
        for (auto& argument : alternates.values().characterVariant)
            characterVariantArguments.append(CSSPrimitiveValue::createCustomIdent(argument));
        valueList.append(CSSFunctionValue::create(CSSValueCharacterVariant, WTFMove(characterVariantArguments)));
    }

    if (!alternates.values().swash.isNull())
        valueList.append(CSSFunctionValue::create(CSSValueSwash, CSSPrimitiveValue::createCustomIdent(alternates.values().swash)));

    if (!alternates.values().ornaments.isNull())
        valueList.append(CSSFunctionValue::create(CSSValueOrnaments, CSSPrimitiveValue::createCustomIdent(alternates.values().ornaments)));

    if (!alternates.values().annotation.isNull())
        valueList.append(CSSFunctionValue::create(CSSValueAnnotation, CSSPrimitiveValue::createCustomIdent(alternates.values().annotation)));

    if (valueList.size() == 1)
        return WTFMove(valueList[0]);

    return CSSValueList::createSpaceSeparated(WTFMove(valueList));
}

inline void ExtractorCustom::extractFontVariantAlternatesSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractFontVariantAlternates(state)->cssText(context));
}

inline Ref<CSSValue> ExtractorCustom::extractFontVariantEastAsian(ExtractorState& state)
{
    auto variant = state.style.fontDescription().variantEastAsianVariant();
    auto width = state.style.fontDescription().variantEastAsianWidth();
    auto ruby = state.style.fontDescription().variantEastAsianRuby();
    if (variant == FontVariantEastAsianVariant::Normal && width == FontVariantEastAsianWidth::Normal && ruby == FontVariantEastAsianRuby::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    CSSValueListBuilder valueList;
    switch (variant) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        valueList.append(CSSPrimitiveValue::create(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        valueList.append(CSSPrimitiveValue::create(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        valueList.append(CSSPrimitiveValue::create(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        valueList.append(CSSPrimitiveValue::create(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        valueList.append(CSSPrimitiveValue::create(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        valueList.append(CSSPrimitiveValue::create(CSSValueTraditional));
        break;
    }

    switch (width) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        valueList.append(CSSPrimitiveValue::create(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        valueList.append(CSSPrimitiveValue::create(CSSValueProportionalWidth));
        break;
    }

    if (ruby == FontVariantEastAsianRuby::Yes)
        valueList.append(CSSPrimitiveValue::create(CSSValueRuby));

    return CSSValueList::createSpaceSeparated(WTFMove(valueList));
}

inline void ExtractorCustom::extractFontVariantEastAsianSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractFontVariantEastAsian(state)->cssText(context));
}

inline Ref<CSSValue> ExtractorCustom::extractTop(ExtractorState& state)
{
    return extractZoomAdjustedInsetValue<CSSPropertyTop>(state);
}

inline void ExtractorCustom::extractTopSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedInsetSerialization<CSSPropertyTop>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractRight(ExtractorState& state)
{
    return extractZoomAdjustedInsetValue<CSSPropertyRight>(state);
}

inline void ExtractorCustom::extractRightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedInsetSerialization<CSSPropertyRight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractBottom(ExtractorState& state)
{
    return extractZoomAdjustedInsetValue<CSSPropertyBottom>(state);
}

inline void ExtractorCustom::extractBottomSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedInsetSerialization<CSSPropertyBottom>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractLeft(ExtractorState& state)
{
    return extractZoomAdjustedInsetValue<CSSPropertyLeft>(state);
}

inline void ExtractorCustom::extractLeftSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedInsetSerialization<CSSPropertyLeft>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMarginTop(ExtractorState& state)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::BlockStart) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Top)))
        return ExtractorConverter::convertNumberAsPixels(state, box->marginTop());
    return extractZoomAdjustedMarginValue<&RenderStyle::marginTop, &RenderBoxModelObject::marginTop>(state);
}

inline void ExtractorCustom::extractMarginTopSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::BlockStart) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Top))) {
        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, box->marginTop());
        return;
    }

    extractZoomAdjustedMarginSerialization<&RenderStyle::marginTop, &RenderBoxModelObject::marginTop>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMarginRight(ExtractorState& state)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::InlineEnd) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Right)))
        return ExtractorConverter::convertNumberAsPixels(state, box->marginRight());

    auto& marginRight = state.style.marginRight();
    if (marginRight.isFixed() || !box)
        return ExtractorConverter::convertStyleType(state, marginRight);

    float value;
    if (marginRight.isPercentOrCalculated()) {
        // RenderBox gives a marginRight() that is the distance between the right-edge of the child box
        // and the right-edge of the containing box, when display == DisplayType::Block. Let's calculate the absolute
        // value of the specified margin-right % instead of relying on RenderBox's marginRight() value.
        value = Style::evaluateMinimum(marginRight, box->containingBlockLogicalWidthForContent());
    } else
        value = box->marginRight();
    return ExtractorConverter::convertNumberAsPixels(state, value);
}

inline void ExtractorCustom::extractMarginRightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::InlineEnd) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Right))) {
        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, box->marginRight());
        return;
    }

    auto& marginRight = state.style.marginRight();
    if (marginRight.isFixed() || !box) {
        ExtractorSerializer::serializeStyleType(state, builder, context, marginRight);
        return;
    }

    float value;
    if (marginRight.isPercentOrCalculated()) {
        // RenderBox gives a marginRight() that is the distance between the right-edge of the child box
        // and the right-edge of the containing box, when display == DisplayType::Block. Let's calculate the absolute
        // value of the specified margin-right % instead of relying on RenderBox's marginRight() value.
        value = Style::evaluateMinimum(marginRight, box->containingBlockLogicalWidthForContent());
    } else
        value = box->marginRight();

    ExtractorSerializer::serializeNumberAsPixels(state, builder, context, value);
}

inline Ref<CSSValue> ExtractorCustom::extractMarginBottom(ExtractorState& state)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::BlockEnd) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Bottom)))
        return ExtractorConverter::convertNumberAsPixels(state, box->marginBottom());
    return extractZoomAdjustedMarginValue<&RenderStyle::marginBottom, &RenderBoxModelObject::marginBottom>(state);
}

inline void ExtractorCustom::extractMarginBottomSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::BlockEnd) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Bottom))) {
        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, box->marginBottom());
        return;
    }

    extractZoomAdjustedMarginSerialization<&RenderStyle::marginBottom, &RenderBoxModelObject::marginBottom>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMarginLeft(ExtractorState& state)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::InlineStart) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Left)))
        return ExtractorConverter::convertNumberAsPixels(state, box->marginLeft());
    return extractZoomAdjustedMarginValue<&RenderStyle::marginLeft, &RenderBoxModelObject::marginLeft>(state);
}

inline void ExtractorCustom::extractMarginLeftSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::InlineStart) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Left))) {
        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, box->marginLeft());
        return;
    }
    extractZoomAdjustedMarginSerialization<&RenderStyle::marginLeft, &RenderBoxModelObject::marginLeft>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractPaddingTop(ExtractorState& state)
{
    return extractZoomAdjustedPaddingValue<&RenderStyle::paddingTop, &RenderBoxModelObject::computedCSSPaddingTop>(state);
}

inline void ExtractorCustom::extractPaddingTopSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedPaddingSerialization<&RenderStyle::paddingTop, &RenderBoxModelObject::computedCSSPaddingTop>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractPaddingRight(ExtractorState& state)
{
    return extractZoomAdjustedPaddingValue<&RenderStyle::paddingRight, &RenderBoxModelObject::computedCSSPaddingRight>(state);
}

inline void ExtractorCustom::extractPaddingRightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedPaddingSerialization<&RenderStyle::paddingRight, &RenderBoxModelObject::computedCSSPaddingRight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractPaddingBottom(ExtractorState& state)
{
    return extractZoomAdjustedPaddingValue<&RenderStyle::paddingBottom, &RenderBoxModelObject::computedCSSPaddingBottom>(state);
}

inline void ExtractorCustom::extractPaddingBottomSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedPaddingSerialization<&RenderStyle::paddingBottom, &RenderBoxModelObject::computedCSSPaddingBottom>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractPaddingLeft(ExtractorState& state)
{
    return extractZoomAdjustedPaddingValue<&RenderStyle::paddingLeft, &RenderBoxModelObject::computedCSSPaddingLeft>(state);
}

inline void ExtractorCustom::extractPaddingLeftSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedPaddingSerialization<&RenderStyle::paddingLeft, &RenderBoxModelObject::computedCSSPaddingLeft>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractHeight(ExtractorState& state)
{
    return extractZoomAdjustedPreferredSizeValue<&RenderStyle::height, &LayoutRect::height>(state);
}

inline void ExtractorCustom::extractHeightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedPreferredSizeSerialization<&RenderStyle::height, &LayoutRect::height>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractWidth(ExtractorState& state)
{
    return extractZoomAdjustedPreferredSizeValue<&RenderStyle::width, &LayoutRect::width>(state);
}

inline void ExtractorCustom::extractWidthSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedPreferredSizeSerialization<&RenderStyle::width, &LayoutRect::width>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMaxHeight(ExtractorState& state)
{
    return extractZoomAdjustedMaxSizeValue<&RenderStyle::maxHeight>(state);
}

inline void ExtractorCustom::extractMaxHeightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedMaxSizeSerialization<&RenderStyle::maxHeight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMaxWidth(ExtractorState& state)
{
    return extractZoomAdjustedMaxSizeValue<&RenderStyle::maxWidth>(state);
}

inline void ExtractorCustom::extractMaxWidthSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedMaxSizeSerialization<&RenderStyle::maxWidth>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMinHeight(ExtractorState& state)
{
    return extractZoomAdjustedMinSizeValue<&RenderStyle::minHeight>(state);
}

inline void ExtractorCustom::extractMinHeightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedMinSizeSerialization<&RenderStyle::minHeight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMinWidth(ExtractorState& state)
{
    return extractZoomAdjustedMinSizeValue<&RenderStyle::minWidth>(state);
}

inline void ExtractorCustom::extractMinWidthSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractZoomAdjustedMinSizeSerialization<&RenderStyle::minWidth>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractCounterIncrement(ExtractorState& state)
{
    return extractCounterValue<CSSPropertyCounterIncrement>(state);
}

inline void ExtractorCustom::extractCounterIncrementSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractCounterSerialization<CSSPropertyCounterIncrement>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractCounterReset(ExtractorState& state)
{
    return extractCounterValue<CSSPropertyCounterReset>(state);
}

inline void ExtractorCustom::extractCounterResetSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractCounterSerialization<CSSPropertyCounterReset>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractCounterSet(ExtractorState& state)
{
    return extractCounterValue<CSSPropertyCounterSet>(state);
}

inline void ExtractorCustom::extractCounterSetSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractCounterSerialization<CSSPropertyCounterSet>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractContainIntrinsicHeight(ExtractorState& state)
{
    return ExtractorConverter::convertContainIntrinsicSize(state, state.style.containIntrinsicHeightType(), state.style.containIntrinsicHeight());
}

inline void ExtractorCustom::extractContainIntrinsicHeightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeContainIntrinsicSize(state, builder, context, state.style.containIntrinsicHeightType(), state.style.containIntrinsicHeight());
}

inline Ref<CSSValue> ExtractorCustom::extractContainIntrinsicWidth(ExtractorState& state)
{
    return ExtractorConverter::convertContainIntrinsicSize(state, state.style.containIntrinsicWidthType(), state.style.containIntrinsicWidth());
}

inline void ExtractorCustom::extractContainIntrinsicWidthSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeContainIntrinsicSize(state, builder, context, state.style.containIntrinsicWidthType(), state.style.containIntrinsicWidth());
}

inline Ref<CSSValue> ExtractorCustom::extractBorderImageOutset(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageQuad(state, state.style.borderImage().outset());
}

inline void ExtractorCustom::extractBorderImageOutsetSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeNinePieceImageQuad(state, builder, context, state.style.borderImage().outset());
}

inline Ref<CSSValue> ExtractorCustom::extractBorderImageRepeat(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageRepeat(state, state.style.borderImage());
}

inline void ExtractorCustom::extractBorderImageRepeatSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeNinePieceImageRepeat(state, builder, context, state.style.borderImage());
}

inline Ref<CSSValue> ExtractorCustom::extractBorderImageSlice(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageSlices(state, state.style.borderImage());
}

inline void ExtractorCustom::extractBorderImageSliceSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeNinePieceImageSlices(state, builder, context, state.style.borderImage());
}

inline RefPtr<CSSValue> ExtractorCustom::extractBorderImageWidth(ExtractorState& state)
{
    auto& borderImage = state.style.borderImage();
    if (borderImage.overridesBorderWidths())
        return nullptr;
    return ExtractorConverter::convertNinePieceImageQuad(state, borderImage.borderSlices());
}

inline void ExtractorCustom::extractBorderImageWidthSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& borderImage = state.style.borderImage();
    if (borderImage.overridesBorderWidths())
        return;
    ExtractorSerializer::serializeNinePieceImageQuad(state, builder, context, borderImage.borderSlices());
}

inline Ref<CSSValue> ExtractorCustom::extractMaskBorderOutset(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageQuad(state, state.style.maskBorder().outset());
}

inline void ExtractorCustom::extractMaskBorderOutsetSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeNinePieceImageQuad(state, builder, context, state.style.maskBorder().outset());
}

inline Ref<CSSValue> ExtractorCustom::extractMaskBorderRepeat(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageRepeat(state, state.style.maskBorder());
}

inline void ExtractorCustom::extractMaskBorderRepeatSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeNinePieceImageRepeat(state, builder, context, state.style.maskBorder());
}

inline Ref<CSSValue> ExtractorCustom::extractMaskBorderSlice(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageSlices(state, state.style.maskBorder());
}

inline void ExtractorCustom::extractMaskBorderSliceSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeNinePieceImageSlices(state, builder, context, state.style.maskBorder());
}

inline Ref<CSSValue> ExtractorCustom::extractMaskBorderWidth(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageQuad(state, state.style.maskBorder().borderSlices());
}

inline void ExtractorCustom::extractMaskBorderWidthSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeNinePieceImageQuad(state, builder, context, state.style.maskBorder().borderSlices());
}

inline Ref<CSSValue> ExtractorCustom::extractTransform(ExtractorState& state)
{
    if (!state.style.hasTransform())
        return CSSPrimitiveValue::create(CSSValueNone);

    if (state.renderer) {
        TransformationMatrix transform;
        state.style.applyTransform(transform, TransformOperationData(state.renderer->transformReferenceBoxRect(state.style), state.renderer), { });
        return CSSTransformListValue::create(ExtractorConverter::convertTransformationMatrix(state, transform));
    }

    // https://w3c.github.io/csswg-drafts/css-transforms-1/#serialization-of-the-computed-value
    // If we don't have a renderer, then the value should be "none" if we're asking for the
    // resolved value (such as when calling getComputedStyle()).
    if (state.valueType == ExtractorState::PropertyValueType::Resolved)
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& operation : state.style.transform())
        list.append(ExtractorConverter::convertTransformOperation(state, operation));
    if (!list.isEmpty())
        return CSSTransformListValue::create(WTFMove(list));

    return CSSPrimitiveValue::create(CSSValueNone);
}

inline void ExtractorCustom::extractTransformSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (!state.style.hasTransform()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    if (state.renderer) {
        TransformationMatrix transform;
        state.style.applyTransform(transform, TransformOperationData(state.renderer->transformReferenceBoxRect(state.style), state.renderer), { });
        ExtractorSerializer::serializeTransformationMatrix(state, builder, context, transform);
        return;
    }

    // https://w3c.github.io/csswg-drafts/css-transforms-1/#serialization-of-the-computed-value
    // If we don't have a renderer, then the value should be "none" if we're asking for the
    // resolved value (such as when calling getComputedStyle()).
    if (state.valueType == ExtractorState::PropertyValueType::Resolved) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    if (state.style.transform().isEmpty()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    builder.append(interleave(state.style.transform(), [&](auto& builder, auto& transform) {
        ExtractorSerializer::serializeTransformOperation(state, builder, context, transform);
    }, ' '));
}

inline Ref<CSSValue> ExtractorCustom::extractTranslate(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyTranslate>(state);
}

inline void ExtractorCustom::extractTranslateSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyTranslate>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractScale(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyScale>(state);
}

inline void ExtractorCustom::extractScaleSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyScale>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractRotate(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyRotate>(state);
}

inline void ExtractorCustom::extractRotateSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyRotate>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractGridAutoFlow(ExtractorState& state)
{
    CSSValueListBuilder list;
    ASSERT(state.style.isGridAutoFlowDirectionRow() || state.style.isGridAutoFlowDirectionColumn());
    if (state.style.isGridAutoFlowDirectionColumn())
        list.append(CSSPrimitiveValue::create(CSSValueColumn));
    else if (!state.style.isGridAutoFlowAlgorithmDense())
        list.append(CSSPrimitiveValue::create(CSSValueRow));

    if (state.style.isGridAutoFlowAlgorithmDense())
        list.append(CSSPrimitiveValue::create(CSSValueDense));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractGridAutoFlowSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ASSERT(state.style.isGridAutoFlowDirectionRow() || state.style.isGridAutoFlowDirectionColumn());

    bool listEmpty = true;
    if (state.style.isGridAutoFlowDirectionColumn()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::Column { });
        listEmpty = false;
    } else if (!state.style.isGridAutoFlowAlgorithmDense()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::Row { });
        listEmpty = false;
    }

    if (state.style.isGridAutoFlowAlgorithmDense()) {
        if (!listEmpty)
            builder.append(' ');
        CSS::serializationForCSS(builder, context, CSS::Keyword::Dense { });
    }
}

inline Ref<CSSValue> ExtractorCustom::extractGridTemplateAreas(ExtractorState& state)
{
    if (!state.style.namedGridAreaRowCount()) {
        ASSERT(!state.style.namedGridAreaColumnCount());
        return CSSPrimitiveValue::create(CSSValueNone);
    }
    return CSSGridTemplateAreasValue::create(
        state.style.namedGridArea(),
        state.style.namedGridAreaRowCount(),
        state.style.namedGridAreaColumnCount()
    );
}

inline void ExtractorCustom::extractGridTemplateAreasSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (!state.style.namedGridAreaRowCount()) {
        ASSERT(!state.style.namedGridAreaColumnCount());
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(CSSGridTemplateAreasValue::create(
        state.style.namedGridArea(),
        state.style.namedGridAreaRowCount(),
        state.style.namedGridAreaColumnCount()
    )->cssText(context));
}

inline Ref<CSSValue> ExtractorCustom::extractGridTemplateColumns(ExtractorState& state)
{
    return WebCore::Style::extractGridTemplateValue<GridTrackSizingDirection::ForColumns>(state);
}

inline void ExtractorCustom::extractGridTemplateColumnsSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    WebCore::Style::extractGridTemplateSerialization<GridTrackSizingDirection::ForColumns>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractGridTemplateRows(ExtractorState& state)
{
    return WebCore::Style::extractGridTemplateValue<GridTrackSizingDirection::ForRows>(state);
}

inline void ExtractorCustom::extractGridTemplateRowsSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    WebCore::Style::extractGridTemplateSerialization<GridTrackSizingDirection::ForRows>(state, builder, context);
}

// MARK: - Shorthands

inline RefPtr<CSSValue> ExtractorCustom::extractAnimationShorthand(ExtractorState& state)
{
    const auto& animations = state.style.animations();
    if (!animations || animations->isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (const auto& animation : *animations) {
        // If any of the reset-only longhands are set, we cannot serialize this value.
        if (animation->isTimelineSet() || animation->isRangeStartSet() || animation->isRangeEndSet()) {
            list.clear();
            break;
        }
        list.append(ExtractorConverter::convertSingleAnimation(state, animation));
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractAnimationShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    const auto& animations = state.style.animations();
    if (!animations || animations->isEmpty()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    for (const auto& animation : *animations) {
        // If any of the reset-only longhands are set, we cannot serialize this value.
        if (animation->isTimelineSet() || animation->isRangeStartSet() || animation->isRangeEndSet())
            return;
    }

    builder.append(interleave(*animations, [&](auto& builder, const auto& animation) {
        ExtractorSerializer::serializeSingleAnimation(state, builder, context, animation);
    }, ", "_s));
}

inline RefPtr<CSSValue> ExtractorCustom::extractAnimationRangeShorthand(ExtractorState& state)
{
    auto mapper = [](auto& state, const Animation* animation, const AnimationList* animationList) -> RefPtr<CSSValue> {
        if (!animation)
            return ExtractorConverter::convertAnimationRange(state, Animation::initialRange(), animation, animationList);
        if (!animation->isRangeFilled())
            return ExtractorConverter::convertAnimationRange(state, animation->range(), animation, animationList);
        return nullptr;
    };
    return extractAnimationOrTransitionValue(state, state.style.animations(), mapper);
}

inline void ExtractorCustom::extractAnimationRangeShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto mapper = [](auto& state, auto& builder, const auto& context, bool includeComma, const Animation* animation, const AnimationList* animationList) {
        if (!animation) {
            if (includeComma)
                builder.append(", "_s);
            ExtractorSerializer::serializeAnimationRange(state, builder, context, Animation::initialRange(), animation, animationList);
            return;
        }
        if (!animation->isRangeFilled()) {
            if (includeComma)
                builder.append(", "_s);
            ExtractorSerializer::serializeAnimationRange(state, builder, context, animation->range(), animation, animationList);
            return;
        }
    };
    return extractAnimationOrTransitionValueSerialization(state, builder, context, state.style.animations(), mapper);
}

inline RefPtr<CSSValue> ExtractorCustom::extractBackgroundShorthand(ExtractorState& state)
{
    static constexpr std::array propertiesBeforeSlashSeparator { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat, CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition };
    static constexpr std::array propertiesAfterSlashSeparator { CSSPropertyBackgroundSize, CSSPropertyBackgroundOrigin, CSSPropertyBackgroundClip };

    return extractFillLayerPropertyShorthand(
        state,
        CSSPropertyBackground,
        StylePropertyShorthand(CSSPropertyBackground, std::span { propertiesBeforeSlashSeparator }),
        StylePropertyShorthand(CSSPropertyBackground, std::span { propertiesAfterSlashSeparator }),
        CSSPropertyBackgroundColor
    );
}

inline void ExtractorCustom::extractBackgroundShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    static constexpr std::array propertiesBeforeSlashSeparator { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat, CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition };
    static constexpr std::array propertiesAfterSlashSeparator { CSSPropertyBackgroundSize, CSSPropertyBackgroundOrigin, CSSPropertyBackgroundClip };

    extractFillLayerPropertyShorthandSerialization(
        state,
        builder,
        context,
        CSSPropertyBackground,
        StylePropertyShorthand(CSSPropertyBackground, std::span { propertiesBeforeSlashSeparator }),
        StylePropertyShorthand(CSSPropertyBackground, std::span { propertiesAfterSlashSeparator }),
        CSSPropertyBackgroundColor
    );
}

inline RefPtr<CSSValue> ExtractorCustom::extractBackgroundPositionShorthand(ExtractorState& state)
{
    auto mapper = [](auto& state, auto& layer) -> Ref<CSSValue> {
        return CSSValueList::createSpaceSeparated(
            ExtractorConverter::convertLength(state, layer.xPosition()),
            ExtractorConverter::convertLength(state, layer.yPosition())
        );
    };
    return extractFillLayerValue(state, state.style.backgroundLayers(), mapper);
}

inline void ExtractorCustom::extractBackgroundPositionShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto mapper = [](auto& state, auto& builder, const auto& context, bool includeComma, auto& layer) {
        if (includeComma)
            builder.append(", "_s);
        ExtractorSerializer::serializeLength(state, builder, context, layer.xPosition());
        builder.append(' ');
        ExtractorSerializer::serializeLength(state, builder, context, layer.yPosition());
    };
    extractFillLayerValueSerialization(state, builder, context, state.style.backgroundLayers(), mapper);
}

inline RefPtr<CSSValue> ExtractorCustom::extractBlockStepShorthand(ExtractorState& state)
{
    CSSValueListBuilder list;
    if (auto blockStepSize = state.style.blockStepSize())
        list.append(ExtractorConverter::convertLength(state, *blockStepSize));

    if (auto blockStepInsert = state.style.blockStepInsert(); blockStepInsert != RenderStyle::initialBlockStepInsert())
        list.append(ExtractorConverter::convert(state, blockStepInsert));

    if (auto blockStepAlign = state.style.blockStepAlign(); blockStepAlign != RenderStyle::initialBlockStepAlign())
        list.append(ExtractorConverter::convert(state, blockStepAlign));

    if (auto blockStepRound = state.style.blockStepRound(); blockStepRound != RenderStyle::initialBlockStepRound())
        list.append(ExtractorConverter::convert(state, blockStepRound));

    if (!list.isEmpty())
        return CSSValueList::createSpaceSeparated(list);

    return CSSPrimitiveValue::create(CSSValueNone);
}

inline void ExtractorCustom::extractBlockStepShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    bool listEmpty = true;

    if (auto blockStepSize = state.style.blockStepSize()) {
        if (!listEmpty)
            builder.append(' ');
        ExtractorSerializer::serializeLength(state, builder, context, *blockStepSize);
        listEmpty = false;
    }

    if (auto blockStepInsert = state.style.blockStepInsert(); blockStepInsert != RenderStyle::initialBlockStepInsert()) {
        if (!listEmpty)
            builder.append(' ');
        ExtractorSerializer::serialize(state, builder, context, blockStepInsert);
        listEmpty = false;
    }

    if (auto blockStepAlign = state.style.blockStepAlign(); blockStepAlign != RenderStyle::initialBlockStepAlign()) {
        if (!listEmpty)
            builder.append(' ');
        ExtractorSerializer::serialize(state, builder, context, blockStepAlign);
        listEmpty = false;
    }

    if (auto blockStepRound = state.style.blockStepRound(); blockStepRound != RenderStyle::initialBlockStepRound()) {
        if (!listEmpty)
            builder.append(' ');
        ExtractorSerializer::serialize(state, builder, context, blockStepRound);
        listEmpty = false;
    }

    if (listEmpty)
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
}

inline RefPtr<CSSValue> ExtractorCustom::extractBorderShorthand(ExtractorState& state)
{
    static constexpr std::array properties { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
    return WebCore::Style::extractBorderShorthand(state, std::span { properties });
}

inline void ExtractorCustom::extractBorderShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    static constexpr std::array properties { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
    WebCore::Style::extractBorderShorthandSerialization(state, builder, context, std::span { properties });
}

inline RefPtr<CSSValue> ExtractorCustom::extractBorderBlockShorthand(ExtractorState& state)
{
    static constexpr std::array properties { CSSPropertyBorderBlockStart, CSSPropertyBorderBlockEnd };
    return WebCore::Style::extractBorderShorthand(state, std::span { properties });
}

inline void ExtractorCustom::extractBorderBlockShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    static constexpr std::array properties { CSSPropertyBorderBlockStart, CSSPropertyBorderBlockEnd };
    WebCore::Style::extractBorderShorthandSerialization(state, builder, context, std::span { properties });
}

inline RefPtr<CSSValue> ExtractorCustom::extractBorderImageShorthand(ExtractorState& state)
{
    auto& borderImage = state.style.borderImage();
    if (!borderImage.image())
        return CSSPrimitiveValue::create(CSSValueNone);
    if (borderImage.overridesBorderWidths())
        return nullptr;
    return ExtractorConverter::convertNinePieceImage(state, borderImage);
}

inline void ExtractorCustom::extractBorderImageShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& borderImage = state.style.borderImage();
    if (!borderImage.image()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }
    if (borderImage.overridesBorderWidths())
        return;
    ExtractorSerializer::serializeNinePieceImage(state, builder, context, borderImage);
}

inline RefPtr<CSSValue> ExtractorCustom::extractBorderInlineShorthand(ExtractorState& state)
{
    static constexpr std::array properties { CSSPropertyBorderInlineStart, CSSPropertyBorderInlineEnd };
    return WebCore::Style::extractBorderShorthand(state, std::span { properties });
}

inline void ExtractorCustom::extractBorderInlineShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    static constexpr std::array properties { CSSPropertyBorderInlineStart, CSSPropertyBorderInlineEnd };
    WebCore::Style::extractBorderShorthandSerialization(state, builder, context, std::span { properties });
}

inline RefPtr<CSSValue> ExtractorCustom::extractBorderRadiusShorthand(ExtractorState& state)
{
    return WebCore::Style::extractBorderRadiusShorthand(state, CSSPropertyBorderRadius);
}

inline void ExtractorCustom::extractBorderRadiusShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractBorderRadiusShorthand(state)->cssText(context));
}

inline RefPtr<CSSValue> ExtractorCustom::extractColumnsShorthand(ExtractorState& state)
{
    if (state.style.hasAutoColumnCount())
        return state.style.hasAutoColumnWidth() ? CSSPrimitiveValue::create(CSSValueAuto) : ExtractorConverter::convertNumberAsPixels(state, state.style.columnWidth());
    if (state.style.hasAutoColumnWidth())
        return state.style.hasAutoColumnCount() ? CSSPrimitiveValue::create(CSSValueAuto) : CSSPrimitiveValue::create(state.style.columnCount());
    return extractStandardSpaceSeparatedShorthand(state, columnsShorthand());
}

inline void ExtractorCustom::extractColumnsShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (state.style.hasAutoColumnCount()) {
        if (state.style.hasAutoColumnWidth()) {
            CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
            return;
        }
        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, state.style.columnWidth());
        return;
    }
    if (state.style.hasAutoColumnWidth()) {
        if (state.style.hasAutoColumnCount()) {
            CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
            return;
        }
        ExtractorSerializer::serializeNumber(state, builder, context, state.style.columnCount());
        return;
    }

    extractStandardSpaceSeparatedShorthandSerialization(state, builder, context, columnsShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractContainerShorthand(ExtractorState& state)
{
    auto name = [&]() -> Ref<CSSValue> {
        if (state.style.containerNames().isNone())
            return CSSPrimitiveValue::create(CSSValueNone);
        return ExtractorGenerated::extractValue(state, CSSPropertyContainerName).releaseNonNull();
    }();

    if (state.style.containerType() == ContainerType::Normal)
        return CSSValueList::createSlashSeparated(WTFMove(name));

    return CSSValueList::createSlashSeparated(
        WTFMove(name),
        ExtractorGenerated::extractValue(state, CSSPropertyContainerType).releaseNonNull()
    );
}

inline void ExtractorCustom::extractContainerShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (state.style.containerNames().isNone())
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
    else
        ExtractorGenerated::extractValueSerialization(state, builder, context, CSSPropertyContainerName);

    if (state.style.containerType() == ContainerType::Normal)
        return;

    builder.append(" / "_s);
    ExtractorGenerated::extractValueSerialization(state, builder, context, CSSPropertyContainerType);
}

inline RefPtr<CSSValue> ExtractorCustom::extractFlexFlowShorthand(ExtractorState& state)
{
    if (state.style.flexWrap() == RenderStyle::initialFlexWrap())
        return ExtractorConverter::convert(state, state.style.flexDirection());
    if (state.style.flexDirection() == RenderStyle::initialFlexDirection())
        return ExtractorConverter::convert(state, state.style.flexWrap());
    return extractStandardSpaceSeparatedShorthand(state, flexFlowShorthand());
}

inline void ExtractorCustom::extractFlexFlowShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (state.style.flexWrap() == RenderStyle::initialFlexWrap()) {
        ExtractorSerializer::serialize(state, builder, context, state.style.flexDirection());
        return;
    }
    if (state.style.flexDirection() == RenderStyle::initialFlexDirection()) {
        ExtractorSerializer::serialize(state, builder, context, state.style.flexWrap());
        return;
    }
    extractStandardSpaceSeparatedShorthandSerialization(state, builder, context, flexFlowShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractFontShorthand(ExtractorState& state)
{
    auto& description = state.style.fontDescription();
    auto fontWidth = fontWidthKeyword(description.width());
    auto fontStyle = fontStyleKeyword(description.italic(), description.fontStyleAxis());

    auto propertiesResetByShorthandAreExpressible = [&] {
        // The font shorthand can express "font-variant-caps: small-caps". Overwrite with "normal" so we can use isAllNormal to check that all the other settings are normal.
        auto variantSettingsOmittingExpressible = description.variantSettings();
        if (variantSettingsOmittingExpressible.caps == FontVariantCaps::Small)
            variantSettingsOmittingExpressible.caps = FontVariantCaps::Normal;

        // When we add font-language-override, also add code to check for non-expressible values for it here.
        return variantSettingsOmittingExpressible.isAllNormal()
            && fontWidth
            && fontStyle
            && description.fontSizeAdjust().isNone()
            && description.kerning() == Kerning::Auto
            && description.featureSettings().isEmpty()
            && description.opticalSizing() == FontOpticalSizing::Enabled
            && description.variationSettings().isEmpty();
    };

    auto computedFont = CSSFontValue::create();

    if (!propertiesResetByShorthandAreExpressible())
        return computedFont;

    computedFont->size = ExtractorConverter::convertNumberAsPixels(state, description.computedSize());

    auto computedLineHeight = dynamicDowncast<CSSPrimitiveValue>(ExtractorGenerated::extractValue(state, CSSPropertyLineHeight));
    if (computedLineHeight && !isValueID(*computedLineHeight, CSSValueNormal))
        computedFont->lineHeight = computedLineHeight.releaseNonNull();

    if (description.variantCaps() == FontVariantCaps::Small)
        computedFont->variant = CSSPrimitiveValue::create(CSSValueSmallCaps);
    if (float weight = description.weight(); weight != 400)
        computedFont->weight = CSSPrimitiveValue::create(weight);
    if (*fontWidth != CSSValueNormal)
        computedFont->width = CSSPrimitiveValue::create(*fontWidth);
    if (*fontStyle != CSSValueNormal)
        computedFont->style = CSSPrimitiveValue::create(*fontStyle);

    CSSValueListBuilder familyList;
    for (unsigned i = 0; i < state.style.fontCascade().familyCount(); ++i)
        familyList.append(ExtractorConverter::convertFontFamily(state, state.style.fontCascade().familyAt(i)));
    computedFont->family = CSSValueList::createCommaSeparated(WTFMove(familyList));

    return computedFont;
}

inline void ExtractorCustom::extractFontShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractFontShorthand(state)->cssText(context));
}

inline RefPtr<CSSValue> ExtractorCustom::extractFontSynthesisShorthand(ExtractorState& state)
{
    auto& description = state.style.fontDescription();

    CSSValueListBuilder list;
    if (description.hasAutoFontSynthesisWeight())
        list.append(CSSPrimitiveValue::create(CSSValueWeight));
    if (description.hasAutoFontSynthesisStyle())
        list.append(CSSPrimitiveValue::create(CSSValueStyle));
    if (description.hasAutoFontSynthesisSmallCaps())
        list.append(CSSPrimitiveValue::create(CSSValueSmallCaps));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractFontSynthesisShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& description = state.style.fontDescription();

    bool listEmpty = true;
    auto appendOption = [&](bool hasValue, auto value) {
        if (hasValue) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(description.hasAutoFontSynthesisWeight(), CSSValueWeight);
    appendOption(description.hasAutoFontSynthesisStyle(), CSSValueStyle);
    appendOption(description.hasAutoFontSynthesisSmallCaps(), CSSValueSmallCaps);

    if (listEmpty)
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
}

inline RefPtr<CSSValue> ExtractorCustom::extractFontVariantShorthand(ExtractorState& state)
{
    CSSValueListBuilder list;
    for (auto longhand : fontVariantShorthand()) {
        auto value = ExtractorGenerated::extractValue(state, longhand);
        // We may not have a value if the longhand is disabled.
        if (!value || isValueID(value, CSSValueNormal))
            continue;
        list.append(value.releaseNonNull());
    }
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNormal);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractFontVariantShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractFontVariantShorthand(state)->cssText(context));
}

inline RefPtr<CSSValue> ExtractorCustom::extractLineClampShorthand(ExtractorState& state)
{
    auto maxLines = state.style.maxLines();
    if (!maxLines)
        return CSSPrimitiveValue::create(CSSValueNone);

    Ref maxLinesValue = CSSPrimitiveValue::create(maxLines, CSSUnitType::CSS_INTEGER);

    switch (state.style.blockEllipsis().type) {
    case BlockEllipsis::Type::None:
        return CSSValuePair::create(WTFMove(maxLinesValue), CSSPrimitiveValue::create(CSSValueNone));
    case BlockEllipsis::Type::Auto:
        return CSSValuePair::create(WTFMove(maxLinesValue), CSSPrimitiveValue::create(CSSValueAuto));
    case BlockEllipsis::Type::String:
        return CSSValuePair::create(WTFMove(maxLinesValue), CSSPrimitiveValue::createCustomIdent(state.style.blockEllipsis().string));
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorCustom::extractLineClampShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto maxLines = state.style.maxLines();
    if (!maxLines) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { maxLines });
    builder.append(' ');

    switch (state.style.blockEllipsis().type) {
    case BlockEllipsis::Type::None:
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    case BlockEllipsis::Type::Auto:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
        return;
    case BlockEllipsis::Type::String:
        CSS::serializationForCSS(builder, context, CustomIdentifier { state.style.blockEllipsis().string });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline RefPtr<CSSValue> ExtractorCustom::extractMaskShorthand(ExtractorState& state)
{
    static constexpr std::array propertiesBeforeSlashSeparator { CSSPropertyMaskImage, CSSPropertyMaskPosition };
    static constexpr std::array propertiesAfterSlashSeparator { CSSPropertyMaskSize, CSSPropertyMaskRepeat, CSSPropertyMaskOrigin, CSSPropertyMaskClip, CSSPropertyMaskComposite, CSSPropertyMaskMode };

    return extractFillLayerPropertyShorthand(
        state,
        CSSPropertyMask,
        StylePropertyShorthand(CSSPropertyMask, std::span { propertiesBeforeSlashSeparator }),
        StylePropertyShorthand(CSSPropertyMask, std::span { propertiesAfterSlashSeparator }),
        CSSPropertyInvalid
    );
}

inline void ExtractorCustom::extractMaskShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    static constexpr std::array propertiesBeforeSlashSeparator { CSSPropertyMaskImage, CSSPropertyMaskPosition };
    static constexpr std::array propertiesAfterSlashSeparator { CSSPropertyMaskSize, CSSPropertyMaskRepeat, CSSPropertyMaskOrigin, CSSPropertyMaskClip, CSSPropertyMaskComposite, CSSPropertyMaskMode };

    extractFillLayerPropertyShorthandSerialization(
        state,
        builder,
        context,
        CSSPropertyMask,
        StylePropertyShorthand(CSSPropertyMask, std::span { propertiesBeforeSlashSeparator }),
        StylePropertyShorthand(CSSPropertyMask, std::span { propertiesAfterSlashSeparator }),
        CSSPropertyInvalid
    );
}

inline RefPtr<CSSValue> ExtractorCustom::extractMaskBorderShorthand(ExtractorState& state)
{
    auto& maskBorder = state.style.maskBorder();
    if (!maskBorder.image())
        return CSSPrimitiveValue::create(CSSValueNone);
    if (maskBorder.overridesBorderWidths())
        return nullptr;
    return ExtractorConverter::convertNinePieceImage(state, maskBorder);
}

inline void ExtractorCustom::extractMaskBorderShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& maskBorder = state.style.maskBorder();
    if (!maskBorder.image()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }
    if (maskBorder.overridesBorderWidths())
        return;
    ExtractorSerializer::serializeNinePieceImage(state, builder, context, maskBorder);
}

inline RefPtr<CSSValue> ExtractorCustom::extractMaskPositionShorthand(ExtractorState& state)
{
    auto mapper = [](auto& state, auto& layer) -> Ref<CSSValue> {
        return CSSValueList::createSpaceSeparated(
            ExtractorConverter::convertLength(state, layer.xPosition()),
            ExtractorConverter::convertLength(state, layer.yPosition())
        );
    };
    return extractFillLayerValue(state, state.style.maskLayers(), mapper);
}

inline void ExtractorCustom::extractMaskPositionShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto mapper = [](auto& state, auto& builder, const auto& context, bool includeComma, auto& layer) {
        if (includeComma)
            builder.append(", "_s);
        ExtractorSerializer::serializeLength(state, builder, context, layer.xPosition());
        builder.append(' ');
        ExtractorSerializer::serializeLength(state, builder, context, layer.yPosition());
    };
    extractFillLayerValueSerialization(state, builder, context, state.style.maskLayers(), mapper);
}

inline RefPtr<CSSValue> ExtractorCustom::extractOffsetShorthand(ExtractorState& state)
{
    // [ <'offset-position'>? [ <'offset-path'> [ <'offset-distance'> || <'offset-rotate'> ]? ]? ]! [ / <'offset-anchor'> ]?

    // The first four elements are serialized in a space separated CSSValueList.
    // This is then combined with offset-anchor in a slash separated CSSValueList.

    CSSValueListBuilder innerList;

    WTF::switchOn(state.style.offsetPosition(),
        [&](const CSS::Keyword::Auto&) { },
        [&](const CSS::Keyword::Normal&) { },
        [&](const Position& position) {
            innerList.append(ExtractorConverter::convertStyleType(state, position));
        }
    );

    bool nonInitialDistance = state.style.offsetDistance() != state.style.initialOffsetDistance();
    bool nonInitialRotate = state.style.offsetRotate() != state.style.initialOffsetRotate();

    if (state.style.hasOffsetPath() || nonInitialDistance || nonInitialRotate)
        innerList.append(ExtractorConverter::convertStyleType(state, state.style.offsetPath()));

    if (nonInitialDistance)
        innerList.append(ExtractorConverter::convertStyleType(state, state.style.offsetDistance()));
    if (nonInitialRotate)
        innerList.append(ExtractorConverter::convertStyleType(state, state.style.offsetRotate()));

    auto inner = innerList.isEmpty()
        ? Ref<CSSValue> { CSSPrimitiveValue::create(CSSValueAuto) }
        : Ref<CSSValue> { CSSValueList::createSpaceSeparated(WTFMove(innerList)) };

    return WTF::switchOn(state.style.offsetAnchor(),
        [&](const CSS::Keyword::Auto&) -> Ref<CSSValue> {
            return inner;
        },
        [&](const Position& position) -> Ref<CSSValue> {
            return CSSValueList::createSlashSeparated(
                WTFMove(inner),
                ExtractorConverter::convertStyleType(state, position)
            );
        }
    );
}

inline void ExtractorCustom::extractOffsetShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractOffsetShorthand(state)->cssText(context));
}

inline RefPtr<CSSValue> ExtractorCustom::extractOverscrollBehaviorShorthand(ExtractorState& state)
{
    return ExtractorConverter::convert(state, std::max(state.style.overscrollBehaviorX(), state.style.overscrollBehaviorY()));
}

inline void ExtractorCustom::extractOverscrollBehaviorShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serialize(state, builder, context, std::max(state.style.overscrollBehaviorX(), state.style.overscrollBehaviorY()));
}

inline RefPtr<CSSValue> ExtractorCustom::extractPageBreakAfterShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertPageBreak(state, state.style.breakAfter());
}

inline void ExtractorCustom::extractPageBreakAfterShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializePageBreak(state, builder, context, state.style.breakAfter());
}

inline RefPtr<CSSValue> ExtractorCustom::extractPageBreakBeforeShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertPageBreak(state, state.style.breakBefore());
}

inline void ExtractorCustom::extractPageBreakBeforeShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializePageBreak(state, builder, context, state.style.breakBefore());
}

inline RefPtr<CSSValue> ExtractorCustom::extractPageBreakInsideShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertPageBreak(state, state.style.breakInside());
}

inline void ExtractorCustom::extractPageBreakInsideShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializePageBreak(state, builder, context, state.style.breakInside());
}

inline RefPtr<CSSValue> ExtractorCustom::extractPerspectiveOriginShorthand(ExtractorState& state)
{
    CSSValueListBuilder list;
    if (state.renderer) {
        auto box = state.renderer->transformReferenceBoxRect(state.style);
        list.append(ExtractorConverter::convertNumberAsPixels(state, minimumValueForLength(state.style.perspectiveOriginX(), box.width())));
        list.append(ExtractorConverter::convertNumberAsPixels(state, minimumValueForLength(state.style.perspectiveOriginY(), box.height())));
    } else {
        list.append(ExtractorConverter::convertLength(state, state.style.perspectiveOriginX()));
        list.append(ExtractorConverter::convertLength(state, state.style.perspectiveOriginY()));
    }
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractPerspectiveOriginShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (state.renderer) {
        auto box = state.renderer->transformReferenceBoxRect(state.style);
        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, minimumValueForLength(state.style.perspectiveOriginX(), box.width()));
        builder.append(' ');
        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, minimumValueForLength(state.style.perspectiveOriginY(), box.height()));
    } else {
        ExtractorSerializer::serializeLength(state, builder, context, state.style.perspectiveOriginX());
        builder.append(' ');
        ExtractorSerializer::serializeLength(state, builder, context, state.style.perspectiveOriginY());
    }
}

inline RefPtr<CSSValue> ExtractorCustom::extractPositionTryShorthand(ExtractorState& state)
{
    if (state.style.positionTryOrder() == RenderStyle::initialPositionTryOrder())
        return ExtractorGenerated::extractValue(state, CSSPropertyPositionTryFallbacks);
    return extractStandardSpaceSeparatedShorthand(state, positionTryShorthand());
}

inline void ExtractorCustom::extractPositionTryShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (state.style.positionTryOrder() == RenderStyle::initialPositionTryOrder()) {
        ExtractorGenerated::extractValueSerialization(state, builder, context, CSSPropertyPositionTryFallbacks);
        return;
    }
    return extractStandardSpaceSeparatedShorthandSerialization(state, builder, context, positionTryShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractScrollTimelineShorthand(ExtractorState& state)
{
    auto& timelines = state.style.scrollTimelines();
    if (timelines.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& timeline : timelines) {
        auto& name = timeline->name();
        auto axis = timeline->axis();

        ASSERT(!name.isNull());
        auto nameCSSValue = CSSPrimitiveValue::createCustomIdent(name);

        if (axis == ScrollAxis::Block)
            list.append(WTFMove(nameCSSValue));
        else
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, ExtractorConverter::convert(state, axis)));
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractScrollTimelineShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& timelines = state.style.scrollTimelines();
    if (timelines.isEmpty()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    builder.append(interleave(timelines, [&](auto& builder, auto& timeline) {
        ASSERT(!timeline->name().isNull());

        CSS::serializationForCSS(builder, context, CustomIdentifier { timeline->name() });
        if (auto axis = timeline->axis(); axis != ScrollAxis::Block) {
            builder.append(' ');
            ExtractorSerializer::serialize(state, builder, context, axis);
        }
    }, ", "_s));
}

inline RefPtr<CSSValue> ExtractorCustom::extractTextBoxShorthand(ExtractorState& state)
{
    auto textBoxTrim = state.style.textBoxTrim();
    auto textBoxEdge = state.style.textBoxEdge();
    auto textBoxEdgeIsAuto = textBoxEdge == TextEdge { TextEdgeType::Auto, TextEdgeType::Auto };

    if (textBoxTrim == TextBoxTrim::None && textBoxEdgeIsAuto)
        return CSSPrimitiveValue::create(CSSValueNormal);
    if (textBoxEdgeIsAuto)
        return ExtractorConverter::convert(state, textBoxTrim);
    if (textBoxTrim == TextBoxTrim::TrimBoth)
        return ExtractorConverter::convertTextBoxEdge(state, textBoxEdge);

    return CSSValuePair::create(
        ExtractorConverter::convert(state, textBoxTrim),
        ExtractorConverter::convertTextBoxEdge(state, textBoxEdge)
    );
}

inline void ExtractorCustom::extractTextBoxShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto textBoxTrim = state.style.textBoxTrim();
    auto textBoxEdge = state.style.textBoxEdge();
    auto textBoxEdgeIsAuto = textBoxEdge == TextEdge { TextEdgeType::Auto, TextEdgeType::Auto };

    if (textBoxTrim == TextBoxTrim::None && textBoxEdgeIsAuto) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::Normal { });
        return;
    }
    if (textBoxEdgeIsAuto) {
        ExtractorSerializer::serialize(state, builder, context, textBoxTrim);
        return;
    }
    if (textBoxTrim == TextBoxTrim::TrimBoth) {
        ExtractorSerializer::serializeTextBoxEdge(state, builder, context, textBoxEdge);
        return;
    }

    ExtractorSerializer::serialize(state, builder, context, textBoxTrim);
    builder.append(' ');
    ExtractorSerializer::serializeTextBoxEdge(state, builder, context, textBoxEdge);
}

inline RefPtr<CSSValue> ExtractorCustom::extractTextDecorationSkipShorthand(ExtractorState& state)
{
    switch (state.style.textDecorationSkipInk()) {
    case TextDecorationSkipInk::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case TextDecorationSkipInk::Auto:
        return CSSPrimitiveValue::create(CSSValueAuto);
    case TextDecorationSkipInk::All:
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorCustom::extractTextDecorationSkipShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    switch (state.style.textDecorationSkipInk()) {
    case TextDecorationSkipInk::None:
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    case TextDecorationSkipInk::Auto:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
        return;
    case TextDecorationSkipInk::All:
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline RefPtr<CSSValue> ExtractorCustom::extractTextWrapShorthand(ExtractorState& state)
{
    auto textWrapMode = state.style.textWrapMode();
    auto textWrapStyle = state.style.textWrapStyle();

    if (textWrapStyle == TextWrapStyle::Auto)
        return ExtractorConverter::convert(state, textWrapMode);
    if (textWrapMode == TextWrapMode::Wrap)
        return ExtractorConverter::convert(state, textWrapStyle);

    return CSSValuePair::create(
        ExtractorConverter::convert(state, textWrapMode),
        ExtractorConverter::convert(state, textWrapStyle)
    );
}

inline void ExtractorCustom::extractTextWrapShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto textWrapMode = state.style.textWrapMode();
    auto textWrapStyle = state.style.textWrapStyle();

    if (textWrapStyle == TextWrapStyle::Auto) {
        ExtractorSerializer::serialize(state, builder, context, textWrapMode);
        return;
    }
    if (textWrapMode == TextWrapMode::Wrap) {
        ExtractorSerializer::serialize(state, builder, context, textWrapStyle);
        return;
    }

    ExtractorSerializer::serialize(state, builder, context, textWrapMode);
    builder.append(' ');
    ExtractorSerializer::serialize(state, builder, context, textWrapStyle);
}

inline RefPtr<CSSValue> ExtractorCustom::extractTransformOriginShorthand(ExtractorState& state)
{
    CSSValueListBuilder list;
    if (state.renderer) {
        auto box = state.renderer->transformReferenceBoxRect(state.style);
        list.append(ExtractorConverter::convertNumberAsPixels(state, minimumValueForLength(state.style.transformOriginX(), box.width())));
        list.append(ExtractorConverter::convertNumberAsPixels(state, minimumValueForLength(state.style.transformOriginY(), box.height())));
        if (auto transformOriginZ = state.style.transformOriginZ())
            list.append(ExtractorConverter::convertNumberAsPixels(state, transformOriginZ));
    } else {
        list.append(ExtractorConverter::convertLength(state, state.style.transformOriginX()));
        list.append(ExtractorConverter::convertLength(state, state.style.transformOriginY()));
        if (auto transformOriginZ = state.style.transformOriginZ())
            list.append(ExtractorConverter::convertNumberAsPixels(state, transformOriginZ));
    }
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractTransformOriginShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (state.renderer) {
        auto box = state.renderer->transformReferenceBoxRect(state.style);
        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, minimumValueForLength(state.style.transformOriginX(), box.width()));
        builder.append(' ');
        ExtractorSerializer::serializeNumberAsPixels(state, builder, context, minimumValueForLength(state.style.transformOriginY(), box.height()));
        if (auto transformOriginZ = state.style.transformOriginZ()) {
            builder.append(' ');
            ExtractorSerializer::serializeNumberAsPixels(state, builder, context, transformOriginZ);
        }
    } else {
        ExtractorSerializer::serializeLength(state, builder, context, state.style.transformOriginX());
        builder.append(' ');
        ExtractorSerializer::serializeLength(state, builder, context, state.style.transformOriginY());
        if (auto transformOriginZ = state.style.transformOriginZ()) {
            builder.append(' ');
            ExtractorSerializer::serializeNumberAsPixels(state, builder, context, transformOriginZ);
        }
    }
}

inline RefPtr<CSSValue> ExtractorCustom::extractTransitionShorthand(ExtractorState& state)
{
    auto transitions = state.style.transitions();
    if (!transitions || transitions->isEmpty())
        return CSSPrimitiveValue::create(CSSValueAll);

    CSSValueListBuilder list;
    for (auto& transition : *transitions)
        list.append(ExtractorConverter::convertSingleTransition(state, transition));
    ASSERT(!list.isEmpty());
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractTransitionShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto transitions = state.style.transitions();
    if (!transitions || transitions->isEmpty()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::All { });
        return;
    }

    builder.append(interleave(*transitions, [&](auto& builder, auto& transition) {
        ExtractorSerializer::serializeSingleTransition(state, builder, context, transition);
    }, ", "_s));
}

inline RefPtr<CSSValue> ExtractorCustom::extractViewTimelineShorthand(ExtractorState& state)
{
    auto& timelines = state.style.viewTimelines();
    if (timelines.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& timeline : timelines) {
        auto& name = timeline->name();
        auto axis = timeline->axis();
        auto& insets = timeline->insets();

        auto hasDefaultAxis = axis == ScrollAxis::Block;
        auto hasDefaultInsets = [insets]() {
            if (!insets.start && !insets.end)
                return true;
            if (insets.start->isAuto())
                return true;
            return false;
        }();

        ASSERT(!name.isNull());
        auto nameCSSValue = CSSPrimitiveValue::createCustomIdent(name);

        if (hasDefaultAxis && hasDefaultInsets)
            list.append(WTFMove(nameCSSValue));
        else if (hasDefaultAxis)
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, createCSSValue(state.pool, state.style, insets)));
        else if (hasDefaultInsets)
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, ExtractorConverter::convert(state, axis)));
        else {
            list.append(CSSValueList::createSpaceSeparated(
                WTFMove(nameCSSValue),
                ExtractorConverter::convert(state, axis),
                createCSSValue(state.pool, state.style, insets)
            ));
        }
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractViewTimelineShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractViewTimelineShorthand(state)->cssText(context));
}

inline RefPtr<CSSValue> ExtractorCustom::extractWhiteSpaceShorthand(ExtractorState& state)
{
    auto whiteSpaceCollapse = state.style.whiteSpaceCollapse();
    auto textWrapMode = state.style.textWrapMode();

    // Convert to backwards-compatible keywords if possible.
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Collapse && textWrapMode == TextWrapMode::Wrap)
        return CSSPrimitiveValue::create(CSSValueNormal);
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Preserve && textWrapMode == TextWrapMode::NoWrap)
        return CSSPrimitiveValue::create(CSSValuePre);
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Preserve && textWrapMode == TextWrapMode::Wrap)
        return CSSPrimitiveValue::create(CSSValuePreWrap);
    if (whiteSpaceCollapse == WhiteSpaceCollapse::PreserveBreaks && textWrapMode == TextWrapMode::Wrap)
        return CSSPrimitiveValue::create(CSSValuePreLine);

    // Omit default longhand values.
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Collapse)
        return ExtractorConverter::convert(state, textWrapMode);
    if (textWrapMode == TextWrapMode::Wrap)
        return ExtractorConverter::convert(state, whiteSpaceCollapse);

    return CSSValuePair::create(
        ExtractorConverter::convert(state, whiteSpaceCollapse),
        ExtractorConverter::convert(state, textWrapMode)
    );
}

inline void ExtractorCustom::extractWhiteSpaceShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto whiteSpaceCollapse = state.style.whiteSpaceCollapse();
    auto textWrapMode = state.style.textWrapMode();

    // Convert to backwards-compatible keywords if possible.
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Collapse && textWrapMode == TextWrapMode::Wrap) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::Normal { });
        return;
    }
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Preserve && textWrapMode == TextWrapMode::NoWrap) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::Pre { });
        return;
    }
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Preserve && textWrapMode == TextWrapMode::Wrap) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::PreWrap { });
        return;
    }
    if (whiteSpaceCollapse == WhiteSpaceCollapse::PreserveBreaks && textWrapMode == TextWrapMode::Wrap) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::PreLine { });
        return;
    }

    // Omit default longhand values.
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Collapse) {
        ExtractorSerializer::serialize(state, builder, context, textWrapMode);
        return;
    }
    if (textWrapMode == TextWrapMode::Wrap) {
        ExtractorSerializer::serialize(state, builder, context, whiteSpaceCollapse);
        return;
    }

    ExtractorSerializer::serialize(state, builder, context, whiteSpaceCollapse);
    builder.append(' ');
    ExtractorSerializer::serialize(state, builder, context, textWrapMode);
}

inline RefPtr<CSSValue> ExtractorCustom::extractWebkitBorderImageShorthand(ExtractorState& state)
{
    auto& borderImage = state.style.borderImage();
    if (!borderImage.image())
        return CSSPrimitiveValue::create(CSSValueNone);
    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    bool overridesBorderWidths = borderImage.borderSlices().anyOf([](const auto& side) { return side.isFixed(); });
    if (overridesBorderWidths != borderImage.overridesBorderWidths())
        return nullptr;
    return ExtractorConverter::convertNinePieceImage(state, borderImage);
}

inline void ExtractorCustom::extractWebkitBorderImageShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& borderImage = state.style.borderImage();
    if (!borderImage.image()) {
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }
    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    bool overridesBorderWidths = borderImage.borderSlices().anyOf([](const auto& side) { return side.isFixed(); });
    if (overridesBorderWidths != borderImage.overridesBorderWidths())
        return;
    ExtractorSerializer::serializeNinePieceImage(state, builder, context, borderImage);
}

inline RefPtr<CSSValue> ExtractorCustom::extractWebkitBorderRadiusShorthand(ExtractorState& state)
{
    return WebCore::Style::extractBorderRadiusShorthand(state, CSSPropertyWebkitBorderRadius);
}

inline void ExtractorCustom::extractWebkitBorderRadiusShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    WebCore::Style::extractBorderRadiusShorthandSerialization(state, builder, context, CSSPropertyWebkitBorderRadius);
}

inline RefPtr<CSSValue> ExtractorCustom::extractWebkitColumnBreakAfterShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertWebkitColumnBreak(state, state.style.breakAfter());
}

inline void ExtractorCustom::extractWebkitColumnBreakAfterShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeWebkitColumnBreak(state, builder, context, state.style.breakAfter());
}

inline RefPtr<CSSValue> ExtractorCustom::extractWebkitColumnBreakBeforeShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertWebkitColumnBreak(state, state.style.breakBefore());
}

inline void ExtractorCustom::extractWebkitColumnBreakBeforeShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeWebkitColumnBreak(state, builder, context, state.style.breakBefore());
}

inline RefPtr<CSSValue> ExtractorCustom::extractWebkitColumnBreakInsideShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertWebkitColumnBreak(state, state.style.breakInside());
}

inline void ExtractorCustom::extractWebkitColumnBreakInsideShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    ExtractorSerializer::serializeWebkitColumnBreak(state, builder, context, state.style.breakInside());
}

inline RefPtr<CSSValue> ExtractorCustom::extractWebkitMaskBoxImageShorthand(ExtractorState& state)
{
    return extractMaskBorderShorthand(state);
}

inline void ExtractorCustom::extractWebkitMaskBoxImageShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractMaskBorderShorthandSerialization(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractWebkitMaskPositionShorthand(ExtractorState& state)
{
    return extractMaskPositionShorthand(state);
}

inline void ExtractorCustom::extractWebkitMaskPositionShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractMaskPositionShorthandSerialization(state, builder, context);
}

} // namespace Style
} // namespace WebCore
