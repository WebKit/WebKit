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

#pragma once

#include "HitTestLocation.h"
#include "LayoutPoint.h"
#include "RenderObjectEnums.h"
#include <wtf/CheckedRef.h>
#include <wtf/Vector.h>

namespace WebCore {

class HitTestRequest;
class HitTestResult;
class RenderBlock;
class RenderBox;

namespace Paint {

// The static functions hit test one block. hitTest() walks the block's subtree in reverse paint order with
// PaintOrderIterator when the subtree can be iterated, using a BlockHitTester instance for the walk.
class BlockHitTester {
public:
    static bool hitTest(RenderBlock&, const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

private:
    BlockHitTester(RenderBlock&, const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

    bool hitTestSubtree();

    static bool hitTestBlock(RenderBlock&, const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& accumulatedOffset, HitTestAction);
    static bool hitTestChildren(RenderBlock&, const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation, HitTestAction);
    static bool hitTestContents(RenderBlock&, const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& scrolledOffset, HitTestAction);
    static bool hitTestInlineChildren(RenderBlock&, const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& scrolledOffset, HitTestAction);

    enum class HitTestBlockType : bool { Block, StackingContainer };
    static bool hitTestChild(RenderBlock&, RenderBox& child, const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& childPoint, HitTestAction, HitTestBlockType);
    static bool hitTestFloat(RenderBlock&, RenderBox& floatBox, const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& floatBoxLocation);
    static bool hitTestExcludedChildrenInBorder(RenderBlock&, const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation, HitTestAction);

    struct Parent {
        HitTestAction action;
        bool shouldHitTestContent;
        // False when the location is outside the block's clip, so nothing inside it can be hit.
        bool checkChildren;
    };

    bool hitTestLegendAfterItems(RenderBlock& container, HitTestAction, const LayoutPoint& adjustedLocation);
    bool hitTestOutsideListMarker(RenderBlock&, HitTestAction, const LayoutPoint& adjustedLocation);

    // A stacking container being hit tested, one walk per action.
    struct StackingContainer {
        size_t depth;
        uint8_t actionIndex { 0 };
    };

    void pushParent(Parent&& parent) { m_parentStack.append(WTF::move(parent)); }
    void popParent() { m_parentStack.removeLast(); }
    const Parent& parent() const LIFETIME_BOUND { return m_parentStack.last(); }

    const CheckedRef<RenderBlock> m_block;
    const HitTestRequest& m_request;
    HitTestResult& m_result;
    const HitTestLocation& m_location;
    const LayoutPoint m_accumulatedOffset;
    const HitTestAction m_action;
    Vector<Parent, 16> m_parentStack;
    Vector<StackingContainer, 4> m_stackingContainerStack;
};

} // namespace Paint
} // namespace WebCore
