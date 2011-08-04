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

#ifndef qtouchwebpage_p_h
#define qtouchwebpage_p_h

#include <QMenu>
#include "qtouchwebpage.h"
#include "qwebnavigationcontroller.h"

class QRectF;
class QString;
class QTouchWebPage;
class QTouchWebPageProxy;

class QTouchWebPagePrivate {
public:
    QTouchWebPagePrivate(QTouchWebPage* view);

    static QTouchWebPagePrivate* getPageViewPrivate(QTouchWebPage* view) { return view->d; }

    void setPage(QTouchWebPageProxy*);

    void setViewportRect(const QRectF&);
    void commitScaleChange();

    QTouchWebPage* const q;
    QTouchWebPageProxy* page;
    QWebNavigationController* navigationController;
};

#endif /* qtouchwebpage_p_h */
