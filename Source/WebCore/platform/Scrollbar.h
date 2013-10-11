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

#ifndef Scrollbar_h
#define Scrollbar_h

#include "ScrollbarThemeClient.h"
#include "ScrollTypes.h"
#include "Timer.h"
#include "Widget.h"
#include <wtf/MathExtras.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class GraphicsContext;
class IntRect;
class PlatformMouseEvent;
class ScrollableArea;
class ScrollbarTheme;

class Scrollbar : public Widget,
                  public ScrollbarThemeClient {

public:
    // Must be implemented by platforms that can't simply use the Scrollbar base class.  Right now the only platform that is not using the base class is GTK.
    static PassRefPtr<Scrollbar> createNativeScrollbar(ScrollableArea*, ScrollbarOrientation orientation, ScrollbarControlSize size);

    virtual ~Scrollbar();

    // ScrollbarThemeClient implementation.
    virtual int x() const OVERRIDE { return Widget::x(); }
    virtual int y() const OVERRIDE { return Widget::y(); }
    virtual int width() const OVERRIDE { return Widget::width(); }
    virtual int height() const OVERRIDE { return Widget::height(); }
    virtual IntSize size() const OVERRIDE { return Widget::size(); }
    virtual IntPoint location() const OVERRIDE { return Widget::location(); }

    virtual ScrollView* parent() const OVERRIDE { return Widget::parent(); }
    virtual ScrollView* root() const OVERRIDE { return Widget::root(); }

    virtual void setFrameRect(const IntRect&) OVERRIDE;
    virtual IntRect frameRect() const OVERRIDE { return Widget::frameRect(); }

    virtual void invalidate() OVERRIDE { Widget::invalidate(); }
    virtual void invalidateRect(const IntRect&) OVERRIDE;

    virtual ScrollbarOverlayStyle scrollbarOverlayStyle() const OVERRIDE;
    virtual bool isScrollableAreaActive() const OVERRIDE;
    virtual bool isScrollViewScrollbar() const OVERRIDE;

    virtual IntPoint convertFromContainingWindow(const IntPoint& windowPoint) OVERRIDE { return Widget::convertFromContainingWindow(windowPoint); }

    virtual bool isCustomScrollbar() const OVERRIDE { return false; }
    virtual ScrollbarOrientation orientation() const OVERRIDE { return m_orientation; }

    virtual int value() const OVERRIDE { return lroundf(m_currentPos); }
    virtual float currentPos() const OVERRIDE { return m_currentPos; }
    virtual int visibleSize() const OVERRIDE { return m_visibleSize; }
    virtual int totalSize() const OVERRIDE { return m_totalSize; }
    virtual int maximum() const OVERRIDE { return m_totalSize - m_visibleSize; }
    virtual ScrollbarControlSize controlSize() const OVERRIDE { return m_controlSize; }

    virtual int lineStep() const OVERRIDE { return m_lineStep; }
    virtual int pageStep() const OVERRIDE { return m_pageStep; }

    virtual ScrollbarPart pressedPart() const OVERRIDE { return m_pressedPart; }
    virtual ScrollbarPart hoveredPart() const OVERRIDE { return m_hoveredPart; }

    virtual void styleChanged() OVERRIDE { }

    virtual bool enabled() const OVERRIDE { return m_enabled; }
    virtual void setEnabled(bool) OVERRIDE;

    // Called by the ScrollableArea when the scroll offset changes.
    void offsetDidChange();

    static int pixelsPerLineStep() { return 40; }
    static float minFractionToStepWhenPaging() { return 0.875f; }
    static int maxOverlapBetweenPages();

    void disconnectFromScrollableArea() { m_scrollableArea = 0; }
    ScrollableArea* scrollableArea() const { return m_scrollableArea; }

    int pressedPos() const { return m_pressedPos; }

    float pixelStep() const { return m_pixelStep; }

    virtual void setHoveredPart(ScrollbarPart);
    virtual void setPressedPart(ScrollbarPart);

    void setSteps(int lineStep, int pageStep, int pixelsPerStep = 1);
    void setProportion(int visibleSize, int totalSize);
    void setPressedPos(int p) { m_pressedPos = p; }

    virtual void paint(GraphicsContext*, const IntRect& damageRect) OVERRIDE;

    virtual bool isOverlayScrollbar() const OVERRIDE;
    bool shouldParticipateInHitTesting();

    bool isWindowActive() const;

    // These methods are used for platform scrollbars to give :hover feedback.  They will not get called
    // when the mouse went down in a scrollbar, since it is assumed the scrollbar will start
    // grabbing all events in that case anyway.
    bool mouseMoved(const PlatformMouseEvent&);
    void mouseEntered();
    bool mouseExited();

    // Used by some platform scrollbars to know when they've been released from capture.
    bool mouseUp(const PlatformMouseEvent&);

    bool mouseDown(const PlatformMouseEvent&);

    ScrollbarTheme* theme() const { return m_theme; }

    virtual void setParent(ScrollView*) OVERRIDE;

    bool suppressInvalidation() const { return m_suppressInvalidation; }
    void setSuppressInvalidation(bool s) { m_suppressInvalidation = s; }

    virtual IntRect convertToContainingView(const IntRect&) const OVERRIDE;
    virtual IntRect convertFromContainingView(const IntRect&) const OVERRIDE;

    virtual IntPoint convertToContainingView(const IntPoint&) const OVERRIDE;
    virtual IntPoint convertFromContainingView(const IntPoint&) const OVERRIDE;

    void moveThumb(int pos, bool draggingDocument = false);

    virtual bool isAlphaLocked() const OVERRIDE { return m_isAlphaLocked; }
    virtual void setIsAlphaLocked(bool flag) OVERRIDE { m_isAlphaLocked = flag; }

    virtual bool supportsUpdateOnSecondaryThread() const;

protected:
    Scrollbar(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize, ScrollbarTheme* = 0);

    void updateThumb();
    virtual void updateThumbPosition();
    virtual void updateThumbProportion();

    void autoscrollTimerFired(Timer<Scrollbar>*);
    void startTimerIfNeeded(double delay);
    void stopTimerIfNeeded();
    void autoscrollPressedPart(double delay);
    ScrollDirection pressedPartScrollDirection();
    ScrollGranularity pressedPartScrollGranularity();

    ScrollableArea* m_scrollableArea;
    ScrollbarOrientation m_orientation;
    ScrollbarControlSize m_controlSize;
    ScrollbarTheme* m_theme;

    int m_visibleSize;
    int m_totalSize;
    float m_currentPos;
    float m_dragOrigin;
    int m_lineStep;
    int m_pageStep;
    float m_pixelStep;

    ScrollbarPart m_hoveredPart;
    ScrollbarPart m_pressedPart;
    int m_pressedPos;
    float m_scrollPos;
    bool m_draggingDocument;
    int m_documentDragPos;

    bool m_enabled;

    Timer<Scrollbar> m_scrollTimer;
    bool m_overlapsResizer;

    bool m_suppressInvalidation;

    bool m_isAlphaLocked;

private:
    virtual bool isScrollbar() const OVERRIDE { return true; }
};

} // namespace WebCore

#endif // Scrollbar_h
