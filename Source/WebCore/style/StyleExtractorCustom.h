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
#include "StyleInterpolation.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"

namespace WebCore {
namespace Style {

// Custom handling of computed value extraction.
class ExtractorCustom {
public:
    static Ref<CSSValue> extractValueAspectRatio(ExtractorState&);

    static Ref<CSSValue> extractValueDirection(ExtractorState&);
    static Ref<CSSValue> extractValueWritingMode(ExtractorState&);

    static Ref<CSSValue> extractValueFill(ExtractorState&);
    static Ref<CSSValue> extractValueStroke(ExtractorState&);

    static Ref<CSSValue> extractValueFloat(ExtractorState&);

    static Ref<CSSValue> extractValueClip(ExtractorState&);

    static Ref<CSSValue> extractValueContent(ExtractorState&);

    static Ref<CSSValue> extractValueCursor(ExtractorState&);

    static Ref<CSSValue> extractValueBaselineShift(ExtractorState&);
    static Ref<CSSValue> extractValueVerticalAlign(ExtractorState&);
    static Ref<CSSValue> extractValueTextEmphasisStyle(ExtractorState&);
    static Ref<CSSValue> extractValueTextIndent(ExtractorState&);
    static Ref<CSSValue> extractValueLetterSpacing(ExtractorState&);
    static Ref<CSSValue> extractValueWordSpacing(ExtractorState&);
    static Ref<CSSValue> extractValueLineHeight(ExtractorState&);

    static Ref<CSSValue> extractValueFontFamily(ExtractorState&);
    static Ref<CSSValue> extractValueFontSize(ExtractorState&);
    static Ref<CSSValue> extractValueFontStyle(ExtractorState&);
    static Ref<CSSValue> extractValueFontVariantLigatures(ExtractorState&);
    static Ref<CSSValue> extractValueFontVariantNumeric(ExtractorState&);
    static Ref<CSSValue> extractValueFontVariantAlternates(ExtractorState&);
    static Ref<CSSValue> extractValueFontVariantEastAsian(ExtractorState&);

    static Ref<CSSValue> extractValueTop(ExtractorState&);
    static Ref<CSSValue> extractValueRight(ExtractorState&);
    static Ref<CSSValue> extractValueBottom(ExtractorState&);
    static Ref<CSSValue> extractValueLeft(ExtractorState&);

    static Ref<CSSValue> extractValueMarginTop(ExtractorState&);
    static Ref<CSSValue> extractValueMarginRight(ExtractorState&);
    static Ref<CSSValue> extractValueMarginBottom(ExtractorState&);
    static Ref<CSSValue> extractValueMarginLeft(ExtractorState&);

    static Ref<CSSValue> extractValuePaddingTop(ExtractorState&);
    static Ref<CSSValue> extractValuePaddingRight(ExtractorState&);
    static Ref<CSSValue> extractValuePaddingBottom(ExtractorState&);
    static Ref<CSSValue> extractValuePaddingLeft(ExtractorState&);

    static Ref<CSSValue> extractValueHeight(ExtractorState&);
    static Ref<CSSValue> extractValueWidth(ExtractorState&);
    static Ref<CSSValue> extractValueMaxHeight(ExtractorState&);
    static Ref<CSSValue> extractValueMaxWidth(ExtractorState&);
    static Ref<CSSValue> extractValueMinHeight(ExtractorState&);
    static Ref<CSSValue> extractValueMinWidth(ExtractorState&);

    static Ref<CSSValue> extractValueCounterIncrement(ExtractorState&);
    static Ref<CSSValue> extractValueCounterReset(ExtractorState&);
    static Ref<CSSValue> extractValueCounterSet(ExtractorState&);

    static Ref<CSSValue> extractValueContainIntrinsicHeight(ExtractorState&);
    static Ref<CSSValue> extractValueContainIntrinsicWidth(ExtractorState&);

    static Ref<CSSValue> extractValueBorderImageOutset(ExtractorState&);
    static Ref<CSSValue> extractValueBorderImageRepeat(ExtractorState&);
    static Ref<CSSValue> extractValueBorderImageSlice(ExtractorState&);
    static RefPtr<CSSValue> extractValueBorderImageWidth(ExtractorState&);
    static Ref<CSSValue> extractValueMaskBorderOutset(ExtractorState&);
    static Ref<CSSValue> extractValueMaskBorderRepeat(ExtractorState&);
    static Ref<CSSValue> extractValueMaskBorderSlice(ExtractorState&);
    static Ref<CSSValue> extractValueMaskBorderWidth(ExtractorState&);

    static Ref<CSSValue> extractValueTransform(ExtractorState&);
    static Ref<CSSValue> extractValueTranslate(ExtractorState&);
    static Ref<CSSValue> extractValueScale(ExtractorState&);
    static Ref<CSSValue> extractValueRotate(ExtractorState&);
    static Ref<CSSValue> extractValuePerspective(ExtractorState&);

    static Ref<CSSValue> extractValueGridAutoFlow(ExtractorState&);
    static Ref<CSSValue> extractValueGridTemplateAreas(ExtractorState&);
    static Ref<CSSValue> extractValueGridTemplateColumns(ExtractorState&);
    static Ref<CSSValue> extractValueGridTemplateRows(ExtractorState&);

    // MARK: Shorthands

    static RefPtr<CSSValue> extractValueAnimationShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueAnimationRangeShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueBackgroundShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueBackgroundPositionShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueBlockStepShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueBorderShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueBorderBlockShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueBorderImageShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueBorderInlineShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueBorderRadiusShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueBorderSpacingShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueColumnsShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueContainerShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueFlexFlowShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueFontShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueFontSynthesisShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueFontVariantShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueGridShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueGridAreaShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueGridColumnShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueGridRowShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueGridTemplateShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueLineClampShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueMaskShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueMaskBorderShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueMaskPositionShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueOffsetShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueOverscrollBehaviorShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValuePageBreakAfterShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValuePageBreakBeforeShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValuePageBreakInsideShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValuePerspectiveOriginShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValuePositionTryShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueScrollTimelineShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueTextBoxShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueTextDecorationSkipShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueTextEmphasisShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueTextWrapShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueTransformOriginShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueTransitionShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueViewTimelineShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueWhiteSpaceShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueWebkitBorderImageShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueWebkitBorderRadiusShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueWebkitColumnBreakAfterShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueWebkitColumnBreakBeforeShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueWebkitColumnBreakInsideShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueWebkitMaskBoxImageShorthand(ExtractorState&);
    static RefPtr<CSSValue> extractValueWebkitMaskPositionShorthand(ExtractorState&);
};

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

template<CSSPropertyID propertyID> Ref<CSSValue> extractZoomAdjustedInsetValue(ExtractorState& state)
{
    auto offsetComputedLength = [&] {
        // If specified as a length, the corresponding absolute length; if specified as
        // a percentage, the specified value; otherwise, 'auto'. Hence, we can just
        // return the value in the style.
        //
        // See http://www.w3.org/TR/CSS21/cascade.html#computed-value

        if constexpr (propertyID == CSSPropertyTop)
            return state.style.top();
        else if constexpr (propertyID == CSSPropertyRight)
            return state.style.right();
        else if constexpr (propertyID == CSSPropertyBottom)
            return state.style.bottom();
        else if constexpr (propertyID == CSSPropertyLeft)
            return state.style.left();
    };

    auto offset = offsetComputedLength();

    // If the element is not displayed; return the "computed value".
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (!box)
        return ExtractorConverter::convertLength(state, offset);

    auto* containingBlock = box->containingBlock();

    // Resolve a "computed value" percentage if the element is positioned.
    if (containingBlock && offset.isPercentOrCalculated() && box->isPositioned()) {
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
        return ExtractorConverter::convertNumberAsPixels(state, floatValueForLength(offset, containingBlockSize));
    }

    // Return a "computed value" length.
    if (!offset.isAuto())
        return ExtractorConverter::convertLength(state, offset);

    auto offsetUsedStyleRelative = [&](RenderBox& box) -> LayoutUnit {
        // For relatively positioned boxes, the offset is with respect to the top edges
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
        return ExtractorConverter::convertNumberAsPixels(state, offsetUsedStyleRelative(*box));

    auto offsetUsedStyleOutOfFlowPositioned = [&](RenderBlock& container, RenderBox& box) {
        // For out-of-flow positioned boxes, the offset is how far an box's margin
        // edge is offset below the edge of the box's containing block.
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
        return ExtractorConverter::convertNumberAsPixels(state, offsetUsedStyleOutOfFlowPositioned(*containingBlock, *box));

    return CSSPrimitiveValue::create(CSSValueAuto);
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

template<auto lengthGetter, auto computedCSSValueGetter> Ref<CSSValue> extractZoomAdjustedMarginValue(ExtractorState& state)
{
    auto* renderBox = dynamicDowncast<RenderBox>(state.renderer);
    if (!renderBox)
        return ExtractorConverter::convertLength(state, (state.style.*lengthGetter)());
    return ExtractorConverter::convertNumberAsPixels(state, (renderBox->*computedCSSValueGetter)());
}

template<auto lengthGetter, auto computedCSSValueGetter> Ref<CSSValue> extractZoomAdjustedPaddingValue(ExtractorState& state)
{
    auto unzoomedLength = (state.style.*lengthGetter)();
    auto* renderBox = dynamicDowncast<RenderBox>(state.renderer);
    if (!renderBox || unzoomedLength.isFixed())
        return ExtractorConverter::convertLength(state, unzoomedLength);
    return ExtractorConverter::convertNumberAsPixels(state, (renderBox->*computedCSSValueGetter)());
}

template<auto lengthGetter, auto boxGetter> Ref<CSSValue> extractZoomAdjustedPreferredSizeValue(ExtractorState& state)
{
    auto sizingBox = [](auto& renderer) -> LayoutRect {
        auto* box = dynamicDowncast<RenderBox>(renderer);
        if (!box)
            return LayoutRect();
        return box->style().boxSizing() == BoxSizing::BorderBox ? box->borderBoxRect() : box->computedCSSContentBoxRect();
    };

    auto isNonReplacedInline = [](auto& renderer) {
        return renderer.isInline() && !renderer.isReplacedOrAtomicInline();
    };

    if (state.renderer && !state.renderer->isRenderOrLegacyRenderSVGModelObject()) {
        // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
        // the "height" property does not apply for non-replaced inline elements.
        if (!isNonReplacedInline(*state.renderer))
            return ExtractorConverter::convertNumberAsPixels(state, (sizingBox(*state.renderer).*boxGetter)());
    }
    return ExtractorConverter::convertLength(state, (state.style.*lengthGetter)());
}

template<auto lengthGetter> Ref<CSSValue> extractZoomAdjustedMaxSizeValue(ExtractorState& state)
{
    auto unzoomedLength = (state.style.*lengthGetter)();
    if (unzoomedLength.isUndefined())
        return CSSPrimitiveValue::create(CSSValueNone);
    return ExtractorConverter::convertLength(state, unzoomedLength);
}

template<auto lengthGetter> Ref<CSSValue> extractZoomAdjustedMinSizeValue(ExtractorState& state)
{
    auto isFlexOrGridItem = [](auto renderer) {
        auto* box = dynamicDowncast<RenderBox>(renderer);
        return box && (box->isFlexItem() || box->isGridItem());
    };

    auto unzoomedLength = (state.style.*lengthGetter)();
    if (unzoomedLength.isAuto()) {
        if (isFlexOrGridItem(state.renderer))
            return CSSPrimitiveValue::create(CSSValueAuto);
        return ExtractorConverter::convertNumberAsPixels(state, 0);
    }
    return ExtractorConverter::convertLength(state, unzoomedLength);
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

template<GridTrackSizingDirection direction> static Ref<CSSValue> extractGridTemplateValue(ExtractorState& state)
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

// MARK: Shorthand Utilities

inline Ref<CSSValue> extractSingleShorthand(ExtractorState& state, const StylePropertyShorthand& shorthand)
{
    ASSERT(shorthand.length() == 1);
    return ExtractorGenerated::extractValue(state, *shorthand.begin()).releaseNonNull();
}

inline Ref<CSSValueList> extractStandardShorthand(ExtractorState& state, const StylePropertyShorthand& shorthand)
{
    CSSValueListBuilder list;
    for (auto longhand : shorthand)
        list.append(ExtractorGenerated::extractValue(state, longhand).releaseNonNull());
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline RefPtr<CSSValueList> extract2ValueShorthand(ExtractorState& state, const StylePropertyShorthand& shorthand)
{
    // Assume the properties are in the usual order start, end.
    auto longhands = shorthand.properties();
    auto startValue = ExtractorGenerated::extractValue(state, longhands[0]);
    auto endValue = ExtractorGenerated::extractValue(state, longhands[1]);

    // All 2 properties must be specified.
    if (!startValue || !endValue)
        return nullptr;

    if (compareCSSValuePtr(startValue, endValue))
        return CSSValueList::createSpaceSeparated(startValue.releaseNonNull());
    return CSSValueList::createSpaceSeparated(startValue.releaseNonNull(), endValue.releaseNonNull());
}

inline RefPtr<CSSValueList> extract4ValueShorthand(ExtractorState& state, const StylePropertyShorthand& shorthand)
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

inline Ref<CSSValue> extractGridShorthand(ExtractorState& state, const StylePropertyShorthand& shorthand)
{
    CSSValueListBuilder builder;
    for (auto longhand : shorthand)
        builder.append(ExtractorGenerated::extractValue(state, longhand).releaseNonNull());
    return CSSValueList::createSlashSeparated(WTFMove(builder));
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
        auto x = ExtractorConverter::convertLength(state, radius.width);
        auto y = radius.width == radius.height ? x.copyRef() : ExtractorConverter::convertLength(state, radius.height);
        return std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>> { WTFMove(x), WTFMove(y) };
    };

    bool showHorizontalBottomLeft = state.style.borderTopRightRadius().width != state.style.borderBottomLeftRadius().width;
    bool showHorizontalBottomRight = showHorizontalBottomLeft || (state.style.borderBottomRightRadius().width != state.style.borderTopLeftRadius().width);
    bool showHorizontalTopRight = showHorizontalBottomRight || (state.style.borderTopRightRadius().width != state.style.borderTopLeftRadius().width);

    bool showVerticalBottomLeft = state.style.borderTopRightRadius().height != state.style.borderBottomLeftRadius().height;
    bool showVerticalBottomRight = showVerticalBottomLeft || (state.style.borderBottomRightRadius().height != state.style.borderTopLeftRadius().height);
    bool showVerticalTopRight = showVerticalBottomRight || (state.style.borderTopRightRadius().height != state.style.borderTopLeftRadius().height);

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
    auto before = extractStandardShorthand(state, propertiesBeforeSlashSeparator);
    auto after = extractStandardShorthand(state, propertiesAfterSlashSeparator);

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


// MARK: - Custom Extractors

inline Ref<CSSValue> ExtractorCustom::extractValueAspectRatio(ExtractorState& state)
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
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueAuto);
}

inline Ref<CSSValue> ExtractorCustom::extractValueDirection(ExtractorState& state)
{
    auto direction = [&] {
        if (state.element.ptr() == state.element->document().documentElement() && !state.style.hasExplicitlySetDirection())
            return RenderStyle::initialDirection();
        return state.style.writingMode().computedTextDirection();
    }();
    return ExtractorConverter::convert(state, direction);
}

inline Ref<CSSValue> ExtractorCustom::extractValueWritingMode(ExtractorState& state)
{
    auto writingMode = [&] {
        if (state.element.ptr() == state.element->document().documentElement() && !state.style.hasExplicitlySetWritingMode())
            return RenderStyle::initialWritingMode();
        return state.style.writingMode().computedWritingMode();
    }();
    return ExtractorConverter::convert(state, writingMode);
}

inline Ref<CSSValue> ExtractorCustom::extractValueFill(ExtractorState& state)
{
    return ExtractorConverter::convertSVGPaint(state, state.style.svgStyle().fillPaintType(), state.style.svgStyle().fillPaintUri(), state.style.svgStyle().fillPaintColor());
}

inline Ref<CSSValue> ExtractorCustom::extractValueStroke(ExtractorState& state)
{
    return ExtractorConverter::convertSVGPaint(state, state.style.svgStyle().strokePaintType(), state.style.svgStyle().strokePaintUri(), state.style.svgStyle().strokePaintColor());
}

inline Ref<CSSValue> ExtractorCustom::extractValueFloat(ExtractorState& state)
{
    if (state.style.hasOutOfFlowPosition())
        return CSSPrimitiveValue::create(CSSValueNone);
    return ExtractorConverter::convert(state, state.style.floating());
}

inline Ref<CSSValue> ExtractorCustom::extractValueClip(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueContent(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueCursor(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueBaselineShift(ExtractorState& state)
{
    switch (state.style.svgStyle().baselineShift()) {
    case BaselineShift::Baseline:
        return CSSPrimitiveValue::create(CSSValueBaseline);
    case BaselineShift::Super:
        return CSSPrimitiveValue::create(CSSValueSuper);
    case BaselineShift::Sub:
        return CSSPrimitiveValue::create(CSSValueSub);
    case BaselineShift::Length:
        return ExtractorConverter::convertSVGLengthUsingElement(state, state.style.svgStyle().baselineShiftValue());
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueBaseline);
}

inline Ref<CSSValue> ExtractorCustom::extractValueVerticalAlign(ExtractorState& state)
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
        return CSSPrimitiveValue::create(state.style.verticalAlignLength(), state.style);
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueBottom);
}

inline Ref<CSSValue> ExtractorCustom::extractValueTextEmphasisStyle(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueTextIndent(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueLetterSpacing(ExtractorState& state)
{
    auto& spacing = state.style.computedLetterSpacing();
    if (spacing.isFixed()) {
        if (spacing.isZero())
            return CSSPrimitiveValue::create(CSSValueNormal);
        return ExtractorConverter::convertNumberAsPixels(state, spacing.value());
    }
    return CSSPrimitiveValue::create(spacing, state.style);
}

inline Ref<CSSValue> ExtractorCustom::extractValueWordSpacing(ExtractorState& state)
{
    auto& spacing = state.style.computedWordSpacing();
    if (spacing.isFixed())
        return ExtractorConverter::convertNumberAsPixels(state, spacing.value());
    return CSSPrimitiveValue::create(spacing, state.style);
}

inline Ref<CSSValue> ExtractorCustom::extractValueLineHeight(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueFontFamily(ExtractorState& state)
{
    if (state.style.fontCascade().familyCount() == 1)
        return ExtractorConverter::convertFontFamily(state, state.style.fontCascade().familyAt(0));

    CSSValueListBuilder list;
    for (unsigned i = 0; i < state.style.fontCascade().familyCount(); ++i)
        list.append(ExtractorConverter::convertFontFamily(state, state.style.fontCascade().familyAt(i)));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline Ref<CSSValue> ExtractorCustom::extractValueFontSize(ExtractorState& state)
{
    return ExtractorConverter::convertNumberAsPixels(state, state.style.fontDescription().computedSize());
}

inline Ref<CSSValue> ExtractorCustom::extractValueFontStyle(ExtractorState& state)
{
    auto italic = state.style.fontDescription().italic();
    if (auto keyword = fontStyleKeyword(italic, state.style.fontDescription().fontStyleAxis()))
        return CSSPrimitiveValue::create(*keyword);
    return CSSFontStyleWithAngleValue::create({ CSS::AngleUnit::Deg, static_cast<float>(*italic) });
}

inline Ref<CSSValue> ExtractorCustom::extractValueFontVariantLigatures(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueFontVariantNumeric(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueFontVariantAlternates(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueFontVariantEastAsian(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueTop(ExtractorState& state)
{
    return extractZoomAdjustedInsetValue<CSSPropertyTop>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueRight(ExtractorState& state)
{
    return extractZoomAdjustedInsetValue<CSSPropertyRight>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueBottom(ExtractorState& state)
{
    return extractZoomAdjustedInsetValue<CSSPropertyBottom>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueLeft(ExtractorState& state)
{
    return extractZoomAdjustedInsetValue<CSSPropertyLeft>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueMarginTop(ExtractorState& state)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::BlockStart) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Top)))
        return ExtractorConverter::convertNumberAsPixels(state, box->marginTop());
    return extractZoomAdjustedMarginValue<&RenderStyle::marginTop, &RenderBoxModelObject::marginTop>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueMarginRight(ExtractorState& state)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::InlineEnd) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Right)))
        return ExtractorConverter::convertNumberAsPixels(state, box->marginRight());

    auto& marginRight = state.style.marginRight();
    if (marginRight.isFixed() || !box)
        return ExtractorConverter::convertLength(state, marginRight);

    float value;
    if (marginRight.isPercentOrCalculated()) {
        // RenderBox gives a marginRight() that is the distance between the right-edge of the child box
        // and the right-edge of the containing box, when display == DisplayType::Block. Let's calculate the absolute
        // value of the specified margin-right % instead of relying on RenderBox's marginRight() value.
        value = minimumValueForLength(marginRight, box->containingBlockLogicalWidthForContent());
    } else
        value = box->marginRight();
    return ExtractorConverter::convertNumberAsPixels(state, value);
}

inline Ref<CSSValue> ExtractorCustom::extractValueMarginBottom(ExtractorState& state)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::BlockEnd) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Bottom)))
        return ExtractorConverter::convertNumberAsPixels(state, box->marginBottom());
    return extractZoomAdjustedMarginValue<&RenderStyle::marginBottom, &RenderBoxModelObject::marginBottom>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueMarginLeft(ExtractorState& state)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::InlineStart) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Left)))
        return ExtractorConverter::convertNumberAsPixels(state, box->marginLeft());
    return extractZoomAdjustedMarginValue<&RenderStyle::marginLeft, &RenderBoxModelObject::marginLeft>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValuePaddingTop(ExtractorState& state)
{
    return extractZoomAdjustedPaddingValue<&RenderStyle::paddingTop, &RenderBoxModelObject::computedCSSPaddingTop>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValuePaddingRight(ExtractorState& state)
{
    return extractZoomAdjustedPaddingValue<&RenderStyle::paddingRight, &RenderBoxModelObject::computedCSSPaddingRight>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValuePaddingBottom(ExtractorState& state)
{
    return extractZoomAdjustedPaddingValue<&RenderStyle::paddingBottom, &RenderBoxModelObject::computedCSSPaddingBottom>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValuePaddingLeft(ExtractorState& state)
{
    return extractZoomAdjustedPaddingValue<&RenderStyle::paddingLeft, &RenderBoxModelObject::computedCSSPaddingLeft>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueHeight(ExtractorState& state)
{
    return extractZoomAdjustedPreferredSizeValue<&RenderStyle::height, &LayoutRect::height>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueWidth(ExtractorState& state)
{
    return extractZoomAdjustedPreferredSizeValue<&RenderStyle::width, &LayoutRect::width>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueMaxHeight(ExtractorState& state)
{
    return extractZoomAdjustedMaxSizeValue<&RenderStyle::maxHeight>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueMaxWidth(ExtractorState& state)
{
    return extractZoomAdjustedMaxSizeValue<&RenderStyle::maxWidth>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueMinHeight(ExtractorState& state)
{
    return extractZoomAdjustedMinSizeValue<&RenderStyle::minHeight>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueMinWidth(ExtractorState& state)
{
    return extractZoomAdjustedMinSizeValue<&RenderStyle::minWidth>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueCounterIncrement(ExtractorState& state)
{
    return extractCounterValue<CSSPropertyCounterIncrement>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueCounterReset(ExtractorState& state)
{
    return extractCounterValue<CSSPropertyCounterReset>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueCounterSet(ExtractorState& state)
{
    return extractCounterValue<CSSPropertyCounterSet>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueContainIntrinsicHeight(ExtractorState& state)
{
    return ExtractorConverter::convertContainIntrinsicSize(state, state.style.containIntrinsicHeightType(), state.style.containIntrinsicHeight());
}

inline Ref<CSSValue> ExtractorCustom::extractValueContainIntrinsicWidth(ExtractorState& state)
{
    return ExtractorConverter::convertContainIntrinsicSize(state, state.style.containIntrinsicWidthType(), state.style.containIntrinsicWidth());
}

inline Ref<CSSValue> ExtractorCustom::extractValueBorderImageOutset(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageQuad(state, state.style.borderImage().outset());
}

inline Ref<CSSValue> ExtractorCustom::extractValueBorderImageRepeat(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageRepeat(state, state.style.borderImage());
}

inline Ref<CSSValue> ExtractorCustom::extractValueBorderImageSlice(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageSlices(state, state.style.borderImage());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueBorderImageWidth(ExtractorState& state)
{
    auto& borderImage = state.style.borderImage();
    if (borderImage.overridesBorderWidths())
        return nullptr;
    return ExtractorConverter::convertNinePieceImageQuad(state, borderImage.borderSlices());
}

inline Ref<CSSValue> ExtractorCustom::extractValueMaskBorderOutset(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageQuad(state, state.style.maskBorder().outset());
}

inline Ref<CSSValue> ExtractorCustom::extractValueMaskBorderRepeat(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageRepeat(state, state.style.maskBorder());
}

inline Ref<CSSValue> ExtractorCustom::extractValueMaskBorderSlice(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageSlices(state, state.style.maskBorder());
}

inline Ref<CSSValue> ExtractorCustom::extractValueMaskBorderWidth(ExtractorState& state)
{
    return ExtractorConverter::convertNinePieceImageQuad(state, state.style.maskBorder().borderSlices());
}

inline Ref<CSSValue> ExtractorCustom::extractValueTransform(ExtractorState& state)
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
    for (auto& operation : state.style.transform()) {
        if (auto functionValue = ExtractorConverter::convertTransformOperation(state, operation))
            list.append(functionValue.releaseNonNull());
    }
    if (!list.isEmpty())
        return CSSTransformListValue::create(WTFMove(list));

    return CSSPrimitiveValue::create(CSSValueNone);
}

inline Ref<CSSValue> ExtractorCustom::extractValueTranslate(ExtractorState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-translate
    // Computed value: the keyword none or a pair of computed <length-percentage> values and an absolute length

    auto* translate = state.style.translate();
    if (!translate || is<RenderInline>(state.renderer))
        return CSSPrimitiveValue::create(CSSValueNone);

    auto includeAxis = [](const auto& length) {
        return !length.isZero() || length.isPercent();
    };

    if (includeAxis(translate->z()))
        return CSSValueList::createSpaceSeparated(ExtractorConverter::convertLength(state, translate->x()), ExtractorConverter::convertLength(state, translate->y()), ExtractorConverter::convertLength(state, translate->z()));
    if (includeAxis(translate->y()))
        return CSSValueList::createSpaceSeparated(ExtractorConverter::convertLength(state, translate->x()), ExtractorConverter::convertLength(state, translate->y()));
    if (!translate->x().isUndefined() && !translate->x().isEmptyValue())
        return CSSValueList::createSpaceSeparated(ExtractorConverter::convertLength(state, translate->x()));

    return CSSPrimitiveValue::create(CSSValueNone);
}

inline Ref<CSSValue> ExtractorCustom::extractValueScale(ExtractorState& state)
{
    auto* scale = state.style.scale();
    if (!scale || is<RenderInline>(state.renderer))
        return CSSPrimitiveValue::create(CSSValueNone);

    if (scale->z() != 1)
        return CSSValueList::createSpaceSeparated(ExtractorConverter::convert(state, scale->x()), ExtractorConverter::convert(state, scale->y()), ExtractorConverter::convert(state, scale->z()));
    if (scale->x() != scale->y())
        return CSSValueList::createSpaceSeparated(ExtractorConverter::convert(state, scale->x()), ExtractorConverter::convert(state, scale->y()));
    return CSSValueList::createSpaceSeparated(ExtractorConverter::convert(state, scale->x()));
}

inline Ref<CSSValue> ExtractorCustom::extractValueRotate(ExtractorState& state)
{
    auto* rotate = state.style.rotate();
    if (!rotate || is<RenderInline>(state.renderer))
        return CSSPrimitiveValue::create(CSSValueNone);

    auto angle = CSSPrimitiveValue::create(rotate->angle(), CSSUnitType::CSS_DEG);
    if (!rotate->is3DOperation() || (!rotate->x() && !rotate->y() && rotate->z()))
        return angle;
    if (rotate->x() && !rotate->y() && !rotate->z())
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueX), WTFMove(angle));
    if (!rotate->x() && rotate->y() && !rotate->z())
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueY), WTFMove(angle));
    return CSSValueList::createSpaceSeparated(
        CSSPrimitiveValue::create(rotate->x()),
        CSSPrimitiveValue::create(rotate->y()),
        CSSPrimitiveValue::create(rotate->z()),
        WTFMove(angle)
    );
}

inline Ref<CSSValue> ExtractorCustom::extractValuePerspective(ExtractorState& state)
{
    if (!state.style.hasPerspective())
        return CSSPrimitiveValue::create(CSSValueNone);
    return ExtractorConverter::convertNumberAsPixels(state, state.style.perspective());
}

inline Ref<CSSValue> ExtractorCustom::extractValueGridAutoFlow(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueGridTemplateAreas(ExtractorState& state)
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

inline Ref<CSSValue> ExtractorCustom::extractValueGridTemplateColumns(ExtractorState& state)
{
    return extractGridTemplateValue<GridTrackSizingDirection::ForColumns>(state);
}

inline Ref<CSSValue> ExtractorCustom::extractValueGridTemplateRows(ExtractorState& state)
{
    return extractGridTemplateValue<GridTrackSizingDirection::ForRows>(state);
}

// MARK: - Shorthands

inline RefPtr<CSSValue> ExtractorCustom::extractValueAnimationShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueAnimationRangeShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueBackgroundShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueBackgroundPositionShorthand(ExtractorState& state)
{
    auto mapper = [](auto& state, auto& layer) -> Ref<CSSValue> {
        return CSSValueList::createSpaceSeparated(
            ExtractorConverter::convertLength(state, layer.xPosition()),
            ExtractorConverter::convertLength(state, layer.yPosition())
        );
    };
    return extractFillLayerValue(state, state.style.backgroundLayers(), mapper);
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueBlockStepShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueBorderShorthand(ExtractorState& state)
{
    static constexpr std::array properties { CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };

    auto value = ExtractorGenerated::extractValue(state, CSSPropertyBorderTop);
    for (auto& property : properties) {
        if (!compareCSSValuePtr<CSSValue>(value, ExtractorGenerated::extractValue(state, property)))
            return nullptr;
    }
    return value;
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueBorderBlockShorthand(ExtractorState& state)
{
    auto start = ExtractorGenerated::extractValue(state, CSSPropertyBorderBlockStart);
    auto end = ExtractorGenerated::extractValue(state, CSSPropertyBorderBlockEnd);
    if (!compareCSSValuePtr<CSSValue>(start, end))
        return nullptr;
    return start;
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueBorderImageShorthand(ExtractorState& state)
{
    auto& borderImage = state.style.borderImage();
    if (!borderImage.image())
        return CSSPrimitiveValue::create(CSSValueNone);
    if (borderImage.overridesBorderWidths())
        return nullptr;
    return ExtractorConverter::convertNinePieceImage(state, borderImage);
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueBorderInlineShorthand(ExtractorState& state)
{
    auto start = ExtractorGenerated::extractValue(state, CSSPropertyBorderInlineStart);
    auto end = ExtractorGenerated::extractValue(state, CSSPropertyBorderInlineEnd);
    if (!compareCSSValuePtr<CSSValue>(start, end))
        return nullptr;
    return start;
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueBorderRadiusShorthand(ExtractorState& state)
{
    return extractBorderRadiusShorthand(state, CSSPropertyBorderRadius);
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueBorderSpacingShorthand(ExtractorState& state)
{
    return CSSValuePair::create(
        ExtractorConverter::convertNumberAsPixels(state, state.style.horizontalBorderSpacing()),
        ExtractorConverter::convertNumberAsPixels(state, state.style.verticalBorderSpacing())
    );
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueColumnsShorthand(ExtractorState& state)
{
    if (state.style.hasAutoColumnCount())
        return state.style.hasAutoColumnWidth() ? CSSPrimitiveValue::create(CSSValueAuto) : ExtractorConverter::convertNumberAsPixels(state, state.style.columnWidth());
    if (state.style.hasAutoColumnWidth())
        return state.style.hasAutoColumnCount() ? CSSPrimitiveValue::create(CSSValueAuto) : CSSPrimitiveValue::create(state.style.columnCount());
    return extractStandardShorthand(state, columnsShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueContainerShorthand(ExtractorState& state)
{
    auto name = [&]() -> Ref<CSSValue> {
        if (state.style.containerNames().isEmpty())
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueFlexFlowShorthand(ExtractorState& state)
{
    if (state.style.flexWrap() == RenderStyle::initialFlexWrap())
        return ExtractorConverter::convert(state, state.style.flexDirection());
    if (state.style.flexDirection() == RenderStyle::initialFlexDirection())
        return ExtractorConverter::convert(state, state.style.flexWrap());
    return extractStandardShorthand(state, flexFlowShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueFontShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueFontSynthesisShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueFontVariantShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueGridShorthand(ExtractorState& state)
{
    return extractGridShorthand(state, gridShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueGridAreaShorthand(ExtractorState& state)
{
    return extractGridShorthand(state, gridAreaShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueGridColumnShorthand(ExtractorState& state)
{
    return extractGridShorthand(state, gridColumnShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueGridRowShorthand(ExtractorState& state)
{
    return extractGridShorthand(state, gridRowShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueGridTemplateShorthand(ExtractorState& state)
{
    return extractGridShorthand(state, gridTemplateShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueLineClampShorthand(ExtractorState& state)
{
    auto maxLines = state.style.maxLines();
    if (!maxLines)
        return CSSPrimitiveValue::create(CSSValueNone);

    Ref maxLineCount = CSSPrimitiveValue::create(maxLines, CSSUnitType::CSS_INTEGER);
    auto blockEllipsisType = state.style.blockEllipsis().type;

    if (blockEllipsisType == BlockEllipsis::Type::None)
        return CSSValuePair::create(WTFMove(maxLineCount), CSSPrimitiveValue::create(CSSValueNone));

    if (blockEllipsisType == BlockEllipsis::Type::Auto)
        return CSSValuePair::create(WTFMove(maxLineCount), CSSPrimitiveValue::create(CSSValueAuto));

    if (blockEllipsisType == BlockEllipsis::Type::String)
        return CSSValuePair::create(WTFMove(maxLineCount), CSSPrimitiveValue::createCustomIdent(state.style.blockEllipsis().string));

    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueNone);
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueMaskShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueMaskBorderShorthand(ExtractorState& state)
{
    auto& maskBorder = state.style.maskBorder();
    if (!maskBorder.image())
        return CSSPrimitiveValue::create(CSSValueNone);
    if (maskBorder.overridesBorderWidths())
        return nullptr;
    return ExtractorConverter::convertNinePieceImage(state, maskBorder);
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueMaskPositionShorthand(ExtractorState& state)
{
    auto mapper = [](auto& state, auto& layer) -> Ref<CSSValue> {
        return CSSValueList::createSpaceSeparated(
            ExtractorConverter::convertLength(state, layer.xPosition()),
            ExtractorConverter::convertLength(state, layer.yPosition())
        );
    };
    return extractFillLayerValue(state, state.style.maskLayers(), mapper);
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueOffsetShorthand(ExtractorState& state)
{
    // [ <'offset-position'>? [ <'offset-path'> [ <'offset-distance'> || <'offset-rotate'> ]? ]? ]! [ / <'offset-anchor'> ]?

    // The first four elements are serialized in a space separated CSSValueList.
    // This is then combined with offset-anchor in a slash separated CSSValueList.

    auto isAuto = [](const auto& position) { return position.x.isAuto() && position.y.isAuto(); };
    auto isNormal = [](const auto& position) { return position.x.isNormal(); };

    CSSValueListBuilder innerList;

    if (!isAuto(state.style.offsetPosition()) && !isNormal(state.style.offsetPosition()))
        innerList.append(ExtractorConverter::convertPosition(state, state.style.offsetPosition()));

    bool nonInitialDistance = !state.style.offsetDistance().isZero();
    bool nonInitialRotate = state.style.offsetRotate() != state.style.initialOffsetRotate();

    if (state.style.offsetPath() || nonInitialDistance || nonInitialRotate)
        innerList.append(ExtractorConverter::convertPathOperation(state, state.style.offsetPath(), PathConversion::ForceAbsolute));

    if (nonInitialDistance)
        innerList.append(CSSPrimitiveValue::create(state.style.offsetDistance(), state.style));
    if (nonInitialRotate)
        innerList.append(ExtractorConverter::convertOffsetRotate(state, state.style.offsetRotate()));

    auto inner = innerList.isEmpty()
        ? Ref<CSSValue> { CSSPrimitiveValue::create(CSSValueAuto) }
        : Ref<CSSValue> { CSSValueList::createSpaceSeparated(WTFMove(innerList)) };

    if (isAuto(state.style.offsetAnchor()))
        return inner;

    return CSSValueList::createSlashSeparated(WTFMove(inner), ExtractorConverter::convertPosition(state, state.style.offsetAnchor()));
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueOverscrollBehaviorShorthand(ExtractorState& state)
{
    return ExtractorConverter::convert(state, std::max(state.style.overscrollBehaviorX(), state.style.overscrollBehaviorY()));
}

inline RefPtr<CSSValue> ExtractorCustom::extractValuePageBreakAfterShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertPageBreak(state, state.style.breakAfter());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValuePageBreakBeforeShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertPageBreak(state, state.style.breakBefore());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValuePageBreakInsideShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertPageBreak(state, state.style.breakInside());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValuePerspectiveOriginShorthand(ExtractorState& state)
{
    if (state.renderer) {
        auto box = state.renderer->transformReferenceBoxRect(state.style);
        return CSSValueList::createSpaceSeparated(
            ExtractorConverter::convertNumberAsPixels(state, minimumValueForLength(state.style.perspectiveOriginX(), box.width())),
            ExtractorConverter::convertNumberAsPixels(state, minimumValueForLength(state.style.perspectiveOriginY(), box.height()))
        );
    }
    return CSSValueList::createSpaceSeparated(
        ExtractorConverter::convertLength(state, state.style.perspectiveOriginX()),
        ExtractorConverter::convertLength(state, state.style.perspectiveOriginY())
    );
}

inline RefPtr<CSSValue> ExtractorCustom::extractValuePositionTryShorthand(ExtractorState& state)
{
    if (state.style.positionTryOrder() == RenderStyle::initialPositionTryOrder())
        return ExtractorGenerated::extractValue(state, CSSPropertyPositionTryFallbacks);
    return extractStandardShorthand(state, positionTryShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueScrollTimelineShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueTextBoxShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueTextDecorationSkipShorthand(ExtractorState& state)
{
    switch (state.style.textDecorationSkipInk()) {
    case TextDecorationSkipInk::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case TextDecorationSkipInk::Auto:
        return CSSPrimitiveValue::create(CSSValueAuto);
    case TextDecorationSkipInk::All:
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueInitial);
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueTextEmphasisShorthand(ExtractorState& state)
{
    return CSSValueList::createSpaceSeparated(
        extractValueTextEmphasisStyle(state),
        ExtractorConverter::convertColor(state, state.style.textEmphasisColor())
    );
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueTextWrapShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueTransformOriginShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueTransitionShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueViewTimelineShorthand(ExtractorState& state)
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
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, ExtractorConverter::convertSingleViewTimelineInsets(state, insets)));
        else if (hasDefaultInsets)
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, ExtractorConverter::convert(state, axis)));
        else {
            list.append(CSSValueList::createSpaceSeparated(
                WTFMove(nameCSSValue),
                ExtractorConverter::convert(state, axis),
                ExtractorConverter::convertSingleViewTimelineInsets(state, insets)
            ));
        }
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueWhiteSpaceShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueWebkitBorderImageShorthand(ExtractorState& state)
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

inline RefPtr<CSSValue> ExtractorCustom::extractValueWebkitBorderRadiusShorthand(ExtractorState& state)
{
    return extractBorderRadiusShorthand(state, CSSPropertyWebkitBorderRadius);
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueWebkitColumnBreakAfterShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertWebkitColumnBreak(state, state.style.breakAfter());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueWebkitColumnBreakBeforeShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertWebkitColumnBreak(state, state.style.breakBefore());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueWebkitColumnBreakInsideShorthand(ExtractorState& state)
{
    return ExtractorConverter::convertWebkitColumnBreak(state, state.style.breakInside());
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueWebkitMaskBoxImageShorthand(ExtractorState& state)
{
    return ExtractorGenerated::extractValue(state, CSSPropertyMaskBorder);
}

inline RefPtr<CSSValue> ExtractorCustom::extractValueWebkitMaskPositionShorthand(ExtractorState& state)
{
    return ExtractorGenerated::extractValue(state, CSSPropertyMaskPosition);
}

} // namespace Style
} // namespace WebCore
