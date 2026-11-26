/*
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
#include "BlockHitTester.h"

#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LayoutIntegrationLineLayout.h"
#include "LocalFrameViewInlines.h"
#include "PaintOrderIterator.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderSVGText.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Paint {

BlockHitTester::BlockHitTester(RenderBlock& block, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& location, const LayoutPoint& accumulatedOffset, HitTestAction action)
    : m_block(block)
    , m_request(request)
    , m_result(result)
    , m_location(location)
    , m_accumulatedOffset(accumulatedOffset)
    , m_action(action)
{
}

static HitTestAction actionForChildren(HitTestAction action)
{
    return action == HitTestAction::ChildBlockBackgrounds ? HitTestAction::ChildBlockBackground : action;
}

// A stacking container is hit tested one action per walk, in the reverse of the order it paints in.
static constexpr std::array stackingContainerActions { HitTestAction::Foreground, HitTestAction::Float, HitTestAction::ChildBlockBackgrounds, HitTestAction::BlockBackground };

static bool isBackgroundAction(HitTestAction action)
{
    return action == HitTestAction::BlockBackground || action == HitTestAction::ChildBlockBackground;
}

bool BlockHitTester::hitTest(RenderBlock& block, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& location, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    if (PaintOrderIterator::canIterate(block))
        return BlockHitTester { block, request, result, location, accumulatedOffset, action }.hitTestSubtree();
    return hitTestBlock(block, request, result, location, accumulatedOffset, action);
}

bool BlockHitTester::hitTestBlock(RenderBlock& block, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& location, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    auto adjustedLocation = accumulatedOffset + block.location();
    auto localOffset = toLayoutSize(adjustedLocation);

    // Check if we need to do anything at all.
    if (!block.hitTestVisualOverflow(location, accumulatedOffset))
        return false;

    if (isBackgroundAction(action) && block.visibleToHitTesting(request) && block.isPointInOverflowControl(result, location.point(), adjustedLocation)) {
        block.updateHitTestResult(result, location.point() - localOffset);
        // FIXME: isPointInOverflowControl() doesn't handle rect-based tests yet.
        if (result.addNodeToListBasedTestResult(protect(block.nodeForHitTest()).get(), request, location) == HitTestProgress::Stop)
            return true;
    }

    if (!block.hitTestClipPath(location, accumulatedOffset))
        return false;

    if (!isSkippedContentRoot(block)) {
        // If we have clipping, then we can't have any spillout.
        bool useClip = block.hasControlClip() || block.hasNonVisibleOverflow();
        bool checkChildren = !useClip || (block.hasControlClip() ? location.intersects(block.controlClipRect(adjustedLocation)) : location.intersects(block.overflowClipRect(adjustedLocation, OverlayScrollbarSizeRelevancy::IncludeOverlayScrollbarSize)));
        if (checkChildren && hitTestChildren(block, request, result, location, adjustedLocation, action))
            return true;

        if (!checkChildren && hitTestExcludedChildrenInBorder(block, request, result, location, adjustedLocation, action))
            return true;
    }

    if (!block.hitTestBorderRadius(location, accumulatedOffset))
        return false;

    // Now hit test our background
    if (isBackgroundAction(action)) {
        LayoutRect boundsRect(adjustedLocation, block.borderBoxSize());
        if (block.visibleToHitTesting(request) && location.intersects(boundsRect)) {
            block.updateHitTestResult(result, block.physicalToBlockStartRelative(location.point() - localOffset));
            if (result.addNodeToListBasedTestResult(protect(block.nodeForHitTest()).get(), request, location, boundsRect) == HitTestProgress::Stop)
                return true;
        }
    }

    return false;
}

bool BlockHitTester::hitTestChildren(RenderBlock& block, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& location, const LayoutPoint& adjustedLocation, HitTestAction action)
{
    ASSERT(!isSkippedContentRoot(block));
    // Grids hit test with PaintOrderIterator.
    ASSERT(!is<RenderGrid>(block));

    if (CheckedPtr flexBox = dynamicDowncast<RenderFlexibleBox>(block))
        return flexBox->flexLayout().hitTest(request, result, location, adjustedLocation, action);

    // Hit test descendants first.
    auto localOffset = toLayoutSize(adjustedLocation);
    auto scrolledOffset = localOffset - toLayoutSize(block.scrollPosition());

    if (hitTestContents(block, request, result, location, toLayoutPoint(scrolledOffset), action)) {
        block.updateHitTestResult(result, block.physicalToBlockStartRelative(location.point() - localOffset));
        return true;
    }
    return false;
}

bool BlockHitTester::hitTestContents(RenderBlock& block, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& location, const LayoutPoint& scrolledOffset, HitTestAction action)
{
    if (block.childrenInline() && !block.isRenderTable()) {
        if (block.style().isSkippedRootOrSkippedContent())
            return false;
        return hitTestInlineChildren(block, request, result, location, scrolledOffset, action);
    }

    for (CheckedPtr child = block.lastChildBox(); child; child = child->previousSiblingBox()) {
        if (hitTestChild(block, *child, request, result, location, block.blockStartRelativeToPhysicalForChild(*child, scrolledOffset), action, HitTestBlockType::Block))
            return true;
    }
    return false;
}

bool BlockHitTester::hitTestInlineChildren(RenderBlock& block, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& location, const LayoutPoint& scrolledOffset, HitTestAction action)
{
    ASSERT(block.childrenInline());

    if (CheckedPtr svgText = dynamicDowncast<RenderSVGText>(block))
        return svgText->hitTestInlineChildren(request, result, location, scrolledOffset, action);

    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(block);
    CheckedPtr inlineLayout = blockFlow ? blockFlow->inlineLayout() : nullptr;
    if (!inlineLayout)
        return false;

    if (action == HitTestAction::Float && inlineLayout->hasFloats()) {
        // The floats and blocks-in-inline, in the reverse of the order they paint in.
        for (auto& child : inlineLayout->floatsAndBlockLevelBoxesInTreeOrder() | std::views::reverse) {
            if (child->hasSelfPaintingLayer())
                continue;
            auto childPoint = block.blockStartRelativeToPhysicalForChild(child, scrolledOffset);
            if (child->isFloating() ? hitTestFloat(block, child, request, result, location, childPoint) : child->nodeAtPoint(request, result, location, childPoint, action))
                return true;
        }
        return false;
    }

    return inlineLayout->hitTest(request, result, location, scrolledOffset, action);
}

bool BlockHitTester::hitTestChild(RenderBlock& block, RenderBox& child, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& location, const LayoutPoint& childPoint, HitTestAction action, HitTestBlockType hitTestType)
{
    if (child.hasSelfPaintingLayer())
        return false;
    // A stacking container is hit tested all actions at once, in the foreground action of its container.
    if (hitTestType == HitTestBlockType::StackingContainer)
        return action == HitTestAction::Foreground && child.hitTest(request, result, location, childPoint);
    if (child.isFloating()) {
        // Only a RenderBlockFlow places floats.
        return action == HitTestAction::Float && is<RenderBlockFlow>(block) && hitTestFloat(block, child, request, result, location, childPoint);
    }
    return child.nodeAtPoint(request, result, location, childPoint, actionForChildren(action));
}

bool BlockHitTester::hitTestFloat(RenderBlock& block, RenderBox& floatBox, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& location, const LayoutPoint& floatBoxLocation)
{
    ASSERT(floatBox.isFloating() && !floatBox.hasSelfPaintingLayer());

    // Layout skips the floats that line-clamp hides after the clamp point.
    if (floatBox.isForceHiddenByLineClamp())
        return false;

    auto adjustedFloatBoxLocation = floatBoxLocation;
    if (CheckedPtr renderView = dynamicDowncast<RenderView>(block))
        adjustedFloatBoxLocation += toLayoutSize(protect(renderView->frameView())->scrollPosition());

    // A float is hit tested all actions at once.
    if (!floatBox.hitTest(request, result, location, adjustedFloatBoxLocation))
        return false;
    block.updateHitTestResult(result, location.point() - toLayoutSize(adjustedFloatBoxLocation));
    return true;
}

bool BlockHitTester::hitTestExcludedChildrenInBorder(RenderBlock& block, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& location, const LayoutPoint& adjustedLocation, HitTestAction action)
{
    if (!block.isFieldset())
        return false;

    CheckedPtr legend = block.findFieldsetLegend();
    if (!legend || !legend->isExcludedFromNormalLayout() || legend->hasSelfPaintingLayer())
        return false;

    return legend->nodeAtPoint(request, result, location, block.blockStartRelativeToPhysicalForChild(*legend, adjustedLocation), actionForChildren(action));
}

// A fieldset's legend is not a grid or flex item. It paints beneath them, so it is hit tested after them.
bool BlockHitTester::hitTestLegendAfterItems(RenderBlock& container, HitTestAction action, const LayoutPoint& adjustedLocation)
{
    if (is<RenderGrid>(container))
        return hitTestExcludedChildrenInBorder(container, m_request, m_result, m_location, adjustedLocation, action);

    // FIXME: Grid and flex containers hit test the legend differently. Grid does it in every action, without the
    // scroll offset. Flex does it below. They should do the same.
    // A flex container hits its legend in the foreground action, with all actions at once.
    if (action != HitTestAction::Foreground || !container.isFieldset())
        return false;
    CheckedPtr legend = container.findFieldsetLegend();
    if (!legend || !legend->isExcludedFromNormalLayout() || legend->hasSelfPaintingLayer())
        return false;
    auto scrolledOffset = container.hasNonVisibleOverflow() ? adjustedLocation - toLayoutSize(container.scrollPosition()) : adjustedLocation;
    if (!legend->hitTest(m_request, m_result, m_location, container.blockStartRelativeToPhysicalForChild(*legend, scrolledOffset)))
        return false;
    container.updateHitTestResult(m_result, container.physicalToBlockStartRelative(toLayoutPoint(m_location.point() - adjustedLocation)));
    return true;
}

// The outside marker of a list item is excluded from its normal flow and from its visual overflow. It is hit tested
// after the list item, with the same action, also when the list item itself is skipped.
bool BlockHitTester::hitTestOutsideListMarker(RenderBlock& block, HitTestAction action, const LayoutPoint& adjustedLocation)
{
    CheckedPtr listItem = dynamicDowncast<RenderListItem>(block);
    CheckedPtr marker = listItem ? listItem->markerBox() : nullptr;
    if (!marker || marker->hasSelfPaintingLayer())
        return false;
    CheckedRef<RenderBox> markerBox = *marker;
    return markerBox->nodeAtPoint(m_request, m_result, m_location, listItem->blockStartRelativeToPhysicalForChild(*marker, adjustedLocation), action);
}

bool BlockHitTester::hitTestSubtree()
{
    using Event = PaintOrderIterator::Event;

    auto iterator = PaintOrderIterator { m_block, m_accumulatedOffset, PaintOrderIterator::Direction::Reverse };

    // On a hit, each block whose contents contain it updates the result, innermost first,
    // like the recursive code does while unwinding.
    auto updateEnclosingBlocks = [&](size_t count) {
        for (size_t index = count; index--;) {
            CheckedRef block = iterator.blockAt(index);
            if (index + 1 < iterator.depth() && iterator.isStackingContainerAt(index + 1) && is<RenderBlockFlow>(block)) {
                // A float without a node for the hit leaves it to its enclosing block, with the location relative
                // to the float. The first update to set a node wins, so this comes before the one below.
                CheckedRef floatBox = iterator.blockAt(index + 1);
                block->updateHitTestResult(m_result, m_location.point() - toLayoutSize(iterator.paintOffsetAt(index + 1) - toLayoutSize(floatBox->location())));
            }
            auto localOffset = toLayoutSize(iterator.paintOffsetAt(index));
            block->updateHitTestResult(m_result, block->physicalToBlockStartRelative(m_location.point() - localOffset));
        }
    };

    for (; !iterator.atEnd(); iterator.traverseNext()) {
        ASSERT(m_parentStack.size() == iterator.depth() - (iterator.event() == Event::EnterBlock ? 1 : 0));
        CheckedRef block = iterator.block();
        auto adjustedLocation = iterator.paintOffset();
        auto accumulatedOffset = adjustedLocation - toLayoutSize(block->location());
        auto localOffset = toLayoutSize(adjustedLocation);
        auto depth = iterator.depth();

        switch (iterator.event()) {
        case Event::EnterBlock: {
            auto action = m_parentStack.isEmpty() ? m_action : actionForChildren(parent().action);
            if (iterator.isStackingContainer()) {
                bool isFirstWalk = m_stackingContainerStack.isEmpty() || m_stackingContainerStack.last().depth != depth;
                if (isFirstWalk) {
                    auto shouldHitTest = [&] {
                        // Floats are hit tested in the float action of their enclosing block flow. Line-clamp hides the ones after the clamp point.
                        if (is<RenderBlockFlow>(iterator.enclosingBlock()))
                            return action == HitTestAction::Float && !block->isForceHiddenByLineClamp();
                        // Grid and flex items are hit tested in the foreground action of their container.
                        return action == HitTestAction::Foreground;
                    };
                    if (!shouldHitTest()) {
                        iterator.skipBlock();
                        break;
                    }
                    m_stackingContainerStack.append({ depth });
                }
                action = stackingContainerActions[m_stackingContainerStack.last().actionIndex];
            }

            // Returns true on a hit on the outside list marker.
            auto skipBlock = [&] {
                // Every walk would be skipped the same way.
                if (iterator.isStackingContainer())
                    m_stackingContainerStack.removeLast();
                iterator.skipBlock();
                return hitTestOutsideListMarker(block, action, adjustedLocation);
            };

            if (!block->hitTestVisualOverflow(m_location, accumulatedOffset)) {
                if (skipBlock()) {
                    updateEnclosingBlocks(depth - 1);
                    return true;
                }
                break;
            }

            if (isBackgroundAction(action) && block->visibleToHitTesting(m_request) && block->isPointInOverflowControl(m_result, m_location.point(), adjustedLocation)) {
                block->updateHitTestResult(m_result, m_location.point() - localOffset);
                // FIXME: isPointInOverflowControl() doesn't handle rect-based tests yet.
                if (m_result.addNodeToListBasedTestResult(protect(block->nodeForHitTest()).get(), m_request, m_location) == HitTestProgress::Stop) {
                    updateEnclosingBlocks(depth - 1);
                    return true;
                }
            }

            if (!block->hitTestClipPath(m_location, accumulatedOffset)) {
                if (skipBlock()) {
                    updateEnclosingBlocks(depth - 1);
                    return true;
                }
                break;
            }

            bool shouldHitTestContent = !isSkippedContentRoot(block);
            // If we have clipping, then we can't have any spillout.
            bool useClip = block->hasControlClip() || block->hasNonVisibleOverflow();
            bool checkChildren = !useClip || (block->hasControlClip() ? m_location.intersects(block->controlClipRect(adjustedLocation)) : m_location.intersects(block->overflowClipRect(adjustedLocation, OverlayScrollbarSizeRelevancy::IncludeOverlayScrollbarSize)));
            pushParent({ action, shouldHitTestContent, checkChildren });
            break;
        }
        case Event::AfterContents: {
            // In reverse order this comes first.
            auto entered = parent();
            if (!entered.shouldHitTestContent || !entered.checkChildren)
                iterator.skipContents();
            break;
        }
        case Event::InlineContent:
            if (block->style().isSkippedRootOrSkippedContent())
                break;
            if (hitTestInlineChildren(block, m_request, m_result, m_location, iterator.scrolledOffset(), parent().action)) {
                updateEnclosingBlocks(depth);
                return true;
            }
            break;
        case Event::Child: {
            auto hitTestType = is<RenderBlockFlow>(block) ? HitTestBlockType::Block : HitTestBlockType::StackingContainer;
            if (hitTestChild(block, protect(iterator.child()), m_request, m_result, m_location, iterator.childPaintOffset(), parent().action, hitTestType)) {
                updateEnclosingBlocks(depth);
                return true;
            }
            break;
        }
        case Event::BeforeContents: {
            // In reverse order this comes last: the block's own background.
            auto entered = parent();
            if (entered.shouldHitTestContent && !entered.checkChildren && hitTestExcludedChildrenInBorder(block, m_request, m_result, m_location, adjustedLocation, entered.action)) {
                updateEnclosingBlocks(depth - 1);
                return true;
            }
            if (entered.shouldHitTestContent && entered.checkChildren && !is<RenderBlockFlow>(block) && hitTestLegendAfterItems(block, entered.action, adjustedLocation)) {
                updateEnclosingBlocks(depth - 1);
                return true;
            }
            if (!block->hitTestBorderRadius(m_location, accumulatedOffset))
                break;
            if (isBackgroundAction(entered.action)) {
                LayoutRect boundsRect(adjustedLocation, block->borderBoxSize());
                if (block->visibleToHitTesting(m_request) && m_location.intersects(boundsRect)) {
                    block->updateHitTestResult(m_result, block->physicalToBlockStartRelative(m_location.point() - localOffset));
                    if (m_result.addNodeToListBasedTestResult(protect(block->nodeForHitTest()).get(), m_request, m_location, boundsRect) == HitTestProgress::Stop) {
                        updateEnclosingBlocks(depth - 1);
                        return true;
                    }
                }
            }
            break;
        }
        case Event::LeaveBlock: {
            auto action = parent().action;
            popParent();
            if (hitTestOutsideListMarker(block, action, adjustedLocation)) {
                updateEnclosingBlocks(depth - 1);
                return true;
            }
            if (iterator.isStackingContainer()) {
                ASSERT(m_stackingContainerStack.last().depth == depth);
                if (++m_stackingContainerStack.last().actionIndex < stackingContainerActions.size()) {
                    iterator.reenterBlock();
                    break;
                }
                m_stackingContainerStack.removeLast();
            }
            break;
        }
        }
    }
    return false;
}

} // namespace Paint
} // namespace WebCore
