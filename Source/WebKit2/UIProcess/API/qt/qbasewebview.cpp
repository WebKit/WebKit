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
#include "qbasewebview.h"

#include "QtWebPageProxy.h"
#include "qbasewebview_p.h"
#include "qwebnavigationcontroller.h"

QBaseWebViewPrivate::QBaseWebViewPrivate()
    : q_ptr(0)
    , navigationController(0)
{
}

void QBaseWebViewPrivate::setPageProxy(QtWebPageProxy* pageProxy)
{
    this->pageProxy.reset(pageProxy);
}

QBaseWebView::QBaseWebView(QBaseWebViewPrivate &dd, QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , d_ptr(&dd)
{
    d_ptr->q_ptr = this;
}

QBaseWebView::~QBaseWebView()
{
}

void QBaseWebView::load(const QUrl& url)
{
    Q_D(QBaseWebView);
    d->pageProxy->load(url);
}

QUrl QBaseWebView::url() const
{
    Q_D(const QBaseWebView);
    return d->pageProxy->url();
}

int QBaseWebView::loadProgress() const
{
    Q_D(const QBaseWebView);
    return d->pageProxy->loadProgress();
}

QString QBaseWebView::title() const
{
    Q_D(const QBaseWebView);
    return d->pageProxy->title();
}

QWebNavigationController* QBaseWebView::navigationController() const
{
    Q_D(const QBaseWebView);
    if (!d->navigationController)
        const_cast<QBaseWebViewPrivate*>(d)->navigationController = new QWebNavigationController(d->pageProxy.data());
    return d->navigationController;
}

QWebPreferences* QBaseWebView::preferences() const
{
    Q_D(const QBaseWebView);
    return d->pageProxy->preferences();
}

#include "moc_qbasewebview.cpp"
