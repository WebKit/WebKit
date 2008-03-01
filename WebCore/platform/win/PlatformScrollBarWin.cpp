/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Brent Fulgham
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "PlatformScrollBar.h"

#include "EventHandler.h"
#include "FrameView.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "PlatformMouseEvent.h"
#include "SoftLinking.h"

#include <windows.h>
#include "RenderThemeWin.h"

// Generic state constants
#define TS_NORMAL    1
#define TS_HOVER     2
#define TS_ACTIVE    3
#define TS_DISABLED  4
#define TS_FOCUSED   5

#define ABS_UPNORMAL 1
#define ABS_DOWNNORMAL 5
#define ABS_LEFTNORMAL 9
#define ABS_RIGHTNORMAL 13

static const unsigned SP_ABS_HOT_MODIFIER = 1;
static const unsigned SP_ABS_PRESSED_MODIFIER = 2;
static const unsigned SP_ABS_DISABLE_MODIFIER = 3;

// Scrollbar constants
#define SP_BUTTON 1
#define SP_THUMBHOR 2
#define SP_THUMBVERT 3
#define SP_TRACKSTARTHOR 4
#define SP_TRACKENDHOR 5
#define SP_TRACKSTARTVERT 6
#define SP_TRACKENDVERT 7
#define SP_GRIPPERHOR 8
#define SP_GRIPPERVERT 9

using namespace std;

namespace WebCore {

// FIXME: We should get these numbers from SafariTheme
static int cHorizontalWidth;
static int cHorizontalHeight;
static int cVerticalWidth;
static int cVerticalHeight;
static int cHorizontalButtonWidth;
static int cVerticalButtonHeight;
static int cRealButtonLength = 28;
static int cButtonInset = 14;
static int cButtonHitInset = 3;
// cRealButtonLength - cButtonInset
static int cThumbWidth;
static int cThumbHeight;
static int cThumbMinLength = 26;

static HANDLE cScrollBarTheme = 0;

// FIXME:  Refactor the soft-linking code so that it can be shared with RenderThemeWin
SOFT_LINK_LIBRARY(uxtheme)
SOFT_LINK(uxtheme, OpenThemeData, HANDLE, WINAPI, (HWND hwnd, LPCWSTR pszClassList), (hwnd, pszClassList))
SOFT_LINK(uxtheme, CloseThemeData, HRESULT, WINAPI, (HANDLE hTheme), (hTheme))
SOFT_LINK(uxtheme, DrawThemeBackground, HRESULT, WINAPI, (HANDLE hTheme, HDC hdc, int iPartId, int iStateId, const RECT* pRect, const RECT* pClipRect), (hTheme, hdc, iPartId, iStateId, pRect, pClipRect))
SOFT_LINK(uxtheme, DrawThemeEdge, HRESULT, WINAPI, (HANDLE hTheme, HDC hdc, int iPartId, int iStateId, const RECT* pRect, unsigned uEdge, unsigned uFlags, const RECT* pClipRect), (hTheme, hdc, iPartId, iStateId, pRect, uEdge, uFlags, pClipRect))
SOFT_LINK(uxtheme, GetThemeContentRect, HRESULT, WINAPI, (HANDLE hTheme, HDC hdc, int iPartId, int iStateId, const RECT* pRect, const RECT* pContentRect), (hTheme, hdc, iPartId, iStateId, pRect, pContentRect))
SOFT_LINK(uxtheme, GetThemePartSize, HRESULT, WINAPI, (HANDLE hTheme, HDC hdc, int iPartId, int iStateId, RECT* pRect, int ts, SIZE* psz), (hTheme, hdc, iPartId, iStateId, pRect, ts, psz))
SOFT_LINK(uxtheme, GetThemeSysFont, HRESULT, WINAPI, (HANDLE hTheme, int iFontId, OUT LOGFONT* pFont), (hTheme, iFontId, pFont))
SOFT_LINK(uxtheme, GetThemeColor, HRESULT, WINAPI, (HANDLE hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, OUT COLORREF* pColor), (hTheme, hdc, iPartId, iStateId, iPropId, pColor))

static void checkAndInitScrollbarTheme()
{
    if (uxthemeLibrary() && !cScrollBarTheme)
        cScrollBarTheme = OpenThemeData(0, L"Scrollbar");
}

// May need to add stuff to these later, so keep the graphics context retrieval/release in some helpers.
static HDC prepareForDrawing(GraphicsContext* g, const IntRect& r)
{
    return g->getWindowsContext(r);
}
 
static void doneDrawing(GraphicsContext* g, HDC hdc, const IntRect& r)
{
    g->releaseWindowsContext(hdc, r);
}
// End Copied from RenderThemeWin

const double cInitialTimerDelay = 0.25;
const double cNormalTimerDelay = 0.05;

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
    : Scrollbar(client, orientation, size)
    , m_hoveredPart(NoPart)
    , m_pressedPart(NoPart)
    , m_pressedPos(0)
    , m_scrollTimer(this, &PlatformScrollbar::autoscrollTimerFired)
    , m_overlapsResizer(false)
{
    // Obtain the correct scrollbar sizes from the system.
    // FIXME:  We should update these on a WM_SETTINGSCHANGE, too.
    if (!cHorizontalHeight) {
       cHorizontalHeight = ::GetSystemMetrics(SM_CYHSCROLL);
       cHorizontalWidth = ::GetSystemMetrics(SM_CXHSCROLL);
       cVerticalHeight = ::GetSystemMetrics(SM_CYVSCROLL);
       cVerticalWidth = ::GetSystemMetrics(SM_CXVSCROLL);
       cThumbWidth = ::GetSystemMetrics(SM_CXHTHUMB);
       cThumbHeight = ::GetSystemMetrics(SM_CYVTHUMB);
       cHorizontalButtonWidth = ::GetSystemMetrics(SM_CYVSCROLL);
       cVerticalButtonHeight = ::GetSystemMetrics(SM_CXHSCROLL);
    }

    if (orientation == VerticalScrollbar)
        setFrameGeometry(IntRect(0, 0, cVerticalWidth, cVerticalHeight));
    else
        setFrameGeometry(IntRect(0, 0, cHorizontalWidth, cHorizontalHeight));
}

PlatformScrollbar::~PlatformScrollbar()
{
    stopTimerIfNeeded();
}

void PlatformScrollbar::updateThumbPosition()
{
    invalidateTrack();
}

void PlatformScrollbar::updateThumbProportion()
{
    invalidateTrack();
}

static IntRect trackRepaintRect(const IntRect& trackRect, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
{
    const int cButtonLength = (orientation == VerticalScrollbar) ? cVerticalButtonHeight : cHorizontalButtonWidth;

    IntRect paintRect(trackRect);
    if (orientation == HorizontalScrollbar)
        paintRect.inflateX(cButtonLength);
    else
        paintRect.inflateY(cButtonLength);

    return paintRect;
}

static IntRect buttonRepaintRect(const IntRect& buttonRect, ScrollbarOrientation orientation, ScrollbarControlSize controlSize, bool start)
{
    IntRect paintRect(buttonRect);
    if (orientation == HorizontalScrollbar) {
        paintRect.setWidth(cRealButtonLength);
        if (!start)
            paintRect.setX(buttonRect.x() - (cRealButtonLength - buttonRect.width()));
    } else {
        paintRect.setHeight(cRealButtonLength);
        if (!start)
            paintRect.setY(buttonRect.y() - (cRealButtonLength - buttonRect.height()));
    }

    return paintRect;
}

void PlatformScrollbar::invalidateTrack()
{
    IntRect rect = trackRepaintRect(trackRect(), m_orientation, controlSize());
    rect.move(-x(), -y());
    invalidateRect(rect);
}

void PlatformScrollbar::invalidatePart(ScrollbarPart part)
{
    if (part == NoPart)
        return;

    IntRect result;    
    switch (part) {
        case BackButtonPart:
            result = buttonRepaintRect(backButtonRect(), m_orientation, controlSize(), true);
            break;
        case ForwardButtonPart:
            result = buttonRepaintRect(forwardButtonRect(), m_orientation, controlSize(), false);
            break;
        default: {
            IntRect beforeThumbRect, thumbRect, afterThumbRect;
            splitTrack(trackRect(), beforeThumbRect, thumbRect, afterThumbRect);
            if (part == BackTrackPart)
                result = beforeThumbRect;
            else if (part == ForwardTrackPart)
                result = afterThumbRect;
            else
                result = thumbRect;
        }
    }
    result.move(-x(), -y());
    invalidateRect(result);
}

int PlatformScrollbar::width() const
{
    return Widget::width();
}

int PlatformScrollbar::height() const
{
    return Widget::height();
}

void PlatformScrollbar::setRect(const IntRect& rect)
{
    // Get our window resizer rect and see if we overlap.  Adjust to avoid the overlap
    // if necessary.
    IntRect adjustedRect(rect);
    if (parent() && parent()->isFrameView()) {
        bool overlapsResizer = false;
        FrameView* view = static_cast<FrameView*>(parent());
        IntRect resizerRect = view->windowResizerRect();
        resizerRect.setLocation(view->convertFromContainingWindow(resizerRect.location()));
        if (rect.intersects(resizerRect)) {
            if (orientation() == HorizontalScrollbar) {
                int overlap = rect.right() - resizerRect.x();
                if (overlap > 0 && resizerRect.right() >= rect.right()) {
                    adjustedRect.setWidth(rect.width() - overlap);
                    overlapsResizer = true;
                }
            } else {
                int overlap = rect.bottom() - resizerRect.y();
                if (overlap > 0 && resizerRect.bottom() >= rect.bottom()) {
                    adjustedRect.setHeight(rect.height() - overlap);
                    overlapsResizer = true;
                }
            }
        }

        if (overlapsResizer != m_overlapsResizer) {
            m_overlapsResizer = overlapsResizer;
            view->adjustOverlappingScrollbarCount(m_overlapsResizer ? 1 : -1);
        }
    }

    setFrameGeometry(adjustedRect);
}

void PlatformScrollbar::setParent(ScrollView* parentView)
{
    if (!parentView && m_overlapsResizer && parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->adjustOverlappingScrollbarCount(-1);
    Widget::setParent(parentView);
}

void PlatformScrollbar::setEnabled(bool enabled)
{
    if (enabled != isEnabled()) {
        Widget::setEnabled(enabled);
        invalidate();
    }
}

void PlatformScrollbar::paint(GraphicsContext* graphicsContext, const IntRect& damageRect)
{
    if (graphicsContext->updatingControlTints()) {
        invalidate();
        return;
    }

    if (graphicsContext->paintingDisabled())
        return;

    // Don't paint anything if the scrollbar doesn't intersect the damage rect.
    if (!frameGeometry().intersects(damageRect))
        return;

    IntRect track = trackRect();
    paintTrack(graphicsContext, track, true, damageRect);

    if (hasButtons()) {
        paintButton(graphicsContext, backButtonRect(), true, damageRect);
        paintButton(graphicsContext, forwardButtonRect(), false, damageRect);
    }

    if (hasThumb() && damageRect.intersects(track)) {
        IntRect startTrackRect, thumbRect, endTrackRect;
        splitTrack(track, startTrackRect, thumbRect, endTrackRect);
        paintThumb(graphicsContext, thumbRect, damageRect);
    }
}

bool PlatformScrollbar::hasButtons() const
{
    return isEnabled() && (m_orientation == HorizontalScrollbar ? width() : height()) >= 2 * (cRealButtonLength - cButtonHitInset);
}

bool PlatformScrollbar::hasThumb() const
{
    return isEnabled() && (m_orientation == HorizontalScrollbar ? width() : height()) >= 2 * cButtonInset + cThumbMinLength + 1;
}

IntRect PlatformScrollbar::backButtonRect() const
{
    // Our actual rect will shrink to half the available space when
    // we have < 34 pixels left.  This allows the scrollbar
    // to scale down and function even at tiny sizes.
    if (m_orientation == HorizontalScrollbar)
        return IntRect(x(), y(), cHorizontalButtonWidth, cHorizontalHeight);
    return IntRect(x(), y(), cVerticalWidth, cVerticalButtonHeight);
}

IntRect PlatformScrollbar::forwardButtonRect() const
{
    // Our desired rect is essentially 17x17.
    
    // Our actual rect will shrink to half the available space when
    // we have < 34 pixels left.  This allows the scrollbar
    // to scale down and function even at tiny sizes.
    if (m_orientation == HorizontalScrollbar)
        return IntRect(x() + width() - cHorizontalButtonWidth, y(), cHorizontalButtonWidth, cHorizontalHeight);
    return IntRect(x(), y() + height() - cVerticalButtonHeight, cVerticalWidth, cVerticalButtonHeight);
}

IntRect PlatformScrollbar::trackRect() const
{
    if (m_orientation == HorizontalScrollbar) {
        if (!hasButtons())
            return IntRect(x(), y(), width(), cHorizontalHeight);
        return IntRect(x() + cHorizontalButtonWidth, y(), width() - 2 * cHorizontalButtonWidth, cHorizontalHeight);
    }

    if (!hasButtons())
        return IntRect(x(), y(), cVerticalWidth, height());
    return IntRect(x(), y() + cVerticalButtonHeight, cVerticalWidth, height() - 2 * cVerticalButtonHeight);
}

IntRect PlatformScrollbar::thumbRect() const
{
    IntRect beforeThumbRect, thumbRect, afterThumbRect;
    splitTrack(trackRect(), beforeThumbRect, thumbRect, afterThumbRect);
    return thumbRect;
}

IntRect PlatformScrollbar::gripperRect(const IntRect& thumbRect) const
{
    return IntRect();
}

void PlatformScrollbar::splitTrack(const IntRect& trackRect, IntRect& beforeThumbRect, IntRect& thumbRect, IntRect& afterThumbRect) const
{
    // This function won't even get called unless we're big enough to have some combination of these three rects where at least
    // one of them is non-empty.
    int thumbPos = thumbPosition();
    if (m_orientation == HorizontalScrollbar) {
        thumbRect = IntRect(trackRect.x() + thumbPos, trackRect.y() + (trackRect.height() - cThumbHeight) / 2, thumbLength(), cThumbHeight);
        beforeThumbRect = IntRect(trackRect.x(), trackRect.y(), thumbPos, trackRect.height());
        afterThumbRect = IntRect(thumbRect.x() + thumbRect.width(), trackRect.y(), trackRect.right() - thumbRect.right(), trackRect.height());
    } else {
        thumbRect = IntRect(trackRect.x() + (trackRect.width() - cThumbWidth) / 2, trackRect.y() + thumbPos, cThumbWidth, thumbLength());
        beforeThumbRect = IntRect(trackRect.x(), trackRect.y(), trackRect.width(), thumbPos);
        afterThumbRect = IntRect(trackRect.x(), thumbRect.y() + thumbRect.height(), trackRect.width(), trackRect.bottom() - thumbRect.bottom());
    }
}

int PlatformScrollbar::thumbPosition() const
{
    if (isEnabled())
        return (float)m_currentPos * (trackLength() - thumbLength()) / (m_totalSize - m_visibleSize);
    return 0;
}

int PlatformScrollbar::thumbLength() const
{
    if (!isEnabled())
        return 0;

    float proportion = (float)(m_visibleSize) / m_totalSize;
    int trackLen = trackLength();
    int length = proportion * trackLen;
    int minLength = cThumbMinLength;
    length = max(length, minLength);
    if (length > trackLen)
        length = 0; // Once the thumb is below the track length, it just goes away (to make more room for the track).
    return length;
}

int PlatformScrollbar::trackLength() const
{
    return (m_orientation == HorizontalScrollbar) ? trackRect().width() : trackRect().height();
}

void PlatformScrollbar::paintButton(GraphicsContext* context, const IntRect& rect, bool start, const IntRect& damageRect) const
{
    IntRect paintRect = buttonRepaintRect(rect, m_orientation, controlSize(), start);
    
    if (!damageRect.intersects(paintRect))
        return;

    unsigned part = 0;
    unsigned state = 0;
    unsigned classicPart = 0;
    unsigned classicState = 0;

    if (m_orientation == HorizontalScrollbar) {
       state = start ? ABS_LEFTNORMAL : ABS_RIGHTNORMAL;
       classicPart = start ? DFCS_SCROLLLEFT : DFCS_SCROLLRIGHT;
    } else {
       state = start ? ABS_UPNORMAL : ABS_DOWNNORMAL;
       classicPart = start ? DFCS_SCROLLUP : DFCS_SCROLLDOWN;
    }

    if (!isEnabled()) {
        state += SP_ABS_DISABLE_MODIFIER;
        classicState |= DFCS_INACTIVE;
    } else if ((m_pressedPart == BackButtonPart && start)
            || (m_pressedPart == ForwardButtonPart && !start)) {
        state += SP_ABS_PRESSED_MODIFIER;
        classicState |= DFCS_PUSHED | DFCS_FLAT;
    } else if (m_client->isActive()) {
       state += SP_ABS_HOT_MODIFIER;
       classicState |= DFCS_HOT;
    }

    HDC hdc = prepareForDrawing(context, rect);
    RECT widgetRect = rect;
    checkAndInitScrollbarTheme();

    if (cScrollBarTheme)
        DrawThemeBackground(cScrollBarTheme, hdc, SP_BUTTON, state, &widgetRect, NULL);
    else
        DrawFrameControl(hdc, &widgetRect, classicPart, classicState);

    doneDrawing(context, hdc, rect);
}

void PlatformScrollbar::paintTrack(GraphicsContext* context, const IntRect& rect, bool start, const IntRect& damageRect) const
{
    IntRect paintRect = hasButtons() ? trackRepaintRect(rect, m_orientation, controlSize()) : rect;
    
    if (!damageRect.intersects(paintRect))
        return;

    unsigned part = 0;
    unsigned classicPart = DFC_SCROLL;
    if (m_orientation == HorizontalScrollbar)
       part = start ? SP_TRACKSTARTHOR : SP_TRACKENDHOR;
    else
       part = start ? SP_TRACKSTARTVERT : SP_TRACKENDVERT;

    unsigned state = TS_DISABLED;
    unsigned classicState = DFCS_MONO;
    if (m_client->isActive())
        state |= TS_ACTIVE;
    else
       classicState |= DFCS_INACTIVE;

    if (hasButtons())
        state |= TS_NORMAL;

    HDC hdc = prepareForDrawing(context, rect);
    RECT widgetRect = rect;
    checkAndInitScrollbarTheme();

    if (cScrollBarTheme)
        DrawThemeBackground(cScrollBarTheme, hdc, part, state, &widgetRect, NULL);
    else
        DrawFrameControl(hdc, &widgetRect, DFC_SCROLL, classicState);

    doneDrawing(context, hdc, rect);
}

void PlatformScrollbar::paintThumb(GraphicsContext* context, const IntRect& rect, const IntRect& damageRect) const
{
    if (!damageRect.intersects(rect))
        return;

    unsigned part = (m_orientation == HorizontalScrollbar) ? SP_THUMBHOR : SP_THUMBVERT;
    unsigned state = 0;

    if (!isEnabled())
        state += SP_ABS_DISABLE_MODIFIER;
    else if (m_client->isActive())
       state += SP_ABS_HOT_MODIFIER;

    HDC hdc = prepareForDrawing(context, rect);
    RECT widgetRect = rect;
    checkAndInitScrollbarTheme();

    if (cScrollBarTheme) {
        DrawThemeBackground(cScrollBarTheme, hdc, part, state, &widgetRect, NULL);
        paintGripper(hdc, widgetRect);
    } else {
        HGDIOBJ hSaveBrush = SelectObject(hdc, GetSysColorBrush(COLOR_BTNFACE));
        DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_RECT);
        SelectObject(hdc,hSaveBrush);
    }

    doneDrawing(context, hdc, rect);
}

void PlatformScrollbar::paintGripper(HDC hdc, const IntRect& rect) const
{
    unsigned part = (m_orientation == HorizontalScrollbar) ? SP_GRIPPERHOR : SP_GRIPPERVERT;
    unsigned state = 0;

    if (m_client->isActive())
        state |= TS_ACTIVE;

    RECT widgetRect = rect;
    checkAndInitScrollbarTheme();

    if (cScrollBarTheme)
        DrawThemeBackground(cScrollBarTheme, hdc, part, state, &widgetRect, NULL);
}

ScrollbarPart PlatformScrollbar::hitTest(const PlatformMouseEvent& evt)
{
    if (!isEnabled())
        return NoPart;

    IntPoint mousePosition = convertFromContainingWindow(evt.pos());
    mousePosition.move(x(), y());

    if (hasButtons()) {
        if (backButtonRect().contains(mousePosition))
            return BackButtonPart;

        if (forwardButtonRect().contains(mousePosition))
            return ForwardButtonPart;
    }

    if (!hasThumb())
        return NoPart;

    IntRect track = trackRect();
    if (track.contains(mousePosition)) {
        IntRect beforeThumbRect, thumbRect, afterThumbRect;
        splitTrack(track, beforeThumbRect, thumbRect, afterThumbRect);
        if (beforeThumbRect.contains(mousePosition))
            return BackTrackPart;
        if (thumbRect.contains(mousePosition))
            return ThumbPart;
        return ForwardTrackPart;
    }

    return NoPart;
}

bool PlatformScrollbar::handleMouseMoveEvent(const PlatformMouseEvent& evt)
{
    if (m_pressedPart == ThumbPart) {
        // Drag the thumb.
        int thumbPos = thumbPosition();
        int thumbLen = thumbLength();
        int trackLen = trackLength();
        int maxPos = trackLen - thumbLen;
        int delta = 0;
        if (m_orientation == HorizontalScrollbar)
            delta = convertFromContainingWindow(evt.pos()).x() - m_pressedPos;
        else
            delta = convertFromContainingWindow(evt.pos()).y() - m_pressedPos;

        if (delta > 0)
            // The mouse moved down/right.
            delta = min(maxPos - thumbPos, delta);
        else if (delta < 0)
            // The mouse moved up/left.
            delta = max(-thumbPos, delta);

        if (delta != 0) {
            setValue((float)(thumbPos + delta) * (m_totalSize - m_visibleSize) / (trackLen - thumbLen));
            m_pressedPos += thumbPosition() - thumbPos;
        }

        return true;
    }

    if (m_pressedPart != NoPart)
        m_pressedPos = (m_orientation == HorizontalScrollbar ? convertFromContainingWindow(evt.pos()).x() : convertFromContainingWindow(evt.pos()).y());

    ScrollbarPart part = hitTest(evt);    
    if (part != m_hoveredPart) {
        if (m_pressedPart != NoPart) {
            if (part == m_pressedPart) {
                // The mouse is moving back over the pressed part.  We
                // need to start up the timer action again.
                startTimerIfNeeded(cNormalTimerDelay);
                invalidatePart(m_pressedPart);
            } else if (m_hoveredPart == m_pressedPart) {
                // The mouse is leaving the pressed part.  Kill our timer
                // if needed.
                stopTimerIfNeeded();
                invalidatePart(m_pressedPart);
            }
        } else {
            invalidatePart(part);
            invalidatePart(m_hoveredPart);
        }
        m_hoveredPart = part;
    } 

    return true;
}

bool PlatformScrollbar::handleMouseOutEvent(const PlatformMouseEvent& evt)
{
    invalidatePart(m_hoveredPart);
    m_hoveredPart = NoPart;

    return true;
}

bool PlatformScrollbar::handleMousePressEvent(const PlatformMouseEvent& evt)
{
    m_pressedPart = hitTest(evt);
    m_pressedPos = (m_orientation == HorizontalScrollbar ? convertFromContainingWindow(evt.pos()).x() : convertFromContainingWindow(evt.pos()).y());
    invalidatePart(m_pressedPart);
    autoscrollPressedPart(cInitialTimerDelay);
    return true;
}

bool PlatformScrollbar::handleMouseReleaseEvent(const PlatformMouseEvent& evt)
{
    invalidatePart(m_pressedPart);
    m_pressedPart = NoPart;
    m_pressedPos = 0;
    stopTimerIfNeeded();

    if (parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->frame()->eventHandler()->setMousePressed(false);

    return true;
}

void PlatformScrollbar::startTimerIfNeeded(double delay)
{
    // Don't do anything for the thumb.
    if (m_pressedPart == ThumbPart)
        return;

    // Handle the track.  We halt track scrolling once the thumb is level
    // with us.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse()) {
        invalidatePart(m_pressedPart);
        m_hoveredPart = ThumbPart;
        return;
    }

    // We can't scroll if we've hit the beginning or end.
    ScrollDirection dir = pressedPartScrollDirection();
    if (dir == ScrollUp || dir == ScrollLeft) {
        if (m_currentPos == 0)
            return;
    } else {
        if (m_currentPos == m_totalSize - m_visibleSize)
            return;
    }

    m_scrollTimer.startOneShot(delay);
}

void PlatformScrollbar::stopTimerIfNeeded()
{
    if (m_scrollTimer.isActive())
        m_scrollTimer.stop();
}

void PlatformScrollbar::autoscrollPressedPart(double delay)
{
    // Don't do anything for the thumb or if nothing was pressed.
    if (m_pressedPart == ThumbPart || m_pressedPart == NoPart)
        return;

    // Handle the track.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse()) {
        invalidatePart(m_pressedPart);
        m_hoveredPart = ThumbPart;
        return;
    }

    // Handle the arrows and track.
    if (scroll(pressedPartScrollDirection(), pressedPartScrollGranularity()))
        startTimerIfNeeded(delay);
}

void PlatformScrollbar::autoscrollTimerFired(Timer<PlatformScrollbar>*)
{
    autoscrollPressedPart(cNormalTimerDelay);
}

ScrollDirection PlatformScrollbar::pressedPartScrollDirection()
{
    if (m_orientation == HorizontalScrollbar) {
        if (m_pressedPart == BackButtonPart || m_pressedPart == BackTrackPart)
            return ScrollLeft;
        return ScrollRight;
    } else {
        if (m_pressedPart == BackButtonPart || m_pressedPart == BackTrackPart)
            return ScrollUp;
        return ScrollDown;
    }
}

ScrollGranularity PlatformScrollbar::pressedPartScrollGranularity()
{
    if (m_pressedPart == BackButtonPart || m_pressedPart == ForwardButtonPart)
        return ScrollByLine;
    return ScrollByPage;
}

bool PlatformScrollbar::thumbUnderMouse()
{
    // Construct a rect.
    IntRect thumb = thumbRect();
    thumb.move(-x(), -y());
    int begin = (m_orientation == HorizontalScrollbar) ? thumb.x() : thumb.y();
    int end = (m_orientation == HorizontalScrollbar) ? thumb.right() : thumb.bottom();
    return (begin <= m_pressedPos && m_pressedPos < end);
}

int PlatformScrollbar::horizontalScrollbarHeight(ScrollbarControlSize controlSize)
{
    return cHorizontalWidth;
}

int PlatformScrollbar::verticalScrollbarWidth(ScrollbarControlSize controlSize)
{
    return cVerticalHeight;
}

IntRect PlatformScrollbar::windowClipRect() const
{
    IntRect clipRect(0, 0, width(), height());

    clipRect = convertToContainingWindow(clipRect);
    if (m_client)
        clipRect.intersect(m_client->windowClipRect());

    return clipRect;
}

void PlatformScrollbar::themeChanged()
{
}

}

