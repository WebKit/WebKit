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

#include <QGraphicsSceneResizeEvent>
#include <QStyleOptionGraphicsItem>
#include <QtDeclarative/qsgcanvas.h>
#include <QtDeclarative/qsgevent.h>
#include <QtDeclarative/qsgitem.h>
#include <QtGui/QCursor>
#include <QtGui/QFocusEvent>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QHoverEvent>
#include <QtGui/QInputMethodEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QTouchEvent>
#include <QtGui/QWheelEvent>

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
    return QSize(q->width(), q->height());
}

void QDesktopWebViewPrivate::contentSizeChanged(const QSize&)
{
}

bool QDesktopWebViewPrivate::isActive()
{
    // FIXME: The scene graph did not have the concept of being active or not when this was written.
    return true;
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
    QWidget* widget = q->canvas();
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
    // FIXME: Add a proper implementation when Qt 5 supports tooltip.
    q->canvas()->setToolTip(newToolTip);
}

void QDesktopWebViewPrivate::didChangeStatusText(const QString& newMessage)
{
    emit q->statusBarMessageChanged(newMessage);
}

void QDesktopWebViewPrivate::didChangeCursor(const QCursor& newCursor)
{
    // FIXME: add proper cursor handling when Qt 5 supports it.
    q->canvas()->setCursor(newCursor);
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

    QWidget* widget = q->canvas();
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

QDesktopWebView::QDesktopWebView(QSGItem* parent)
    : QSGPaintedItem(parent)
    , d(new QDesktopWebViewPrivate(this))
{
    init();
}

QDesktopWebView::QDesktopWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef, QSGItem* parent)
    : QSGPaintedItem(parent)
    , d(new QDesktopWebViewPrivate(this, contextRef, pageGroupRef))
{
    init();
}

void QDesktopWebView::init()
{
    setAcceptedMouseButtons(Qt::MouseButtonMask);
    setAcceptHoverEvents(true);
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

static void paintCrashedPage(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, Qt::gray);
    painter->drawText(rect, Qt::AlignCenter, QLatin1String(":("));
}

void QDesktopWebView::keyPressEvent(QKeyEvent* event)
{
    this->event(event);
}

void QDesktopWebView::keyReleaseEvent(QKeyEvent* event)
{
    this->event(event);
}

void QDesktopWebView::inputMethodEvent(QInputMethodEvent* event)
{
    this->event(event);
}

void QDesktopWebView::focusInEvent(QFocusEvent* event)
{
    this->event(event);
}

void QDesktopWebView::focusOutEvent(QFocusEvent* event)
{
    this->event(event);
}

void QDesktopWebView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    this->event(event);
}

void QDesktopWebView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    this->event(event);
}

void QDesktopWebView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    this->event(event);
}

void QDesktopWebView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    this->event(event);
}

void QDesktopWebView::wheelEvent(QWheelEvent* event)
{
    // FIXME: for some reason, the scene graph delivers QWheelEvent instead of QGraphicsSceneWheelEvent.
    // We transform them in QGraphicsSceneWheelEvent for consistency. Otherwise the position would be complete magic.
    // We shoud modify the scenegraph to get the correct type of events.
    QGraphicsSceneWheelEvent graphicsEvent(QEvent::GraphicsSceneWheel);
    graphicsEvent.setPos(event->pos());
    graphicsEvent.setButtons(event->buttons());
    graphicsEvent.setDelta(event->delta());
    graphicsEvent.setModifiers(event->modifiers());
    graphicsEvent.setOrientation(event->orientation());
    graphicsEvent.setScenePos(mapToScene(event->pos()));
    graphicsEvent.setScreenPos(event->globalPos());
    this->event(&graphicsEvent);
}

void QDesktopWebView::touchEvent(QTouchEvent* event)
{
    this->event(event);
}

void QDesktopWebView::hoverEnterEvent(QHoverEvent* event)
{
    this->event(event);
}

void QDesktopWebView::hoverMoveEvent(QHoverEvent* event)
{
    this->event(event);
}

void QDesktopWebView::hoverLeaveEvent(QHoverEvent* event)
{
    this->event(event);
}

void QDesktopWebView::dragMoveEvent(QSGDragEvent* event)
{
    this->event(event);
}

void QDesktopWebView::dragEnterEvent(QSGDragEvent* event)
{
    this->event(event);
}

void QDesktopWebView::dragExitEvent(QSGDragEvent* event)
{
    this->event(event);
}

void QDesktopWebView::dragDropEvent(QSGDragEvent* event)
{
    this->event(event);
}

void QDesktopWebView::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QSGPaintedItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        d->page.setDrawingAreaSize(newGeometry.size().toSize());
}

void QDesktopWebView::paint(QPainter* painter)
{
    const QRectF rect = boundingRect();
    if (d->isCrashed) {
        paintCrashedPage(painter, rect);
        return;
    }

    d->page.paint(painter, rect.toAlignedRect());
}

bool QDesktopWebView::event(QEvent* ev)
{
    if (d->page.handleEvent(ev))
        return true;
    return QSGItem::event(ev);
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

#include "moc_qdesktopwebview.cpp"
