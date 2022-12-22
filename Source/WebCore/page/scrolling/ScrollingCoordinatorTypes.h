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

#include "FloatPoint.h"
#include "KeyboardScroll.h"
#include "ScrollTypes.h"
#include <wtf/EnumTraits.h>
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

enum class ScrollingLayerPositionAction {
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

    bool horizontalScrollbarHiddenByStyle { false };
    bool verticalScrollbarHiddenByStyle { false };

    bool useDarkAppearanceForScrollbars { false };

    bool operator==(const ScrollableAreaParameters& other) const
    {
        return horizontalScrollElasticity == other.horizontalScrollElasticity
            && verticalScrollElasticity == other.verticalScrollElasticity
            && horizontalScrollbarMode == other.horizontalScrollbarMode
            && verticalScrollbarMode == other.verticalScrollbarMode
            && horizontalOverscrollBehavior == other.horizontalOverscrollBehavior
            && verticalOverscrollBehavior == other.verticalOverscrollBehavior
            && allowsHorizontalScrolling == other.allowsHorizontalScrolling
            && allowsVerticalScrolling == other.allowsVerticalScrolling
            && horizontalScrollbarHiddenByStyle == other.horizontalScrollbarHiddenByStyle
            && verticalScrollbarHiddenByStyle == other.verticalScrollbarHiddenByStyle
            && useDarkAppearanceForScrollbars == other.useDarkAppearanceForScrollbars;
    }
};

enum class ViewportRectStability {
    Stable,
    Unstable,
    ChangingObscuredInsetsInteractively // This implies Unstable.
};

enum class ScrollRequestType : uint8_t {
    PositionUpdate,
    CancelAnimatedScroll
};

struct RequestedScrollData {
    ScrollRequestType requestType { ScrollRequestType::PositionUpdate };
    FloatPoint scrollPosition;
    ScrollType scrollType { ScrollType::User };
    ScrollClamping clamping { ScrollClamping::Clamped };
    ScrollIsAnimated animated { ScrollIsAnimated::No };

    bool operator==(const RequestedScrollData& other) const
    {
        return requestType == other.requestType
            && scrollPosition == other.scrollPosition
            && scrollType == other.scrollType
            && clamping == other.clamping
            && animated == other.animated;
    }
};

enum class KeyboardScrollAction : uint8_t {
    StartAnimation,
    StopWithAnimation,
    StopImmediately
};

struct RequestedKeyboardScrollData {
    KeyboardScrollAction action { KeyboardScrollAction::StartAnimation };
    std::optional<KeyboardScroll> keyboardScroll;

    bool operator==(const RequestedKeyboardScrollData& other) const
    {
        return action == other.action && keyboardScroll == other.keyboardScroll;
    }
};

enum class ScrollUpdateType : uint8_t {
    PositionUpdate,
    AnimatedScrollWillStart,
    AnimatedScrollDidEnd,
    WheelEventScrollWillStart,
    WheelEventScrollDidEnd,
};

struct ScrollUpdate {
    ScrollingNodeID nodeID { 0 };
    FloatPoint scrollPosition;
    std::optional<FloatPoint> layoutViewportOrigin;
    ScrollUpdateType updateType { ScrollUpdateType::PositionUpdate };
    ScrollingLayerPositionAction updateLayerPositionAction { ScrollingLayerPositionAction::Sync };
    
    bool canMerge(const ScrollUpdate& other) const
    {
        return nodeID == other.nodeID && updateLayerPositionAction == other.updateLayerPositionAction && updateType == other.updateType;
    }
    
    void merge(ScrollUpdate&& other)
    {
        scrollPosition = other.scrollPosition;
        layoutViewportOrigin = other.layoutViewportOrigin;
    }
};

enum class WheelEventProcessingSteps : uint8_t {
    ScrollingThread                             = 1 << 0,
    MainThreadForScrolling                      = 1 << 1,
    MainThreadForNonBlockingDOMEventDispatch    = 1 << 2,
    MainThreadForBlockingDOMEventDispatch       = 1 << 3,
};

struct WheelEventHandlingResult {
    OptionSet<WheelEventProcessingSteps> steps;
    bool wasHandled { false };
    bool needsMainThreadProcessing() const { return steps.containsAny({ WheelEventProcessingSteps::MainThreadForScrolling, WheelEventProcessingSteps::MainThreadForNonBlockingDOMEventDispatch, WheelEventProcessingSteps::MainThreadForBlockingDOMEventDispatch }); }

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
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollableAreaParameters);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ViewportRectStability);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, WheelEventProcessingSteps);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollUpdateType);

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::SynchronousScrollingReason> {
    using values = EnumValues<
        WebCore::SynchronousScrollingReason,
        WebCore::SynchronousScrollingReason::ForcedOnMainThread,
        WebCore::SynchronousScrollingReason::HasViewportConstrainedObjectsWithoutSupportingFixedLayers,
        WebCore::SynchronousScrollingReason::HasNonLayerViewportConstrainedObjects,
        WebCore::SynchronousScrollingReason::IsImageDocument,
        WebCore::SynchronousScrollingReason::HasSlowRepaintObjects,
        WebCore::SynchronousScrollingReason::DescendantScrollersHaveSynchronousScrolling
    >;
};

template<> struct EnumTraits<WebCore::ScrollRequestType> {
    using values = EnumValues<
        WebCore::ScrollRequestType,
        WebCore::ScrollRequestType::PositionUpdate,
        WebCore::ScrollRequestType::CancelAnimatedScroll
    >;
};

template<> struct EnumTraits<WebCore::ScrollingNodeType> {
    using values = EnumValues<
        WebCore::ScrollingNodeType,
        WebCore::ScrollingNodeType::MainFrame,
        WebCore::ScrollingNodeType::Subframe,
        WebCore::ScrollingNodeType::FrameHosting,
        WebCore::ScrollingNodeType::Overflow,
        WebCore::ScrollingNodeType::OverflowProxy,
        WebCore::ScrollingNodeType::Fixed,
        WebCore::ScrollingNodeType::Sticky,
        WebCore::ScrollingNodeType::Positioned
    >;
};

template<> struct EnumTraits<WebCore::KeyboardScrollAction> {
    using values = EnumValues<
        WebCore::KeyboardScrollAction,
        WebCore::KeyboardScrollAction::StartAnimation,
        WebCore::KeyboardScrollAction::StopWithAnimation,
        WebCore::KeyboardScrollAction::StopImmediately
    >;
};

template<> struct EnumTraits<WebCore::WheelEventProcessingSteps> {
    using values = EnumValues<
        WebCore::WheelEventProcessingSteps,
        WebCore::WheelEventProcessingSteps::ScrollingThread,
        WebCore::WheelEventProcessingSteps::MainThreadForScrolling,
        WebCore::WheelEventProcessingSteps::MainThreadForNonBlockingDOMEventDispatch,
        WebCore::WheelEventProcessingSteps::MainThreadForBlockingDOMEventDispatch
    >;
};

} // namespace WTF
