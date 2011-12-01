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

#ifndef qquickwebpage_p_h
#define qquickwebpage_p_h

#include "qwebkitglobal.h"

#include <QtCore/QSharedPointer>
#include <QtDeclarative/QQuickItem>

class QQuickWebView;
class QQuickWebPagePrivate;
class QWebPreferences;

namespace WebKit {
class QtSGUpdateQueue;
}
class QtWebPageProxy;

class QWEBKIT_EXPORT QQuickWebPage : public QQuickItem {
    Q_OBJECT
public:
    QQuickWebPage(QQuickItem* parent = 0);
    virtual ~QQuickWebPage();

    // Internal. To be removed soon.
    WebKit::QtSGUpdateQueue* sceneGraphUpdateQueue() const;

protected:
    virtual void keyPressEvent(QKeyEvent*);
    virtual void keyReleaseEvent(QKeyEvent*);
    virtual void inputMethodEvent(QInputMethodEvent*);
    virtual void focusInEvent(QFocusEvent*);
    virtual void focusOutEvent(QFocusEvent*);
    virtual void mousePressEvent(QMouseEvent*);
    virtual void mouseMoveEvent(QMouseEvent*);
    virtual void mouseReleaseEvent(QMouseEvent *);
    virtual void mouseDoubleClickEvent(QMouseEvent*);
    virtual void wheelEvent(QWheelEvent*);
    virtual void hoverEnterEvent(QHoverEvent*);
    virtual void hoverMoveEvent(QHoverEvent*);
    virtual void hoverLeaveEvent(QHoverEvent*);
    virtual void dragMoveEvent(QDragMoveEvent*);
    virtual void dragEnterEvent(QDragEnterEvent*);
    virtual void dragLeaveEvent(QDragLeaveEvent*);
    virtual void dropEvent(QDropEvent*);
    virtual void touchEvent(QTouchEvent*);
    virtual bool event(QEvent*);
    virtual void geometryChanged(const QRectF&, const QRectF&);
    virtual QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*);

private:
    QQuickWebPagePrivate* d;
    friend class QQuickWebView;
    friend class QQuickWebViewPrivate;
    friend class QtWebPageProxy;
};

QML_DECLARE_TYPE(QQuickWebPage)

#endif // qquickwebpage_p_h
