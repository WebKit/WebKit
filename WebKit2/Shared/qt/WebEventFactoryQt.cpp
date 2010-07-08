/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebEventFactoryQt.h"

#include <qgraphicssceneevent.h>
#include <QApplication>
#include <QKeyEvent>
#include <WebCore/PlatformKeyboardEvent.h>
#include <wtf/ASCIICType.h>
#include <wtf/CurrentTime.h>

using namespace WebCore;

namespace WebKit {

static WebMouseEvent::Button mouseButtonForEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton || (event->buttons() & Qt::LeftButton))
        return WebMouseEvent::LeftButton;
    else if (event->button() == Qt::RightButton || (event->buttons() & Qt::RightButton))
        return WebMouseEvent::RightButton;
    else if (event->button() == Qt::MidButton || (event->buttons() & Qt::MidButton))
        return WebMouseEvent::MiddleButton;
    return WebMouseEvent::NoButton;
}

static WebEvent::Type webEventTypeForEvent(QEvent* event)
{
    switch (event->type()) {
        case QEvent::GraphicsSceneMouseDoubleClick:
        case QEvent::GraphicsSceneMousePress:
            return WebEvent::MouseDown;
        case QEvent::GraphicsSceneMouseRelease:
            return WebEvent::MouseUp;
        case QEvent::GraphicsSceneMouseMove:
            return WebEvent::MouseMove;
        case QEvent::Wheel:
            return WebEvent::Wheel;
        case QEvent::KeyPress:
            return WebEvent::KeyDown;
        case QEvent::KeyRelease:
            return WebEvent::KeyUp;
        default:
            // assert
            return WebEvent::MouseMove;
    }
}

static inline WebEvent::Modifiers modifiersForEvent(Qt::KeyboardModifiers modifiers)
{
    unsigned result = 0;
    if (modifiers & Qt::ShiftModifier)
        result |= WebEvent::ShiftKey;
    if (modifiers & Qt::ControlModifier)
        result |= WebEvent::ControlKey;
    if (modifiers & Qt::AltModifier)
        result |= WebEvent::AltKey;
    if (modifiers & Qt::MetaModifier)
        result |= WebEvent::MetaKey;
    return (WebEvent::Modifiers)result;
}

WebMouseEvent WebEventFactory::createWebMouseEvent(QGraphicsSceneMouseEvent* event, int eventClickCount)
{
    IntPoint position(event->pos().toPoint());
    IntPoint globalPosition(event->screenPos());

    WebEvent::Type type             = webEventTypeForEvent(event);
    WebMouseEvent::Button button    = mouseButtonForEvent(event);
    int positionX                   = position.x();
    int positionY                   = position.y();
    int globalPositionX             = globalPosition.x();
    int globalPositionY             = globalPosition.y();
    int clickCount                  = eventClickCount;
    WebEvent::Modifiers modifiers   = modifiersForEvent(event->modifiers());
    double timestamp                = WTF::currentTime();

    return WebMouseEvent(type, button, positionX, positionY, globalPositionX, globalPositionY, clickCount, modifiers, timestamp);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(QGraphicsSceneWheelEvent* e)
{
    int x                                   = e->pos().x();
    int y                                   = e->pos().y();
    int globalX                             = e->screenPos().x();
    int globalY                             = e->screenPos().y();
    float deltaX                            = 0;
    float deltaY                            = 0;
    float wheelTicksX                       = 0;
    float wheelTicksY                       = 0;
    WebWheelEvent::Granularity granularity  = WebWheelEvent::ScrollByPixelWheelEvent;
    WebEvent::Modifiers modifiers           = modifiersForEvent(e->modifiers());
    double timestamp                        = WTF::currentTime();

    // A delta that is not mod 120 indicates a device that is sending
    // fine-resolution scroll events, so use the delta as number of wheel ticks
    // and number of pixels to scroll.See also webkit.org/b/29601
    bool fullTick = !(e->delta() % 120);

    if (e->orientation() == Qt::Horizontal) {
        deltaX = (fullTick) ? e->delta() / 120.0f : e->delta();
        wheelTicksX = deltaX;
    } else {
        deltaY = (fullTick) ? e->delta() / 120.0f : e->delta();
        wheelTicksY = deltaY;
    }

    // Use the same single scroll step as QTextEdit
    // (in QTextEditPrivate::init [h,v]bar->setSingleStep)
    static const float cDefaultQtScrollStep = 20.f;
#ifndef QT_NO_WHEELEVENT
    deltaX *= (fullTick) ? QApplication::wheelScrollLines() * cDefaultQtScrollStep : 1;
    deltaY *= (fullTick) ? QApplication::wheelScrollLines() * cDefaultQtScrollStep : 1;
#endif

    return WebWheelEvent(WebEvent::Wheel, x, y, globalX,  globalY, deltaX, deltaY, wheelTicksX, wheelTicksY, granularity, modifiers, timestamp);
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(QKeyEvent* event)
{
    const int state                 = event->modifiers();
    WebEvent::Type type             = webEventTypeForEvent(event);
    const String text               = event->text();
    const String unmodifiedText     = event->text();
    bool isAutoRepeat               = event->isAutoRepeat();
    bool isSystemKey                = false; // FIXME: No idea what that is.
    bool isKeypad                   = (state & Qt::KeypadModifier);
    const String keyIdentifier      = keyIdentifierForQtKeyCode(event->key());
    int windowsVirtualKeyCode       = windowsKeyCodeForKeyEvent(event->key(), isKeypad);
    int  nativeVirtualKeyCode       = event->nativeVirtualKey();
    WebEvent::Modifiers modifiers   = modifiersForEvent(event->modifiers());
    double timestamp                = WTF::currentTime();

    return WebKeyboardEvent(type, text, unmodifiedText, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, isAutoRepeat, isKeypad, isSystemKey, modifiers, timestamp);
}

} // namespace WebKit
