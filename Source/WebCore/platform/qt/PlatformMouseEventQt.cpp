/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2011 Research In Motion Limited.
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

#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <wtf/CurrentTime.h>

namespace WebCore {

static void mouseEventModifiersFromQtKeyboardModifiers(Qt::KeyboardModifiers keyboardModifiers, bool& altKey, bool& ctrlKey, bool& metaKey, bool& shiftKey)
{
    altKey = keyboardModifiers & Qt::AltModifier;
    ctrlKey = keyboardModifiers & Qt::ControlModifier;
    metaKey = keyboardModifiers & Qt::MetaModifier;
    shiftKey = keyboardModifiers & Qt::ShiftModifier;
}

static void mouseEventTypeAndMouseButtonFromQEvent(const QEvent* event, PlatformEvent::Type& mouseEventType, MouseButton& mouseButton)
{
    enum { MouseEvent, GraphicsSceneMouseEvent } frameworkMouseEventType;
    switch (event->type()) {
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonPress:
        frameworkMouseEventType = MouseEvent;
        mouseEventType = PlatformEvent::MousePressed;
        break;
    case QEvent::MouseButtonRelease:
        frameworkMouseEventType = MouseEvent;
        mouseEventType = PlatformEvent::MouseReleased;
        break;
    case QEvent::MouseMove:
        frameworkMouseEventType = MouseEvent;
        mouseEventType = PlatformEvent::MouseMoved;
        break;
#if !defined(QT_NO_GRAPHICSVIEW)
    case QEvent::GraphicsSceneMouseDoubleClick:
    case QEvent::GraphicsSceneMousePress:
        frameworkMouseEventType = GraphicsSceneMouseEvent;
        mouseEventType = PlatformEvent::MousePressed;
        break;
    case QEvent::GraphicsSceneMouseRelease:
        frameworkMouseEventType = GraphicsSceneMouseEvent;
        mouseEventType = PlatformEvent::MouseReleased;
        break;
    case QEvent::GraphicsSceneMouseMove:
        frameworkMouseEventType = GraphicsSceneMouseEvent;
        mouseEventType = PlatformEvent::MouseMoved;
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
        frameworkMouseEventType = MouseEvent;
        mouseEventType = PlatformEvent::MouseMoved;
        break;
    }

    Qt::MouseButtons mouseButtons;
    switch (frameworkMouseEventType) {
    case MouseEvent: {
        const QMouseEvent* mouseEvent = static_cast<const QMouseEvent*>(event);
        mouseButtons = mouseEventType == PlatformEvent::MouseMoved ? mouseEvent->buttons() : mouseEvent->button();
        break;
    }
    case GraphicsSceneMouseEvent: {
        const QGraphicsSceneMouseEvent* mouseEvent = static_cast<const QGraphicsSceneMouseEvent*>(event);
        mouseButtons = mouseEventType == PlatformEvent::MouseMoved ? mouseEvent->buttons() : mouseEvent->button();
        break;
    }
    }

    if (mouseButtons & Qt::LeftButton)
        mouseButton = LeftButton;
    else if (mouseButtons & Qt::RightButton)
        mouseButton = RightButton;
    else if (mouseButtons & Qt::MidButton)
        mouseButton = MiddleButton;
    else
        mouseButton = NoButton;
}

#if !defined(QT_NO_GRAPHICSVIEW)
PlatformMouseEvent::PlatformMouseEvent(QGraphicsSceneMouseEvent* event, int clickCount)
{
    m_timestamp = WTF::currentTime();

    // FIXME: Why don't we handle a context menu event here as we do in PlatformMouseEvent(QInputEvent*, int)?
    // See <https://bugs.webkit.org/show_bug.cgi?id=60728>.
    PlatformEvent::Type type;
    mouseEventTypeAndMouseButtonFromQEvent(event, type, m_button);

    m_type = type;
    m_position = IntPoint(event->pos().toPoint());
    m_globalPosition = IntPoint(event->screenPos());

    m_clickCount = clickCount;
    mouseEventModifiersFromQtKeyboardModifiers(event->modifiers(), m_altKey, m_ctrlKey, m_metaKey, m_shiftKey);
}
#endif // QT_NO_GRAPHICSVIEW

PlatformMouseEvent::PlatformMouseEvent(QInputEvent* event, int clickCount)
{
    m_timestamp = WTF::currentTime();

    bool isContextMenuEvent = false;
#ifndef QT_NO_CONTEXTMENU
    if (event->type() == QEvent::ContextMenu) {
        isContextMenuEvent = true;
        m_type = PlatformEvent::MousePressed;
        QContextMenuEvent* ce = static_cast<QContextMenuEvent*>(event);
        m_position = IntPoint(ce->pos());
        m_globalPosition = IntPoint(ce->globalPos());
        m_button = RightButton;
    }
#endif
    if (!isContextMenuEvent) {
        PlatformEvent::Type type;
        mouseEventTypeAndMouseButtonFromQEvent(event, type, m_button);
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

        m_type = type;
        m_position = IntPoint(mouseEvent->pos());
        m_globalPosition = IntPoint(mouseEvent->globalPos());
    }

    m_clickCount = clickCount;
    mouseEventModifiersFromQtKeyboardModifiers(event->modifiers(), m_altKey, m_ctrlKey, m_metaKey, m_shiftKey);
}

}

// vim: ts=4 sw=4 et
