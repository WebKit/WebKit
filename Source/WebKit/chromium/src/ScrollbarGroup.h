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

class WebPluginScrollbarImpl;

class ScrollbarGroup : public WebCore::ScrollableArea {
public:
    ScrollbarGroup(WebCore::FrameView*, const WebCore::IntRect& frameRect);
    ~ScrollbarGroup();

    void scrollbarCreated(WebPluginScrollbarImpl*);
    void scrollbarDestroyed(WebPluginScrollbarImpl*);
    void setLastMousePosition(const WebCore::IntPoint&);
    void setFrameRect(const WebCore::IntRect&);

    // WebCore::ScrollableArea methods
    virtual int scrollSize(WebCore::ScrollbarOrientation) const OVERRIDE;
    virtual int scrollPosition(WebCore::Scrollbar*) const OVERRIDE;
    virtual void setScrollOffset(const WebCore::IntPoint&) OVERRIDE;
    virtual void invalidateScrollbarRect(WebCore::Scrollbar*, const WebCore::IntRect&) OVERRIDE;
    virtual void invalidateScrollCornerRect(const WebCore::IntRect&) OVERRIDE;
    virtual bool isActive() const OVERRIDE;
    virtual ScrollableArea* enclosingScrollableArea() const OVERRIDE;
    virtual WebCore::IntRect scrollCornerRect() const OVERRIDE { return WebCore::IntRect(); }
    virtual bool isScrollCornerVisible() const OVERRIDE;
    virtual void getTickmarks(Vector<WebCore::IntRect>&) const OVERRIDE;
    virtual WebCore::IntPoint convertFromContainingViewToScrollbar(const WebCore::Scrollbar*, const WebCore::IntPoint& parentPoint) const OVERRIDE;
    virtual WebCore::Scrollbar* horizontalScrollbar() const OVERRIDE;
    virtual WebCore::Scrollbar* verticalScrollbar() const OVERRIDE;
    virtual WebCore::IntPoint scrollPosition() const OVERRIDE;
    virtual WebCore::IntPoint minimumScrollPosition() const OVERRIDE;
    virtual WebCore::IntPoint maximumScrollPosition() const OVERRIDE;
    virtual int visibleHeight() const OVERRIDE;
    virtual int visibleWidth() const OVERRIDE;
    virtual WebCore::IntSize contentsSize() const OVERRIDE;
    virtual WebCore::IntSize overhangAmount() const OVERRIDE;
    virtual WebCore::IntPoint lastKnownMousePosition() const OVERRIDE;
    virtual bool shouldSuspendScrollAnimations() const OVERRIDE;
    virtual void scrollbarStyleChanged(int newStyle, bool forceUpdate) OVERRIDE;
    virtual bool scrollbarsCanBeActive() const OVERRIDE;
    virtual WebCore::IntRect scrollableAreaBoundingBox() const OVERRIDE;

private:
    WebCore::FrameView* m_frameView;
    WebCore::IntPoint m_lastMousePosition;
    WebCore::IntRect m_frameRect;
    WebPluginScrollbarImpl* m_horizontalScrollbar;
    WebPluginScrollbarImpl* m_verticalScrollbar;
};

} // namespace WebKit

#endif
