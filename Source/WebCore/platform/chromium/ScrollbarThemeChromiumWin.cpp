/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2008, 2009 Google Inc.
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

#include "config.h"
#include "ScrollbarThemeChromiumWin.h"

#include <windows.h>
#include <vsstyle.h>

#include "GraphicsContext.h"
#include "LayoutTestSupport.h"
#include "PlatformContextSkia.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"
#include "SystemInfo.h"
#include <public/Platform.h>
#include <public/WebRect.h>
#include <public/win/WebThemeEngine.h>

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeChromiumWin theme;
    return &theme;
}

// The scrollbar size in DumpRenderTree on the Mac - so we can match their
// layout results.  Entries are for regular, small, and mini scrollbars.
// Metrics obtained using [NSScroller scrollerWidthForControlSize:]
static const int kMacScrollbarSize[3] = { 15, 11, 15 };

// Constants used to figure the drag rect outside which we should snap the
// scrollbar thumb back to its origin.  These calculations are based on
// observing the behavior of the MSVC8 main window scrollbar + some
// guessing/extrapolation.
static const int kOffEndMultiplier = 3;
static const int kOffSideMultiplier = 8;

int ScrollbarThemeChromiumWin::scrollbarThickness(ScrollbarControlSize controlSize)
{
    static int thickness;
    if (!thickness) {
        if (isRunningLayoutTest())
            return kMacScrollbarSize[controlSize];
        thickness = GetSystemMetrics(SM_CXVSCROLL);
    }
    return thickness;
}

bool ScrollbarThemeChromiumWin::invalidateOnMouseEnterExit()
{
    return windowsVersion() >= WindowsVista;
}

bool ScrollbarThemeChromiumWin::shouldSnapBackToDragOrigin(ScrollbarThemeClient* scrollbar, const PlatformMouseEvent& evt)
{
    // Find the rect within which we shouldn't snap, by expanding the track rect
    // in both dimensions.
    IntRect rect = trackRect(scrollbar);
    const bool horz = scrollbar->orientation() == HorizontalScrollbar;
    const int thickness = scrollbarThickness(scrollbar->controlSize());
    rect.inflateX((horz ? kOffEndMultiplier : kOffSideMultiplier) * thickness);
    rect.inflateY((horz ? kOffSideMultiplier : kOffEndMultiplier) * thickness);

    // Convert the event to local coordinates.
    IntPoint mousePosition = scrollbar->convertFromContainingWindow(evt.position());
    mousePosition.move(scrollbar->x(), scrollbar->y());

    // We should snap iff the event is outside our calculated rect.
    return !rect.contains(mousePosition);
}

void ScrollbarThemeChromiumWin::paintTrackPiece(GraphicsContext* gc, ScrollbarThemeClient* scrollbar, const IntRect& rect, ScrollbarPart partType)
{
    bool horz = scrollbar->orientation() == HorizontalScrollbar;

    int partId;
    if (partType == BackTrackPart)
        partId = horz ? SBP_UPPERTRACKHORZ : SBP_UPPERTRACKVERT;
    else
        partId = horz ? SBP_LOWERTRACKHORZ : SBP_LOWERTRACKVERT;

    IntRect alignRect = trackRect(scrollbar, false);

    WebKit::WebCanvas* canvas = gc->platformContext()->canvas();
    // Draw the track area before/after the thumb on the scroll bar.
    WebKit::Platform::current()->themeEngine()->paintScrollbarTrack(canvas, partId, getThemeState(scrollbar, partType), getClassicThemeState(scrollbar, partType), WebKit::WebRect(rect), WebKit::WebRect(alignRect));

}

void ScrollbarThemeChromiumWin::paintButton(GraphicsContext* gc, ScrollbarThemeClient* scrollbar, const IntRect& rect, ScrollbarPart part)
{
    bool horz = scrollbar->orientation() == HorizontalScrollbar;

    int partId;
    if (part == BackButtonStartPart || part == ForwardButtonStartPart)
        partId = horz ? DFCS_SCROLLLEFT : DFCS_SCROLLUP;
    else
        partId = horz ? DFCS_SCROLLRIGHT : DFCS_SCROLLDOWN;

    WebKit::WebCanvas* canvas = gc->platformContext()->canvas();
    // Draw the thumb (the box you drag in the scroll bar to scroll).
    WebKit::Platform::current()->themeEngine()->paintScrollbarArrow(canvas, getThemeArrowState(scrollbar, part), partId | getClassicThemeState(scrollbar, part), WebKit::WebRect(rect));
}

void ScrollbarThemeChromiumWin::paintThumb(GraphicsContext* gc, ScrollbarThemeClient* scrollbar, const IntRect& rect)
{
    bool horz = scrollbar->orientation() == HorizontalScrollbar;

    WebKit::WebCanvas* canvas = gc->platformContext()->canvas();
    // Draw the thumb (the box you drag in the scroll bar to scroll).
    WebKit::Platform::current()->themeEngine()->paintScrollbarThumb(canvas, horz ? SBP_THUMBBTNHORZ : SBP_THUMBBTNVERT, getThemeState(scrollbar, ThumbPart), getClassicThemeState(scrollbar, ThumbPart), WebKit::WebRect(rect));

    // Draw the gripper (the three little lines on the thumb).
    WebKit::Platform::current()->themeEngine()->paintScrollbarThumb(canvas, horz ? SBP_GRIPPERHORZ : SBP_GRIPPERVERT, getThemeState(scrollbar, ThumbPart), getClassicThemeState(scrollbar, ThumbPart), WebKit::WebRect(rect));
}

int ScrollbarThemeChromiumWin::getThemeState(ScrollbarThemeClient* scrollbar, ScrollbarPart part) const
{
    // When dragging the thumb, draw thumb pressed and other segments normal
    // regardless of where the cursor actually is.  See also four places in
    // getThemeArrowState().
    if (scrollbar->pressedPart() == ThumbPart) {
        if (part == ThumbPart)
            return SCRBS_PRESSED;
        return (windowsVersion() < WindowsVista) ? SCRBS_NORMAL : SCRBS_HOVER;
    }
    if (!scrollbar->enabled())
        return SCRBS_DISABLED;
    if (scrollbar->hoveredPart() != part || part == BackTrackPart || part == ForwardTrackPart)
        return (scrollbar->hoveredPart() == NoPart || (windowsVersion() < WindowsVista)) ? SCRBS_NORMAL : SCRBS_HOVER;
    if (scrollbar->pressedPart() == NoPart)
        return SCRBS_HOT;
    return (scrollbar->pressedPart() == part) ? SCRBS_PRESSED : SCRBS_NORMAL;
}

int ScrollbarThemeChromiumWin::getThemeArrowState(ScrollbarThemeClient* scrollbar, ScrollbarPart part) const
{
    // We could take advantage of knowing the values in the state enum to write
    // some simpler code, but treating the state enum as a black box seems
    // clearer and more future-proof.
    if (part == BackButtonStartPart || part == ForwardButtonStartPart) {
        if (scrollbar->orientation() == HorizontalScrollbar) {
            if (scrollbar->pressedPart() == ThumbPart)
                return (windowsVersion() < WindowsVista) ? ABS_LEFTNORMAL : ABS_LEFTHOVER;
            if (!scrollbar->enabled())
                return ABS_LEFTDISABLED;
            if (scrollbar->hoveredPart() != part)
                return ((scrollbar->hoveredPart() == NoPart) || (windowsVersion() < WindowsVista)) ? ABS_LEFTNORMAL : ABS_LEFTHOVER;
            if (scrollbar->pressedPart() == NoPart)
                return ABS_LEFTHOT;
            return (scrollbar->pressedPart() == part) ?
                ABS_LEFTPRESSED : ABS_LEFTNORMAL;
        }
        if (scrollbar->pressedPart() == ThumbPart)
            return (windowsVersion() < WindowsVista) ? ABS_UPNORMAL : ABS_UPHOVER;
        if (!scrollbar->enabled())
            return ABS_UPDISABLED;
        if (scrollbar->hoveredPart() != part)
            return ((scrollbar->hoveredPart() == NoPart) || (windowsVersion() < WindowsVista)) ? ABS_UPNORMAL : ABS_UPHOVER;
        if (scrollbar->pressedPart() == NoPart)
            return ABS_UPHOT;
        return (scrollbar->pressedPart() == part) ? ABS_UPPRESSED : ABS_UPNORMAL;
    }
    if (scrollbar->orientation() == HorizontalScrollbar) {
        if (scrollbar->pressedPart() == ThumbPart)
            return (windowsVersion() < WindowsVista) ? ABS_RIGHTNORMAL : ABS_RIGHTHOVER;
        if (!scrollbar->enabled())
            return ABS_RIGHTDISABLED;
        if (scrollbar->hoveredPart() != part)
            return ((scrollbar->hoveredPart() == NoPart) || (windowsVersion() < WindowsVista)) ? ABS_RIGHTNORMAL : ABS_RIGHTHOVER;
        if (scrollbar->pressedPart() == NoPart)
            return ABS_RIGHTHOT;
        return (scrollbar->pressedPart() == part) ? ABS_RIGHTPRESSED : ABS_RIGHTNORMAL;
    }
    if (scrollbar->pressedPart() == ThumbPart)
        return (windowsVersion() < WindowsVista) ? ABS_DOWNNORMAL : ABS_DOWNHOVER;
    if (!scrollbar->enabled())
        return ABS_DOWNDISABLED;
    if (scrollbar->hoveredPart() != part)
        return ((scrollbar->hoveredPart() == NoPart) || (windowsVersion() < WindowsVista)) ? ABS_DOWNNORMAL : ABS_DOWNHOVER;
    if (scrollbar->pressedPart() == NoPart)
        return ABS_DOWNHOT;
    return (scrollbar->pressedPart() == part) ? ABS_DOWNPRESSED : ABS_DOWNNORMAL;
}

int ScrollbarThemeChromiumWin::getClassicThemeState(ScrollbarThemeClient* scrollbar, ScrollbarPart part) const
{
    // When dragging the thumb, draw the buttons normal even when hovered.
    if (scrollbar->pressedPart() == ThumbPart)
        return 0;
    if (!scrollbar->enabled())
        return DFCS_INACTIVE;
    if (scrollbar->hoveredPart() != part || part == BackTrackPart || part == ForwardTrackPart)
        return 0;
    if (scrollbar->pressedPart() == NoPart)
        return DFCS_HOT;
    return (scrollbar->pressedPart() == part) ? (DFCS_PUSHED | DFCS_FLAT) : 0;
}

bool ScrollbarThemeChromiumWin::shouldCenterOnThumb(ScrollbarThemeClient*, const PlatformMouseEvent& evt)
{
    return evt.shiftKey() && evt.button() == LeftButton;
}

IntSize ScrollbarThemeChromiumWin::buttonSize(ScrollbarThemeClient* scrollbar)
{
    // Our desired rect is essentially thickness by thickness.

    // Our actual rect will shrink to half the available space when we have < 2
    // times thickness pixels left.  This allows the scrollbar to scale down
    // and function even at tiny sizes.

    int thickness = scrollbarThickness(scrollbar->controlSize());

    // In layout test mode, we force the button "girth" (i.e., the length of
    // the button along the axis of the scrollbar) to be a fixed size.
    // FIXME: This is retarded!  scrollbarThickness is already fixed in layout
    // test mode so that should be enough to result in repeatable results, but
    // preserving this hack avoids having to rebaseline pixel tests.
    const int kLayoutTestModeGirth = 17;
    int girth = isRunningLayoutTest() ? kLayoutTestModeGirth : thickness;

    if (scrollbar->orientation() == HorizontalScrollbar) {
        int width = scrollbar->width() < 2 * girth ? scrollbar->width() / 2 : girth;
        return IntSize(width, thickness);
    }

    int height = scrollbar->height() < 2 * girth ? scrollbar->height() / 2 : girth;
    return IntSize(thickness, height);
}


} // namespace WebCore
