/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Staikos Computing Services Inc. <info@staikos.net>
 * Copyright (C) 2007 Trolltech ASA
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

#include <QStyleOptionSlider>

namespace WebCore {

class PlatformScrollbar : public Widget, public Scrollbar {
public:
    PlatformScrollbar(ScrollbarClient*, ScrollbarOrientation, ScrollbarControlSize);
    virtual ~PlatformScrollbar();

    virtual bool isWidget() const { return true; }
    virtual int width() const;
    virtual int height() const;
    virtual void setRect(const IntRect&);

    virtual IntRect frameGeometry() const;
    virtual void setFrameGeometry(const IntRect& r);

    virtual void setEnabled(bool);
    virtual void paint(GraphicsContext*, const IntRect& damageRect);

    virtual bool handleMouseMoveEvent(const PlatformMouseEvent&);
    virtual bool handleMouseOutEvent(const PlatformMouseEvent&);
    virtual bool handleMousePressEvent(const PlatformMouseEvent&);
    virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&);

    bool isEnabled() const;

    static int horizontalScrollbarHeight(ScrollbarControlSize size = RegularScrollbar);
    static int verticalScrollbarWidth(ScrollbarControlSize size = RegularScrollbar);

    void autoscrollTimerFired(Timer<PlatformScrollbar>*);
    void invalidate();

protected:    
    virtual void updateThumbPosition();
    virtual void updateThumbProportion();

private:
    int thumbPosition() const;
    int thumbLength() const;
    int trackLength() const;

    void startTimerIfNeeded(double delay);
    void stopTimerIfNeeded();
    void autoscrollPressedPart(double delay);
    ScrollDirection pressedPartScrollDirection();
    ScrollGranularity pressedPartScrollGranularity();

    bool thumbUnderMouse();

    int m_pressedPos;
    QStyle::SubControl m_pressedPart;
    QStyle::SubControl m_hoveredPart;
    Timer<PlatformScrollbar> m_scrollTimer;
    QStyleOptionSlider m_opt;
};

}

#endif // PlatformScrollbar_h

