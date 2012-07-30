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

#include "config.h"
#include "WebPluginScrollbarImpl.h"

#include "GraphicsContext.h"
#include "KeyboardCodes.h"
#include "ScrollAnimator.h"
#include "ScrollTypes.h"
#include "Scrollbar.h"
#include "ScrollbarGroup.h"
#include "ScrollbarTheme.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebPluginContainerImpl.h"
#include "WebPluginScrollbarClient.h"
#include "WebViewImpl.h"
#include "painting/GraphicsContextBuilder.h"
#include <public/WebCanvas.h>
#include <public/WebRect.h>
#include <public/WebVector.h>

using namespace std;
using namespace WebCore;

namespace WebKit {

WebPluginScrollbar* WebPluginScrollbar::createForPlugin(Orientation orientation,
                                                        WebPluginContainer* pluginContainer,
                                                        WebPluginScrollbarClient* client)
{
    WebPluginContainerImpl* plugin = static_cast<WebPluginContainerImpl*>(pluginContainer);
    return new WebPluginScrollbarImpl(orientation, plugin->scrollbarGroup(), client);
}

int WebPluginScrollbar::defaultThickness()
{
    return ScrollbarTheme::theme()->scrollbarThickness();
}

WebPluginScrollbarImpl::WebPluginScrollbarImpl(Orientation orientation,
                                   ScrollbarGroup* group,
                                   WebPluginScrollbarClient* client)
    : m_group(group)
    , m_client(client)
    , m_scrollOffset(0)
{
    m_scrollbar = Scrollbar::createNativeScrollbar(
        static_cast<ScrollableArea*>(m_group),
        static_cast<WebCore::ScrollbarOrientation>(orientation),
        WebCore::RegularScrollbar);
    m_group->scrollbarCreated(this);
}

WebPluginScrollbarImpl::~WebPluginScrollbarImpl()
{
    m_group->scrollbarDestroyed(this);
}

void WebPluginScrollbarImpl::setScrollOffset(int scrollOffset)
{
    m_scrollOffset = scrollOffset;
    m_client->valueChanged(this);
}

void WebPluginScrollbarImpl::invalidateScrollbarRect(const IntRect& rect)
{
    WebRect webrect(rect);
    webrect.x += m_scrollbar->x();
    webrect.y += m_scrollbar->y();
    m_client->invalidateScrollbarRect(this, webrect);
}

void WebPluginScrollbarImpl::getTickmarks(Vector<IntRect>& tickmarks) const
{
    WebVector<WebRect> ticks;
    m_client->getTickmarks(const_cast<WebPluginScrollbarImpl*>(this), &ticks);
    tickmarks.resize(ticks.size());
    for (size_t i = 0; i < ticks.size(); ++i)
        tickmarks[i] = ticks[i];
}

IntPoint WebPluginScrollbarImpl::convertFromContainingViewToScrollbar(const IntPoint& parentPoint) const
{
    IntPoint offset(parentPoint.x() - m_scrollbar->x(), parentPoint.y() - m_scrollbar->y());
    return m_scrollbar->Widget::convertFromContainingView(offset);
}

void WebPluginScrollbarImpl::scrollbarStyleChanged()
{
    m_client->overlayChanged(this);
}

bool WebPluginScrollbarImpl::isOverlay() const
{
    return m_scrollbar->isOverlayScrollbar();
}

int WebPluginScrollbarImpl::value() const
{
    return m_scrollOffset;
}

WebPoint WebPluginScrollbarImpl::location() const
{
    return m_scrollbar->frameRect().location();
}

WebSize WebPluginScrollbarImpl::size() const
{
    return m_scrollbar->frameRect().size();
}

bool WebPluginScrollbarImpl::enabled() const
{
    return m_scrollbar->enabled();
}

int WebPluginScrollbarImpl::maximum() const
{
    return m_scrollbar->maximum();
}

int WebPluginScrollbarImpl::totalSize() const
{
    return m_scrollbar->totalSize();
}

bool WebPluginScrollbarImpl::isScrollViewScrollbar() const
{
    return m_scrollbar->isScrollViewScrollbar();
}

bool WebPluginScrollbarImpl::isScrollableAreaActive() const
{
    return m_scrollbar->isScrollableAreaActive();
}

void WebPluginScrollbarImpl::getTickmarks(WebVector<WebRect>& tickmarks) const
{
    m_client->getTickmarks(const_cast<WebPluginScrollbarImpl*>(this), &tickmarks);
}

WebScrollbar::ScrollbarControlSize WebPluginScrollbarImpl::controlSize() const
{
    return static_cast<WebScrollbar::ScrollbarControlSize>(m_scrollbar->controlSize());
}

WebScrollbar::ScrollbarPart WebPluginScrollbarImpl::pressedPart() const
{
    return static_cast<WebScrollbar::ScrollbarPart>(m_scrollbar->pressedPart());
}

WebScrollbar::ScrollbarPart WebPluginScrollbarImpl::hoveredPart() const
{
    return static_cast<WebScrollbar::ScrollbarPart>(m_scrollbar->hoveredPart());
}

WebScrollbar::ScrollbarOverlayStyle WebPluginScrollbarImpl::scrollbarOverlayStyle() const
{
    return static_cast<WebScrollbar::ScrollbarOverlayStyle>(m_scrollbar->scrollbarOverlayStyle());
}

WebScrollbar::Orientation WebPluginScrollbarImpl::orientation() const
{
    if (m_scrollbar->orientation() == WebCore::HorizontalScrollbar)
        return WebScrollbar::Horizontal;
    return WebScrollbar::Vertical;
}

bool WebPluginScrollbarImpl::isCustomScrollbar() const
{
    return m_scrollbar->isCustomScrollbar();
}

void WebPluginScrollbarImpl::setLocation(const WebRect& rect)
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

void WebPluginScrollbarImpl::setValue(int position)
{
    m_group->scrollToOffsetWithoutAnimation(m_scrollbar->orientation(), static_cast<float>(position));
}

void WebPluginScrollbarImpl::setDocumentSize(int size)
{
    int length = m_scrollbar->orientation() == HorizontalScrollbar ? m_scrollbar->width() : m_scrollbar->height();
    m_scrollbar->setEnabled(size > length);
    m_scrollbar->setProportion(length, size);
}

void WebPluginScrollbarImpl::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    WebCore::ScrollDirection dir;
    bool horizontal = m_scrollbar->orientation() == HorizontalScrollbar;
    if (direction == ScrollForward)
        dir = horizontal ? ScrollRight : ScrollDown;
    else
        dir = horizontal ? ScrollLeft : ScrollUp;

    m_group->scroll(dir, static_cast<WebCore::ScrollGranularity>(granularity), multiplier);
}

void WebPluginScrollbarImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    m_scrollbar->paint(&GraphicsContextBuilder(canvas).context(), rect);
}

bool WebPluginScrollbarImpl::handleInputEvent(const WebInputEvent& event)
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

bool WebPluginScrollbarImpl::onMouseDown(const WebInputEvent& event)
{
    WebMouseEvent mousedown = *static_cast<const WebMouseEvent*>(&event);
    if (!m_scrollbar->frameRect().contains(mousedown.x, mousedown.y))
        return false;

    mousedown.x -= m_scrollbar->x();
    mousedown.y -= m_scrollbar->y();
    m_scrollbar->mouseDown(PlatformMouseEventBuilder(m_scrollbar.get(), mousedown));
    return true;
}

bool WebPluginScrollbarImpl::onMouseUp(const WebInputEvent& event)
{
    WebMouseEvent mouseup = *static_cast<const WebMouseEvent*>(&event);
    if (m_scrollbar->pressedPart() == WebCore::NoPart)
        return false;

    return m_scrollbar->mouseUp(PlatformMouseEventBuilder(m_scrollbar.get(), mouseup));
}

bool WebPluginScrollbarImpl::onMouseMove(const WebInputEvent& event)
{
    WebMouseEvent mousemove = *static_cast<const WebMouseEvent*>(&event);
    if (m_scrollbar->frameRect().contains(mousemove.x, mousemove.y)
        || m_scrollbar->pressedPart() != WebCore::NoPart) {
        mousemove.x -= m_scrollbar->x();
        mousemove.y -= m_scrollbar->y();
        return m_scrollbar->mouseMoved(PlatformMouseEventBuilder(m_scrollbar.get(), mousemove));
    }

    if (m_scrollbar->hoveredPart() != WebCore::NoPart && !m_scrollbar->isOverlayScrollbar())
        m_scrollbar->mouseExited();
    return false;
}

bool WebPluginScrollbarImpl::onMouseLeave(const WebInputEvent& event)
{
    if (m_scrollbar->hoveredPart() != WebCore::NoPart)
        m_scrollbar->mouseExited();

    return false;
}

bool WebPluginScrollbarImpl::onMouseWheel(const WebInputEvent& event)
{
    WebMouseWheelEvent mousewheel = *static_cast<const WebMouseWheelEvent*>(&event);
    PlatformWheelEventBuilder platformEvent(m_scrollbar.get(), mousewheel);
    return m_group->handleWheelEvent(platformEvent);
}

bool WebPluginScrollbarImpl::onKeyDown(const WebInputEvent& event)
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
        return m_group->scroll(scrollDirection, scrollGranularity);
    }
    return false;
}

} // namespace WebKit
