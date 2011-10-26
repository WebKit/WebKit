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

#include "QtTouchViewInterface.h"
#include "QtWebPageProxy.h"
#include "qtouchwebpage_p.h"
#include "qtouchwebview_p.h"
#include "WebPageGroup.h"
#include "WebPreferences.h"

QTouchWebViewPrivate::QTouchWebViewPrivate(QTouchWebView* q)
    : q(q)
    , pageView(new QTouchWebPage(q))
    , viewInterface(q, pageView.data())
    , interactionEngine(q, pageView.data())
    , page(&viewInterface, &interactionEngine)
{
    QTouchWebPagePrivate* const pageViewPrivate = pageView.data()->d;
    pageViewPrivate->setPage(&page);

    QObject::connect(&interactionEngine, SIGNAL(viewportUpdateRequested()), q, SLOT(_q_viewportUpdated()));
    QObject::connect(&interactionEngine, SIGNAL(viewportTrajectoryVectorChanged(const QPointF&)), q, SLOT(_q_viewportTrajectoryVectorChanged(const QPointF&)));
}

void QTouchWebViewPrivate::loadDidCommit()
{
    interactionEngine.reset();
}

void QTouchWebViewPrivate::_q_viewportUpdated()
{
    const QRectF visibleRectInPageViewCoordinate = q->mapRectToItem(pageView.data(), q->boundingRect()).intersected(pageView->boundingRect());
    float scale = pageView->scale();
    page.setVisibleContentRectAndScale(visibleRectInPageViewCoordinate, scale);
}

void QTouchWebViewPrivate::_q_viewportTrajectoryVectorChanged(const QPointF& trajectoryVector)
{
    page.setVisibleContentRectTrajectoryVector(trajectoryVector);
}

void QTouchWebViewPrivate::updateViewportConstraints()
{
    QSize availableSize = q->boundingRect().size().toSize();

    if (availableSize.isEmpty())
        return;

    WebPageProxy* wkPage = toImpl(page.pageRef());
    WebPreferences* wkPrefs = wkPage->pageGroup()->preferences();

    WebCore::ViewportAttributes attr = WebCore::computeViewportAttributes(viewportArguments, wkPrefs->layoutFallbackWidth(), wkPrefs->deviceWidth(), wkPrefs->deviceHeight(), wkPrefs->deviceDPI(), availableSize);

    QtViewportInteractionEngine::Constraints newConstraints;
    newConstraints.initialScale = attr.initialScale;
    newConstraints.minimumScale = attr.minimumScale;
    newConstraints.maximumScale = attr.maximumScale;
    newConstraints.isUserScalable = !!attr.userScalable;
    interactionEngine.setConstraints(newConstraints);

    // Overwrite minimum scale value with fit-to-view value, unless the the content author
    // explicitly says no. NB: We can only do this when we know we have a valid size, ie.
    // after initial layout has completed.

    // FIXME: Do this on the web process side.
    wkPage->setResizesToContentsUsingLayoutSize(attr.layoutSize);
}

void QTouchWebViewPrivate::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    viewportArguments = args;
    updateViewportConstraints();
}

QTouchWebView::QTouchWebView(QSGItem* parent)
    : QSGItem(parent)
    , d(new QTouchWebViewPrivate(this))
{
    setFlags(QSGItem::ItemClipsChildrenToShape);
    connect(this, SIGNAL(visibleChanged()), SLOT(onVisibleChanged()));
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
        d->updateViewportConstraints();
        d->_q_viewportUpdated();
    }
}

void QTouchWebView::touchEvent(QTouchEvent* event)
{
    forceActiveFocus();
    QSGItem::touchEvent(event);
}

void QTouchWebView::onVisibleChanged()
{
    WebPageProxy* pageProxy = toImpl(d->page.pageRef());

    pageProxy->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

#include "moc_qtouchwebview.cpp"
