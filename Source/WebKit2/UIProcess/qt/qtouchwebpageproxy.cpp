/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "qtouchwebpageproxy.h"

#include <IntRect.h>
#include <NativeWebTouchEvent.h>
#include <WebEventFactoryQt.h>

using namespace WebCore;

QTouchWebPageProxy::QTouchWebPageProxy(TouchViewInterface* viewInterface, ViewportInteractionEngine* viewportInteractionEngine)
    : QtWebPageProxy(viewInterface, 0)
    , m_panGestureRecognizer(viewportInteractionEngine)
    , m_pinchGestureRecognizer(viewportInteractionEngine)
{
    init();
}

PassOwnPtr<DrawingAreaProxy> QTouchWebPageProxy::createDrawingAreaProxy()
{
    return TiledDrawingAreaProxy::create(touchViewInterface(), m_webPageProxy.get());
}

void QTouchWebPageProxy::processDidCrash()
{
    QtWebPageProxy::processDidCrash();
    m_panGestureRecognizer.reset();
    m_pinchGestureRecognizer.reset();
}

void QTouchWebPageProxy::paintContent(QPainter* painter, const QRect& area)
{
    m_webPageProxy->drawingArea()->paint(IntRect(area), painter);
}

#if ENABLE(TOUCH_EVENTS)
void QTouchWebPageProxy::doneWithTouchEvent(const NativeWebTouchEvent& event, bool wasEventHandled)
{
    if (wasEventHandled) {
        m_panGestureRecognizer.reset();
        m_pinchGestureRecognizer.reset();
    } else {
        m_panGestureRecognizer.recognize(event.nativeEvent());
        m_pinchGestureRecognizer.recognize(event.nativeEvent());
    }
}
#endif

bool QTouchWebPageProxy::handleEvent(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchEnd:
    case QEvent::TouchUpdate:
        touchEvent(static_cast<QTouchEvent*>(ev));
        return true;
    }
    return QtWebPageProxy::handleEvent(ev);
}

void QTouchWebPageProxy::setVisibleContentRectAndScale(const QRectF& visibleContentRect, float scale)
{
    QRect alignedVisibleContentRect = visibleContentRect.toAlignedRect();
    drawingArea()->setVisibleContentRectAndScale(alignedVisibleContentRect, scale);

    // FIXME: Once we support suspend and resume, this should be delayed until the page is active if the page is suspended.
    m_webPageProxy->setFixedVisibleContentRect(alignedVisibleContentRect);
}

void QTouchWebPageProxy::setResizesToContentsUsingLayoutSize(const QSize& targetLayoutSize)
{
    m_webPageProxy->setResizesToContentsUsingLayoutSize(targetLayoutSize);
}

void QTouchWebPageProxy::touchEvent(QTouchEvent* event)
{
#if ENABLE(TOUCH_EVENTS)
    m_webPageProxy->handleTouchEvent(NativeWebTouchEvent(event));
    event->accept();
#else
    ASSERT_NOT_REACHED();
    ev->ignore();
#endif
}

void QTouchWebPageProxy::findZoomableAreaForPoint(const QPoint& point)
{
    m_webPageProxy->findZoomableAreaForPoint(point);
}

void QTouchWebPageProxy::renderNextFrame()
{
    drawingArea()->renderNextFrame();
}
