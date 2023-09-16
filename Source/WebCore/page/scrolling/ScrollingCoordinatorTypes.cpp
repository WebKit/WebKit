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

FloatPoint RequestedScrollData::destinationPosition(const FloatPoint& currentScrollPosition) const
{
    return requestType == ScrollRequestType::DeltaUpdate ? (currentScrollPosition + std::get<FloatSize>(scrollPositionOrDelta)) : std::get<FloatPoint>(scrollPositionOrDelta);
}

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
    if (scrollableAreaParameters.horizontalNativeScrollbarVisibility == NativeScrollbarVisibility::HiddenByStyle)
        ts.dumpProperty("horizontal scrollbar hidden by style", scrollableAreaParameters.horizontalNativeScrollbarVisibility);
    if (scrollableAreaParameters.verticalNativeScrollbarVisibility == NativeScrollbarVisibility::HiddenByStyle)
        ts.dumpProperty("vertical scrollbar hidden by style", scrollableAreaParameters.verticalNativeScrollbarVisibility);

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

TextStream& operator<<(TextStream& ts, WheelEventHandlingResult result)
{
    ts << "steps " << result.steps << " was handled " << result.wasHandled;
    return ts;
}

TextStream& operator<<(TextStream& ts, WheelEventProcessingSteps steps)
{
    switch (steps) {
    case WheelEventProcessingSteps::AsyncScrolling: ts << "async scrolling"; break;
    case WheelEventProcessingSteps::SynchronousScrolling: ts << "synchronous scrolling"; break;
    case WheelEventProcessingSteps::NonBlockingDOMEventDispatch: ts << "non-blocking DOM event dispatch"; break;
    case WheelEventProcessingSteps::BlockingDOMEventDispatch: ts << "blocking DOM event dispatch"; break;
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

TextStream& operator<<(WTF::TextStream& ts, ScrollRequestType type)
{
    switch (type) {
    case ScrollRequestType::CancelAnimatedScroll: ts << "cancel animated scroll"; break;
    case ScrollRequestType::PositionUpdate: ts << "position update"; break;
    case ScrollRequestType::DeltaUpdate: ts << "delta update"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, RequestedScrollData requestedScrollData)
{
    ts.dumpProperty("requested-type", requestedScrollData.requestType);

    if (requestedScrollData.requestType != ScrollRequestType::CancelAnimatedScroll) {
        if (requestedScrollData.requestType == ScrollRequestType::DeltaUpdate)
            ts.dumpProperty("requested-scroll-delta", std::get<FloatSize>(requestedScrollData.scrollPositionOrDelta));
        else
            ts.dumpProperty("requested-scroll-position", std::get<FloatPoint>(requestedScrollData.scrollPositionOrDelta));
        ts.dumpProperty("requested-scroll-position-is-programatic", requestedScrollData.scrollType);
        ts.dumpProperty("requested-scroll-position-clamping", requestedScrollData.clamping);
        ts.dumpProperty("requested-scroll-position-animated", requestedScrollData.animated == ScrollIsAnimated::Yes);
        if (requestedScrollData.requestedDataBeforeAnimatedScroll) {
            auto oldType = std::get<0>(*requestedScrollData.requestedDataBeforeAnimatedScroll);
            ts.dumpProperty("requested-scroll-position-old-data-type", oldType);

            if (oldType == ScrollRequestType::DeltaUpdate)
                ts.dumpProperty("requested-scroll-position-old-delta", std::get<FloatSize>(std::get<1>(*requestedScrollData.requestedDataBeforeAnimatedScroll)));
            else
                ts.dumpProperty("requested-scroll-position-old-position", std::get<FloatPoint>(std::get<1>(*requestedScrollData.requestedDataBeforeAnimatedScroll)));
            ts.dumpProperty("requested-scroll-position-old-data-is-programatic", std::get<2>(*requestedScrollData.requestedDataBeforeAnimatedScroll));
            ts.dumpProperty("requested-scroll-position-old-data-animated", std::get<3>(*requestedScrollData.requestedDataBeforeAnimatedScroll));
        }
    }
    return ts;
}

} // namespace WebCore
