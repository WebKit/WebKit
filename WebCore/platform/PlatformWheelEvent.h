/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef PlatformWheelEvent_h
#define PlatformWheelEvent_h

#include "IntPoint.h"

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent;
#endif
#endif

#if PLATFORM(WIN)
typedef struct HWND__* HWND;
typedef unsigned WPARAM;
typedef long LPARAM;
#endif

#if PLATFORM(GTK)
typedef struct _GdkEventScroll GdkEventScroll;
#endif

#if PLATFORM(QT)
QT_BEGIN_NAMESPACE
class QWheelEvent;
QT_END_NAMESPACE
#endif

#if PLATFORM(WX)
class wxMouseEvent;
class wxPoint;
#endif

namespace WebCore {

    // Wheel events come in three flavors:
    // The ScrollByPixelWheelEvent is a fine-grained event that specifies the precise number of pixels to scroll.  It is sent by MacBook touchpads on OS X.
    // For ScrollByPixelWheelEvents, the delta values contain the precise number of pixels to scroll.
    // The ScrollByLineWheelEvent (the normal wheel event) sends a delta that can be corrected by a line multiplier to determine how many lines to scroll.
    //      If the platform has configurable line sensitivity (Windows), then the number of lines to scroll is used in order to behave like the platform.
    //      If the platform does not have configurable line sensitivity, then WebCore's default behavior is used (which scrolls 3 * the wheel line delta).
    // For ScrollByLineWheelEvents, the delta values represent the number of lines to scroll.
    // The ScrollByPageWheelEvent indicates that the wheel event should scroll an entire page instead.  In this case WebCore's built in paging behavior is used to page
    // up and down (you get the same behavior as if the user was clicking in a scrollbar track to page up or page down).  Page scrolling only works in the vertical direction.
    enum PlatformWheelEventGranularity { ScrollByLineWheelEvent, ScrollByPageWheelEvent, ScrollByPixelWheelEvent };
    
    // WebCore uses a line multiple of ~3 (40px per line step) when doing arrowing with a scrollbar or line stepping via the arrow keys.  The delta for wheeling is expressed
    // as a # of actual lines (40 / 3 = 1 wheel line).  We use the horizontalLineMultiplier and verticalLineMultiplier methods to incorporate the line multiplier into the deltas.  On
    // platforms that do not support wheel sensitivity, we use this hardcoded constant value of 3 to ensure that wheeling by default matches the WebCore multiplier you
    // get when doing other kinds of line stepping.
    const int cLineMultiplier = 3;

    class PlatformWheelEvent {
    public:
        const IntPoint& pos() const { return m_position; } // PlatformWindow coordinates.
        const IntPoint& globalPos() const { return m_globalPosition; } // Screen coordinates.

        float deltaX() const { return m_deltaX; }
        float deltaY() const { return m_deltaY; }

        PlatformWheelEventGranularity granularity() const { return m_granularity; }

        bool isAccepted() const { return m_isAccepted; }
        bool shiftKey() const { return m_shiftKey; }
        bool ctrlKey() const { return m_ctrlKey; }
        bool altKey() const { return m_altKey; }
        bool metaKey() const { return m_metaKey; }

        int x() const { return m_position.x(); } // PlatformWindow coordinates.
        int y() const { return m_position.y(); }
        int globalX() const { return m_globalPosition.x(); } // Screen coordinates.
        int globalY() const { return m_globalPosition.y(); }

        void accept() { m_isAccepted = true; }
        void ignore() { m_isAccepted = false; }

#if PLATFORM(MAC)
        PlatformWheelEvent(NSEvent*);
#endif
#if PLATFORM(WIN)
        PlatformWheelEvent(HWND, WPARAM, LPARAM, bool isHorizontal);
#endif
#if PLATFORM(GTK)
        PlatformWheelEvent(GdkEventScroll*);
#endif
#if PLATFORM(QT)
        PlatformWheelEvent(QWheelEvent*);
#endif
#if PLATFORM(WX)
        PlatformWheelEvent(const wxMouseEvent&, const wxPoint&);
#endif

    protected:
#if !PLATFORM(WIN)
        int horizontalLineMultiplier() const { return cLineMultiplier; }
        int verticalLineMultiplier() const { return cLineMultiplier; }
#else
        int horizontalLineMultiplier() const;
        int verticalLineMultiplier() const;
#endif

        IntPoint m_position;
        IntPoint m_globalPosition;
        float m_deltaX;
        float m_deltaY;
        PlatformWheelEventGranularity m_granularity;
        bool m_isAccepted;
        bool m_shiftKey;
        bool m_ctrlKey;
        bool m_altKey;
        bool m_metaKey;
    };

} // namespace WebCore

#endif // PlatformWheelEvent_h
