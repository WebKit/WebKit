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

#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSPropertyNames.h"
#include "RenderElementStyleInlines.h"
#include "StyleExtractorConverter.h"
#include "StyleExtractorSerializer.h"
#include "StyleInterpolation.h"
#include "StyleOrderedNamedLinesCollector.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"

namespace WebCore {
namespace Style {

// Custom handling of computed value extraction.
class ExtractorCustom {
public:
    static Ref<CSSValue> extractDirection(ExtractorState&);
    static Ref<CSSValue> extractWritingMode(ExtractorState&);
    static Ref<CSSValue> extractFloat(ExtractorState&);
    static Ref<CSSValue> extractContent(ExtractorState&);
    static Ref<CSSValue> extractLetterSpacing(ExtractorState&);
    static Ref<CSSValue> extractWordSpacing(ExtractorState&);
    static Ref<CSSValue> extractLineHeight(ExtractorState&);
    static Ref<CSSValue> extractFontFamily(ExtractorState&);
    static Ref<CSSValue> extractFontSize(ExtractorState&);
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
    static Ref<CSSValue> extractTransform(ExtractorState&);
    static RefPtr<CSSValue> extractBorderImageWidth(ExtractorState&);
    static Ref<CSSValue> extractTranslate(ExtractorState&);
    static Ref<CSSValue> extractScale(ExtractorState&);
    static Ref<CSSValue> extractRotate(ExtractorState&);
    static Ref<CSSValue> extractGridAutoFlow(ExtractorState&);
    static Ref<CSSValue> extractGridTemplateColumns(ExtractorState&);
    static Ref<CSSValue> extractGridTemplateRows(ExtractorState&);
    static Ref<CSSValue> extractAnimationDuration(ExtractorState&);
    static Ref<CSSValue> extractWidows(ExtractorState&);
    static Ref<CSSValue> extractOrphans(ExtractorState&);
    static Ref<CSSValue> extractWebkitTextCombine(ExtractorState&);
    static Ref<CSSValue> extractWebkitRubyPosition(ExtractorState&);
    static Ref<CSSValue> extractWebkitMaskComposite(ExtractorState&);
    static Ref<CSSValue> extractWebkitMaskSourceType(ExtractorState&);

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
    static RefPtr<CSSValue> extractTextDecorationShorthand(ExtractorState&);
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
    static RefPtr<CSSValue> extractMarkerShorthand(ExtractorState&);

    // MARK: Custom Serialization

    static void extractDirectionSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWritingModeSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFloatSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractContentSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractLetterSpacingSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWordSpacingSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractLineHeightSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontFamilySerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractFontSizeSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
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
    static void extractBorderImageWidthSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTransformSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractTranslateSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractScaleSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractRotateSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractGridAutoFlowSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractGridTemplateColumnsSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractGridTemplateRowsSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractAnimationDurationSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWidowsSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractOrphansSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitTextCombineSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitRubyPositionSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitMaskCompositeSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
    static void extractWebkitMaskSourceTypeSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);

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
    static void extractTextDecorationShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
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
    static void extractMarkerShorthandSerialization(ExtractorState&, StringBuilder&, const CSS::SerializationContext&);
};

// MARK: - Shared Adaptor

// Shared adaptors are used by adaptors to further adapt a value that has been partially extracted from a RenderStyle. Like adaptors, they use a provided functor to allow them to be used for both CSSValue creation and serialization.

template<CSSPropertyID propertyID> struct InsetEdgeSharedAdaptor {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, const InsetEdge& value, F&& functor) const
    {
        // If the element is not displayed; return the "computed value".
        CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
        if (!box)
            return functor(value);

        auto* containingBlock = dynamicDowncast<RenderBoxModelObject>(box->container());

        // Resolve a "computed value" percentage if the element is positioned.
        if (containingBlock && value.isPercentOrCalculated() && box->isPositioned()) {
            constexpr bool isVerticalProperty = (propertyID == CSSPropertyTop || propertyID == CSSPropertyBottom);

            LayoutUnit containingBlockSize;
            if (box->isStickilyPositioned()) {
                auto& enclosingClippingBox = box->enclosingClippingBoxForStickyPosition().first;
                if (isVerticalProperty == enclosingClippingBox.isHorizontalWritingMode())
                    containingBlockSize = enclosingClippingBox.contentBoxLogicalHeight();
                else
                    containingBlockSize = enclosingClippingBox.contentBoxLogicalWidth();
            } else {
                if (box->isOutOfFlowPositioned()) {
                    if (isVerticalProperty)
                        containingBlockSize = box->containingBlockRangeForPositioned(*containingBlock, BoxAxis::Vertical).size();
                    else
                        containingBlockSize = box->containingBlockRangeForPositioned(*containingBlock, BoxAxis::Horizontal).size();
                } else {
                    if (isVerticalProperty == containingBlock->isHorizontalWritingMode())
                        containingBlockSize = box->containingBlockLogicalHeightForContent(AvailableLogicalHeightType::ExcludeMarginBorderPadding);
                    else
                        containingBlockSize = box->containingBlockLogicalWidthForContent();
                }
            }
            return functor(Length<> { evaluate<LayoutUnit>(value, containingBlockSize, containingBlock->style().usedZoomForLength()) });
        }

        // Return a "computed value" length.
        if (!value.isAuto())
            return functor(value);

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
            return functor(Length<> { insetUsedStyleRelative(*box) });

        auto insetUsedStyleOutOfFlowPositioned = [&](auto& container, auto& box) {
            // For out-of-flow positioned boxes, the inset is how far an box's margin
            // edge is inset below the edge of the box's containing block.
            // See http://www.w3.org/TR/CSS2/visuren.html#position-props
            //
            // Margins are included in offsetTop/offsetLeft so we need to remove them here.
            auto paddingBoxWidth = [&]() -> LayoutUnit {
                if (CheckedPtr renderBlock = dynamicDowncast<RenderBlock>(container))
                    return renderBlock->paddingBoxWidth();
                if (CheckedPtr inlineBox = dynamicDowncast<RenderInline>(container))
                    return inlineBox->innerPaddingBoxWidth();
                ASSERT_NOT_REACHED();
                return { };
            };
            auto paddingBoxHeight = [&]() -> LayoutUnit {
                if (CheckedPtr renderBlock = dynamicDowncast<RenderBlock>(container))
                    return renderBlock->paddingBoxHeight();
                if (CheckedPtr inlineBox = dynamicDowncast<RenderInline>(container))
                    return inlineBox->innerPaddingBoxHeight();
                ASSERT_NOT_REACHED();
                return { };
            };
            if constexpr (propertyID == CSSPropertyTop)
                return box.offsetTop() - box.marginTop();
            else if constexpr (propertyID == CSSPropertyRight)
                return paddingBoxWidth() - (box.offsetLeft() + box.offsetWidth()) - box.marginRight();
            else if constexpr (propertyID == CSSPropertyBottom)
                return paddingBoxHeight() - (box.offsetTop() + box.offsetHeight()) - box.marginBottom();
            else if constexpr (propertyID == CSSPropertyLeft)
                return box.offsetLeft() - box.marginLeft();
        };

        if (containingBlock && box->isOutOfFlowPositioned())
            return functor(Length<> { insetUsedStyleOutOfFlowPositioned(*containingBlock, *box) });

        return functor(CSS::Keyword::Auto { });
    }
};

template<CSSPropertyID propertyID> struct MarginEdgeSharedAdaptor {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, const MarginEdge& value, F&& functor) const
    {
        auto rendererCanHaveTrimmedMargin = [](const RenderBox& renderer) {
            auto marginTrimSide = [] -> Style::MarginTrimSide {
                if constexpr (propertyID == CSSPropertyMarginTop)
                    return Style::MarginTrimSide::BlockStart;
                else if constexpr (propertyID == CSSPropertyMarginRight)
                    return Style::MarginTrimSide::InlineEnd;
                else if constexpr (propertyID == CSSPropertyMarginBottom)
                    return Style::MarginTrimSide::BlockEnd;
                else if constexpr (propertyID == CSSPropertyMarginLeft)
                    return Style::MarginTrimSide::InlineStart;
            };

            // A renderer will have a specific margin marked as trimmed by setting its rare data bit if:
            // 1.) The layout system the box is in has this logic (setting the rare data bit for this
            // specific margin) implemented
            // 2.) The block container/flexbox/grid has this margin specified in its margin-trim style
            // If marginTrimSide is empty we will check if any of the supported margins are in the style
            if (renderer.isFlexItem() || renderer.isGridItem())
                return renderer.parent()->style().marginTrim().contains(marginTrimSide());

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
        };

        auto toMarginTrimSide = [](const RenderBox& renderer) -> Style::MarginTrimSide {
            auto formattingContextRootStyle = [](const RenderBox& renderer) -> const RenderStyle& {
                if (auto* ancestorToUse = (renderer.isFlexItem() || renderer.isGridItem()) ? renderer.parent() : renderer.containingBlock())
                    return ancestorToUse->style();
                ASSERT_NOT_REACHED();
                return renderer.style();
            };

            auto boxSide = [] -> BoxSide {
                if constexpr (propertyID == CSSPropertyMarginTop)
                    return BoxSide::Top;
                else if constexpr (propertyID == CSSPropertyMarginRight)
                    return BoxSide::Right;
                else if constexpr (propertyID == CSSPropertyMarginBottom)
                    return BoxSide::Bottom;
                else if constexpr (propertyID == CSSPropertyMarginLeft)
                    return BoxSide::Left;
            };

            switch (mapSidePhysicalToLogical(formattingContextRootStyle(renderer).writingMode(), boxSide())) {
            case LogicalBoxSide::BlockStart:
                return Style::MarginTrimSide::BlockStart;
            case LogicalBoxSide::BlockEnd:
                return Style::MarginTrimSide::BlockEnd;
            case LogicalBoxSide::InlineStart:
                return Style::MarginTrimSide::InlineStart;
            case LogicalBoxSide::InlineEnd:
                return Style::MarginTrimSide::InlineEnd;
            default:
                ASSERT_NOT_REACHED();
                return Style::MarginTrimSide::BlockStart;
            }
        };

        auto usedValue = [](auto& box) {
            if constexpr (propertyID == CSSPropertyMarginTop)
                return Length<> { box.marginTop() };
            else if constexpr (propertyID == CSSPropertyMarginRight)
                return Length<> { box.marginRight() };
            else if constexpr (propertyID == CSSPropertyMarginBottom)
                return Length<> { box.marginBottom() };
            else if constexpr (propertyID == CSSPropertyMarginLeft)
                return Length<> { box.marginLeft() };
        };

        CheckedPtr box = dynamicDowncast<RenderBox>(state.renderer);
        if (!box)
            return functor(value);

        if constexpr (propertyID == CSSPropertyMarginRight) {
            if (rendererCanHaveTrimmedMargin(*box) && box->hasTrimmedMargin(toMarginTrimSide(*box)))
                return functor(usedValue(*box));

            if (value.isFixed())
                return functor(value);

            if (value.isPercentOrCalculated()) {
                // RenderBox gives a marginRight() that is the distance between the right-edge of the child box
                // and the right-edge of the containing box, when display == DisplayType::Block. Let's calculate the absolute
                // value of the specified margin-right % instead of relying on RenderBox's marginRight() value.
                return functor(Length<> { evaluateMinimum<float>(value, box->containingBlockLogicalWidthForContent(), state.style.usedZoomForLength()) });
            }
        }

        return functor(usedValue(*box));
    }
};

template<CSSPropertyID propertyID> struct PaddingEdgeSharedAdaptor {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, const PaddingEdge& value, F&& functor) const
    {
        auto* renderBox = dynamicDowncast<RenderBox>(state.renderer);
        if (!renderBox || value.isFixed())
            return functor(value);

        if constexpr (propertyID == CSSPropertyPaddingTop)
            return functor(Length<> { renderBox->computedCSSPaddingTop() });
        else if constexpr (propertyID == CSSPropertyPaddingRight)
            return functor(Length<> { renderBox->computedCSSPaddingRight() });
        else if constexpr (propertyID == CSSPropertyPaddingBottom)
            return functor(Length<> { renderBox->computedCSSPaddingBottom() });
        else if constexpr (propertyID == CSSPropertyPaddingLeft)
            return functor(Length<> { renderBox->computedCSSPaddingLeft() });
    }
};

template<CSSPropertyID propertyID> struct PreferredSizeSharedAdaptor {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, const PreferredSize& value, F&& functor) const
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
                if constexpr (propertyID == CSSPropertyHeight)
                    return functor(Length<> { sizingBox(*state.renderer).height() });
                else if constexpr (propertyID == CSSPropertyWidth)
                    return functor(Length<> { sizingBox(*state.renderer).width() });
            }
        }
        return functor(value);
    }
};

template<CSSPropertyID> struct MaximumSizeSharedAdaptor {
    template<typename F> decltype(auto) computedValue(ExtractorState&, const MaximumSize& value, F&& functor) const
    {
        if (value.isNone())
            return functor(CSS::Keyword::None { });
        return functor(value);
    }
};

template<CSSPropertyID> struct MinimumSizeSharedAdaptor {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, const MinimumSize& value, F&& functor) const
    {
        auto isFlexOrGridItem = [](auto renderer) {
            auto* box = dynamicDowncast<RenderBox>(renderer);
            return box && (box->isFlexItem() || box->isGridItem());
        };

        if (value.isAuto()) {
            if (isFlexOrGridItem(state.renderer))
                return functor(CSS::Keyword::Auto { });
            return functor(Length<> { 0 });
        }
        return functor(value);
    }
};

template<CSSPropertyID> struct PageBreakSharedAdaptor {
    template<typename F> decltype(auto) computedValue(ExtractorState&, BreakBetween value, F&& functor) const
    {
        switch (value) {
        case BreakBetween::Page:
        case BreakBetween::LeftPage:
        case BreakBetween::RightPage:
        case BreakBetween::RectoPage:
        case BreakBetween::VersoPage:
            // CSS 2.1 allows us to map these to always.
            return functor(CSS::Keyword::Always { });
        case BreakBetween::Avoid:
        case BreakBetween::AvoidPage:
            return functor(CSS::Keyword::Avoid { });
        case BreakBetween::AvoidColumn:
        case BreakBetween::Column:
        case BreakBetween::Auto:
            return functor(CSS::Keyword::Auto { });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    template<typename F> decltype(auto) computedValue(ExtractorState&, BreakInside value, F&& functor) const
    {
        switch (value) {
        case BreakInside::Avoid:
        case BreakInside::AvoidPage:
            return functor(CSS::Keyword::Avoid { });
        case BreakInside::AvoidColumn:
        case BreakInside::Auto:
            return functor(CSS::Keyword::Auto { });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
};

template<CSSPropertyID> struct WebkitColumnBreakSharedAdaptor {
    template<typename F> decltype(auto) computedValue(ExtractorState&, BreakBetween value, F&& functor) const
    {
        switch (value) {
        case BreakBetween::Column:
            return functor(CSS::Keyword::Always { });
        case BreakBetween::Avoid:
        case BreakBetween::AvoidColumn:
            return functor(CSS::Keyword::Avoid { });
        case BreakBetween::Page:
        case BreakBetween::LeftPage:
        case BreakBetween::RightPage:
        case BreakBetween::RectoPage:
        case BreakBetween::VersoPage:
        case BreakBetween::AvoidPage:
        case BreakBetween::Auto:
            return functor(CSS::Keyword::Auto { });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    template<typename F> decltype(auto) computedValue(ExtractorState&, BreakInside value, F&& functor) const
    {
        switch (value) {
        case BreakInside::Avoid:
        case BreakInside::AvoidColumn:
            return functor(CSS::Keyword::Avoid { });
        case BreakInside::AvoidPage:
        case BreakInside::Auto:
            return functor(CSS::Keyword::Auto { });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
};

// MARK: - Adaptors

// Adaptors are used to implement the logic for extracting a value from a RenderStyle and performing some operation of the CSS value equivalent. This allows the same code to be used for CSS creation and serialization.

template<CSSPropertyID> struct PropertyExtractorAdaptor;

template<> struct PropertyExtractorAdaptor<CSSPropertyDirection> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        if (state.element.ptr() == state.element->document().documentElement() && !state.style.hasExplicitlySetDirection())
            return functor(RenderStyle::initialDirection());
        return functor(state.style.writingMode().computedTextDirection());
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyWritingMode> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        if (state.element.ptr() == state.element->document().documentElement() && !state.style.hasExplicitlySetWritingMode())
            return functor(RenderStyle::initialWritingMode());
        return functor(state.style.writingMode().computedWritingMode());
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyFloat> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        if (state.style.hasOutOfFlowPosition())
            return functor(CSS::Keyword::None { });
        return functor(state.style.floating());
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyContent> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        if (state.style.hasUsedContentNone())
            return functor(CSS::Keyword::None { });
        return functor(state.style.content());
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyLetterSpacing> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        // "For legacy reasons, a computed letter-spacing of zero yields a
        //  resolved value (getComputedStyle() return value) of `normal`."
        // https://www.w3.org/TR/css-text-4/#letter-spacing-property

        auto& spacing = state.style.computedLetterSpacing();
        if (auto fixedSpacing = spacing.tryFixed(); fixedSpacing && fixedSpacing->isZero())
            return functor(CSS::Keyword::Normal { });
        return functor(spacing);
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyWordSpacing> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return functor(state.style.computedWordSpacing());
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyLineHeight> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return WTF::switchOn(state.style.lineHeight(),
            [&](const CSS::Keyword::Normal& keyword) {
                return functor(keyword);
            },
            [&](const LineHeight::Fixed& fixed) {
                return functor(fixed);
            },
            [&](const LineHeight::Percentage& percentage) {
                // CSSValueConversion<LineHeight> will convert a percentage value to a fixed value,
                // and a number value to a percentage value. To be able to roundtrip a number value, we thus
                // look for a percent value and convert it back to a number.
                if (state.valueType == ExtractorState::PropertyValueType::Computed)
                    return functor(Number<CSS::Nonnegative> { percentage.value / 100 });

                // This is imperfect, because it doesn't include the zoom factor and the real computation
                // for how high to be in pixels does include things like minimum font size and the zoom factor.
                // On the other hand, since font-size doesn't include the zoom factor, we really can't do
                // that here either.
                return functor(Length<CSS::Nonnegative> { percentage.value * state.style.fontDescription().computedSize() / 100 });
            },
            [&](const LineHeight::Calc& calc) {
                // FIXME: We pass 1.0f here to get the unzoomed value but it really is not clear why we are even
                // evaluating calc here. We should probably revisit this and figure out another way to do this.
                return functor(Length<CSS::Nonnegative> { evaluate<float>(calc, 0.0f, Style::ZoomFactor { 1.0f, 1.0f }) });
            }
        );
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyFontFamily> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        auto fontFamily = state.style.fontFamily();
        if (fontFamily.size() == 1)
            return functor(fontFamily.first());
        return functor(fontFamily);
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyFontSize> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return functor(Length<CSS::Nonnegative> { state.style.fontDescription().computedSize() });
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyTop> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return InsetEdgeSharedAdaptor<CSSPropertyTop> { }.computedValue(state, state.style.top(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyRight> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return InsetEdgeSharedAdaptor<CSSPropertyRight> { }.computedValue(state, state.style.right(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyBottom> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return InsetEdgeSharedAdaptor<CSSPropertyBottom> { }.computedValue(state, state.style.bottom(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyLeft> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return InsetEdgeSharedAdaptor<CSSPropertyLeft> { }.computedValue(state, state.style.left(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyMarginTop> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return MarginEdgeSharedAdaptor<CSSPropertyMarginTop> { }.computedValue(state, state.style.marginTop(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyMarginRight> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return MarginEdgeSharedAdaptor<CSSPropertyMarginRight> { }.computedValue(state, state.style.marginRight(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyMarginBottom> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return MarginEdgeSharedAdaptor<CSSPropertyMarginBottom> { }.computedValue(state, state.style.marginBottom(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyMarginLeft> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return MarginEdgeSharedAdaptor<CSSPropertyMarginLeft> { }.computedValue(state, state.style.marginLeft(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyPaddingTop> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return PaddingEdgeSharedAdaptor<CSSPropertyPaddingTop> { }.computedValue(state, state.style.paddingTop(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyPaddingRight> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return PaddingEdgeSharedAdaptor<CSSPropertyPaddingRight> { }.computedValue(state, state.style.paddingRight(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyPaddingBottom> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return PaddingEdgeSharedAdaptor<CSSPropertyPaddingBottom> { }.computedValue(state, state.style.paddingBottom(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyPaddingLeft> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return PaddingEdgeSharedAdaptor<CSSPropertyPaddingLeft> { }.computedValue(state, state.style.paddingLeft(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyHeight> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return PreferredSizeSharedAdaptor<CSSPropertyHeight> { }.computedValue(state, state.style.height(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyWidth> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return PreferredSizeSharedAdaptor<CSSPropertyWidth> { }.computedValue(state, state.style.width(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyMaxHeight> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return MaximumSizeSharedAdaptor<CSSPropertyMaxHeight> { }.computedValue(state, state.style.maxHeight(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyMaxWidth> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return MaximumSizeSharedAdaptor<CSSPropertyMaxWidth> { }.computedValue(state, state.style.maxWidth(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyMinHeight> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return MinimumSizeSharedAdaptor<CSSPropertyMinHeight> { }.computedValue(state, state.style.minHeight(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyMinWidth> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return MinimumSizeSharedAdaptor<CSSPropertyMinWidth> { }.computedValue(state, state.style.minWidth(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyGridAutoFlow> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        // FIXME: Adjust this once CSSWG clarifies exactly how the initial value should compute.
        // For now, this gives the most backwards-compatible behavior.
        auto gridFlow = state.style.gridAutoFlow();
        switch (gridFlow.direction()) {
        case GridAutoFlow::Direction::Column:
            switch (gridFlow.packing()) {
            case GridAutoFlow::Packing::Dense:
                return functor(SpaceSeparatedTuple { CSS::Keyword::Column { }, CSS::Keyword::Dense { } });
            case GridAutoFlow::Packing::Sparse:
                return functor(CSS::Keyword::Column { });
            }
        case GridAutoFlow::Direction::Row:
            switch (gridFlow.packing()) {
            case GridAutoFlow::Packing::Dense:
                if (!state.style.gridTemplateRows().isNone() && state.style.gridTemplateColumns().isNone()
                    && (state.style.display() == DisplayType::GridLanes || state.style.display() == DisplayType::InlineGridLanes))
                    return functor(SpaceSeparatedTuple { CSS::Keyword::Row { }, CSS::Keyword::Dense { } });
                return functor(CSS::Keyword::Dense { });
            case GridAutoFlow::Packing::Sparse:
                return functor(CSS::Keyword::Row { });
            }
        default:
            ASSERT(state.style.display() != DisplayType::GridLanes && state.style.display() != DisplayType::InlineGridLanes
                && state.style.display() != DisplayType::Grid && state.style.display() != DisplayType::InlineGrid);
            switch (gridFlow.packing()) {
            case GridAutoFlow::Packing::Dense:
                return functor(CSS::Keyword::Dense { });
            case GridAutoFlow::Packing::Sparse:
                return functor(CSS::Keyword::Normal { });
            }
        }
    }
};

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

// FIXME: if 'auto' value is removed then this can likely also be removed.
template<> struct PropertyExtractorAdaptor<CSSPropertyWidows> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return functor(state.style.widows().tryValue().value_or(2));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyOrphans> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return functor(state.style.orphans().tryValue().value_or(2));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyWebkitTextCombine> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        auto textCombine = state.style.textCombine();
        if (textCombine == TextCombine::All)
            return functor(CSS::Keyword::Horizontal { });
        return functor(textCombine);
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyWebkitRubyPosition> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        switch (state.style.rubyPosition()) {
        case RubyPosition::Over:
            return functor(CSS::Keyword::Before { });
        case RubyPosition::Under:
            return functor(CSS::Keyword::After { });
        case RubyPosition::InterCharacter:
        case RubyPosition::LegacyInterCharacter:
            return functor(CSS::Keyword::InterCharacter { });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyBlockStep> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        auto blockStepSize = state.style.blockStepSize();
        bool hasBlockStepSize = blockStepSize != RenderStyle::initialBlockStepSize();
        auto blockStepSizeValue = [&] -> std::optional<Style::BlockStepSize> {
            return hasBlockStepSize ? std::make_optional(blockStepSize) : std::nullopt;
        };

        auto blockStepInsert = state.style.blockStepInsert();
        bool hasBlockStepInsert = blockStepInsert != RenderStyle::initialBlockStepInsert();
        auto blockStepInsertValue = [&] -> std::optional<BlockStepInsert> {
            return hasBlockStepInsert ? std::make_optional(blockStepInsert) : std::nullopt;
        };

        auto blockStepAlign = state.style.blockStepAlign();
        bool hasBlockStepAlign = blockStepAlign != RenderStyle::initialBlockStepAlign();
        auto blockStepAlignValue = [&] -> std::optional<BlockStepAlign> {
            return hasBlockStepAlign ? std::make_optional(blockStepAlign) : std::nullopt;
        };

        auto blockStepRound = state.style.blockStepRound();
        bool hasBlockStepRound = blockStepRound != RenderStyle::initialBlockStepRound();
        auto blockStepRoundValue = [&] -> std::optional<BlockStepRound> {
            return hasBlockStepRound ? std::make_optional(blockStepRound) : std::nullopt;
        };

        if (!hasBlockStepSize && !hasBlockStepInsert && !hasBlockStepAlign && !hasBlockStepRound)
            return functor(CSS::Keyword::None { });

        return functor(SpaceSeparatedTuple { blockStepSizeValue(), blockStepInsertValue(), blockStepAlignValue(), blockStepRoundValue() });
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyFontSynthesis> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        auto& description = state.style.fontDescription();

        bool hasWeight = description.hasAutoFontSynthesisWeight();
        auto weightValue = [&] -> std::optional<CSS::Keyword::Weight> {
            return hasWeight ? std::make_optional(CSS::Keyword::Weight { }) : std::nullopt;
        };

        bool hasStyle = description.hasAutoFontSynthesisStyle();
        auto styleValue = [&] -> std::optional<CSS::Keyword::Style> {
            return hasStyle ? std::make_optional(CSS::Keyword::Style { }) : std::nullopt;
        };

        bool hasSmallCaps = description.hasAutoFontSynthesisSmallCaps();
        auto smallCapsValue = [&] -> std::optional<CSS::Keyword::SmallCaps> {
            return hasSmallCaps ? std::make_optional(CSS::Keyword::SmallCaps { }) : std::nullopt;
        };

        if (!hasWeight && !hasStyle && !hasSmallCaps)
            return functor(CSS::Keyword::None { });
        return functor(SpaceSeparatedTuple { weightValue(), styleValue(), smallCapsValue() });
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyLineClamp> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        auto maxLines = state.style.maxLines().tryValue();
        if (!maxLines)
            return functor(CSS::Keyword::None { });
        return functor(SpaceSeparatedTuple { *maxLines, state.style.blockEllipsis() });
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyMaskBorder> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return functor(state.style.maskBorder());
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyOverscrollBehavior> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return functor(std::max(state.style.overscrollBehaviorX(), state.style.overscrollBehaviorY()));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyPageBreakAfter> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return PageBreakSharedAdaptor<CSSPropertyPageBreakAfter> { }.computedValue(state, state.style.breakAfter(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyPageBreakBefore> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return PageBreakSharedAdaptor<CSSPropertyPageBreakBefore> { }.computedValue(state, state.style.breakBefore(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyPageBreakInside> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return PageBreakSharedAdaptor<CSSPropertyPageBreakInside> { }.computedValue(state, state.style.breakInside(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyWebkitColumnBreakAfter> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return WebkitColumnBreakSharedAdaptor<CSSPropertyWebkitColumnBreakAfter> { }.computedValue(state, state.style.breakAfter(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyWebkitColumnBreakBefore> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return WebkitColumnBreakSharedAdaptor<CSSPropertyWebkitColumnBreakBefore> { }.computedValue(state, state.style.breakBefore(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyWebkitColumnBreakInside> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        return WebkitColumnBreakSharedAdaptor<CSSPropertyWebkitColumnBreakInside> { }.computedValue(state, state.style.breakInside(), std::forward<F>(functor));
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyPerspectiveOrigin> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        if (state.renderer) {
            auto box = state.renderer->transformReferenceBoxRect(state.style);

            auto perspectiveOriginX = Length<> { evaluate<float>(state.style.perspectiveOriginX(), box.width(), ZoomNeeded { }) };
            auto perspectiveOriginY = Length<> { evaluate<float>(state.style.perspectiveOriginY(), box.height(), ZoomNeeded { }) };

            return functor(SpaceSeparatedTuple { perspectiveOriginX, perspectiveOriginY });
        }

        return functor(SpaceSeparatedTuple { state.style.perspectiveOriginX(), state.style.perspectiveOriginY() });
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyTextBox> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        auto textBoxTrim = state.style.textBoxTrim();
        auto textBoxEdge = state.style.textBoxEdge();

        auto hasDefaultTextBoxTrim = textBoxTrim == RenderStyle::initialTextBoxTrim();
        auto hasDefaultTextBoxEdge = textBoxEdge == RenderStyle::initialTextBoxEdge();

        if (hasDefaultTextBoxTrim && hasDefaultTextBoxEdge)
            return functor(CSS::Keyword::Normal { });
        if (hasDefaultTextBoxEdge)
            return functor(textBoxTrim);
        if (textBoxTrim == TextBoxTrim::TrimBoth)
            return functor(textBoxEdge);

        return functor(SpaceSeparatedTuple { textBoxTrim, textBoxEdge });
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyTextDecoration> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        auto textDecorationLine = state.style.textDecorationLine();
        bool hasTextDecorationLine = textDecorationLine != RenderStyle::initialTextDecorationLine();
        auto textDecorationLineValue = [&] -> std::optional<TextDecorationLine> {
            return hasTextDecorationLine ? std::make_optional(textDecorationLine) : std::nullopt;
        };

        auto textDecorationThickness = state.style.textDecorationThickness();
        bool hasTextDecorationThickness = state.style.textDecorationThickness() != RenderStyle::initialTextDecorationThickness();
        auto textDecorationThicknessValue = [&] -> std::optional<TextDecorationThickness> {
            return hasTextDecorationThickness ? std::make_optional(textDecorationThickness) : std::nullopt;
        };

        auto textDecorationStyle = state.style.textDecorationStyle();
        bool hasTextDecorationStyle = state.style.textDecorationStyle() != RenderStyle::initialTextDecorationStyle();
        auto textDecorationStyleValue = [&] -> std::optional<TextDecorationStyle> {
            return hasTextDecorationStyle ? std::make_optional(textDecorationStyle) : std::nullopt;
        };

        auto textDecorationColor = state.style.textDecorationColor();
        bool hasTextDecorationColor = !textDecorationColor.isCurrentColor();
        auto textDecorationColorValue = [&] -> std::optional<Color> {
            return hasTextDecorationColor ? std::make_optional(textDecorationColor) : std::nullopt;
        };

        if (!hasTextDecorationLine && !hasTextDecorationStyle && !hasTextDecorationColor && !hasTextDecorationThickness)
            return functor(CSS::Keyword::None { });

        return functor(SpaceSeparatedTuple { textDecorationLineValue(), textDecorationThicknessValue(), textDecorationStyleValue(), textDecorationColorValue() });
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyTextWrap> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        auto textWrapMode = state.style.textWrapMode();
        auto textWrapStyle = state.style.textWrapStyle();

        // Omit default longhand values.
        if (textWrapStyle == RenderStyle::initialTextWrapStyle())
            return functor(textWrapMode);
        if (textWrapMode == RenderStyle::initialTextWrapMode())
            return functor(textWrapStyle);

        return functor(SpaceSeparatedTuple { textWrapMode, textWrapStyle });
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyTransformOrigin> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        if (state.renderer) {
            auto box = state.renderer->transformReferenceBoxRect(state.style);

            auto transformOriginX = Length<> { evaluate<float>(state.style.transformOriginX(), box.width(), ZoomNeeded { }) };
            auto transformOriginY = Length<> { evaluate<float>(state.style.transformOriginY(), box.height(), ZoomNeeded { }) };

            if (auto transformOriginZ = state.style.transformOriginZ(); !transformOriginZ.isZero())
                return functor(SpaceSeparatedTuple { transformOriginX, transformOriginY, transformOriginZ });
            return functor(SpaceSeparatedTuple { transformOriginX, transformOriginY });
        }

        if (auto transformOriginZ = state.style.transformOriginZ(); !transformOriginZ.isZero())
            return functor(SpaceSeparatedTuple { state.style.transformOriginX(), state.style.transformOriginY(), transformOriginZ });
        return functor(SpaceSeparatedTuple { state.style.transformOriginX(), state.style.transformOriginY() });
    }
};

template<> struct PropertyExtractorAdaptor<CSSPropertyWhiteSpace> {
    template<typename F> decltype(auto) computedValue(ExtractorState& state, F&& functor) const
    {
        auto whiteSpaceCollapse = state.style.whiteSpaceCollapse();
        auto textWrapMode = state.style.textWrapMode();

        // Convert to backwards-compatible keywords if possible.
        if (whiteSpaceCollapse == WhiteSpaceCollapse::Collapse && textWrapMode == TextWrapMode::Wrap)
            return functor(CSS::Keyword::Normal { });
        if (whiteSpaceCollapse == WhiteSpaceCollapse::Preserve && textWrapMode == TextWrapMode::NoWrap)
            return functor(CSS::Keyword::Pre { });
        if (whiteSpaceCollapse == WhiteSpaceCollapse::Preserve && textWrapMode == TextWrapMode::Wrap)
            return functor(CSS::Keyword::PreWrap { });
        if (whiteSpaceCollapse == WhiteSpaceCollapse::PreserveBreaks && textWrapMode == TextWrapMode::Wrap)
            return functor(CSS::Keyword::PreLine { });

        // Omit default longhand values.
        if (whiteSpaceCollapse == RenderStyle::initialWhiteSpaceCollapse())
            return functor(textWrapMode);
        if (textWrapMode == RenderStyle::initialTextWrapMode())
            return functor(whiteSpaceCollapse);

        return functor(SpaceSeparatedTuple { whiteSpaceCollapse, textWrapMode });
    }
};

// MARK: - Adaptor Invokers

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

template<CSSPropertyID propertyID, typename List, typename Mapper> Ref<CSSValue> extractCoordinatedValueListValue(ExtractorState& state, const List& list, Mapper&& mapper)
{
    using PropertyAccessor = CoordinatedValueListPropertyConstAccessor<propertyID>;

    CSSValueListBuilder resultListBuilder;

    if constexpr (List::value_type::computedValueUsesUsedValues) {
        for (auto& value : list.usedValues())
            resultListBuilder.append(mapper(state, PropertyAccessor { value }.get(), value, list));
    } else {
        if (!list.isInitial()) {
            for (auto& value : list.computedValues()) {
                if (!PropertyAccessor { value }.isFilled())
                    resultListBuilder.append(mapper(state, PropertyAccessor { value }.get(), value, list));
            }
        } else
            resultListBuilder.append(mapper(state, PropertyAccessor::initial(), std::nullopt, list));
    }

    return CSSValueList::createCommaSeparated(WTFMove(resultListBuilder));
}

template<CSSPropertyID propertyID, typename List, typename Mapper> void extractCoordinatedValueListSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const List& list, Mapper&& mapper)
{
    using PropertyAccessor = CoordinatedValueListPropertyConstAccessor<propertyID>;

    bool includeComma = false;

    if constexpr (List::value_type::computedValueUsesUsedValues) {
        for (auto& value : list.usedValues()) {
            if (includeComma)
                builder.append(", "_s);
            mapper(state, builder, context, PropertyAccessor { value }.get(), value, list);
            includeComma = true;
        }
    } else {
        if (!list.isInitial()) {
            for (auto& value : list.computedValues()) {
                if (!PropertyAccessor { value }.isFilled()) {
                    if (includeComma)
                        builder.append(", "_s);
                    mapper(state, builder, context, PropertyAccessor { value }.get(), value, list);
                    includeComma = true;
                }
            }
        } else
            mapper(state, builder, context, PropertyAccessor::initial(), std::nullopt, list);
    }
}

template<CSSPropertyID propertyID> Ref<CSSValue> extractCounterValue(ExtractorState& state)
{
    auto& map = state.style.counterDirectives().map;
    if (map.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

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
            list.append(createCSSValue(state.pool, state.style, CustomIdentifier { keyValue.key }));
            list.append(createCSSValue(state.pool, state.style, Integer<> { *number }));
        }
    }
    if (!list.isEmpty())
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
}

template<CSSPropertyID propertyID> void extractCounterSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& map = state.style.counterDirectives().map;
    if (map.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
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

            serializationForCSS(builder, context, state.style, CustomIdentifier { keyValue.key });
            builder.append(' ');
            serializationForCSS(builder, context, state.style, Integer<> { *number });

            listEmpty = false;
        }
    }

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
}

template<GridTrackSizingDirection direction> Ref<CSSValue> extractGridTemplateValue(ExtractorState& state)
{
    auto addValuesForNamedGridLinesAtIndex = [](auto& list, auto& collector, auto i, auto renderEmpty) {
        if (collector.isEmpty() && !renderEmpty)
            return;

        Vector<String> lineNames;
        collector.collectLineNamesForIndex(lineNames, i);
        if (!lineNames.isEmpty() || renderEmpty)
            list.append(CSSGridLineNamesValue::create(lineNames));
    };

    auto& tracks = state.style.gridTemplateList(direction);

    auto* renderGrid = dynamicDowncast<RenderGrid>(state.renderer);

    auto& trackSizes = tracks.sizes;
    auto& autoRepeatTrackSizes = tracks.autoRepeatSizes;

    // Handle the 'none' case.
    bool trackListIsEmpty = trackSizes.isEmpty() && autoRepeatTrackSizes.isEmpty();
    if (renderGrid && trackListIsEmpty) {
        // For grids we should consider every listed track, whether implicitly or explicitly
        // created. Empty grids have a sole grid line per axis.
        auto& positions = renderGrid->positions(direction);
        trackListIsEmpty = positions.size() == 1;
    }

    bool isSubgrid = tracks.subgrid;

    if (trackListIsEmpty && !isSubgrid)
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    CSSValueListBuilder list;

    // If the element is a grid container, the resolved value is the used value,
    // specifying track sizes in pixels and expanding the repeat() notation.
    // If subgrid was specified, but the element isn't a subgrid (due to not having
    // an appropriate grid parent), then we fall back to using the specified value.
    if (renderGrid && (!isSubgrid || renderGrid->isSubgrid(direction))) {
        if (isSubgrid) {
            list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Subgrid { }));

            OrderedNamedLinesCollectorInSubgridLayout collector(state, tracks, renderGrid->numTracks(direction));
            for (int i = 0; i < collector.namedGridLineCount(); i++)
                addValuesForNamedGridLinesAtIndex(list, collector, i, true);
            return CSSValueList::createSpaceSeparated(WTFMove(list));
        }

        OrderedNamedLinesCollectorInGridLayout collector(state, tracks, renderGrid->autoRepeatCountForDirection(direction), autoRepeatTrackSizes.size());
        auto computedTrackSizes = renderGrid->trackSizesForComputedStyle(direction);
        // Named grid line indices are relative to the explicit grid, but we are including all tracks.
        // So we need to subtract the number of leading implicit tracks in order to get the proper line index.
        int offset = -renderGrid->explicitGridStartForDirection(direction);

        int start = 0;
        int end = computedTrackSizes.size();
        ASSERT(start <= end);
        ASSERT(static_cast<unsigned>(end) <= computedTrackSizes.size());
        for (int i = start; i < end; ++i) {
            if (i + offset >= 0)
                addValuesForNamedGridLinesAtIndex(list, collector, i + offset, false);
            list.append(createCSSValue(state.pool, state.style, Length<> { computedTrackSizes[i] }));
        }
        if (end + offset >= 0)
            addValuesForNamedGridLinesAtIndex(list, collector, end + offset, false);
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    }

    // Otherwise, the resolved value is the computed value, preserving repeat().
    auto& computedTracks = tracks.list;

    auto repeatVisitor = [&](CSSValueListBuilder& list, const RepeatEntry& entry) {
        if (std::holds_alternative<Vector<String>>(entry)) {
            const auto& names = std::get<Vector<String>>(entry);
            if (names.isEmpty() && !isSubgrid)
                return;
            list.append(CSSGridLineNamesValue::create(names));
        } else
            list.append(createCSSValue(state.pool, state.style, std::get<GridTrackSize>(entry)));
    };

    for (auto& entry : computedTracks) {
        WTF::switchOn(entry,
            [&](const GridTrackSize& size) {
                list.append(createCSSValue(state.pool, state.style, size));
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
                list.append(createCSSValue(state.pool, state.style, CSS::Keyword::Subgrid { }));
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
        auto x = createCSSValue(state.pool, state.style, radius.width());
        auto y = radius.width() == radius.height() ? x.copyRef() : createCSSValue(state.pool, state.style, radius.height());
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

template<CSSPropertyID property> inline Ref<CSSValue> extractFillLayerPropertyShorthand(ExtractorState& state, const StylePropertyShorthand& propertiesBeforeSlashSeparator, const StylePropertyShorthand& propertiesAfterSlashSeparator, CSSPropertyID lastLayerProperty)
{
    static_assert(property == CSSPropertyBackground || property == CSSPropertyMask);

    auto computeRenderStyle = [&](std::unique_ptr<RenderStyle>& ownedStyle) -> const RenderStyle* {
        if (auto renderer = state.element->renderer(); renderer && renderer->isComposited() && Interpolation::isAccelerated(property, state.element->document().settings())) {
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

        const auto& layers = [&] {
            if constexpr (property == CSSPropertyMask)
                return style->maskLayers();
            else
                return style->backgroundLayers();
        }();

        if constexpr (property == CSSPropertyMask) {
            if (layers.usedLength() == 1 && !layers.usedFirst().hasImage())
                return 0;
        }

        return layers.usedLength();
    }();
    if (!layerCount) {
        ASSERT(property == CSSPropertyMask);
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
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

template<CSSPropertyID property> inline void extractFillLayerPropertyShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StylePropertyShorthand& propertiesBeforeSlashSeparator, const StylePropertyShorthand& propertiesAfterSlashSeparator, CSSPropertyID lastLayerProperty)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractFillLayerPropertyShorthand<property>(state, propertiesBeforeSlashSeparator, propertiesAfterSlashSeparator, lastLayerProperty)->cssText(context));
}

// MARK: - Custom Extractors

inline Ref<CSSValue> ExtractorCustom::extractDirection(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyDirection>(state);
}

inline void ExtractorCustom::extractDirectionSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyDirection>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractWritingMode(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyWritingMode>(state);
}

inline void ExtractorCustom::extractWritingModeSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyWritingMode>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractFloat(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyFloat>(state);
}

inline void ExtractorCustom::extractFloatSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyFloat>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractContent(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyContent>(state);
}

inline void ExtractorCustom::extractContentSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyContent>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractLetterSpacing(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyLetterSpacing>(state);
}

inline void ExtractorCustom::extractLetterSpacingSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyLetterSpacing>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractWordSpacing(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyWordSpacing>(state);
}

inline void ExtractorCustom::extractWordSpacingSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyWordSpacing>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractLineHeight(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyLineHeight>(state);
}

inline void ExtractorCustom::extractLineHeightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyLineHeight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractFontFamily(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyFontFamily>(state);
}

inline void ExtractorCustom::extractFontFamilySerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyFontFamily>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractFontSize(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyFontSize>(state);
}

inline void ExtractorCustom::extractFontSizeSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyFontSize>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractTop(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyTop>(state);
}

inline void ExtractorCustom::extractTopSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyTop>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractRight(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyRight>(state);
}

inline void ExtractorCustom::extractRightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyRight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractBottom(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyBottom>(state);
}

inline void ExtractorCustom::extractBottomSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyBottom>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractLeft(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyLeft>(state);
}

inline void ExtractorCustom::extractLeftSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyLeft>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMarginTop(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyMarginTop>(state);
}

inline void ExtractorCustom::extractMarginTopSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyMarginTop>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMarginRight(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyMarginRight>(state);
}

inline void ExtractorCustom::extractMarginRightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyMarginRight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMarginBottom(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyMarginBottom>(state);
}

inline void ExtractorCustom::extractMarginBottomSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyMarginBottom>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMarginLeft(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyMarginLeft>(state);
}

inline void ExtractorCustom::extractMarginLeftSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyMarginLeft>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractPaddingTop(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyPaddingTop>(state);
}

inline void ExtractorCustom::extractPaddingTopSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyPaddingTop>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractPaddingRight(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyPaddingRight>(state);
}

inline void ExtractorCustom::extractPaddingRightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyPaddingRight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractPaddingBottom(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyPaddingBottom>(state);
}

inline void ExtractorCustom::extractPaddingBottomSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyPaddingBottom>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractPaddingLeft(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyPaddingLeft>(state);
}

inline void ExtractorCustom::extractPaddingLeftSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyPaddingLeft>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractHeight(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyHeight>(state);
}

inline void ExtractorCustom::extractHeightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyHeight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractWidth(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyWidth>(state);
}

inline void ExtractorCustom::extractWidthSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyWidth>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMaxHeight(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyMaxHeight>(state);
}

inline void ExtractorCustom::extractMaxHeightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyMaxHeight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMaxWidth(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyMaxWidth>(state);
}

inline void ExtractorCustom::extractMaxWidthSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyMaxWidth>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMinHeight(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyMinHeight>(state);
}

inline void ExtractorCustom::extractMinHeightSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyMinHeight>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractMinWidth(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyMinWidth>(state);
}

inline void ExtractorCustom::extractMinWidthSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyMinWidth>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractGridAutoFlow(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyGridAutoFlow>(state);
}

inline void ExtractorCustom::extractGridAutoFlowSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyGridAutoFlow>(state, builder, context);
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

inline RefPtr<CSSValue> ExtractorCustom::extractBorderImageWidth(ExtractorState& state)
{
    auto& borderImage = state.style.borderImage();
    if (borderImage.overridesBorderWidths())
        return nullptr;
    return createCSSValue(state.pool, state.style, borderImage.width());
}

inline void ExtractorCustom::extractBorderImageWidthSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& borderImage = state.style.borderImage();
    if (borderImage.overridesBorderWidths())
        return;
    serializationForCSS(builder, context, state.style, borderImage.width());
}

inline Ref<CSSValue> ExtractorCustom::extractTransform(ExtractorState& state)
{
    if (!state.style.hasTransform())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    if (state.renderer) {
        TransformationMatrix transform;
        state.style.applyTransform(transform, TransformOperationData(state.renderer->transformReferenceBoxRect(state.style), state.renderer), { });
        return CSSTransformListValue::create(ExtractorConverter::convertTransformationMatrix(state, transform));
    }

    // https://w3c.github.io/csswg-drafts/css-transforms-1/#serialization-of-the-computed-value
    // If we don't have a renderer, then the value should be "none" if we're asking for the
    // resolved value (such as when calling getComputedStyle()).
    if (state.valueType == ExtractorState::PropertyValueType::Resolved)
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    return createCSSValue(state.pool, state.style, state.style.transform());
}

inline void ExtractorCustom::extractTransformSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (!state.style.hasTransform()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
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
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    serializationForCSS(builder, context, state.style, state.style.transform());
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

inline Ref<CSSValue> ExtractorCustom::extractGridTemplateColumns(ExtractorState& state)
{
    return extractGridTemplateValue<GridTrackSizingDirection::Columns>(state);
}

inline void ExtractorCustom::extractGridTemplateColumnsSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractGridTemplateSerialization<GridTrackSizingDirection::Columns>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractGridTemplateRows(ExtractorState& state)
{
    return extractGridTemplateValue<GridTrackSizingDirection::Rows>(state);
}

inline void ExtractorCustom::extractGridTemplateRowsSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractGridTemplateSerialization<GridTrackSizingDirection::Rows>(state, builder, context);
}

inline Ref<CSSValue> convertSingleAnimationDuration(ExtractorState& state, const Style::SingleAnimationDuration& duration, const std::optional<Style::Animation>& animation, const Style::Animations& animationList)
{
    auto animationListHasMultipleExplicitTimelines = [&] {
        if (animationList.computedLength() <= 1)
            return false;
        auto explicitTimelines = 0;
        for (auto& animation : animationList.computedValues()) {
            if (animation.isTimelineSet())
                ++explicitTimelines;
            if (explicitTimelines > 1)
                return true;
        }
        return false;
    };

    auto animationHasExplicitNonAutoTimeline = [&] {
        if (!animation || !animation->isTimelineSet())
            return false;
        return !animation->timeline().isAuto();
    };

    // https://drafts.csswg.org/css-animations-2/#animation-duration
    // For backwards-compatibility with Level 1, when the computed value of animation-timeline is auto
    // (i.e. only one list value, and that value being auto), the resolved value of auto for
    // animation-duration is 0s whenever its used value would also be 0s.
    if (duration.isAuto() && (animationListHasMultipleExplicitTimelines() || animationHasExplicitNonAutoTimeline()))
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
    return createCSSValue(state.pool, state.style, duration.tryTime().value_or(0_css_s));
}

inline Ref<CSSValue> ExtractorCustom::extractAnimationDuration(ExtractorState& state)
{
    auto mapper = [](auto& state, const auto& value, const std::optional<Animation>& animation, const auto& animations) -> Ref<CSSValue> {
        return convertSingleAnimationDuration(state, value, animation, animations);
    };
    return extractCoordinatedValueListValue<CSSPropertyAnimationDuration>(state, state.style.animations(), mapper);
}

inline void ExtractorCustom::extractAnimationDurationSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto mapper = [&](auto& state, auto& builder, const auto& context, const auto& value, const std::optional<Animation>& animation, const auto& animations) {
        // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
        builder.append(convertSingleAnimationDuration(state, value, animation, animations)->cssText(context));
    };
    return extractCoordinatedValueListSerialization<CSSPropertyAnimationDuration>(state, builder, context, state.style.animations(), mapper);
}

inline Ref<CSSValue> ExtractorCustom::extractWidows(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyWidows>(state);
}

inline void ExtractorCustom::extractWidowsSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyWidows>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractOrphans(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyOrphans>(state);
}

inline void ExtractorCustom::extractOrphansSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyOrphans>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractWebkitTextCombine(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyWebkitTextCombine>(state);
}

inline void ExtractorCustom::extractWebkitTextCombineSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyWebkitTextCombine>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractWebkitRubyPosition(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyWebkitRubyPosition>(state);
}

inline void ExtractorCustom::extractWebkitRubyPositionSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyWebkitRubyPosition>(state, builder, context);
}

inline Ref<CSSValue> ExtractorCustom::extractWebkitMaskComposite(ExtractorState& extractorState)
{
    auto mapper = [](auto&, const auto& value, const std::optional<MaskLayers::value_type>&, const auto&) -> Ref<CSSValue> {
        return CSSPrimitiveValue::create(toCSSValueIDForWebkitMaskComposite(value));
    };
    return extractCoordinatedValueListValue<CSSPropertyID::CSSPropertyMaskComposite>(extractorState, extractorState.style.maskLayers(), mapper);
}

inline void ExtractorCustom::extractWebkitMaskCompositeSerialization(ExtractorState& extractorState, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto mapper = [](auto&, auto& builder, const auto&, const auto& value, const std::optional<MaskLayers::value_type>&, const auto&) {
        builder.append(nameLiteralForSerialization(toCSSValueIDForWebkitMaskComposite(value)));
    };
    extractCoordinatedValueListSerialization<CSSPropertyID::CSSPropertyMaskComposite>(extractorState, builder, context, extractorState.style.maskLayers(), mapper);
}

inline Ref<CSSValue> ExtractorCustom::extractWebkitMaskSourceType(ExtractorState& extractorState)
{
    auto mapper = [](auto&, const auto& value, const std::optional<MaskLayers::value_type>&, const auto&) -> Ref<CSSValue> {
        return CSSPrimitiveValue::create(toCSSValueIDForWebkitMaskSourceType(value));
    };
    return extractCoordinatedValueListValue<CSSPropertyID::CSSPropertyMaskMode>(extractorState, extractorState.style.maskLayers(), mapper);
}

inline void ExtractorCustom::extractWebkitMaskSourceTypeSerialization(ExtractorState& extractorState, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto mapper = [](auto&, auto& builder, const auto&, const auto& value, const std::optional<MaskLayers::value_type>&, const auto&) {
        builder.append(nameLiteralForSerialization(toCSSValueIDForWebkitMaskSourceType(value)));
    };
    extractCoordinatedValueListSerialization<CSSPropertyID::CSSPropertyMaskMode>(extractorState, builder, context, extractorState.style.maskLayers(), mapper);
}

// MARK: - Shorthands

inline Ref<CSSValue> convertSingleAnimation(ExtractorState& state, const Animation& animation, const Animations& animations)
{
    static NeverDestroyed<EasingFunction> initialTimingFunction(Animation::initialTimingFunction());
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

    auto name = [&] -> String {
        if (auto keyframesName = animation.name().tryKeyframesName())
            return keyframesName->name;
        return nullString();
    }();

    auto showsTimingFunction = [&] {
        if (animation.timingFunction() != initialTimingFunction.get())
            return true;
        return name == ease || name == easeIn || name == easeInOut || name == easeOut || name == linear || name == stepEnd || name == stepStart;
    };

    auto showsIterationCount = [&] {
        if (animation.iterationCount() != Animation::initialIterationCount())
            return true;
        return name == infinite;
    };

    auto showsDirection = [&] {
        if (animation.direction() != Animation::initialDirection())
            return true;
        return name == normal || name == reverse || name == alternate || name == alternateReverse;
    };

    auto showsFillMode = [&] {
        if (animation.fillMode() != Animation::initialFillMode())
            return true;
        return name == forwards || name == backwards || name == both;
    };

    auto showsPlaysState = [&] {
        if (animation.playState() != Animation::initialPlayState())
            return true;
        return name == running || name == paused;
    };

    CSSValueListBuilder list;
    if (showsDuration)
        list.append(convertSingleAnimationDuration(state, animation.duration(), animation, animations));
    if (showsTimingFunction())
        list.append(createCSSValue(state.pool, state.style, animation.timingFunction()));
    if (showsDelay)
        list.append(createCSSValue(state.pool, state.style, animation.delay()));
    if (showsIterationCount())
        list.append(createCSSValue(state.pool, state.style, animation.iterationCount()));
    if (showsDirection())
        list.append(createCSSValue(state.pool, state.style, animation.direction()));
    if (showsFillMode())
        list.append(createCSSValue(state.pool, state.style, animation.fillMode()));
    if (showsPlaysState())
        list.append(createCSSValue(state.pool, state.style, animation.playState()));
    if (animation.name() != Animation::initialName())
        list.append(createCSSValue(state.pool, state.style, animation.name()));
    if (animation.timeline() != Animation::initialTimeline())
        list.append(createCSSValue(state.pool, state.style, animation.timeline()));
    if (animation.compositeOperation() != Animation::initialCompositeOperation())
        list.append(createCSSValue(state.pool, state.style, animation.compositeOperation()));
    if (list.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline RefPtr<CSSValue> ExtractorCustom::extractAnimationShorthand(ExtractorState& state)
{
    auto& animations = state.style.animations();
    if (animations.isInitial())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    CSSValueListBuilder list;
    for (auto& animation : animations.computedValues()) {
        // If any of the reset-only longhands are set, we cannot serialize this value.
        if (animation.isTimelineSet() || animation.isRangeStartSet() || animation.isRangeEndSet()) {
            list.clear();
            break;
        }
        list.append(convertSingleAnimation(state, animation, animations));
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractAnimationShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& animations = state.style.animations();
    if (animations.isInitial()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    for (auto& animation : animations.computedValues()) {
        // If any of the reset-only longhands are set, we cannot serialize this value.
        if (animation.isTimelineSet() || animation.isRangeStartSet() || animation.isRangeEndSet())
            return;
    }

    builder.append(interleave(animations.computedValues(), [&](auto& builder, const auto& animation) {
        // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
        builder.append(convertSingleAnimation(state, animation, animations)->cssText(context));
    }, ", "_s));
}

static Ref<CSSValueList> convertAnimationRange(ExtractorState& state, const SingleAnimationRange& range)
{
    CSSValueListBuilder list;

    auto createRangeValue = [&](auto& edge) -> Ref<CSSValueList> {
        Ref value = createCSSValue(state.pool, state.style, edge);
        if (auto list = dynamicDowncast<CSSValueList>(value))
            return list.releaseNonNull();
        return CSSValueList::createSpaceSeparated(WTFMove(value));
    };

    Ref startValue = createRangeValue(range.start);
    Ref endValue = createRangeValue(range.end);
    bool endValueEqualsStart = startValue->equals(endValue);

    if (startValue->length())
        list.append(WTFMove(startValue));

    bool isNormal = range.end.isNormal();
    bool isDefaultAndSameNameAsStart = range.start.name() == range.end.name() && range.end.hasDefaultOffset();
    if (endValue->length() && !endValueEqualsStart && !isNormal && !isDefaultAndSameNameAsStart)
        list.append(WTFMove(endValue));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline RefPtr<CSSValue> ExtractorCustom::extractAnimationRangeShorthand(ExtractorState& state)
{
    auto mapper = [](auto& state, const auto& value, const std::optional<Animation>&, const auto&) -> Ref<CSSValue> {
        return convertAnimationRange(state, value);
    };
    return extractCoordinatedValueListValue<CSSPropertyAnimationRange>(state, state.style.animations(), mapper);
}

inline void ExtractorCustom::extractAnimationRangeShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto mapper = [&](auto& state, auto& builder, const auto& context, const auto& value, const std::optional<Animation>&, const auto&) {
        // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
        builder.append(convertAnimationRange(state, value)->cssText(context));
    };
    return extractCoordinatedValueListSerialization<CSSPropertyAnimationRange>(state, builder, context, state.style.animations(), mapper);
}

inline RefPtr<CSSValue> ExtractorCustom::extractBackgroundShorthand(ExtractorState& state)
{
    static constexpr std::array propertiesBeforeSlashSeparator { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat, CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition };
    static constexpr std::array propertiesAfterSlashSeparator { CSSPropertyBackgroundSize, CSSPropertyBackgroundOrigin, CSSPropertyBackgroundClip };

    return extractFillLayerPropertyShorthand<CSSPropertyBackground>(
        state,
        StylePropertyShorthand(CSSPropertyBackground, std::span { propertiesBeforeSlashSeparator }),
        StylePropertyShorthand(CSSPropertyBackground, std::span { propertiesAfterSlashSeparator }),
        CSSPropertyBackgroundColor
    );
}

inline void ExtractorCustom::extractBackgroundShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    static constexpr std::array propertiesBeforeSlashSeparator { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat, CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition };
    static constexpr std::array propertiesAfterSlashSeparator { CSSPropertyBackgroundSize, CSSPropertyBackgroundOrigin, CSSPropertyBackgroundClip };

    extractFillLayerPropertyShorthandSerialization<CSSPropertyBackground>(
        state,
        builder,
        context,
        StylePropertyShorthand(CSSPropertyBackground, std::span { propertiesBeforeSlashSeparator }),
        StylePropertyShorthand(CSSPropertyBackground, std::span { propertiesAfterSlashSeparator }),
        CSSPropertyBackgroundColor
    );
}

inline RefPtr<CSSValue> ExtractorCustom::extractBackgroundPositionShorthand(ExtractorState& state)
{
    auto mapper = [](auto& state, const auto& value, const std::optional<BackgroundLayer>&, const auto&) -> Ref<CSSValue> {
        return createCSSValue(state.pool, state.style, value);
    };
    return extractCoordinatedValueListValue<CSSPropertyBackgroundPosition>(state, state.style.backgroundLayers(), mapper);
}

inline void ExtractorCustom::extractBackgroundPositionShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto mapper = [](auto& state, auto& builder, const auto& context, const auto& value, const std::optional<BackgroundLayer>&, const auto&) {
        serializationForCSS(builder, context, state.style, value);
    };
    extractCoordinatedValueListSerialization<CSSPropertyBackgroundPosition>(state, builder, context, state.style.backgroundLayers(), mapper);
}

inline RefPtr<CSSValue> ExtractorCustom::extractBlockStepShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyBlockStep>(state);
}

inline void ExtractorCustom::extractBlockStepShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyBlockStep>(state, builder, context);
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
    if (borderImage.source().isNone())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
    if (borderImage.overridesBorderWidths())
        return nullptr;
    return createCSSValue(state.pool, state.style, borderImage);
}

inline void ExtractorCustom::extractBorderImageShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& borderImage = state.style.borderImage();
    if (borderImage.source().isNone()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }
    if (borderImage.overridesBorderWidths())
        return;
    serializationForCSS(builder, context, state.style, borderImage);
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
    if (state.style.columnCount() == RenderStyle::initialColumnCount())
        return createCSSValue(state.pool, state.style, state.style.columnWidth());
    if (state.style.columnWidth() == RenderStyle::initialColumnWidth())
        return createCSSValue(state.pool, state.style, state.style.columnCount());
    return extractStandardSpaceSeparatedShorthand(state, columnsShorthand());
}

inline void ExtractorCustom::extractColumnsShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (state.style.columnCount() == RenderStyle::initialColumnCount()) {
        serializationForCSS(builder, context, state.style, state.style.columnWidth());
        return;
    }
    if (state.style.columnWidth() == RenderStyle::initialColumnWidth()) {
        serializationForCSS(builder, context, state.style, state.style.columnCount());
        return;
    }
    extractStandardSpaceSeparatedShorthandSerialization(state, builder, context, columnsShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractContainerShorthand(ExtractorState& state)
{
    auto name = [&]() -> Ref<CSSValue> {
        if (state.style.containerNames().isNone())
            return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
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
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
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
        return createCSSValue(state.pool, state.style, state.style.flexDirection());
    if (state.style.flexDirection() == RenderStyle::initialFlexDirection())
        return createCSSValue(state.pool, state.style, state.style.flexWrap());
    return extractStandardSpaceSeparatedShorthand(state, flexFlowShorthand());
}

inline void ExtractorCustom::extractFlexFlowShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    if (state.style.flexWrap() == RenderStyle::initialFlexWrap()) {
        serializationForCSS(builder, context, state.style, state.style.flexDirection());
        return;
    }
    if (state.style.flexDirection() == RenderStyle::initialFlexDirection()) {
        serializationForCSS(builder, context, state.style, state.style.flexWrap());
        return;
    }
    extractStandardSpaceSeparatedShorthandSerialization(state, builder, context, flexFlowShorthand());
}

inline RefPtr<CSSValue> ExtractorCustom::extractFontShorthand(ExtractorState& state)
{
    auto& description = state.style.fontDescription();
    auto fontWidth = fontWidthKeyword(description.width());
    auto fontStyle = fontStyleKeyword(description.fontStyleSlope(), description.fontStyleAxis());

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

    computedFont->family = createCSSValue(state.pool, state.style, state.style.fontFamily());

    return computedFont;
}

inline void ExtractorCustom::extractFontShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractFontShorthand(state)->cssText(context));
}

inline RefPtr<CSSValue> ExtractorCustom::extractFontSynthesisShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyFontSynthesis>(state);
}

inline void ExtractorCustom::extractFontSynthesisShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyFontSynthesis>(state, builder, context);
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
        return createCSSValue(state.pool, state.style, CSS::Keyword::Normal { });
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractFontVariantShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(extractFontVariantShorthand(state)->cssText(context));
}

inline RefPtr<CSSValue> ExtractorCustom::extractLineClampShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyLineClamp>(state);
}

inline void ExtractorCustom::extractLineClampShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyLineClamp>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractMaskShorthand(ExtractorState& state)
{
    static constexpr std::array propertiesBeforeSlashSeparator { CSSPropertyMaskImage, CSSPropertyMaskPosition };
    static constexpr std::array propertiesAfterSlashSeparator { CSSPropertyMaskSize, CSSPropertyMaskRepeat, CSSPropertyMaskOrigin, CSSPropertyMaskClip, CSSPropertyMaskComposite, CSSPropertyMaskMode };

    return extractFillLayerPropertyShorthand<CSSPropertyMask>(
        state,
        StylePropertyShorthand(CSSPropertyMask, std::span { propertiesBeforeSlashSeparator }),
        StylePropertyShorthand(CSSPropertyMask, std::span { propertiesAfterSlashSeparator }),
        CSSPropertyInvalid
    );
}

inline void ExtractorCustom::extractMaskShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    static constexpr std::array propertiesBeforeSlashSeparator { CSSPropertyMaskImage, CSSPropertyMaskPosition };
    static constexpr std::array propertiesAfterSlashSeparator { CSSPropertyMaskSize, CSSPropertyMaskRepeat, CSSPropertyMaskOrigin, CSSPropertyMaskClip, CSSPropertyMaskComposite, CSSPropertyMaskMode };

    extractFillLayerPropertyShorthandSerialization<CSSPropertyMask>(
        state,
        builder,
        context,
        StylePropertyShorthand(CSSPropertyMask, std::span { propertiesBeforeSlashSeparator }),
        StylePropertyShorthand(CSSPropertyMask, std::span { propertiesAfterSlashSeparator }),
        CSSPropertyInvalid
    );
}

inline RefPtr<CSSValue> ExtractorCustom::extractMaskBorderShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyMaskBorder>(state);
}

inline void ExtractorCustom::extractMaskBorderShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyMaskBorder>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractMaskPositionShorthand(ExtractorState& state)
{
    auto mapper = [](auto& state, const auto& value, const std::optional<MaskLayer>&, const auto&) -> Ref<CSSValue> {
        return createCSSValue(state.pool, state.style, value);
    };
    return extractCoordinatedValueListValue<CSSPropertyMaskPosition>(state, state.style.maskLayers(), mapper);
}

inline void ExtractorCustom::extractMaskPositionShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto mapper = [](auto& state, auto& builder, const auto& context, const auto& value, const std::optional<MaskLayer>&, const auto&) {
        serializationForCSS(builder, context, state.style, value);
    };
    extractCoordinatedValueListSerialization<CSSPropertyMaskPosition>(state, builder, context, state.style.maskLayers(), mapper);
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
            innerList.append(createCSSValue(state.pool, state.style, position));
        }
    );

    bool nonInitialDistance = state.style.offsetDistance() != state.style.initialOffsetDistance();
    bool nonInitialRotate = state.style.offsetRotate() != state.style.initialOffsetRotate();

    if (state.style.hasOffsetPath() || nonInitialDistance || nonInitialRotate)
        innerList.append(createCSSValue(state.pool, state.style, state.style.offsetPath()));

    if (nonInitialDistance)
        innerList.append(createCSSValue(state.pool, state.style, state.style.offsetDistance()));
    if (nonInitialRotate)
        innerList.append(createCSSValue(state.pool, state.style, state.style.offsetRotate()));

    auto inner = innerList.isEmpty()
        ? Ref<CSSValue> { createCSSValue(state.pool, state.style, CSS::Keyword::Auto { }) }
        : Ref<CSSValue> { CSSValueList::createSpaceSeparated(WTFMove(innerList)) };

    return WTF::switchOn(state.style.offsetAnchor(),
        [&](const CSS::Keyword::Auto&) -> Ref<CSSValue> {
            return inner;
        },
        [&](const Position& position) -> Ref<CSSValue> {
            return CSSValueList::createSlashSeparated(
                WTFMove(inner),
                createCSSValue(state.pool, state.style, position)
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
    return extractCSSValue<CSSPropertyOverscrollBehavior>(state);
}

inline void ExtractorCustom::extractOverscrollBehaviorShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyOverscrollBehavior>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractPageBreakAfterShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyPageBreakAfter>(state);
}

inline void ExtractorCustom::extractPageBreakAfterShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyPageBreakAfter>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractPageBreakBeforeShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyPageBreakBefore>(state);
}

inline void ExtractorCustom::extractPageBreakBeforeShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyPageBreakBefore>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractPageBreakInsideShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyPageBreakInside>(state);
}

inline void ExtractorCustom::extractPageBreakInsideShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyPageBreakInside>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractPerspectiveOriginShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyPerspectiveOrigin>(state);
}

inline void ExtractorCustom::extractPerspectiveOriginShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyPerspectiveOrigin>(state, builder, context);
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
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    CSSValueListBuilder list;
    for (auto& timeline : timelines) {
        auto& name = timeline->name();
        auto axis = timeline->axis();

        ASSERT(!name.isNull());
        auto nameCSSValue = createCSSValue(state.pool, state.style, CustomIdentifier { name });

        if (axis == ScrollAxis::Block)
            list.append(WTFMove(nameCSSValue));
        else
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, createCSSValue(state.pool, state.style, axis)));
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractScrollTimelineShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& timelines = state.style.scrollTimelines();
    if (timelines.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    builder.append(interleave(timelines, [&](auto& builder, auto& timeline) {
        ASSERT(!timeline->name().isNull());

        serializationForCSS(builder, context, state.style, CustomIdentifier { timeline->name() });
        if (auto axis = timeline->axis(); axis != ScrollAxis::Block) {
            builder.append(' ');
            serializationForCSS(builder, context, state.style, axis);
        }
    }, ", "_s));
}

inline RefPtr<CSSValue> ExtractorCustom::extractTextBoxShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyTextBox>(state);
}

inline void ExtractorCustom::extractTextBoxShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyTextBox>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractTextDecorationShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyTextDecoration>(state);
}

inline void ExtractorCustom::extractTextDecorationShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyTextDecoration>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractTextDecorationSkipShorthand(ExtractorState& state)
{
    switch (state.style.textDecorationSkipInk()) {
    case TextDecorationSkipInk::None:
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
    case TextDecorationSkipInk::Auto:
        return createCSSValue(state.pool, state.style, CSS::Keyword::Auto { });
    case TextDecorationSkipInk::All:
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorCustom::extractTextDecorationSkipShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    switch (state.style.textDecorationSkipInk()) {
    case TextDecorationSkipInk::None:
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    case TextDecorationSkipInk::Auto:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    case TextDecorationSkipInk::All:
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline RefPtr<CSSValue> ExtractorCustom::extractTextWrapShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyTextWrap>(state);
}

inline void ExtractorCustom::extractTextWrapShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyTextWrap>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractTransformOriginShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyTransformOrigin>(state);
}

inline void ExtractorCustom::extractTransformOriginShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyTransformOrigin>(state, builder, context);
}

inline Ref<CSSValue> convertSingleTransition(ExtractorState& state, const Transition& transition)
{
    static NeverDestroyed<EasingFunction> initialTimingFunction(Transition::initialTimingFunction());

    // If we have a transition-delay but no transition-duration set, we must serialize
    // the transition-duration because they're both <time> values and transition-delay
    // comes first.
    auto showsDelay = transition.delay() != Transition::initialDelay();
    auto showsDuration = showsDelay || transition.duration() != Transition::initialDuration();

    CSSValueListBuilder list;
    if (transition.property() != Transition::initialProperty())
        list.append(createCSSValue(state.pool, state.style, transition.property()));
    if (showsDuration)
        list.append(createCSSValue(state.pool, state.style, transition.duration()));
    if (transition.timingFunction() != initialTimingFunction.get())
        list.append(createCSSValue(state.pool, state.style, transition.timingFunction()));
    if (showsDelay)
        list.append(createCSSValue(state.pool, state.style, transition.delay()));
    if (transition.behavior() != Transition::initialBehavior())
        list.append(createCSSValue(state.pool, state.style, transition.behavior()));
    if (list.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::All { });
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

inline RefPtr<CSSValue> ExtractorCustom::extractTransitionShorthand(ExtractorState& state)
{
    auto& transitions = state.style.transitions();
    if (transitions.isInitial())
        return createCSSValue(state.pool, state.style, CSS::Keyword::All { });

    CSSValueListBuilder list;
    for (auto& transition : transitions.computedValues())
        list.append(convertSingleTransition(state, transition));
    ASSERT(!list.isEmpty());
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

inline void ExtractorCustom::extractTransitionShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& transitions = state.style.transitions();
    if (transitions.isInitial()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::All { });
        return;
    }

    builder.append(interleave(transitions.computedValues(), [&](auto& builder, auto& transition) {
        // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
        builder.append(convertSingleTransition(state, transition)->cssText(context));
    }, ", "_s));
}

inline RefPtr<CSSValue> ExtractorCustom::extractViewTimelineShorthand(ExtractorState& state)
{
    auto& timelines = state.style.viewTimelines();
    if (timelines.isEmpty())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });

    CSSValueListBuilder list;
    for (auto& timeline : timelines) {
        auto& name = timeline->name();
        auto axis = timeline->axis();
        auto& insets = timeline->insets();

        auto hasDefaultAxis = axis == ScrollAxis::Block;
        auto hasDefaultInsets = insets.start().isAuto() && insets.end().isAuto();

        ASSERT(!name.isNull());
        auto nameCSSValue = createCSSValue(state.pool, state.style, CustomIdentifier { name });

        if (hasDefaultAxis && hasDefaultInsets)
            list.append(WTFMove(nameCSSValue));
        else if (hasDefaultAxis)
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, createCSSValue(state.pool, state.style, insets)));
        else if (hasDefaultInsets)
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, createCSSValue(state.pool, state.style, axis)));
        else {
            list.append(CSSValueList::createSpaceSeparated(
                WTFMove(nameCSSValue),
                createCSSValue(state.pool, state.style, axis),
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
    return extractCSSValue<CSSPropertyWhiteSpace>(state);
}

inline void ExtractorCustom::extractWhiteSpaceShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyWhiteSpace>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractWebkitBorderImageShorthand(ExtractorState& state)
{
    auto& borderImage = state.style.borderImage();
    if (borderImage.source().isNone())
        return createCSSValue(state.pool, state.style, CSS::Keyword::None { });
    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    bool overridesBorderWidths = borderImage.width().values.anyOf([](const auto& side) { return side.isFixed(); });
    if (overridesBorderWidths != borderImage.overridesBorderWidths())
        return nullptr;
    return createCSSValue(state.pool, state.style, borderImage);
}

inline void ExtractorCustom::extractWebkitBorderImageShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto& borderImage = state.style.borderImage();
    if (borderImage.source().isNone()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }
    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    bool overridesBorderWidths = borderImage.width().values.anyOf([](const auto& side) { return side.isFixed(); });
    if (overridesBorderWidths != borderImage.overridesBorderWidths())
        return;

    serializationForCSS(builder, context, state.style, borderImage);
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
    return extractCSSValue<CSSPropertyWebkitColumnBreakAfter>(state);
}

inline void ExtractorCustom::extractWebkitColumnBreakAfterShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyWebkitColumnBreakAfter>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractWebkitColumnBreakBeforeShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyWebkitColumnBreakBefore>(state);
}

inline void ExtractorCustom::extractWebkitColumnBreakBeforeShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyWebkitColumnBreakBefore>(state, builder, context);
}

inline RefPtr<CSSValue> ExtractorCustom::extractWebkitColumnBreakInsideShorthand(ExtractorState& state)
{
    return extractCSSValue<CSSPropertyWebkitColumnBreakInside>(state);
}

inline void ExtractorCustom::extractWebkitColumnBreakInsideShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    extractSerialization<CSSPropertyWebkitColumnBreakInside>(state, builder, context);
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

inline void ExtractorCustom::extractMarkerShorthandSerialization(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context)
{
    auto markerStart = state.style.markerStart();
    auto markerMid = state.style.markerMid();
    auto markerEnd = state.style.markerEnd();
    if (markerStart == markerMid && markerMid == markerEnd)
        serializationForCSS(builder, context, state.style, markerStart);
}

inline RefPtr<CSSValue> ExtractorCustom::extractMarkerShorthand(ExtractorState& state)
{
    auto markerStart = state.style.markerStart();
    auto markerMid = state.style.markerMid();
    auto markerEnd = state.style.markerEnd();
    if (markerStart == markerMid && markerMid == markerEnd)
        return createCSSValue(state.pool, state.style, markerStart);
    return nullptr;
}

} // namespace Style
} // namespace WebCore
