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

#include "qdesktopwebview_p.h"
#include "qdesktopwebpageproxy.h"
#include "DrawingAreaProxyImpl.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include <QApplication>
#include <QEvent>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsWidget>
#include <WebCore/DragData.h>
#include <WebCore/Region.h>

using namespace WebCore;
using namespace WebKit;

static inline Qt::DropAction dragOperationToDropAction(unsigned dragOperation)
{
    Qt::DropAction result = Qt::IgnoreAction;
    if (dragOperation & DragOperationCopy)
        result = Qt::CopyAction;
    else if (dragOperation & DragOperationMove)
        result = Qt::MoveAction;
    else if (dragOperation & DragOperationGeneric)
        result = Qt::MoveAction;
    else if (dragOperation & DragOperationLink)
        result = Qt::LinkAction;
    return result;
}

QDesktopWebPageProxy::QDesktopWebPageProxy(QDesktopWebViewPrivate* desktopWebView, WKContextRef context, WKPageGroupRef pageGroup)
    : QtWebPageProxy(desktopWebView, desktopWebView, context, pageGroup)
{
    init();
}

PassOwnPtr<DrawingAreaProxy> QDesktopWebPageProxy::createDrawingAreaProxy()
{
    return DrawingAreaProxyImpl::create(m_webPageProxy.get());
}

void QDesktopWebPageProxy::paintContent(QPainter* painter, const QRect& area)
{
    // FIXME: Do something with the unpainted region?
    WebCore::Region unpaintedRegion;
    static_cast<DrawingAreaProxyImpl*>(m_webPageProxy->drawingArea())->paint(painter, area, unpaintedRegion);
}

void QDesktopWebPageProxy::setViewportArguments(const WebCore::ViewportArguments&)
{
    // We ignore the viewport definition on the Desktop.
}

#if ENABLE(TOUCH_EVENTS)
void QDesktopWebPageProxy::doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled)
{
    // We do not handle touch on Desktop for now, the events are not supposed to be forwarded to the WebProcess.
    ASSERT_NOT_REACHED();
}
#endif

bool QDesktopWebPageProxy::handleEvent(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::GraphicsSceneMouseMove:
        return handleMouseMoveEvent(reinterpret_cast<QGraphicsSceneMouseEvent*>(ev));
    case QEvent::GraphicsSceneMousePress:
        return handleMousePressEvent(reinterpret_cast<QGraphicsSceneMouseEvent*>(ev));
    case QEvent::GraphicsSceneMouseRelease:
        return handleMouseReleaseEvent(reinterpret_cast<QGraphicsSceneMouseEvent*>(ev));
    case QEvent::GraphicsSceneMouseDoubleClick:
        return handleMouseDoubleClickEvent(reinterpret_cast<QGraphicsSceneMouseEvent*>(ev));
    case QEvent::GraphicsSceneWheel:
        return handleWheelEvent(reinterpret_cast<QGraphicsSceneWheelEvent*>(ev));
    case QEvent::GraphicsSceneHoverMove:
        return handleHoverMoveEvent(reinterpret_cast<QGraphicsSceneHoverEvent*>(ev));
    case QEvent::GraphicsSceneDragEnter:
        return handleDragEnterEvent(reinterpret_cast<QGraphicsSceneDragDropEvent*>(ev));
    case QEvent::GraphicsSceneDragLeave:
        return handleDragLeaveEvent(reinterpret_cast<QGraphicsSceneDragDropEvent*>(ev));
    case QEvent::GraphicsSceneDragMove:
        return handleDragMoveEvent(reinterpret_cast<QGraphicsSceneDragDropEvent*>(ev));
    case QEvent::GraphicsSceneDrop:
        return handleDropEvent(reinterpret_cast<QGraphicsSceneDragDropEvent*>(ev));
    }
    return QtWebPageProxy::handleEvent(ev);
}

bool QDesktopWebPageProxy::handleMouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
    // For some reason mouse press results in mouse hover (which is
    // converted to mouse move for WebKit). We ignore these hover
    // events by comparing lastPos with newPos.
    // NOTE: lastPos from the event always comes empty, so we work
    // around that here.
    static QPointF lastPos = QPointF();
    if (lastPos == ev->pos())
        return ev->isAccepted();
    lastPos = ev->pos();

    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/0));

    return ev->isAccepted();
}

bool QDesktopWebPageProxy::handleMousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    if (m_tripleClickTimer.isActive() && (ev->pos() - m_tripleClick).manhattanLength() < QApplication::startDragDistance()) {
        m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/3));
        return ev->isAccepted();
    }

    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/1));
    return ev->isAccepted();
}

bool QDesktopWebPageProxy::handleMouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/0));
    return ev->isAccepted();
}

bool QDesktopWebPageProxy::handleMouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/2));

    m_tripleClickTimer.start(QApplication::doubleClickInterval(), this);
    m_tripleClick = ev->pos().toPoint();
    return ev->isAccepted();
}

bool QDesktopWebPageProxy::handleWheelEvent(QGraphicsSceneWheelEvent* ev)
{
    m_webPageProxy->handleWheelEvent(NativeWebWheelEvent(ev));
    return ev->isAccepted();
}

bool QDesktopWebPageProxy::handleHoverMoveEvent(QGraphicsSceneHoverEvent* ev)
{
    QGraphicsSceneMouseEvent me(QEvent::GraphicsSceneMouseMove);
    me.setPos(ev->pos());
    me.setScreenPos(ev->screenPos());
    me.setAccepted(ev->isAccepted());

    return handleMouseMoveEvent(&me);
}

bool QDesktopWebPageProxy::handleDragEnterEvent(QGraphicsSceneDragDropEvent* ev)
{
    m_webPageProxy->resetDragOperation();
    // FIXME: Should not use QCursor::pos()
    DragData dragData(ev->mimeData(), ev->pos().toPoint(), QCursor::pos(), dropActionToDragOperation(ev->possibleActions()));
    m_webPageProxy->dragEntered(&dragData);
    ev->acceptProposedAction();
    return true;
}

bool QDesktopWebPageProxy::handleDragLeaveEvent(QGraphicsSceneDragDropEvent* ev)
{
    bool accepted = ev->isAccepted();

    // FIXME: Should not use QCursor::pos()
    DragData dragData(0, IntPoint(), QCursor::pos(), DragOperationNone);
    m_webPageProxy->dragExited(&dragData);
    m_webPageProxy->resetDragOperation();

    ev->setAccepted(accepted);
    return accepted;
}

bool QDesktopWebPageProxy::handleDragMoveEvent(QGraphicsSceneDragDropEvent* ev)
{
    bool accepted = ev->isAccepted();

    // FIXME: Should not use QCursor::pos()
    DragData dragData(ev->mimeData(), ev->pos().toPoint(), QCursor::pos(), dropActionToDragOperation(ev->possibleActions()));
    m_webPageProxy->dragUpdated(&dragData);
    ev->setDropAction(dragOperationToDropAction(m_webPageProxy->dragOperation()));
    if (m_webPageProxy->dragOperation() != DragOperationNone)
        ev->accept();

    ev->setAccepted(accepted);
    return accepted;
}

bool QDesktopWebPageProxy::handleDropEvent(QGraphicsSceneDragDropEvent* ev)
{
    bool accepted = ev->isAccepted();

    // FIXME: Should not use QCursor::pos()
    DragData dragData(ev->mimeData(), ev->pos().toPoint(), QCursor::pos(), dropActionToDragOperation(ev->possibleActions()));
    SandboxExtension::Handle handle;
    m_webPageProxy->performDrag(&dragData, String(), handle);
    ev->setDropAction(dragOperationToDropAction(m_webPageProxy->dragOperation()));
    ev->accept();

    ev->setAccepted(accepted);
    return accepted;
}

void QDesktopWebPageProxy::timerEvent(QTimerEvent* ev)
{
    int timerId = ev->timerId();
    if (timerId == m_tripleClickTimer.timerId())
        m_tripleClickTimer.stop();
    else
        QObject::timerEvent(ev);
}
