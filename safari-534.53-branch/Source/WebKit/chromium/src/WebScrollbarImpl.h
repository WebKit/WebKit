/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebScrollbarImpl_h
#define WebScrollbarImpl_h

#include "ScrollableArea.h"
#include "WebScrollbar.h"

#include <wtf/RefPtr.h>

namespace WebCore {
class Scrollbar;
}

namespace WebKit {

class WebScrollbarImpl : public WebScrollbar,
                         public WebCore::ScrollableArea {
public:
    WebScrollbarImpl(WebScrollbarClient*, Orientation orientation);
    ~WebScrollbarImpl();

    // WebKit::WebScrollbar methods
    virtual void setLocation(const WebRect&);
    virtual int value() const;
    virtual void setValue(int position);
    virtual void setDocumentSize(int size);
    virtual void scroll(ScrollDirection, ScrollGranularity, float multiplier);
    virtual void paint(WebCanvas*, const WebRect&);
    virtual bool handleInputEvent(const WebInputEvent&);

    // WebCore::ScrollableArea methods
    virtual int scrollSize(WebCore::ScrollbarOrientation) const;
    virtual int scrollPosition(WebCore::Scrollbar*) const;
    virtual void setScrollOffset(const WebCore::IntPoint&);
    virtual void invalidateScrollbarRect(WebCore::Scrollbar*, const WebCore::IntRect&);
    virtual void invalidateScrollCornerRect(const WebCore::IntRect&);
    virtual bool isActive() const;
    virtual WebCore::IntRect scrollCornerRect() const { return WebCore::IntRect(); }
    virtual bool isScrollCornerVisible() const;
    virtual void getTickmarks(Vector<WebCore::IntRect>&) const;
    virtual WebCore::Scrollbar* horizontalScrollbar() const;
    virtual WebCore::Scrollbar* verticalScrollbar() const;

private:
    bool onMouseDown(const WebInputEvent& event);
    bool onMouseUp(const WebInputEvent& event);
    bool onMouseMove(const WebInputEvent& event);
    bool onMouseLeave(const WebInputEvent& event);
    bool onMouseWheel(const WebInputEvent& event);
    bool onKeyDown(const WebInputEvent& event);

    WebScrollbarClient* m_client;

    int m_scrollOffset;
    RefPtr<WebCore::Scrollbar> m_scrollbar;
};

} // namespace WebKit

#endif
