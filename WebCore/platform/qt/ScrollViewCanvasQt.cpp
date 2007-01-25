/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "ScrollViewCanvasQt.h"
#include "ScrollViewCanvasQt.moc"

#include "EventHandler.h"
#include "FrameQt.h"
#include "FrameView.h"
#include "TypingCommand.h"
#include "KeyboardCodes.h"
#include "GraphicsContext.h"
#include "SelectionController.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <qdebug.h>


//this output ms that it takes WebKit to render in each paint event
//#define QWEBKIT_TIME_RENDERING
#ifdef QWEBKIT_TIME_RENDERING
#include <QDebug>
#include <QTime>
#endif

namespace WebCore {

ScrollViewCanvasQt::ScrollViewCanvasQt(FrameView* frameView, QWidget* parent)
    : QWidget(0),
      m_frameView(frameView),
      m_eventHandler(frameView->frame()->eventHandler())
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    resize(500, 500);
}


void ScrollViewCanvasQt::paintEvent(QPaintEvent* ev)
{
    if (!m_frameView || !m_frameView->frame()->renderer())
        return;

#ifdef QWEBKIT_TIME_RENDERING
    QTime time;
    time.start();
#endif
        
    QRect clip = ev->rect();
    if (m_frameView->layoutPending()) {
        //qDebug()<<"pending "<<m_frameView->layoutPending()
        //        <<" delayed = "<<m_frameView->haveDelayedLayoutScheduled();
        m_frameView->layout();
    }
        
    QPainter p(this);
    GraphicsContext ctx(&p);
    m_frameView->frame()->paint(&ctx, clip);
    p.end();

#ifdef    QWEBKIT_TIME_RENDERING
    int elapsed = time.elapsed();
    qDebug()<<"paint event on "<<clip<<", took to render =  "<<elapsed;
#endif
}

void ScrollViewCanvasQt::mouseMoveEvent(QMouseEvent* ev)
{
    if (!m_frameView)
        return;

    m_frameView->handleMouseMoveEvent(PlatformMouseEvent(ev, 0));
}

void ScrollViewCanvasQt::mousePressEvent(QMouseEvent* ev)
{
    if (!m_eventHandler)
        return;

    m_eventHandler->handleMousePressEvent(PlatformMouseEvent(ev, 1));
}

void ScrollViewCanvasQt::mouseReleaseEvent(QMouseEvent* ev)
{
    if (!m_frameView) 
        return;

    m_frameView->handleMouseReleaseEvent(PlatformMouseEvent(ev, 0));
}

void ScrollViewCanvasQt::keyPressEvent(QKeyEvent* ev)
{
    handleKeyEvent(ev, false);
}

void ScrollViewCanvasQt::keyReleaseEvent(QKeyEvent* ev)
{
    handleKeyEvent(ev, true);
}

void ScrollViewCanvasQt::handleKeyEvent(QKeyEvent* ev, bool isKeyUp)
{
    PlatformKeyboardEvent kevent(ev, isKeyUp);

    if (!m_frameView)
        return;

    FrameQt* frame = static_cast<FrameQt*>(m_frameView->frame());
    if (!frame)
        return;

    bool handled = frame->keyEvent(kevent);

    if (!handled && !kevent.isKeyUp()) {
        Node* start = frame->selectionController()->start().node();
        if (start && start->isContentEditable()) {
            switch(kevent.WindowsKeyCode()) {
                case VK_BACK:
                    TypingCommand::deleteKeyPressed(frame->document());
                    break;
                case VK_DELETE:
                    TypingCommand::forwardDeleteKeyPressed(frame->document());
                    break;
                case VK_LEFT:
                    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::LEFT, CharacterGranularity);
                    break;
                case VK_RIGHT:
                    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::RIGHT, CharacterGranularity);
                    break;
                case VK_UP:
                    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, ParagraphGranularity);
                    break;
                case VK_DOWN:
                    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, ParagraphGranularity);
                    break;
                default:
                    TypingCommand::insertText(frame->document(), kevent.text(), false);

            }

            handled = true;
        }

        // FIXME: doScroll stuff()!
    }
}

}

void WebCore::ScrollViewCanvasQt::wheelEvent(QWheelEvent* e)
{
    PlatformWheelEvent wkEvent(e);
    bool accepted = false;
    if (m_eventHandler)
        accepted = m_eventHandler->handleWheelEvent(wkEvent);
    
    e->setAccepted(accepted);
    if (!accepted)
        QWidget::wheelEvent(e);
        
}
void WebCore::ScrollViewCanvasQt::dragEnterEvent(QDragEnterEvent *)
{
    
}
void WebCore::ScrollViewCanvasQt::dragLeaveEvent(QDragLeaveEvent *)
{
    
}
void WebCore::ScrollViewCanvasQt::dragMoveEvent(QDragMoveEvent *)
{
    
}
// vim: ts=4 sw=4 et
