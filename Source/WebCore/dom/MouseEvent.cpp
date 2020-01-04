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
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MouseEvent);

using namespace JSC;

Ref<MouseEvent> MouseEvent::create(const AtomString& type, const MouseEventInit& initializer)
{
    return adoptRef(*new MouseEvent(type, initializer));
}

Ref<MouseEvent> MouseEvent::create(const AtomString& eventType, RefPtr<WindowProxy>&& view, const PlatformMouseEvent& event, int detail, Node* relatedTarget)
{
    bool isMouseEnterOrLeave = eventType == eventNames().mouseenterEvent || eventType == eventNames().mouseleaveEvent;
    auto isCancelable = eventType != eventNames().mousemoveEvent && !isMouseEnterOrLeave ? IsCancelable::Yes : IsCancelable::No;
    auto canBubble = !isMouseEnterOrLeave ? CanBubble::Yes : CanBubble::No;
    auto isComposed = !isMouseEnterOrLeave ? IsComposed::Yes : IsComposed::No;

    return MouseEvent::create(eventType, canBubble, isCancelable, isComposed, event.timestamp().approximateMonotonicTime(), WTFMove(view), detail,
        event.globalPosition(), event.position(),
#if ENABLE(POINTER_LOCK)
        event.movementDelta(),
#else
        { },
#endif
        event.modifiers(), event.button(), event.buttons(), relatedTarget, event.force(), event.syntheticClickType());
}

Ref<MouseEvent> MouseEvent::create(const AtomString& type, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed, MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail,
    const IntPoint& screenLocation, const IntPoint& windowLocation, const IntPoint& movementDelta, OptionSet<Modifier> modifiers, short button, unsigned short buttons,
    EventTarget* relatedTarget, double force, unsigned short syntheticClickType, IsSimulated isSimulated, IsTrusted isTrusted)
{
    return adoptRef(*new MouseEvent(type, canBubble, isCancelable, isComposed, timestamp, WTFMove(view), detail,
        screenLocation, windowLocation, movementDelta, modifiers, button, buttons, relatedTarget, force, syntheticClickType, isSimulated, isTrusted));
}

Ref<MouseEvent> MouseEvent::create(const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed, RefPtr<WindowProxy>&& view, int detail,
    int screenX, int screenY, int clientX, int clientY, OptionSet<Modifier> modifiers, short button, unsigned short buttons,
    unsigned short syntheticClickType, EventTarget* relatedTarget)
{
    return adoptRef(*new MouseEvent(eventType, canBubble, isCancelable, isComposed, WTFMove(view), detail, { screenX, screenY }, { clientX, clientY }, modifiers, button, buttons, syntheticClickType, relatedTarget));
}

MouseEvent::MouseEvent() = default;

MouseEvent::MouseEvent(const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed,
    MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail,
    const IntPoint& screenLocation, const IntPoint& windowLocation, const IntPoint& movementDelta, OptionSet<Modifier> modifiers, short button, unsigned short buttons,
    EventTarget* relatedTarget, double force, unsigned short syntheticClickType, IsSimulated isSimulated, IsTrusted isTrusted)
    : MouseRelatedEvent(eventType, canBubble, isCancelable, isComposed, timestamp, WTFMove(view), detail, screenLocation, windowLocation, movementDelta, modifiers, isSimulated, isTrusted)
    , m_button(button == -2 ? 0 : button)
    , m_buttons(buttons)
    , m_syntheticClickType(button == -2 ? 0 : syntheticClickType)
    , m_buttonDown(button != -2)
    , m_relatedTarget(relatedTarget)
    , m_force(force)
{
}

MouseEvent::MouseEvent(const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed,
    RefPtr<WindowProxy>&& view, int detail, const IntPoint& screenLocation, const IntPoint& clientLocation,
    OptionSet<Modifier> modifiers, short button, unsigned short buttons, unsigned short syntheticClickType, EventTarget* relatedTarget)
    : MouseRelatedEvent(eventType, canBubble, isCancelable, isComposed, MonotonicTime::now(), WTFMove(view), detail, screenLocation, { }, { }, modifiers, IsSimulated::No)
    , m_button(button == -2 ? 0 : button)
    , m_buttons(buttons)
    , m_syntheticClickType(button == -2 ? 0 : syntheticClickType)
    , m_buttonDown(button != -2)
    , m_relatedTarget(relatedTarget)
{
    initCoordinates(clientLocation);
}

MouseEvent::MouseEvent(const AtomString& eventType, const MouseEventInit& initializer)
    : MouseRelatedEvent(eventType, initializer)
    , m_button(initializer.button == -2 ? 0 : initializer.button)
    , m_buttons(initializer.buttons)
    , m_buttonDown(initializer.button != -2)
    , m_relatedTarget(initializer.relatedTarget)
{
    initCoordinates({ initializer.clientX, initializer.clientY });
}

MouseEvent::~MouseEvent() = default;

void MouseEvent::initMouseEvent(const AtomString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view, int detail,
    int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, short button, EventTarget* relatedTarget)
{
    if (isBeingDispatched())
        return;

    initUIEvent(type, canBubble, cancelable, WTFMove(view), detail);

    m_screenLocation = IntPoint(screenX, screenY);
    setModifierKeys(ctrlKey, altKey, shiftKey, metaKey);
    m_button = button == -2 ? 0 : button;
    m_syntheticClickType = 0;
    m_buttonDown = button != -2;
    m_relatedTarget = relatedTarget;

    initCoordinates(IntPoint(clientX, clientY));

    setIsSimulated(false);
}

// FIXME: We need this quirk because iAd Producer is calling this function with a relatedTarget that is not an EventTarget (rdar://problem/30640101).
// We should remove this quirk when possible.
void MouseEvent::initMouseEventQuirk(JSGlobalObject& state, ScriptExecutionContext& scriptExecutionContext, const AtomString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view, int detail, int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, short button, JSValue relatedTargetValue)
{
    EventTarget* relatedTarget = nullptr;
#if PLATFORM(MAC)
    // Impacts iBooks too because of widgets generated by iAd Producer (rdar://problem/30797958).
    if (MacApplication::isIAdProducer() || MacApplication::isIBooks()) {
        // jsEventTargetCast() does not throw and will silently convert bad input to nullptr.
        auto jsRelatedTarget = jsEventTargetCast(state.vm(), relatedTargetValue);
        if (!jsRelatedTarget && !relatedTargetValue.isUndefinedOrNull())
            scriptExecutionContext.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "Calling initMouseEvent() with a relatedTarget that is not an EventTarget is deprecated."_s);
        relatedTarget = jsRelatedTarget ? &jsRelatedTarget->wrapped() : nullptr;
    } else {
#else
    UNUSED_PARAM(scriptExecutionContext);
#endif
        // This is what the bindings generator would have produced.
        auto throwScope = DECLARE_THROW_SCOPE(state.vm());
        relatedTarget = convert<IDLNullable<IDLInterface<EventTarget>>>(state, relatedTargetValue, [](JSGlobalObject& state, ThrowScope& scope) {
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
