/*
 * Copyright (C) 2006 Trolltech ASA
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
#include <jsobjects.h>
#include <qwebpage.h>
#include <qwebframe.h>
#include <qevent.h>
#include <qapplication.h>

class HackWebFrame : public QWebFrame
{
public:
    void mousePressEvent(QMouseEvent *e) {
        QWebFrame::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) {
        QWebFrame::mouseReleaseEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e) {
        QWebFrame::mouseMoveEvent(e);
    }

protected:
    HackWebFrame(QWebPage *parent, QWebFrameData *frameData) : QWebFrame(parent, frameData) {}
    HackWebFrame(QWebFrame *parent, QWebFrameData *frameData) : QWebFrame(parent, frameData) {}
    ~HackWebFrame() {}
};

LayoutTestController::LayoutTestController() : QObject()
{
    m_isLoading = true;
    m_textDump = false;
    m_waitForDone = false;
    m_timeoutTimer = 0;
    m_topLoadingFrame = 0;
}

void LayoutTestController::reset()
{
    m_isLoading = true;
    m_textDump = false;
    m_waitForDone = false;
    if (m_timeoutTimer) {
        killTimer(m_timeoutTimer);
        m_timeoutTimer = 0;
    }
    m_topLoadingFrame = 0;
}

void LayoutTestController::maybeDump(bool ok)
{
    QWebFrame *frame = qobject_cast<QWebFrame*>(sender());
    if (frame != m_topLoadingFrame)
        return;

    m_topLoadingFrame = 0;

    if (!ok || !shouldWaitUntilDone()) {
        emit done();
        m_isLoading = false;
    }
}

void LayoutTestController::waitUntilDone()
{
    //qDebug() << ">>>>waitForDone";
    m_waitForDone = true;
    m_timeoutTimer = startTimer(5000);
}

void LayoutTestController::notifyDone()
{
    //qDebug() << ">>>>notifyDone";
    if (!m_timeoutTimer)
        return;
    killTimer(m_timeoutTimer);
    m_timeoutTimer = 0;
    emit done();
    m_isLoading = false;
}

void LayoutTestController::dumpEditingCallbacks()
{
    //qDebug() << ">>>dumpEditingCallbacks";
}

void LayoutTestController::queueReload()
{
    //qDebug() << ">>>queueReload";
}

void LayoutTestController::provisionalLoad()
{
    QWebFrame *frame = qobject_cast<QWebFrame*>(sender());
    if (!m_topLoadingFrame && m_isLoading)
        m_topLoadingFrame = frame;
}

void LayoutTestController::timerEvent(QTimerEvent *)
{
    qDebug() << ">>>>>>>>>>>>> timeout";
    notifyDone();
}


EventSender::EventSender(QWebPage *parent)
    : QObject(parent)
{
    m_page = parent;
}

void EventSender::mouseDown()
{
    QWebFrame *frame = frameUnderMouse();
//     qDebug() << "EventSender::mouseDown" << frame;
    if (!frame)
        return;
    QMouseEvent event(QEvent::MouseButtonPress, m_mousePos - frame->pos(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    static_cast<HackWebFrame *>(frame)->mousePressEvent(&event);
}

void EventSender::mouseUp()
{
    QWebFrame *frame = frameUnderMouse();
//     qDebug() << "EventSender::mouseUp" << frame;
    if (!frame)
        return;
    QMouseEvent event(QEvent::MouseButtonRelease, m_mousePos - frame->pos(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    static_cast<HackWebFrame *>(frame)->mouseReleaseEvent(&event);
}

void EventSender::mouseMoveTo(int x, int y)
{
    QWebFrame *frame = frameUnderMouse();
//     qDebug() << "EventSender::mouseMoveTo" << x << y;
    m_mousePos = QPoint(x, y);
    QMouseEvent event(QEvent::MouseMove, m_mousePos - frame->pos(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    static_cast<HackWebFrame *>(frame)->mouseMoveEvent(&event);
}

void EventSender::leapForward(int ms)
{
    m_timeLeap += ms;
    qDebug() << "EventSender::leapForward" << ms;
}

void EventSender::keyDown(const QString &string, const QStringList &modifiers)
{
    qDebug() << "EventSender::keyDown" << string << modifiers;
}

QWebFrame *EventSender::frameUnderMouse() const
{
    QWebFrame *frame = m_page->mainFrame();

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
