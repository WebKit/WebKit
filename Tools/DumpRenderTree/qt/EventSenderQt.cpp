/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "EventSenderQt.h"

#include <QGraphicsSceneMouseEvent>
#include <QtTest/QtTest>

#define KEYCODE_DEL         127
#define KEYCODE_BACKSPACE   8
#define KEYCODE_LEFTARROW   0xf702
#define KEYCODE_RIGHTARROW  0xf703
#define KEYCODE_UPARROW     0xf700
#define KEYCODE_DOWNARROW   0xf701

// Ports like Gtk and Windows expose a different approach for their zooming
// API if compared to Qt: they have specific methods for zooming in and out,
// as well as a settable zoom factor, while Qt has only a 'setZoomValue' method.
// Hence Qt DRT adopts a fixed zoom-factor (1.2) for compatibility.
#define ZOOM_STEP           1.2

#define DRT_MESSAGE_DONE (QEvent::User + 1)

struct DRTEventQueue {
    QEvent* m_event;
    int m_delay;
};

static DRTEventQueue eventQueue[1024];
static unsigned endOfQueue;
static unsigned startOfQueue;

EventSender::EventSender(QWebPage* parent)
    : QObject(parent)
{
    m_page = parent;
    m_mouseButtonPressed = false;
    m_drag = false;
    memset(eventQueue, 0, sizeof(eventQueue));
    endOfQueue = 0;
    startOfQueue = 0;
    m_eventLoop = 0;
    m_currentButton = 0;
    resetClickCount();
    m_page->view()->installEventFilter(this);
    // So that we can match Scrollbar::pixelsPerLineStep() in WheelEventQt.cpp and
    // pass fast/events/platform-wheelevent-in-scrolling-div.html
    QApplication::setWheelScrollLines(2);
}

static Qt::KeyboardModifiers getModifiers(const QStringList& modifiers)
{
    Qt::KeyboardModifiers modifs = 0;
    for (int i = 0; i < modifiers.size(); ++i) {
        const QString& m = modifiers.at(i);
        if (m == "ctrlKey")
            modifs |= Qt::ControlModifier;
        else if (m == "shiftKey")
            modifs |= Qt::ShiftModifier;
        else if (m == "altKey")
            modifs |= Qt::AltModifier;
        else if (m == "metaKey")
            modifs |= Qt::MetaModifier;
    }
    return modifs;
}

void EventSender::mouseDown(int button, const QStringList& modifiers)
{
    Qt::KeyboardModifiers modifs = getModifiers(modifiers);
    Qt::MouseButton mouseButton;
    switch (button) {
    case 0:
        mouseButton = Qt::LeftButton;
        break;
    case 1:
        mouseButton = Qt::MidButton;
        break;
    case 2:
        mouseButton = Qt::RightButton;
        break;
    case 3:
        // fast/events/mouse-click-events expects the 4th button to be treated as the middle button
        mouseButton = Qt::MidButton;
        break;
    default:
        mouseButton = Qt::LeftButton;
        break;
    }

    // only consider a click to count, an event originated by the
    // same previous button and at the same position.
    if (m_currentButton == button
        && m_mousePos == m_clickPos
        && m_clickTimer.isActive())
        m_clickCount++;
    else
        m_clickCount = 1;

    m_currentButton = button;
    m_clickPos = m_mousePos;
    m_mouseButtons |= mouseButton;

//     qDebug() << "EventSender::mouseDown" << frame;
    QEvent* event;
    if (isGraphicsBased()) {
        event = createGraphicsSceneMouseEvent((m_clickCount == 2) ?
                    QEvent::GraphicsSceneMouseDoubleClick : QEvent::GraphicsSceneMousePress,
                    m_mousePos, m_mousePos, mouseButton, m_mouseButtons, modifs);
    } else {
        event = new QMouseEvent((m_clickCount == 2) ? QEvent::MouseButtonDblClick :
                    QEvent::MouseButtonPress, m_mousePos, m_mousePos,
                    mouseButton, m_mouseButtons, modifs);
    }

    sendOrQueueEvent(event);

    m_clickTimer.start(QApplication::doubleClickInterval(), this);
}

void EventSender::mouseUp(int button)
{
    Qt::MouseButton mouseButton;
    switch (button) {
    case 0:
        mouseButton = Qt::LeftButton;
        break;
    case 1:
        mouseButton = Qt::MidButton;
        break;
    case 2:
        mouseButton = Qt::RightButton;
        break;
    case 3:
        // fast/events/mouse-click-events expects the 4th button to be treated as the middle button
        mouseButton = Qt::MidButton;
        break;
    default:
        mouseButton = Qt::LeftButton;
        break;
    }

    m_mouseButtons &= ~mouseButton;

//     qDebug() << "EventSender::mouseUp" << frame;
    QEvent* event;
    if (isGraphicsBased()) {
        event = createGraphicsSceneMouseEvent(QEvent::GraphicsSceneMouseRelease,
                    m_mousePos, m_mousePos, mouseButton, m_mouseButtons, Qt::NoModifier);
    } else {
        event = new QMouseEvent(QEvent::MouseButtonRelease,
                    m_mousePos, m_mousePos, mouseButton, m_mouseButtons, Qt::NoModifier);
    }

    sendOrQueueEvent(event);
}

void EventSender::mouseMoveTo(int x, int y)
{
//     qDebug() << "EventSender::mouseMoveTo" << x << y;
    m_mousePos = QPoint(x, y);

    QEvent* event;
    if (isGraphicsBased()) {
        event = createGraphicsSceneMouseEvent(QEvent::GraphicsSceneMouseMove,
                    m_mousePos, m_mousePos, Qt::NoButton, m_mouseButtons, Qt::NoModifier);
    } else {
        event = new QMouseEvent(QEvent::MouseMove,
                    m_mousePos, m_mousePos, Qt::NoButton, m_mouseButtons, Qt::NoModifier);
    }

    sendOrQueueEvent(event);
}

#ifndef QT_NO_WHEELEVENT
void EventSender::mouseScrollBy(int x, int y)
{
    continuousMouseScrollBy((x*120), (y*120));
}

void EventSender::continuousMouseScrollBy(int x, int y)
{
    // continuousMouseScrollBy() mimics devices that send fine-grained scroll events where the 'delta' specified is not the usual
    // multiple of 120. See http://doc.qt.nokia.com/4.6/qwheelevent.html#delta for a good explanation of this.
    if (x) {
        QEvent* event;
        if (isGraphicsBased()) {
            event = createGraphicsSceneWheelEvent(QEvent::GraphicsSceneWheel,
                        m_mousePos, m_mousePos, x, Qt::NoModifier, Qt::Horizontal);
        } else
            event = new QWheelEvent(m_mousePos, m_mousePos, x, m_mouseButtons, Qt::NoModifier, Qt::Horizontal);

        sendOrQueueEvent(event);
    }
    if (y) {
        QEvent* event;
        if (isGraphicsBased()) {
            event = createGraphicsSceneWheelEvent(QEvent::GraphicsSceneWheel,
                        m_mousePos, m_mousePos, y, Qt::NoModifier, Qt::Vertical);
        } else
            event = new QWheelEvent(m_mousePos, m_mousePos, y, m_mouseButtons, Qt::NoModifier, Qt::Vertical);

        sendOrQueueEvent(event);
    }
}
#endif

void EventSender::leapForward(int ms)
{
    eventQueue[endOfQueue].m_delay = ms;
    //qDebug() << "EventSender::leapForward" << ms;
}

void EventSender::keyDown(const QString& string, const QStringList& modifiers, unsigned int location)
{
    QString s = string;
    Qt::KeyboardModifiers modifs = getModifiers(modifiers);
    if (location == 3)
        modifs |= Qt::KeypadModifier;
    int code = 0;
    if (string.length() == 1) {
        code = string.unicode()->unicode();
        //qDebug() << ">>>>>>>>> keyDown" << code << (char)code;
        // map special keycodes used by the tests to something that works for Qt/X11
        if (code == '\r') {
            code = Qt::Key_Return;
        } else if (code == '\t') {
            code = Qt::Key_Tab;
            if (modifs == Qt::ShiftModifier)
                code = Qt::Key_Backtab;
            s = QString();
        } else if (code == KEYCODE_DEL || code == KEYCODE_BACKSPACE) {
            code = Qt::Key_Backspace;
            if (modifs == Qt::AltModifier)
                modifs = Qt::ControlModifier;
            s = QString();
        } else if (code == 'o' && modifs == Qt::ControlModifier) {
            // Mimic the emacs ctrl-o binding on Mac by inserting a paragraph
            // separator and then putting the cursor back to its original
            // position. Allows us to pass emacs-ctrl-o.html
            s = QLatin1String("\n");
            code = '\n';
            modifs = 0;
            QKeyEvent event(QEvent::KeyPress, code, modifs, s);
            sendEvent(m_page, &event);
            QKeyEvent event2(QEvent::KeyRelease, code, modifs, s);
            sendEvent(m_page, &event2);
            s = QString();
            code = Qt::Key_Left;
        } else if (code == 'y' && modifs == Qt::ControlModifier) {
            s = QLatin1String("c");
            code = 'c';
        } else if (code == 'k' && modifs == Qt::ControlModifier) {
            s = QLatin1String("x");
            code = 'x';
        } else if (code == 'a' && modifs == Qt::ControlModifier) {
            s = QString();
            code = Qt::Key_Home;
            modifs = 0;
        } else if (code == KEYCODE_LEFTARROW) {
            s = QString();
            code = Qt::Key_Left;
            if (modifs & Qt::MetaModifier) {
                code = Qt::Key_Home;
                modifs &= ~Qt::MetaModifier;
            }
        } else if (code == KEYCODE_RIGHTARROW) {
            s = QString();
            code = Qt::Key_Right;
            if (modifs & Qt::MetaModifier) {
                code = Qt::Key_End;
                modifs &= ~Qt::MetaModifier;
            }
        } else if (code == KEYCODE_UPARROW) {
            s = QString();
            code = Qt::Key_Up;
            if (modifs & Qt::MetaModifier) {
                code = Qt::Key_PageUp;
                modifs &= ~Qt::MetaModifier;
            }
        } else if (code == KEYCODE_DOWNARROW) {
            s = QString();
            code = Qt::Key_Down;
            if (modifs & Qt::MetaModifier) {
                code = Qt::Key_PageDown;
                modifs &= ~Qt::MetaModifier;
            }
        } else if (code == 'a' && modifs == Qt::ControlModifier) {
            s = QString();
            code = Qt::Key_Home;
            modifs = 0;
        } else
            code = string.unicode()->toUpper().unicode();
    } else {
        //qDebug() << ">>>>>>>>> keyDown" << string;

        if (string.startsWith(QLatin1Char('F')) && string.count() <= 3) {
            s = s.mid(1);
            int functionKey = s.toInt();
            Q_ASSERT(functionKey >= 1 && functionKey <= 35);
            code = Qt::Key_F1 + (functionKey - 1);
        // map special keycode strings used by the tests to something that works for Qt/X11
        } else if (string == QLatin1String("leftArrow")) {
            s = QString();
            code = Qt::Key_Left;
        } else if (string == QLatin1String("rightArrow")) {
            s = QString();
            code = Qt::Key_Right;
        } else if (string == QLatin1String("upArrow")) {
            s = QString();
            code = Qt::Key_Up;
        } else if (string == QLatin1String("downArrow")) {
            s = QString();
            code = Qt::Key_Down;
        } else if (string == QLatin1String("pageUp")) {
            s = QString();
            code = Qt::Key_PageUp;
        } else if (string == QLatin1String("pageDown")) {
            s = QString();
            code = Qt::Key_PageDown;
        } else if (string == QLatin1String("home")) {
            s = QString();
            code = Qt::Key_Home;
        } else if (string == QLatin1String("end")) {
            s = QString();
            code = Qt::Key_End;
        } else if (string == QLatin1String("insert")) {
            s = QString();
            code = Qt::Key_Insert;
        } else if (string == QLatin1String("delete")) {
            s = QString();
            code = Qt::Key_Delete;
        } else if (string == QLatin1String("printScreen")) {
            s = QString();
            code = Qt::Key_Print;
        } else if (string == QLatin1String("menu")) {
            s = QString();
            code = Qt::Key_Menu;
        }
    }
    QKeyEvent event(QEvent::KeyPress, code, modifs, s);
    sendEvent(m_page, &event);
    QKeyEvent event2(QEvent::KeyRelease, code, modifs, s);
    sendEvent(m_page, &event2);
}

QStringList EventSender::contextClick()
{
    QMouseEvent event(QEvent::MouseButtonPress, m_mousePos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    sendEvent(m_page, &event);
    QMouseEvent event2(QEvent::MouseButtonRelease, m_mousePos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    sendEvent(m_page, &event2);

    if (isGraphicsBased()) {
        QGraphicsSceneContextMenuEvent ctxEvent(QEvent::GraphicsSceneContextMenu);
        ctxEvent.setReason(QGraphicsSceneContextMenuEvent::Mouse);
        ctxEvent.setPos(m_mousePos);
        WebCore::WebViewGraphicsBased* view = qobject_cast<WebCore::WebViewGraphicsBased*>(m_page->view());
        if (view)
            sendEvent(view->graphicsView(), &ctxEvent);
    } else {
        QContextMenuEvent ctxEvent(QContextMenuEvent::Mouse, m_mousePos);
        sendEvent(m_page->view(), &ctxEvent);
    }
    return DumpRenderTreeSupportQt::contextMenu(m_page);
}

void EventSender::scheduleAsynchronousClick()
{
    QMouseEvent* event = new QMouseEvent(QEvent::MouseButtonPress, m_mousePos, Qt::LeftButton, Qt::RightButton, Qt::NoModifier);
    postEvent(m_page, event);
    QMouseEvent* event2 = new QMouseEvent(QEvent::MouseButtonRelease, m_mousePos, Qt::LeftButton, Qt::RightButton, Qt::NoModifier);
    postEvent(m_page, event2);
}

void EventSender::addTouchPoint(int x, int y)
{
    const int id = m_touchPoints.isEmpty() ? 0 : m_touchPoints.last().id() + 1;
    const QPointF pos(x, y);
    QTouchEvent::TouchPoint point(id);
    point.setPos(pos);
    point.setStartPos(pos);
    point.setState(Qt::TouchPointPressed);
    m_touchPoints.append(point);
}

void EventSender::updateTouchPoint(int index, int x, int y)
{
    if (index < 0 || index >= m_touchPoints.count())
        return;

    QTouchEvent::TouchPoint &p = m_touchPoints[index];
    p.setPos(QPointF(x, y));
    p.setState(Qt::TouchPointMoved);
}

void EventSender::setTouchModifier(const QString &modifier, bool enable)
{
    Qt::KeyboardModifier mod = Qt::NoModifier;
    if (!modifier.compare(QLatin1String("shift"), Qt::CaseInsensitive))
        mod = Qt::ShiftModifier;
    else if (!modifier.compare(QLatin1String("alt"), Qt::CaseInsensitive))
        mod = Qt::AltModifier;
    else if (!modifier.compare(QLatin1String("meta"), Qt::CaseInsensitive))
        mod = Qt::MetaModifier;
    else if (!modifier.compare(QLatin1String("ctrl"), Qt::CaseInsensitive))
        mod = Qt::ControlModifier;

    if (enable)
        m_touchModifiers |= mod;
    else
        m_touchModifiers &= ~mod;
}

void EventSender::touchStart()
{
    if (!m_touchActive) {
        sendTouchEvent(QEvent::TouchBegin);
        m_touchActive = true;
    } else
        sendTouchEvent(QEvent::TouchUpdate);
}

void EventSender::touchMove()
{
    sendTouchEvent(QEvent::TouchUpdate);
}

void EventSender::touchEnd()
{
    for (int i = 0; i < m_touchPoints.count(); ++i)
        if (m_touchPoints[i].state() != Qt::TouchPointReleased) {
            sendTouchEvent(QEvent::TouchUpdate);
            return;
        }
    sendTouchEvent(QEvent::TouchEnd);
    m_touchActive = false;
}

void EventSender::clearTouchPoints()
{
    m_touchPoints.clear();
    m_touchModifiers = Qt::KeyboardModifiers();
    m_touchActive = false;
}

void EventSender::releaseTouchPoint(int index)
{
    if (index < 0 || index >= m_touchPoints.count())
        return;

    m_touchPoints[index].setState(Qt::TouchPointReleased);
}

void EventSender::sendTouchEvent(QEvent::Type type)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    static QTouchDevice* device = 0;
    if (!device) {
        device = new QTouchDevice;
        device->setType(QTouchDevice::TouchScreen);
        QWindowSystemInterface::registerTouchDevice(device);
    }

    QTouchEvent event(type, device, m_touchModifiers);
#else
    QTouchEvent event(type, QTouchEvent::TouchScreen, m_touchModifiers);
#endif
    event.setTouchPoints(m_touchPoints);
    sendEvent(m_page, &event);
    QList<QTouchEvent::TouchPoint>::Iterator it = m_touchPoints.begin();
    while (it != m_touchPoints.end()) {
        if (it->state() == Qt::TouchPointReleased)
            it = m_touchPoints.erase(it);
        else {
            it->setState(Qt::TouchPointStationary);
            ++it;
        }
    }
}

void EventSender::zoomPageIn()
{
    if (QWebFrame* frame = m_page->mainFrame())
        frame->setZoomFactor(frame->zoomFactor() * ZOOM_STEP);
}

void EventSender::zoomPageOut()
{
    if (QWebFrame* frame = m_page->mainFrame())
        frame->setZoomFactor(frame->zoomFactor() / ZOOM_STEP);
}

void EventSender::textZoomIn()
{
    if (QWebFrame* frame = m_page->mainFrame())
        frame->setTextSizeMultiplier(frame->textSizeMultiplier() * ZOOM_STEP);
}

void EventSender::textZoomOut()
{
    if (QWebFrame* frame = m_page->mainFrame())
        frame->setTextSizeMultiplier(frame->textSizeMultiplier() / ZOOM_STEP);
}

void EventSender::scalePageBy(float scaleFactor, float x, float y)
{
    if (QWebFrame* frame = m_page->mainFrame())
        DumpRenderTreeSupportQt::scalePageBy(frame, scaleFactor, QPoint(x, y));
}

QWebFrame* EventSender::frameUnderMouse() const
{
    QWebFrame* frame = m_page->mainFrame();

redo:
    QList<QWebFrame*> children = frame->childFrames();
    for (int i = 0; i < children.size(); ++i) {
        if (children.at(i)->geometry().contains(m_mousePos)) {
            frame = children.at(i);
            goto redo;
        }
    }
    if (frame->geometry().contains(m_mousePos))
        return frame;
    return 0;
}

void EventSender::sendOrQueueEvent(QEvent* event)
{
    // Mouse move events are queued if
    // 1. A previous event was queued.
    // 2. A delay was set-up by leapForward().
    // 3. A call to mouseMoveTo while the mouse button is pressed could initiate a drag operation, and that does not return until mouseUp is processed.
    // To be safe and avoid a deadlock, this event is queued.
    if (endOfQueue == startOfQueue && !eventQueue[endOfQueue].m_delay && (!(m_mouseButtonPressed && (m_eventLoop && event->type() == QEvent::MouseButtonRelease)))) {
        sendEvent(m_page->view(), event);
        delete event;
        return;
    }
    eventQueue[endOfQueue++].m_event = event;
    eventQueue[endOfQueue].m_delay = 0;
    replaySavedEvents(event->type() != QEvent::MouseMove);
}

void EventSender::replaySavedEvents(bool flush)
{
    if (startOfQueue < endOfQueue) {
        // First send all the events that are ready to be sent
        while (!eventQueue[startOfQueue].m_delay && startOfQueue < endOfQueue) {
            QEvent* ev = eventQueue[startOfQueue++].m_event;
            postEvent(m_page->view(), ev);
        }
        if (startOfQueue == endOfQueue) {
            // Reset the queue
            startOfQueue = 0;
            endOfQueue = 0;
        } else {
            QTest::qWait(eventQueue[startOfQueue].m_delay);
            eventQueue[startOfQueue].m_delay = 0;
        }
    }
    if (!flush)
        return;

    // Send a marker event, it will tell us when it is safe to exit the new event loop
    QEvent* drtEvent = new QEvent((QEvent::Type)DRT_MESSAGE_DONE);
    QApplication::postEvent(m_page->view(), drtEvent);

    // Start an event loop for async handling of Drag & Drop
    m_eventLoop = new QEventLoop;
    m_eventLoop->exec();
    delete m_eventLoop;
    m_eventLoop = 0;
}

bool EventSender::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_page->view())
        return false;
    switch (event->type()) {
    case QEvent::Leave:
        return true;
    case QEvent::MouseButtonPress:
    case QEvent::GraphicsSceneMousePress:
        m_mouseButtonPressed = true;
        break;
    case QEvent::MouseMove:
    case QEvent::GraphicsSceneMouseMove:
        if (m_mouseButtonPressed)
            m_drag = true;
        break;
    case QEvent::MouseButtonRelease:
    case QEvent::GraphicsSceneMouseRelease:
        m_mouseButtonPressed = false;
        m_drag = false;
        break;
    case DRT_MESSAGE_DONE:
        m_eventLoop->exit();
        return true;
    }
    return false;
}

void EventSender::timerEvent(QTimerEvent* ev)
{
    m_clickTimer.stop();
}

QGraphicsSceneMouseEvent* EventSender::createGraphicsSceneMouseEvent(QEvent::Type type, const QPoint& pos, const QPoint& screenPos, Qt::MouseButton button, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    QGraphicsSceneMouseEvent* event;
    event = new QGraphicsSceneMouseEvent(type);
    event->setPos(pos);
    event->setScreenPos(screenPos);
    event->setButton(button);
    event->setButtons(buttons);
    event->setModifiers(modifiers);

    return event;
}

QGraphicsSceneWheelEvent* EventSender::createGraphicsSceneWheelEvent(QEvent::Type type, const QPoint& pos, const QPoint& screenPos, int delta, Qt::KeyboardModifiers modifiers, Qt::Orientation orientation)
{
    QGraphicsSceneWheelEvent* event;
    event = new QGraphicsSceneWheelEvent(type);
    event->setPos(pos);
    event->setScreenPos(screenPos);
    event->setDelta(delta);
    event->setModifiers(modifiers);
    event->setOrientation(orientation);

    return event;
}

void EventSender::sendEvent(QObject* receiver, QEvent* event)
{
    if (WebCore::WebViewGraphicsBased* view = qobject_cast<WebCore::WebViewGraphicsBased*>(receiver))
        view->scene()->sendEvent(view->graphicsView(), event);
    else
        QApplication::sendEvent(receiver, event);
}

void EventSender::postEvent(QObject* receiver, QEvent* event)
{
    // QGraphicsScene does not have a postEvent method, so send the event in this case
    // and delete it after that.
    if (WebCore::WebViewGraphicsBased* view = qobject_cast<WebCore::WebViewGraphicsBased*>(receiver)) {
        view->scene()->sendEvent(view->graphicsView(), event);
        delete event;
    } else
        QApplication::postEvent(receiver, event); // event deleted by the system
}
