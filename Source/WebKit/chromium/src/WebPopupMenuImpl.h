/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebPopupMenuImpl_h
#define WebPopupMenuImpl_h

#include "FramelessScrollViewClient.h"
#include "WebPopupMenu.h"
#include "platform/WebPoint.h"
#include "platform/WebSize.h"
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class Frame;
class FramelessScrollView;
class KeyboardEvent;
class Page;
#if ENABLE(GESTURE_RECOGNIZER)
class PlatformGestureRecognizer;
#endif
class PlatformKeyboardEvent;
class Range;
class Widget;
}

namespace WebKit {
class WebGestureEvent;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebRange;
struct WebRect;
class WebTouchEvent;

class WebPopupMenuImpl : public WebPopupMenu,
                         public WebCore::FramelessScrollViewClient,
                         public RefCounted<WebPopupMenuImpl> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // WebWidget functions:
    virtual void close() OVERRIDE;
    virtual WebSize size() OVERRIDE { return m_size; }
    virtual void willStartLiveResize() OVERRIDE;
    virtual void resize(const WebSize&) OVERRIDE;
    virtual void willEndLiveResize() OVERRIDE;
    virtual void animate(double frameBeginTime) OVERRIDE;
    virtual void layout() OVERRIDE;
    virtual void paint(WebCanvas*, const WebRect&) OVERRIDE;
    virtual void themeChanged() OVERRIDE;
    virtual void composite(bool finish) OVERRIDE;
    virtual bool handleInputEvent(const WebInputEvent&) OVERRIDE;
    virtual void mouseCaptureLost() OVERRIDE;
    virtual void setFocus(bool enable) OVERRIDE;
    virtual bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart, int selectionEnd) OVERRIDE;
    virtual bool confirmComposition() OVERRIDE;
    virtual bool confirmComposition(const WebString& text) OVERRIDE;
    virtual bool compositionRange(size_t* location, size_t* length) OVERRIDE;
    virtual WebTextInputType textInputType() OVERRIDE;
    virtual bool caretOrSelectionRange(size_t* location, size_t* length) OVERRIDE;
    virtual void setTextDirection(WebTextDirection) OVERRIDE;
    virtual bool isAcceleratedCompositingActive() const OVERRIDE { return false; }

    // WebPopupMenuImpl
    void init(WebCore::FramelessScrollView* widget, const WebRect& bounds);

    WebWidgetClient* client() { return m_client; }

    void handleMouseMove(const WebMouseEvent&);
    void handleMouseLeave(const WebMouseEvent&);
    void handleMouseDown(const WebMouseEvent&);
    void handleMouseUp(const WebMouseEvent&);
    void handleMouseDoubleClick(const WebMouseEvent&);
    void handleMouseWheel(const WebMouseWheelEvent&);
    bool handleGestureEvent(const WebGestureEvent&);
    bool handleTouchEvent(const WebTouchEvent&);
    bool handleKeyEvent(const WebKeyboardEvent&);

   protected:
    friend class WebPopupMenu; // For WebPopupMenu::create.
    friend class WTF::RefCounted<WebPopupMenuImpl>;

    WebPopupMenuImpl(WebWidgetClient*);
    ~WebPopupMenuImpl();

    // WebCore::HostWindow methods:
    virtual void invalidateRootView(const WebCore::IntRect&, bool) OVERRIDE;
    virtual void invalidateContentsAndRootView(const WebCore::IntRect&, bool) OVERRIDE;
    virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&, bool) OVERRIDE;
    virtual void scheduleAnimation() OVERRIDE;
    virtual void scroll(
        const WebCore::IntSize& scrollDelta, const WebCore::IntRect& scrollRect,
        const WebCore::IntRect& clipRect) OVERRIDE;
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) const OVERRIDE;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const OVERRIDE;
    virtual PlatformPageClient platformPageClient() const OVERRIDE { return 0; }
    virtual void scrollbarsModeDidChange() const OVERRIDE;
    virtual void setCursor(const WebCore::Cursor&) OVERRIDE;
    virtual void setCursorHiddenUntilMouseMoves(bool) OVERRIDE;

    // WebCore::FramelessScrollViewClient methods:
    virtual void popupClosed(WebCore::FramelessScrollView*) OVERRIDE;

    WebWidgetClient* m_client;
    WebSize m_size;

    WebPoint m_lastMousePosition;

    // This is a non-owning ref. The popup will notify us via popupClosed()
    // before it is destroyed.
    WebCore::FramelessScrollView* m_widget;

#if ENABLE(GESTURE_RECOGNIZER)
    OwnPtr<WebCore::PlatformGestureRecognizer> m_gestureRecognizer;
#endif
};

} // namespace WebKit

#endif
