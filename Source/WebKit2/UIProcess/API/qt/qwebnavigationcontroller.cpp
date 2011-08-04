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

#include "qwebnavigationcontroller.h"

#include "QtWebPageProxy.h"
#include "qwebkittypes.h"

class QWebNavigationControllerPrivate {
public:
    QWebNavigationControllerPrivate(QtWebPageProxy* pageProxy)
        : pageProxy(pageProxy)
    {
        ASSERT(pageProxy);
    }

    QtWebPageProxy* pageProxy;
};

QWebNavigationController::QWebNavigationController(QtWebPageProxy* pageProxy)
    : QObject(pageProxy)
    , d(new QWebNavigationControllerPrivate(pageProxy))
{
}

QWebNavigationController::~QWebNavigationController()
{
    delete d;
}

QAction* QWebNavigationController::backAction() const
{
    return d->pageProxy->navigationAction(QtWebKit::Back);
}

QAction* QWebNavigationController::forwardAction() const
{
    return d->pageProxy->navigationAction(QtWebKit::Forward);
}

QAction* QWebNavigationController::stopAction() const
{
    return d->pageProxy->navigationAction(QtWebKit::Stop);
}

QAction* QWebNavigationController::reloadAction() const
{
    return d->pageProxy->navigationAction(QtWebKit::Reload);
}

QAction* QWebNavigationController::navigationAction(QtWebKit::NavigationAction which) const
{
    return d->pageProxy->navigationAction(which);
}

void QWebNavigationController::back()
{
    d->pageProxy->navigationAction(QtWebKit::Back)->trigger();
}

void QWebNavigationController::forward()
{
    d->pageProxy->navigationAction(QtWebKit::Forward)->trigger();
}

void QWebNavigationController::stop()
{
    d->pageProxy->navigationAction(QtWebKit::Stop)->trigger();
}

void QWebNavigationController::reload()
{
    d->pageProxy->navigationAction(QtWebKit::Reload)->trigger();
}
