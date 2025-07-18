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

void RequestedScrollData::merge(RequestedScrollData&& other)
{
    if (other.requestType == ScrollRequestType::CancelAnimatedScroll && animated == ScrollIsAnimated::No) {
        // Carry over the previously requested scroll position so that we can set `requestedDataBeforeAnimatedScroll`
        // below in the case where the cancelled animated scroll is immediately followed by another animated scroll.
        other.scrollPositionOrDelta = scrollPositionOrDelta;
    } else if ((other.requestType == ScrollRequestType::PositionUpdate || other.requestType == ScrollRequestType::DeltaUpdate) && other.animated == ScrollIsAnimated::Yes) {
        switch (animated) {
        case ScrollIsAnimated::No:
            other.requestedDataBeforeAnimatedScroll = { requestType, scrollPositionOrDelta, scrollType, clamping };
            break;
        case ScrollIsAnimated::Yes:
            other.requestedDataBeforeAnimatedScroll = requestedDataBeforeAnimatedScroll;
            break;
        }
    } else if (other.requestType == ScrollRequestType::DeltaUpdate && animated == ScrollIsAnimated::No) {
        switch (requestType) {
        case ScrollRequestType::PositionUpdate: {
            other.requestType = ScrollRequestType::PositionUpdate;
            other.scrollPositionOrDelta = std::get<FloatPoint>(scrollPositionOrDelta) + std::get<FloatSize>(other.scrollPositionOrDelta);
            break;
        }
        case ScrollRequestType::DeltaUpdate:
            std::get<FloatSize>(other.scrollPositionOrDelta) += std::get<FloatSize>(scrollPositionOrDelta);
            break;
        default:
            break;
        }
    }
    *this = WTFMove(other);
}

FloatPoint RequestedScrollData::destinationPosition(FloatPoint currentScrollPosition) const
{
    return computeDestinationPosition(currentScrollPosition, requestType, scrollPositionOrDelta);
}

FloatPoint RequestedScrollData::computeDestinationPosition(FloatPoint currentScrollPosition, ScrollRequestType requestType, const Variant<FloatPoint, FloatSize>& scrollPositionOrDelta)
{
    if (requestType == ScrollRequestType::DeltaUpdate)
        return currentScrollPosition + std::get<FloatSize>(scrollPositionOrDelta);

    return std::get<FloatPoint>(scrollPositionOrDelta);
}

TextStream& operator<<(TextStream& ts, SynchronousScrollingReason reason)
{
    switch (reason) {
    case SynchronousScrollingReason::ForcedOnMainThread: ts << "forced on main thread"_s; break;
    case SynchronousScrollingReason::HasViewportConstrainedObjectsWithoutSupportingFixedLayers: ts << "has viewport constrained objects without supporting fixed layers"_s; break;
    case SynchronousScrollingReason::HasNonLayerViewportConstrainedObjects: ts << "has non-layer viewport-constrained objects"_s; break;
    case SynchronousScrollingReason::IsImageDocument: ts << "is image document"_s; break;
    case SynchronousScrollingReason::HasSlowRepaintObjects: ts << "has slow repaint objects"_s; break;
    case SynchronousScrollingReason::DescendantScrollersHaveSynchronousScrolling: ts << "descendant scrollers have synchronous scrolling"_s; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollingNodeType nodeType)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
        ts << "main-frame-scrolling"_s;
        break;
    case ScrollingNodeType::Subframe:
        ts << "subframe-scrolling"_s;
        break;
    case ScrollingNodeType::FrameHosting:
        ts << "frame-hosting"_s;
        break;
    case ScrollingNodeType::PluginScrolling:
        ts << "plugin-scrolling"_s;
        break;
    case ScrollingNodeType::PluginHosting:
        ts << "plugin-hosting"_s;
        break;
    case ScrollingNodeType::Overflow:
        ts << "overflow-scrolling"_s;
        break;
    case ScrollingNodeType::OverflowProxy:
        ts << "overflow-scroll-proxy"_s;
        break;
    case ScrollingNodeType::Fixed:
        ts << "fixed"_s;
        break;
    case ScrollingNodeType::Sticky:
        ts << "sticky"_s;
        break;
    case ScrollingNodeType::Positioned:
        ts << "positioned"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollingLayerPositionAction action)
{
    switch (action) {
    case ScrollingLayerPositionAction::Set:
        ts << "set"_s;
        break;
    case ScrollingLayerPositionAction::SetApproximate:
        ts << "set approximate"_s;
        break;
    case ScrollingLayerPositionAction::Sync:
        ts << "sync"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const ScrollableAreaParameters& scrollableAreaParameters)
{
    ts.dumpProperty("horizontal scroll elasticity"_s, scrollableAreaParameters.horizontalScrollElasticity);
    ts.dumpProperty("vertical scroll elasticity"_s, scrollableAreaParameters.verticalScrollElasticity);
    ts.dumpProperty("horizontal scrollbar mode"_s, scrollableAreaParameters.horizontalScrollbarMode);
    ts.dumpProperty("vertical scrollbar mode"_s, scrollableAreaParameters.verticalScrollbarMode);

    if (scrollableAreaParameters.allowsHorizontalScrolling)
        ts.dumpProperty("allows horizontal scrolling"_s, scrollableAreaParameters.allowsHorizontalScrolling);
    if (scrollableAreaParameters.allowsVerticalScrolling)
        ts.dumpProperty("allows vertical scrolling"_s, scrollableAreaParameters.allowsVerticalScrolling);
    if (scrollableAreaParameters.horizontalNativeScrollbarVisibility == NativeScrollbarVisibility::HiddenByStyle)
        ts.dumpProperty("horizontal scrollbar hidden by style"_s, scrollableAreaParameters.horizontalNativeScrollbarVisibility);
    if (scrollableAreaParameters.verticalNativeScrollbarVisibility == NativeScrollbarVisibility::HiddenByStyle)
        ts.dumpProperty("vertical scrollbar hidden by style"_s, scrollableAreaParameters.verticalNativeScrollbarVisibility);

    return ts;
}

TextStream& operator<<(TextStream& ts, ViewportRectStability stability)
{
    switch (stability) {
    case ViewportRectStability::Stable:
        ts << "stable"_s;
        break;
    case ViewportRectStability::Unstable:
        ts << "unstable"_s;
        break;
    case ViewportRectStability::ChangingObscuredInsetsInteractively:
        ts << "changing obscured insets interactively"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, WheelEventHandlingResult result)
{
    ts << "steps "_s << result.steps << " was handled "_s << result.wasHandled;
    return ts;
}

TextStream& operator<<(TextStream& ts, WheelEventProcessingSteps steps)
{
    switch (steps) {
    case WheelEventProcessingSteps::AsyncScrolling: ts << "async scrolling"_s; break;
    case WheelEventProcessingSteps::SynchronousScrolling: ts << "synchronous scrolling"_s; break;
    case WheelEventProcessingSteps::NonBlockingDOMEventDispatch: ts << "non-blocking DOM event dispatch"_s; break;
    case WheelEventProcessingSteps::BlockingDOMEventDispatch: ts << "blocking DOM event dispatch"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollUpdateType type)
{
    switch (type) {
    case ScrollUpdateType::PositionUpdate: ts << "position update"_s; break;
    case ScrollUpdateType::AnimatedScrollWillStart: ts << "animated scroll will start"_s; break;
    case ScrollUpdateType::AnimatedScrollDidEnd: ts << "animated scroll did end"_s; break;
    case ScrollUpdateType::WheelEventScrollWillStart: ts << "wheel event scroll will start"_s; break;
    case ScrollUpdateType::WheelEventScrollDidEnd: ts << "wheel event scroll did end"_s; break;
    }
    return ts;
}

TextStream& operator<<(WTF::TextStream& ts, ScrollRequestType type)
{
    switch (type) {
    case ScrollRequestType::CancelAnimatedScroll: ts << "cancel animated scroll"_s; break;
    case ScrollRequestType::PositionUpdate: ts << "position update"_s; break;
    case ScrollRequestType::DeltaUpdate: ts << "delta update"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const RequestedScrollData& requestedScrollData)
{
    ts.dumpProperty("type"_s, requestedScrollData.requestType);

    if (requestedScrollData.requestType == ScrollRequestType::CancelAnimatedScroll)
        return ts;

    if (requestedScrollData.requestType == ScrollRequestType::DeltaUpdate)
        ts.dumpProperty("scroll delta"_s, std::get<FloatSize>(requestedScrollData.scrollPositionOrDelta));
    else
        ts.dumpProperty("position"_s, std::get<FloatPoint>(requestedScrollData.scrollPositionOrDelta));

    if (requestedScrollData.scrollType == ScrollType::Programmatic)
        ts.dumpProperty("is programmatic"_s, requestedScrollData.scrollType);

    if (requestedScrollData.clamping == ScrollClamping::Clamped)
        ts.dumpProperty("clamping"_s, requestedScrollData.clamping);

    if (requestedScrollData.animated == ScrollIsAnimated::Yes)
        ts.dumpProperty("animated"_s, requestedScrollData.animated == ScrollIsAnimated::Yes);

    if (requestedScrollData.requestedDataBeforeAnimatedScroll) {
        auto oldType = std::get<0>(*requestedScrollData.requestedDataBeforeAnimatedScroll);
        ts.dumpProperty("before-animated scroll type"_s, oldType);

        if (oldType == ScrollRequestType::DeltaUpdate)
            ts.dumpProperty("before-animated scroll delta"_s, std::get<FloatSize>(std::get<1>(*requestedScrollData.requestedDataBeforeAnimatedScroll)));
        else
            ts.dumpProperty("before-animated scroll position"_s, std::get<FloatPoint>(std::get<1>(*requestedScrollData.requestedDataBeforeAnimatedScroll)));

        ts.dumpProperty("before-animated scroll programatic"_s, std::get<2>(*requestedScrollData.requestedDataBeforeAnimatedScroll));
        ts.dumpProperty("before-animated scroll animated"_s, std::get<3>(*requestedScrollData.requestedDataBeforeAnimatedScroll));
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, const ScrollUpdate& update)
{
    ts << "updateType: " << update.updateType << " nodeID: " << update.nodeID << " scrollPosition: " << update.scrollPosition << " layoutViewportOrigin: " << update.layoutViewportOrigin << " updateLayerPositionAction: " << update.updateLayerPositionAction;
    return ts;
}

} // namespace WebCore
