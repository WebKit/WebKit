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

void QTouchWebViewPrivate::init(QTouchWebView* viewport)
{
    pageView.reset(new QTouchWebPage(viewport));
    viewInterface.reset(new WebKit::QtTouchViewInterface(viewport, pageView.data()));
    interactionEngine.reset(new QtViewportInteractionEngine(viewport, pageView.data()));
    QTouchWebPagePrivate* const pageViewPrivate = pageView.data()->d;
    setPageProxy(new QtTouchWebPageProxy(viewInterface.data(), interactionEngine.data()));
    pageViewPrivate->setPageProxy(touchPageProxy());

    QObject::connect(interactionEngine.data(), SIGNAL(viewportUpdateRequested()), viewport, SLOT(_q_viewportUpdated()));
    QObject::connect(interactionEngine.data(), SIGNAL(viewportTrajectoryVectorChanged(const QPointF&)), viewport, SLOT(_q_viewportTrajectoryVectorChanged(const QPointF&)));
}

void QTouchWebViewPrivate::loadDidCommit()
{
    interactionEngine->reset();
}

void QTouchWebViewPrivate::_q_viewportUpdated()
{
    Q_Q(QTouchWebView);
    const QRectF visibleRectInPageViewCoordinates = q->mapRectToItem(pageView.data(), q->boundingRect()).intersected(pageView->boundingRect());
    float scale = pageView->scale();
    touchPageProxy()->setVisibleContentRectAndScale(visibleRectInPageViewCoordinates, scale);
}

void QTouchWebViewPrivate::_q_viewportTrajectoryVectorChanged(const QPointF& trajectoryVector)
{
    touchPageProxy()->setVisibleContentRectTrajectoryVector(trajectoryVector);
}

void QTouchWebViewPrivate::updateViewportSize()
{
    Q_Q(QTouchWebView);
    QSize viewportSize = q->boundingRect().size().toSize();

    if (viewportSize.isEmpty())
        return;

    WebPageProxy* wkPage = toImpl(pageProxy->pageRef());
    // Let the WebProcess know about the new viewport size, so that
    // it can resize the content accordingly.
    wkPage->setViewportSize(viewportSize);
    updateViewportConstraints();
    _q_viewportUpdated();
}

void QTouchWebViewPrivate::updateViewportConstraints()
{
    Q_Q(QTouchWebView);
    QSize availableSize = q->boundingRect().size().toSize();

    if (availableSize.isEmpty())
        return;

    WebPageProxy* wkPage = toImpl(pageProxy->pageRef());
    WebPreferences* wkPrefs = wkPage->pageGroup()->preferences();

    // FIXME: Remove later; Hardcode some values for now to make sure the DPI adjustment is being tested.
    wkPrefs->setDeviceDPI(240);
    wkPrefs->setDeviceWidth(480);
    wkPrefs->setDeviceHeight(720);

    WebCore::ViewportAttributes attr = WebCore::computeViewportAttributes(viewportArguments, wkPrefs->layoutFallbackWidth(), wkPrefs->deviceWidth(), wkPrefs->deviceHeight(), wkPrefs->deviceDPI(), availableSize);

    QtViewportInteractionEngine::Constraints newConstraints;
    newConstraints.initialScale = attr.initialScale;
    newConstraints.minimumScale = attr.minimumScale;
    newConstraints.maximumScale = attr.maximumScale;
    newConstraints.devicePixelRatio = attr.devicePixelRatio;
    newConstraints.isUserScalable = !!attr.userScalable;
    interactionEngine->setConstraints(newConstraints);

    // Overwrite minimum scale value with fit-to-view value, unless the the content author
    // explicitly says no. NB: We can only do this when we know we have a valid size, ie.
    // after initial layout has completed.
}

void QTouchWebViewPrivate::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    viewportArguments = args;
    updateViewportConstraints();
}

QTouchWebView::QTouchWebView(QQuickItem* parent)
    : QBaseWebView(*(new QTouchWebViewPrivate), parent)
{
    Q_D(QTouchWebView);
    d->init(this);
    setFlags(QQuickItem::ItemClipsChildrenToShape);
    connect(this, SIGNAL(visibleChanged()), SLOT(onVisibleChanged()));
}

QTouchWebView::~QTouchWebView()
{
}

QTouchWebPage* QTouchWebView::page()
{
    Q_D(QTouchWebView);
    return d->pageView.data();
}

void QTouchWebView::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    Q_D(QTouchWebView);
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        d->updateViewportSize();
}

void QTouchWebView::touchEvent(QTouchEvent* event)
{
    forceActiveFocus();
    QQuickItem::touchEvent(event);
}

void QTouchWebView::onVisibleChanged()
{
    Q_D(QTouchWebView);
    WebPageProxy* wkPage = toImpl(d->pageProxy->pageRef());

    wkPage->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

#include "moc_qtouchwebview.cpp"
