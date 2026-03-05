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

#pragma once

#include <WebCore/FloatPoint.h>
#include <WebCore/KeyboardScroll.h>
#include <WebCore/ScrollTypes.h>
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>

namespace WebCore {

enum class SynchronousScrollingReason : uint8_t {
    // Flags for frame scrolling.
    ForcedOnMainThread                                          = 1 << 0,
    HasViewportConstrainedObjectsWithoutSupportingFixedLayers   = 1 << 1,
    HasNonLayerViewportConstrainedObjects                       = 1 << 2,
    IsImageDocument                                             = 1 << 3,
    // Flags for frame and overflow scrolling.
    HasSlowRepaintObjects                                       = 1 << 4,
    DescendantScrollersHaveSynchronousScrolling                 = 1 << 5,
};

enum class ScrollingNodeType : uint8_t {
    MainFrame,
    Subframe,
    FrameHosting,
    PluginScrolling,
    PluginHosting,
    Overflow,
    OverflowProxy,
    Fixed,
    Sticky,
    Positioned
};

enum class ScrollingStateTreeAsTextBehavior : uint8_t {
    IncludeLayerIDs         = 1 << 0,
    IncludeNodeIDs          = 1 << 1,
    IncludeLayerPositions   = 1 << 2,
};

constexpr auto debugScrollingStateTreeAsTextBehaviors = OptionSet<ScrollingStateTreeAsTextBehavior> {
    ScrollingStateTreeAsTextBehavior::IncludeLayerIDs, ScrollingStateTreeAsTextBehavior::IncludeNodeIDs, ScrollingStateTreeAsTextBehavior::IncludeLayerPositions
};

enum class ScrollingLayerPositionAction : uint8_t {
    Set,
    SetApproximate,
    Sync
};

struct ScrollableAreaParameters {
    ScrollElasticity horizontalScrollElasticity { ScrollElasticity::None };
    ScrollElasticity verticalScrollElasticity { ScrollElasticity::None };

    ScrollbarMode horizontalScrollbarMode { ScrollbarMode::Auto };
    ScrollbarMode verticalScrollbarMode { ScrollbarMode::Auto };
    
    OverscrollBehavior horizontalOverscrollBehavior { OverscrollBehavior::Auto };
    OverscrollBehavior verticalOverscrollBehavior { OverscrollBehavior::Auto };

    bool allowsHorizontalScrolling { false };
    bool allowsVerticalScrolling { false };

    NativeScrollbarVisibility horizontalNativeScrollbarVisibility { NativeScrollbarVisibility::Visible };
    NativeScrollbarVisibility verticalNativeScrollbarVisibility { NativeScrollbarVisibility::Visible };

    ScrollbarWidth scrollbarWidthStyle { ScrollbarWidth::Auto };
    std::optional<ScrollbarColor> scrollbarColorStyle;

    friend bool operator==(const ScrollableAreaParameters&, const ScrollableAreaParameters&) = default;
};

enum class ViewportRectStability {
    Stable,
    Unstable,
    ChangingObscuredInsetsInteractively // This implies Unstable.
};

enum class ScrollRequestType : uint8_t {
    PositionUpdate,
    AnimatedPositionUpdate,
    DeltaUpdate,
    AnimatedDeltaUpdate,
    ImplicitDeltaUpdate, // Allows animated scrolls to continue
    CancelAnimatedScroll
};

constexpr inline bool isAnimatedUpdate(ScrollRequestType type)
{
    return type == ScrollRequestType::AnimatedPositionUpdate || type == ScrollRequestType::AnimatedDeltaUpdate;
}

struct ScrollRequestIdentifierType;
using ScrollRequestIdentifier = ObjectIdentifier<ScrollRequestIdentifierType>;

struct RequestedScrollData {
    ScrollRequestType requestType { ScrollRequestType::PositionUpdate };
    Variant<FloatPoint, FloatSize> scrollPositionOrDelta { };
    Markable<ScrollRequestIdentifier> identifier { };
    ScrollType scrollType { ScrollType::User };
    ScrollClamping clamping { ScrollClamping::Clamped };
    ScrollbarRevealBehavior scrollbarRevealBehavior { ScrollbarRevealBehavior::Default };

    WEBCORE_EXPORT FloatPoint destinationPosition(FloatPoint currentScrollPosition) const;
    WEBCORE_EXPORT static FloatPoint computeDestinationPosition(FloatPoint currentScrollPosition, ScrollRequestType, const Variant<FloatPoint, FloatSize>& scrollPositionOrDelta);

    bool comparePositionOrDelta(const RequestedScrollData& other) const
    {
        if (requestType == ScrollRequestType::PositionUpdate)
            return std::get<FloatPoint>(scrollPositionOrDelta) == std::get<FloatPoint>(other.scrollPositionOrDelta);
        if (requestType == ScrollRequestType::DeltaUpdate || requestType == ScrollRequestType::ImplicitDeltaUpdate)
            return std::get<FloatSize>(scrollPositionOrDelta) == std::get<FloatSize>(other.scrollPositionOrDelta);
        return true;
    }

    bool operator==(const RequestedScrollData& other) const
    {
        // The identifier is not checked.
        return requestType == other.requestType
            && comparePositionOrDelta(other)
            && scrollType == other.scrollType
            && clamping == other.clamping
            && scrollbarRevealBehavior == other.scrollbarRevealBehavior;
    }
};

using ScrollRequestData = Vector<RequestedScrollData, 2>;

enum class KeyboardScrollAction : uint8_t {
    StartAnimation,
    StopWithAnimation,
    StopImmediately
};

struct RequestedKeyboardScrollData {
    KeyboardScrollAction action { KeyboardScrollAction::StartAnimation };
    std::optional<KeyboardScroll> keyboardScroll;

    friend bool operator==(const RequestedKeyboardScrollData&, const RequestedKeyboardScrollData&) = default;
};

enum class ScrollUpdateType : uint8_t {
    PositionUpdate,
    AnimatedScrollWillStart,
    AnimatedScrollDidEnd,
    WheelEventScrollWillStart,
    WheelEventScrollDidEnd,
};

enum class ShouldFireScrollEnd : bool { No, Yes };

struct ScrollUpdateData {
    ScrollUpdateType updateType { ScrollUpdateType::PositionUpdate };
    ScrollingLayerPositionAction updateLayerPositionAction { ScrollingLayerPositionAction::Sync };
    std::optional<FloatPoint> layoutViewportOrigin { };
};

struct ScrollRequestResponseData {
    ScrollRequestType requestType { ScrollRequestType::PositionUpdate };
    Markable<ScrollRequestIdentifier> responseIdentifier { };
};

struct ScrollUpdate {
    ScrollingNodeID nodeID;
    FloatPoint scrollPosition;
    ShouldFireScrollEnd shouldFireScrollEnd { ShouldFireScrollEnd::No };
    Variant<ScrollUpdateData, ScrollRequestResponseData> data;

    bool canMerge(const ScrollUpdate&) const;
    void merge(ScrollUpdate&&);
};

enum class WheelEventProcessingSteps : uint8_t {
    AsyncScrolling                      = 1 << 0,
    SynchronousScrolling                = 1 << 1, // Synchronous with painting and script.
    NonBlockingDOMEventDispatch         = 1 << 2,
    BlockingDOMEventDispatch            = 1 << 3,
};

struct WheelEventHandlingResult {
    OptionSet<WheelEventProcessingSteps> steps;
    bool wasHandled { false };
    bool needsMainThreadProcessing() const { return steps.containsAny({ WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::NonBlockingDOMEventDispatch, WheelEventProcessingSteps::BlockingDOMEventDispatch }); }

    static WheelEventHandlingResult handled(OptionSet<WheelEventProcessingSteps> steps = { })
    {
        return { steps, true };
    }
    static WheelEventHandlingResult unhandled(OptionSet<WheelEventProcessingSteps> steps = { })
    {
        return { steps, false };
    }
    static WheelEventHandlingResult result(bool handled)
    {
        return { { }, handled };
    }
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, SynchronousScrollingReason);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollingNodeType);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollingLayerPositionAction);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const ScrollableAreaParameters&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ViewportRectStability);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, WheelEventHandlingResult);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, WheelEventProcessingSteps);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollRequestType);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollUpdateType);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const ScrollUpdate&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const RequestedScrollData&);

} // namespace WebCore
