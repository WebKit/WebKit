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

#include "config.h"
#include "WebPopupMenuImpl.h"

#include "Cursor.h"
#include "FramelessScrollView.h"
#include "FrameView.h"
#include "IntRect.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "SkiaUtils.h"

#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebRect.h"
#include "WebWidgetClient.h"

#include <skia/ext/platform_canvas.h>

using namespace WebCore;

namespace WebKit {

// WebPopupMenu ---------------------------------------------------------------

WebPopupMenu* WebPopupMenu::create(WebWidgetClient* client)
{
    return new WebPopupMenuImpl(client);
}

// WebWidget ------------------------------------------------------------------

WebPopupMenuImpl::WebPopupMenuImpl(WebWidgetClient* client)
    : m_client(client)
    , m_widget(0)
{
    // set to impossible point so we always get the first mouse pos
    m_lastMousePosition = WebPoint(-1, -1);
}

WebPopupMenuImpl::~WebPopupMenuImpl()
{
    if (m_widget)
        m_widget->setClient(0);
}

void WebPopupMenuImpl::Init(FramelessScrollView* widget, const WebRect& bounds)
{
    m_widget = widget;
    m_widget->setClient(this);

    if (m_client) {
        m_client->setWindowRect(bounds);
        m_client->show(WebNavigationPolicy());  // Policy is ignored
    }
}

void WebPopupMenuImpl::MouseMove(const WebMouseEvent& event)
{
    // don't send mouse move messages if the mouse hasn't moved.
    if (event.x != m_lastMousePosition.x || event.y != m_lastMousePosition.y) {
        m_lastMousePosition = WebPoint(event.x, event.y);
        m_widget->handleMouseMoveEvent(PlatformMouseEventBuilder(m_widget, event));
    }
}

void WebPopupMenuImpl::MouseLeave(const WebMouseEvent& event)
{
    m_widget->handleMouseMoveEvent(PlatformMouseEventBuilder(m_widget, event));
}

void WebPopupMenuImpl::MouseDown(const WebMouseEvent& event)
{
    m_widget->handleMouseDownEvent(PlatformMouseEventBuilder(m_widget, event));
}

void WebPopupMenuImpl::MouseUp(const WebMouseEvent& event)
{
    mouseCaptureLost();
    m_widget->handleMouseReleaseEvent(PlatformMouseEventBuilder(m_widget, event));
}

void WebPopupMenuImpl::MouseWheel(const WebMouseWheelEvent& event)
{
    m_widget->handleWheelEvent(PlatformWheelEventBuilder(m_widget, event));
}

bool WebPopupMenuImpl::KeyEvent(const WebKeyboardEvent& event)
{
    return m_widget->handleKeyEvent(PlatformKeyboardEventBuilder(event));
}

// WebWidget -------------------------------------------------------------------

void WebPopupMenuImpl::close()
{
    if (m_widget)
        m_widget->hide();

    m_client = 0;

    deref();  // Balances ref() from WebWidget::Create
}

void WebPopupMenuImpl::resize(const WebSize& newSize)
{
    if (m_size == newSize)
        return;
    m_size = newSize;

    if (m_widget) {
      IntRect newGeometry(0, 0, m_size.width, m_size.height);
      m_widget->setFrameRect(newGeometry);
    }

    if (m_client) {
      WebRect damagedRect(0, 0, m_size.width, m_size.height);
      m_client->didInvalidateRect(damagedRect);
    }
}

void WebPopupMenuImpl::layout()
{
}

void WebPopupMenuImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    if (!m_widget)
        return;

    if (!rect.isEmpty()) {
#if WEBKIT_USING_CG
        GraphicsContext gc(canvas);
#elif WEBKIT_USING_SKIA
        PlatformContextSkia context(canvas);
        // PlatformGraphicsContext is actually a pointer to PlatformContextSkia.
        GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
#else
        notImplemented();
#endif
        m_widget->paint(&gc, rect);
    }
}

bool WebPopupMenuImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    if (!m_widget)
        return false;

    // TODO (jcampan): WebKit seems to always return false on mouse events
    // methods. For now we'll assume it has processed them (as we are only
    // interested in whether keyboard events are processed).
    switch (inputEvent.type) {
    case WebInputEvent::MouseMove:
        MouseMove(*static_cast<const WebMouseEvent*>(&inputEvent));
        return true;

    case WebInputEvent::MouseLeave:
        MouseLeave(*static_cast<const WebMouseEvent*>(&inputEvent));
        return true;

    case WebInputEvent::MouseWheel:
        MouseWheel(*static_cast<const WebMouseWheelEvent*>(&inputEvent));
        return true;

    case WebInputEvent::MouseDown:
        MouseDown(*static_cast<const WebMouseEvent*>(&inputEvent));
        return true;

    case WebInputEvent::MouseUp:
        MouseUp(*static_cast<const WebMouseEvent*>(&inputEvent));
        return true;

    // In Windows, RawKeyDown only has information about the physical key, but
    // for "selection", we need the information about the character the key
    // translated into. For English, the physical key value and the character
    // value are the same, hence, "selection" works for English. But for other
    // languages, such as Hebrew, the character value is different from the
    // physical key value. Thus, without accepting Char event type which
    // contains the key's character value, the "selection" won't work for
    // non-English languages, such as Hebrew.
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
    case WebInputEvent::Char:
        return KeyEvent(*static_cast<const WebKeyboardEvent*>(&inputEvent));

    default:
        break;
    }
    return false;
}

void WebPopupMenuImpl::mouseCaptureLost()
{
}

void WebPopupMenuImpl::setFocus(bool enable)
{
}

bool WebPopupMenuImpl::handleCompositionEvent(
    WebCompositionCommand command, int cursorPosition, int targetStart,
    int targetEnd, const WebString& imeString)
{
    return false;
}

bool WebPopupMenuImpl::queryCompositionStatus(bool* enabled, WebRect* caretRect)
{
    return false;
}

void WebPopupMenuImpl::setTextDirection(WebTextDirection direction)
{
}


//-----------------------------------------------------------------------------
// WebCore::HostWindow

void WebPopupMenuImpl::invalidateContents(const IntRect&, bool)
{
    notImplemented();
}

void WebPopupMenuImpl::invalidateWindow(const IntRect&, bool)
{
    notImplemented();
}

void WebPopupMenuImpl::invalidateContentsAndWindow(const IntRect& paintRect, bool /*immediate*/)
{
    if (paintRect.isEmpty())
        return;
    if (m_client)
        m_client->didInvalidateRect(paintRect);
}

void WebPopupMenuImpl::invalidateContentsForSlowScroll(const IntRect& updateRect, bool immediate)
{
    invalidateContentsAndWindow(updateRect, immediate);
}

void WebPopupMenuImpl::scroll(const IntSize& scrollDelta,
                              const IntRect& scrollRect,
                              const IntRect& clipRect)
{
    if (m_client) {
        int dx = scrollDelta.width();
        int dy = scrollDelta.height();
        m_client->didScrollRect(dx, dy, clipRect);
    }
}

IntPoint WebPopupMenuImpl::screenToWindow(const IntPoint& point) const
{
    notImplemented();
    return IntPoint();
}

IntRect WebPopupMenuImpl::windowToScreen(const IntRect& rect) const
{
    notImplemented();
    return IntRect();
}

void WebPopupMenuImpl::scrollRectIntoView(const IntRect&, const ScrollView*) const
{
    // Nothing to be done here since we do not have the concept of a container
    // that implements its own scrolling.
}

void WebPopupMenuImpl::scrollbarsModeDidChange() const
{
    // Nothing to be done since we have no concept of different scrollbar modes.
}

//-----------------------------------------------------------------------------
// WebCore::FramelessScrollViewClient

void WebPopupMenuImpl::popupClosed(FramelessScrollView* widget)
{
    ASSERT(widget == m_widget);
    if (m_widget) {
        m_widget->setClient(0);
        m_widget = 0;
    }
    m_client->closeWidgetSoon();
}

} // namespace WebKit
