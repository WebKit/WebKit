/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "LayoutIntegrationLineLayout.h"

#include "BlockFormattingState.h"
#include "BlockLayoutState.h"
#include "ElementInlines.h"
#include "EventRegion.h"
#include "FormattingContextBoxIterator.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineContentCache.h"
#include "InlineDamage.h"
#include "InlineFormattingContext.h"
#include "InlineInvalidation.h"
#include "InlineItemsBuilder.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationCoverage.h"
#include "LayoutIntegrationInlineContentBuilder.h"
#include "LayoutIntegrationInlineContentPainter.h"
#include "LayoutIntegrationPagination.h"
#include "LayoutTreeBuilder.h"
#include "PaintInfo.h"
#include "PlacedFloats.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderElementInlines.h"
#include "RenderFrameSet.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderView.h"
#include "Settings.h"
#include "ShapeOutsideInfo.h"
#include "TextBreakingPositionCache.h"
#include <wtf/Assertions.h>
#include <wtf/Range.h>

namespace WebCore {
namespace LayoutIntegration {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(LayoutIntegration_LineLayout);

enum class TypeOfChangeForInvalidation : uint8_t {
    NodeInsertion,
    NodeRemoval,
    NodeMutation
};
static bool shouldInvalidateLineLayoutPathAfterChangeFor(const RenderBlockFlow& rootBlockContainer, const RenderObject& renderer, const LineLayout& lineLayout, TypeOfChangeForInvalidation typeOfChange)
{
    auto isSupportedRendererWithChange = [&](auto& renderer) {
        if (is<RenderText>(renderer))
            return true;
        if (!renderer.isInFlow())
            return false;
        if (is<RenderLineBreak>(renderer))
            return true;
        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer); renderBox && renderBox->hasRelativeDimensions())
            return false;
        if (is<RenderReplaced>(renderer))
            return typeOfChange == TypeOfChangeForInvalidation::NodeInsertion;
        if (auto* inlineRenderer = dynamicDowncast<RenderInline>(renderer))
            return typeOfChange == TypeOfChangeForInvalidation::NodeInsertion && !inlineRenderer->firstChild();
        return false;
    };
    if (!isSupportedRendererWithChange(renderer))
        return true;

    auto isSupportedParent = [&] {
        auto* parent = renderer.parent();
        // Content append under existing inline box is not yet supported.
        return is<RenderBlockFlow>(parent) || (is<RenderInline>(parent) && !parent->everHadLayout());
    };
    if (!isSupportedParent())
        return true;
    if (rootBlockContainer.containsFloats())
        return true;

    auto isBidiContent = [&] {
        if (lineLayout.contentNeedsVisualReordering())
            return true;
        if (auto* textRenderer = dynamicDowncast<RenderText>(renderer)) {
            auto hasStrongDirectionalityContent = textRenderer->hasStrongDirectionalityContent();
            if (!hasStrongDirectionalityContent) {
                hasStrongDirectionalityContent = Layout::TextUtil::containsStrongDirectionalityText(textRenderer->text());
                const_cast<RenderText*>(textRenderer)->setHasStrongDirectionalityContent(*hasStrongDirectionalityContent);
            }
            return *hasStrongDirectionalityContent;
        }
        if (is<RenderInline>(renderer)) {
            auto& style = renderer.style();
            return style.writingMode().isBidiRTL() || (style.rtlOrdering() == Order::Logical && style.unicodeBidi() != UnicodeBidi::Normal);
        }
        return false;
    };
    if (isBidiContent()) {
        // FIXME: InlineItemsBuilder needs some work to support paragraph level bidi handling.
        return true;
    }
    auto hasFirstLetter = [&] {
        // FIXME: RenderTreeUpdater::updateTextRenderer produces odd values for offset/length when first-letter is present webkit.org/b/263343
        if (rootBlockContainer.style().hasPseudoStyle(PseudoId::FirstLetter))
            return true;
        if (rootBlockContainer.isAnonymous())
            return rootBlockContainer.containingBlock() && rootBlockContainer.containingBlock()->style().hasPseudoStyle(PseudoId::FirstLetter);
        return false;
    };
    if (hasFirstLetter())
        return true;

    if (auto* previousDamage = lineLayout.damage(); previousDamage && (previousDamage->reasons() != Layout::InlineDamage::Reason::Append || !previousDamage->layoutStartPosition())) {
        // Only support subsequent append operations where we managed to invalidate the content for partial layout.
        return true;
    }

    auto shouldBalance = rootBlockContainer.style().textWrapMode() == TextWrapMode::Wrap && rootBlockContainer.style().textWrapStyle() == TextWrapStyle::Balance;
    auto shouldPrettify = rootBlockContainer.style().textWrapMode() == TextWrapMode::Wrap && rootBlockContainer.style().textWrapStyle() == TextWrapStyle::Pretty;
    if (rootBlockContainer.writingMode().isBidiRTL() || shouldBalance || shouldPrettify)
        return true;

    auto rootHasNonSupportedRenderer = [&] (bool shouldOnlyCheckForRelativeDimension = false) {
        for (auto* sibling = rootBlockContainer.firstChild(); sibling; sibling = sibling->nextSibling()) {
            auto siblingHasRelativeDimensions = false;
            if (auto* renderBox = dynamicDowncast<RenderBox>(*sibling); renderBox && renderBox->hasRelativeDimensions())
                siblingHasRelativeDimensions = true;

            if (shouldOnlyCheckForRelativeDimension && !siblingHasRelativeDimensions)
                continue;

            if (siblingHasRelativeDimensions || (!is<RenderText>(*sibling) && !is<RenderLineBreak>(*sibling) && !is<RenderReplaced>(*sibling)))
                return true;
        }
        return !canUseForLineLayout(rootBlockContainer);
    };
    switch (typeOfChange) {
    case TypeOfChangeForInvalidation::NodeRemoval:
        return (!renderer.previousSibling() && !renderer.nextSibling()) || rootHasNonSupportedRenderer();
    case TypeOfChangeForInvalidation::NodeInsertion:
        return rootHasNonSupportedRenderer(!renderer.nextSibling());
    case TypeOfChangeForInvalidation::NodeMutation:
        return rootHasNonSupportedRenderer();
    default:
        ASSERT_NOT_REACHED();
        return true;
    }
}

static inline std::pair<LayoutRect, LayoutRect> toMarginAndBorderBoxVisualRect(const Layout::BoxGeometry& logicalGeometry, LayoutUnit containerLogicalWidth, WritingMode writingMode)
{
    bool isHorizontalWritingMode = writingMode.isHorizontal();

    auto borderBoxLogicalRect = Layout::BoxGeometry::borderBoxRect(logicalGeometry);
    auto horizontalMargin = Layout::BoxGeometry::HorizontalEdges { logicalGeometry.marginStart(), logicalGeometry.marginEnd() };
    auto verticalMargin = Layout::BoxGeometry::VerticalEdges { logicalGeometry.marginBefore(), logicalGeometry.marginAfter() };

    auto flipMarginsIfApplicable = [&] {
        if (writingMode == WritingMode())
            return;

        if (!isHorizontalWritingMode) {
            auto logicalHorizontalMargin = horizontalMargin;
            horizontalMargin = !writingMode.isBlockFlipped()
                ? Layout::BoxGeometry::HorizontalEdges { verticalMargin.after, verticalMargin.before }
                : Layout::BoxGeometry::HorizontalEdges { verticalMargin.before, verticalMargin.after };
            verticalMargin = { logicalHorizontalMargin.start, logicalHorizontalMargin.end };
        }
        if (writingMode.isBidiRTL()) {
            if (isHorizontalWritingMode)
                horizontalMargin = { horizontalMargin.end, horizontalMargin.start };
            else
                verticalMargin = { verticalMargin.after, verticalMargin.before };
        }
    };
    flipMarginsIfApplicable();

    auto borderBoxVisualTopLeft = LayoutPoint { };
    auto borderBoxLeft = writingMode.isBidiLTR() ? borderBoxLogicalRect.left() : containerLogicalWidth - (borderBoxLogicalRect.left() + borderBoxLogicalRect.width());
    if (isHorizontalWritingMode)
        borderBoxVisualTopLeft = { borderBoxLeft, borderBoxLogicalRect.top() };
    else {
        auto marginBoxVisualLeft = borderBoxLogicalRect.top() - logicalGeometry.marginBefore();
        auto marginBoxVisualTop = borderBoxLeft - logicalGeometry.marginStart();
        if (writingMode.isBidiLTR())
            borderBoxVisualTopLeft = { marginBoxVisualLeft + horizontalMargin.start, marginBoxVisualTop + verticalMargin.before };
        else
            borderBoxVisualTopLeft = { marginBoxVisualLeft + horizontalMargin.start, marginBoxVisualTop + verticalMargin.after };
    }

    auto borderBoxVisualRect = LayoutRect { borderBoxVisualTopLeft, isHorizontalWritingMode ? borderBoxLogicalRect.size() : borderBoxLogicalRect.size().transposedSize() };
    auto marginBoxVisualRect = borderBoxVisualRect;

    marginBoxVisualRect.move(-horizontalMargin.start, -verticalMargin.before);
    marginBoxVisualRect.expand(horizontalMargin.start + horizontalMargin.end, verticalMargin.before + verticalMargin.after);
    return { marginBoxVisualRect, borderBoxVisualRect };
}

static const InlineDisplay::Line& lastLineWithInlineContent(const InlineDisplay::Lines& lines)
{
    // Out-of-flow/float content only don't produce lines with inline content. They should not be taken into
    // account when computing content box height/baselines.
    for (auto& line : makeReversedRange(lines)) {
        ASSERT(line.boxCount());
        if (line.boxCount() > 1)
            return line;
    }
    return lines.first();
}

LineLayout::LineLayout(RenderBlockFlow& flow)
    : m_rootLayoutBox(BoxTreeUpdater { flow }.build())
    , m_layoutState(flow.view().layoutState())
    , m_blockFormattingState(layoutState().ensureBlockFormattingState(rootLayoutBox()))
    , m_inlineContentCache(layoutState().inlineContentCache(rootLayoutBox()))
    , m_boxGeometryUpdater(flow.view().layoutState(), rootLayoutBox())
{
}

LineLayout::~LineLayout()
{
    auto& rootRenderer = flow();

    if (!isDamaged() && !rootRenderer.document().renderTreeBeingDestroyed())
        Layout::InlineItemsBuilder::populateBreakingPositionCache(m_inlineContentCache.inlineItems().content(), rootRenderer.document());
    clearInlineContent();
    layoutState().destroyInlineContentCache(rootLayoutBox());
    layoutState().destroyBlockFormattingState(rootLayoutBox());
    m_boxGeometryUpdater.clear();
    m_lineDamage = { };
    m_rootLayoutBox = nullptr;

    BoxTreeUpdater { rootRenderer }.tearDown();
}

static inline bool isContentRenderer(const RenderObject& renderer)
{
    // FIXME: These fake renderers have their parent set but are not actually in the tree.
    return !renderer.isRenderReplica() && !renderer.isRenderScrollbarPart();
}

RenderBlockFlow* LineLayout::blockContainer(const RenderObject& renderer)
{
    if (!isContentRenderer(renderer))
        return nullptr;

    for (auto* parent = renderer.parent(); parent; parent = parent->parent()) {
        if (!parent->childrenInline())
            return nullptr;
        if (auto* renderBlockFlow = dynamicDowncast<RenderBlockFlow>(*parent))
            return renderBlockFlow;
    }

    return nullptr;
}

bool LineLayout::contains(const RenderElement& renderer) const
{
    if (!renderer.layoutBox())
        return false;
    if (!renderer.layoutBox()->isInFormattingContextEstablishedBy(rootLayoutBox()))
        return false;
    return layoutState().hasBoxGeometry(*renderer.layoutBox());
}

LineLayout* LineLayout::containing(RenderObject& renderer)
{
    if (!isContentRenderer(renderer))
        return nullptr;

    if (!renderer.isInline()) {
        // IFC may contain block level boxes (floats and out-of-flow boxes).
        if (renderer.isRenderSVGBlock()) {
            // SVG content inside svg root shows up as block (see RenderSVGBlock). We only support inline root svg as "atomic content".
            return nullptr;
        }
        if (renderer.isRenderFrameSet()) {
            // Since RenderFrameSet is not a RenderBlock, finding container for nested framesets can't use containingBlock ancestor walk.
            if (auto* parent = dynamicDowncast<RenderBlockFlow>(renderer.parent()))
                return parent->inlineLayout();
            return nullptr;
        }
        auto adjustedContainingBlock = [&] {
            RenderElement* containingBlock = nullptr;
            // Only out of flow and floating block level boxes may participate in IFC.
            if (renderer.isOutOfFlowPositioned()) {
                // Here we are looking for the containing block as if the out-of-flow box was inflow (for static position purpose).
                containingBlock = renderer.parent();
                if (is<RenderInline>(containingBlock))
                    containingBlock = containingBlock->containingBlock();
            } else if (renderer.isFloating()) {
                // Note that containigBlock() on boxes in top layer (i.e. dialog) may return incorrect result during style change even with not-yet-updated style.
                containingBlock = RenderObject::containingBlockForPositionType(renderer.style().position(), renderer);
            }
            return dynamicDowncast<RenderBlockFlow>(containingBlock);
        };
        if (auto* blockContainer = adjustedContainingBlock())
            return blockContainer->inlineLayout();
        return nullptr;
    }

    if (auto* container = blockContainer(renderer))
        return container->inlineLayout();

    return nullptr;
}

const LineLayout* LineLayout::containing(const RenderObject& renderer)
{
    return containing(const_cast<RenderObject&>(renderer));
}

bool LineLayout::canUseFor(const RenderBlockFlow& flow)
{
    return canUseForLineLayout(flow);
}

bool LineLayout::canUseForPreferredWidthComputation(const RenderBlockFlow& flow)
{
    return LayoutIntegration::canUseForPreferredWidthComputation(flow);
}

bool LineLayout::shouldInvalidateLineLayoutPathAfterContentChange(const RenderBlockFlow& parent, const RenderObject& rendererWithNewContent, const LineLayout& lineLayout)
{
    return shouldInvalidateLineLayoutPathAfterChangeFor(parent, rendererWithNewContent, lineLayout, TypeOfChangeForInvalidation::NodeMutation);
}

bool LineLayout::shouldInvalidateLineLayoutPathAfterTreeMutation(const RenderBlockFlow& parent, const RenderObject& renderer, const LineLayout& lineLayout, bool isRemoval)
{
    return shouldInvalidateLineLayoutPathAfterChangeFor(parent, renderer, lineLayout, isRemoval ? TypeOfChangeForInvalidation::NodeRemoval : TypeOfChangeForInvalidation::NodeInsertion);
}

void LineLayout::updateFormattingContexGeometries(LayoutUnit availableLogicalWidth)
{
    m_boxGeometryUpdater.setFormattingContextRootGeometry(availableLogicalWidth);
    m_inlineContentConstraints = m_boxGeometryUpdater.formattingContextConstraints(availableLogicalWidth);
    m_boxGeometryUpdater.setFormattingContextContentGeometry(m_inlineContentConstraints->horizontal().logicalWidth, { });
}

void LineLayout::updateStyle(const RenderObject& renderer)
{
    BoxTreeUpdater::updateStyle(renderer);
}

bool LineLayout::rootStyleWillChange(const RenderBlockFlow& root, const RenderStyle& newStyle)
{
    if (!root.layoutBox() || !root.layoutBox()->isElementBox()) {
        ASSERT_NOT_REACHED();
        return false;
    }
    if (!m_inlineContent)
        return false;

    return Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() }.rootStyleWillChange(downcast<Layout::ElementBox>(*root.layoutBox()), newStyle);
}

bool LineLayout::styleWillChange(const RenderElement& renderer, const RenderStyle& newStyle, StyleDifference diff)
{
    if (!renderer.layoutBox()) {
        ASSERT_NOT_REACHED();
        return false;
    }
    if (!m_inlineContent)
        return false;

    return Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() }.styleWillChange(*renderer.layoutBox(), newStyle, diff);
}

bool LineLayout::boxContentWillChange(const RenderBox& renderer)
{
    if (!m_inlineContent || !renderer.layoutBox())
        return false;

    return Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() }.inlineLevelBoxContentWillChange(*renderer.layoutBox());
}

void LineLayout::updateOverflow()
{
    InlineContentBuilder { flow() }.updateLineOverflow(*m_inlineContent);
}

std::pair<LayoutUnit, LayoutUnit> LineLayout::computeIntrinsicWidthConstraints()
{
    auto parentBlockLayoutState = Layout::BlockLayoutState { m_blockFormattingState.placedFloats() };
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox(), layoutState(), parentBlockLayoutState };
    if (m_lineDamage)
        m_inlineContentCache.resetMinimumMaximumContentSizes();
    // FIXME: This is where we need to switch between minimum and maximum box geometries.
    // Currently we only support content where min == max.
    m_boxGeometryUpdater.setFormattingContextContentGeometry({ }, Layout::IntrinsicWidthMode::Minimum);
    auto [minimumContentSize, maximumContentSize] = inlineFormattingContext.minimumMaximumContentSize(m_lineDamage.get());
    return { minimumContentSize, maximumContentSize };
}

static inline std::optional<Layout::BlockLayoutState::LineClamp> lineClamp(const RenderBlockFlow& rootRenderer)
{
    auto& layoutState = *rootRenderer.view().frameView().layoutContext().layoutState();
    if (auto legacyLineClamp = layoutState.legacyLineClamp())
        return Layout::BlockLayoutState::LineClamp { std::max(legacyLineClamp->maximumLineCount - legacyLineClamp->currentLineCount, static_cast<size_t>(0)), false, true };
    if (auto lineClamp = layoutState.lineClamp())
        return Layout::BlockLayoutState::LineClamp { lineClamp->maximumLines, lineClamp->shouldDiscardOverflow, false };
    return { };
}

static inline Layout::BlockLayoutState::TextBoxTrim textBoxTrim(const RenderBlockFlow& rootRenderer)
{
    auto* layoutState = rootRenderer.view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return { };
    auto textBoxTrimForIFC = Layout::BlockLayoutState::TextBoxTrim { };
    auto isLineInverted = rootRenderer.writingMode().isLineInverted();
    if (layoutState->hasTextBoxTrimStart())
        textBoxTrimForIFC.add(isLineInverted ? Layout::BlockLayoutState::TextBoxTrimSide::End : Layout::BlockLayoutState::TextBoxTrimSide::Start);
    if (layoutState->hasTextBoxTrimEnd(rootRenderer))
        textBoxTrimForIFC.add(isLineInverted ? Layout::BlockLayoutState::TextBoxTrimSide::Start : Layout::BlockLayoutState::TextBoxTrimSide::End);
    return textBoxTrimForIFC;
}

static inline std::optional<Layout::BlockLayoutState::LineGrid> lineGrid(const RenderBlockFlow& rootRenderer)
{
    auto& layoutState = *rootRenderer.view().frameView().layoutContext().layoutState();
    if (auto* lineGrid = layoutState.lineGrid()) {
        if (lineGrid->writingMode().computedWritingMode() != rootRenderer.writingMode().computedWritingMode())
            return { };

        auto layoutOffset = layoutState.layoutOffset();
        auto lineGridOffset = layoutState.lineGridOffset();
        if (lineGrid->style().writingMode().isVertical()) {
            layoutOffset = layoutOffset.transposedSize();
            lineGridOffset = lineGridOffset.transposedSize();
        }

        auto columnWidth = lineGrid->style().fontCascade().primaryFont().maxCharWidth();
        auto rowHeight = LayoutUnit::fromFloatCeil(lineGrid->style().computedLineHeight());
        auto topRowOffset = lineGrid->borderAndPaddingBefore();

        std::optional<LayoutSize> paginationOrigin;
        auto pageLogicalTop = 0_lu;
        if (layoutState.isPaginated()) {
            paginationOrigin = layoutState.lineGridPaginationOrigin();
            if (lineGrid->writingMode().isVertical())
                paginationOrigin = paginationOrigin->transposedSize();
            pageLogicalTop = rootRenderer.pageLogicalTopForOffset(0_lu);
        }

        return Layout::BlockLayoutState::LineGrid { layoutOffset, lineGridOffset, columnWidth, rowHeight, topRowOffset, lineGrid->style().fontCascade().primaryFont(), paginationOrigin, pageLogicalTop };
    }

    return { };
}

std::optional<LayoutRect> LineLayout::layout()
{
    preparePlacedFloats();

    auto isPartialLayout = Layout::InlineInvalidation::mayOnlyNeedPartialLayout(m_lineDamage.get());
    if (!isPartialLayout) {
        // FIXME: Partial layout should not rely on previous inline display content.
        clearInlineContent();
    }

    ASSERT(m_inlineContentConstraints);
    auto intrusiveInitialLetterBottom = [&]() -> std::optional<LayoutUnit> {
        if (auto lowestInitialLetterLogicalBottom = flow().lowestInitialLetterLogicalBottom())
            return { *lowestInitialLetterLogicalBottom - m_inlineContentConstraints->logicalTop() };
        return { };
    };
    auto inlineContentConstraints = [&]() -> Layout::ConstraintsForInlineContent {
        if (!isPartialLayout || !m_inlineContent)
            return *m_inlineContentConstraints;
        auto damagedLineIndex = m_lineDamage->layoutStartPosition()->lineIndex;
        if (!damagedLineIndex)
            return *m_inlineContentConstraints;
        if (damagedLineIndex >= m_inlineContent->displayContent().lines.size()) {
            ASSERT_NOT_REACHED();
            return *m_inlineContentConstraints;
        }
        auto constraintsForInFlowContent = Layout::ConstraintsForInFlowContent { m_inlineContentConstraints->horizontal(), m_lineDamage->layoutStartPosition()->partialContentTop };
        return { constraintsForInFlowContent, m_inlineContentConstraints->visualLeft() };
    };

    auto parentBlockLayoutState = Layout::BlockLayoutState {
        m_blockFormattingState.placedFloats(),
        lineClamp(flow()),
        textBoxTrim(flow()),
        flow().style().textBoxEdge(),
        intrusiveInitialLetterBottom(),
        lineGrid(flow())
    };
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox(), layoutState(), parentBlockLayoutState };
    // Temporary, integration only.
    inlineFormattingContext.layoutState().setNestedListMarkerOffsets(m_boxGeometryUpdater.takeNestedListMarkerOffsets());

    auto layoutResult = inlineFormattingContext.layout(inlineContentConstraints(), m_lineDamage.get());
    auto repaintRect = LayoutRect { constructContent(inlineFormattingContext.layoutState(), WTFMove(layoutResult)) };

    m_lineDamage = { };

    auto adjustments = adjustContentForPagination(parentBlockLayoutState, isPartialLayout);

    updateRenderTreePositions(adjustments, inlineFormattingContext.layoutState());

    if (m_lineDamage) {
        // Pagination may require another layout pass.
        layout();

        ASSERT(!m_lineDamage);
    }

    return isPartialLayout ? std::make_optional(repaintRect) : std::nullopt;
}

FloatRect LineLayout::constructContent(const Layout::InlineLayoutState& inlineLayoutState, Layout::InlineLayoutResult&& layoutResult)
{
    auto damagedRect = InlineContentBuilder { flow() }.build(WTFMove(layoutResult), ensureInlineContent(), m_lineDamage.get());

    m_inlineContent->clearGapBeforeFirstLine = inlineLayoutState.clearGapBeforeFirstLine();
    m_inlineContent->clearGapAfterLastLine = inlineLayoutState.clearGapAfterLastLine();
    m_inlineContent->shrinkToFit();

    m_inlineContentCache.inlineItems().shrinkToFit();
    m_blockFormattingState.shrinkToFit();

    // FIXME: These needs to be incorporated into the partial damage.
    auto additionalHeight = m_inlineContent->firstLinePaginationOffset + m_inlineContent->clearGapBeforeFirstLine + m_inlineContent->clearGapAfterLastLine;
    damagedRect.expand({ 0, additionalHeight });
    return damagedRect;
}

void LineLayout::updateRenderTreePositions(const Vector<LineAdjustment>& lineAdjustments, const Layout::InlineLayoutState& inlineLayoutState)
{
    if (!m_inlineContent)
        return;

    auto& blockFlow = flow();
    auto placedFloatsWritingMode = m_blockFormattingState.placedFloats().blockFormattingContextRoot().style().writingMode();

    auto visualAdjustmentOffset = [&](auto lineIndex) {
        if (lineAdjustments.isEmpty())
            return LayoutSize { };
        if (!placedFloatsWritingMode.isHorizontal())
            return LayoutSize { lineAdjustments[lineIndex].offset, 0_lu };
        return LayoutSize { 0_lu, lineAdjustments[lineIndex].offset };
    };

    for (auto& box : m_inlineContent->displayContent().boxes) {
        if (box.isInlineBox() || box.isText())
            continue;

        auto& layoutBox = box.layoutBox();
        if (!layoutBox.isAtomicInlineBox())
            continue;

        auto& renderer = downcast<RenderBox>(*box.layoutBox().rendererForIntegration());
        if (auto* layer = renderer.layer())
            layer->setIsHiddenByOverflowTruncation(box.isFullyTruncated());

        renderer.setLocation(Layout::toLayoutPoint(box.visualRectIgnoringBlockDirection().location()));
    }

    UncheckedKeyHashMap<CheckedRef<const Layout::Box>, LayoutSize> floatPaginationOffsetMap;
    if (!lineAdjustments.isEmpty()) {
        for (auto& floatItem : m_blockFormattingState.placedFloats().list()) {
            if (!floatItem.layoutBox() || !floatItem.placedByLine())
                continue;
            auto adjustmentOffset = visualAdjustmentOffset(*floatItem.placedByLine());
            floatPaginationOffsetMap.add(*floatItem.layoutBox(), adjustmentOffset);
        }
    }

    for (auto& layoutBox : formattingContextBoxes(rootLayoutBox())) {
        if (!layoutBox.isFloatingPositioned() && !layoutBox.isOutOfFlowPositioned())
            continue;
        if (layoutBox.isLineBreakBox())
            continue;
        auto& renderer = downcast<RenderBox>(*layoutBox.rendererForIntegration());
        auto& logicalGeometry = layoutState().geometryForBox(layoutBox);

        if (layoutBox.isFloatingPositioned()) {
            auto isInitialLetter = layoutBox.style().pseudoElementType() == PseudoId::FirstLetter;
            auto& floatingObject = flow().insertFloatingObjectForIFC(renderer);
            auto containerLogicalWidth = m_inlineContentConstraints->visualLeft() + m_inlineContentConstraints->horizontal().logicalWidth + m_inlineContentConstraints->horizontal().logicalLeft;
            auto [marginBoxVisualRect, borderBoxVisualRect] = toMarginAndBorderBoxVisualRect(logicalGeometry, containerLogicalWidth, placedFloatsWritingMode);

            auto paginationOffset = floatPaginationOffsetMap.getOptional(layoutBox);
            if (paginationOffset) {
                marginBoxVisualRect.move(*paginationOffset);
                borderBoxVisualRect.move(*paginationOffset);
            }
            if (isInitialLetter) {
                auto firstLineTrim = LayoutUnit { inlineLayoutState.firstLineStartTrimForInitialLetter() };
                marginBoxVisualRect.move(0_lu, -firstLineTrim);
                borderBoxVisualRect.move(0_lu, -firstLineTrim);
            }

            floatingObject.setFrameRect(marginBoxVisualRect);
            floatingObject.setMarginOffset({ borderBoxVisualRect.x() - marginBoxVisualRect.x(), borderBoxVisualRect.y() - marginBoxVisualRect.y() });
            floatingObject.setIsPlaced(true);

            auto oldRect = renderer.frameRect();
            renderer.setLocation(borderBoxVisualRect.location());

            if (renderer.checkForRepaintDuringLayout()) {
                auto hasMoved = oldRect.location() != renderer.location();
                if (hasMoved)
                    renderer.repaintDuringLayoutIfMoved(oldRect);
                else
                    renderer.repaint();
            }

            if (paginationOffset) {
                // Float content may be affected by the new position.
                renderer.markForPaginationRelayoutIfNeeded();
                renderer.layoutIfNeeded();
            }

            continue;
        }

        if (layoutBox.isOutOfFlowPositioned()) {
            ASSERT(renderer.layer());
            auto& layer = *renderer.layer();
            auto borderBoxLogicalTopLeft = Layout::BoxGeometry::borderBoxRect(logicalGeometry).topLeft();
            auto previousStaticPosition = LayoutPoint { layer.staticInlinePosition(), layer.staticBlockPosition() };
            auto delta = borderBoxLogicalTopLeft - previousStaticPosition;
            auto hasStaticInlinePositioning = layoutBox.style().hasStaticInlinePosition(renderer.isHorizontalWritingMode());

            if (layoutBox.style().isOriginalDisplayInlineType()) {
                blockFlow.setStaticInlinePositionForChild(renderer, borderBoxLogicalTopLeft.y(), borderBoxLogicalTopLeft.x());
                if (hasStaticInlinePositioning)
                    renderer.move(delta.width(), delta.height());
            }

            layer.setStaticBlockPosition(borderBoxLogicalTopLeft.y());
            layer.setStaticInlinePosition(borderBoxLogicalTopLeft.x());

            if (!delta.isZero() && hasStaticInlinePositioning)
                renderer.setChildNeedsLayout(MarkOnlyThis);
            continue;
        }
    }
}

void LineLayout::preparePlacedFloats()
{
    auto& placedFloats = m_blockFormattingState.placedFloats();
    placedFloats.clear();

    if (!flow().containsFloats())
        return;

    auto placedFloatsWritingMode = placedFloats.blockFormattingContextRoot().style().writingMode();
    auto placedFloatsIsLeftToRight = placedFloatsWritingMode.isBidiLTR();
    auto isHorizontalWritingMode = placedFloatsWritingMode.isHorizontal();
    for (auto& floatingObject : *flow().floatingObjectSet()) {
        auto& visualRect = floatingObject->frameRect();
        auto logicalPosition = [&] {
            switch (floatingObject->renderer().style().floating()) {
            case Float::Left:
                return placedFloatsIsLeftToRight ? Layout::PlacedFloats::Item::Position::Start : Layout::PlacedFloats::Item::Position::End;
            case Float::Right:
                return placedFloatsIsLeftToRight ? Layout::PlacedFloats::Item::Position::End : Layout::PlacedFloats::Item::Position::Start;
            case Float::InlineStart: {
                auto* floatBoxContainingBlock = floatingObject->renderer().containingBlock();
                if (floatBoxContainingBlock && placedFloatsWritingMode.isInlineOpposing(floatBoxContainingBlock->writingMode()))
                    return Layout::PlacedFloats::Item::Position::End;
                return Layout::PlacedFloats::Item::Position::Start;
            }
            case Float::InlineEnd: {
                auto* floatBoxContainingBlock = floatingObject->renderer().containingBlock();
                if (floatBoxContainingBlock && placedFloatsWritingMode.isInlineOpposing(floatBoxContainingBlock->writingMode()))
                    return Layout::PlacedFloats::Item::Position::Start;
                return Layout::PlacedFloats::Item::Position::End;
            }
            default:
                ASSERT_NOT_REACHED();
                return Layout::PlacedFloats::Item::Position::Start;
            }
        };

        auto boxGeometry = Layout::BoxGeometry { };
        auto logicalRect = [&] {
            // FIXME: We are flooring here for legacy compatibility. See FloatingObjects::intervalForFloatingObject and RenderBlockFlow::clearFloats.
            auto logicalTop = isHorizontalWritingMode ? LayoutUnit(visualRect.y().floor()) : visualRect.x();
            auto logicalLeft = isHorizontalWritingMode ? visualRect.x() : LayoutUnit(visualRect.y().floor());
            auto logicalHeight = (isHorizontalWritingMode ? LayoutUnit(visualRect.maxY().floor()) : visualRect.maxX()) - logicalTop;
            auto logicalWidth = (isHorizontalWritingMode ? visualRect.maxX() : LayoutUnit(visualRect.maxY().floor())) - logicalLeft;
            if (!placedFloatsIsLeftToRight) {
                auto rootBorderBoxWidth = m_inlineContentConstraints->visualLeft() + m_inlineContentConstraints->horizontal().logicalWidth + m_inlineContentConstraints->horizontal().logicalLeft;
                logicalLeft = rootBorderBoxWidth - (logicalLeft + logicalWidth);
            }
            return LayoutRect { logicalLeft, logicalTop, logicalWidth, logicalHeight };
        }();

        boxGeometry.setTopLeft(logicalRect.location());
        boxGeometry.setContentBoxWidth(logicalRect.width());
        boxGeometry.setContentBoxHeight(logicalRect.height());
        boxGeometry.setBorder({ });
        boxGeometry.setPadding({ });
        boxGeometry.setHorizontalMargin({ });
        boxGeometry.setVerticalMargin({ });

        auto shapeOutsideInfo = floatingObject->renderer().shapeOutsideInfo();
        auto* shape = shapeOutsideInfo ? &shapeOutsideInfo->computedShape() : nullptr;

        placedFloats.append({ logicalPosition(), boxGeometry, logicalRect.location(), shape });
    }
}

bool LineLayout::isPaginated() const
{
    return m_inlineContent && m_inlineContent->isPaginated;
}

std::optional<LayoutUnit> LineLayout::clampedContentLogicalHeight() const
{
    if (!m_inlineContent)
        return { };

    auto& lines = m_inlineContent->displayContent().lines;
    if (lines.isEmpty()) {
        // Out-of-flow only content (and/or with floats) may produce blank inline content.
        return { };
    }

    auto firstTruncatedLineIndex = [&]() -> std::optional<size_t> {
        for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            if (lines[lineIndex].isFullyTruncatedInBlockDirection())
                return lineIndex;
        }
        return { };
    }();
    if (!firstTruncatedLineIndex)
        return { };
    if (!*firstTruncatedLineIndex) {
        // This content is fully truncated in the block direction.
        return LayoutUnit { };
    }

    auto contentHeight = lines[*firstTruncatedLineIndex - 1].lineBoxLogicalRect().maxY() - lines.first().lineBoxLogicalRect().y();
    auto additionalHeight = m_inlineContent->firstLinePaginationOffset + m_inlineContent->clearGapBeforeFirstLine + m_inlineContent->clearGapAfterLastLine;
    return LayoutUnit { contentHeight + additionalHeight };
}

LayoutUnit LineLayout::contentBoxLogicalHeight() const
{
    if (!m_inlineContent)
        return { };

    auto& lines = m_inlineContent->displayContent().lines;
    if (lines.isEmpty()) {
        // Out-of-flow only content (and/or with floats) may produce blank inline content.
        return { };
    }

    auto contentHeight = lastLineWithInlineContent(lines).lineBoxLogicalRect().maxY() - lines.first().lineBoxLogicalRect().y();
    auto additionalHeight = m_inlineContent->firstLinePaginationOffset + m_inlineContent->clearGapBeforeFirstLine + m_inlineContent->clearGapAfterLastLine;
    return LayoutUnit { contentHeight + additionalHeight };
}

size_t LineLayout::lineCount() const
{
    if (!m_inlineContent)
        return 0;
    if (!m_inlineContent->hasContent())
        return 0;

    return m_inlineContent->displayContent().lines.size();
}

bool LineLayout::hasVisualOverflow() const
{
    return m_inlineContent && m_inlineContent->hasVisualOverflow();
}

LayoutUnit LineLayout::firstLinePhysicalBaseline() const
{
    if (!m_inlineContent || m_inlineContent->displayContent().boxes.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto& firstLine = m_inlineContent->displayContent().lines.first();
    return physicalBaselineForLine(firstLine);
}

LayoutUnit LineLayout::lastLinePhysicalBaseline() const
{
    if (!m_inlineContent || m_inlineContent->displayContent().lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }
    return physicalBaselineForLine(lastLineWithInlineContent(m_inlineContent->displayContent().lines));
}

LayoutUnit LineLayout::physicalBaselineForLine(const InlineDisplay::Line& line) const
{
    switch (rootLayoutBox().writingMode().blockDirection()) {
    case FlowDirection::TopToBottom:
    case FlowDirection::BottomToTop:
        return LayoutUnit { line.lineBoxTop() + line.baseline() };
    case FlowDirection::LeftToRight:
        return LayoutUnit { line.lineBoxLeft() + (line.lineBoxWidth() - line.baseline()) };
    case FlowDirection::RightToLeft:
        return LayoutUnit { line.lineBoxLeft() + line.baseline() };
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

LayoutUnit LineLayout::lastLineLogicalBaseline() const
{
    if (!m_inlineContent || m_inlineContent->displayContent().lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto& lastLine = lastLineWithInlineContent(m_inlineContent->displayContent().lines);
    switch (rootLayoutBox().writingMode().blockDirection()) {
    case FlowDirection::TopToBottom:
    case FlowDirection::BottomToTop:
        return LayoutUnit { lastLine.lineBoxTop() + lastLine.baseline() };
    case FlowDirection::LeftToRight: {
        // FIXME: We should set the computed height on the root's box geometry (in RenderBlockFlow) so that
        // we could call m_layoutState.geometryForRootBox().borderBoxHeight() instead.

        // Line is always visual coordinates while logicalHeight is not (i.e. this translate to "box visual width" - "line visual right")
        auto lineLogicalTop = flow().logicalHeight() - lastLine.lineBoxRight();
        return LayoutUnit { lineLogicalTop + lastLine.baseline() };
    }
    case FlowDirection::RightToLeft:
        return LayoutUnit { lastLine.lineBoxLeft() + lastLine.baseline() };
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

Vector<LineAdjustment> LineLayout::adjustContentForPagination(const Layout::BlockLayoutState& blockLayoutState, bool isPartialLayout)
{
    ASSERT(!m_lineDamage);

    if (!m_inlineContent)
        return { };

    auto& layoutState = *flow().view().frameView().layoutContext().layoutState();
    if (!layoutState.isPaginated())
        return { };

    bool allowLayoutRestart = !isPartialLayout;
    auto [adjustments, layoutRestartLine] = computeAdjustmentsForPagination(*m_inlineContent, m_blockFormattingState.placedFloats(), allowLayoutRestart, blockLayoutState, flow());

    adjustLinePositionsForPagination(*m_inlineContent, adjustments);

    if (layoutRestartLine) {
        auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
        auto canRestart = invalidation.restartForPagination(layoutRestartLine->index, layoutRestartLine->offset);
        if (!canRestart)
            m_lineDamage = { };
    }

    return adjustments;
}

void LineLayout::collectOverflow()
{
    if (!m_inlineContent)
        return;

    for (auto& line : m_inlineContent->displayContent().lines) {
        flow().addLayoutOverflow(Layout::toLayoutRect(line.scrollableOverflow()));
        if (!flow().hasNonVisibleOverflow())
            flow().addVisualOverflow(Layout::toLayoutRect(line.inkOverflow()));
    }
}

InlineContent& LineLayout::ensureInlineContent()
{
    if (!m_inlineContent)
        m_inlineContent = makeUnique<InlineContent>(*this);
    return *m_inlineContent;
}

InlineIterator::TextBoxIterator LineLayout::textBoxesFor(const RenderText& renderText) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = *renderText.layoutBox();
    auto firstIndex = m_inlineContent->firstBoxIndexForLayoutBox(layoutBox);
    if (!firstIndex)
        return { };

    return InlineIterator::textBoxFor(*m_inlineContent, *firstIndex);
}

InlineIterator::LeafBoxIterator LineLayout::boxFor(const RenderElement& renderElement) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = *renderElement.layoutBox();
    auto firstIndex = m_inlineContent->firstBoxIndexForLayoutBox(layoutBox);
    if (!firstIndex)
        return { };

    return InlineIterator::boxFor(*m_inlineContent, *firstIndex);
}

InlineIterator::InlineBoxIterator LineLayout::firstInlineBoxFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = *renderInline.layoutBox();
    auto* box = m_inlineContent->firstBoxForLayoutBox(layoutBox);
    if (!box)
        return { };

    return InlineIterator::inlineBoxFor(*m_inlineContent, *box);
}

InlineIterator::InlineBoxIterator LineLayout::firstRootInlineBox() const
{
    if (!m_inlineContent || !m_inlineContent->hasContent())
        return { };

    return InlineIterator::inlineBoxFor(*m_inlineContent, m_inlineContent->displayContent().boxes[0]);
}

InlineIterator::LineBoxIterator LineLayout::firstLineBox() const
{
    if (!m_inlineContent)
        return { };

    return { InlineIterator::LineBoxIteratorModernPath(*m_inlineContent, 0) };
}

InlineIterator::LineBoxIterator LineLayout::lastLineBox() const
{
    if (!m_inlineContent)
        return { };

    return { InlineIterator::LineBoxIteratorModernPath(*m_inlineContent, m_inlineContent->displayContent().lines.isEmpty() ? 0 : m_inlineContent->displayContent().lines.size() - 1) };
}

LayoutRect LineLayout::firstInlineBoxRect(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = *renderInline.layoutBox();
    auto* firstBox = m_inlineContent->firstBoxForLayoutBox(layoutBox);
    if (!firstBox)
        return { };

    // FIXME: We should be able to flip the display boxes soon after the root block
    // is finished sizing in one go.
    auto firstBoxRect = Layout::toLayoutRect(firstBox->visualRectIgnoringBlockDirection());
    switch (rootLayoutBox().writingMode().blockDirection()) {
    case FlowDirection::TopToBottom:
    case FlowDirection::BottomToTop:
    case FlowDirection::LeftToRight:
        return firstBoxRect;
    case FlowDirection::RightToLeft:
        firstBoxRect.setX(flow().width() - firstBoxRect.maxX());
        return firstBoxRect;
    default:
        ASSERT_NOT_REACHED();
        return firstBoxRect;
    }
}

LayoutRect LineLayout::enclosingBorderBoxRectFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    // FIXME: This keeps the existing output.
    if (!m_inlineContent->hasContent())
        return { };

    auto borderBoxLogicalRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(layoutState().geometryForBox(*renderInline.layoutBox())) };
    return flow().writingMode().isHorizontal() ? borderBoxLogicalRect : borderBoxLogicalRect.transposedRect();
}

LayoutRect LineLayout::visualOverflowBoundingBoxRectFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = *renderInline.layoutBox();

    LayoutRect result;
    m_inlineContent->traverseNonRootInlineBoxes(layoutBox, [&](auto& inlineBox) {
        result.unite(Layout::toLayoutRect(inlineBox.inkOverflow()));
    });

    return result;
}

Vector<FloatRect> LineLayout::collectInlineBoxRects(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = *renderInline.layoutBox();

    Vector<FloatRect> result;
    m_inlineContent->traverseNonRootInlineBoxes(layoutBox, [&](auto& inlineBox) {
        result.append(inlineBox.visualRectIgnoringBlockDirection());
    });

    return result;
}

static LayoutPoint flippedContentOffsetIfNeeded(const RenderBlockFlow& root, const RenderBox& childRenderer, LayoutPoint contentOffset)
{
    if (root.writingMode().isBlockFlipped())
        return root.flipForWritingModeForChild(childRenderer, contentOffset);
    return contentOffset;
}

static LayoutRect flippedRectForWritingMode(const RenderBlockFlow& root, const FloatRect& rect)
{
    auto flippedRect = LayoutRect { rect };
    root.flipForWritingMode(flippedRect);
    return flippedRect;
}

void LineLayout::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, const RenderInline* layerRenderer)
{
    if (!m_inlineContent)
        return;

    auto shouldPaintForPhase = [&] {
        switch (paintInfo.phase) {
        case PaintPhase::Accessibility:
        case PaintPhase::Foreground:
        case PaintPhase::EventRegion:
        case PaintPhase::TextClip:
        case PaintPhase::Mask:
        case PaintPhase::Selection:
        case PaintPhase::Outline:
        case PaintPhase::ChildOutlines:
        case PaintPhase::SelfOutline:
            return true;
        default:
            return false;
        }
    };
    if (!shouldPaintForPhase())
        return;

    InlineContentPainter { paintInfo, paintOffset, layerRenderer, *m_inlineContent, flow() }.paint();
}

bool LineLayout::hitTest(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction, const RenderInline* layerRenderer)
{
    if (hitTestAction != HitTestForeground)
        return false;

    if (!m_inlineContent)
        return false;

    auto hitTestBoundingBox = locationInContainer.boundingBox();
    hitTestBoundingBox.moveBy(-accumulatedOffset);
    auto boxRange = m_inlineContent->boxesForRect(hitTestBoundingBox);

    LayerPaintScope layerPaintScope(layerRenderer);

    for (auto& box : makeReversedRange(boxRange)) {
        bool visibleForHitTesting = request.userTriggered() ? box.isVisible() : box.isVisibleIgnoringUsedVisibility();
        if (!visibleForHitTesting)
            continue;

        auto& renderer = *box.layoutBox().rendererForIntegration();

        if (!layerPaintScope.includes(box))
            continue;

        if (box.isAtomicInlineBox()) {
            if (renderer.hitTest(request, result, locationInContainer, flippedContentOffsetIfNeeded(flow(), downcast<RenderBox>(renderer), accumulatedOffset)))
                return true;
            continue;
        }

        auto& currentLine = m_inlineContent->displayContent().lines[box.lineIndex()];
        auto boxRect = flippedRectForWritingMode(flow(), InlineDisplay::Box::visibleRectIgnoringBlockDirection(box, currentLine.visibleRectIgnoringBlockDirection()));
        boxRect.moveBy(accumulatedOffset);

        if (!locationInContainer.intersects(boxRect))
            continue;

        auto& elementRenderer = *[&]() {
            auto* renderElement = dynamicDowncast<RenderElement>(renderer);
            return renderElement ? renderElement : renderer.parent();
        }();
        if (!elementRenderer.visibleToHitTesting(request))
            continue;
        
        renderer.updateHitTestResult(result, flow().flipForWritingMode(locationInContainer.point() - toLayoutSize(accumulatedOffset)));
        if (result.addNodeToListBasedTestResult(renderer.protectedNodeForHitTest().get(), request, locationInContainer, boxRect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

void LineLayout::shiftLinesBy(LayoutUnit blockShift)
{
    if (!m_inlineContent)
        return;
    bool isHorizontalWritingMode = flow().writingMode().isHorizontal();

    for (auto& line : m_inlineContent->displayContent().lines)
        line.moveInBlockDirection(blockShift, isHorizontalWritingMode);

    LayoutUnit deltaX = isHorizontalWritingMode ? 0_lu : blockShift;
    LayoutUnit deltaY = isHorizontalWritingMode ? blockShift : 0_lu;
    for (auto& box : m_inlineContent->displayContent().boxes) {
        if (isHorizontalWritingMode)
            box.moveVertically(blockShift);
        else
            box.moveHorizontally(blockShift);

        if (box.isAtomicInlineBox()) {
            CheckedRef renderer = downcast<RenderBox>(*box.layoutBox().rendererForIntegration());
            renderer->move(deltaX, deltaY);
        }
    }

    for (auto& layoutBox : formattingContextBoxes(rootLayoutBox())) {
        if (layoutBox.isOutOfFlowPositioned() && layoutBox.style().hasStaticBlockPosition(isHorizontalWritingMode)) {
            CheckedRef renderer = downcast<RenderLayerModelObject>(*layoutBox.rendererForIntegration());
            if (!renderer->layer())
                continue;
            CheckedRef layer = *renderer->layer();
            layer->setStaticBlockPosition(layer->staticBlockPosition() + blockShift);
            renderer->setChildNeedsLayout(MarkOnlyThis);
        }
    }
}

bool LineLayout::insertedIntoTree(const RenderElement& parent, RenderObject& child)
{
    if (!m_inlineContent) {
        // This should only be called on partial layout.
        ASSERT_NOT_REACHED();
        return false;
    }

    auto& childLayoutBox = BoxTreeUpdater { flow() }.insert(parent, child, child.previousSibling());
    if (auto* childInlineTextBox = dynamicDowncast<Layout::InlineTextBox>(childLayoutBox)) {
        auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
        return invalidation.textInserted(*childInlineTextBox);
    }

    if (childLayoutBox.isLineBreakBox() || childLayoutBox.isReplacedBox() || childLayoutBox.isInlineBox()) {
        auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
        return invalidation.inlineLevelBoxInserted(childLayoutBox);
    }

    ASSERT_NOT_IMPLEMENTED_YET();
    return false;
}

bool LineLayout::removedFromTree(const RenderElement& parent, RenderObject& child)
{
    if (!m_inlineContent) {
        // This should only be called on partial layout.
        ASSERT_NOT_REACHED();
        return false;
    }

    auto& childLayoutBox = *child.layoutBox();
    auto* childInlineTextBox = dynamicDowncast<Layout::InlineTextBox>(childLayoutBox);
    auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
    auto boxIsInvalidated = childInlineTextBox ? invalidation.textWillBeRemoved(*childInlineTextBox) : childLayoutBox.isLineBreakBox() ? invalidation.inlineLevelBoxWillBeRemoved(childLayoutBox) : false;
    if (boxIsInvalidated)
        m_lineDamage->addDetachedBox(BoxTreeUpdater { flow() }.remove(parent, child));
    return boxIsInvalidated;
}

bool LineLayout::updateTextContent(const RenderText& textRenderer, size_t offset, int delta)
{
    if (!m_inlineContent) {
        // This is supposed to be only called on partial layout, but
        // RenderText::setText may be (force) called after min/max size computation and before layout.
        // We may need to invalidate anyway to clean up inline item list.
        return false;
    }

    BoxTreeUpdater::updateContent(textRenderer);

    auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
    auto& inlineTextBox = *textRenderer.layoutBox();
    return delta >= 0 ? invalidation.textInserted(inlineTextBox, offset) : invalidation.textWillBeRemoved(inlineTextBox, offset);
}

void LineLayout::releaseCaches(RenderView& view)
{
    for (auto& renderer : descendantsOfType<RenderBlockFlow>(view)) {
        if (auto* lineLayout = renderer.inlineLayout())
            lineLayout->releaseCachesAndResetDamage();
    }
}

void LineLayout::releaseCachesAndResetDamage()
{
    m_inlineContentCache.inlineItems().content().clear();
    if (m_inlineContent)
        m_inlineContent->releaseCaches();
    if (m_lineDamage)
        Layout::InlineInvalidation::resetInlineDamage(*m_lineDamage);
}

void LineLayout::clearInlineContent()
{
    if (!m_inlineContent)
        return;
    m_inlineContent = nullptr;
}

Layout::InlineDamage& LineLayout::ensureLineDamage()
{
    if (!m_lineDamage)
        m_lineDamage = makeUnique<Layout::InlineDamage>();
    return *m_lineDamage;
}

bool LineLayout::contentNeedsVisualReordering() const
{
    return m_inlineContentCache.inlineItems().requiresVisualReordering();
}

#if ENABLE(TREE_DEBUGGING)
void LineLayout::outputLineTree(WTF::TextStream& stream, size_t depth) const
{
    if (m_inlineContent)
        showInlineContent(stream, *m_inlineContent, depth, isDamaged());
}
#endif

}
}
