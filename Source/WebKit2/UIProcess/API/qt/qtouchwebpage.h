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

#ifndef qtouchwebpage_h
#define qtouchwebpage_h

#include "qwebkitglobal.h"

#include <QtDeclarative/qquickitem.h>
#include <QSharedPointer>

class QTouchWebPagePrivate;
class QWebNavigationController;
class QWebPreferences;

namespace WebKit {
class QtTouchViewInterface;
}

class QWEBKIT_EXPORT QTouchWebPage : public QQuickItem {
    Q_OBJECT
public:
    QTouchWebPage(QQuickItem* parent = 0);

    virtual ~QTouchWebPage();

    virtual bool event(QEvent*);

protected:
    virtual void keyPressEvent(QKeyEvent*);
    virtual void keyReleaseEvent(QKeyEvent*);
    virtual void inputMethodEvent(QInputMethodEvent*);
    virtual void focusInEvent(QFocusEvent*);
    virtual void focusOutEvent(QFocusEvent*);
    virtual void touchEvent(QTouchEvent*);

    virtual void geometryChanged(const QRectF&, const QRectF&);
    virtual void itemChange(ItemChange, const ItemChangeData&);

private:
    Q_PRIVATE_SLOT(d, void _q_onAfterSceneRender());
    Q_PRIVATE_SLOT(d, void _q_onSceneGraphInitialized());

    void initSceneGraphConnections();

    QTouchWebPagePrivate* d;
    friend class QTouchWebViewPrivate;
    friend class WebKit::QtTouchViewInterface;
};

#endif /* qtouchwebpage_h */
