/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebScrollbarThemeClientImpl_h
#define WebScrollbarThemeClientImpl_h

#include "ScrollbarThemeClient.h"
#include <public/WebScrollbar.h>
#include <wtf/Noncopyable.h>

namespace WebCore {
class ScrollView;
}

namespace WebKit {

// Adapts a WebScrollbar to the ScrollbarThemeClient interface
class WebScrollbarThemeClientImpl : public WebCore::ScrollbarThemeClient {
    WTF_MAKE_NONCOPYABLE(WebScrollbarThemeClientImpl);
public:
    // Caller must retain ownership of this pointer and ensure that its lifetime
    // exceeds this instance.
    WebScrollbarThemeClientImpl(WebScrollbar*);

    // Implement WebCore::ScrollbarThemeClient interface
    virtual int x() const OVERRIDE;
    virtual int y() const OVERRIDE;
    virtual int width() const OVERRIDE;
    virtual int height() const OVERRIDE;
    virtual WebCore::IntSize size() const OVERRIDE;
    virtual WebCore::IntPoint location() const OVERRIDE;
    virtual WebCore::ScrollView* parent() const OVERRIDE;
    virtual WebCore::ScrollView* root() const OVERRIDE;
    virtual void setFrameRect(const WebCore::IntRect&) OVERRIDE;
    virtual WebCore::IntRect frameRect() const OVERRIDE;
    virtual void invalidate() OVERRIDE;
    virtual void invalidateRect(const WebCore::IntRect&) OVERRIDE;
    virtual WebCore::ScrollbarOverlayStyle scrollbarOverlayStyle() const OVERRIDE;
    virtual void getTickmarks(Vector<WebCore::IntRect>&) const OVERRIDE;
    virtual bool isScrollableAreaActive() const OVERRIDE;
    virtual bool isScrollViewScrollbar() const OVERRIDE;
    virtual WebCore::IntPoint convertFromContainingWindow(const WebCore::IntPoint&) OVERRIDE;
    virtual bool isCustomScrollbar() const OVERRIDE;
    virtual WebCore::ScrollbarOrientation orientation() const OVERRIDE;
    virtual int value() const OVERRIDE;
    virtual float currentPos() const OVERRIDE;
    virtual int visibleSize() const OVERRIDE;
    virtual int totalSize() const OVERRIDE;
    virtual int maximum() const OVERRIDE;
    virtual WebCore::ScrollbarControlSize controlSize() const OVERRIDE;
    virtual int lineStep() const OVERRIDE;
    virtual int pageStep() const OVERRIDE;
    virtual WebCore::ScrollbarPart pressedPart() const OVERRIDE;
    virtual WebCore::ScrollbarPart hoveredPart() const OVERRIDE;
    virtual void styleChanged() OVERRIDE;
    virtual bool enabled() const OVERRIDE;
    virtual void setEnabled(bool) OVERRIDE;
    virtual bool isOverlayScrollbar() const OVERRIDE;
    virtual bool isAlphaLocked() const OVERRIDE;
    virtual void setIsAlphaLocked(bool) OVERRIDE;

private:
    WebKit::WebScrollbar* m_scrollbar;
};

}

#endif // WebScrollbarThemeClientImpl_h
