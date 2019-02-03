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

#include <cstdint>
#include <wtf/Assertions.h>

namespace WebCore {

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
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return ScrollUp;
}

enum ScrollGranularity : uint8_t {
    ScrollByLine,
    ScrollByPage,
    ScrollByDocument,
    ScrollByPixel
};

enum ScrollElasticity : uint8_t {
    ScrollElasticityAutomatic,
    ScrollElasticityNone,
    ScrollElasticityAllowed
};

enum ScrollbarOrientation : uint8_t {
    HorizontalScrollbar,
    VerticalScrollbar
};

enum ScrollbarMode : uint8_t {
    ScrollbarAuto,
    ScrollbarAlwaysOff,
    ScrollbarAlwaysOn
};

enum ScrollbarControlSize : uint8_t {
    RegularScrollbar,
    SmallScrollbar
};

enum class ScrollbarExpansionState : uint8_t {
    Regular,
    Expanded
};

enum class ScrollEventAxis : uint8_t {
    Horizontal,
    Vertical
};

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

enum class ScrollClamping : uint8_t {
    Unclamped,
    Clamped
};

enum ScrollBehaviorForFixedElements : uint8_t {
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
    DoNotReveal
};

using ScrollbarControlState = unsigned;
using ScrollbarControlPartMask = unsigned;
using ScrollingNodeID = uint64_t;

} // namespace WebCore
