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
    connect(pageProxy, SIGNAL(updateNavigationState()), this, SIGNAL(navigationStateChanged()));
}

QWebNavigationController::~QWebNavigationController()
{
    delete d;
}

bool QWebNavigationController::canGoBack() const
{
    return d->pageProxy->canGoBack();
}

bool QWebNavigationController::canGoForward() const
{
    return d->pageProxy->canGoForward();
}

bool QWebNavigationController::canStop() const
{
    return d->pageProxy->canStop();
}

bool QWebNavigationController::canReload() const
{
    return d->pageProxy->canReload();
}

void QWebNavigationController::goBack()
{
    d->pageProxy->goBack();
}

void QWebNavigationController::goForward()
{
    d->pageProxy->goForward();
}

void QWebNavigationController::stop()
{
    d->pageProxy->stop();
}

void QWebNavigationController::reload()
{
    d->pageProxy->reload();
}
