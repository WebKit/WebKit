/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
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
    virtual int x() const override { return Widget::x(); }
    virtual int y() const override { return Widget::y(); }
    virtual int width() const override { return Widget::width(); }
    virtual int height() const override { return Widget::height(); }
    virtual IntSize size() const override { return Widget::size(); }
    virtual IntPoint location() const override { return Widget::location(); }

    virtual ScrollView* parent() const override { return Widget::parent(); }
    virtual ScrollView* root() const override { return Widget::root(); }

    virtual void setFrameRect(const IntRect&) override;
    virtual IntRect frameRect() const override { return Widget::frameRect(); }

    virtual void invalidate() override { Widget::invalidate(); }
    virtual void invalidateRect(const IntRect&) override;

    virtual ScrollbarOverlayStyle scrollbarOverlayStyle() const override;
    virtual bool isScrollableAreaActive() const override;
    virtual bool isScrollViewScrollbar() const override;

    virtual IntPoint convertFromContainingWindow(const IntPoint& windowPoint) override { return Widget::convertFromContainingWindow(windowPoint); }

    virtual bool isCustomScrollbar() const override { return false; }
    virtual ScrollbarOrientation orientation() const override { return m_orientation; }

    virtual int value() const override { return lroundf(m_currentPos); }
    virtual float currentPos() const override { return m_currentPos; }
    virtual int visibleSize() const override { return m_visibleSize; }
    virtual int totalSize() const override { return m_totalSize; }
    virtual int maximum() const override { return m_totalSize - m_visibleSize; }
    virtual ScrollbarControlSize controlSize() const override { return m_controlSize; }

    virtual int lineStep() const override { return m_lineStep; }
    virtual int pageStep() const override { return m_pageStep; }

    virtual ScrollbarPart pressedPart() const override { return m_pressedPart; }
    virtual ScrollbarPart hoveredPart() const override { return m_hoveredPart; }

    virtual void styleChanged() override { }

    virtual bool enabled() const override { return m_enabled; }
    virtual void setEnabled(bool) override;

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

    virtual void paint(GraphicsContext*, const IntRect& damageRect) override;

    virtual bool isOverlayScrollbar() const override;
    bool shouldParticipateInHitTesting();

    bool isWindowActive() const;

    // These methods are used for platform scrollbars to give :hover feedback.  They will not get called
    // when the mouse went down in a scrollbar, since it is assumed the scrollbar will start
    // grabbing all events in that case anyway.
#if !PLATFORM(IOS)
    bool mouseMoved(const PlatformMouseEvent&);
#endif
    void mouseEntered();
    bool mouseExited();

    // Used by some platform scrollbars to know when they've been released from capture.
    bool mouseUp(const PlatformMouseEvent&);

    bool mouseDown(const PlatformMouseEvent&);

    ScrollbarTheme* theme() const { return m_theme; }

    virtual void setParent(ScrollView*) override;

    bool suppressInvalidation() const { return m_suppressInvalidation; }
    void setSuppressInvalidation(bool s) { m_suppressInvalidation = s; }

    virtual IntRect convertToContainingView(const IntRect&) const override;
    virtual IntRect convertFromContainingView(const IntRect&) const override;

    virtual IntPoint convertToContainingView(const IntPoint&) const override;
    virtual IntPoint convertFromContainingView(const IntPoint&) const override;

    void moveThumb(int pos, bool draggingDocument = false);

    virtual bool isAlphaLocked() const override { return m_isAlphaLocked; }
    virtual void setIsAlphaLocked(bool flag) override { m_isAlphaLocked = flag; }

    virtual bool supportsUpdateOnSecondaryThread() const;

protected:
    Scrollbar(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize, ScrollbarTheme* = 0);

    void updateThumb();
    virtual void updateThumbPosition();
    virtual void updateThumbProportion();

    void autoscrollTimerFired(Timer<Scrollbar>&);
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
    virtual bool isScrollbar() const override { return true; }
};

WIDGET_TYPE_CASTS(Scrollbar, isScrollbar());

} // namespace WebCore

#endif // Scrollbar_h
