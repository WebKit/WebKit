/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
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
#include "PlatformMouseEvent.h"

#include "SystemTime.h"

#include <QMouseEvent>

namespace WebCore {

PlatformMouseEvent::PlatformMouseEvent(QMouseEvent* event, int clickCount)
{
    m_position = IntPoint(event->pos());
    m_globalPosition = IntPoint(event->globalPos());
    
    m_timestamp = WebCore::currentTime();
    switch(event->type()) {
    case QEvent::MouseMove:
        m_eventType = MouseEventMoved;
        break;
    case QEvent::MouseButtonPress:
        m_eventType = MouseEventPressed;
        break;
    case QEvent::MouseButtonRelease:
        m_eventType = MouseEventReleased;
        break;
    default:
        m_eventType = MouseEventMoved;
    }

    if (event->button() == Qt::LeftButton || (event->buttons() & Qt::LeftButton))
        m_button = LeftButton;
    else if (event->button() == Qt::RightButton || (event->buttons() & Qt::RightButton))
        m_button = RightButton;
    else if (event->button() == Qt::MidButton || (event->buttons() & Qt::MidButton))
        m_button = MiddleButton;
    else
        m_button = NoButton;

    m_clickCount = clickCount;
    m_shiftKey =  (event->modifiers() & Qt::ShiftModifier) != 0;
    m_ctrlKey = (event->modifiers() & Qt::ControlModifier) != 0;
    m_altKey =  (event->modifiers() & Qt::AltModifier) != 0;
    m_metaKey = (event->modifiers() & Qt::MetaModifier) != 0;    
}

}

// vim: ts=4 sw=4 et
