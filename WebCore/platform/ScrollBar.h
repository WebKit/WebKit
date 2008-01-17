/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef ScrollBar_h
#define ScrollBar_h

#include <wtf/RefCounted.h>
#include "ScrollTypes.h"
#include <wtf/MathExtras.h>

namespace WebCore {

class GraphicsContext;
class IntRect;
class Scrollbar;
class PlatformMouseEvent;

// These match the numbers we use over in WebKit (WebFrameView.m).
#define LINE_STEP   40
#define PAGE_KEEP   40

enum ScrollbarControlSize { RegularScrollbar, SmallScrollbar, MiniScrollbar };

class ScrollbarClient {
public:
    virtual ~ScrollbarClient() {}
    virtual void valueChanged(Scrollbar*) = 0;

    // Used to obtain a window clip rect.
    virtual IntRect windowClipRect() const = 0;

    // FIXME: It would be nice to set this state on the scroll bar instead of
    // having to ask for it from the client at paint time.
    virtual bool isActive() const = 0;
};

class Scrollbar : public RefCounted<Scrollbar> {
protected:
    Scrollbar(ScrollbarClient*, ScrollbarOrientation, ScrollbarControlSize);

public:
    virtual ~Scrollbar() {}

    void setClient(ScrollbarClient* client) { m_client = client; }

    virtual bool isWidget() const = 0;

    ScrollbarOrientation orientation() const { return m_orientation; }
    int value() const { return lroundf(m_currentPos); } 
    
    ScrollbarControlSize controlSize() const { return m_controlSize; }

    void setSteps(int lineStep, int pageStep, int pixelsPerStep = 1);
    
    bool setValue(int);
    void setProportion(int visibleSize, int totalSize);

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1.0);
    
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void setRect(const IntRect&) = 0;
    virtual void setEnabled(bool) = 0;
    virtual void paint(GraphicsContext*, const IntRect& damageRect) = 0;

    static bool hasPlatformScrollbars() {
        // To use the platform's built-in scrollbars by default, return true.  We may
        // support styled engine scrollbars someday, and some platforms may wish to not
        // implement a platform scrollbar at all by default.  That's what this method is for.
        return true;
    }

    // These methods are used for platform scrollbars to give :hover feedback.  They will not get called
    // when the mouse went down in a scrollbar, since it is assumed the scrollbar will start
    // grabbing all events in that case anyway.
    virtual bool handleMouseMoveEvent(const PlatformMouseEvent&) { return false; }
    virtual bool handleMouseOutEvent(const PlatformMouseEvent&) { return false; }

    // Used by some platform scrollbars to know when they've been released from capture.
    virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&) { return false; }
   
protected:
    virtual void updateThumbPosition() = 0;
    virtual void updateThumbProportion() = 0;

    ScrollbarClient* client() const { return m_client; }

    ScrollbarClient* m_client;
    ScrollbarOrientation m_orientation;
    ScrollbarControlSize m_controlSize;
    int m_visibleSize;
    int m_totalSize;
    float m_currentPos;
    int m_lineStep;
    int m_pageStep;
    float m_pixelStep;
};

}

#endif
