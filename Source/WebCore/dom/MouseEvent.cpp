/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003-2016 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "MouseEvent.h"

#include "DataTransfer.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLIFrameElement.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertNullable.h"
#include "JSEventTarget.h"
#include "JSEventTargetCustom.h"
#include "PlatformMouseEvent.h"
#include "RuntimeApplicationChecks.h"

namespace WebCore {

using namespace JSC;

Ref<MouseEvent> MouseEvent::create(const AtomicString& type, const MouseEventInit& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new MouseEvent(type, initializer, isTrusted));
}

Ref<MouseEvent> MouseEvent::create(const AtomicString& eventType, RefPtr<WindowProxy>&& view, const PlatformMouseEvent& event, int detail, Node* relatedTarget)
{
    bool isMouseEnterOrLeave = eventType == eventNames().mouseenterEvent || eventType == eventNames().mouseleaveEvent;
    bool isCancelable = eventType != eventNames().mousemoveEvent && !isMouseEnterOrLeave;
    bool canBubble = !isMouseEnterOrLeave;

    return MouseEvent::create(eventType, canBubble, isCancelable, event.timestamp().approximateMonotonicTime(), WTFMove(view),
        detail, event.globalPosition().x(), event.globalPosition().y(), event.position().x(), event.position().y(),
#if ENABLE(POINTER_LOCK)
        event.movementDelta().x(), event.movementDelta().y(),
#endif
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(), event.button(), event.buttons(),
        relatedTarget, event.force(), event.syntheticClickType());
}

Ref<MouseEvent> MouseEvent::create(const AtomicString& type, bool canBubble, bool cancelable, MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail, int screenX, int screenY, int pageX, int pageY,
#if ENABLE(POINTER_LOCK)
    int movementX, int movementY,
#endif
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button, unsigned short buttons, EventTarget* relatedTarget, double force, unsigned short syntheticClickType, DataTransfer* dataTransfer, bool isSimulated)
{
    return adoptRef(*new MouseEvent(type, canBubble, cancelable, timestamp, WTFMove(view),
        detail, { screenX, screenY }, { pageX, pageY },
#if ENABLE(POINTER_LOCK)
        { movementX, movementY },
#endif
        ctrlKey, altKey, shiftKey, metaKey, button, buttons, relatedTarget, force, syntheticClickType, dataTransfer, isSimulated));
}

Ref<MouseEvent> MouseEvent::create(const AtomicString& eventType, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view, int detail, int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button, unsigned short buttons, unsigned short syntheticClickType, EventTarget* relatedTarget)
{
    return adoptRef(*new MouseEvent(eventType, canBubble, cancelable, WTFMove(view), detail, { screenX, screenY }, { clientX, clientY }, ctrlKey, altKey, shiftKey, metaKey, button, buttons, syntheticClickType, relatedTarget));
}

MouseEvent::MouseEvent() = default;

MouseEvent::MouseEvent(const AtomicString& eventType, bool canBubble, bool cancelable, MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail, const IntPoint& screenLocation, const IntPoint& windowLocation,
#if ENABLE(POINTER_LOCK)
        const IntPoint& movementDelta,
#endif
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button, unsigned short buttons, EventTarget* relatedTarget, double force, unsigned short syntheticClickType, DataTransfer* dataTransfer, bool isSimulated)
    : MouseRelatedEvent(eventType, canBubble, cancelable, timestamp, WTFMove(view), detail, screenLocation, windowLocation,
#if ENABLE(POINTER_LOCK)
        movementDelta,
#endif
        ctrlKey, altKey, shiftKey, metaKey, isSimulated)
    , m_button(button == (unsigned short)-1 ? 0 : button)
    , m_buttons(buttons)
    , m_syntheticClickType(button == (unsigned short)-1 ? 0 : syntheticClickType)
    , m_buttonDown(button != (unsigned short)-1)
    , m_relatedTarget(relatedTarget)
    , m_force(force)
    , m_dataTransfer(dataTransfer)
{
}

MouseEvent::MouseEvent(const AtomicString& eventType, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view, int detail, const IntPoint& screenLocation, const IntPoint& clientLocation, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button, unsigned short buttons, unsigned short syntheticClickType, EventTarget* relatedTarget)
    : MouseRelatedEvent(eventType, canBubble, cancelable, MonotonicTime::now(), WTFMove(view), detail, screenLocation, { },
#if ENABLE(POINTER_LOCK)
        { },
#endif
        ctrlKey, altKey, shiftKey, metaKey, false)
    , m_button(button == (unsigned short)-1 ? 0 : button)
    , m_buttons(buttons)
    , m_syntheticClickType(button == (unsigned short)-1 ? 0 : syntheticClickType)
    , m_buttonDown(button != (unsigned short)-1)
    , m_relatedTarget(relatedTarget)
{
    initCoordinates(clientLocation);
}

MouseEvent::MouseEvent(const AtomicString& eventType, const MouseEventInit& initializer, IsTrusted isTrusted)
    : MouseRelatedEvent(eventType, initializer, isTrusted)
    , m_button(initializer.button == (unsigned short)-1 ? 0 : initializer.button)
    , m_buttons(initializer.buttons)
    , m_buttonDown(initializer.button != (unsigned short)-1)
    , m_relatedTarget(initializer.relatedTarget)
{
    initCoordinates({ initializer.clientX, initializer.clientY });
}

MouseEvent::~MouseEvent() = default;

void MouseEvent::initMouseEvent(const AtomicString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view, int detail, int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button, EventTarget* relatedTarget)
{
    if (isBeingDispatched())
        return;

    initUIEvent(type, canBubble, cancelable, WTFMove(view), detail);

    m_screenLocation = IntPoint(screenX, screenY);
    m_ctrlKey = ctrlKey;
    m_altKey = altKey;
    m_shiftKey = shiftKey;
    m_metaKey = metaKey;
    m_button = button == (unsigned short)-1 ? 0 : button;
    m_syntheticClickType = 0;
    m_buttonDown = button != (unsigned short)-1;
    m_relatedTarget = relatedTarget;

    initCoordinates(IntPoint(clientX, clientY));

    setIsSimulated(false);
    m_dataTransfer = nullptr;
}

// FIXME: We need this quirk because iAd Producer is calling this function with a relatedTarget that is not an EventTarget (rdar://problem/30640101).
// We should remove this quirk when possible.
void MouseEvent::initMouseEventQuirk(ExecState& state, ScriptExecutionContext& scriptExecutionContext, const AtomicString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view, int detail, int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button, JSValue relatedTargetValue)
{
    EventTarget* relatedTarget = nullptr;
#if PLATFORM(MAC)
    // Impacts iBooks too because of widgets generated by iAd Producer (rdar://problem/30797958).
    if (MacApplication::isIAdProducer() || MacApplication::isIBooks()) {
        // jsEventTargetCast() does not throw and will silently convert bad input to nullptr.
        auto jsRelatedTarget = jsEventTargetCast(state.vm(), relatedTargetValue);
        if (!jsRelatedTarget && !relatedTargetValue.isUndefinedOrNull())
            scriptExecutionContext.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, ASCIILiteral("Calling initMouseEvent() with a relatedTarget that is not an EventTarget is deprecated."));
        relatedTarget = jsRelatedTarget ? &jsRelatedTarget->wrapped() : nullptr;
    } else {
#else
    UNUSED_PARAM(scriptExecutionContext);
#endif
        // This is what the bindings generator would have produced.
        auto throwScope = DECLARE_THROW_SCOPE(state.vm());
        relatedTarget = convert<IDLNullable<IDLInterface<EventTarget>>>(state, relatedTargetValue, [](ExecState& state, ThrowScope& scope) {
            throwArgumentTypeError(state, scope, 14, "relatedTarget", "MouseEvent", "initMouseEvent", "EventTarget");
        });
        RETURN_IF_EXCEPTION(throwScope, void());
#if PLATFORM(MAC)
    }
#endif
    initMouseEvent(type, canBubble, cancelable, WTFMove(view), detail, screenX, screenY, clientX, clientY, ctrlKey, altKey, shiftKey, metaKey, button, relatedTarget);
}

EventInterface MouseEvent::eventInterface() const
{
    return MouseEventInterfaceType;
}

bool MouseEvent::isMouseEvent() const
{
    return true;
}

bool MouseEvent::isDragEvent() const
{
    // This function is only used to decide to return nullptr for dataTransfer even when m_dataTransfer is non-null.
    // FIXME: Is that really valuable? Why will m_dataTransfer be non-null but we need to return null for dataTransfer?
    // Quite peculiar to decide based on the type string; may have been be provided by call to JavaScript constructor.
    auto& type = this->type();
    return type == eventNames().dragEvent
        || type == eventNames().dragendEvent
        || type == eventNames().dragenterEvent
        || type == eventNames().dragleaveEvent
        || type == eventNames().dragoverEvent
        || type == eventNames().dragstartEvent
        || type == eventNames().dropEvent;
}

bool MouseEvent::canTriggerActivationBehavior(const Event& event)
{
    return event.type() == eventNames().clickEvent && (!is<MouseEvent>(event) || downcast<MouseEvent>(event).button() != RightButton);
}

int MouseEvent::which() const
{
    // For the DOM, the return values for left, middle and right mouse buttons are 0, 1, 2, respectively.
    // For the Netscape "which" property, the return values for left, middle and right mouse buttons are 1, 2, 3, respectively.
    // So we must add 1.
    if (!m_buttonDown)
        return 0;
    return m_button + 1;
}

RefPtr<Node> MouseEvent::toElement() const
{
    // MSIE extension - "the object toward which the user is moving the mouse pointer"
    EventTarget* target;
    if (type() == eventNames().mouseoutEvent || type() == eventNames().mouseleaveEvent)
        target = relatedTarget();
    else
        target = this->target();
    return is<Node>(target) ? &downcast<Node>(*target) : nullptr;
}

RefPtr<Node> MouseEvent::fromElement() const
{
    // MSIE extension - "object from which activation or the mouse pointer is exiting during the event" (huh?)
    EventTarget* target;
    if (type() == eventNames().mouseoutEvent || type() == eventNames().mouseleaveEvent)
        target = this->target();
    else
        target = relatedTarget();
    return is<Node>(target) ? &downcast<Node>(*target) : nullptr;
}

} // namespace WebCore
