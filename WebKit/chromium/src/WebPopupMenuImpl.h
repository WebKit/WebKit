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
#include "WebPoint.h"
#include "WebPopupMenu.h"
#include "WebSize.h"
#include <wtf/RefCounted.h>

namespace WebCore {
class Frame;
class FramelessScrollView;
class KeyboardEvent;
class Page;
class PlatformKeyboardEvent;
class Range;
class Widget;
}

namespace WebKit {
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
struct WebRect;

class WebPopupMenuImpl : public WebPopupMenu,
                         public WebCore::FramelessScrollViewClient,
                         public RefCounted<WebPopupMenuImpl> {
public:
    // WebWidget
    virtual void close();
    virtual WebSize size() { return m_size; }
    virtual void resize(const WebSize&);
    virtual void layout();
    virtual void paint(WebCanvas* canvas, const WebRect& rect);
    virtual void themeChanged();
    virtual void composite(bool finish);
    virtual bool handleInputEvent(const WebInputEvent&);
    virtual void mouseCaptureLost();
    virtual void setFocus(bool enable);
    virtual bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart, int selectionEnd);
    virtual bool confirmComposition();
    virtual WebTextInputType textInputType();
    virtual WebRect caretOrSelectionBounds();
    virtual void setTextDirection(WebTextDirection direction);
    virtual bool isAcceleratedCompositingActive() const { return false; }

    // WebPopupMenuImpl
    void Init(WebCore::FramelessScrollView* widget,
              const WebRect& bounds);

    WebWidgetClient* client() { return m_client; }

    void MouseMove(const WebMouseEvent&);
    void MouseLeave(const WebMouseEvent&);
    void MouseDown(const WebMouseEvent&);
    void MouseUp(const WebMouseEvent&);
    void MouseDoubleClick(const WebMouseEvent&);
    void MouseWheel(const WebMouseWheelEvent&);
    bool KeyEvent(const WebKeyboardEvent&);

   protected:
    friend class WebPopupMenu;  // For WebPopupMenu::create
    friend class WTF::RefCounted<WebPopupMenuImpl>;

    WebPopupMenuImpl(WebWidgetClient* client);
    ~WebPopupMenuImpl();

    // WebCore::HostWindow methods:
    virtual void invalidateContents(const WebCore::IntRect&, bool);
    virtual void invalidateWindow(const WebCore::IntRect&, bool);
    virtual void invalidateContentsAndWindow(const WebCore::IntRect&, bool);
    virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&, bool);
    virtual void scroll(
        const WebCore::IntSize& scrollDelta, const WebCore::IntRect& scrollRect,
        const WebCore::IntRect& clipRect);
    virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&) const;
    virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&) const;
    virtual PlatformPageClient platformPageClient() const { return 0; }
    virtual void scrollRectIntoView(const WebCore::IntRect&, const WebCore::ScrollView*) const;
    virtual void scrollbarsModeDidChange() const;
    virtual void setCursor(const WebCore::Cursor&);

    // WebCore::FramelessScrollViewClient methods:
    virtual void popupClosed(WebCore::FramelessScrollView*);

    WebWidgetClient* m_client;
    WebSize m_size;

    WebPoint m_lastMousePosition;

    // This is a non-owning ref.  The popup will notify us via popupClosed()
    // before it is destroyed.
    WebCore::FramelessScrollView* m_widget;
};

} // namespace WebKit

#endif
