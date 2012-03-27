/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollbarGroup_h
#define ScrollbarGroup_h

#include "ScrollableArea.h"

#include <wtf/RefPtr.h>

namespace WebCore {
class FrameView;
}

namespace WebKit {

class WebScrollbarImpl;

class ScrollbarGroup : public WebCore::ScrollableArea {
public:
    ScrollbarGroup(WebCore::FrameView*, const WebCore::IntRect& frameRect);
    ~ScrollbarGroup();

    void scrollbarCreated(WebScrollbarImpl*);
    void scrollbarDestroyed(WebScrollbarImpl*);
    void setLastMousePosition(const WebCore::IntPoint&);
    void setFrameRect(const WebCore::IntRect&);

    // WebCore::ScrollableArea methods
    virtual int scrollSize(WebCore::ScrollbarOrientation) const;
    virtual int scrollPosition(WebCore::Scrollbar*) const;
    virtual void setScrollOffset(const WebCore::IntPoint&);
    virtual void invalidateScrollbarRect(WebCore::Scrollbar*, const WebCore::IntRect&);
    virtual void invalidateScrollCornerRect(const WebCore::IntRect&);
    virtual bool isActive() const;
    virtual ScrollableArea* enclosingScrollableArea() const;
    virtual WebCore::IntRect scrollCornerRect() const { return WebCore::IntRect(); }
    virtual bool isScrollCornerVisible() const;
    virtual void getTickmarks(Vector<WebCore::IntRect>&) const;
    virtual WebCore::IntPoint convertFromContainingViewToScrollbar(const WebCore::Scrollbar*, const WebCore::IntPoint& parentPoint) const;
    virtual WebCore::Scrollbar* horizontalScrollbar() const;
    virtual WebCore::Scrollbar* verticalScrollbar() const;
    virtual WebCore::IntPoint scrollPosition() const;
    virtual WebCore::IntPoint minimumScrollPosition() const;
    virtual WebCore::IntPoint maximumScrollPosition() const;
    virtual int visibleHeight() const;
    virtual int visibleWidth() const;
    virtual WebCore::IntSize contentsSize() const;
    virtual WebCore::IntSize overhangAmount() const;
    virtual WebCore::IntPoint currentMousePosition() const;
    virtual bool shouldSuspendScrollAnimations() const;
    virtual void scrollbarStyleChanged(int newStyle, bool forceUpdate);
    virtual bool isOnActivePage() const;
    virtual WebCore::IntRect scrollableAreaBoundingBox() const;

private:
    WebCore::FrameView* m_frameView;
    WebCore::IntPoint m_lastMousePosition;
    WebCore::IntRect m_frameRect;
    WebScrollbarImpl* m_horizontalScrollbar;
    WebScrollbarImpl* m_verticalScrollbar;
};

} // namespace WebKit

#endif
