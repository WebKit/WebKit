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

#ifndef QtFlickProvider_h
#define QtFlickProvider_h

#include <QDeclarativeListProperty>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QObject>

QT_BEGIN_NAMESPACE
class QPointF;
class QSizeF;
class QQuickItem;
class QQuickWebPage;
class QQuickWebView;
class QTouchEvent;
QT_END_NAMESPACE

class QtFlickProvider : public QObject {
    Q_OBJECT
public:
    QtFlickProvider(QQuickWebView* viewItem, QQuickWebPage* pageItem);

    void handleTouchFlickEvent(QTouchEvent*);

    QQuickItem* contentItem();
    QDeclarativeListProperty<QObject> flickableData();

    QPointF contentPos() const;
    void setContentPos(const QPointF&);

    QSizeF viewportSize() const;
    void setViewportSize(const QSizeF&);

    void returnToBounds();
    void cancelFlick();

    bool isMoving() const;
    bool isDragging() const;
    bool isFlicking() const;

    qreal contentWidth() const;
    void setContentWidth(qreal);
    qreal contentHeight() const;
    void setContentHeight(qreal);
    qreal contentX() const;
    void setContentX(qreal);
    qreal contentY() const;
    void setContentY(qreal);

Q_SIGNALS:
    void contentWidthChanged();
    void contentHeightChanged();
    void contentXChanged();
    void contentYChanged();
    void movementStarted();
    void movementEnded();
    void flickingChanged();
    void draggingChanged();

private:
    QMetaMethod m_returnToBoundsMethod;
    QMetaMethod m_cancelFlickMethod;

    QMetaProperty m_contentWidth;
    QMetaProperty m_contentHeight;
    QMetaProperty m_contentX;
    QMetaProperty m_contentY;

    QMetaProperty m_moving;
    QMetaProperty m_dragging;
    QMetaProperty m_flicking;

    QMetaProperty m_flickableData;

    QQuickItem* m_contentItem;
    QQuickItem* m_flickable;
};

#endif // QtFlickProvider_h
