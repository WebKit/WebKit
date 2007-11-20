/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc.  All rights reserved.
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

#ifndef PlatformScrollbar_h
#define PlatformScrollbar_h

#include "Widget.h"
#include "ScrollBar.h"
#include "Timer.h"

typedef struct HDC__* HDC;

namespace WebCore {

enum ScrollbarPart { NoPart, BackButtonPart, BackTrackPart, ThumbPart, ForwardTrackPart, ForwardButtonPart };

class PlatformScrollbar : public Widget, public Scrollbar {
public:
    PlatformScrollbar(ScrollbarClient*, ScrollbarOrientation, ScrollbarControlSize);

    virtual ~PlatformScrollbar();

    virtual bool isWidget() const { return true; }

    virtual void setParent(ScrollView*);

    virtual int width() const;
    virtual int height() const;
    virtual void setRect(const IntRect&);
    virtual void setEnabled(bool);
    virtual void paint(GraphicsContext*, const IntRect& damageRect);

    virtual bool handleMouseMoveEvent(const PlatformMouseEvent&);
    virtual bool handleMouseOutEvent(const PlatformMouseEvent&);
    virtual bool handleMousePressEvent(const PlatformMouseEvent&);
    virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&);

    virtual IntRect windowClipRect() const;

    static void themeChanged();
    static int horizontalScrollbarHeight(ScrollbarControlSize size = RegularScrollbar);
    static int verticalScrollbarWidth(ScrollbarControlSize size = RegularScrollbar);

    void autoscrollTimerFired(Timer<PlatformScrollbar>*);

protected:    
    virtual void updateThumbPosition();
    virtual void updateThumbProportion();

private:
    bool hasButtons() const;
    bool hasThumb() const;
    IntRect backButtonRect() const;
    IntRect forwardButtonRect() const;
    IntRect trackRect() const;
    IntRect thumbRect() const;
    IntRect gripperRect(const IntRect& thumbRect) const;    
    void splitTrack(const IntRect& trackRect, IntRect& beforeThumbRect, IntRect& thumbRect, IntRect& afterThumbRect) const;

    int thumbPosition() const;
    int thumbLength() const;
    int trackLength() const;

    void paintButton(GraphicsContext*, const IntRect& buttonRect, bool start, const IntRect& damageRect) const;
    void paintTrack(GraphicsContext*, const IntRect& trackRect, bool start, const IntRect& damageRect) const;
    void paintThumb(GraphicsContext*, const IntRect& thumbRect, const IntRect& damageRect) const;
    void paintGripper(HDC, const IntRect& gripperRect) const;
    
    ScrollbarPart hitTest(const PlatformMouseEvent&);
    
    void startTimerIfNeeded(double delay);
    void stopTimerIfNeeded();
    void autoscrollPressedPart(double delay);
    ScrollDirection pressedPartScrollDirection();
    ScrollGranularity pressedPartScrollGranularity();

    bool thumbUnderMouse();

    void invalidatePart(ScrollbarPart);
    void invalidateTrack();

    ScrollbarPart m_hoveredPart;
    ScrollbarPart m_pressedPart;
    int m_pressedPos;
    Timer<PlatformScrollbar> m_scrollTimer;
    bool m_overlapsResizer;
};

}

#endif // PlatformScrollbar_h

