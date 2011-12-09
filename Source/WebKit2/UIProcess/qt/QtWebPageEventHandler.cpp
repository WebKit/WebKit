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
#include "QtWebPageEventHandler.h"

#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "QtViewportInteractionEngine.h"
#include <QDrag>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QStyleHints>
#include <QTouchEvent>
#include <WebCore/DragData.h>

using namespace WebKit;
using namespace WebCore;

Qt::DropAction QtWebPageEventHandler::dragOperationToDropAction(unsigned dragOperation)
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

Qt::DropActions QtWebPageEventHandler::dragOperationToDropActions(unsigned dragOperations)
{
    Qt::DropActions result = Qt::IgnoreAction;
    if (dragOperations & DragOperationCopy)
        result |= Qt::CopyAction;
    if (dragOperations & DragOperationMove)
        result |= Qt::MoveAction;
    if (dragOperations & DragOperationGeneric)
        result |= Qt::MoveAction;
    if (dragOperations & DragOperationLink)
        result |= Qt::LinkAction;
    return result;
}

WebCore::DragOperation QtWebPageEventHandler::dropActionToDragOperation(Qt::DropActions actions)
{
    unsigned result = 0;
    if (actions & Qt::CopyAction)
        result |= DragOperationCopy;
    if (actions & Qt::MoveAction)
        result |= (DragOperationMove | DragOperationGeneric);
    if (actions & Qt::LinkAction)
        result |= DragOperationLink;
    if (result == (DragOperationCopy | DragOperationMove | DragOperationGeneric | DragOperationLink))
        result = DragOperationEvery;
    return (DragOperation)result;
}

QtWebPageEventHandler::QtWebPageEventHandler(WKPageRef pageRef, WebKit::QtViewportInteractionEngine* viewportInteractionEngine)
    : m_webPageProxy(toImpl(pageRef))
    , m_interactionEngine(viewportInteractionEngine)
    , m_panGestureRecognizer(this)
    , m_pinchGestureRecognizer(this)
    , m_tapGestureRecognizer(this)
    , m_previousClickButton(Qt::NoButton)
    , m_clickCount(0)
{
}

QtWebPageEventHandler::~QtWebPageEventHandler()
{
}

bool QtWebPageEventHandler::handleEvent(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::MouseMove:
        return handleMouseMoveEvent(static_cast<QMouseEvent*>(ev));
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
        // If a MouseButtonDblClick was received then we got a MouseButtonPress before
        // handleMousePressEvent will take care of double clicks.
        return handleMousePressEvent(static_cast<QMouseEvent*>(ev));
    case QEvent::MouseButtonRelease:
        return handleMouseReleaseEvent(static_cast<QMouseEvent*>(ev));
    case QEvent::Wheel:
        return handleWheelEvent(static_cast<QWheelEvent*>(ev));
    case QEvent::HoverLeave:
        return handleHoverLeaveEvent(static_cast<QHoverEvent*>(ev));
    case QEvent::HoverEnter: // Fall-through, for WebKit the distinction doesn't matter.
    case QEvent::HoverMove:
        return handleHoverMoveEvent(static_cast<QHoverEvent*>(ev));
    case QEvent::DragEnter:
        return handleDragEnterEvent(static_cast<QDragEnterEvent*>(ev));
    case QEvent::DragLeave:
        return handleDragLeaveEvent(static_cast<QDragLeaveEvent*>(ev));
    case QEvent::DragMove:
        return handleDragMoveEvent(static_cast<QDragMoveEvent*>(ev));
    case QEvent::Drop:
        return handleDropEvent(static_cast<QDropEvent*>(ev));
    case QEvent::KeyPress:
        return handleKeyPressEvent(static_cast<QKeyEvent*>(ev));
    case QEvent::KeyRelease:
        return handleKeyReleaseEvent(static_cast<QKeyEvent*>(ev));
    case QEvent::FocusIn:
        return handleFocusInEvent(static_cast<QFocusEvent*>(ev));
    case QEvent::FocusOut:
        return handleFocusOutEvent(static_cast<QFocusEvent*>(ev));
    case QEvent::TouchBegin:
    case QEvent::TouchEnd:
    case QEvent::TouchUpdate:
        touchEvent(static_cast<QTouchEvent*>(ev));
        return true;
    }

    // FIXME: Move all common event handling here.
    return false;
}

bool QtWebPageEventHandler::handleMouseMoveEvent(QMouseEvent* ev)
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

    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount*/ 0));

    return ev->isAccepted();
}

bool QtWebPageEventHandler::handleMousePressEvent(QMouseEvent* ev)
{
    if (m_clickTimer.isActive()
        && m_previousClickButton == ev->button()
        && (ev->pos() - m_lastClick).manhattanLength() < qApp->styleHints()->startDragDistance()) {
        m_clickCount++;
    } else {
        m_clickCount = 1;
        m_previousClickButton = ev->button();
    }

    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, m_clickCount));

    m_lastClick = ev->pos();
    m_clickTimer.start(qApp->styleHints()->mouseDoubleClickInterval(), this);
    return ev->isAccepted();
}

bool QtWebPageEventHandler::handleMouseReleaseEvent(QMouseEvent* ev)
{
    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount*/ 0));
    return ev->isAccepted();
}

bool QtWebPageEventHandler::handleWheelEvent(QWheelEvent* ev)
{
    m_webPageProxy->handleWheelEvent(NativeWebWheelEvent(ev));
    // FIXME: Handle whether the page used the wheel event or not.
    if (m_interactionEngine)
        m_interactionEngine->wheelEvent(ev);
    return ev->isAccepted();
}

bool QtWebPageEventHandler::handleHoverLeaveEvent(QHoverEvent* ev)
{
    // To get the correct behavior of mouseout, we need to turn the Leave event of our webview into a mouse move
    // to a very far region.
    QHoverEvent fakeEvent(QEvent::HoverMove, QPoint(INT_MIN, INT_MIN), ev->oldPos());
    fakeEvent.setTimestamp(ev->timestamp());
    return handleHoverMoveEvent(&fakeEvent);
}

bool QtWebPageEventHandler::handleHoverMoveEvent(QHoverEvent* ev)
{
    QMouseEvent me(QEvent::MouseMove, ev->pos(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    me.setAccepted(ev->isAccepted());
    me.setTimestamp(ev->timestamp());

    return handleMouseMoveEvent(&me);
}

bool QtWebPageEventHandler::handleDragEnterEvent(QDragEnterEvent* ev)
{
    m_webPageProxy->resetDragOperation();
    // FIXME: Should not use QCursor::pos()
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), dropActionToDragOperation(ev->possibleActions()));
    m_webPageProxy->dragEntered(&dragData);
    ev->acceptProposedAction();
    return true;
}

bool QtWebPageEventHandler::handleDragLeaveEvent(QDragLeaveEvent* ev)
{
    bool accepted = ev->isAccepted();

    // FIXME: Should not use QCursor::pos()
    DragData dragData(0, IntPoint(), QCursor::pos(), DragOperationNone);
    m_webPageProxy->dragExited(&dragData);
    m_webPageProxy->resetDragOperation();

    ev->setAccepted(accepted);
    return accepted;
}

bool QtWebPageEventHandler::handleDragMoveEvent(QDragMoveEvent* ev)
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

bool QtWebPageEventHandler::handleDropEvent(QDropEvent* ev)
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

void QtWebPageEventHandler::handleSingleTapEvent(const QTouchEvent::TouchPoint& point)
{
    WebGestureEvent gesture(WebEvent::GestureSingleTap, point.pos().toPoint(), point.screenPos().toPoint(), WebEvent::Modifiers(0), 0);
    m_webPageProxy->handleGestureEvent(gesture);
}

void QtWebPageEventHandler::handleDoubleTapEvent(const QTouchEvent::TouchPoint& point)
{
    m_webPageProxy->findZoomableAreaForPoint(point.pos().toPoint());
}

void QtWebPageEventHandler::timerEvent(QTimerEvent* ev)
{
    int timerId = ev->timerId();
    if (timerId == m_clickTimer.timerId())
        m_clickTimer.stop();
    else
        QObject::timerEvent(ev);
}

bool QtWebPageEventHandler::handleKeyPressEvent(QKeyEvent* ev)
{
    m_webPageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(ev));
    return true;
}

bool QtWebPageEventHandler::handleKeyReleaseEvent(QKeyEvent* ev)
{
    m_webPageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(ev));
    return true;
}

bool QtWebPageEventHandler::handleFocusInEvent(QFocusEvent*)
{
    m_webPageProxy->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

bool QtWebPageEventHandler::handleFocusOutEvent(QFocusEvent*)
{
    m_webPageProxy->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

void QtWebPageEventHandler::setViewportInteractionEngine(QtViewportInteractionEngine* engine)
{
    m_interactionEngine = engine;
}

void QtWebPageEventHandler::touchEvent(QTouchEvent* event)
{
#if ENABLE(TOUCH_EVENTS)
    m_webPageProxy->handleTouchEvent(NativeWebTouchEvent(event));
    event->accept();
#else
    ASSERT_NOT_REACHED();
    ev->ignore();
#endif
}

void QtWebPageEventHandler::resetGestureRecognizers()
{
    m_panGestureRecognizer.reset();
    m_pinchGestureRecognizer.reset();
    m_tapGestureRecognizer.reset();
}

void QtWebPageEventHandler::doneWithTouchEvent(const NativeWebTouchEvent& event, bool wasEventHandled)
{
    if (!m_interactionEngine)
        return;

    if (wasEventHandled || event.type() == WebEvent::TouchCancel) {
        resetGestureRecognizers();
        return;
    }

    const QTouchEvent* ev = event.nativeEvent();

    switch (ev->type()) {
    case QEvent::TouchBegin:
        ASSERT(!m_interactionEngine->panGestureActive());
        ASSERT(!m_interactionEngine->pinchGestureActive());

        // The interaction engine might still be animating kinetic scrolling or a scale animation
        // such as double-tap to zoom or the bounce back effect. A touch stops the kinetic scrolling
        // where as it does not stop the scale animation.
        if (m_interactionEngine->scrollAnimationActive())
            m_interactionEngine->interruptScrollAnimation();
        break;
    case QEvent::TouchUpdate:
        // The scale animation can only be interrupted by a pinch gesture, which will then take over.
        if (m_interactionEngine->scaleAnimationActive() && m_pinchGestureRecognizer.isRecognized())
            m_interactionEngine->interruptScaleAnimation();
        break;
    default:
        break;
    }

    // If the scale animation is active we don't pass the event to the recognizers. In the future
    // we would want to queue the event here and repost then when the animation ends.
    if (m_interactionEngine->scaleAnimationActive())
        return;

    // Convert the event timestamp from second to millisecond.
    qint64 eventTimestampMillis = static_cast<qint64>(event.timestamp() * 1000);
    m_panGestureRecognizer.recognize(ev, eventTimestampMillis);
    m_pinchGestureRecognizer.recognize(ev);

    if (m_panGestureRecognizer.isRecognized() || m_pinchGestureRecognizer.isRecognized())
        m_tapGestureRecognizer.reset();
    else {
        const QTouchEvent* ev = event.nativeEvent();
        m_tapGestureRecognizer.recognize(ev, eventTimestampMillis);
    }
}

void QtWebPageEventHandler::didFindZoomableArea(const IntPoint& target, const IntRect& area)
{
    if (!m_interactionEngine)
        return;

    // FIXME: As the find method might not respond immediately during load etc,
    // we should ignore all but the latest request.
    m_interactionEngine->zoomToAreaGestureEnded(QPointF(target), QRectF(area));
}

void QtWebPageEventHandler::focusEditableArea(const IntRect& caret, const IntRect& area)
{
    if (!m_interactionEngine)
        return;

    m_interactionEngine->focusEditableArea(QRectF(caret), QRectF(area));
}

#include "moc_QtWebPageEventHandler.cpp"
