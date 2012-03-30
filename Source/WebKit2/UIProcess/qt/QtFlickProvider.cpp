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
#include "QtFlickProvider.h"

#include "qquickwebpage_p.h"
#include "qquickwebview_p.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QPointF>
#include <QQuickCanvas>
#include <QQuickItem>
#include <QTouchEvent>
#include <QtQml/QQmlEngine>
#include <wtf/Assertions.h>

namespace {

inline QMetaMethod resolveMetaMethod(QObject* flickable, const char* name)
{
    ASSERT(flickable);

    const QMetaObject* metaObject = flickable->metaObject();
    ASSERT(metaObject);

    int methodIndex = metaObject->indexOfMethod(name);
    ASSERT(methodIndex != -1);

    return metaObject->method(methodIndex);
}

inline QMetaProperty resolveMetaProperty(QObject* flickable, const char* name)
{
    ASSERT(flickable);

    const QMetaObject* metaObject = flickable->metaObject();
    ASSERT(metaObject);

    int propertyIndex = metaObject->indexOfProperty(name);
    ASSERT(propertyIndex != -1);

    return metaObject->property(propertyIndex);
}

} // namespace

QtFlickProvider::QtFlickProvider(QQuickWebView* viewItem, QQuickWebPage* pageItem)
    : QObject(viewItem)
{
    ASSERT(viewItem);
    ASSERT(pageItem);

    QDeclarativeEngine* engine = qmlEngine(viewItem);
    QDeclarativeContext* context = qmlContext(viewItem);

    ASSERT(engine);
    ASSERT(context);

    QDeclarativeComponent component(engine, viewItem);

    // Create the internal Flickable instance dynamically.
    // We only use public QML API so that we do not depend
    // on private API of the QtDeclarative module.
    component.setData(QByteArrayLiteral("import QtQuick 2.0\nFlickable {}"), QUrl());

    m_flickable = qobject_cast<QQuickItem*>(component.create(context));

    QMetaProperty content = resolveMetaProperty(m_flickable, "contentItem");
    m_contentItem = content.read(m_flickable).value<QQuickItem*>();
    ASSERT(m_contentItem);

    // Resolve meta methods and properties of the Flickable instance.
    m_returnToBoundsMethod = resolveMetaMethod(m_flickable, "returnToBounds()");
    m_cancelFlickMethod = resolveMetaMethod(m_flickable, "cancelFlick()");

    m_contentWidth = resolveMetaProperty(m_flickable, "contentWidth");
    m_contentHeight = resolveMetaProperty(m_flickable, "contentHeight");
    m_contentX = resolveMetaProperty(m_flickable, "contentX");
    m_contentY = resolveMetaProperty(m_flickable, "contentY");
    m_moving = resolveMetaProperty(m_flickable, "moving");
    m_dragging = resolveMetaProperty(m_flickable, "dragging");
    m_flicking = resolveMetaProperty(m_flickable, "flicking");

    m_flickableData = resolveMetaProperty(m_flickable, "flickableData");

    // Set the viewItem as the parent of the flickable instance
    // and reparent the page so it is placed on the flickable contentItem.
    m_flickable->setParentItem(viewItem);
    pageItem->setParentItem(m_contentItem);

    // Propagate flickable signals.
    connect(m_flickable, SIGNAL(movementStarted()), SIGNAL(movementStarted()), Qt::DirectConnection);
    connect(m_flickable, SIGNAL(movementEnded()), SIGNAL(movementEnded()), Qt::DirectConnection);
    connect(m_flickable, SIGNAL(flickingChanged()), SIGNAL(flickingChanged()), Qt::DirectConnection);
    connect(m_flickable, SIGNAL(draggingChanged()), SIGNAL(draggingChanged()), Qt::DirectConnection);
    connect(m_flickable, SIGNAL(contentWidthChanged()), SIGNAL(contentWidthChanged()), Qt::DirectConnection);
    connect(m_flickable, SIGNAL(contentHeightChanged()), SIGNAL(contentHeightChanged()), Qt::DirectConnection);
    connect(m_flickable, SIGNAL(contentXChanged()), SIGNAL(contentXChanged()), Qt::DirectConnection);
    connect(m_flickable, SIGNAL(contentYChanged()), SIGNAL(contentYChanged()), Qt::DirectConnection);
}

void QtFlickProvider::handleTouchFlickEvent(QTouchEvent* touchEvent)
{
    // Since the Flickable does not handle touch events directly the sent
    // touch event would end up in the WebView again and would thus trigger
    // an infinite loop.
    // Hence do the touch to mouse event conversion for the Flickable here.
    QEvent::Type mouseEventType = QEvent::None;
    Qt::MouseButton mouseButton = Qt::NoButton;

    switch (touchEvent->type()) {
    case QEvent::TouchBegin:
        mouseEventType = QEvent::MouseButtonPress;

        // We need to set the mouse button so that the Flickable
        // item receives the initial mouse press event.
        mouseButton = Qt::LeftButton;
        break;
    case QEvent::TouchUpdate:
        mouseEventType = QEvent::MouseMove;
        break;
    case QEvent::TouchEnd:
        mouseEventType = QEvent::MouseButtonRelease;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    QPointF touchPosition = touchEvent->touchPoints().last().pos();
    QMouseEvent mouseEvent(mouseEventType, touchPosition, mouseButton, mouseButton, Qt::NoModifier);

    // Send the event to the canvas and let the canvas propagate it
    // to the Flickable. This makes sure that the Flickable grabs
    // the mouse so that it also receives the events of gestures
    // which started inside the viewport but ended outside of it.
    QCoreApplication::sendEvent(m_flickable->canvas(), &mouseEvent);
}

QQuickItem* QtFlickProvider::contentItem()
{
    ASSERT(m_contentItem);
    return m_contentItem;
}

QDeclarativeListProperty<QObject> QtFlickProvider::flickableData()
{
    return m_flickableData.read(m_flickable).value<QDeclarativeListProperty<QObject> >();
}

QPointF QtFlickProvider::contentPos() const
{
    qreal x = m_contentX.read(m_flickable).value<qreal>();
    qreal y = m_contentY.read(m_flickable).value<qreal>();
    return QPointF(x, y);
}
void QtFlickProvider::setContentPos(const QPointF& pos)
{
    m_contentX.write(m_flickable, pos.x());
    m_contentY.write(m_flickable, pos.y());
}

QSizeF QtFlickProvider::viewportSize() const
{
    qreal width = m_flickable->width();
    qreal height = m_flickable->height();

    return QSizeF(width, height);
}

void QtFlickProvider::setViewportSize(const QSizeF& size)
{
    ASSERT(size.isValid());
    m_flickable->setWidth(size.width());
    m_flickable->setHeight(size.height());
}

void QtFlickProvider::returnToBounds()
{
    m_returnToBoundsMethod.invoke(m_flickable, Qt::DirectConnection);
}

void QtFlickProvider::cancelFlick()
{
    m_cancelFlickMethod.invoke(m_flickable, Qt::DirectConnection);
}

bool QtFlickProvider::isMoving() const
{
    return m_moving.read(m_flickable).value<bool>();
}

bool QtFlickProvider::isDragging() const
{
    return m_dragging.read(m_flickable).value<bool>();
}

bool QtFlickProvider::isFlicking() const
{
    return m_flicking.read(m_flickable).value<bool>();
}

qreal QtFlickProvider::contentWidth() const
{
    return m_contentWidth.read(m_flickable).value<qreal>();
}

void QtFlickProvider::setContentWidth(qreal width)
{
    m_contentWidth.write(m_flickable, width);
}

qreal QtFlickProvider::contentHeight() const
{
    return m_contentHeight.read(m_flickable).value<qreal>();
}

void QtFlickProvider::setContentHeight(qreal height)
{
    m_contentHeight.write(m_flickable, height);
}

qreal QtFlickProvider::contentX() const
{
    return m_contentX.read(m_flickable).value<qreal>();
}

void QtFlickProvider::setContentX(qreal x)
{
    m_contentX.write(m_flickable, x);
}

qreal QtFlickProvider::contentY() const
{
    return m_contentY.read(m_flickable).value<qreal>();
}

void QtFlickProvider::setContentY(qreal y)
{
    m_contentY.write(m_flickable, y);
}
