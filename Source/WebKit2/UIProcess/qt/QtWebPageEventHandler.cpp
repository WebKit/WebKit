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
#include "qquickwebpage_p.h"
#include <QDrag>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QtQuick/QQuickCanvas>
#include <QStyleHints>
#include <QTextFormat>
#include <QTouchEvent>
#include <WebCore/DragData.h>
#include <WebCore/Editor.h>

using namespace WebKit;
using namespace WebCore;

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

static inline Qt::DropActions dragOperationToDropActions(unsigned dragOperations)
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

static inline WebCore::DragOperation dropActionToDragOperation(Qt::DropActions actions)
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

QtWebPageEventHandler::QtWebPageEventHandler(WKPageRef pageRef, QQuickWebPage* qmlWebPage)
    : m_webPageProxy(toImpl(pageRef))
    , m_panGestureRecognizer(this)
    , m_pinchGestureRecognizer(this)
    , m_tapGestureRecognizer(this)
    , m_webPage(qmlWebPage)
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
    case QEvent::InputMethod:
        inputMethodEvent(static_cast<QInputMethodEvent*>(ev));
        return false; // Look at comment in qquickwebpage.cpp
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

void QtWebPageEventHandler::inputMethodEvent(QInputMethodEvent* ev)
{
    QString commit = ev->commitString();
    QString composition = ev->preeditString();

    // NOTE: We might want to handle events of one char as special
    // and resend them as key events to make web site completion work.

    int cursorPositionWithinComposition = 0;

    Vector<CompositionUnderline> underlines;

    for (int i = 0; i < ev->attributes().size(); ++i) {
        const QInputMethodEvent::Attribute& attr = ev->attributes().at(i);
        switch (attr.type) {
        case QInputMethodEvent::TextFormat: {
            if (composition.isEmpty())
                break;

            QTextCharFormat textCharFormat = attr.value.value<QTextFormat>().toCharFormat();
            QColor qcolor = textCharFormat.underlineColor();
            Color color = makeRGBA(qcolor.red(), qcolor.green(), qcolor.blue(), qcolor.alpha());
            int start = qMin(attr.start, (attr.start + attr.length));
            int end = qMax(attr.start, (attr.start + attr.length));
            underlines.append(CompositionUnderline(start, end, color, false));
            break;
        }
        case QInputMethodEvent::Cursor:
            if (attr.length)
                cursorPositionWithinComposition = attr.start;
            break;
        // Selection is handled further down.
        default: break;
        }
    }

    if (composition.isEmpty()) {
        int selectionStart = -1;
        int selectionLength = 0;
        for (int i = 0; i < ev->attributes().size(); ++i) {
            const QInputMethodEvent::Attribute& attr = ev->attributes().at(i);
            if (attr.type == QInputMethodEvent::Selection) {
                selectionStart = attr.start;
                selectionLength = attr.length;

                ASSERT_UNUSED(selectionStart, selectionStart >= 0);
                ASSERT_UNUSED(selectionLength, selectionLength >= 0);
                break;
            }
        }

        // FIXME: Confirm the composition here.
    } else {
        ASSERT_UNUSED(cursorPositionWithinComposition, cursorPositionWithinComposition >= 0);

        // FIXME: Set the composition here.
    }

    ev->accept();
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

void QtWebPageEventHandler::startDrag(const WebCore::DragData& dragData, PassRefPtr<ShareableBitmap> dragImage)
{
    QImage dragQImage;
    if (dragImage)
        dragQImage = dragImage->createQImage();
    else if (dragData.platformData() && dragData.platformData()->hasImage())
        dragQImage = qvariant_cast<QImage>(dragData.platformData()->imageData());

    DragOperation dragOperationMask = dragData.draggingSourceOperationMask();
    QMimeData* mimeData = const_cast<QMimeData*>(dragData.platformData());
    Qt::DropActions supportedDropActions = dragOperationToDropActions(dragOperationMask);

    QPoint clientPosition;
    QPoint globalPosition;
    Qt::DropAction actualDropAction = Qt::IgnoreAction;

    if (QWindow* window = m_webPage->canvas()) {
        QDrag* drag = new QDrag(window);
        drag->setPixmap(QPixmap::fromImage(dragQImage));
        drag->setMimeData(mimeData);
        actualDropAction = drag->exec(supportedDropActions);
        globalPosition = QCursor::pos();
        clientPosition = window->mapFromGlobal(globalPosition);
    }

    m_webPageProxy->dragEnded(clientPosition, globalPosition, dropActionToDragOperation(actualDropAction));
}

#include "moc_QtWebPageEventHandler.cpp"
