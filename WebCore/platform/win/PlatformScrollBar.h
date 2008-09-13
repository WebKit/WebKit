/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "ScrollBar.h"
#include "Timer.h"
#include <wtf/PassRefPtr.h>

typedef struct HDC__* HDC;

namespace WebCore {

class PlatformScrollbar : public Scrollbar {
public:
    static PassRefPtr<PlatformScrollbar> create(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
    {
        return adoptRef(new PlatformScrollbar(client, orientation, size));
    }

    virtual ~PlatformScrollbar();

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

protected:    
    virtual void updateThumbPosition();
    virtual void updateThumbProportion();

private:
    PlatformScrollbar(ScrollbarClient*, ScrollbarOrientation, ScrollbarControlSize);

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

    bool thumbUnderMouse();

    void invalidatePart(ScrollbarPart);
    void invalidateTrack();
};

}

#endif // PlatformScrollbar_h

