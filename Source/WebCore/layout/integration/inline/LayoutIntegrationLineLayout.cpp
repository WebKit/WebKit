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
#include "FormattingContextBoxIterator.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineContentCache.h"
#include "InlineDisplayBoxInlines.h"
#include "InlineDamage.h"
#include "InlineFormattingContext.h"
#include "InlineInvalidation.h"
#include "InlineItemsBuilder.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationCoverage.h"
#include "LayoutIntegrationInlineContentBuilder.h"
#include "LayoutIntegrationInlineContentPainter.h"
#include "LayoutIntegrationPagination.h"
#include "LayoutIntegrationUtils.h"
#include "LayoutTreeBuilder.h"
#include "PaintInfo.h"
#include "PlacedFloats.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderDescendantIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderView.h"
#include "SVGTextFragment.h"
#include "ShapeOutsideInfo.h"
#include <ranges>
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
static bool shouldInvalidateLineLayoutAfterChangeFor(const RenderBlockFlow& rootBlockContainer, const RenderObject& renderer, const LineLayout& lineLayout, TypeOfChangeForInvalidation typeOfChange)
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
        if (CheckedPtr renderInline = dynamicDowncast<RenderInline>(renderer)) {
            auto& style = renderInline->style();
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
        if (rootBlockContainer.style().hasPseudoStyle(PseudoElementType::FirstLetter))
            return true;
        if (rootBlockContainer.isAnonymous())
            return rootBlockContainer.containingBlock() && rootBlockContainer.containingBlock()->style().hasPseudoStyle(PseudoElementType::FirstLetter);
        return false;
    };
    if (hasFirstLetter())
        return true;

    if (auto* previousDamage = lineLayout.damage(); previousDamage && (previousDamage->reasons() != Layout::InlineDamage::Reason::Append || !previousDamage->layoutStartPosition())) {
        // Only support subsequent append operations where we managed to invalidate the content for partial layout.
        return true;
    }

    auto& rootBlockContainerStyle = rootBlockContainer.style();
    auto shouldBalance = rootBlockContainerStyle.textWrapMode() == TextWrapMode::Wrap && rootBlockContainerStyle.textWrapStyle() == TextWrapStyle::Balance;
    auto shouldPrettify = rootBlockContainerStyle.textWrapMode() == TextWrapMode::Wrap && rootBlockContainerStyle.textWrapStyle() == TextWrapStyle::Pretty;
    auto hasAutospace = !rootBlockContainerStyle.textAutospace().isNoAutospace();
    if (rootBlockContainer.writingMode().isBidiRTL() || shouldBalance || shouldPrettify || hasAutospace)
        return true;

    auto rootHasNonSupportedRenderer = [&] (bool shouldOnlyCheckForRelativeDimension = false) {
        for (auto* sibling = rootBlockContainer.firstChild(); sibling; sibling = sibling->nextSibling()) {
            if (auto* inlineBox = dynamicDowncast<RenderInline>(*sibling); inlineBox && !inlineBox->style().textAutospace().isNoAutospace())
                return true;

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

static const InlineDisplay::Line& lastLineWithInflowContent(const InlineDisplay::Lines& lines)
{
    // Out-of-flow/float content only don't produce lines with inline content. They should not be taken into
    // account when computing content box height/baselines.
    for (auto& line : lines | std::views::reverse) {
        ASSERT(line.boxCount());
        if (line.hasInflowContent())
            return line;
    }
    return lines.first();
}

LineLayout::LineLayout(RenderBlockFlow& flow)
    : m_rootLayoutBox(BoxTreeUpdater { flow }.build())
    , m_document(flow.document())
    , m_layoutState(flow.view().layoutState())
    , m_blockFormattingState(layoutState().ensureBlockFormattingState(rootLayoutBox()))
    , m_inlineContentCache(layoutState().inlineContentCache(rootLayoutBox()))
    , m_boxGeometryUpdater(flow.view().layoutState(), rootLayoutBox())
{
}

LineLayout::~LineLayout()
{
    auto& rootRenderer = flow();
    auto shouldPopulateBreakingPositionCache = [&] {
        auto mayHaveInvalidContent = isDamaged() || !m_inlineContent;
        if (m_document->renderTreeBeingDestroyed() || mayHaveInvalidContent)
            return false;
        return !m_inlineContentCache.inlineItems().isPopulatedFromCache();
    };
    if (shouldPopulateBreakingPositionCache())
        Layout::InlineItemsBuilder::populateBreakingPositionCache(m_inlineContentCache.inlineItems().content(), rootRenderer.document());
    clearInlineContent();
    layoutState().destroyInlineContentCache(rootLayoutBox());
    layoutState().destroyBlockFormattingState(rootLayoutBox());
    m_boxGeometryUpdater.clear();
    m_lineDamage = { };
    m_rootLayoutBox = nullptr;

    BoxTreeUpdater { rootRenderer, *m_document }.tearDown();
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
                containingBlock = RenderObject::containingBlockForPositionType(downcast<RenderBox>(renderer).style().position(), renderer);
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

bool LineLayout::shouldInvalidateLineLayoutAfterContentChange(const RenderBlockFlow& parent, const RenderObject& rendererWithNewContent, const LineLayout& lineLayout)
{
    return shouldInvalidateLineLayoutAfterChangeFor(parent, rendererWithNewContent, lineLayout, TypeOfChangeForInvalidation::NodeMutation);
}

bool LineLayout::shouldInvalidateLineLayoutAfterTreeMutation(const RenderBlockFlow& parent, const RenderObject& renderer, const LineLayout& lineLayout, bool isRemoval)
{
    return shouldInvalidateLineLayoutAfterChangeFor(parent, renderer, lineLayout, isRemoval ? TypeOfChangeForInvalidation::NodeRemoval : TypeOfChangeForInvalidation::NodeInsertion);
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
    if (!m_inlineContent)
        return;
    InlineContentBuilder { flow() }.updateLineOverflow(*m_inlineContent);
}

std::pair<LayoutUnit, LayoutUnit> LineLayout::computeIntrinsicWidthConstraints()
{
    auto parentBlockLayoutState = Layout::BlockLayoutState { m_blockFormattingState.placedFloats(), { } };
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
    auto textBoxTrim = rootRenderer.view().frameView().layoutContext().textBoxTrim();
    if (!textBoxTrim)
        return { };

    auto textBoxTrimForIFC = Layout::BlockLayoutState::TextBoxTrim { };
    auto isLineInverted = rootRenderer.writingMode().isLineInverted();
    if (textBoxTrim->trimFirstFormattedLine)
        textBoxTrimForIFC.add(isLineInverted ? Layout::BlockLayoutState::TextBoxTrimSide::End : Layout::BlockLayoutState::TextBoxTrimSide::Start);

    if (textBoxTrim->lastFormattedLineRoot.get() == &rootRenderer)
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

        auto columnWidth = lineGrid->style().fontCascade().primaryFont()->maxCharWidth();
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

std::optional<LayoutRect> LineLayout::layout(RenderBlockFlow::MarginInfo& marginInfo, ForceFullLayout forcedFullLayout)
{
    if (forcedFullLayout == ForceFullLayout::Yes && m_lineDamage)
        Layout::InlineInvalidation::resetInlineDamage(*m_lineDamage);

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
        return { constraintsForInFlowContent, m_inlineContentConstraints->visualLeft(), m_inlineContentConstraints->formattingRootBorderBoxSize() };
    };

    auto parentBlockLayoutState = Layout::BlockLayoutState {
        m_blockFormattingState.placedFloats(),
        Layout::IntegrationUtils::toMarginState(marginInfo),
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

    updateRenderTreePositions(adjustments, inlineFormattingContext.layoutState(), layoutResult.didDiscardContent);

    if (m_lineDamage) {
        // Pagination may require another layout pass.
        layout(marginInfo);

        ASSERT(!m_lineDamage);
    }

    marginInfo = Layout::IntegrationUtils::toMarginInfo(parentBlockLayoutState.marginState());
    return isPartialLayout ? std::make_optional(repaintRect) : std::nullopt;
}

FloatRect LineLayout::constructContent(const Layout::InlineLayoutState& inlineLayoutState, Layout::InlineLayoutResult&& layoutResult)
{
    auto damagedRect = InlineContentBuilder { flow() }.build(WTFMove(layoutResult), ensureInlineContent(), m_lineDamage.get());

    m_inlineContent->setClearGapBeforeFirstLine(inlineLayoutState.clearGapBeforeFirstLine());
    m_inlineContent->setClearGapAfterLastLine(inlineLayoutState.clearGapAfterLastLine());
    m_inlineContent->shrinkToFit();

    m_inlineContentCache.inlineItems().shrinkToFit();
    m_blockFormattingState.shrinkToFit();

    // FIXME: These needs to be incorporated into the partial damage.
    auto offsetAndGaps = m_inlineContent->firstLinePaginationOffset() + m_inlineContent->clearBeforeAfterGaps();
    damagedRect.expand({ 0, offsetAndGaps });
    return damagedRect;
}

void LineLayout::updateRenderTreePositions(const Vector<LineAdjustment>& lineAdjustments, const Layout::InlineLayoutState& inlineLayoutState, bool didDiscardContent)
{
    if (!m_inlineContent && !didDiscardContent)
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

    if (m_inlineContent) {
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

    for (auto& layoutBox : formattingContextBoxes(rootLayoutBox())) {
        if (didDiscardContent)
            layoutBox.rendererForIntegration()->clearNeedsLayout();

        if (!layoutBox.isFloatingPositioned() && !layoutBox.isOutOfFlowPositioned())
            continue;
        if (layoutBox.isLineBreakBox())
            continue;
        auto& renderer = downcast<RenderBox>(*layoutBox.rendererForIntegration());
        auto& logicalGeometry = layoutState().geometryForBox(layoutBox);

        if (layoutBox.isFloatingPositioned()) {
            // FIXME: Find out what to do with discarded (see line-clamp) floats in render tree.
            auto isInitialLetter = layoutBox.style().pseudoElementType() == PseudoElementType::FirstLetter;
            auto& floatingObject = flow().insertFloatingBox(renderer);
            auto [marginBoxVisualRect, borderBoxVisualRect] = Layout::IntegrationUtils::toMarginAndBorderBoxVisualRect(logicalGeometry, m_inlineContentConstraints->formattingRootBorderBoxSize(), placedFloatsWritingMode);

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
            auto borderBoxLogicalTopLeft = Layout::BoxGeometry::borderBoxTopLeft(logicalGeometry);
            auto previousStaticPosition = LayoutPoint { layer.staticInlinePosition(), layer.staticBlockPosition() };
            auto delta = borderBoxLogicalTopLeft - previousStaticPosition;
            auto hasStaticInlinePositioning = layoutBox.style().hasStaticInlinePosition(renderer.isHorizontalWritingMode());

            if (layoutBox.style().isOriginalDisplayInlineType()) {
                blockFlow.setStaticInlinePositionForChild(renderer, borderBoxLogicalTopLeft.x());
                if (hasStaticInlinePositioning)
                    renderer.move(delta.width(), delta.height());
            }

            layer.setStaticBlockPosition(borderBoxLogicalTopLeft.y());
            layer.setStaticInlinePosition(borderBoxLogicalTopLeft.x());
            continue;
        }
    }
}

FloatRect LineLayout::applySVGTextFragments(SVGTextFragmentMap&& fragmentMap)
{
    auto& boxes = m_inlineContent->displayContent().boxes;
    auto& lines = m_inlineContent->displayContent().lines;
    auto& fragments = m_inlineContent->svgTextFragmentsForBoxes();
    fragments.resize(m_inlineContent->displayContent().boxes.size());

    FloatRect fullBoundaries;

    struct Parent {
        size_t index;
        FloatRect boundaries;
    };
    Vector<Parent, 8> parentStack;

    auto popParent = [&](const Layout::Box* parent) {
        while (!parentStack.isEmpty() && &boxes[parentStack.last().index].layoutBox() != parent) {
            auto boundaries = parentStack.last().boundaries;
            boxes[parentStack.last().index].setRect(boundaries, boundaries);
            parentStack.removeLast();
            if (!parentStack.isEmpty())
                parentStack.last().boundaries.unite(boundaries);
            else
                fullBoundaries = boundaries;
        }
    };

    for (size_t i = 0; i < boxes.size(); ++i) {
        popParent(&boxes[i].layoutBox().parent());

        auto textBox = InlineIterator::svgTextBoxFor(*m_inlineContent, i);
        if (!textBox) {
            parentStack.append({ i, { } });
            continue;
        }

        auto it = fragmentMap.find(makeKey(*textBox));
        if (it != fragmentMap.end())
            fragments[i] = WTFMove(it->value);

        auto boundaries = textBox->calculateBoundariesIncludingSVGTransform();
        boxes[i].setRect(boundaries, boundaries);
        parentStack.last().boundaries.unite(boundaries);
    }

    popParent(nullptr);

    // Move so the top-left text box is at (0, 0).
    for (auto& box : boxes) {
        box.moveHorizontally(-fullBoundaries.x());
        box.moveVertically(-fullBoundaries.y());
    }

    if (lines.size())
        lines[0].setLineBoxRectForSVGText(FloatRect { { }, fullBoundaries.size() });

    return fullBoundaries;
}

void LineLayout::preparePlacedFloats()
{
    auto& placedFloats = m_blockFormattingState.placedFloats();
    placedFloats.clear();

    if (!flow().containsFloats())
        return;

    auto placedFloatsWritingMode = placedFloats.blockFormattingContextRoot().style().writingMode();
    auto placedFloatsIsLeftToRight = placedFloatsWritingMode.isLogicalLeftInlineStart();
    auto isHorizontalWritingMode = placedFloatsWritingMode.isHorizontal();
    for (auto& floatingObject : *flow().floatingObjectSet()) {
        if (!floatingObject->renderer())
            continue;

        auto& visualRect = floatingObject->frameRect();

        auto usedPosition = RenderStyle::usedFloat(*floatingObject->renderer());
        auto logicalPosition = (usedPosition == UsedFloat::Left) == placedFloatsIsLeftToRight ? Layout::PlacedFloats::Item::Position::Start : Layout::PlacedFloats::Item::Position::End;

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

        auto shapeOutsideInfo = floatingObject->renderer()->shapeOutsideInfo();
        RefPtr shape = shapeOutsideInfo ? &shapeOutsideInfo->computedShape() : nullptr;

        placedFloats.add({ logicalPosition, boxGeometry, logicalRect.location(), WTFMove(shape) });
    }
}

bool LineLayout::isPaginated() const
{
    return m_inlineContent && m_inlineContent->isPaginated();
}

bool LineLayout::hasEllipsisInBlockDirectionOnLastFormattedLine() const
{
    if (!m_inlineContent)
        return false;

    for (auto& line : m_inlineContent->displayContent().lines | std::views::reverse) {
        if (!line.hasInflowContent()) {
            // Out-of-flow content could initiate a line with no inline content.
            continue;
        }
        auto lastFormattedLineEllipsis = line.ellipsis();
        return lastFormattedLineEllipsis && lastFormattedLineEllipsis->type == InlineDisplay::Line::Ellipsis::Type::Block;
    }
    return false;
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
    auto offsetAndGaps = m_inlineContent->firstLinePaginationOffset() + m_inlineContent->clearBeforeAfterGaps();
    return LayoutUnit { contentHeight + offsetAndGaps };
}

LayoutUnit LineLayout::contentLogicalHeight() const
{
    if (!m_inlineContent)
        return { };

    auto& lines = m_inlineContent->displayContent().lines;
    if (lines.isEmpty()) {
        // Out-of-flow only content (and/or with floats) may produce blank inline content.
        return { };
    }

    auto contentHeight = lastLineWithInflowContent(lines).lineBoxLogicalRect().maxY() - lines.first().lineBoxLogicalRect().y();
    auto offsetAndGaps = m_inlineContent->firstLinePaginationOffset() + m_inlineContent->clearBeforeAfterGaps();
    return LayoutUnit { contentHeight + offsetAndGaps };
}

bool LineLayout::isSelfCollapsingContent() const
{
    if (!m_inlineContent || !m_inlineContent->hasInflowContent())
        return true;

    auto& displayContent = m_inlineContent->displayContent();
    for (auto& line : displayContent.lines) {
        if (line.hasInlineContent())
            return false;
        if (line.hasBlockContent()) {
            auto blockLevelBox = [&]() -> RenderBox* {
                for (auto index = line.firstBoxIndex(); index < line.lastBoxIndex(); ++index) {
                    if (displayContent.boxes[index].isBlockLevelBox())
                        return dynamicDowncast<RenderBox>(displayContent.boxes[index].layoutBox().rendererForIntegration());
                    ASSERT(displayContent.boxes[index].isInlineBox());
                }
                return { };
            };
            if (auto* renderBox = blockLevelBox(); renderBox && !renderBox->isSelfCollapsingBlock())
                return false;
        }
    }
    return true;
}

size_t LineLayout::lineCount() const
{
    if (!m_inlineContent)
        return 0;
    if (!m_inlineContent->hasInflowContent())
        return 0;

    auto& lines = m_inlineContent->displayContent().lines;
    if (lines.isEmpty())
        return 0;
    // In some cases (trailing out-of-flow, non-contentful content after <br>) we produce last line with no content but root inline box only.
    return lines.last().hasInflowContent() ? lines.size() : lines.size() - 1;
}

bool LineLayout::hasInkOverflow() const
{
    return m_inlineContent && m_inlineContent->hasInkOverflow();
}

LayoutUnit LineLayout::firstLineBaseline() const
{
    if (!m_inlineContent || m_inlineContent->displayContent().boxes.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto baselineForLineOrBlock = [&](auto& line) -> std::optional<LayoutUnit> {
        if (!line.hasInflowContent())
            return { };

        if (auto* blockLevelBox = m_inlineContent->blockLevelBoxForLine(line)) {
            // For block-in-inline look for the baseline of the child box.
            CheckedRef blockRenderer = downcast<RenderBox>(*blockLevelBox->layoutBox().rendererForIntegration());
            return blockRenderer->firstLineBaseline();
        }
        return baselineForLine(line);
    };

    for (auto& line : m_inlineContent->displayContent().lines) {
        if (auto baseline = baselineForLineOrBlock(line))
            return *baseline;
    }
    return baselineForLine(m_inlineContent->displayContent().lines.first());
}

LayoutUnit LineLayout::lastLineBaseline() const
{
    if (!m_inlineContent || m_inlineContent->displayContent().lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto baselineForLineOrBlock = [&](auto& line) -> std::optional<LayoutUnit> {
        if (!line.hasInflowContent())
            return { };

        if (auto* blockLevelBox = m_inlineContent->blockLevelBoxForLine(line)) {
            // For block-in-inline look for the baseline of the child box.
            CheckedRef blockRenderer = downcast<RenderBox>(*blockLevelBox->layoutBox().rendererForIntegration());
            return blockRenderer->lastLineBaseline();
        }
        return baselineForLine(line);
    };

    for (auto& line : m_inlineContent->displayContent().lines | std::views::reverse) {
        if (auto baseline = baselineForLineOrBlock(line))
            return *baseline;
    }
    return baselineForLine(m_inlineContent->displayContent().lines.last());
}

LayoutUnit LineLayout::baselineForLine(const InlineDisplay::Line& line) const
{
    auto rootWritingMode = rootLayoutBox().writingMode();
    auto baseline = line.baseline();
    if (rootWritingMode.isLineInverted())
        baseline = line.lineBoxLogicalRect().height() - baseline;

    switch (rootWritingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
    case FlowDirection::BottomToTop:
        return LayoutUnit { line.lineBoxTop() + baseline };
    case FlowDirection::LeftToRight:
    case FlowDirection::RightToLeft:
        return LayoutUnit { line.lineBoxLeft() + baseline };
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

    if (!adjustments.isEmpty()) {
        adjustLinePositionsForPagination(*m_inlineContent, adjustments);
        m_inlineContent->setFirstLinePaginationOffset(adjustments[0].offset);
    }

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

    flow().addLayoutOverflow(LayoutRect { m_inlineContent->scrollableOverflow() });
    if (!flow().hasNonVisibleOverflow())
        flow().addVisualOverflow(LayoutRect { m_inlineContent->inkOverflow() });
}

InlineContent& LineLayout::ensureInlineContent()
{
    if (!m_inlineContent)
        m_inlineContent = makeUnique<InlineContent>(flow());
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
    if (!m_inlineContent || !m_inlineContent->hasInflowContent())
        return { };

    return InlineIterator::inlineBoxFor(*m_inlineContent, m_inlineContent->displayContent().boxes[0]);
}

InlineIterator::LineBoxIterator LineLayout::firstLineBox() const
{
    if (!m_inlineContent || !m_inlineContent->hasInflowContent())
        return { };

    return { InlineIterator::LineBoxIteratorModernPath(*m_inlineContent, 0) };
}

InlineIterator::LineBoxIterator LineLayout::lastLineBox() const
{
    if (!m_inlineContent || !m_inlineContent->hasInflowContent())
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

    // FIXME: This preserves existing output.
    if (!m_inlineContent->hasInflowContent())
        return { };

    auto borderBoxLogicalRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(layoutState().geometryForBox(*renderInline.layoutBox())) };
    return flow().writingMode().isHorizontal() ? borderBoxLogicalRect : borderBoxLogicalRect.transposedRect();
}

LayoutRect LineLayout::inkOverflowBoundingBoxRectFor(const RenderInline& renderInline) const
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
        auto rect = inlineBox.visualRectIgnoringBlockDirection();
        if (result.isEmpty() || !rect.isEmpty())
            result.append(rect);
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

bool LineLayout::isContentConsideredStale() const
{
    auto* rootRenderer = m_rootLayoutBox->rendererForIntegration();
    if (!rootRenderer)
        return true;
    if (rootRenderer->needsLayout())
        return true;
    if (rootRenderer->style().isSkippedRootOrSkippedContent())
        return true;
    if (m_lineDamage && m_lineDamage->hasDetachedContent())
        return true;
    return false;
}

void LineLayout::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, const RenderInline* layerRenderer)
{
    if (!m_inlineContent)
        return;

    if (isContentConsideredStale()) {
        ASSERT_NOT_REACHED();
        return;
    }

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
        case PaintPhase::ChildBlockBackground:
        case PaintPhase::ChildBlockBackgrounds:
        case PaintPhase::Float:
            // These phases are only needed in blocks-in-inline case.
            return m_inlineContent->hasBlockLevelBoxes();
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
    if (!m_inlineContent)
        return false;

    // All real inline content is foreground.
    if (hitTestAction != HitTestForeground && !m_inlineContent->hasBlockLevelBoxes())
        return false;
    if (hitTestAction == HitTestBlockBackground)
        return false;

    if (isContentConsideredStale()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    auto hitTestBoundingBox = locationInContainer.boundingBox();
    hitTestBoundingBox.moveBy(-accumulatedOffset);
    auto boxRange = m_inlineContent->boxesForRect(hitTestBoundingBox);

    LayerPaintScope layerPaintScope(layerRenderer);

    for (auto& box : boxRange | std::views::reverse) {
        if (!layerPaintScope.testIsIncludesAndUpdate(box))
            continue;

        bool visibleForHitTesting = request.userTriggered() ? box.isVisible() : box.isVisibleIgnoringUsedVisibility();
        if (!visibleForHitTesting)
            continue;

        auto shouldHitTestForPhase = [&] {
            switch (hitTestAction) {
            case HitTestForeground:
                // Inline boxes around block-in-inline are hit tested in block background phase.
                return !m_inlineContent->isInlineBoxWrapperForBlockLevelBox(box);
            case HitTestChildBlockBackground:
                return box.isBlockLevelBox() || m_inlineContent->isInlineBoxWrapperForBlockLevelBox(box);
            case HitTestChildBlockBackgrounds:
            case HitTestFloat:
                return box.isBlockLevelBox();
            case HitTestBlockBackground:
                break;
            }
            ASSERT_NOT_REACHED();
            return false;
        };

        if (!shouldHitTestForPhase())
            continue;

        auto& renderer = *box.layoutBox().rendererForIntegration();

        if (box.isBlockLevelBox()) {
            auto& renderBox = downcast<RenderBox>(renderer);
            auto childHitTest = hitTestAction == HitTestChildBlockBackgrounds ? HitTestChildBlockBackground : hitTestAction;
            LayoutPoint childPoint = flippedContentOffsetIfNeeded(flow(), renderBox, accumulatedOffset);
            if (!renderBox.hasSelfPaintingLayer() && renderBox.nodeAtPoint(request, result, locationInContainer, childPoint, childHitTest))
                return true;
            continue;
        }

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
        line.moveInBlockDirection(blockShift);

    auto deltaX = isHorizontalWritingMode ? 0_lu : blockShift;
    auto deltaY = isHorizontalWritingMode ? blockShift : 0_lu;
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
            if (CheckedPtr layerRenderer = dynamicDowncast<RenderLayerModelObject>(layoutBox.rendererForIntegration())) {
                if (CheckedPtr layer = layerRenderer->layer())
                    layer->setStaticBlockPosition(layer->staticBlockPosition() + blockShift);
            }
            if (CheckedPtr renderBox = dynamicDowncast<RenderBox>(layoutBox.rendererForIntegration()))
                renderBox->move(deltaX, deltaY);
        }
    }
}

bool LineLayout::insertedIntoTree(const RenderElement& parent, RenderObject& child)
{
    if (flow().style().isSkippedRootOrSkippedContent())
        return false;

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
    if (flow().style().isSkippedRootOrSkippedContent())
        return false;

    if (!child.everHadLayout()) {
        ensureLineDamage().addDetachedBox(BoxTreeUpdater { flow() }.remove(parent, child));
        return false;
    }

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

bool LineLayout::updateTextContent(const RenderText& textRenderer, std::optional<size_t> offset, size_t oldLength)
{
    if (flow().style().isSkippedRootOrSkippedContent())
        return false;

    if (!m_inlineContent) {
        // This is supposed to be only called on partial layout, but
        // RenderText::setText may be (force) called after min/max size computation and before layout.
        // We may need to invalidate anyway to clean up inline item list.
        return false;
    }

    BoxTreeUpdater::updateContent(textRenderer);

    auto invalidation = Layout::InlineInvalidation { ensureLineDamage(), m_inlineContentCache.inlineItems().content(), m_inlineContent->displayContent() };
    auto& inlineTextBox = *textRenderer.layoutBox();
    if (!offset) {
        // Text content is entirely replaced.
        return invalidation.textInserted(inlineTextBox, { 0 });
    }

    if (*offset == oldLength) {
        // This is essentially just an append.
        return invalidation.textInserted(inlineTextBox);
    }

    int delta = inlineTextBox.content().length() - oldLength;
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

bool LineLayout::hasBlocks() const
{
    return m_inlineContent && m_inlineContent->hasBlockLevelBoxes();
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
