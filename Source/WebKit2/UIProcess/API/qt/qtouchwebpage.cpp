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
#include <QSGNode>
#include <QUrl>

QTouchWebPage::QTouchWebPage(QSGItem* parent)
    : QSGItem(parent)
    , d(new QTouchWebPagePrivate(this))
{
    setFlag(ItemHasContents);

    // We do the transform from the top left so the viewport can assume the position 0, 0
    // is always where rendering starts.
    setTransformOrigin(TopLeft);
    connect(this, SIGNAL(visibleChanged()), SLOT(onVisibleChanged()));
}

QTouchWebPage::~QTouchWebPage()
{
    delete d;
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

int QTouchWebPage::loadProgress() const
{
    return d->page->loadProgress();
}

/*! \reimp
*/
QSGNode* QTouchWebPage::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!oldNode)
        oldNode = new QSGNode;

    // A swap is on the queue, and SGUpdateQueue::applyUpdates will empty the queue, so we know that
    // the old frame's buffers won't be used anymore (for buffers used all the way from the web process
    // to the graphic card). Notify the web process that it can render the next frame.
    if (d->sgUpdateQueue.isSwapPending())
        // The UI thread is currently locked and calling this from the rendering thread didn't crash
        // yet, so I'm assuming that this is OK. Else we have to wait until the SG update is over
        // and the UI thread returns to the event loop picking our event before it can be sent to
        // the web process. This would increase our chances of missing a frame.
        d->page->renderNextFrame();
    d->sgUpdateQueue.applyUpdates(oldNode);

    // QSGItem takes ownership of this return value and it's children between and after updatePaintNode calls.
    return oldNode;
}

/*! \reimp
*/
bool QTouchWebPage::event(QEvent* ev)
{
    if (d->page->handleEvent(ev))
        return true;
    return QSGItem::event(ev);
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
    QSGItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        d->page->setDrawingAreaSize(newGeometry.size().toSize());
}

QWebNavigationController* QTouchWebPage::navigationController() const
{
    if (!d->navigationController)
        d->navigationController = new QWebNavigationController(d->page);
    return d->navigationController;
}

QTouchWebPagePrivate::QTouchWebPagePrivate(QTouchWebPage* view)
    : q(view)
    , page(0)
    , navigationController(0)
    , sgUpdateQueue(view)
{
}

void QTouchWebPagePrivate::setPage(QTouchWebPageProxy* page)
{
    ASSERT(!this->page);
    ASSERT(page);
    this->page = page;
}

void QTouchWebPage::onVisibleChanged()
{
    d->page->setPageIsVisible(isVisible());
}

#include "moc_qtouchwebpage.cpp"
