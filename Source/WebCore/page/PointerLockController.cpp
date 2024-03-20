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
#include "DocumentInlines.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PointerCaptureController.h"
#include "UserGestureIndicator.h"
#include "VoidCallback.h"

namespace WebCore {

class UnadjustedMovementPlatformMouseEvent : public PlatformMouseEvent {
public:
    explicit UnadjustedMovementPlatformMouseEvent(const PlatformMouseEvent& src)
        : PlatformMouseEvent(src)
    {
        m_movementDelta = src.unadjustedMovementDelta();
    }
};

PointerLockController::PointerLockController(Page& page)
    : m_page(page)
{
}

PointerLockController::~PointerLockController() = default;

void PointerLockController::requestPointerLock(Element* target, std::optional<PointerLockOptions>&& options, RefPtr<DeferredPromise> promise)
{
    if (!target || !target->isConnected() || m_documentOfRemovedElementWhileWaitingForUnlock) {
        enqueueEvent(eventNames().pointerlockerrorEvent, target);
        if (promise)
            promise->reject(ExceptionCode::WrongDocumentError, "Pointer lock target must be in an active document."_s);
        return;
    }

    if (m_documentAllowedToRelockWithoutUserGesture != &target->document() && !UserGestureIndicator::processingUserGesture()) {
        enqueueEvent(eventNames().pointerlockerrorEvent, target);
        // If the request was not started from an engagement gesture and the Document has not previously released a successful Pointer Lock with exitPointerLock():
        if (promise)
            promise->reject(ExceptionCode::NotAllowedError, "Pointer lock requires a user gesture."_s);
        return;
    }

    if (target->document().isSandboxed(SandboxPointerLock)) {
        auto reason = "Blocked pointer lock on an element because the element's frame is sandboxed and the 'allow-pointer-lock' permission is not set."_s;
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        target->document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, reason);
        enqueueEvent(eventNames().pointerlockerrorEvent, target);
        // If this's node document's active sandboxing flag set has the sandboxed pointer lock browsing context flag set:
        if (promise)
            promise->reject(ExceptionCode::SecurityError, reason);
        return;
    }

    if (options && options->unadjustedMovement && !supportsUnadjustedMovement()) {
        // If options["unadjustedMovement"] is true and the platform does not support unadjustedMovement:
        enqueueEvent(eventNames().pointerlockerrorEvent, target);
        if (promise)
            promise->reject(ExceptionCode::NotSupportedError, "Unadjusted movement is unavailable."_s);
        return;
    }

    if (m_element) {
        if (&m_element->document() != &target->document()) {
            enqueueEvent(eventNames().pointerlockerrorEvent, target);
            // If the user agent's pointer-lock target is an element whose shadow-including root is not equal to this's shadow-including root, then:
            if (promise)
                promise->reject(ExceptionCode::InvalidStateError, "Pointer lock cannot be moved to an element in a different document."_s);
            return;
        }
        m_element = target;
        m_options = WTFMove(options);
        if (m_lockPending) {
            // m_lockPending means an answer from the ChromeClient for a previous requestPointerLock on the page is pending. It's currently unknown which way that will go, whether the request will be approved or rejected. Therefore queue the promise for later when that's known and don't send out any pointerlockchangeEvent yet.
            if (promise)
                m_promises.append(promise.releaseNonNull());
        } else {
            // !m_lockPending means the pointer lock is currently held on the page, and page is just re-targeting it or changing the options.
            enqueueEvent(eventNames().pointerlockchangeEvent, target);
            if (promise)
                promise->resolve();
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=261786
            // This probably needs to be called in all code paths.
            m_page.pointerCaptureController().pointerLockWasApplied();
        }
    } else {
        m_lockPending = true;
        m_element = target;
        m_options = WTFMove(options);
        if (promise)
            m_promises.append(promise.releaseNonNull());
        // ChromeClient::requestPointerLock() can call back into didAcquirePointerLock(), so all state including element, options, and promise needs to be stored before it is called.
        if (!m_page.chrome().client().requestPointerLock()) {
            enqueueEvent(eventNames().pointerlockerrorEvent, target);
            rejectPromises(ExceptionCode::NotSupportedError, "Pointer lock is unavailable."_s);
            clearElement();
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

void PointerLockController::elementWasRemovedInternal()
{
    m_documentOfRemovedElementWhileWaitingForUnlock = m_element->document();
    requestPointerUnlock();
    // It is possible that the element is removed while pointer lock is still pending.
    // This is essentially the same situation as the !target->isConnected() test in
    // PointerLockController::requestPointerLock, but it has arisen in between then and now.
    rejectPromises(ExceptionCode::WrongDocumentError, "Pointer lock target element was removed."_s);
    // Set element null immediately to block any future interaction with it
    // including mouse events received before the unlock completes.
    clearElement();
}

void PointerLockController::documentDetached(Document& document)
{
    if (m_documentAllowedToRelockWithoutUserGesture == &document)
        m_documentAllowedToRelockWithoutUserGesture = nullptr;

    if (m_element && &m_element->document() == &document) {
        m_documentOfRemovedElementWhileWaitingForUnlock = m_element->document();
        requestPointerUnlock();
        // It is possible that the document is detached while pointer lock is still pending.
        rejectPromises(ExceptionCode::WrongDocumentError, "Pointer lock target document was detached."_s);
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

    enqueueEvent(eventNames().pointerlockchangeEvent, m_element.copyRef().get());
    resolvePromises();
    m_lockPending = false;
    m_forceCursorVisibleUponUnlock = false;
    m_documentAllowedToRelockWithoutUserGesture = m_element->document();
}

void PointerLockController::didNotAcquirePointerLock()
{
    // didNotAcquirePointerLock is sent in response to ChromeClient::requestPointerLock if the window does not have focus or the UI delegate is not allowing pointer lock.
    // From the draft spec:
    // If the this's shadow-including root is the active document of a browsing context which is not (or has an ancestor browsing context which is not) in focus by a window which is in focus by the operating system's window manager:
    // 1. Fire an event named pointerlockerror at this's node document.
    // 2. Reject promise with a "WrongDocumentError" DOMException.

    enqueueEvent(eventNames().pointerlockerrorEvent, m_element.copyRef().get());
    rejectPromises(ExceptionCode::WrongDocumentError, "Pointer lock requires the window to have focus."_s);
    clearElement();
    m_unlockPending = false;
    m_documentOfRemovedElementWhileWaitingForUnlock = nullptr;
}

void PointerLockController::didLosePointerLock()
{
    if (!m_unlockPending)
        m_documentAllowedToRelockWithoutUserGesture = nullptr;

    enqueueEvent(eventNames().pointerlockchangeEvent, m_element ? m_element->protectedDocument().ptr() : RefPtr { m_documentOfRemovedElementWhileWaitingForUnlock.get() }.get());
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
    protectedElement->dispatchMouseEvent((m_options && m_options->unadjustedMovement) ? UnadjustedMovementPlatformMouseEvent(event) : event, eventType, event.clickCount());

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
    m_options = std::nullopt;
    ASSERT(m_promises.isEmpty());
}

void PointerLockController::enqueueEvent(const AtomString& type, Element* element)
{
    if (element)
        enqueueEvent(type, element->protectedDocument().ptr());
}

void PointerLockController::enqueueEvent(const AtomString& type, Document* document)
{
    // FIXME: Spec doesn't specify which task source use.
    if (document)
        document->queueTaskToDispatchEvent(TaskSource::UserInteraction, Event::create(type, Event::CanBubble::Yes, Event::IsCancelable::No));
}

void PointerLockController::resolvePromises()
{
    for (auto& promise : std::exchange(m_promises, { }))
        promise->resolve();
}

void PointerLockController::rejectPromises(ExceptionCode code, const String& reason)
{
    for (auto& promise : std::exchange(m_promises, { }))
        promise->reject(code, reason);
}

bool PointerLockController::supportsUnadjustedMovement()
{
#if HAVE(MOUSE_UNACCELERATED_MOVEMENT)
    return true;
#else
    return false;
#endif
}


} // namespace WebCore

#endif // ENABLE(POINTER_LOCK)
