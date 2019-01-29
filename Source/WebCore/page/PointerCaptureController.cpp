/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "PointerCaptureController.h"

#if ENABLE(POINTER_EVENTS)

#include "Document.h"
#include "Element.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "Page.h"
#include "PointerEvent.h"

namespace WebCore {

PointerCaptureController::PointerCaptureController() = default;

ExceptionOr<void> PointerCaptureController::setPointerCapture(Element* capturingTarget, int32_t pointerId)
{
    // https://w3c.github.io/pointerevents/#setting-pointer-capture

    // 1. If the pointerId provided as the method's argument does not match any of the active pointers, then throw a DOMException with the name NotFoundError.
    auto iterator = m_activePointerIdsToCapturingData.find(pointerId);
    if (iterator == m_activePointerIdsToCapturingData.end())
        return Exception { NotFoundError };

    // 2. If the Element on which this method is invoked is not connected, throw an exception with the name InvalidStateError.
    if (!capturingTarget->isConnected())
        return Exception { InvalidStateError };

#if ENABLE(POINTER_LOCK)
    // 3. If this method is invoked while the document has a locked element, throw an exception with the name InvalidStateError.
    if (auto* page = capturingTarget->document().page()) {
        if (page->pointerLockController().isLocked())
            return Exception { InvalidStateError };
    }
#endif

    // 4. If the pointer is not in the active buttons state, then terminate these steps.
    // FIXME: implement when we support mouse events.

    // 5. For the specified pointerId, set the pending pointer capture target override to the Element on which this method was invoked.
    iterator->value.pendingTargetOverride = capturingTarget;

    return { };
}

ExceptionOr<void> PointerCaptureController::releasePointerCapture(Element* capturingTarget, int32_t pointerId, ImplicitCapture implicit)
{
    // https://w3c.github.io/pointerevents/#releasing-pointer-capture

    // Pointer capture is released on an element explicitly by calling the element.releasePointerCapture(pointerId) method.
    // When this method is called, a user agent MUST run the following steps:

    // 1. If the pointerId provided as the method's argument does not match any of the active pointers and these steps are not
    // being invoked as a result of the implicit release of pointer capture, then throw a DOMException with the name NotFoundError.
    auto iterator = m_activePointerIdsToCapturingData.find(pointerId);
    if (implicit == ImplicitCapture::No && iterator == m_activePointerIdsToCapturingData.end())
        return Exception { NotFoundError };

    // 2. If hasPointerCapture is false for the Element with the specified pointerId, then terminate these steps.
    if (!hasPointerCapture(capturingTarget, pointerId))
        return { };

    // 3. For the specified pointerId, clear the pending pointer capture target override, if set.
    iterator->value.pendingTargetOverride = nullptr;

    return { };
}

bool PointerCaptureController::hasPointerCapture(Element* capturingTarget, int32_t pointerId)
{
    // https://w3c.github.io/pointerevents/#dom-element-haspointercapture

    // Indicates whether the element on which this method is invoked has pointer capture for the pointer identified by the argument pointerId.
    // In particular, returns true if the pending pointer capture target override for pointerId is set to the element on which this method is
    // invoked, and false otherwise.

    auto iterator = m_activePointerIdsToCapturingData.find(pointerId);
    if (iterator == m_activePointerIdsToCapturingData.end())
        return false;

    auto& capturingData = iterator->value;
    return capturingData.pendingTargetOverride == capturingTarget || capturingData.targetOverride == capturingTarget;
}

void PointerCaptureController::pointerLockWasApplied()
{
    // https://w3c.github.io/pointerevents/#implicit-release-of-pointer-capture

    // When a pointer lock is successfully applied on an element, a user agent MUST run the steps as if the releasePointerCapture()
    // method has been called if any element is set to be captured or pending to be captured.
    for (auto& capturingData : m_activePointerIdsToCapturingData.values()) {
        capturingData.pendingTargetOverride = nullptr;
        capturingData.targetOverride = nullptr;
    }
}

void PointerCaptureController::touchEndedOrWasCancelledForIdentifier(int32_t pointerId)
{
    m_activePointerIdsToCapturingData.remove(pointerId);
}

void PointerCaptureController::pointerEventWillBeDispatched(const PointerEvent& event, EventTarget* target)
{
    // https://w3c.github.io/pointerevents/#implicit-pointer-capture

    // Some input devices (such as touchscreens) implement a "direct manipulation" metaphor where a pointer is intended to act primarily on the UI
    // element it became active upon (providing a physical illusion of direct contact, instead of indirect contact via a cursor that conceptually
    // floats above the UI). Such devices are identified by the InputDeviceCapabilities.pointerMovementScrolls property and should have "implicit
    // pointer capture" behavior as follows.

    // Direct manipulation devices should behave exactly as if setPointerCapture was called on the target element just before the invocation of any
    // pointerdown listeners. The hasPointerCapture API may be used (eg. within any pointerdown listener) to determine whether this has occurred. If
    // releasePointerCapture is not called for the pointer before the next pointer event is fired, then a gotpointercapture event will be dispatched
    // to the target (as normal) indicating that capture is active.

    if (!is<Element>(target) || event.type() != eventNames().pointerdownEvent)
        return;

    auto pointerId = event.pointerId();
    m_activePointerIdsToCapturingData.set(pointerId, CapturingData { });
    setPointerCapture(downcast<Element>(target), pointerId);
}

void PointerCaptureController::pointerEventWasDispatched(const PointerEvent& event)
{
    // https://w3c.github.io/pointerevents/#implicit-release-of-pointer-capture

    auto iterator = m_activePointerIdsToCapturingData.find(event.pointerId());
    if (iterator != m_activePointerIdsToCapturingData.end()) {
        auto& capturingData = iterator->value;
        // Immediately after firing the pointerup or pointercancel events, a user agent MUST clear the pending pointer capture target
        // override for the pointerId of the pointerup or pointercancel event that was just dispatched, and then run Process Pending
        // Pointer Capture steps to fire lostpointercapture if necessary.
        if (event.type() == eventNames().pointerupEvent)
            capturingData.pendingTargetOverride = nullptr;
        
        // When the pointer capture target override is no longer connected, the pending pointer capture target override and pointer
        // capture target override nodes SHOULD be cleared and also a PointerEvent named lostpointercapture corresponding to the captured
        // pointer SHOULD be fired at the document.
        if (capturingData.targetOverride && !capturingData.targetOverride->isConnected()) {
            capturingData.pendingTargetOverride = nullptr;
            capturingData.targetOverride = nullptr;
        }
    }

    processPendingPointerCapture(event);
}

void PointerCaptureController::processPendingPointerCapture(const PointerEvent& event)
{
    // https://w3c.github.io/pointerevents/#process-pending-pointer-capture

    auto iterator = m_activePointerIdsToCapturingData.find(event.pointerId());
    if (iterator == m_activePointerIdsToCapturingData.end())
        return;

    auto& capturingData = iterator->value;

    // 1. If the pointer capture target override for this pointer is set and is not equal to the pending pointer capture target override,
    // then fire a pointer event named lostpointercapture at the pointer capture target override node.
    if (capturingData.targetOverride && capturingData.targetOverride != capturingData.pendingTargetOverride)
        capturingData.targetOverride->dispatchEvent(PointerEvent::createForPointerCapture(eventNames().lostpointercaptureEvent, event));

    // 2. If the pending pointer capture target override for this pointer is set and is not equal to the pointer capture target override,
    // then fire a pointer event named gotpointercapture at the pending pointer capture target override.
    if (capturingData.pendingTargetOverride && capturingData.targetOverride != capturingData.pendingTargetOverride)
        capturingData.pendingTargetOverride->dispatchEvent(PointerEvent::createForPointerCapture(eventNames().gotpointercaptureEvent, event));

    // 3. Set the pointer capture target override to the pending pointer capture target override, if set. Otherwise, clear the pointer
    // capture target override.
    capturingData.targetOverride = capturingData.pendingTargetOverride;
}

} // namespace WebCore

#endif // ENABLE(POINTER_EVENTS)
