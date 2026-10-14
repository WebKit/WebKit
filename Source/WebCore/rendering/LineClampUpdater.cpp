/**
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "LineClampUpdater.h"

#include "LayoutIntegrationLineLayout.h"
#include "LocalFrameView.h"
#include "LocalFrameViewInlines.h"
#include "RenderBlockFlow.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "StyleDisplay.h"
#include "StyleMaximumLines.h"

namespace WebCore {

bool LineClampUpdater::shouldDiscardOverflow(const RenderBlock& blockContainer)
{
    // FIXME: continue: discard should turn the box into a fragmentation container instead (https://drafts.csswg.org/css-overflow-4/#valdef-continue-discard).
    auto overflowContinue = blockContainer.style().overflowContinue();
    return overflowContinue == OverflowContinue::Discard || overflowContinue == OverflowContinue::Collapse;
}

LineClampUpdater::LineClampUpdater(const RenderBlock& blockContainer)
    : m_blockContainer(blockContainer)
{
    auto* layoutState = m_blockContainer->view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return;

    m_previousLineClamp = layoutState->lineClamp();
    auto maximumLinesForBlockContainer = m_blockContainer->style().maxLines().tryValue();
    // "If the box is a multicol container, the behavior is the same as continue: auto."
    // https://drafts.csswg.org/css-overflow-4/#continue
    if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(blockContainer); blockFlow && blockFlow->multiColumnFlow())
        maximumLinesForBlockContainer = { };
    if (blockContainer.isFieldset() || (layoutState->legacyLineClamp() && blockContainer.isNonReplacedAtomicInlineLevelBox()) || blockContainer.isFloatingOrOutOfFlowPositioned()) {
        // Legacy line clamp does not cross into the interior of an atomic inline-level box.
        layoutState->setLineClamp({ });

        m_skippedLegacyLineClampToRestore = layoutState->legacyLineClamp();
        layoutState->setLegacyLineClamp({ });
        // The box may still clamp its own content.
        if (!maximumLinesForBlockContainer && !isAutoLineClampRoot())
            return;
    }

    if (maximumLinesForBlockContainer) {
        // Ignore top level legacy line clamp for now.
        if (m_blockContainer->style().overflowContinue() == OverflowContinue::WebkitLegacy)
            return;
        // New, top level line clamp.
        m_isLineClampRoot = true;
        layoutState->setLineClamp(RenderLayoutState::LineClamp { static_cast<size_t>(maximumLinesForBlockContainer->value), shouldDiscardOverflow(m_blockContainer), { } });
        return;
    }

    if (isAutoLineClampRoot()) {
        // Until layout finds the auto clamp point (see autoClampPoint), content is laid out as before the clamp point of a line-clamp container.
        m_isLineClampRoot = true;
        layoutState->setLineClamp(RenderLayoutState::LineClamp { std::numeric_limits<size_t>::max(), true, { }, true });
        return;
    }

    if (m_previousLineClamp) {
        // Propagated line clamp.
        if (blockContainer.establishesIndependentFormattingContext() || blockContainer.style().display() == Style::DisplayType::RubyText) {
            // Contents of descendants that establish independent formatting contexts are skipped over while counting line boxes,
            // and a ruby annotation belongs to the line of its base: it is clamped with that line, not line by line on its own.
            layoutState->setLineClamp({ });
            return;
        }
        auto effectiveShouldDiscard = m_previousLineClamp->shouldDiscardOverflow || shouldDiscardOverflow(m_blockContainer);
        layoutState->setLineClamp(RenderLayoutState::LineClamp { m_previousLineClamp->maximumLines, effectiveShouldDiscard, { }, m_previousLineClamp->isStableTextWrap });
        return;
    }
}

LineClampUpdater::~LineClampUpdater()
{
    auto* layoutState = m_blockContainer->view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return;

    if (m_skippedLegacyLineClampToRestore)
        layoutState->setLegacyLineClamp(m_skippedLegacyLineClampToRestore);

    if (!m_previousLineClamp) {
        layoutState->setLineClamp({ });
        return;
    }

    auto lineClamp = layoutState->lineClamp();
    if (!lineClamp || m_blockContainer->establishesIndependentFormattingContext()) {
        // "Only line boxes in the same block formatting context are counted: the contents of descendants
        // that establish independent formatting contexts are skipped over while counting line boxes."
        // https://drafts.csswg.org/css-overflow-4/#max-lines
        layoutState->setLineClamp(m_previousLineClamp);
        return;
    }

    size_t lineCount = m_isLineClampRoot ? 0 : m_previousLineClamp->maximumLines - std::min(m_previousLineClamp->maximumLines, lineClamp->maximumLines);
    if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(m_blockContainer.get()); blockFlow && blockFlow->childrenInline())
        lineCount = blockFlow->lineCount();
    auto hasConstrainedTextWrapContent = m_previousLineClamp->hasConstrainedTextWrapContent || lineClamp->hasConstrainedTextWrapContent;
    layoutState->setLineClamp(RenderLayoutState::LineClamp { m_previousLineClamp->maximumLines - std::min(m_previousLineClamp->maximumLines, lineCount), m_previousLineClamp->shouldDiscardOverflow, m_previousLineClamp->clampAfterBox, m_previousLineClamp->isStableTextWrap, hasConstrainedTextWrapContent });
}

bool LineClampUpdater::isAutoLineClampRoot() const
{
    // line-clamp: auto (or just a block-ellipsis value) leaves max-lines at auto on a line-clamp container.
    CheckedRef style = m_blockContainer->style();
    if (!style->maxLines().isAuto() || !shouldDiscardOverflow(m_blockContainer))
        return false;
    // "If the box is a multicol container, the behavior is the same as continue: auto."
    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(m_blockContainer.get());
    return !blockFlow || !blockFlow->multiColumnFlow();
}

void LineClampUpdater::setMaximumLines(size_t maximumLines)
{
    auto* layoutState = m_blockContainer->view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return;
    m_isLineClampRoot = true;
    layoutState->setLineClamp(RenderLayoutState::LineClamp { maximumLines, shouldDiscardOverflow(m_blockContainer), { } });
}

void LineClampUpdater::setClampAfterBox(const RenderBox& clampAfterBox)
{
    auto* layoutState = m_blockContainer->view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return;
    m_isLineClampRoot = true;
    // Lines before the clamp point do not run out of the budget (and no line gets the block ellipsis).
    layoutState->setLineClamp(RenderLayoutState::LineClamp { std::numeric_limits<size_t>::max(), shouldDiscardOverflow(m_blockContainer), &clampAfterBox });
}

void LineClampUpdater::resetLineClamp()
{
    ASSERT(m_isLineClampRoot);
    auto* layoutState = m_blockContainer->view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return;
    layoutState->setLineClamp({ });
}

// The content bottom of the box in its own coordinates, where its block-end padding (and scrollbar) starts.
static LayoutUnit contentLogicalBottom(const RenderBox& box)
{
    return box.logicalHeight() - box.borderAndPaddingAfter() - box.scrollbarLogicalHeight();
}

// "the block size the box would have if its automatic block size were infinite", from the line-clamp container's border box top edge.
static std::optional<LayoutUnit> autoClampBlockSizeLimit(const RenderBlock& lineClampContainer)
{
    auto maximumContentHeight = lineClampContainer.constrainContentBoxLogicalHeightByMinMax(lineClampContainer.computeContentLogicalHeight(lineClampContainer.style().logicalHeight(), std::nullopt).value_or(LayoutUnit::max()), std::nullopt);
    if (maximumContentHeight == LayoutUnit::max())
        return { };
    return lineClampContainer.borderAndPaddingBefore() + maximumContentHeight;
}

bool LineClampUpdater::fitsAutoClampBlockSizeLimit() const
{
    auto blockSizeLimit = autoClampBlockSizeLimit(m_blockContainer);
    return !blockSizeLimit || contentLogicalBottom(m_blockContainer) <= *blockSizeLimit;
}

Vector<LineClampUpdater::AutoClampPoint> LineClampUpdater::autoClampPoints() const
{
    if (!isAutoLineClampRoot() || fitsAutoClampBlockSizeLimit())
        return { };
    auto& lineClampContainer = m_blockContainer.get();
    auto blockSizeLimit = *autoClampBlockSizeLimit(lineClampContainer);

    // "The auto clamp point will be set to the last possible clamp point such that, for it and all previous possible clamp points,
    // the line-clamp container's automatic block size (as determined below) is not greater than the block size the box would have
    // if its automatic block size were infinite."
    // https://drafts.csswg.org/css-overflow-4/#line-clamp-containers
    // These are the possible clamp points up to the last one whose content ends within that block size. Block-end margins are not taken
    // into account here: block layout collapses them once it lays out the content clamped (see RenderBlockFlow::layoutInFlowChildren).
    // The lines of this block formatting context in the subtree, and the ones of them that end within the block size limit.
    auto countLines = [&](const RenderBox& root) -> std::pair<size_t, size_t> {
        size_t lineCount = 0;
        size_t fittingLineCount = 0;
        for (CheckedPtr<const RenderObject> descendant = &root; descendant;) {
            CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(*descendant);
            auto isSkippedOver = descendant != &lineClampContainer && (descendant->isFloatingOrOutOfFlowPositioned() || (blockFlow && (blockFlow->establishesIndependentFormattingContext() || blockFlow->style().display() == Style::DisplayType::RubyText)));
            if (isSkippedOver) {
                descendant = descendant->nextInPreOrderAfterChildren(&root);
                continue;
            }
            if (blockFlow && blockFlow->inlineLayout()) {
                // The line-clamp container's block size limit in this block's coordinates, less the border and padding (of the block and its ancestors) still following its lines.
                auto availableHeightForLines = blockSizeLimit;
                for (CheckedPtr<const RenderBlock> ancestor = blockFlow; ancestor && ancestor != &lineClampContainer; ancestor = ancestor->containingBlock())
                    availableHeightForLines -= ancestor->logicalTop() + ancestor->borderAndPaddingAfter();
                lineCount += blockFlow->inlineLayout()->lineCountForHeight(LayoutUnit::max()).first;
                fittingLineCount += blockFlow->inlineLayout()->lineCountForHeight(availableHeightForLines).first;
            }
            descendant = descendant->nextInPreOrder(&root);
        }
        return { lineCount, fittingLineCount };
    };

    // "The start of the line-clamp container."
    auto clampPoints = Vector<AutoClampPoint> { AutoClampPoint { size_t { 0 } } };
    auto appendLineClampPoints = [&](size_t lineCountBefore, size_t lineCount) {
        for (size_t line = 1; line <= lineCount; ++line)
            clampPoints.append(lineCountBefore + line);
    };

    if (lineClampContainer.childrenInline()) {
        auto [lineCount, fittingLineCount] = countLines(lineClampContainer);
        appendLineClampPoints(0, fittingLineCount);
        return clampPoints;
    }

    // "A point between two in-flow block-level sibling boxes in the line-clamp container's block formatting context."
    size_t lineCountBeforeChild = 0;
    for (CheckedPtr child = lineClampContainer.firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        auto [lineCount, fittingLineCount] = countLines(*child);
        if (child->logicalBottom() > blockSizeLimit) {
            // A block keeps its set height whatever its content, and its min-height keeps it at least that tall, so no clamp point inside it fits either.
            CheckedRef childStyle = child->style();
            auto fixedMinimumHeight = childStyle->logicalMinHeight().tryFixed();
            auto minimumHeightOverflows = fixedMinimumHeight && child->logicalTop() + LayoutUnit { fixedMinimumHeight->resolveZoom(childStyle->usedZoomForLength()) } > blockSizeLimit;
            if (!childStyle->logicalHeight().isAuto() || minimumHeightOverflows)
                fittingLineCount = 0;
            appendLineClampPoints(lineCountBeforeChild, fittingLineCount);
            return clampPoints;
        }
        appendLineClampPoints(lineCountBeforeChild, lineCount);
        lineCountBeforeChild += lineCount;
        // "Ignoring any intervening absolutely positioned elements or element closing boundaries" the last line box of this
        // child immediately precedes the clamp point after it and gets the block ellipsis. Without a line box, no line does.
        // https://drafts.csswg.org/css-overflow-4/#block-ellipsis
        if (!lineCount)
            clampPoints.append(CheckedRef<const RenderBox> { *child });
    }
    return clampPoints;
}

bool LineClampUpdater::hasConstrainedTextWrapContent() const
{
    auto lineClamp = m_blockContainer->view().frameView().layoutContext().layoutState()->lineClamp();
    return lineClamp && lineClamp->hasConstrainedTextWrapContent;
}

void LineClampUpdater::setIsStableTextWrap(bool isStableTextWrap)
{
    auto* layoutState = m_blockContainer->view().frameView().layoutContext().layoutState();
    if (auto lineClamp = layoutState->lineClamp()) {
        lineClamp->isStableTextWrap = isStableTextWrap;
        lineClamp->hasConstrainedTextWrapContent = false;
        layoutState->setLineClamp(lineClamp);
    }
}

void LineClampUpdater::setAutoClampPoint(const AutoClampPoint& clampPoint)
{
    WTF::switchOn(clampPoint, [&](size_t maximumLines) {
        setMaximumLines(maximumLines);
    }, [&](const CheckedRef<const RenderBox>& clampAfterBox) {
        setClampAfterBox(clampAfterBox);
    });
}

// "Any absolutely positioned box which has an invisible box within its containing block chain, and all of its descendants."
static bool isInsideInvisibleBox(const RenderObject& renderer, const RenderElement& invisibleBox)
{
    if (!renderer.isOutOfFlowPositioned())
        return true;
    // The containing block may be an inline box (unlike containingBlock(), container() returns it).
    CheckedPtr containingBlock = renderer.container();
    return containingBlock && (containingBlock == &invisibleBox || containingBlock->isDescendantOf(&invisibleBox));
}

static void setIsForceHiddenByLineClamp(RenderElement& renderer, const RenderElement& invisibleBox, bool isHidden)
{
    renderer.setIsForceHiddenByLineClamp(isHidden);
    // Invisible boxes are not laid out (see skipLayoutForForcedHidden), so a box that becomes visible again needs layout.
    if (!isHidden)
        renderer.setNeedsLayout(MarkingBehavior::MarkOnlyThis);
    for (CheckedRef child : childrenOfType<RenderElement>(renderer)) {
        if (isInsideInvisibleBox(child, invisibleBox))
            setIsForceHiddenByLineClamp(child, invisibleBox, isHidden);
    }
}

static void clearNeedsLayout(RenderObject& renderer, const RenderBox& invisibleBox)
{
    // Descendants first: a block expects the out-of-flow boxes it is the containing block for to be clean by the time it is.
    if (CheckedPtr element = dynamicDowncast<RenderElement>(renderer)) {
        for (CheckedRef child : childrenOfType<RenderObject>(*element)) {
            if (isInsideInvisibleBox(child, invisibleBox))
                clearNeedsLayout(child, invisibleBox);
        }
    }
    renderer.clearNeedsLayout();
}

void LineClampUpdater::skipLayoutForForcedHidden(RenderBox& renderer)
{
    // Like inline content after the clamp point (see InlineFormattingContext::resetBoxGeometriesForDiscardedContent), invisible boxes are not laid out,
    // and "its automatic block size will not take into account any invisible boxes".
    // https://drafts.csswg.org/css-overflow-4/#line-clamp-containers
    renderer.setBorderBoxInContainer({ });
    clearNeedsLayout(renderer, renderer);
}

bool LineClampUpdater::isAfterClampPoint(const RenderObject& renderer)
{
    // "Within a line-clamp container, the following boxes and line boxes become invisible boxes:
    // Any in-flow or floating boxes that follow the clamp point in the box tree."
    // https://drafts.csswg.org/css-overflow-4/#line-clamp-containers
    auto lineClamp = renderer.view().frameView().layoutContext().layoutState()->lineClamp();
    return lineClamp && lineClamp->shouldDiscardOverflow && !lineClamp->maximumLines;
}

bool LineClampUpdater::isInsideLineClampContainer(const RenderObject& renderer)
{
    auto lineClamp = renderer.view().frameView().layoutContext().layoutState()->lineClamp();
    return lineClamp && lineClamp->shouldDiscardOverflow;
}

std::optional<LayoutRect> LineClampUpdater::blockEndClipRect(const RenderBox& floatBox, const RenderElement& renderer)
{
    if (!floatBox.isClippedByLineClamp())
        return { };
    // The line-clamp container whose block formatting context the float is in.
    auto lineClampContainerForFloat = [&] -> CheckedPtr<const RenderBlockFlow> {
        CheckedPtr formattingContextRoot = floatBox.containingBlock();
        while (formattingContextRoot && !formattingContextRoot->createsNewFormattingContext())
            formattingContextRoot = formattingContextRoot->containingBlock();
        CheckedPtr lineClampContainer = dynamicDowncast<RenderBlockFlow>(formattingContextRoot.get());
        if (!lineClampContainer || !lineClampContainer->establishesLineClampContainer())
            return nullptr;
        return lineClampContainer;
    };
    CheckedPtr lineClampContainer = lineClampContainerForFloat();
    if (!lineClampContainer)
        return { };

    // Within a line-clamp container, floats "must be visually clipped to the unshaped block-end content edge of the line-clamp container".
    // https://drafts.csswg.org/css-overflow-4/#line-clamp-containers
    // The renderer may be inside the line-clamp container (e.g. the float itself) or outside of it, so this maps through absolute coordinates.
    auto contentBoxQuad = lineClampContainer->localToAbsoluteQuad(FloatQuad { FloatRect { lineClampContainer->contentBoxRect() } });
    auto contentBoxRect = LayoutRect { renderer.absoluteToLocalQuad(contentBoxQuad).boundingBox() };
    auto clipRect = LayoutRect::infiniteRect();
    switch (lineClampContainer->writingMode().blockDirection()) {
    case FlowDirection::TopToBottom:
        clipRect.shiftMaxYEdgeTo(contentBoxRect.maxY());
        break;
    case FlowDirection::BottomToTop:
        clipRect.shiftYEdgeTo(contentBoxRect.y());
        break;
    case FlowDirection::LeftToRight:
        clipRect.shiftMaxXEdgeTo(contentBoxRect.maxX());
        break;
    case FlowDirection::RightToLeft:
        clipRect.shiftXEdgeTo(contentBoxRect.x());
        break;
    }
    return clipRect;
}

void LineClampUpdater::setIsForcedHidden(RenderElement& renderer, bool isHidden)
{
    if (renderer.isForceHiddenByLineClamp() == isHidden)
        return;
    setIsForceHiddenByLineClamp(renderer, renderer, isHidden);
}


} // namespace WebCore
