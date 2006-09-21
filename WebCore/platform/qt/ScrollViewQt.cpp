/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollView.h"
#include "FloatRect.h"
#include "IntPoint.h"

#include "FrameQt.h"
#include "ScrollViewCanvasQt.h"

#include <QScrollBar>
#include <QScrollArea>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

ScrollView::ScrollView()
    : m_area(new QScrollArea(0))
{
}

ScrollView::~ScrollView()
{
}

void ScrollView::setParentWidget(QWidget* parent)
{
    Widget::setParentWidget(parent);

    // 'isFrameView()' can not be called yet in the constructor!
    if (isFrameView()) {
        ScrollViewCanvasQt* canvas = new ScrollViewCanvasQt(this, m_area);

        setQWidget(m_area);
        m_area->setWidget(canvas);
    }
}

void ScrollView::updateContents(const IntRect& updateRect, bool now)
{
    // FIXME: We don't force updating with now=true here...
    if (m_area->widget())
        m_area->widget()->update(updateRect);
}

int ScrollView::visibleWidth() const
{
    return m_area->maximumViewportSize().width();
}

int ScrollView::visibleHeight() const
{
    return m_area->maximumViewportSize().height();
}

FloatRect ScrollView::visibleContentRect() const
{
    const QSize s(m_area->maximumViewportSize());

    return FloatRect(m_area->horizontalScrollBar()->value(),
                     m_area->verticalScrollBar()->value(),
                     s.width(), s.height());
}

void ScrollView::setContentsPos(int newX, int newY)
{
    m_area->horizontalScrollBar()->setValue(newX);
    m_area->verticalScrollBar()->setValue(newY);
}

void ScrollView::resizeContents(int w, int h)
{
    if (m_area->widget())
        m_area->widget()->resize(w, h);
}

int ScrollView::contentsX() const
{
    return m_area->horizontalScrollBar()->value();
}

int ScrollView::contentsY() const
{
    return m_area->verticalScrollBar()->value();
}

int ScrollView::contentsWidth() const
{
    if (!m_area->widget())
        return 0;

    return m_area->widget()->width();
}

int ScrollView::contentsHeight() const
{
    if (!m_area->widget())
        return 0;

    return m_area->widget()->height();
}

IntPoint ScrollView::viewportToContents(const IntPoint& viewportPoint)
{
    // Qt already handles this in the coordinates we get from the mouse events...
    return viewportPoint;
}

IntPoint ScrollView::contentsToViewport(const IntPoint& contentsPoint)
{
    // Qt already handles this in the coordinates we get from the mouse events...
    return contentsPoint;
}

IntSize ScrollView::scrollOffset() const
{
    return IntSize(m_area->horizontalScrollBar()->value(), m_area->verticalScrollBar()->value());
}

void ScrollView::scrollBy(int dx, int dy)
{
    m_area->horizontalScrollBar()->setValue(m_area->horizontalScrollBar()->value() + dx);
    m_area->verticalScrollBar()->setValue(m_area->verticalScrollBar()->value() + dy);
}

ScrollBarMode ScrollView::hScrollBarMode() const
{
    switch (m_area->horizontalScrollBarPolicy())
    {
        case Qt::ScrollBarAsNeeded:
            return ScrollBarAuto;
        case Qt::ScrollBarAlwaysOff:
            return ScrollBarAlwaysOff;
        case Qt::ScrollBarAlwaysOn:
            return ScrollBarAlwaysOn;
    }

    return ScrollBarAuto;
}

ScrollBarMode ScrollView::vScrollBarMode() const
{
    switch (m_area->verticalScrollBarPolicy())
    {
        case Qt::ScrollBarAsNeeded:
            return ScrollBarAuto;
        case Qt::ScrollBarAlwaysOff:
            return ScrollBarAlwaysOff;
        case Qt::ScrollBarAlwaysOn:
            return ScrollBarAlwaysOn;
    }

    return ScrollBarAuto;
}

void ScrollView::suppressScrollBars(bool suppressed, bool /* repaintOnSuppress */)
{
    setScrollBarsMode(suppressed ? ScrollBarAlwaysOff : ScrollBarAuto);
}

void ScrollView::setHScrollBarMode(ScrollBarMode newMode)
{
    switch (newMode)
    {
        case ScrollBarAuto:
            m_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            break;
        case ScrollBarAlwaysOff:
            m_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            break;
        case ScrollBarAlwaysOn:
            m_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            break;
    }
}

void ScrollView::setVScrollBarMode(ScrollBarMode newMode)
{
    switch (newMode)
    {
        case ScrollBarAuto:
            m_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            break;
        case ScrollBarAlwaysOff:
            m_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            break;
        case ScrollBarAlwaysOn:
            m_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            break;
    }
}

void ScrollView::setScrollBarsMode(ScrollBarMode newMode)
{
    setHScrollBarMode(newMode);
    setVScrollBarMode(newMode);
}

void ScrollView::setStaticBackground(bool flag)
{
    // no-op
}

void ScrollView::addChild(Widget* child, int x, int y)
{
    Q_ASSERT(child != 0);
    Q_ASSERT(m_area && m_area->widget());

    // Ignore offscreen position initialization
    if (x != -500000)
        child->move(x, y);

    child->setParentWidget(m_area->widget());
}

void ScrollView::removeChild(Widget*)
{ 
    // no-op
}

void ScrollView::scrollPointRecursively(int x, int y)
{ 
    notImplemented();
}

bool ScrollView::inWindow() const
{
    return true;
}

void ScrollView::wheelEvent(PlatformWheelEvent&)
{
    notImplemented();
}

}

// vim: ts=4 sw=4 et
