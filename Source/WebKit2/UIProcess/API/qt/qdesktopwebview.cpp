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
#include "qdesktopwebview.h"
#include "qdesktopwebview_p.h"
#include "qwkcontext.h"

#include <QCursor>
#include <QGraphicsSceneResizeEvent>
#include <QStyleOptionGraphicsItem>

QDesktopWebViewPrivate::QDesktopWebViewPrivate(QDesktopWebView* q, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
    : q(q)
    , page(this, contextRef ? new QWKContext(contextRef) : defaultWKContext(), pageGroupRef)
    , isCrashed(false)
{
}

void QDesktopWebViewPrivate::setViewNeedsDisplay(const QRect& invalidatedArea)
{
    q->update(invalidatedArea);
}

QSize QDesktopWebViewPrivate::drawingAreaSize()
{
    return q->size().toSize();
}

void QDesktopWebViewPrivate::contentSizeChanged(const QSize&)
{
}

bool QDesktopWebViewPrivate::isActive()
{
    return q->isActive();
}

bool QDesktopWebViewPrivate::hasFocus()
{
    return q->hasFocus();
}

bool QDesktopWebViewPrivate::isVisible()
{
    return q->isVisible();
}

void QDesktopWebViewPrivate::startDrag(Qt::DropActions supportedDropActions, const QImage& dragImage, QMimeData* data, QPoint* clientPosition, QPoint* globalPosition, Qt::DropAction* dropAction)
{
    QWidget* widget = ViewInterface::ownerWidget(q);
    if (!widget)
        return;

    QDrag* drag = new QDrag(widget);
    drag->setPixmap(QPixmap::fromImage(dragImage));
    drag->setMimeData(data);
    *dropAction = drag->exec(supportedDropActions);
    *globalPosition = QCursor::pos();
    *clientPosition = widget->mapFromGlobal(*globalPosition);
}

void QDesktopWebViewPrivate::didChangeUrl(const QUrl& url)
{
    emit q->urlChanged(url);
}

void QDesktopWebViewPrivate::didChangeTitle(const QString& newTitle)
{
    emit q->titleChanged(newTitle);
}

void QDesktopWebViewPrivate::didChangeToolTip(const QString& newToolTip)
{
    q->setToolTip(newToolTip);
}

void QDesktopWebViewPrivate::didChangeStatusText(const QString& newMessage)
{
    emit q->statusBarMessageChanged(newMessage);
}

void QDesktopWebViewPrivate::didChangeCursor(const QCursor& newCursor)
{
    q->setCursor(newCursor);
}

void QDesktopWebViewPrivate::loadDidBegin()
{
    emit q->loadStarted();
}

void QDesktopWebViewPrivate::loadDidSucceed()
{
    emit q->loadSucceeded();
}

void QDesktopWebViewPrivate::loadDidFail(const QWebError& error)
{
    emit q->loadFailed(error);
}

void QDesktopWebViewPrivate::didChangeLoadProgress(int percentageLoaded)
{
    emit q->loadProgress(percentageLoaded);
}

void QDesktopWebViewPrivate::showContextMenu(QSharedPointer<QMenu> menu)
{
    // Remove the active menu in case this function is called twice.
    if (activeMenu)
        activeMenu->hide();

    if (menu->isEmpty())
        return;

    QWidget* widget = ViewInterface::ownerWidget(q);
    if (!widget)
        return;

    activeMenu = menu;

    menu->setParent(widget, menu->windowFlags());
    menu->exec(widget->mapToGlobal(menu->pos()));
    // The last function to get out of exec() clear the local copy.
    if (activeMenu == menu)
        activeMenu.clear();
}

void QDesktopWebViewPrivate::hideContextMenu()
{
    if (activeMenu)
        activeMenu->hide();
}

QDesktopWebView::QDesktopWebView()
    : d(new QDesktopWebViewPrivate(this))
{
    init();
}

QDesktopWebView::QDesktopWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
    : d(new QDesktopWebViewPrivate(this, contextRef, pageGroupRef))
{
    init();
}

void QDesktopWebView::init()
{
    setFocusPolicy(Qt::StrongFocus);
    setAcceptHoverEvents(true);
    setAcceptDrops(true);
}

QDesktopWebView::~QDesktopWebView()
{
    delete d;
}

void QDesktopWebView::load(const QUrl& url)
{
    d->page.load(url);
}

QUrl QDesktopWebView::url() const
{
    return d->page.url();
}

QString QDesktopWebView::title() const
{
    return d->page.title();
}

QAction* QDesktopWebView::navigationAction(QtWebKit::NavigationAction which) const
{
    return d->page.navigationAction(which);
}

void QDesktopWebView::resizeEvent(QGraphicsSceneResizeEvent* ev)
{
    d->page.setDrawingAreaSize(ev->newSize().toSize());
    QGraphicsWidget::resizeEvent(ev);
}

static void paintCrashedPage(QPainter* painter, const QStyleOptionGraphicsItem* option)
{
    painter->fillRect(option->rect, Qt::gray);
    painter->drawText(option->rect, Qt::AlignCenter, QLatin1String(":("));
}

void QDesktopWebView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    if (d->isCrashed) {
        paintCrashedPage(painter, option);
        return;
    }

    d->page.paint(painter, option->exposedRect.toAlignedRect());
}

bool QDesktopWebView::event(QEvent* ev)
{
    if (d->page.handleEvent(ev))
        return true;
    return QGraphicsWidget::event(ev);
}

WKPageRef QDesktopWebView::pageRef() const
{
    return d->page.pageRef();
}

void QDesktopWebViewPrivate::processDidCrash()
{
    isCrashed = true;
    q->update();
}

void QDesktopWebViewPrivate::didRelaunchProcess()
{
    isCrashed = false;
    q->update();
}
