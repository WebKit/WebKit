/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

#include "Chrome.h"
#include "ChromeClient.h"
#include "DocumentPage.h"
#include "Element.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "EventTarget.h"
#include "FrameInlines.h"
#include "HitTestResult.h"
#include "MouseEventTypes.h"
#include "NodeDocument.h"
#include "Page.h"
#include "PointerEvent.h"
#include "Quirks.h"
#include <algorithm>
#include <ranges>
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(POINTER_LOCK)
#include "PointerLockController.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PointerCaptureController);

PointerCaptureController::PointerCaptureController(Page& page)
    : m_page(page)
{
    reset();
}

Element* PointerCaptureController::pointerCaptureElement(Document* document, PointerID pointerId) const
{
    if (auto capturingData = m_activePointerIdsToCapturingData.get(pointerId)) {
        auto pointerCaptureElement = capturingData->targetOverride;
        if (pointerCaptureElement && &pointerCaptureElement->document() == document)
            return pointerCaptureElement.unsafeGet();
    }
    return nullptr;
}

ExceptionOr<void> PointerCaptureController::setPointerCapture(Element* capturingTarget, PointerID pointerId)
{
    // https://w3c.github.io/pointerevents/#setting-pointer-capture

    // 1. If the pointerId provided as the method's argument does not match any of the active pointers, then throw a DOMException with the name NotFoundError.
    RefPtr capturingData = m_activePointerIdsToCapturingData.get(pointerId);
    if (!capturingData)
        return Exception { ExceptionCode::NotFoundError };

    // 2. If the Element on which this method is invoked is not connected, throw an exception with the name InvalidStateError.
    if (!capturingTarget->isConnected())
        return Exception { ExceptionCode::InvalidStateError };

#if ENABLE(POINTER_LOCK)
    // 3. If this method is invoked while the document has a locked element, throw an exception with the name InvalidStateError.
    if (auto* page = capturingTarget->document().page()) {
        if (page->pointerLockController().isLocked())
            return Exception { ExceptionCode::InvalidStateError };
    }
#endif

    // 4. If the pointer is not in the active buttons state or the element's node document is not the active document of the pointer,
    // then terminate these steps.
    if (!capturingData->pointerIsPressed || &capturingTarget->document() != capturingData->activeDocument)
        return { };

    // 5. For the specified pointerId, set the pending pointer capture target override to the Element on which this method was invoked.
    capturingData->pendingTargetOverride = capturingTarget;

    updateHaveAnyCapturingElement();
    return { };
}

ExceptionOr<void> PointerCaptureController::releasePointerCapture(Element* capturingTarget, PointerID pointerId)
{
    // https://w3c.github.io/pointerevents/#releasing-pointer-capture

    // Pointer capture is released on an element explicitly by calling the element.releasePointerCapture(pointerId) method.
    // When this method is called, a user agent MUST run the following steps:

    // 1. If the pointerId provided as the method's argument does not match any of the active pointers and these steps are not
    // being invoked as a result of the implicit release of pointer capture, then throw a DOMException with the name NotFoundError.
    RefPtr capturingData = m_activePointerIdsToCapturingData.get(pointerId);
    if (!capturingData)
        return Exception { ExceptionCode::NotFoundError };

    // 2. If hasPointerCapture is false for the Element with the specified pointerId, then terminate these steps.
    if (!hasPointerCapture(capturingTarget, pointerId))
        return { };

    // 3. For the specified pointerId, clear the pending pointer capture target override, if set.
    capturingData->pendingTargetOverride = nullptr;
    
    // FIXME: This leaves value.targetOverride set: webkit.org/b/221342.

    updateHaveAnyCapturingElement();
    return { };
}

bool PointerCaptureController::hasPointerCapture(Element* capturingTarget, PointerID pointerId)
{
    // https://w3c.github.io/pointerevents/#dom-element-haspointercapture

    // Indicates whether the element on which this method is invoked has pointer capture for the pointer identified by the argument pointerId.
    // In particular, returns true if the pending pointer capture target override for pointerId is set to the element on which this method is
    // invoked, and false otherwise.

    if (!m_haveAnyCapturingElement)
        return false;

    RefPtr capturingData = m_activePointerIdsToCapturingData.get(pointerId);
    return capturingData && capturingData->pendingTargetOverride == capturingTarget;
}

void PointerCaptureController::pointerLockWasApplied()
{
    // https://w3c.github.io/pointerevents/#implicit-release-of-pointer-capture

    // When a pointer lock is successfully applied on an element, a user agent MUST run the steps as if the releasePointerCapture()
    // method has been called if any element is set to be captured or pending to be captured.
    for (auto& capturingData : m_activePointerIdsToCapturingData.values()) {
        capturingData->pendingTargetOverride = nullptr;
        capturingData->targetOverride = nullptr;
    }

    updateHaveAnyCapturingElement();
}

void PointerCaptureController::elementWasRemovedSlow(Element& element)
{
    ASSERT(m_haveAnyCapturingElement);

    for (auto [pointerId, capturingData] : m_activePointerIdsToCapturingData) {
        if (capturingData->pendingTargetOverride == &element || capturingData->targetOverride == &element) {
            // https://w3c.github.io/pointerevents/#implicit-release-of-pointer-capture
            // When the pointer capture target override is no longer connected, the pending pointer capture target override and pointer capture target
            // override nodes SHOULD be cleared and also a PointerEvent named lostpointercapture corresponding to the captured pointer SHOULD be fired
            // at the document.
            ASSERT(isInBounds<PointerID>(pointerId));
            auto pointerType = capturingData->pointerType;
            releasePointerCapture(&element, pointerId);
            // FIXME: Spec doesn't specify which task source to use.
            element.document().queueTaskToDispatchEvent(TaskSource::UserInteraction, PointerEvent::create(eventNames().lostpointercaptureEvent, pointerId, pointerType));
            return;
        }
    }
}

void PointerCaptureController::reset()
{
    m_activePointerIdsToCapturingData.clear();
    m_haveAnyCapturingElement = false;

    m_activePointerIdsToCapturingData.add(mousePointerID, CapturingData::create(mousePointerEventType()));
}

void PointerCaptureController::updateHaveAnyCapturingElement()
{
    m_haveAnyCapturingElement = std::ranges::any_of(m_activePointerIdsToCapturingData.values(), [&](auto& capturingData) {
        return capturingData->hasAnyElement();
    });
}

void PointerCaptureController::touchWithIdentifierWasRemoved(PointerID pointerId)
{
    m_activePointerIdsToCapturingData.remove(pointerId);
    updateHaveAnyCapturingElement();
}

bool PointerCaptureController::hasCancelledPointerEventForIdentifier(PointerID pointerId) const
{
    RefPtr capturingData = m_activePointerIdsToCapturingData.get(pointerId);
    return capturingData && capturingData->state == CapturingData::State::Cancelled;
}

bool PointerCaptureController::preventsCompatibilityMouseEventsForIdentifier(PointerID pointerId) const
{
    RefPtr capturingData = m_activePointerIdsToCapturingData.get(pointerId);
    return capturingData && capturingData->preventsCompatibilityMouseEvents;
}

#if ENABLE(TOUCH_EVENTS) && (PLATFORM(IOS_FAMILY) || PLATFORM(WPE) || PLATFORM(GTK))
static bool hierarchyHasCapturingEventListeners(Element* target, const AtomString& eventName)
{
    for (RefPtr<ContainerNode> currentNode = target; currentNode; currentNode = currentNode->parentInComposedTree()) {
        if (currentNode->hasCapturingEventListeners(eventName))
            return true;
    }
    return false;
}

void PointerCaptureController::dispatchOverOrOutEvent(const AtomString& type, EventTarget* target, const PlatformTouchEvent& event, unsigned index, bool isPrimary, WindowProxy& view, DoublePoint touchDelta)
{
    dispatchEvent(PointerEvent::create(type, event, { }, { }, index, isPrimary, view, touchDelta), target);
}

void PointerCaptureController::dispatchEnterOrLeaveEvent(const AtomString& type, Element& targetElement, const PlatformTouchEvent& event, unsigned index, bool isPrimary, WindowProxy& view, DoublePoint touchDelta)
{
    bool hasCapturingListenerInHierarchy = false;
    for (RefPtr<ContainerNode> currentNode = targetElement; currentNode; currentNode = currentNode->parentInComposedTree()) {
        if (currentNode->hasCapturingEventListeners(type)) {
            hasCapturingListenerInHierarchy = true;
            break;
        }
    }

    Vector<Ref<Element>, 32> targetChain;
    for (RefPtr element = targetElement; element; element = element->parentElementInComposedTree()) {
        if (hasCapturingListenerInHierarchy || element->hasEventListeners(type))
            targetChain.append(*element);
    }

    if (type == eventNames().pointerenterEvent) {
        for (auto& element : targetChain | std::views::reverse)
            dispatchEvent(PointerEvent::create(type, event, { }, { }, index, isPrimary, view, touchDelta), element.ptr());
    } else {
        for (auto& element : targetChain)
            dispatchEvent(PointerEvent::create(type, event, { }, { }, index, isPrimary, view, touchDelta), element.ptr());
    }
}

void PointerCaptureController::dispatchEventForTouchAtIndex(EventTarget& target, const PlatformTouchEvent& platformTouchEvent, unsigned index, bool isPrimary, WindowProxy& view, const DoublePoint& touchDelta)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    RELEASE_ASSERT(is<Element>(target));

    auto mapToPointerEvents = [&](const Vector<PlatformTouchEvent>& events) -> Vector<Ref<PointerEvent>> {
        if (index)
            return { PointerEvent::create(platformTouchEvent, { }, { }, Event::CanBubble::No, Event::IsCancelable::No, index, isPrimary, view, touchDelta) };

        return WTF::map(events, [&](const auto& event) {
            return PointerEvent::create(event, { }, { }, Event::CanBubble::No, Event::IsCancelable::No, index, isPrimary, view, touchDelta);
        });
    };

    auto coalescedEvents = mapToPointerEvents(platformTouchEvent.coalescedEvents());
    auto predictedEvents = mapToPointerEvents(platformTouchEvent.predictedEvents());

    auto pointerEvent = PointerEvent::create(platformTouchEvent, coalescedEvents, predictedEvents, index, isPrimary, view, touchDelta);

    Ref capturingData = ensureCapturingDataForPointerEvent(pointerEvent);

    // Check if the target changed, which would require dispatching boundary events.
    RefPtr<Element> previousTarget = capturingData->previousTarget;
    RefPtr<Element> currentTarget = downcast<Element>(&target);

    capturingData->previousTarget = currentTarget;

    if (pointerEvent->type() == eventNames().pointermoveEvent && previousTarget != currentTarget) {
        // The pointerenter and pointerleave events are only dispatched if there is a capturing event listener on an ancestor
        // or a normal event listener on the element itself since those events do not bubble.
        // This optimization is necessary since these events can cause O(n^2) capturing event-handler checks. This follows the
        // code for similar mouse events in EventHandler::updateMouseEventTargetNode().
        bool hasCapturingPointerEnterListener = hierarchyHasCapturingEventListeners(currentTarget.get(), eventNames().pointerenterEvent);
        bool hasCapturingPointerLeaveListener = hierarchyHasCapturingEventListeners(previousTarget.get(), eventNames().pointerleaveEvent);

        Vector<Ref<Element>, 32> leftElementsChain;
        for (RefPtr element = previousTarget.get(); element; element = element->parentElementInComposedTree())
            leftElementsChain.append(*element);
        Vector<Ref<Element>, 32> enteredElementsChain;
        for (RefPtr element = currentTarget.get(); element; element = element->parentElementInComposedTree())
            enteredElementsChain.append(*element);

        if (!leftElementsChain.isEmpty() && !enteredElementsChain.isEmpty() && leftElementsChain.last().ptr() == enteredElementsChain.last().ptr()) {
            size_t minHeight = std::min(leftElementsChain.size(), enteredElementsChain.size());
            size_t i;
            for (i = 0; i < minHeight; ++i) {
                if (leftElementsChain[leftElementsChain.size() - i - 1].ptr() != enteredElementsChain[enteredElementsChain.size() - i - 1].ptr())
                    break;
            }
            leftElementsChain.shrink(leftElementsChain.size() - i);
            enteredElementsChain.shrink(enteredElementsChain.size() - i);
        }

        if (previousTarget)
            dispatchOverOrOutEvent(eventNames().pointeroutEvent, previousTarget.get(), platformTouchEvent, index, isPrimary, view, touchDelta);

        for (auto& chain : leftElementsChain) {
            if (hasCapturingPointerLeaveListener || chain->hasEventListeners(eventNames().pointerleaveEvent))
                dispatchEvent(PointerEvent::create(eventNames().pointerleaveEvent, platformTouchEvent, coalescedEvents, predictedEvents, index, isPrimary, view, touchDelta), chain.ptr());
        }

        if (currentTarget)
            dispatchOverOrOutEvent(eventNames().pointeroverEvent, currentTarget.get(), platformTouchEvent, index, isPrimary, view, touchDelta);

        for (auto& chain : enteredElementsChain | std::views::reverse) {
            if (hasCapturingPointerEnterListener || chain->hasEventListeners(eventNames().pointerenterEvent))
                dispatchEvent(PointerEvent::create(eventNames().pointerenterEvent, platformTouchEvent, coalescedEvents, predictedEvents, index, isPrimary, view, touchDelta), chain.ptr());
        }
    }

    if (pointerEvent->type() == eventNames().pointerdownEvent) {
        // https://w3c.github.io/pointerevents/#the-pointerdown-event
        // For input devices that do not support hover, a user agent MUST also fire a pointer event named pointerover followed by a pointer event named
        // pointerenter prior to dispatching the pointerdown event.
        dispatchOverOrOutEvent(eventNames().pointeroverEvent, currentTarget.get(), platformTouchEvent, index, isPrimary, view, touchDelta);
        dispatchEnterOrLeaveEvent(eventNames().pointerenterEvent, *currentTarget, platformTouchEvent, index, isPrimary, view, touchDelta);
    }

#if PLATFORM(IOS_FAMILY)
    if (pointerEvent->type() == eventNames().pointercancelEvent) {
        cancelPointer(pointerEvent->pointerId(), IntPoint(platformTouchEvent.touchLocationInRootViewAtIndex(index)), pointerEvent.ptr());
        return;
    }
#endif

    dispatchEvent(pointerEvent, &target);

    if (pointerEvent->type() != eventNames().pointerupEvent)
        return;

    auto dispatchPointerOutAndLeave = [weakPage = m_page, currentTarget, platformTouchEvent, index, isPrimary, view = Ref { view }, touchDelta](SyntheticClickResult result) {
        RefPtr page = weakPage.get();
        if (!page)
            return;

        switch (result) {
        case SyntheticClickResult::Failed:
        case SyntheticClickResult::Click:
            break;
        case SyntheticClickResult::Hover:
        case SyntheticClickResult::PageInvalid:
            return;
        }

        // https://w3c.github.io/pointerevents/#the-pointerup-event
        // For input devices that do not support hover, a user agent MUST also fire a pointer event named pointerout followed by a
        // pointer event named pointerleave after dispatching the pointerup event.
        page->pointerCaptureController().dispatchOverOrOutEvent(eventNames().pointeroutEvent, currentTarget.get(), platformTouchEvent, index, isPrimary, view.get(), touchDelta);
        page->pointerCaptureController().dispatchEnterOrLeaveEvent(eventNames().pointerleaveEvent, *currentTarget, platformTouchEvent, index, isPrimary, view.get(), touchDelta);
    };

    bool shouldWaitForSyntheticClick = [&] {
#if PLATFORM(IOS_FAMILY)
        if (platformTouchEvent.isPotentialTap())
            return currentTarget->protectedDocument()->quirks().shouldDispatchPointerOutAfterHandlingSyntheticClick();
#endif
        return false;
    }();

    if (shouldWaitForSyntheticClick)
        page->chrome().client().callAfterPendingSyntheticClick(WTFMove(dispatchPointerOutAndLeave));
    else
        dispatchPointerOutAndLeave(SyntheticClickResult::Failed);

    capturingData->state = CapturingData::State::Finished;
    capturingData->previousTarget = nullptr;
}
#endif // ENABLE(TOUCH_EVENTS) && (PLATFORM(IOS_FAMILY) || PLATFORM(WPE) || PLATFORM(GTK))

void PointerCaptureController::clearUnmatchedMouseDown(PointerID pointerID)
{
    if (RefPtr capturingData = m_activePointerIdsToCapturingData.get(pointerID))
        capturingData->pointerIsPressed = false;
}

RefPtr<PointerEvent> PointerCaptureController::pointerEventForMouseEvent(const MouseEvent& mouseEvent, PointerID pointerId, const String& pointerType)
{
    // If we already have known touches then we cannot dispatch a mouse event,
    // for instance in the case of a long press to initiate a system drag.
    for (auto& capturingData : m_activePointerIdsToCapturingData.values()) {
        if (capturingData->pointerType == touchPointerEventType() && capturingData->state == CapturingData::State::Ready)
            return nullptr;
    }

    const auto& type = mouseEvent.type();
    const auto& names = eventNames();

    RefPtr capturingData = m_activePointerIdsToCapturingData.get(pointerId);
    bool pointerIsPressed = capturingData ? capturingData->pointerIsPressed : false;

    MouseButton newButton = mouseEvent.button();
    MouseButton previousMouseButton = capturingData ? capturingData->previousMouseButton : MouseButton::PointerHasNotChanged;
    MouseButton button = [&] {
        auto pointerEventType = PointerEvent::typeFromMouseEventType(type);
        if (!PointerEvent::typeRequiresResolvedButton(pointerEventType)) {
            if (newButton == previousMouseButton || !pointerIsPressed)
                return MouseButton::PointerHasNotChanged;
        }
        return newButton;
    }();

    // https://w3c.github.io/pointerevents/#chorded-button-interactions
    // Some pointer devices, such as mouse or pen, support multiple buttons. In the Mouse Event model, each button
    // press produces a mousedown and mouseup event. To better abstract this hardware difference and simplify
    // cross-device input authoring, Pointer Events do not fire overlapping pointerdown and pointerup events
    // for chorded button presses (depressing an additional button while another button on the pointer device is
    // already depressed).
    if (type == names.mousedownEvent || type == names.mouseupEvent) {
        // We're already active and getting another mousedown, this means that we should dispatch
        // a pointermove event and let the button state show the newly depressed button.
        if (type == names.mousedownEvent && pointerIsPressed)
            return PointerEvent::create(names.pointermoveEvent, button, mouseEvent, pointerId, pointerType);

        // We're active and the mouseup still has some pressed button, this means we should dispatch
        // a pointermove event.
        if (type == names.mouseupEvent && pointerIsPressed && mouseEvent.buttons() > 0)
            return PointerEvent::create(names.pointermoveEvent, button, mouseEvent, pointerId, pointerType);
    }

    auto pointerEvent = PointerEvent::create(button, mouseEvent, pointerId, pointerType);
    if (capturingData)
        capturingData->previousMouseButton = newButton;
    else if (pointerEvent)
        ensureCapturingDataForPointerEvent(*pointerEvent)->previousMouseButton = newButton;

    return pointerEvent;
}

void PointerCaptureController::dispatchEvent(PointerEvent& event, EventTarget* target)
{
    if (!target || event.target())
        return;

    // https://w3c.github.io/pointerevents/#firing-events-using-the-pointerevent-interface
    // If the event is not gotpointercapture or lostpointercapture, run Process Pending Pointer Capture steps for this PointerEvent.
    // We only need to do this for non-mouse type since for mouse events this method will be called in Document::prepareMouseEvent().
    if (event.pointerType() != mousePointerEventType())
        processPendingPointerCapture(event.pointerId());

    pointerEventWillBeDispatched(event, target);
    target->dispatchEvent(event);
    pointerEventWasDispatched(event);
}

void PointerCaptureController::pointerEventWillBeDispatched(const PointerEvent& event, EventTarget* target)
{
    if (!is<Element>(target))
        return;

    bool isPointerdown = event.type() == eventNames().pointerdownEvent;
    bool isPointerup = event.type() == eventNames().pointerupEvent;
    bool isPointermove = event.type() == eventNames().pointermoveEvent;

    if (!isPointerdown && !isPointerup && !isPointermove)
        return;

    auto& element = downcast<Element>(*target);

    // Let targetDocument be target's node document.
    // If the event is pointerdown, pointermove, or pointerup set active document for the event's pointerId to targetDocument.
    auto capturingData = ensureCapturingDataForPointerEvent(event);
    capturingData->activeDocument = element.document();

    if (isPointermove)
        return;

    capturingData->pointerIsPressed = isPointerdown;

    if (event.pointerType() == touchPointerEventType() && isPointerdown) {
        // https://w3c.github.io/pointerevents/#implicit-pointer-capture

        // Some input devices (such as touchscreens) implement a "direct manipulation" metaphor where a pointer is intended to act primarily on the UI
        // element it became active upon (providing a physical illusion of direct contact, instead of indirect contact via a cursor that conceptually
        // floats above the UI). Such devices are identified by the InputDeviceCapabilities.pointerMovementScrolls property and should have "implicit
        // pointer capture" behavior as follows.

        // Direct manipulation devices should behave exactly as if setPointerCapture was called on the target element just before the invocation of any
        // pointerdown listeners. The hasPointerCapture API may be used (eg. within any pointerdown listener) to determine whether this has occurred. If
        // releasePointerCapture is not called for the pointer before the next pointer event is fired, then a gotpointercapture event will be dispatched
        // to the target (as normal) indicating that capture is active.
        setPointerCapture(&element, event.pointerId());
    }
    element.document().handlePopoverLightDismiss(event, element);
}

auto PointerCaptureController::ensureCapturingDataForPointerEvent(const PointerEvent& event) -> Ref<CapturingData>
{
    return m_activePointerIdsToCapturingData.ensure(event.pointerId(), [pointerType = event.pointerType()] {
        return CapturingData::create(pointerType);
    }).iterator->value;
}

void PointerCaptureController::pointerEventWasDispatched(const PointerEvent& event)
{
    RefPtr capturingData = m_activePointerIdsToCapturingData.get(event.pointerId());
    if (!capturingData)
        return;

    capturingData->isPrimary = event.isPrimary();

    // Immediately after firing the pointerup or pointercancel events, a user agent MUST clear the pending pointer capture target
    // override for the pointerId of the pointerup or pointercancel event that was just dispatched, and then run Process Pending
    // Pointer Capture steps to fire lostpointercapture if necessary.
    // https://w3c.github.io/pointerevents/#implicit-release-of-pointer-capture
    if (event.type() == eventNames().pointerupEvent) {
        capturingData->pendingTargetOverride = nullptr;
        processPendingPointerCapture(event.pointerId());
    }

    // If a mouse pointer has moved while it isn't pressed, make sure we reset the preventsCompatibilityMouseEvents flag since
    // we could otherwise prevent compatibility mouse events while those are only supposed to be prevented while the pointer is pressed.
    if (event.type() == eventNames().pointermoveEvent && capturingData->pointerType == mousePointerEventType() && !capturingData->pointerIsPressed)
        capturingData->preventsCompatibilityMouseEvents = false;

    // If the pointer event dispatched was pointerdown and the event was canceled, then set the PREVENT MOUSE EVENT flag for this pointerType.
    // https://www.w3.org/TR/pointerevents/#mapping-for-devices-that-support-hover
    if (event.type() == eventNames().pointerdownEvent)
        capturingData->preventsCompatibilityMouseEvents = event.defaultPrevented();
}

void PointerCaptureController::cancelPointer(PointerID pointerId, const IntPoint& documentPoint, PointerEvent* existingCancelEvent)
{
    // https://w3c.github.io/pointerevents/#the-pointercancel-event

    // A user agent MUST fire a pointer event named pointercancel in the following circumstances:
    //
    // The user agent has determined that a pointer is unlikely to continue to produce events (for example, because of a hardware event).
    // After having fired the pointerdown event, if the pointer is subsequently used to manipulate the page viewport (e.g. panning or zooming).
    // Immediately before drag operation starts [HTML], for the pointer that caused the drag operation.
    // After firing the pointercancel event, a user agent MUST also fire a pointer event named pointerout followed by firing a pointer event named pointerleave.

    // https://w3c.github.io/pointerevents/#implicit-release-of-pointer-capture

    // Immediately after firing the pointerup or pointercancel events, a user agent MUST clear the pending pointer capture target
    // override for the pointerId of the pointerup or pointercancel event that was just dispatched, and then run Process Pending
    // Pointer Capture steps to fire lostpointercapture if necessary. After running Process Pending Pointer Capture steps, if the
    // pointer supports hover, user agent MUST also send corresponding boundary events necessary to reflect the current position of
    // the pointer with no capture.

    RefPtr capturingData = m_activePointerIdsToCapturingData.get(pointerId);
    if (!capturingData)
        return;

    if (capturingData->state == CapturingData::State::Cancelled)
        return;

    RefPtr page = m_page.get();
    if (!page)
        return;

    capturingData->pendingTargetOverride = nullptr;
    capturingData->state = CapturingData::State::Cancelled;

#if ENABLE(TOUCH_EVENTS) && (PLATFORM(IOS_FAMILY) || PLATFORM(WPE) || PLATFORM(GTK))
    capturingData->previousTarget = nullptr;
#endif

    auto target = [&]() -> RefPtr<Element> {
        if (capturingData->targetOverride)
            return capturingData->targetOverride;
        constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
        RefPtr localMainFrame = page->localMainFrame();
        if (!localMainFrame)
            return nullptr;
        return localMainFrame->eventHandler().hitTestResultAtPoint(documentPoint, hitType).innerNonSharedElement();
    }();

    if (!target)
        return;

    // After firing the pointercancel event, a user agent MUST also fire a pointer event named pointerout
    // followed by firing a pointer event named pointerleave.
    auto isPrimary = capturingData->isPrimary ? PointerEvent::IsPrimary::Yes : PointerEvent::IsPrimary::No;
    auto& eventNames = WebCore::eventNames();

    if (existingCancelEvent)
        target->dispatchEvent(*existingCancelEvent);
    else {
        auto cancelEvent = PointerEvent::create(eventNames.pointercancelEvent, pointerId, capturingData->pointerType, isPrimary);
        target->dispatchEvent(cancelEvent);
    }

    target->dispatchEvent(PointerEvent::create(eventNames.pointeroutEvent, pointerId, capturingData->pointerType, isPrimary));
    target->dispatchEvent(PointerEvent::create(eventNames.pointerleaveEvent, pointerId, capturingData->pointerType, isPrimary));
    processPendingPointerCapture(pointerId);
}

void PointerCaptureController::processPendingPointerCapture(PointerID pointerId)
{
    RefPtr capturingData = m_activePointerIdsToCapturingData.get(pointerId);
    if (!capturingData)
        return;

    if (m_processingPendingPointerCapture)
        return;

    m_processingPendingPointerCapture = true;

    // Cache the pending target override since it could be modified during the dispatch of events in this function.
    auto pendingTargetOverride = capturingData->pendingTargetOverride;

    // https://w3c.github.io/pointerevents/#process-pending-pointer-capture
    // 1. If the pointer capture target override for this pointer is set and is not equal to the pending pointer capture target override,
    // then fire a pointer event named lostpointercapture at the pointer capture target override node.
    if (auto targetOverride = capturingData->targetOverride; targetOverride && targetOverride != pendingTargetOverride) {
        if (capturingData->targetOverride->isConnected())
            capturingData->targetOverride->dispatchEvent(PointerEvent::createForPointerCapture(eventNames().lostpointercaptureEvent, pointerId, capturingData->isPrimary, capturingData->pointerType));
        if (capturingData->pointerType == mousePointerEventType()) {
            if (RefPtr frame = capturingData->targetOverride->document().frame())
                frame->eventHandler().pointerCaptureElementDidChange(nullptr);
        }
    }

    // 2. If the pending pointer capture target override for this pointer is set and is not equal to the pointer capture target override,
    // then fire a pointer event named gotpointercapture at the pending pointer capture target override.
    if (capturingData->pendingTargetOverride && capturingData->targetOverride != pendingTargetOverride) {
        if (capturingData->pointerType == mousePointerEventType()) {
            if (RefPtr frame = pendingTargetOverride->document().frame())
                frame->eventHandler().pointerCaptureElementDidChange(pendingTargetOverride.get());
        }
        pendingTargetOverride->dispatchEvent(PointerEvent::createForPointerCapture(eventNames().gotpointercaptureEvent, pointerId, capturingData->isPrimary, capturingData->pointerType));
    }

    // 3. Set the pointer capture target override to the pending pointer capture target override, if set. Otherwise, clear the pointer
    // capture target override.
    capturingData->targetOverride = pendingTargetOverride;

    m_processingPendingPointerCapture = false;
}

} // namespace WebCore
