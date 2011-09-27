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

#include "qwebkitglobal.h"

#include <QtDeclarative/qsgitem.h>

class QTouchWebPage;
class QTouchWebViewPrivate;

namespace WebKit {
class TouchViewInterface;
}

class QWEBKIT_EXPORT QTouchWebView : public QSGItem
{
    Q_OBJECT
    Q_PROPERTY(QTouchWebPage* page READ page CONSTANT FINAL)

public:
    QTouchWebView(QSGItem* parent = 0);
    ~QTouchWebView();

    QTouchWebPage *page();

protected:
    virtual void geometryChanged(const QRectF&, const QRectF&);

private:
    Q_PRIVATE_SLOT(d, void _q_viewportUpdated());
    Q_PRIVATE_SLOT(d, void _q_viewportTrajectoryVectorChanged(const QPointF&));

    friend class WebKit::TouchViewInterface;
    QTouchWebViewPrivate *d;
};

#endif /* qtouchwebview_h */
