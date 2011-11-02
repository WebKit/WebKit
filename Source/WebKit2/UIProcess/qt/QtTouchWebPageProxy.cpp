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
#include "QtTouchWebPageProxy.h"

#include "DrawingAreaProxyImpl.h"
#include <IntRect.h>
#include <NativeWebTouchEvent.h>
#include <WebEventFactoryQt.h>
#include <qwebpreferences_p.h>

using namespace WebCore;

QtTouchWebPageProxy::QtTouchWebPageProxy(QtTouchViewInterface* viewInterface, QtViewportInteractionEngine* viewportInteractionEngine)
    : QtWebPageProxy(viewInterface, 0)
    , m_panGestureRecognizer(viewportInteractionEngine)
    , m_pinchGestureRecognizer(viewportInteractionEngine)
{
    QWebPreferencesPrivate::get(preferences())->setAttribute(QWebPreferencesPrivate::AcceleratedCompositingEnabled, true);
    init();
}

PassOwnPtr<DrawingAreaProxy> QtTouchWebPageProxy::createDrawingAreaProxy()
{
    return DrawingAreaProxyImpl::create(m_webPageProxy.get());
}

void QtTouchWebPageProxy::processDidCrash()
{
    QtWebPageProxy::processDidCrash();
    m_panGestureRecognizer.reset();
    m_pinchGestureRecognizer.reset();
}

void QtTouchWebPageProxy::paintContent(QPainter* painter, const QRect& area)
{
}

void QtTouchWebPageProxy::renderToCurrentGLContext(const TransformationMatrix& transform, float opacity)
{
    DrawingAreaProxy* drawingArea = m_webPageProxy->drawingArea();
    if (drawingArea)
        drawingArea->paintToCurrentGLContext(transform, opacity);
}

#if ENABLE(TOUCH_EVENTS)
void QtTouchWebPageProxy::doneWithTouchEvent(const NativeWebTouchEvent& event, bool wasEventHandled)
{
    if (wasEventHandled || event.type() == WebEvent::TouchCancel) {
        m_panGestureRecognizer.reset();
        m_pinchGestureRecognizer.reset();
    } else {
        // Convert the event timestamp from second to millisecond.
        qint64 eventTimestampMillis = static_cast<qint64>(event.timestamp() * 1000);
        m_panGestureRecognizer.recognize(event.nativeEvent(), eventTimestampMillis);
        m_pinchGestureRecognizer.recognize(event.nativeEvent());
    }
}
#endif

bool QtTouchWebPageProxy::handleEvent(QEvent* ev)
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

void QtTouchWebPageProxy::setVisibleContentRectAndScale(const QRectF& visibleContentRect, float scale)
{
    QRect alignedVisibleContentRect = visibleContentRect.toAlignedRect();
    m_webPageProxy->drawingArea()->setVisibleContentsRectAndScale(alignedVisibleContentRect, scale);

    // FIXME: Once we support suspend and resume, this should be delayed until the page is active if the page is suspended.
    m_webPageProxy->setFixedVisibleContentRect(alignedVisibleContentRect);
}

void QtTouchWebPageProxy::setVisibleContentRectTrajectoryVector(const QPointF& trajectoryVector)
{
    m_webPageProxy->drawingArea()->setVisibleContentRectTrajectoryVector(trajectoryVector);
}

void QtTouchWebPageProxy::setResizesToContentsUsingLayoutSize(const QSize& targetLayoutSize)
{
    m_webPageProxy->setResizesToContentsUsingLayoutSize(targetLayoutSize);
}

void QtTouchWebPageProxy::touchEvent(QTouchEvent* event)
{
#if ENABLE(TOUCH_EVENTS)
    m_webPageProxy->handleTouchEvent(NativeWebTouchEvent(event));
    event->accept();
#else
    ASSERT_NOT_REACHED();
    ev->ignore();
#endif
}

void QtTouchWebPageProxy::findZoomableAreaForPoint(const QPoint& point)
{
    m_webPageProxy->findZoomableAreaForPoint(point);
}

void QtTouchWebPageProxy::renderNextFrame()
{
}
