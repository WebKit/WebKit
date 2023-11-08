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
    ASSERT(!other.requestedDataBeforeAnimatedScroll);

    ALWAYS_LOG_WITH_STREAM(stream << "RequestedScrollData::merge current: " <<*this <<" new" << other);

    RequestedScrollData mergedScrollData = other;
    mergedScrollData.requestedDataBeforeAnimatedScroll = requestedDataBeforeAnimatedScroll;

    auto generateRequestedDataBeforeAnimatedScrollIfNeeded = [this, other, &mergedScrollData] () {
        if (animated == ScrollIsAnimated::No && other.animated == ScrollIsAnimated::Yes)
            mergedScrollData.requestedDataBeforeAnimatedScroll = { requestType, scrollPositionOrDelta, scrollType, clamping };
    };

    switch (requestType) {
    case ScrollRequestType::CancelAnimatedScroll: {
        switch (other.requestType) {
        case ScrollRequestType::DeltaUpdate: {
            if (other.animated == ScrollIsAnimated::No) {
                if (auto previousData = std::exchange(requestedDataBeforeAnimatedScroll, std::nullopt)) {
                    auto& [type, positionOrDelta, scrollType, clamping] = *previousData;
                    if (type == ScrollRequestType::PositionUpdate)
                        mergedScrollData.scrollPositionOrDelta = std::get<FloatPoint>(positionOrDelta) + std::get<FloatSize>(other.scrollPositionOrDelta);
                    else
                        mergedScrollData.scrollPositionOrDelta = std::get<FloatSize>(positionOrDelta) + std::get<FloatSize>(other.scrollPositionOrDelta);
                    mergedScrollData.requestType = type;
                }
            }
            break;
        }
        case ScrollRequestType::PositionUpdate:
        case ScrollRequestType::CancelAnimatedScroll:
            break;
        }
        break;
    } // ScrollRequestType::CancelAnimatedScroll
    case ScrollRequestType::PositionUpdate: {
        switch (other.requestType) {
        case ScrollRequestType::CancelAnimatedScroll: {
            if (animated == ScrollIsAnimated::No)
                mergedScrollData.requestedDataBeforeAnimatedScroll = { requestType, scrollPositionOrDelta, scrollType, clamping };
            break;
        }
        case ScrollRequestType::PositionUpdate:
            generateRequestedDataBeforeAnimatedScrollIfNeeded();
            break;
        case ScrollRequestType::DeltaUpdate: {
            generateRequestedDataBeforeAnimatedScrollIfNeeded();
            if (other.animated == ScrollIsAnimated::No) {
                mergedScrollData.scrollPositionOrDelta = std::get<FloatPoint>(scrollPositionOrDelta) + std::get<FloatSize>(other.scrollPositionOrDelta);
                mergedScrollData.requestType = ScrollRequestType::PositionUpdate;
            }
            break;
        }
        }
        break;
    } // ScrollRequestType::PositionUpdate
    case ScrollRequestType::DeltaUpdate: {
        switch (other.requestType) {
        case ScrollRequestType::CancelAnimatedScroll: {
            if (animated == ScrollIsAnimated::No)
                mergedScrollData.requestedDataBeforeAnimatedScroll = { requestType, scrollPositionOrDelta, scrollType, clamping };
            break;
        }
        case ScrollRequestType::PositionUpdate:
            generateRequestedDataBeforeAnimatedScrollIfNeeded();
            break;
        case ScrollRequestType::DeltaUpdate: {
            generateRequestedDataBeforeAnimatedScrollIfNeeded();
            if (other.animated == ScrollIsAnimated::No)
                std::get<FloatSize>(mergedScrollData.scrollPositionOrDelta) += std::get<FloatSize>(other.scrollPositionOrDelta);
            break;
        }
        }
        break;
    } // ScrollRequestType::DeltaUpdate
    }

    if (other.animated == ScrollIsAnimated::No && other.requestType != ScrollRequestType::CancelAnimatedScroll)
        mergedScrollData.requestedDataBeforeAnimatedScroll = { };
    *this = mergedScrollData;
    ALWAYS_LOG_WITH_STREAM(stream << "RequestedScrollData::merged: " <<*this);
}

FloatPoint RequestedScrollData::destinationPosition(FloatPoint currentScrollPosition) const
{
    return computeDestinationPosition(currentScrollPosition, requestType, scrollPositionOrDelta);
}

FloatPoint RequestedScrollData::computeDestinationPosition(FloatPoint currentScrollPosition, ScrollRequestType requestType, const std::variant<FloatPoint, FloatSize>& scrollPositionOrDelta)
{
    if (requestType == ScrollRequestType::DeltaUpdate)
        return currentScrollPosition + std::get<FloatSize>(scrollPositionOrDelta);

    return std::get<FloatPoint>(scrollPositionOrDelta);
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

TextStream& operator<<(TextStream& ts, const ScrollableAreaParameters& scrollableAreaParameters)
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

TextStream& operator<<(TextStream& ts, const RequestedScrollData& requestedScrollData)
{
    ts.dumpProperty("type", requestedScrollData.requestType);

    if (requestedScrollData.requestedDataBeforeAnimatedScroll) {
        auto oldType = std::get<0>(*requestedScrollData.requestedDataBeforeAnimatedScroll);
        ts.dumpProperty("before-animated scroll type", oldType);

        if (oldType == ScrollRequestType::DeltaUpdate)
            ts.dumpProperty("before-animated scroll delta", std::get<FloatSize>(std::get<1>(*requestedScrollData.requestedDataBeforeAnimatedScroll)));
        else
            ts.dumpProperty("before-animated scroll position", std::get<FloatPoint>(std::get<1>(*requestedScrollData.requestedDataBeforeAnimatedScroll)));

        ts.dumpProperty("before-animated scroll programatic", std::get<2>(*requestedScrollData.requestedDataBeforeAnimatedScroll));
        ts.dumpProperty("before-animated scroll animated", std::get<3>(*requestedScrollData.requestedDataBeforeAnimatedScroll));
    }

    if (requestedScrollData.requestType == ScrollRequestType::CancelAnimatedScroll)
        return ts;

    if (requestedScrollData.requestType == ScrollRequestType::DeltaUpdate)
        ts.dumpProperty("scroll delta", std::get<FloatSize>(requestedScrollData.scrollPositionOrDelta));
    else
        ts.dumpProperty("position", std::get<FloatPoint>(requestedScrollData.scrollPositionOrDelta));

    if (requestedScrollData.scrollType == ScrollType::Programmatic)
        ts.dumpProperty("is programmatic", requestedScrollData.scrollType);

    if (requestedScrollData.clamping == ScrollClamping::Clamped)
        ts.dumpProperty("clamping", requestedScrollData.clamping);

    if (requestedScrollData.animated == ScrollIsAnimated::Yes)
        ts.dumpProperty("animated", requestedScrollData.animated == ScrollIsAnimated::Yes);

    return ts;
}

} // namespace WebCore
