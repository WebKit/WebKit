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

#include "QtViewportInteractionEngine.h"
#include "QtWebPageEventHandler.h"
#include "qquickwebview_p.h"
#include "qquickwebview_p_p.h"

using namespace WebKit;

QWebKitTest::QWebKitTest(QQuickWebViewPrivate* webViewPrivate, QObject* parent)
    : QObject(parent)
    , m_webViewPrivate(webViewPrivate)
{
}

QWebKitTest::~QWebKitTest()
{
}

bool QWebKitTest::touchDoubleTap(QObject* item, qreal x, qreal y, int delay)
{
    if (!qobject_cast<QQuickWebView*>(item)) {
        // FIXME: We only support the actual web view for now.
        qWarning("Touch event \"DoubleTap\" not accepted by receiving item");
        return false;
    }

    // FIXME: implement delay using QTest::qWait() or similar.
    Q_UNUSED(delay);

    QPointF localPos(x, y);

    QTouchEvent::TouchPoint point;
    point.setLastPos(localPos);
    QRectF touchRect(0, 0, 40, 40);
    touchRect.moveCenter(localPos);
    point.setRect(touchRect);

    m_webViewPrivate->pageView->eventHandler()->handleDoubleTapEvent(point);

    return true;
}

QSize QWebKitTest::contentsSize() const
{
    return QSize(m_webViewPrivate->pageView->contentsSize().toSize());
}

QVariant QWebKitTest::contentsScale() const
{
    if (QtViewportInteractionEngine* interactionEngine = m_webViewPrivate->viewportInteractionEngine())
        return interactionEngine->currentCSSScale();

    return m_webViewPrivate->attributes.initialScale;
}

QVariant QWebKitTest::devicePixelRatio() const
{
    return m_webViewPrivate->attributes.devicePixelRatio;
}

QVariant QWebKitTest::initialScale() const
{
    return m_webViewPrivate->attributes.initialScale;
}

QVariant QWebKitTest::minimumScale() const
{
    if (QtViewportInteractionEngine* interactionEngine = m_webViewPrivate->viewportInteractionEngine())
        return interactionEngine->m_minimumScale;

    return m_webViewPrivate->attributes.minimumScale;
}

QVariant QWebKitTest::maximumScale() const
{
    if (QtViewportInteractionEngine* interactionEngine = m_webViewPrivate->viewportInteractionEngine())
        return interactionEngine->m_maximumScale;

    return m_webViewPrivate->attributes.maximumScale;
}

QVariant QWebKitTest::isScalable() const
{
    return !!m_webViewPrivate->attributes.userScalable;
}

QVariant QWebKitTest::layoutSize() const
{
    return QSizeF(m_webViewPrivate->attributes.layoutSize.width(), m_webViewPrivate->attributes.layoutSize.height());
}
