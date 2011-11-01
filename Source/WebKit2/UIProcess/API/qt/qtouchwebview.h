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

#ifndef qtouchwebview_h
#define qtouchwebview_h

#include "qbasewebview.h"
#include "qwebkitglobal.h"
#include <QtDeclarative/qquickitem.h>

class QTouchEvent;
class QTouchWebPage;
class QTouchWebViewPrivate;

namespace WebKit {
class QtTouchViewInterface;
}

class QWEBKIT_EXPORT QTouchWebView : public QBaseWebView
{
    Q_OBJECT
    Q_PROPERTY(QTouchWebPage* page READ page CONSTANT FINAL)

public:
    QTouchWebView(QQuickItem* parent = 0);
    ~QTouchWebView();

    QTouchWebPage *page();

protected Q_SLOTS:
    void onVisibleChanged();

protected:
    virtual void geometryChanged(const QRectF&, const QRectF&);
    virtual void touchEvent(QTouchEvent* event);

private:
    Q_PRIVATE_SLOT(d_func(), void _q_viewportUpdated());
    Q_PRIVATE_SLOT(d_func(), void _q_viewportTrajectoryVectorChanged(const QPointF&));

    friend class WebKit::QtTouchViewInterface;
    Q_DECLARE_PRIVATE(QTouchWebView)
};

QML_DECLARE_TYPE(QTouchWebView)

#endif /* qtouchwebview_h */
