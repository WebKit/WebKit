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

#include "config.h"
#include "ScrollbarGroup.h"

#include "FrameView.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "WebPluginScrollbarImpl.h"
#include <public/WebRect.h>

using namespace WebCore;

namespace WebKit {

ScrollbarGroup::ScrollbarGroup(FrameView* frameView, const IntRect& frameRect)
    : m_frameView(frameView)
    , m_frameRect(frameRect)
    , m_horizontalScrollbar(0)
    , m_verticalScrollbar(0)
{
}

ScrollbarGroup::~ScrollbarGroup()
{
    ASSERT(!m_horizontalScrollbar);
    ASSERT(!m_verticalScrollbar);
}

void ScrollbarGroup::scrollbarCreated(WebPluginScrollbarImpl* scrollbar)
{
    bool hadScrollbars = m_horizontalScrollbar || m_verticalScrollbar;
    if (scrollbar->scrollbar()->orientation() == HorizontalScrollbar) {
        ASSERT(!m_horizontalScrollbar);
        m_horizontalScrollbar = scrollbar;
        didAddHorizontalScrollbar(scrollbar->scrollbar());
    } else {
        ASSERT(!m_verticalScrollbar);
        m_verticalScrollbar = scrollbar;
        didAddVerticalScrollbar(scrollbar->scrollbar());
    }

    if (!hadScrollbars) {
        m_frameView->addScrollableArea(this);
        m_frameView->setNeedsLayout();
    }
}

void ScrollbarGroup::scrollbarDestroyed(WebPluginScrollbarImpl* scrollbar)
{
    if (scrollbar == m_horizontalScrollbar) {
        willRemoveHorizontalScrollbar(scrollbar->scrollbar());
        m_horizontalScrollbar = 0;
    } else {
        ASSERT(scrollbar == m_verticalScrollbar);
        willRemoveVerticalScrollbar(scrollbar->scrollbar());
        m_verticalScrollbar = 0;
    }

    if (!m_horizontalScrollbar && !m_verticalScrollbar) {
        m_frameView->removeScrollableArea(this);
        m_frameView->setNeedsLayout();
    }
}

void ScrollbarGroup::setLastMousePosition(const IntPoint& point)
{
    m_lastMousePosition = point;
}

int ScrollbarGroup::scrollSize(WebCore::ScrollbarOrientation orientation) const
{
    WebPluginScrollbarImpl* webScrollbar = orientation == HorizontalScrollbar ? m_horizontalScrollbar : m_verticalScrollbar;
    if (!webScrollbar)
        return 0;
    Scrollbar* scrollbar = webScrollbar->scrollbar();
    return scrollbar->totalSize() - scrollbar->visibleSize();
}

int ScrollbarGroup::scrollPosition(Scrollbar* scrollbar) const
{
    WebPluginScrollbarImpl* webScrollbar = scrollbar->orientation() == HorizontalScrollbar ? m_horizontalScrollbar : m_verticalScrollbar;
    if (!webScrollbar)
        return 0;
    return webScrollbar->scrollOffset();
}

void ScrollbarGroup::setScrollOffset(const IntPoint& offset)
{
    if (m_horizontalScrollbar && m_horizontalScrollbar->scrollOffset() != offset.x())
        m_horizontalScrollbar->setScrollOffset(offset.x());
    else if (m_verticalScrollbar && m_verticalScrollbar->scrollOffset() != offset.y())
        m_verticalScrollbar->setScrollOffset(offset.y());
}

void ScrollbarGroup::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    if (m_horizontalScrollbar && scrollbar == m_horizontalScrollbar->scrollbar())
        m_horizontalScrollbar->invalidateScrollbarRect(rect);
    else if (m_verticalScrollbar && scrollbar == m_verticalScrollbar->scrollbar())
        m_verticalScrollbar->invalidateScrollbarRect(rect);
}

void ScrollbarGroup::invalidateScrollCornerRect(const IntRect&)
{
}

bool ScrollbarGroup::isActive() const
{
    return true;
}

ScrollableArea* ScrollbarGroup::enclosingScrollableArea() const
{
    // FIXME: Return a parent scrollable area that can be scrolled.
    return 0;
}

void ScrollbarGroup::setFrameRect(const IntRect& frameRect)
{
    m_frameRect = frameRect;
}

IntRect ScrollbarGroup::scrollableAreaBoundingBox() const
{
    return m_frameRect;
}

bool ScrollbarGroup::isScrollCornerVisible() const
{
    return false;
}

void ScrollbarGroup::getTickmarks(Vector<IntRect>& tickmarks) const
{
    if (m_verticalScrollbar)
        m_verticalScrollbar->getTickmarks(tickmarks);
}

IntPoint ScrollbarGroup::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    if (m_horizontalScrollbar && scrollbar == m_horizontalScrollbar->scrollbar())
        return m_horizontalScrollbar->convertFromContainingViewToScrollbar(parentPoint);
    if (m_verticalScrollbar && scrollbar == m_verticalScrollbar->scrollbar())
        return m_verticalScrollbar->convertFromContainingViewToScrollbar(parentPoint);
    WEBKIT_ASSERT_NOT_REACHED();
    return IntPoint();
}

Scrollbar* ScrollbarGroup::horizontalScrollbar() const
{
    return m_horizontalScrollbar ? m_horizontalScrollbar->scrollbar() : 0;
}

Scrollbar* ScrollbarGroup::verticalScrollbar() const
{
    return m_verticalScrollbar ? m_verticalScrollbar->scrollbar() : 0;
}

IntPoint ScrollbarGroup::scrollPosition() const
{
    int x = m_horizontalScrollbar ? m_horizontalScrollbar->scrollOffset() : 0;
    int y = m_verticalScrollbar ? m_verticalScrollbar->scrollOffset() : 0;
    return IntPoint(x, y);
}

IntPoint ScrollbarGroup::minimumScrollPosition() const
{
    return IntPoint();
}

IntPoint ScrollbarGroup::maximumScrollPosition() const
{
    return IntPoint(contentsSize().width() - visibleWidth(), contentsSize().height() - visibleHeight());
}

int ScrollbarGroup::visibleHeight() const
{
    if (m_verticalScrollbar)
        return m_verticalScrollbar->scrollbar()->height();
    if (m_horizontalScrollbar)
        return m_horizontalScrollbar->scrollbar()->height();
    WEBKIT_ASSERT_NOT_REACHED();
    return 0;
}

int ScrollbarGroup::visibleWidth() const
{
    if (m_horizontalScrollbar)
        return m_horizontalScrollbar->scrollbar()->width();
    if (m_verticalScrollbar)
        return m_verticalScrollbar->scrollbar()->width();
    WEBKIT_ASSERT_NOT_REACHED();
    return 0;
}

IntSize ScrollbarGroup::contentsSize() const
{
    IntSize size;
    if (m_horizontalScrollbar)
        size.setWidth(m_horizontalScrollbar->scrollbar()->totalSize());
    else if (m_verticalScrollbar) {
        size.setWidth(m_verticalScrollbar->scrollbar()->x());
        if (m_verticalScrollbar->scrollbar()->isOverlayScrollbar())
            size.expand(WebPluginScrollbar::defaultThickness(), 0);
    }
    if (m_verticalScrollbar)
        size.setHeight(m_verticalScrollbar->scrollbar()->totalSize());
    else if (m_horizontalScrollbar) {
        size.setHeight(m_horizontalScrollbar->scrollbar()->y());
        if (m_horizontalScrollbar->scrollbar()->isOverlayScrollbar())
            size.expand(0, WebPluginScrollbar::defaultThickness());
    }
    return size;
}

IntSize ScrollbarGroup::overhangAmount() const
{
    return IntSize();
}

IntPoint ScrollbarGroup::lastKnownMousePosition() const
{
    return m_lastMousePosition;
}

bool ScrollbarGroup::shouldSuspendScrollAnimations() const
{
    return false;
}

void ScrollbarGroup::scrollbarStyleChanged(int, bool forceUpdate)
{
    if (!forceUpdate)
        return;

    if (m_horizontalScrollbar)
        m_horizontalScrollbar->scrollbarStyleChanged();
    if (m_verticalScrollbar)
        m_verticalScrollbar->scrollbarStyleChanged();
}

bool ScrollbarGroup::scrollbarsCanBeActive() const
{
    return true;
}

} // namespace WebKit
