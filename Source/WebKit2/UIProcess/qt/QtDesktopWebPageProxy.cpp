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
#include "QtDesktopWebPageProxy.h"

#include "DrawingAreaProxyImpl.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "WebPopupMenuProxyQtDesktop.h"
#include "qdesktopwebview.h"
#include "qdesktopwebview_p.h"
#include "qwebpreferences_p.h"
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

QtDesktopWebPageProxy::QtDesktopWebPageProxy(QDesktopWebViewPrivate* desktopWebView, WKContextRef context, WKPageGroupRef pageGroup)
    : QtWebPageProxy(desktopWebView, desktopWebView, context, pageGroup)
{
    QWebPreferencesPrivate::get(preferences())->setAttribute(QWebPreferencesPrivate::AcceleratedCompositingEnabled, false);
    init();
}

PassOwnPtr<DrawingAreaProxy> QtDesktopWebPageProxy::createDrawingAreaProxy()
{
    return DrawingAreaProxyImpl::create(m_webPageProxy.get());
}

void QtDesktopWebPageProxy::paintContent(QPainter* painter, const QRect& area)
{
    // FIXME: Do something with the unpainted region?
    WebCore::Region unpaintedRegion;
    static_cast<DrawingAreaProxyImpl*>(m_webPageProxy->drawingArea())->paint(painter, area, unpaintedRegion);
}

void QtDesktopWebPageProxy::didChangeViewportProperties(const WebCore::ViewportArguments&)
{
    // We ignore the viewport definition on the Desktop.
}

#if ENABLE(TOUCH_EVENTS)
void QtDesktopWebPageProxy::doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled)
{
    // We do not handle touch on Desktop for now, the events are not supposed to be forwarded to the WebProcess.
    ASSERT_NOT_REACHED();
}
#endif

PassRefPtr<WebPopupMenuProxy> QtDesktopWebPageProxy::createPopupMenuProxy(WebPageProxy*)
{
    QQuickItem* webViewItem = static_cast<QDesktopWebViewPrivate*>(m_viewInterface)->webView();
    return WebPopupMenuProxyQtDesktop::create(m_webPageProxy.get(), webViewItem);
}

bool QtDesktopWebPageProxy::handleEvent(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::MouseMove:
        return handleMouseMoveEvent(reinterpret_cast<QMouseEvent*>(ev));
    case QEvent::MouseButtonPress:
        return handleMousePressEvent(reinterpret_cast<QMouseEvent*>(ev));
    case QEvent::MouseButtonRelease:
        return handleMouseReleaseEvent(reinterpret_cast<QMouseEvent*>(ev));
    case QEvent::MouseButtonDblClick:
        return handleMouseDoubleClickEvent(reinterpret_cast<QMouseEvent*>(ev));
    case QEvent::Wheel:
        return handleWheelEvent(reinterpret_cast<QWheelEvent*>(ev));
    case QEvent::HoverLeave:
        return handleHoverLeaveEvent(reinterpret_cast<QHoverEvent*>(ev));
    case QEvent::HoverEnter: // Fall-through, for WebKit the distinction doesn't matter.
    case QEvent::HoverMove:
        return handleHoverMoveEvent(reinterpret_cast<QHoverEvent*>(ev));
    case QEvent::DragEnter:
        return handleDragEnterEvent(reinterpret_cast<QDragEnterEvent*>(ev));
    case QEvent::DragLeave:
        return handleDragLeaveEvent(reinterpret_cast<QDragLeaveEvent*>(ev));
    case QEvent::DragMove:
        return handleDragMoveEvent(reinterpret_cast<QDragMoveEvent*>(ev));
    case QEvent::Drop:
        return handleDropEvent(reinterpret_cast<QDropEvent*>(ev));
    // Touch events only for WebKitTestRunner
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        return handleTouchEvent(static_cast<QTouchEvent*>(ev));
    default:
        break;
    }
    return QtWebPageProxy::handleEvent(ev);
}

bool QtDesktopWebPageProxy::handleMouseMoveEvent(QMouseEvent* ev)
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

bool QtDesktopWebPageProxy::handleMousePressEvent(QMouseEvent* ev)
{
    if (m_tripleClickTimer.isActive() && (ev->pos() - m_tripleClick).manhattanLength() < QApplication::startDragDistance()) {
        m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/3));
        return ev->isAccepted();
    }

    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/1));
    return ev->isAccepted();
}

bool QtDesktopWebPageProxy::handleMouseReleaseEvent(QMouseEvent* ev)
{
    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/0));
    return ev->isAccepted();
}

bool QtDesktopWebPageProxy::handleMouseDoubleClickEvent(QMouseEvent* ev)
{
    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/2));

    m_tripleClickTimer.start(QApplication::doubleClickInterval(), this);
    m_tripleClick = ev->localPos().toPoint();
    return ev->isAccepted();
}

bool QtDesktopWebPageProxy::handleWheelEvent(QWheelEvent* ev)
{
    m_webPageProxy->handleWheelEvent(NativeWebWheelEvent(ev));
    return ev->isAccepted();
}

bool QtDesktopWebPageProxy::handleHoverLeaveEvent(QHoverEvent* ev)
{
    // To get the correct behavior of mouseout, we need to turn the Leave event of our webview into a mouse move
    // to a very far region.
    QHoverEvent fakeEvent(QEvent::HoverMove, QPoint(INT_MIN, INT_MIN), ev->oldPos());
    return handleHoverMoveEvent(&fakeEvent);
}

bool QtDesktopWebPageProxy::handleHoverMoveEvent(QHoverEvent* ev)
{
    QMouseEvent me(QEvent::MouseMove, ev->pos(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    me.setAccepted(ev->isAccepted());

    return handleMouseMoveEvent(&me);
}

bool QtDesktopWebPageProxy::handleDragEnterEvent(QDragEnterEvent* ev)
{
    m_webPageProxy->resetDragOperation();
    // FIXME: Should not use QCursor::pos()
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), dropActionToDragOperation(ev->possibleActions()));
    m_webPageProxy->dragEntered(&dragData);
    ev->acceptProposedAction();
    return true;
}

bool QtDesktopWebPageProxy::handleDragLeaveEvent(QDragLeaveEvent* ev)
{
    bool accepted = ev->isAccepted();

    // FIXME: Should not use QCursor::pos()
    DragData dragData(0, IntPoint(), QCursor::pos(), DragOperationNone);
    m_webPageProxy->dragExited(&dragData);
    m_webPageProxy->resetDragOperation();

    ev->setAccepted(accepted);
    return accepted;
}

bool QtDesktopWebPageProxy::handleDragMoveEvent(QDragMoveEvent* ev)
{
    bool accepted = ev->isAccepted();

    // FIXME: Should not use QCursor::pos()
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), dropActionToDragOperation(ev->possibleActions()));
    m_webPageProxy->dragUpdated(&dragData);
    ev->setDropAction(dragOperationToDropAction(m_webPageProxy->dragSession().operation));
    if (m_webPageProxy->dragSession().operation != DragOperationNone)
        ev->accept();

    ev->setAccepted(accepted);
    return accepted;
}

bool QtDesktopWebPageProxy::handleDropEvent(QDropEvent* ev)
{
    bool accepted = ev->isAccepted();

    // FIXME: Should not use QCursor::pos()
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), dropActionToDragOperation(ev->possibleActions()));
    SandboxExtension::Handle handle;
    m_webPageProxy->performDrag(&dragData, String(), handle);
    ev->setDropAction(dragOperationToDropAction(m_webPageProxy->dragSession().operation));
    ev->accept();

    ev->setAccepted(accepted);
    return accepted;
}

bool QtDesktopWebPageProxy::handleTouchEvent(QTouchEvent* event)
{
    m_webPageProxy->handleTouchEvent(NativeWebTouchEvent(event));
    event->accept();
    return true;
}

void QtDesktopWebPageProxy::timerEvent(QTimerEvent* ev)
{
    int timerId = ev->timerId();
    if (timerId == m_tripleClickTimer.timerId())
        m_tripleClickTimer.stop();
    else
        QObject::timerEvent(ev);
}
