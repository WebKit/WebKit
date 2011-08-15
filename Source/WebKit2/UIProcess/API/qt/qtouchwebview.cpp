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
#include "qtouchwebview.h"
#include "qtouchwebview_p.h"

#include "TouchViewInterface.h"
#include "qtouchwebpage_p.h"
#include "QtWebPageProxy.h"
#include "WebPageGroup.h"
#include "WebPreferences.h"
#include <qgraphicssceneevent.h>

QTouchWebViewPrivate::QTouchWebViewPrivate(QTouchWebView* q)
    : q(q)
    , pageView(new QTouchWebPage(q))
    , viewInterface(q, pageView.data())
    , page(&viewInterface, defaultWKContext())
{
    QTouchWebPagePrivate* const pageViewPrivate = pageView.data()->d;
    pageViewPrivate->setPage(&page);
}

void QTouchWebViewPrivate::scroll(qreal deltaX, qreal deltaY)
{
    pageView->setX(pageView->x() + deltaX);
    pageView->setY(pageView->y() + deltaY);
    viewportRectUpdated();
}

void QTouchWebViewPrivate::viewportRectUpdated()
{
    QTouchWebPagePrivate* const pageViewPrivate = pageView.data()->d;
    const QRectF viewportRectInPageViewCoordinate = q->mapRectToItem(pageView.data(), q->boundingRect());
    pageViewPrivate->setViewportRect(viewportRectInPageViewCoordinate);
}

void QTouchWebViewPrivate::updateViewportState()
{
    QSize availableSize = q->boundingRect().size().toSize();

    WebPageProxy* wkPage = toImpl(page.pageRef());
    WebPreferences* wkPrefs = wkPage->pageGroup()->preferences();

    WebCore::ViewportAttributes attr = WebCore::computeViewportAttributes(viewportArguments, wkPrefs->layoutFallbackWidth(), wkPrefs->deviceWidth(), wkPrefs->deviceHeight(), wkPrefs->deviceDPI(), availableSize);

    viewport.initialScale = attr.initialScale;
    viewport.minimumScale = attr.minimumScale;
    viewport.maximumScale = attr.maximumScale;
    viewport.pixelRatio = attr.devicePixelRatio;
    viewport.isUserScalable = !!attr.userScalable;

    // Overwrite minimum scale value with fit-to-view value, unless the the content author
    // explicitly says no. NB: We can only do this when we know we have a valid size, ie.
    // after initial layout has completed.

    // FIXME: Do this on the web process side.
    wkPage->setResizesToContentsUsingLayoutSize(attr.layoutSize);
}

void QTouchWebViewPrivate::setViewportArguments(const WebCore::ViewportArguments& args)
{
    viewportArguments = args;
    updateViewportState();
}

QTouchWebView::QTouchWebView(QSGItem* parent)
    : QSGItem(parent)
    , d(new QTouchWebViewPrivate(this))
{
    setFlags(QSGItem::ItemClipsChildrenToShape);
}

QTouchWebView::~QTouchWebView()
{
    delete d;
}

QTouchWebPage* QTouchWebView::page()
{
    return d->pageView.data();
}

void QTouchWebView::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QSGItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        d->updateViewportState();
        d->viewportRectUpdated();
    }
}

#include "moc_qtouchwebview.cpp"
