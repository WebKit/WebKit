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

#include "config.h"

#include "WebScrollbarThemeClientImpl.h"

using namespace WebCore;

namespace WebKit {

WebScrollbarThemeClientImpl::WebScrollbarThemeClientImpl(WebScrollbar* scrollbar)
    : m_scrollbar(scrollbar)
{
}

int WebScrollbarThemeClientImpl::x() const
{
    return location().x();
}

int WebScrollbarThemeClientImpl::y() const
{
    return location().y();
}

int WebScrollbarThemeClientImpl::width() const
{
    return size().width();
}

int WebScrollbarThemeClientImpl::height() const
{
    return size().height();
}

IntSize WebScrollbarThemeClientImpl::size() const
{
    return m_scrollbar->size();
}

IntPoint WebScrollbarThemeClientImpl::location() const
{
    return m_scrollbar->location();
}

ScrollView* WebScrollbarThemeClientImpl::parent() const
{
    // Unused by Chromium scrollbar themes.
    ASSERT_NOT_REACHED();
    return 0;
}

ScrollView* WebScrollbarThemeClientImpl::root() const
{
    // Unused by Chromium scrollbar themes.
    ASSERT_NOT_REACHED();
    return 0;
}

void WebScrollbarThemeClientImpl::setFrameRect(const IntRect&)
{
    // Unused by Chromium scrollbar themes.
    ASSERT_NOT_REACHED();
}

IntRect WebScrollbarThemeClientImpl::frameRect() const
{
    return IntRect(location(), size());
}

void WebScrollbarThemeClientImpl::invalidate()
{
    // Unused by Chromium scrollbar themes.
    ASSERT_NOT_REACHED();
}

void WebScrollbarThemeClientImpl::invalidateRect(const IntRect&)
{
    // Unused by Chromium scrollbar themes.
    ASSERT_NOT_REACHED();
}

WebCore::ScrollbarOverlayStyle WebScrollbarThemeClientImpl::scrollbarOverlayStyle() const
{
    return static_cast<WebCore::ScrollbarOverlayStyle>(m_scrollbar->scrollbarOverlayStyle());
}

void WebScrollbarThemeClientImpl::getTickmarks(Vector<IntRect>& tickmarks) const
{
    WebVector<WebRect> webTickmarks;
    m_scrollbar->getTickmarks(webTickmarks);
    tickmarks.resize(webTickmarks.size());
    for (size_t i = 0; i < webTickmarks.size(); ++i)
        tickmarks[i] = webTickmarks[i];
}

bool WebScrollbarThemeClientImpl::isScrollableAreaActive() const
{
    return m_scrollbar->isScrollableAreaActive();
}

bool WebScrollbarThemeClientImpl::isScrollViewScrollbar() const
{
    // Unused by Chromium scrollbar themes.
    ASSERT_NOT_REACHED();
    return false;
}

IntPoint WebScrollbarThemeClientImpl::convertFromContainingWindow(const IntPoint& windowPoint)
{
    // Unused by Chromium scrollbar themes.
    ASSERT_NOT_REACHED();
    return windowPoint;
}

bool WebScrollbarThemeClientImpl::isCustomScrollbar() const
{
    return m_scrollbar->isCustomScrollbar();
}

WebCore::ScrollbarOrientation WebScrollbarThemeClientImpl::orientation() const
{
    return static_cast<WebCore::ScrollbarOrientation>(m_scrollbar->orientation());
}

int WebScrollbarThemeClientImpl::value() const
{
    return m_scrollbar->value();
}

float WebScrollbarThemeClientImpl::currentPos() const
{
    return value();
}

int WebScrollbarThemeClientImpl::visibleSize() const
{
    return totalSize() - maximum();
}

int WebScrollbarThemeClientImpl::totalSize() const
{
    return m_scrollbar->totalSize();
}

int WebScrollbarThemeClientImpl::maximum() const
{
    return m_scrollbar->maximum();
}

WebCore::ScrollbarControlSize WebScrollbarThemeClientImpl::controlSize() const
{
    return static_cast<WebCore::ScrollbarControlSize>(m_scrollbar->controlSize());
}

int WebScrollbarThemeClientImpl::lineStep() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

int WebScrollbarThemeClientImpl::pageStep() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

WebCore::ScrollbarPart WebScrollbarThemeClientImpl::pressedPart() const
{
    return static_cast<WebCore::ScrollbarPart>(m_scrollbar->pressedPart());
}

WebCore::ScrollbarPart WebScrollbarThemeClientImpl::hoveredPart() const
{
    return static_cast<WebCore::ScrollbarPart>(m_scrollbar->hoveredPart());
}

void WebScrollbarThemeClientImpl::styleChanged()
{
    ASSERT_NOT_REACHED();
}

bool WebScrollbarThemeClientImpl::enabled() const
{
    return m_scrollbar->enabled();
}

void WebScrollbarThemeClientImpl::setEnabled(bool)
{
    ASSERT_NOT_REACHED();
}

bool WebScrollbarThemeClientImpl::isOverlayScrollbar() const
{
    return m_scrollbar->isOverlay();
}

bool WebScrollbarThemeClientImpl::isAlphaLocked() const
{
    return m_scrollbar->isAlphaLocked();
}

void WebScrollbarThemeClientImpl::setIsAlphaLocked(bool flag)
{
    m_scrollbar->setIsAlphaLocked(flag);
}

} // namespace WebKit
