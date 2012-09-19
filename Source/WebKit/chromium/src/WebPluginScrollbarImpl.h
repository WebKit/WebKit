/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPluginScrollbarImpl_h
#define WebPluginScrollbarImpl_h

#include "WebPluginScrollbar.h"

#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {
class IntPoint;
class IntRect;
class Scrollbar;
}

namespace WebKit {

class ScrollbarGroup;

class WebPluginScrollbarImpl : public WebPluginScrollbar {
public:
    WebPluginScrollbarImpl(Orientation,
                     ScrollbarGroup*,
                     WebPluginScrollbarClient*);
    ~WebPluginScrollbarImpl();

    void setScrollOffset(int);
    void invalidateScrollbarRect(const WebCore::IntRect&);
    // FIXME: Combine this with the other getTickmarks method
    void getTickmarks(Vector<WebCore::IntRect>&) const;
    WebCore::IntPoint convertFromContainingViewToScrollbar(const WebCore::IntPoint& parentPoint) const;
    void scrollbarStyleChanged();

    int scrollOffset() { return m_scrollOffset; }
    WebCore::Scrollbar* scrollbar() { return m_scrollbar.get(); }

    // WebKit::WebScrollbar methods
    virtual bool isOverlay() const OVERRIDE;
    virtual int value() const OVERRIDE;
    virtual WebPoint location() const OVERRIDE;
    virtual WebSize size() const OVERRIDE;
    virtual bool enabled() const OVERRIDE;
    virtual int maximum() const OVERRIDE;
    virtual int totalSize() const OVERRIDE;
    virtual bool isScrollViewScrollbar() const OVERRIDE;
    virtual bool isScrollableAreaActive() const OVERRIDE;
    virtual void getTickmarks(WebVector<WebRect>& tickmarks) const OVERRIDE;
    virtual WebScrollbar::ScrollbarControlSize controlSize() const OVERRIDE;
    virtual WebScrollbar::ScrollbarPart pressedPart() const OVERRIDE;
    virtual WebScrollbar::ScrollbarPart hoveredPart() const OVERRIDE;
    virtual WebScrollbar::ScrollbarOverlayStyle scrollbarOverlayStyle() const OVERRIDE;
    virtual WebScrollbar::Orientation orientation() const OVERRIDE;
    virtual bool isCustomScrollbar() const OVERRIDE;

    // WebKit::WebPluginScrollbar methods
    virtual void setLocation(const WebRect&) OVERRIDE;
    virtual void setValue(int position) OVERRIDE;
    virtual void setDocumentSize(int) OVERRIDE;
    virtual void scroll(ScrollDirection, ScrollGranularity, float multiplier) OVERRIDE;
    virtual void paint(WebCanvas*, const WebRect&) OVERRIDE;
    virtual bool handleInputEvent(const WebInputEvent&) OVERRIDE;
    virtual bool isAlphaLocked() const OVERRIDE;
    virtual void setIsAlphaLocked(bool) OVERRIDE;

private:
    bool onMouseDown(const WebInputEvent&);
    bool onMouseUp(const WebInputEvent&);
    bool onMouseMove(const WebInputEvent&);
    bool onMouseLeave(const WebInputEvent&);
    bool onMouseWheel(const WebInputEvent&);
    bool onKeyDown(const WebInputEvent&);

    ScrollbarGroup* m_group;
    WebPluginScrollbarClient* m_client;

    int m_scrollOffset;
    RefPtr<WebCore::Scrollbar> m_scrollbar;
};

} // namespace WebKit

#endif
