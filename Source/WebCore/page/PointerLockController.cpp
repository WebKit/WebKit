/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PointerLockController.h"

#if ENABLE(POINTER_LOCK)

#include "Chrome.h"
#include "ChromeClient.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PointerCaptureController.h"
#include "UserGestureIndicator.h"
#include "VoidCallback.h"

namespace WebCore {

PointerLockController::PointerLockController(Page& page)
    : m_page(page)
{
}

void PointerLockController::requestPointerLock(Element* target)
{
    if (!target || !target->isConnected() || m_documentOfRemovedElementWhileWaitingForUnlock) {
        enqueueEvent(eventNames().pointerlockerrorEvent, target);
        return;
    }

    if (m_documentAllowedToRelockWithoutUserGesture != &target->document() && !UserGestureIndicator::processingUserGesture()) {
        enqueueEvent(eventNames().pointerlockerrorEvent, target);
        return;
    }

    if (target->document().isSandboxed(SandboxPointerLock)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        target->document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked pointer lock on an element because the element's frame is sandboxed and the 'allow-pointer-lock' permission is not set."_s);
        enqueueEvent(eventNames().pointerlockerrorEvent, target);
        return;
    }

    if (m_element) {
        if (&m_element->document() != &target->document()) {
            enqueueEvent(eventNames().pointerlockerrorEvent, target);
            return;
        }
        m_element = target;
        enqueueEvent(eventNames().pointerlockchangeEvent, target);
        m_page.pointerCaptureController().pointerLockWasApplied();
    } else {
        m_lockPending = true;
        m_element = target;
        if (!m_page.chrome().client().requestPointerLock()) {
            clearElement();
            enqueueEvent(eventNames().pointerlockerrorEvent, target);
        }
    }
}

void PointerLockController::requestPointerUnlock()
{
    if (!m_element)
        return;

    m_unlockPending = true;
    m_page.chrome().client().requestPointerUnlock();
}

void PointerLockController::requestPointerUnlockAndForceCursorVisible()
{
    m_documentAllowedToRelockWithoutUserGesture = nullptr;

    if (!m_element)
        return;

    m_unlockPending = true;
    m_page.chrome().client().requestPointerUnlock();
    m_forceCursorVisibleUponUnlock = true;
}

void PointerLockController::elementWasRemoved(Element& element)
{
    if (m_element == &element) {
        m_documentOfRemovedElementWhileWaitingForUnlock = makeWeakPtr(m_element->document());
        // Set element null immediately to block any future interaction with it
        // including mouse events received before the unlock completes.
        requestPointerUnlock();
        clearElement();
    }
}

void PointerLockController::documentDetached(Document& document)
{
    if (m_documentAllowedToRelockWithoutUserGesture == &document)
        m_documentAllowedToRelockWithoutUserGesture = nullptr;

    if (m_element && &m_element->document() == &document) {
        m_documentOfRemovedElementWhileWaitingForUnlock = makeWeakPtr(m_element->document());
        requestPointerUnlock();
        clearElement();
    }
}

bool PointerLockController::isLocked() const
{
    return m_element && !m_lockPending;
}

bool PointerLockController::lockPending() const
{
    return m_lockPending;
}

Element* PointerLockController::element() const
{
    return m_element.get();
}

void PointerLockController::didAcquirePointerLock()
{
    if (!m_lockPending)
        return;
    
    ASSERT(m_element);
    
    enqueueEvent(eventNames().pointerlockchangeEvent, m_element.get());
    m_lockPending = false;
    m_forceCursorVisibleUponUnlock = false;
    m_documentAllowedToRelockWithoutUserGesture = makeWeakPtr(m_element->document());
}

void PointerLockController::didNotAcquirePointerLock()
{
    enqueueEvent(eventNames().pointerlockerrorEvent, m_element.get());
    clearElement();
    m_unlockPending = false;
}

void PointerLockController::didLosePointerLock()
{
    if (!m_unlockPending)
        m_documentAllowedToRelockWithoutUserGesture = nullptr;

    enqueueEvent(eventNames().pointerlockchangeEvent, m_element ? &m_element->document() : m_documentOfRemovedElementWhileWaitingForUnlock.get());
    clearElement();
    m_unlockPending = false;
    m_documentOfRemovedElementWhileWaitingForUnlock = nullptr;
    if (m_forceCursorVisibleUponUnlock) {
        m_forceCursorVisibleUponUnlock = false;
        m_page.chrome().client().setCursorHiddenUntilMouseMoves(false);
    }
}

void PointerLockController::dispatchLockedMouseEvent(const PlatformMouseEvent& event, const AtomString& eventType)
{
    if (!m_element || !m_element->document().frame())
        return;

    Ref protectedElement { *m_element };
    protectedElement->dispatchMouseEvent(event, eventType, event.clickCount());

    // Create click events
    if (eventType == eventNames().mouseupEvent)
        protectedElement->dispatchMouseEvent(event, eventNames().clickEvent, event.clickCount());
}

void PointerLockController::dispatchLockedWheelEvent(const PlatformWheelEvent& event)
{
    if (!m_element || !m_element->document().frame())
        return;

    OptionSet<EventHandling> defaultHandling;
    m_element->dispatchWheelEvent(event, defaultHandling);
}

void PointerLockController::clearElement()
{
    m_lockPending = false;
    m_element = nullptr;
}

void PointerLockController::enqueueEvent(const AtomString& type, Element* element)
{
    if (element)
        enqueueEvent(type, &element->document());
}

void PointerLockController::enqueueEvent(const AtomString& type, Document* document)
{
    // FIXME: Spec doesn't specify which task source use.
    if (auto protectedDocument = makeRefPtr(document))
        protectedDocument->queueTaskToDispatchEvent(TaskSource::UserInteraction, Event::create(type, Event::CanBubble::Yes, Event::IsCancelable::No));
}

} // namespace WebCore

#endif // ENABLE(POINTER_LOCK)
