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

#include "config.h"
#include "WebScrollbarImpl.h"

#include "GraphicsContext.h"
#include "KeyboardCodes.h"
#include "painting/GraphicsContextBuilder.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "ScrollTypes.h"
#include "WebCanvas.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebRect.h"
#include "WebScrollbarClient.h"
#include "WebVector.h"
#include "WebViewImpl.h"

using namespace std;
using namespace WebCore;

namespace WebKit {

WebScrollbar* WebScrollbar::create(WebScrollbarClient* client, Orientation orientation)
{
    return new WebScrollbarImpl(client, orientation);
}

int WebScrollbar::defaultThickness()
{
    return ScrollbarTheme::nativeTheme()->scrollbarThickness();
}

WebScrollbarImpl::WebScrollbarImpl(WebScrollbarClient* client, Orientation orientation)
    : m_client(client)
    , m_scrollOffset(0)
{
    m_scrollbar = Scrollbar::createNativeScrollbar(
        static_cast<ScrollableArea*>(this),
        static_cast<ScrollbarOrientation>(orientation),
        RegularScrollbar);
}

WebScrollbarImpl::~WebScrollbarImpl()
{
}

void WebScrollbarImpl::setLocation(const WebRect& rect)
{
    IntRect oldRect = m_scrollbar->frameRect();
    m_scrollbar->setFrameRect(rect);
    if (WebRect(oldRect) != rect)
      m_scrollbar->invalidate();

    int length = m_scrollbar->orientation() == HorizontalScrollbar ? m_scrollbar->width() : m_scrollbar->height();
    int pageStep = max(max(static_cast<int>(static_cast<float>(length) * Scrollbar::minFractionToStepWhenPaging()), length - Scrollbar::maxOverlapBetweenPages()), 1);
    m_scrollbar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
    m_scrollbar->setEnabled(m_scrollbar->totalSize() > length);
    m_scrollbar->setProportion(length, m_scrollbar->totalSize());
}

int WebScrollbarImpl::value() const
{
    return m_scrollOffset;
}

void WebScrollbarImpl::setValue(int position)
{
    ScrollableArea::scrollToOffsetWithoutAnimation(m_scrollbar->orientation(), static_cast<float>(position));
}

void WebScrollbarImpl::setDocumentSize(int size)
{
    int length = m_scrollbar->orientation() == HorizontalScrollbar ? m_scrollbar->width() : m_scrollbar->height();
    m_scrollbar->setEnabled(size > length);
    m_scrollbar->setProportion(length, size);
}

void WebScrollbarImpl::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    WebCore::ScrollDirection dir;
    bool horizontal = m_scrollbar->orientation() == HorizontalScrollbar;
    if (direction == ScrollForward)
        dir = horizontal ? ScrollRight : ScrollDown;
    else
        dir = horizontal ? ScrollLeft : ScrollUp;

    WebCore::ScrollableArea::scroll(dir, static_cast<WebCore::ScrollGranularity>(granularity), multiplier);
}

void WebScrollbarImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    m_scrollbar->paint(&GraphicsContextBuilder(canvas).context(), rect);
}

bool WebScrollbarImpl::handleInputEvent(const WebInputEvent& event)
{
    switch (event.type) {
    case WebInputEvent::MouseDown:
        return onMouseDown(event);
    case WebInputEvent::MouseUp:
        return onMouseUp(event);
    case WebInputEvent::MouseMove:
        return onMouseMove(event);
    case WebInputEvent::MouseLeave:
        return onMouseLeave(event);
    case WebInputEvent::MouseWheel:
        return onMouseWheel(event);
    case WebInputEvent::KeyDown:
        return onKeyDown(event);
    case WebInputEvent::Undefined:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyUp:
    case WebInputEvent::Char:
    case WebInputEvent::TouchStart:
    case WebInputEvent::TouchMove:
    case WebInputEvent::TouchEnd:
    case WebInputEvent::TouchCancel:
    default:
         break;
    }
    return false;
}

bool WebScrollbarImpl::onMouseDown(const WebInputEvent& event)
{
    WebMouseEvent mousedown = *static_cast<const WebMouseEvent*>(&event);
    if (!m_scrollbar->frameRect().contains(mousedown.x, mousedown.y))
        return false;

    mousedown.x -= m_scrollbar->x();
    mousedown.y -= m_scrollbar->y();
    m_scrollbar->mouseDown(PlatformMouseEventBuilder(m_scrollbar.get(), mousedown));
    return true;
}

bool WebScrollbarImpl::onMouseUp(const WebInputEvent& event)
{
    if (m_scrollbar->pressedPart() == NoPart)
        return false;

    return m_scrollbar->mouseUp();
}

bool WebScrollbarImpl::onMouseMove(const WebInputEvent& event)
{
    WebMouseEvent mousemove = *static_cast<const WebMouseEvent*>(&event);
    if (m_scrollbar->frameRect().contains(mousemove.x, mousemove.y)
        || m_scrollbar->pressedPart() != NoPart) {
        mousemove.x -= m_scrollbar->x();
        mousemove.y -= m_scrollbar->y();
        return m_scrollbar->mouseMoved(PlatformMouseEventBuilder(m_scrollbar.get(), mousemove));
    }

    if (m_scrollbar->hoveredPart() != NoPart)
        m_scrollbar->mouseExited();
    return false;
}

bool WebScrollbarImpl::onMouseLeave(const WebInputEvent& event)
{
    if (m_scrollbar->hoveredPart() == NoPart)
        return false;

    return m_scrollbar->mouseExited();
}

bool WebScrollbarImpl::onMouseWheel(const WebInputEvent& event)
{
    // Same logic as in Scrollview.cpp.  If we can move at all, we'll accept the event.
    WebMouseWheelEvent mousewheel = *static_cast<const WebMouseWheelEvent*>(&event);
    int maxScrollDelta = m_scrollbar->maximum() - m_scrollbar->value();
    float delta = m_scrollbar->orientation() == HorizontalScrollbar ? mousewheel.deltaX : mousewheel.deltaY;
    if ((delta < 0 && maxScrollDelta > 0) || (delta > 0 && m_scrollbar->value() > 0)) {
        if (mousewheel.scrollByPage) {
            ASSERT(m_scrollbar->orientation() == VerticalScrollbar);
            bool negative = delta < 0;
            delta = max(max(static_cast<float>(m_scrollbar->visibleSize()) * Scrollbar::minFractionToStepWhenPaging(), static_cast<float>(m_scrollbar->visibleSize() - Scrollbar::maxOverlapBetweenPages())), 1.0f);
            if (negative)
                delta *= -1;
        }
        ScrollableArea::scroll((m_scrollbar->orientation() == HorizontalScrollbar) ? WebCore::ScrollLeft : WebCore::ScrollUp, WebCore::ScrollByPixel, delta);
        return true;
    }

    return false;
    }

bool WebScrollbarImpl::onKeyDown(const WebInputEvent& event)
{
    WebKeyboardEvent keyboard = *static_cast<const WebKeyboardEvent*>(&event);
    int keyCode;
    // We have to duplicate this logic from WebViewImpl because there it uses
    // Char and RawKeyDown events, which don't exist at this point.
    if (keyboard.windowsKeyCode == VKEY_SPACE)
        keyCode = ((keyboard.modifiers & WebInputEvent::ShiftKey) ? VKEY_PRIOR : VKEY_NEXT);
    else {
        if (keyboard.modifiers == WebInputEvent::ControlKey) {
            // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
            // key combinations which affect scrolling. Safari is buggy in the
            // sense that it scrolls the page for all Ctrl+scrolling key
            // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
            switch (keyboard.windowsKeyCode) {
            case VKEY_HOME:
            case VKEY_END:
                break;
            default:
                return false;
            }
        }

        if (keyboard.isSystemKey || (keyboard.modifiers & WebInputEvent::ShiftKey))
            return false;

        keyCode = keyboard.windowsKeyCode;
    }
    WebCore::ScrollDirection scrollDirection;
    WebCore::ScrollGranularity scrollGranularity;
    if (WebViewImpl::mapKeyCodeForScroll(keyCode, &scrollDirection, &scrollGranularity)) {
        // Will return false if scroll direction wasn't compatible with this scrollbar.
        return ScrollableArea::scroll(scrollDirection, scrollGranularity);
    }
    return false;
}

int WebScrollbarImpl::scrollSize(WebCore::ScrollbarOrientation orientation) const
{
    return (orientation == m_scrollbar->orientation()) ? (m_scrollbar->totalSize() - m_scrollbar->visibleSize()) : 0;
}

int WebScrollbarImpl::scrollPosition(Scrollbar*) const
{
    return m_scrollOffset;
}

void WebScrollbarImpl::setScrollOffset(const IntPoint& offset)
{
    if (m_scrollbar->orientation() == HorizontalScrollbar)
        m_scrollOffset = offset.x();
    else
        m_scrollOffset = offset.y();

    m_client->valueChanged(this);
}

void WebScrollbarImpl::invalidateScrollbarRect(Scrollbar*, const IntRect& rect)
{
    WebRect webrect(rect);
    webrect.x += m_scrollbar->x();
    webrect.y += m_scrollbar->y();
    m_client->invalidateScrollbarRect(this, webrect);
}

void WebScrollbarImpl::invalidateScrollCornerRect(const IntRect&)
{
}

bool WebScrollbarImpl::isActive() const
{
    return true;
}

bool WebScrollbarImpl::isScrollCornerVisible() const
{
    return false;
}

void WebScrollbarImpl::getTickmarks(Vector<IntRect>& tickmarks) const
{
    WebVector<WebRect> ticks;
    m_client->getTickmarks(const_cast<WebScrollbarImpl*>(this), &ticks);
    tickmarks.resize(ticks.size());
    for (size_t i = 0; i < ticks.size(); ++i)
        tickmarks[i] = ticks[i];
}

Scrollbar* WebScrollbarImpl::horizontalScrollbar() const
{
    return m_scrollbar->orientation() == HorizontalScrollbar ? m_scrollbar.get() : 0;
}

Scrollbar* WebScrollbarImpl::verticalScrollbar() const
{
    return m_scrollbar->orientation() == VerticalScrollbar ? m_scrollbar.get() : 0;
}

} // namespace WebKit
