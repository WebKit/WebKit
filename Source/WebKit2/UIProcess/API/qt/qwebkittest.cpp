/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "qwebkittest_p.h"

#include "QtViewportHandler.h"
#include "qquickwebview_p_p.h"
#include <QMutableListIterator>
#include <QTouchEvent>
#include <qpa/qwindowsysteminterface.h>

using namespace WebKit;

QWebKitTest::QWebKitTest(QQuickWebViewPrivate* webViewPrivate, QObject* parent)
    : QObject(parent)
    , m_webViewPrivate(webViewPrivate)
{
}

QWebKitTest::~QWebKitTest()
{
}

static QTouchEvent::TouchPoint touchPoint(qreal x, qreal y)
{
    QPointF localPos(x, y);

    QTouchEvent::TouchPoint point;
    point.setId(1);
    point.setLastPos(localPos);
    QRectF touchRect(0, 0, 40, 40);
    touchRect.moveCenter(localPos);
    point.setRect(touchRect);
    point.setPressure(1);

    return point;
}

bool QWebKitTest::sendTouchEvent(QQuickWebView* window, QEvent::Type type, const QList<QTouchEvent::TouchPoint>& points, ulong timestamp)
{
    ASSERT(window);

    static QTouchDevice* device = 0;
    if (!device) {
        device = new QTouchDevice;
        device->setType(QTouchDevice::TouchScreen);
        QWindowSystemInterface::registerTouchDevice(device);
    }

    Qt::TouchPointStates touchPointStates = 0;
    foreach (const QTouchEvent::TouchPoint& touchPoint, points)
        touchPointStates |= touchPoint.state();

    QTouchEvent event(type, device, Qt::NoModifier, touchPointStates, points);
    event.setTimestamp(timestamp);
    event.setAccepted(false);

    window->touchEvent(&event);

    return event.isAccepted();
}

bool QWebKitTest::touchTap(QObject* item, qreal x, qreal y, int delay)
{
    QQuickWebView* window = qobject_cast<QQuickWebView*>(item);

    if (!window) {
        qWarning("Touch event \"TouchBegin\" not accepted by receiving item");
        return false;
    }

    // FIXME: implement delay using QTest::qWait() or similar.
    Q_UNUSED(delay);

    QList<QTouchEvent::TouchPoint> points;
    points.append(touchPoint(x, y));

    points[0].setState(Qt::TouchPointPressed);
    sendTouchEvent(window, QEvent::TouchBegin, points, QDateTime::currentMSecsSinceEpoch());

    points[0].setState(Qt::TouchPointReleased);
    sendTouchEvent(window, QEvent::TouchEnd, points, QDateTime::currentMSecsSinceEpoch());

    return true;
}

bool QWebKitTest::touchDoubleTap(QObject* item, qreal x, qreal y, int delay)
{
    if (!touchTap(item, x, y, delay))
        return false;

    if (!touchTap(item, x, y, delay))
        return false;

    return true;
}

QSize QWebKitTest::contentsSize() const
{
    return QSize(m_webViewPrivate->pageView->contentsSize().toSize());
}

QVariant QWebKitTest::contentsScale() const
{
    if (QtViewportHandler* viewport = m_webViewPrivate->viewportHandler())
        return viewport->currentCSSScale();
    return 1.0;
}

QVariant QWebKitTest::devicePixelRatio() const
{
    if (QtViewportHandler* viewport = m_webViewPrivate->viewportHandler())
        return viewport->m_devicePixelRatio;
    return 1.0;
}

QVariant QWebKitTest::initialScale() const
{
    if (QtViewportHandler* viewport = m_webViewPrivate->viewportHandler())
        return viewport->m_rawAttributes.initialScale;
    return 1.0;
}

QVariant QWebKitTest::minimumScale() const
{
    if (QtViewportHandler* viewport = m_webViewPrivate->viewportHandler())
        return viewport->m_minimumScale;
    return 1.0;
}

QVariant QWebKitTest::maximumScale() const
{
    if (QtViewportHandler* viewport = m_webViewPrivate->viewportHandler())
        return viewport->m_maximumScale;
    return 1.0;
}

QVariant QWebKitTest::isScalable() const
{
    if (QtViewportHandler* viewport = m_webViewPrivate->viewportHandler())
        return !!viewport->m_rawAttributes.userScalable;
    return false;
}

QVariant QWebKitTest::layoutSize() const
{
    if (QtViewportHandler* viewport = m_webViewPrivate->viewportHandler())
        return QSizeF(viewport->m_rawAttributes.layoutSize);
    return QSizeF();
}
