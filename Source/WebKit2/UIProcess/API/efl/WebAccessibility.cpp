/*
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#include "config.h"
#include "WebAccessibility.h"

#if HAVE(ACCESSIBILITY) && defined(HAVE_ECORE_X)

#include "EwkView.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

WebAccessibility::WebAccessibility(EwkView* ewkView)
    : m_activateAction(false)
    , m_nextAction(false)
    , m_prevAction(false)
    , m_readAction(false)
    , m_currentPoint(WebCore::IntPoint(-1, -1))
    , m_ewkView(ewkView)
    , m_eventHandler(ecore_event_handler_add(ECORE_X_EVENT_CLIENT_MESSAGE, eventHandler, this))
{
}

WebAccessibility::~WebAccessibility()
{
    ecore_event_handler_del(m_eventHandler);
}

Eina_Bool WebAccessibility::eventHandler(void* data, int, void* event)
{
    WebAccessibility* webAccessibility = static_cast<WebAccessibility*>(data);
    if (!webAccessibility)
        return false;

    Ecore_X_Event_Client_Message* ev = static_cast<Ecore_X_Event_Client_Message*>(event);
    if (!ev)
        return false;

    return webAccessibility->action(ev);
}

Eina_Bool WebAccessibility::action(Ecore_X_Event_Client_Message* event)
{
    if (event->message_type == ECORE_X_ATOM_E_ILLUME_ACCESS_CONTROL) {
        unsigned gestureType = static_cast<unsigned>(event->data.l[1]);
        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_ACTIVATE)
            return activate();

        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_READ)
            return read(event->data.l[2], event->data.l[3]);

        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_OVER)
            return read(event->data.l[2], event->data.l[3]);

        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_READ_NEXT)
            return readNext();

        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_READ_PREV)
            return readPrev();

        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_UP)
            return up();

        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_DOWN)
            return down();

        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_SCROLL)
            return scroll();

        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_MOUSE)
            return mouse();

        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_ENABLE)
            return enable();

        if (gestureType == ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_DISABLE)
            return disable();
    }

    return true;
}

bool WebAccessibility::activate()
{
    notImplemented();
    return false;
}

bool WebAccessibility::read(int, int)
{
    ASSERT(m_ewkView);
    m_readAction = m_ewkView->page()->accessibilityObjectReadByPoint(WebCore::IntPoint(m_currentPoint.x(), m_currentPoint.y()));
    return m_readAction;
}

bool WebAccessibility::readNext()
{
    ASSERT(m_ewkView);
    m_nextAction = m_ewkView->page()->accessibilityObjectReadPrevious();
    return m_nextAction;
}

bool WebAccessibility::readPrev()
{
    ASSERT(m_ewkView);
    m_prevAction = m_ewkView->page()->accessibilityObjectReadPrevious();
    return m_prevAction;
}

bool WebAccessibility::up()
{
    notImplemented();
    return false;
}

bool WebAccessibility::down()
{
    notImplemented();
    return false;
}

bool WebAccessibility::scroll()
{
    notImplemented();
    return false;
}

bool WebAccessibility::mouse()
{
    notImplemented();
    return false;
}

bool WebAccessibility::enable()
{
    notImplemented();
    return false;
}

bool WebAccessibility::disable()
{
    notImplemented();
    return false;
}

} // namespace WebKit

#endif // HAVE(ACCESSIBILITY) && defined(HAVE_ECORE_X)
