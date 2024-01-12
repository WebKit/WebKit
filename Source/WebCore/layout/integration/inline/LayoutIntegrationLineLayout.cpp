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
#include "EventRegion.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineContentCache.h"
#include "InlineDamage.h"
#include "InlineFormattingContext.h"
#include "InlineInvalidation.h"
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
#include "RenderView.h"
#include "Settings.h"
#include "ShapeOutsideInfo.h"
#include <wtf/Assertions.h>
#include <wtf/Range.h>

namespace WebCore {
namespace LayoutIntegration {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(LayoutIntegration_LineLayout);

static inline std::pair<LayoutRect, LayoutRect> toMarginAndBorderBoxVisualRect(const Layout::BoxGeometry& logicalGeometry, LayoutUnit containerLogicalWidth, WritingMode writingMode, bool isLeftToRightDirection)
{
    auto isFlippedBlocksWritingMode = WebCore::isFlippedWritingMode(writingMode);
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);

    auto borderBoxLogicalRect = Layout::BoxGeometry::borderBoxRect(logicalGeometry);
    auto horizontalMargin = Layout::BoxGeometry::HorizontalEdges { logicalGeometry.marginStart(), logicalGeometry.marginEnd() };
    auto verticalMargin = Layout::BoxGeometry::VerticalEdges { logicalGeometry.marginBefore(), logicalGeometry.marginAfter() };

    auto flipMarginsIfApplicable = [&] {
        if (isHorizontalWritingMode && isLeftToRightDirection && !isFlippedBlocksWritingMode)
            return;

        if (!isHorizontalWritingMode) {
            auto logicalHorizontalMargin = horizontalMargin;
            horizontalMargin = !isFlippedBlocksWritingMode ? Layout::BoxGeometry::HorizontalEdges { verticalMargin.after, verticalMargin.before } : Layout::BoxGeometry::HorizontalEdges { verticalMargin.before, verticalMargin.after };
            verticalMargin = { logicalHorizontalMargin.start, logicalHorizontalMargin.end };
        }
        if (!isLeftToRightDirection) {
            if (isHorizontalWritingMode)
                horizontalMargin = { horizontalMargin.end, horizontalMargin.start };
            else
                verticalMargin = { verticalMargin.after, verticalMargin.before };
        }
    };
    flipMarginsIfApplicable();

    auto borderBoxVisualTopLeft = LayoutPoint { };
    auto borderBoxLeft = isLeftToRightDirection ? borderBoxLogicalRect.left() : containerLogicalWidth - (borderBoxLogicalRect.left() + borderBoxLogicalRect.width());
    if (isHorizontalWritingMode)
        borderBoxVisualTopLeft = { borderBoxLeft, borderBoxLogicalRect.top() };
    else {
        auto marginBoxVisualLeft = borderBoxLogicalRect.top() - logicalGeometry.marginBefore();
        auto marginBoxVisualTop = borderBoxLeft - logicalGeometry.marginStart();
        if (isLeftToRightDirection)
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

LineLayout::LineLayout(RenderBlockFlow& flow)
    : m_boxTree(flow)
    , m_layoutState(flow.view().layoutState())
    , m_blockFormattingState(layoutState().ensureBlockFormattingState(rootLayoutBox()))
    , m_inlineContentCache(layoutState().inlineContentCache(rootLayoutBox()))
    , m_boxGeometryUpdater(m_boxTree, flow.view().layoutState())
{
}

LineLayout::~LineLayout()
{
    clearInlineContent();
    layoutState().destroyInlineContentCache(rootLayoutBox());
    layoutState().destroyBlockFormattingState(rootLayoutBox());
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
        if (is<RenderBlockFlow>(*parent))
            return downcast<RenderBlockFlow>(parent);
    }

    return nullptr;
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
            if (auto* parent = renderer.parent(); is<RenderBlockFlow>(parent))
                return downcast<RenderBlockFlow>(*parent).modernLineLayout();
            return nullptr;
        }
        auto adjustedContainingBlock = [&] {
            RenderElement* containingBlock = nullptr;
            if (renderer.isOutOfFlowPositioned()) {
                // Here we are looking for the containing block as if the out-of-flow box was inflow (for static position purpose).
                containingBlock = renderer.parent();
                if (is<RenderInline>(containingBlock))
                    containingBlock = containingBlock->containingBlock();
            } else if (renderer.isFloating())
                containingBlock = renderer.containingBlock();
            return dynamicDowncast<RenderBlockFlow>(containingBlock);
        };
        if (auto* blockContainer = adjustedContainingBlock())
            return blockContainer->modernLineLayout();
        return nullptr;
    }

    if (auto* container = blockContainer(renderer))
        return container->modernLineLayout();

    return nullptr;
}

const LineLayout* LineLayout::containing(const RenderObject& renderer)
{
    return containing(const_cast<RenderObject&>(renderer));
}

bool LineLayout::isEnabled(const Document& document)
{
    return document.settings().inlineFormattingContextIntegrationEnabled();
}

bool LineLayout::canUseFor(const RenderBlockFlow& flow)
{
    if (!isEnabled(flow.document()))
        return false;

    return canUseForLineLayout(flow);
}

bool LineLayout::canUseForPreferredWidthComputation(const RenderBlockFlow& flow)
{
    ASSERT(isEnabled(flow.document()));
    return LayoutIntegration::canUseForPreferredWidthComputation(flow);
}

bool LineLayout::shouldInvalidateLineLayoutPathAfterContentChange(const RenderBlockFlow& parent, const RenderObject& rendererWithNewContent, const LineLayout& lineLayout)
{
    ASSERT(isEnabled(parent.document()));
    return shouldInvalidateLineLayoutPathAfterChangeFor(parent, rendererWithNewContent, lineLayout, TypeOfChangeForInvalidation::NodeMutation);
}

bool LineLayout::shouldInvalidateLineLayoutPathAfterTreeMutation(const RenderBlockFlow& parent, const RenderObject& renderer, const LineLayout& lineLayout, bool isRemoval)
{
    ASSERT(isEnabled(parent.document()));
    return shouldInvalidateLineLayoutPathAfterChangeFor(parent, renderer, lineLayout, isRemoval ? TypeOfChangeForInvalidation::NodeRemoval : TypeOfChangeForInvalidation::NodeInsertion);
}

void LineLayout::updateInlineContentDimensions()
{
    m_boxGeometryUpdater.setGeometriesForLayout();
}

void LineLayout::updateStyle(const RenderBoxModelObject& renderer, const RenderStyle& oldStyle)
{
    if (m_inlineContent) {
        auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
        invalidation.styleChanged(m_boxTree.layoutBoxForRenderer(renderer), oldStyle);
    }
    m_boxTree.updateStyle(renderer);
}

void LineLayout::updateOverflow()
{
    InlineContentBuilder { flow(), m_boxTree }.updateLineOverflow(*m_inlineContent);
}

std::pair<LayoutUnit, LayoutUnit> LineLayout::computeIntrinsicWidthConstraints()
{
    auto parentBlockLayoutState = Layout::BlockLayoutState { m_blockFormattingState.placedFloats() };
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox(), layoutState(), parentBlockLayoutState };
    if (m_lineDamage)
        m_inlineContentCache.resetMinimumMaximumContentSizes();
    // FIXME: This is where we need to switch between minimum and maximum box geometries.
    auto [minimumContentSize, maximumContentSize] = inlineFormattingContext.minimumMaximumContentSize(m_lineDamage.get());
    return { minimumContentSize, maximumContentSize };
}

static inline std::optional<Layout::BlockLayoutState::LineClamp> lineClamp(const RenderBlockFlow& rootRenderer)
{
    auto& layoutState = *rootRenderer.view().frameView().layoutContext().layoutState();
    if (auto lineClamp = layoutState.lineClamp())
        return Layout::BlockLayoutState::LineClamp { lineClamp->maximumLineCount, lineClamp->currentLineCount };
    return { };
}

static inline Layout::BlockLayoutState::TextBoxTrim textBoxTrim(const RenderBlockFlow& rootRenderer)
{
    auto* layoutState = rootRenderer.view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return { };
    auto textBoxTrimForIFC = Layout::BlockLayoutState::TextBoxTrim { };
    if (layoutState->hasTextBoxTrimStart())
        textBoxTrimForIFC.add(Layout::BlockLayoutState::TextBoxTrimSide::Start);
    if (layoutState->hasTextBoxTrimEnd(rootRenderer))
        textBoxTrimForIFC.add(Layout::BlockLayoutState::TextBoxTrimSide::End);
    return textBoxTrimForIFC;
}

static inline std::optional<Layout::BlockLayoutState::LineGrid> lineGrid(const RenderBlockFlow& rootRenderer)
{
    auto& layoutState = *rootRenderer.view().frameView().layoutContext().layoutState();
    if (auto* lineGrid = layoutState.lineGrid()) {
        if (lineGrid->style().writingMode() != rootRenderer.style().writingMode())
            return { };

        auto layoutOffset = layoutState.layoutOffset();
        auto lineGridOffset = layoutState.lineGridOffset();
        if (lineGrid->style().isVerticalWritingMode()) {
            layoutOffset = layoutOffset.transposedSize();
            lineGridOffset = lineGridOffset.transposedSize();
        }

        auto columnWidth = lineGrid->style().fontCascade().primaryFont().maxCharWidth();
        auto rowHeight = lineGrid->style().computedLineHeight();
        auto topRowOffset = lineGrid->borderAndPaddingBefore();

        std::optional<LayoutSize> paginationOrigin;
        auto pageLogicalTop = 0_lu;
        if (layoutState.isPaginated()) {
            paginationOrigin = layoutState.lineGridPaginationOrigin();
            if (lineGrid->style().isVerticalWritingMode())
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

    // FIXME: Partial layout should not rely on inline display content, but instead InlineContentCache
    // should retain all the pieces of data required -and then we can destroy damaged content here instead of after
    // layout in constructContent.
    auto isPartialLayout = isDamaged() && m_lineDamage->start();
    if (!isPartialLayout) {
        m_lineDamage = { };
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
        auto damagedLineIndex = m_lineDamage->start()->lineIndex;
        if (!damagedLineIndex)
            return *m_inlineContentConstraints;
        if (damagedLineIndex >= m_inlineContent->displayContent().lines.size()) {
            ASSERT_NOT_REACHED();
            return *m_inlineContentConstraints;
        }
        auto partialContentTop = LayoutUnit { m_inlineContent->displayContent().lines[damagedLineIndex - 1].lineBoxLogicalRect().maxY() };
        auto constraintsForInFlowContent = Layout::ConstraintsForInFlowContent { m_inlineContentConstraints->horizontal(), partialContentTop };
        return { constraintsForInFlowContent, m_inlineContentConstraints->visualLeft() };
    };

    auto parentBlockLayoutState = Layout::BlockLayoutState {
        m_blockFormattingState.placedFloats(),
        lineClamp(flow()),
        textBoxTrim(flow()),
        intrusiveInitialLetterBottom(),
        lineGrid(flow())
    };
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox(), layoutState(), parentBlockLayoutState };
    // Temporary, integration only.
    inlineFormattingContext.layoutState().setNestedListMarkerOffsets(m_boxGeometryUpdater.takeNestedListMarkerOffsets());
    auto layoutResult = inlineFormattingContext.layout(inlineContentConstraints(), m_lineDamage.get());
    auto repaintRect = LayoutRect { constructContent(inlineFormattingContext.layoutState(), WTFMove(layoutResult)) };
    auto adjustments = adjustContent(parentBlockLayoutState);

    updateRenderTreePositions(adjustments);

    m_lineDamage = { };

    return isPartialLayout ? std::make_optional(repaintRect) : std::nullopt;
}

FloatRect LineLayout::constructContent(const Layout::InlineLayoutState& inlineLayoutState, Layout::InlineLayoutResult&& layoutResult)
{
    auto damagedRect = InlineContentBuilder { flow(), m_boxTree }.build(WTFMove(layoutResult), ensureInlineContent(), m_lineDamage.get());

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

void LineLayout::updateRenderTreePositions(const Vector<LineAdjustment>& lineAdjustments)
{
    if (!m_inlineContent)
        return;

    auto& blockFlow = flow();
    auto& rootStyle = blockFlow.style();
    auto isLeftToRightPlacedFloatsInlineDirection = m_blockFormattingState.placedFloats().isLeftToRightDirection();
    auto writingMode = rootStyle.writingMode();
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);

    auto visualAdjustmentOffset = [&](auto lineIndex) {
        if (lineAdjustments.isEmpty())
            return LayoutSize { };
        if (!isHorizontalWritingMode)
            return LayoutSize { lineAdjustments[lineIndex].offset, 0_lu };
        return LayoutSize { 0_lu, lineAdjustments[lineIndex].offset };
    };

    for (auto& box : m_inlineContent->displayContent().boxes) {
        if (box.isInlineBox() || box.isText())
            continue;

        auto& layoutBox = box.layoutBox();
        if (!layoutBox.isAtomicInlineLevelBox())
            continue;

        auto& renderer = downcast<RenderBox>(m_boxTree.rendererForLayoutBox(layoutBox));
        if (auto* layer = renderer.layer())
            layer->setIsHiddenByOverflowTruncation(box.isFullyTruncated());

        renderer.setLocation(Layout::toLayoutPoint(box.visualRectIgnoringBlockDirection().location()));
        auto relayoutRubyAnnotationIfNeeded = [&] {
            if (!layoutBox.isRubyAnnotationBox())
                return;
            // Annotation inline-block may get resized during inline layout (when base is wider) and
            // we need to apply this new size on the annotation content by running layout.
            auto needsResizing = layoutBox.isInterlinearRubyAnnotationBox() || !isHorizontalWritingMode;
            if (!needsResizing)
                return;
            auto logicalMarginBoxSize = Layout::BoxGeometry::marginBoxRect(layoutState().geometryForBox(layoutBox)).size();
            if (logicalMarginBoxSize == renderer.size())
                return;
            renderer.setSize(logicalMarginBoxSize);
            renderer.setOverridingLogicalWidthLength({ logicalMarginBoxSize.width(), LengthType::Fixed });
            renderer.setNeedsLayout(MarkOnlyThis);
            renderer.layoutIfNeeded();
            renderer.clearOverridingLogicalWidthLength();
        };
        relayoutRubyAnnotationIfNeeded();
    }

    HashMap<CheckedRef<const Layout::Box>, LayoutSize> floatPaginationOffsetMap;
    if (!lineAdjustments.isEmpty()) {
        for (auto& floatItem : m_blockFormattingState.placedFloats().list()) {
            if (!floatItem.layoutBox() || !floatItem.placedByLine())
                continue;
            auto adjustmentOffset = visualAdjustmentOffset(*floatItem.placedByLine());
            floatPaginationOffsetMap.add(*floatItem.layoutBox(), adjustmentOffset);
        }
    }

    for (auto& renderObject : m_boxTree.renderers()) {
        auto& layoutBox = *renderObject->layoutBox();
        if (!layoutBox.isFloatingPositioned() && !layoutBox.isOutOfFlowPositioned())
            continue;
        if (layoutBox.isLineBreakBox())
            continue;
        auto& renderer = downcast<RenderBox>(m_boxTree.rendererForLayoutBox(layoutBox));
        auto& logicalGeometry = layoutState().geometryForBox(layoutBox);

        if (layoutBox.isFloatingPositioned()) {
            auto& floatingObject = flow().insertFloatingObjectForIFC(renderer);
            auto containerLogicalWidth = m_inlineContentConstraints->visualLeft() + m_inlineContentConstraints->horizontal().logicalWidth + m_inlineContentConstraints->horizontal().logicalLeft;
            auto [marginBoxVisualRect, borderBoxVisualRect] = toMarginAndBorderBoxVisualRect(logicalGeometry, containerLogicalWidth, writingMode, isLeftToRightPlacedFloatsInlineDirection);

            auto paginationOffset = floatPaginationOffsetMap.getOptional(layoutBox);
            if (paginationOffset) {
                marginBoxVisualRect.move(*paginationOffset);
                borderBoxVisualRect.move(*paginationOffset);
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

void LineLayout::updateInlineContentConstraints()
{
    m_inlineContentConstraints = m_boxGeometryUpdater.updateInlineContentConstraints();
}

void LineLayout::preparePlacedFloats()
{
    auto& placedFloats = m_blockFormattingState.placedFloats();
    placedFloats.clear();

    if (!flow().containsFloats())
        return;

    auto isHorizontalWritingMode = flow().containingBlock() ? flow().containingBlock()->style().isHorizontalWritingMode() : true;
    auto placedFloatsIsLeftToRightInlineDirection = flow().containingBlock() ? flow().containingBlock()->style().isLeftToRightDirection() : true;
    placedFloats.setIsLeftToRightDirection(placedFloatsIsLeftToRightInlineDirection);
    for (auto& floatingObject : *flow().floatingObjectSet()) {
        auto& visualRect = floatingObject->frameRect();
        auto logicalPosition = [&] {
            switch (floatingObject->renderer().style().floating()) {
            case Float::Left:
                return placedFloatsIsLeftToRightInlineDirection ? Layout::PlacedFloats::Item::Position::Left : Layout::PlacedFloats::Item::Position::Right;
            case Float::Right:
                return placedFloatsIsLeftToRightInlineDirection ? Layout::PlacedFloats::Item::Position::Right : Layout::PlacedFloats::Item::Position::Left;
            case Float::InlineStart: {
                auto* floatBoxContainingBlock = floatingObject->renderer().containingBlock();
                if (floatBoxContainingBlock)
                    return floatBoxContainingBlock->style().isLeftToRightDirection() == placedFloatsIsLeftToRightInlineDirection ? Layout::PlacedFloats::Item::Position::Left : Layout::PlacedFloats::Item::Position::Right;
                return Layout::PlacedFloats::Item::Position::Left;
            }
            case Float::InlineEnd: {
                auto* floatBoxContainingBlock = floatingObject->renderer().containingBlock();
                if (floatBoxContainingBlock)
                    return floatBoxContainingBlock->style().isLeftToRightDirection() == placedFloatsIsLeftToRightInlineDirection ? Layout::PlacedFloats::Item::Position::Right : Layout::PlacedFloats::Item::Position::Left;
                return Layout::PlacedFloats::Item::Position::Right;
            }
            default:
                ASSERT_NOT_REACHED();
                return Layout::PlacedFloats::Item::Position::Left;
            }
        };

        auto boxGeometry = Layout::BoxGeometry { };
        auto logicalRect = [&] {
            // FIXME: We are flooring here for legacy compatibility. See FloatingObjects::intervalForFloatingObject and RenderBlockFlow::clearFloats.
            auto logicalTop = isHorizontalWritingMode ? LayoutUnit(visualRect.y().floor()) : visualRect.x();
            auto logicalLeft = isHorizontalWritingMode ? visualRect.x() : LayoutUnit(visualRect.y().floor());
            auto logicalHeight = (isHorizontalWritingMode ? LayoutUnit(visualRect.maxY().floor()) : visualRect.maxX()) - logicalTop;
            auto logicalWidth = (isHorizontalWritingMode ? visualRect.maxX() : LayoutUnit(visualRect.maxY().floor())) - logicalLeft;
            if (!placedFloatsIsLeftToRightInlineDirection) {
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
            if (lines[lineIndex].isTruncatedInBlockDirection())
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

    auto lastLineWithInlineContent = [&] {
        // Out-of-flow/float content only don't produce lines with inline content. They should not be taken into
        // account when computing content box height.
        for (auto& line : makeReversedRange(lines)) {
            ASSERT(line.boxCount());
            if (line.boxCount() > 1)
                return line;
        }
        return lines.first();
    };
    auto contentHeight = lastLineWithInlineContent().lineBoxLogicalRect().maxY() - lines.first().lineBoxLogicalRect().y();
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

    auto lastLine = m_inlineContent->displayContent().lines.last();
    return physicalBaselineForLine(lastLine);
}

LayoutUnit LineLayout::physicalBaselineForLine(const InlineDisplay::Line& line) const
{
    switch (writingModeToBlockFlowDirection(rootLayoutBox().style().writingMode())) {
    case BlockFlowDirection::TopToBottom:
    case BlockFlowDirection::BottomToTop:
        return LayoutUnit { line.lineBoxTop() + line.baseline() };
    case BlockFlowDirection::LeftToRight:
        return LayoutUnit { line.lineBoxLeft() + (line.lineBoxWidth() - line.baseline()) };
    case BlockFlowDirection::RightToLeft:
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

    auto& lastLine = m_inlineContent->displayContent().lines.last();
    switch (writingModeToBlockFlowDirection(rootLayoutBox().style().writingMode())) {
    case BlockFlowDirection::TopToBottom:
    case BlockFlowDirection::BottomToTop:
        return LayoutUnit { lastLine.lineBoxTop() + lastLine.baseline() };
    case BlockFlowDirection::LeftToRight: {
        // FIXME: We should set the computed height on the root's box geometry (in RenderBlockFlow) so that
        // we could call m_layoutState.geometryForRootBox().borderBoxHeight() instead.

        // Line is always visual coordinates while logicalHeight is not (i.e. this translate to "box visual width" - "line visual right")
        auto lineLogicalTop = flow().logicalHeight() - lastLine.lineBoxRight();
        return LayoutUnit { lineLogicalTop + lastLine.baseline() };
    }
    case BlockFlowDirection::RightToLeft:
        return LayoutUnit { lastLine.lineBoxLeft() + lastLine.baseline() };
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

Vector<LineAdjustment> LineLayout::adjustContent(const Layout::BlockLayoutState& blockLayoutState)
{
    if (!m_inlineContent)
        return { };

    auto& layoutState = *flow().view().frameView().layoutContext().layoutState();
    if (!layoutState.isPaginated())
        return { };

    auto adjustments = computeAdjustmentsForPagination(*m_inlineContent, m_blockFormattingState.placedFloats(), blockLayoutState, flow());
    adjustLinePositionsForPagination(*m_inlineContent, adjustments);

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

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderText);
    auto firstIndex = m_inlineContent->firstBoxIndexForLayoutBox(layoutBox);
    if (!firstIndex)
        return { };

    return InlineIterator::textBoxFor(*m_inlineContent, *firstIndex);
}

InlineIterator::LeafBoxIterator LineLayout::boxFor(const RenderElement& renderElement) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderElement);
    auto firstIndex = m_inlineContent->firstBoxIndexForLayoutBox(layoutBox);
    if (!firstIndex)
        return { };

    return InlineIterator::boxFor(*m_inlineContent, *firstIndex);
}

InlineIterator::InlineBoxIterator LineLayout::firstInlineBoxFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);
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

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);
    auto* firstBox = m_inlineContent->firstBoxForLayoutBox(layoutBox);
    if (!firstBox)
        return { };

    // FIXME: We should be able to flip the display boxes soon after the root block
    // is finished sizing in one go.
    auto firstBoxRect = Layout::toLayoutRect(firstBox->visualRectIgnoringBlockDirection());
    switch (writingModeToBlockFlowDirection(rootLayoutBox().style().writingMode())) {
    case BlockFlowDirection::TopToBottom:
    case BlockFlowDirection::BottomToTop:
    case BlockFlowDirection::LeftToRight:
        return firstBoxRect;
    case BlockFlowDirection::RightToLeft:
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

    auto borderBoxLogicalRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(layoutState().geometryForBox(m_boxTree.layoutBoxForRenderer(renderInline))) };
    return WebCore::isHorizontalWritingMode(flow().style().writingMode()) ? borderBoxLogicalRect : borderBoxLogicalRect.transposedRect();
}

LayoutRect LineLayout::visualOverflowBoundingBoxRectFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);

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

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);

    Vector<FloatRect> result;
    m_inlineContent->traverseNonRootInlineBoxes(layoutBox, [&](auto& inlineBox) {
        result.append(inlineBox.visualRectIgnoringBlockDirection());
    });

    return result;
}

bool LineLayout::contains(const RenderElement& renderer) const
{
    return m_boxTree.contains(renderer);
}

const RenderObject& LineLayout::rendererForLayoutBox(const Layout::Box& layoutBox) const
{
    return m_boxTree.rendererForLayoutBox(layoutBox);
}

const Layout::ElementBox& LineLayout::rootLayoutBox() const
{
    return m_boxTree.rootLayoutBox();
}

Layout::ElementBox& LineLayout::rootLayoutBox()
{
    return m_boxTree.rootLayoutBox();
}

static LayoutPoint flippedContentOffsetIfNeeded(const RenderBlockFlow& root, const RenderBox& childRenderer, LayoutPoint contentOffset)
{
    if (root.style().isFlippedBlocksWritingMode())
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

    InlineContentPainter { paintInfo, paintOffset, layerRenderer, *m_inlineContent, m_boxTree }.paint();
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

    LayerPaintScope layerPaintScope(m_boxTree, layerRenderer);

    for (auto& box : makeReversedRange(boxRange)) {
        if (!box.isVisible())
            continue;

        auto& renderer = m_boxTree.rendererForLayoutBox(box.layoutBox());

        if (!layerPaintScope.includes(box))
            continue;

        if (box.isAtomicInlineLevelBox()) {
            if (renderer.hitTest(request, result, locationInContainer, flippedContentOffsetIfNeeded(flow(), downcast<RenderBox>(renderer), accumulatedOffset)))
                return true;
            continue;
        }

        auto& currentLine = m_inlineContent->displayContent().lines[box.lineIndex()];
        auto boxRect = flippedRectForWritingMode(flow(), InlineDisplay::Box::visibleRectIgnoringBlockDirection(box, currentLine.visibleRectIgnoringBlockDirection()));
        boxRect.moveBy(accumulatedOffset);

        if (!locationInContainer.intersects(boxRect))
            continue;

        auto& elementRenderer = is<RenderElement>(renderer) ? downcast<RenderElement>(renderer) : *renderer.parent();
        if (!elementRenderer.visibleToHitTesting(request))
            continue;
        
        renderer.updateHitTestResult(result, flow().flipForWritingMode(locationInContainer.point() - toLayoutSize(accumulatedOffset)));
        if (result.addNodeToListBasedTestResult(renderer.nodeForHitTest(), request, locationInContainer, boxRect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

void LineLayout::shiftLinesBy(LayoutUnit blockShift)
{
    if (!m_inlineContent)
        return;
    bool isHorizontalWritingMode = WebCore::isHorizontalWritingMode(flow().style().writingMode());

    for (auto& line : m_inlineContent->displayContent().lines)
        line.moveInBlockDirection(blockShift, isHorizontalWritingMode);

    LayoutUnit deltaX = isHorizontalWritingMode ? 0_lu : blockShift;
    LayoutUnit deltaY = isHorizontalWritingMode ? blockShift : 0_lu;
    for (auto& box : m_inlineContent->displayContent().boxes) {
        if (isHorizontalWritingMode)
            box.moveVertically(blockShift);
        else
            box.moveHorizontally(blockShift);

        if (box.isAtomicInlineLevelBox()) {
            CheckedRef renderer = downcast<RenderBox>(m_boxTree.rendererForLayoutBox(box.layoutBox()));
            renderer->move(deltaX, deltaY);
        }
    }

    for (auto& object : m_boxTree.renderers()) {
        Layout::Box& layoutBox = *object->layoutBox();
        if (layoutBox.isOutOfFlowPositioned() && layoutBox.style().hasStaticBlockPosition(isHorizontalWritingMode)) {
            CheckedRef renderer = downcast<RenderLayerModelObject>(m_boxTree.rendererForLayoutBox(layoutBox));
            if (!renderer->layer())
                continue;
            CheckedRef layer = *renderer->layer();
            layer->setStaticBlockPosition(layer->staticBlockPosition() + blockShift);
            renderer->setChildNeedsLayout(MarkOnlyThis);
        }
    }
}

void LineLayout::insertedIntoTree(const RenderElement& parent, RenderObject& child)
{
    if (!m_inlineContent) {
        // This should only be called on partial layout.
        ASSERT_NOT_REACHED();
        return;
    }

    auto& childLayoutBox = m_boxTree.insert(parent, child, child.previousSibling());
    if (is<Layout::InlineTextBox>(childLayoutBox)) {
        auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
        invalidation.textInserted(downcast<Layout::InlineTextBox>(childLayoutBox));
        return;
    }

    if (childLayoutBox.isLineBreakBox() || childLayoutBox.isReplacedBox() || childLayoutBox.isInlineBox()) {
        auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
        invalidation.inlineLevelBoxInserted(childLayoutBox);
        return;
    }

    ASSERT_NOT_IMPLEMENTED_YET();
}

void LineLayout::removedFromTree(const RenderElement& parent, RenderObject& child)
{
    if (!m_inlineContent) {
        // This should only be called on partial layout.
        ASSERT_NOT_REACHED();
        return;
    }

    auto& childLayoutBox = m_boxTree.layoutBoxForRenderer(child);
    auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
    auto boxIsInvalidated = is<Layout::InlineTextBox>(childLayoutBox) ? invalidation.textWillBeRemoved(downcast<Layout::InlineTextBox>(childLayoutBox)) : childLayoutBox.isLineBreakBox() ? invalidation.inlineLevelBoxWillBeRemoved(childLayoutBox) : false;
    if (boxIsInvalidated)
        m_lineDamage->addDetachedBox(m_boxTree.remove(parent, child));
}

void LineLayout::updateTextContent(const RenderText& textRenderer, size_t offset, int delta)
{
    if (!m_inlineContent) {
        // This should only be called on partial layout.
        ASSERT_NOT_REACHED();
        return;
    }

    m_boxTree.updateContent(textRenderer);
    auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
    auto& inlineTextBox = downcast<Layout::InlineTextBox>(m_boxTree.layoutBoxForRenderer(textRenderer));
    if (delta >= 0) {
        invalidation.textInserted(inlineTextBox, offset);
        return;
    }
    invalidation.textWillBeRemoved(inlineTextBox, offset);
}

void LineLayout::releaseCaches(RenderView& view)
{
    if (!isEnabled(view.document()))
        return;

    for (auto& renderer : descendantsOfType<RenderBlockFlow>(view)) {
        if (auto* lineLayout = renderer.modernLineLayout())
            lineLayout->releaseCaches();
    }
}

void LineLayout::releaseCaches()
{
    m_inlineContentCache.inlineItems().content().clear();
    if (m_inlineContent)
        m_inlineContent->releaseCaches();
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
    showInlineContent(stream, *m_inlineContent, depth, isDamaged());
}
#endif

}
}

