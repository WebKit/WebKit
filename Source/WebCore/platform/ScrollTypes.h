/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "FloatPoint.h"
#include "FloatSize.h"
#include "RectEdges.h"
#include <wtf/EnumTraits.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class IntPoint;

// scrollPosition is in content coordinates (0,0 is at scrollOrigin), so may have negative components.
using ScrollPosition = IntPoint;
// scrollOffset() is the value used by scrollbars (min is 0,0), and should never have negative components.
using ScrollOffset = IntPoint;
    
enum class ScrollType : bool {
    User,
    Programmatic
};

enum class OverscrollBehavior : uint8_t {
    Auto,
    Contain,
    None
};

enum ScrollDirection : uint8_t {
    ScrollUp,
    ScrollDown,
    ScrollLeft,
    ScrollRight
};

enum ScrollLogicalDirection : uint8_t {
    ScrollBlockDirectionBackward,
    ScrollBlockDirectionForward,
    ScrollInlineDirectionBackward,
    ScrollInlineDirectionForward
};

enum class ScrollAnimationStatus : uint8_t {
    NotAnimating,
    Animating,
};

enum class ScrollIsAnimated : uint8_t {
    No,
    Yes
};

enum class OverflowAnchor : uint8_t {
    Auto,
    None
};

inline ScrollDirection logicalToPhysical(ScrollLogicalDirection direction, bool isVertical, bool isFlipped)
{
    switch (direction) {
    case ScrollBlockDirectionBackward: {
        if (isVertical) {
            if (!isFlipped)
                return ScrollUp;
            return ScrollDown;
        } else {
            if (!isFlipped)
                return ScrollLeft;
            return ScrollRight;
        }
        break;
    }
    case ScrollBlockDirectionForward: {
        if (isVertical) {
            if (!isFlipped)
                return ScrollDown;
            return ScrollUp;
        } else {
            if (!isFlipped)
                return ScrollRight;
            return ScrollLeft;
        }
        break;
    }
    case ScrollInlineDirectionBackward: {
        if (isVertical) {
            if (!isFlipped)
                return ScrollLeft;
            return ScrollRight;
        } else {
            if (!isFlipped)
                return ScrollUp;
            return ScrollDown;
        }
        break;
    }
    case ScrollInlineDirectionForward: {
        if (isVertical) {
            if (!isFlipped)
                return ScrollRight;
            return ScrollLeft;
        } else {
            if (!isFlipped)
                return ScrollDown;
            return ScrollUp;
        }
        break;
    }
    }
    return ScrollUp;
}

enum class ScrollGranularity : uint8_t {
    Line,
    Page,
    Document,
    Pixel
};

enum class ScrollElasticity : uint8_t {
    Automatic,
    None,
    Allowed
};

enum class ScrollbarOrientation : uint8_t {
    Horizontal,
    Vertical
};

enum class ScrollbarMode : uint8_t {
    Auto,
    AlwaysOff,
    AlwaysOn
};

enum class ScrollbarControlSize : uint8_t {
    Regular,
    Small
};

enum class ScrollbarExpansionState : uint8_t {
    Regular,
    Expanded
};

enum class ScrollEventAxis : uint8_t {
    Horizontal,
    Vertical
};

inline constexpr ScrollEventAxis axisFromDirection(ScrollDirection direction)
{
    switch (direction) {
    case ScrollUp: return ScrollEventAxis::Vertical;
    case ScrollDown: return ScrollEventAxis::Vertical;
    case ScrollLeft: return ScrollEventAxis::Horizontal;
    case ScrollRight: return ScrollEventAxis::Horizontal;
    }
    ASSERT_NOT_REACHED();
    return ScrollEventAxis::Vertical;
}

inline float valueForAxis(FloatSize size, ScrollEventAxis axis)
{
    switch (axis) {
    case ScrollEventAxis::Horizontal: return size.width();
    case ScrollEventAxis::Vertical: return size.height();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

inline FloatSize setValueForAxis(FloatSize size, ScrollEventAxis axis, float value)
{
    switch (axis) {
    case ScrollEventAxis::Horizontal:
        size.setWidth(value);
        return size;
    case ScrollEventAxis::Vertical:
        size.setHeight(value);
        return size;
    }
    ASSERT_NOT_REACHED();
    return size;
}

inline float valueForAxis(FloatPoint point, ScrollEventAxis axis)
{
    switch (axis) {
    case ScrollEventAxis::Horizontal: return point.x();
    case ScrollEventAxis::Vertical: return point.y();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

inline FloatPoint setValueForAxis(FloatPoint point, ScrollEventAxis axis, float value)
{
    switch (axis) {
    case ScrollEventAxis::Horizontal:
        point.setX(value);
        return point;
    case ScrollEventAxis::Vertical: 
        point.setY(value);
        return point;
    }
    ASSERT_NOT_REACHED();
    return point;
}

inline BoxSide boxSideForDirection(ScrollDirection direction)
{
    switch (direction) {
    case ScrollDirection::ScrollUp:
        return BoxSide::Top;
    case ScrollDirection::ScrollDown:
        return BoxSide::Bottom;
    case ScrollDirection::ScrollLeft:
        return BoxSide::Left;
    case ScrollDirection::ScrollRight:
        return BoxSide::Right;
    }
    ASSERT_NOT_REACHED();
    return BoxSide::Top;
}

enum ScrollbarControlStateMask {
    ActiveScrollbarState = 1,
    EnabledScrollbarState = 1 << 1,
    PressedScrollbarState = 1 << 2
};

enum ScrollbarPart {
    NoPart = 0,
    BackButtonStartPart = 1,
    ForwardButtonStartPart = 1 << 1,
    BackTrackPart = 1 << 2,
    ThumbPart = 1 << 3,
    ForwardTrackPart = 1 << 4,
    BackButtonEndPart = 1 << 5,
    ForwardButtonEndPart = 1 << 6,
    ScrollbarBGPart = 1 << 7,
    TrackBGPart = 1 << 8,
    AllParts = 0xffffffff
};

enum ScrollbarButtonsPlacement : uint8_t {
    ScrollbarButtonsNone,
    ScrollbarButtonsSingle,
    ScrollbarButtonsDoubleStart,
    ScrollbarButtonsDoubleEnd,
    ScrollbarButtonsDoubleBoth
};

enum class ScrollbarStyle : uint8_t {
    AlwaysVisible,
    Overlay
};

enum ScrollbarOverlayStyle: uint8_t {
    ScrollbarOverlayStyleDefault,
    ScrollbarOverlayStyleDark,
    ScrollbarOverlayStyleLight
};

enum ScrollPinningBehavior : uint8_t {
    DoNotPin,
    PinToTop,
    PinToBottom
};

enum class ScrollClamping : bool {
    Unclamped,
    Clamped
};

enum ScrollBehaviorForFixedElements : bool {
    StickToDocumentBounds,
    StickToViewportBounds
};

enum class ScrollbarButtonPressAction : uint8_t {
    None,
    CenterOnThumb,
    StartDrag,
    Scroll
};

enum class SelectionRevealMode : uint8_t  {
    Reveal,
    RevealUpToMainFrame, // Scroll overflow and iframes, but not the main frame.
    DelegateMainFrameScroll, // Similar to RevealUpToMainFrame, but make sure to manually call the main frame scroll.
    DoNotReveal
};

enum class ScrollPositioningBehavior : uint8_t {
    None,
    Moves,
    Stationary
};

// This value controls the method used to select snap points during scrolling. This may either
// be "directional" or "closest." The directional method only chooses snap points that are at or
// beyond the scroll destination in the direction of the scroll. The "closest" method does not
// have this constraint.
enum class ScrollSnapPointSelectionMethod : uint8_t {
    Directional,
    Closest,
};

using ScrollbarControlState = unsigned;
using ScrollbarControlPartMask = unsigned;
using ScrollingNodeID = uint64_t;

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollType);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollClamping);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollBehaviorForFixedElements);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollElasticity);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollbarMode);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, OverflowAnchor);

struct ScrollPositionChangeOptions {
    ScrollType type;
    ScrollClamping clamping = ScrollClamping::Clamped;
    ScrollIsAnimated animated = ScrollIsAnimated::No;
    ScrollSnapPointSelectionMethod snapPointSelectionMethod = ScrollSnapPointSelectionMethod::Closest;

    static ScrollPositionChangeOptions createProgrammatic()
    {
        return { ScrollType::Programmatic };
    }

    static ScrollPositionChangeOptions createProgrammaticWithOptions(ScrollClamping clamping, ScrollIsAnimated animated, ScrollSnapPointSelectionMethod snapPointSelectionMethod)
    {
        return { ScrollType::Programmatic, clamping, animated, snapPointSelectionMethod };
    }

    static ScrollPositionChangeOptions createUser()
    {
        return { ScrollType::User };
    }

    static ScrollPositionChangeOptions createProgrammaticUnclamped()
    {
        return { ScrollType::Programmatic, ScrollClamping::Unclamped };
    }
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ScrollIsAnimated> {
    using values = EnumValues<
        WebCore::ScrollIsAnimated,
        WebCore::ScrollIsAnimated::No,
        WebCore::ScrollIsAnimated::Yes
    >;
};

template<> struct EnumTraits<WebCore::ScrollPinningBehavior> {
    using values = EnumValues<
        WebCore::ScrollPinningBehavior,
        WebCore::ScrollPinningBehavior::DoNotPin,
        WebCore::ScrollPinningBehavior::PinToTop,
        WebCore::ScrollPinningBehavior::PinToBottom
    >;
};

template<> struct EnumTraits<WebCore::ScrollGranularity> {
    using values = EnumValues<
        WebCore::ScrollGranularity,
        WebCore::ScrollGranularity::Line,
        WebCore::ScrollGranularity::Page,
        WebCore::ScrollGranularity::Document,
        WebCore::ScrollGranularity::Pixel
    >;
};

template<> struct EnumTraits<WebCore::ScrollDirection> {
    using values = EnumValues<
        WebCore::ScrollDirection,
        WebCore::ScrollDirection::ScrollUp,
        WebCore::ScrollDirection::ScrollDown,
        WebCore::ScrollDirection::ScrollLeft,
        WebCore::ScrollDirection::ScrollRight
    >;
};
} // namespace WTF
