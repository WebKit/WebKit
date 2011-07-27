/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "qtouchwebpage.h"
#include "qtouchwebpage_p.h"
#include "qtouchwebpageproxy.h"

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QUrl>
#include <QtDebug>

QTouchWebPage::QTouchWebPage(QSGItem* parent)
    : QSGPaintedItem(parent)
    , d(new QTouchWebPagePrivate(this))
{
}

QTouchWebPage::~QTouchWebPage()
{
    delete d;
}

void QTouchWebPage::paint(QPainter* painter)
{
    d->page->paint(painter, boundingRect().toAlignedRect());
}

void QTouchWebPage::load(const QUrl& url)
{
    d->page->load(url);
}

QUrl QTouchWebPage::url() const
{
    return d->page->url();
}

QString QTouchWebPage::title() const
{
    return d->page->title();
}

/*! \reimp
*/
bool QTouchWebPage::event(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::Show:
        d->page->setPageIsVisible(true);
        break;
    case QEvent::Hide:
        d->page->setPageIsVisible(false);
        break;
    default:
        break;
    }

    if (d->page->handleEvent(ev))
        return true;
    return QSGPaintedItem::event(ev);
}

void QTouchWebPage::keyPressEvent(QKeyEvent* event)
{
    this->event(event);
}

void QTouchWebPage::keyReleaseEvent(QKeyEvent* event)
{
    this->event(event);
}

void QTouchWebPage::inputMethodEvent(QInputMethodEvent* event)
{
    this->event(event);
}

void QTouchWebPage::focusInEvent(QFocusEvent* event)
{
    this->event(event);
}

void QTouchWebPage::focusOutEvent(QFocusEvent* event)
{
    this->event(event);
}

void QTouchWebPage::touchEvent(QTouchEvent* event)
{
    this->event(event);
}

void QTouchWebPage::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QSGPaintedItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        d->page->setDrawingAreaSize(newGeometry.size().toSize());
}

QAction* QTouchWebPage::navigationAction(QtWebKit::NavigationAction which)
{
    return d->page->navigationAction(which);
}

QTouchWebPagePrivate::QTouchWebPagePrivate(QTouchWebPage* view)
    : q(view)
    , page(0)
{
}

void QTouchWebPagePrivate::commitScaleChange()
{
    page->setContentsScale(q->scale());
}

void QTouchWebPagePrivate::setPage(QTouchWebPageProxy* page)
{
    ASSERT(!this->page);
    ASSERT(page);
    this->page = page;
}

void QTouchWebPagePrivate::setViewportRect(const QRectF& viewportRect)
{
    const QRectF visibleContentRect = q->boundingRect().intersected(viewportRect);
    page->setVisibleContentRect(visibleContentRect);
}

#include "moc_qtouchwebpage.cpp"
