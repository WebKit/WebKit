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
#include "PaintOrderIterator.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayer.h"
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

static bool isBackgroundAction(HitTestAction action)
{
    return action == HitTestAction::BlockBackground || action == HitTestAction::ChildBlockBackground;
}

bool BlockHitTester::hitTest()
{
    using Event = PaintOrderIterator::Event;

    auto iterator = PaintOrderIterator { m_block, m_accumulatedOffset, PaintOrderIterator::Direction::Reverse };

    // On a hit, each block whose contents contain it updates the result, innermost first,
    // like the recursive code does while unwinding.
    auto updateEnclosingBlocks = [&](size_t count) {
        for (size_t index = count; index--;) {
            CheckedRef block = iterator.blockAt(index);
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

            if (!block->hitTestVisualOverflow(m_location, accumulatedOffset)) {
                iterator.skipBlock();
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
                iterator.skipBlock();
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
            if (block->hitTestInlineChildren(m_request, m_result, m_location, iterator.scrolledOffset(), parent().action)) {
                updateEnclosingBlocks(depth);
                return true;
            }
            break;
        case Event::Children:
            // Grid and flex containers hit test their items themselves.
            if (block->hitTestChildren(m_request, m_result, m_location, adjustedLocation, parent().action)) {
                updateEnclosingBlocks(depth - 1);
                return true;
            }
            break;
        case Event::Child: {
            CheckedRef child = iterator.child();
            auto action = parent().action;
            if (child->isFloating()) {
                // Like the painting, in reverse.
                if (action == HitTestAction::Float && is<RenderBlockFlow>(block) && block->hitTestFloat(child, m_request, m_result, m_location, iterator.childPaintOffset())) {
                    updateEnclosingBlocks(depth);
                    return true;
                }
                break;
            }
            if (child->nodeAtPoint(m_request, m_result, m_location, iterator.childPaintOffset(), actionForChildren(action))) {
                updateEnclosingBlocks(depth);
                return true;
            }
            break;
        }
        case Event::BeforeContents: {
            // In reverse order this comes last: the block's own background.
            auto entered = parent();
            if (entered.shouldHitTestContent && !entered.checkChildren && block->hitTestExcludedChildrenInBorder(m_request, m_result, m_location, adjustedLocation, entered.action)) {
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
        case Event::LeaveBlock:
            popParent();
            break;
        }
    }
    return false;
}

} // namespace Paint
} // namespace WebCore
