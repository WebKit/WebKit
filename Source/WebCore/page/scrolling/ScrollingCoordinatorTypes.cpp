/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ScrollingCoordinatorTypes.h"

namespace WebCore {

TextStream& operator<<(TextStream& ts, SynchronousScrollingReason reason)
{
    switch (reason) {
    case SynchronousScrollingReason::ForcedOnMainThread: ts << "forced on main thread"; break;
    case SynchronousScrollingReason::HasViewportConstrainedObjectsWithoutSupportingFixedLayers: ts << "has viewport constrained objects without supporting fixed layers"; break;
    case SynchronousScrollingReason::HasNonLayerViewportConstrainedObjects: ts << "has non-layer viewport-constrained objects"; break;
    case SynchronousScrollingReason::IsImageDocument: ts << "is image document"; break;
    case SynchronousScrollingReason::HasSlowRepaintObjects: ts << "has slow repaint objects"; break;
    case SynchronousScrollingReason::DescendantScrollersHaveSynchronousScrolling: ts << "descendant scrollers have synchronous scrolling"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollingNodeType nodeType)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
        ts << "main-frame-scrolling";
        break;
    case ScrollingNodeType::Subframe:
        ts << "subframe-scrolling";
        break;
    case ScrollingNodeType::FrameHosting:
        ts << "frame-hosting";
        break;
    case ScrollingNodeType::Overflow:
        ts << "overflow-scrolling";
        break;
    case ScrollingNodeType::OverflowProxy:
        ts << "overflow-scroll-proxy";
        break;
    case ScrollingNodeType::Fixed:
        ts << "fixed";
        break;
    case ScrollingNodeType::Sticky:
        ts << "sticky";
        break;
    case ScrollingNodeType::Positioned:
        ts << "positioned";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollingLayerPositionAction action)
{
    switch (action) {
    case ScrollingLayerPositionAction::Set:
        ts << "set";
        break;
    case ScrollingLayerPositionAction::SetApproximate:
        ts << "set approximate";
        break;
    case ScrollingLayerPositionAction::Sync:
        ts << "sync";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollableAreaParameters scrollableAreaParameters)
{
    ts.dumpProperty("horizontal scroll elasticity", scrollableAreaParameters.horizontalScrollElasticity);
    ts.dumpProperty("vertical scroll elasticity", scrollableAreaParameters.verticalScrollElasticity);
    ts.dumpProperty("horizontal scrollbar mode", scrollableAreaParameters.horizontalScrollbarMode);
    ts.dumpProperty("vertical scrollbar mode", scrollableAreaParameters.verticalScrollbarMode);

    if (scrollableAreaParameters.allowsHorizontalScrolling)
        ts.dumpProperty("allows horizontal scrolling", scrollableAreaParameters.allowsHorizontalScrolling);
    if (scrollableAreaParameters.allowsVerticalScrolling)
        ts.dumpProperty("allows vertical scrolling", scrollableAreaParameters.allowsVerticalScrolling);

    if (scrollableAreaParameters.horizontalScrollbarHiddenByStyle)
        ts.dumpProperty("horizontal scrollbar hidden by style", scrollableAreaParameters.horizontalScrollbarHiddenByStyle);
    if (scrollableAreaParameters.verticalScrollbarHiddenByStyle)
        ts.dumpProperty("vertical scrollbar hidden by style", scrollableAreaParameters.verticalScrollbarHiddenByStyle);

    return ts;
}

TextStream& operator<<(TextStream& ts, ViewportRectStability stability)
{
    switch (stability) {
    case ViewportRectStability::Stable:
        ts << "stable";
        break;
    case ViewportRectStability::Unstable:
        ts << "unstable";
        break;
    case ViewportRectStability::ChangingObscuredInsetsInteractively:
        ts << "changing obscured insets interactively";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, WheelEventProcessingSteps steps)
{
    switch (steps) {
    case WheelEventProcessingSteps::ScrollingThread: ts << "scrolling thread"; break;
    case WheelEventProcessingSteps::MainThreadForScrolling: ts << "main thread scrolling"; break;
    case WheelEventProcessingSteps::MainThreadForNonBlockingDOMEventDispatch: ts << "main thread non-blocking DOM event dispatch"; break;
    case WheelEventProcessingSteps::MainThreadForBlockingDOMEventDispatch: ts << "main thread blocking DOM event dispatch"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollUpdateType type)
{
    switch (type) {
    case ScrollUpdateType::PositionUpdate: ts << "position update"; break;
    case ScrollUpdateType::AnimatedScrollWillStart: ts << "animated scroll will start"; break;
    case ScrollUpdateType::AnimatedScrollDidEnd: ts << "animated scroll did end"; break;
    case ScrollUpdateType::WheelEventScrollWillStart: ts << "wheel event scroll will start"; break;
    case ScrollUpdateType::WheelEventScrollDidEnd: ts << "wheel event scroll did end"; break;
    }
    return ts;
}

} // namespace WebCore
