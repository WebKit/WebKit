/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
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

#if !USE(SAFARI_THEME)

#include "PlatformScrollBar.h"

#include "EventHandler.h"
#include "FrameView.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "PlatformMouseEvent.h"
#include "SoftLinking.h"
#include "RenderThemeWin.h"

// Generic state constants
#define TS_NORMAL    1
#define TS_HOVER     2
#define TS_ACTIVE    3
#define TS_DISABLED  4

#define SP_BUTTON          1
#define SP_THUMBHOR        2
#define SP_THUMBVERT       3
#define SP_TRACKSTARTHOR   4
#define SP_TRACKENDHOR     5
#define SP_TRACKSTARTVERT  6
#define SP_TRACKENDVERT    7
#define SP_GRIPPERHOR      8
#define SP_GRIPPERVERT     9

#define TS_UP_BUTTON       0
#define TS_DOWN_BUTTON     4
#define TS_LEFT_BUTTON     8
#define TS_RIGHT_BUTTON    12
#define TS_UP_BUTTON_HOVER   17
#define TS_DOWN_BUTTON_HOVER  18
#define TS_LEFT_BUTTON_HOVER  19
#define TS_RIGHT_BUTTON_HOVER   20

using namespace std;

namespace WebCore {

static unsigned cHorizontalWidth;
static unsigned cHorizontalHeight;
static unsigned cVerticalWidth;
static unsigned cVerticalHeight;
static unsigned cThumbWidth;
static unsigned cThumbHeight;
static unsigned cGripperWidth;
static unsigned cGripperHeight;

static HANDLE scrollbarTheme;
static bool haveTheme;
static bool runningVista;

// FIXME:  Refactor the soft-linking code so that it can be shared with RenderThemeWin
SOFT_LINK_LIBRARY(uxtheme)
SOFT_LINK(uxtheme, OpenThemeData, HANDLE, WINAPI, (HWND hwnd, LPCWSTR pszClassList), (hwnd, pszClassList))
SOFT_LINK(uxtheme, CloseThemeData, HRESULT, WINAPI, (HANDLE hTheme), (hTheme))
SOFT_LINK(uxtheme, DrawThemeBackground, HRESULT, WINAPI, (HANDLE hTheme, HDC hdc, int iPartId, int iStateId, const RECT* pRect, const RECT* pClipRect), (hTheme, hdc, iPartId, iStateId, pRect, pClipRect))
SOFT_LINK(uxtheme, IsThemeActive, BOOL, WINAPI, (), ())
SOFT_LINK(uxtheme, IsThemeBackgroundPartiallyTransparent, BOOL, WINAPI, (HANDLE hTheme, int iPartId, int iStateId), (hTheme, iPartId, iStateId))

static bool isRunningOnVistaOrLater()
{
    static bool os = false;
    static bool initialized = false;
    if (!initialized) {
        OSVERSIONINFOEX vi = {sizeof(vi), 0};
        GetVersionEx((OSVERSIONINFO*)&vi);

        // NOTE: This does not work under a debugger - Vista shims Visual Studio, 
        // making it believe it is xpsp2, which is inherited by debugged applications
        os = vi.dwMajorVersion >= 6;
        initialized = true;
    }
    return os;
}

static void checkAndInitScrollbarTheme()
{
    if (uxthemeLibrary() && !scrollbarTheme)
        scrollbarTheme = OpenThemeData(0, L"Scrollbar");
    haveTheme = scrollbarTheme && IsThemeActive();
}

const double cInitialTimerDelay = 0.25;
const double cNormalTimerDelay = 0.05;

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
    : Scrollbar(client, orientation, size)
{
    // Obtain the correct scrollbar sizes from the system.
    // FIXME:  We should update these on a WM_SETTINGSCHANGE also.
    if (!cHorizontalHeight) {
       cHorizontalHeight = ::GetSystemMetrics(SM_CYHSCROLL);
       cHorizontalWidth = ::GetSystemMetrics(SM_CXHSCROLL);
       cVerticalHeight = ::GetSystemMetrics(SM_CYVSCROLL);
       cVerticalWidth = ::GetSystemMetrics(SM_CXVSCROLL);
       cThumbWidth = ::GetSystemMetrics(SM_CXHTHUMB);
       cThumbHeight = ::GetSystemMetrics(SM_CYVTHUMB);
       cGripperWidth = cThumbWidth / 2;
       cGripperHeight = cThumbHeight / 2;
       checkAndInitScrollbarTheme();
       runningVista = isRunningOnVistaOrLater();
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

void PlatformScrollbar::invalidateTrack()
{
    IntRect rect = trackRect();
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
            result = backButtonRect();
            break;
        case ForwardButtonPart:
            result = forwardButtonRect();
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
    if (graphicsContext->paintingDisabled())
        return;

    // Don't paint anything if the scrollbar doesn't intersect the damage rect.
    if (!frameGeometry().intersects(damageRect))
        return;

    checkAndInitScrollbarTheme();

    // A Windows scrollbar consists of six components:
    // An arrow button, a track piece, a thumb, a gripper inside the thumb, another track piece, and another arrow button.
    // Paint each piece if it intersects the damage rect.
    
    // (1) The first arrow button
    paintButton(graphicsContext, backButtonRect(), true, damageRect);

    IntRect rect;
    if (damageRect.intersects(rect = trackRect())) {
        if (isEnabled()) {
            IntRect startTrackRect, thumbRect, endTrackRect;
            splitTrack(rect, startTrackRect, thumbRect, endTrackRect);

            // (2) The first track piece
            paintTrack(graphicsContext, startTrackRect, true, damageRect);
        
            // (3) The thumb
            paintThumb(graphicsContext, thumbRect, damageRect);

            // (4) The second track piece
            paintTrack(graphicsContext, endTrackRect, false, damageRect);
        } else
            // Just paint a disabled track throughout the track rect.
            paintTrack(graphicsContext, rect, true, damageRect);
    }

    // (5) The second arrow button
    paintButton(graphicsContext, forwardButtonRect(), false, damageRect);
}

bool PlatformScrollbar::hasButtons() const
{
    return isEnabled();
}

bool PlatformScrollbar::hasThumb() const
{
    return isEnabled();
}

IntRect PlatformScrollbar::backButtonRect() const
{
    // Our desired rect is essentially 17x17.
    
    // Our actual rect will shrink to half the available space when
    // we have < 34 pixels left.  This allows the scrollbar
    // to scale down and function even at tiny sizes.
    if (m_orientation == HorizontalScrollbar)
        return IntRect(x(), y(), width() < 2 * cHorizontalWidth ? width() / 2 : cHorizontalWidth, cHorizontalHeight);
    return IntRect(x(), y(), cVerticalWidth, height() < 2 * cVerticalHeight ? height() / 2 : cVerticalHeight);
}

IntRect PlatformScrollbar::forwardButtonRect() const
{
    // Our desired rect is essentially 17x17.
    
    // Our actual rect will shrink to half the available space when
    // we have < 34 pixels left.  This allows the scrollbar
    // to scale down and function even at tiny sizes.

    if (m_orientation == HorizontalScrollbar) {
        int w = width() < 2 * cHorizontalWidth ? width() / 2 : cHorizontalWidth;
        return IntRect(x() + width() - w, y(), w, cHorizontalHeight);
    }
    
    int h = height() < 2 * cVerticalHeight ? height() / 2 : cVerticalHeight;
    return IntRect(x(), y() + height() - h, cVerticalWidth, h);
}

IntRect PlatformScrollbar::trackRect() const
{
    if (m_orientation == HorizontalScrollbar) {
        if (width() < 2 * cHorizontalWidth)
            return IntRect();
        return IntRect(x() + cHorizontalWidth, y(), width() - 2 * cHorizontalWidth, cHorizontalHeight);
    }

    if (height() < 2 * cVerticalHeight)
        return IntRect();
    return IntRect(x(), y() + cVerticalHeight, cVerticalWidth, height() - 2 * cVerticalHeight);
}

IntRect PlatformScrollbar::thumbRect() const
{
    IntRect beforeThumbRect, thumbRect, afterThumbRect;
    splitTrack(trackRect(), beforeThumbRect, thumbRect, afterThumbRect);
    return thumbRect;
}

IntRect PlatformScrollbar::gripperRect(const IntRect& thumbRect) const
{
    // Center in the thumb.
    return IntRect(thumbRect.x() + (thumbRect.width() - cGripperWidth) / 2,
                   thumbRect.y() + (thumbRect.height() - cGripperHeight) / 2,
                   cGripperWidth, cGripperHeight);
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
    int minLength = (m_orientation == HorizontalScrollbar) ? cThumbWidth : cThumbHeight;
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
    if (!damageRect.intersects(rect))
        return;

    int xpState = 0;
    int classicState = 0;
    if (m_orientation == HorizontalScrollbar)
        xpState = start ? TS_LEFT_BUTTON : TS_RIGHT_BUTTON;
    else
        xpState = start ? TS_UP_BUTTON : TS_DOWN_BUTTON;
    classicState = xpState / 4;

    if (!isEnabled()) {
        xpState += TS_DISABLED;
        classicState |= DFCS_INACTIVE;
    } else if ((m_hoveredPart == BackButtonPart && start) ||
               (m_hoveredPart == ForwardButtonPart && !start)) {
        if (m_pressedPart == m_hoveredPart) {
            xpState += TS_ACTIVE;
            classicState |= DFCS_PUSHED | DFCS_FLAT;
        } else
            xpState += TS_HOVER;
    } else {
        if (m_hoveredPart == NoPart || !runningVista)
            xpState += TS_NORMAL;
        else {
            if (m_orientation == HorizontalScrollbar)
                xpState = start ? TS_LEFT_BUTTON_HOVER : TS_RIGHT_BUTTON_HOVER;
            else
                xpState = start ? TS_UP_BUTTON_HOVER : TS_DOWN_BUTTON_HOVER;
        }
    }

    bool alphaBlend = false;
    if (scrollbarTheme)
        alphaBlend = IsThemeBackgroundPartiallyTransparent(scrollbarTheme, SP_BUTTON, xpState);
    HDC hdc = context->getWindowsContext(rect, alphaBlend);

    RECT themeRect(rect);
    if (scrollbarTheme)
        DrawThemeBackground(scrollbarTheme, hdc, SP_BUTTON, xpState, &themeRect, 0);
    else
        ::DrawFrameControl(hdc, &themeRect, DFC_SCROLL, classicState);
    context->releaseWindowsContext(hdc, rect, alphaBlend);
}

void PlatformScrollbar::paintTrack(GraphicsContext* context, const IntRect& rect, bool start, const IntRect& damageRect) const
{
    if (!damageRect.intersects(rect))
        return;

    int part;
    if (m_orientation == HorizontalScrollbar)
        part = start ? SP_TRACKSTARTHOR : SP_TRACKENDHOR;
    else
        part = start ? SP_TRACKSTARTVERT : SP_TRACKENDVERT;

    int state;
    if (!isEnabled())
        state = TS_DISABLED;
    else if ((m_hoveredPart == BackTrackPart && start) ||
             (m_hoveredPart == ForwardTrackPart && !start))
        state = (m_pressedPart == m_hoveredPart ? TS_ACTIVE : TS_HOVER);
    else
        state = TS_NORMAL;

    bool alphaBlend = false;
    if (scrollbarTheme)
        alphaBlend = IsThemeBackgroundPartiallyTransparent(scrollbarTheme, part, state);
    HDC hdc = context->getWindowsContext(rect, alphaBlend);
    RECT themeRect(rect);
    if (scrollbarTheme)
        DrawThemeBackground(scrollbarTheme, hdc, part, state, &themeRect, 0);
    else {
        DWORD color3DFace = ::GetSysColor(COLOR_3DFACE);
        DWORD colorScrollbar = ::GetSysColor(COLOR_SCROLLBAR);
        DWORD colorWindow = ::GetSysColor(COLOR_WINDOW);
        if ((color3DFace != colorScrollbar) && (colorWindow != colorScrollbar))
            ::FillRect(hdc, &themeRect, HBRUSH(COLOR_SCROLLBAR+1));
        else {
            static WORD patternBits[8] = { 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55 };
            HBITMAP patternBitmap = ::CreateBitmap(8, 8, 1, 1, patternBits);
            HBRUSH brush = ::CreatePatternBrush(patternBitmap);
            SaveDC(hdc);
            ::SetTextColor(hdc, ::GetSysColor(COLOR_3DHILIGHT));
            ::SetBkColor(hdc, ::GetSysColor(COLOR_3DFACE));
            ::SetBrushOrgEx(hdc, rect.x(), rect.y(), NULL);
            ::SelectObject(hdc, brush);
            ::FillRect(hdc, &themeRect, brush);
            ::RestoreDC(hdc, -1);
            ::DeleteObject(brush);  
            ::DeleteObject(patternBitmap);
        }
    }
    context->releaseWindowsContext(hdc, rect, alphaBlend);
}

void PlatformScrollbar::paintThumb(GraphicsContext* context, const IntRect& rect, const IntRect& damageRect) const
{
    if (!damageRect.intersects(rect))
        return;

    int state;
    if (!isEnabled())
        state = TS_DISABLED;
    else if (m_pressedPart == ThumbPart)
        state = TS_ACTIVE; // Thumb always stays active once pressed.
    else if (m_hoveredPart == ThumbPart)
        state = TS_HOVER;
    else
        state = TS_NORMAL;

    bool alphaBlend = false;
    if (scrollbarTheme)
        alphaBlend = IsThemeBackgroundPartiallyTransparent(scrollbarTheme, m_orientation == HorizontalScrollbar ? SP_THUMBHOR : SP_THUMBVERT, state);
    HDC hdc = context->getWindowsContext(rect, alphaBlend);
    RECT themeRect(rect);
    if (scrollbarTheme) {
        DrawThemeBackground(scrollbarTheme, hdc, m_orientation == HorizontalScrollbar ? SP_THUMBHOR : SP_THUMBVERT, state, &themeRect, 0);
        IntRect gripper;
        if (damageRect.intersects(gripper = gripperRect(rect)))
            paintGripper(hdc, gripper);
    } else
        ::DrawEdge(hdc, &themeRect, EDGE_RAISED, BF_RECT | BF_MIDDLE);
    context->releaseWindowsContext(hdc, rect, alphaBlend);
}

void PlatformScrollbar::paintGripper(HDC hdc, const IntRect& rect) const
{
    if (!scrollbarTheme)
        return;  // Classic look has no gripper.
   
    int state;
    if (!isEnabled())
        state = TS_DISABLED;
    else if (m_pressedPart == ThumbPart)
        state = TS_ACTIVE; // Thumb always stays active once pressed.
    else if (m_hoveredPart == ThumbPart)
        state = TS_HOVER;
    else
        state = TS_NORMAL;

    RECT themeRect(rect);
    DrawThemeBackground(scrollbarTheme, hdc, m_orientation == HorizontalScrollbar ? SP_GRIPPERHOR : SP_GRIPPERVERT, state, &themeRect, 0);
}

ScrollbarPart PlatformScrollbar::hitTest(const PlatformMouseEvent& evt)
{
    ScrollbarPart result = NoPart;
    if (!isEnabled() || !parent())
        return result;

    IntPoint mousePosition = convertFromContainingWindow(evt.pos());
    mousePosition.move(x(), y());
    if (backButtonRect().contains(mousePosition))
        result = BackButtonPart;
    else if (forwardButtonRect().contains(mousePosition))
        result = ForwardButtonPart;
    else {
        IntRect track = trackRect();
        if (track.contains(mousePosition)) {
            IntRect beforeThumbRect, thumbRect, afterThumbRect;
            splitTrack(track, beforeThumbRect, thumbRect, afterThumbRect);
            if (beforeThumbRect.contains(mousePosition))
                result = BackTrackPart;
            else if (thumbRect.contains(mousePosition))
                result = ThumbPart;
            else
                result = ForwardTrackPart;
        }
    }
    return result;
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
        if (m_hoveredPart == NoPart && runningVista)
            invalidate();  // Just invalidate the whole scrollbar, since the buttons at either end change anyway.

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
    if (runningVista)
        invalidate(); // Just invalidate the whole scrollbar, since the buttons at either end change anyway.
    else
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

bool PlatformScrollbar::thumbUnderMouse()
{
    // Construct a rect.
    IntRect thumb = thumbRect();
    thumb.move(-x(), -y());
    int begin = (m_orientation == HorizontalScrollbar) ? thumb.x() : thumb.y();
    int end = (m_orientation == HorizontalScrollbar) ? thumb.right() : thumb.bottom();
    return (begin <= m_pressedPos && m_pressedPos < end);
}

void PlatformScrollbar::themeChanged()
{
    if (scrollbarTheme) {
        CloseThemeData(scrollbarTheme);
        scrollbarTheme = 0;
    }

    haveTheme = false;
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

}

#endif

