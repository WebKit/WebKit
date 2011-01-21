/*
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
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

#ifndef ScrollableArea_h
#define ScrollableArea_h

#include "IntRect.h"
#include "Scrollbar.h"
#include <wtf/Vector.h>

namespace WebCore {

class FloatPoint;
class PlatformWheelEvent;
class ScrollAnimator;

class ScrollableArea {
public:
    ScrollableArea();
    virtual ~ScrollableArea();

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1);
    void scrollToOffsetWithoutAnimation(const FloatPoint&);
    void scrollToOffsetWithoutAnimation(ScrollbarOrientation, float offset);
    void scrollToXOffsetWithoutAnimation(float x);
    void scrollToYOffsetWithoutAnimation(float x);

    virtual int scrollSize(ScrollbarOrientation) const = 0;
    virtual int scrollPosition(Scrollbar*) const = 0;
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) = 0;
    virtual bool isActive() const = 0;
    virtual bool scrollbarCornerPresent() const = 0;
    virtual void getTickmarks(Vector<IntRect>&) const { }

    // This function should be overriden by subclasses to perform the actual
    // scroll of the content.
    virtual void setScrollOffset(const IntPoint&) = 0;


    // Convert points and rects between the scrollbar and its containing view.
    // The client needs to implement these in order to be aware of layout effects
    // like CSS transforms.
    virtual IntRect convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& scrollbarRect) const
    {
        return scrollbar->Widget::convertToContainingView(scrollbarRect);
    }
    virtual IntRect convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
    {
        return scrollbar->Widget::convertFromContainingView(parentRect);
    }
    virtual IntPoint convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& scrollbarPoint) const
    {
        return scrollbar->Widget::convertToContainingView(scrollbarPoint);
    }
    virtual IntPoint convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
    {
        return scrollbar->Widget::convertFromContainingView(parentPoint);
    }

    virtual Scrollbar* horizontalScrollbar() const { return 0; }
    virtual Scrollbar* verticalScrollbar() const { return 0; }

private:
    // NOTE: Only called from the ScrollAnimator.
    friend class ScrollAnimator;
    void setScrollOffsetFromAnimation(const IntPoint&);

    OwnPtr<ScrollAnimator> m_scrollAnimator;
};

} // namespace WebCore

#endif // ScrollableArea_h
