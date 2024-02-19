/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Google Inc. All rights reserved.
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
#include "HTMLIFrameElement.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertNullable.h"
#include "JSEventTarget.h"
#include "JSEventTargetCustom.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "PlatformMouseEvent.h"
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MouseEvent);

Ref<MouseEvent> MouseEvent::create(const AtomString& type, const MouseEventInit& initializer)
{
    return adoptRef(*new MouseEvent(type, initializer));
}

Ref<MouseEvent> MouseEvent::create(const AtomString& eventType, RefPtr<WindowProxy>&& view, const PlatformMouseEvent& event, int detail, Node* relatedTarget)
{
    auto& eventNames = WebCore::eventNames();
    bool isMouseEnterOrLeave = eventType == eventNames.mouseenterEvent || eventType == eventNames.mouseleaveEvent;
    auto isCancelable = !isMouseEnterOrLeave ? IsCancelable::Yes : IsCancelable::No;
    auto canBubble = !isMouseEnterOrLeave ? CanBubble::Yes : CanBubble::No;
    auto isComposed = !isMouseEnterOrLeave ? IsComposed::Yes : IsComposed::No;

    return MouseEvent::create(eventType, canBubble, isCancelable, isComposed, event.timestamp().approximateMonotonicTime(), WTFMove(view), detail,
        event.globalPosition(), event.position(), event.movementDelta().x(), event.movementDelta().y(),
        event.modifiers(), event.button(), event.buttons(), relatedTarget, event.force(), event.syntheticClickType());
}

Ref<MouseEvent> MouseEvent::create(const AtomString& type, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed, MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail,
    const IntPoint& screenLocation, const IntPoint& windowLocation, double movementX, double movementY, OptionSet<Modifier> modifiers, MouseButton button, unsigned short buttons,
    EventTarget* relatedTarget, double force, SyntheticClickType syntheticClickType, IsSimulated isSimulated, IsTrusted isTrusted)
{
    return adoptRef(*new MouseEvent(type, canBubble, isCancelable, isComposed, timestamp, WTFMove(view), detail,
        screenLocation, windowLocation, movementX, movementY, modifiers, button, buttons, relatedTarget, force, syntheticClickType, isSimulated, isTrusted));
}

Ref<MouseEvent> MouseEvent::create(const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed, RefPtr<WindowProxy>&& view, int detail,
    int screenX, int screenY, int clientX, int clientY, OptionSet<Modifier> modifiers, MouseButton button, unsigned short buttons,
    SyntheticClickType syntheticClickType, EventTarget* relatedTarget)
{
    return adoptRef(*new MouseEvent(eventType, canBubble, isCancelable, isComposed, WTFMove(view), detail, { screenX, screenY }, { clientX, clientY }, 0, 0, modifiers, button, buttons, syntheticClickType, relatedTarget));
}

MouseEvent::MouseEvent() = default;

MouseEvent::MouseEvent(const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed,
    MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail,
    const IntPoint& screenLocation, const IntPoint& windowLocation, double movementX, double movementY, OptionSet<Modifier> modifiers, MouseButton button, unsigned short buttons,
    EventTarget* relatedTarget, double force, SyntheticClickType syntheticClickType, IsSimulated isSimulated, IsTrusted isTrusted)
    : MouseRelatedEvent(eventType, canBubble, isCancelable, isComposed, timestamp, WTFMove(view), detail, screenLocation, windowLocation, movementX, movementY, modifiers, isSimulated, isTrusted)
    , m_button(enumToUnderlyingType(button == MouseButton::None ? MouseButton::Left : button))
    , m_buttons(buttons)
    , m_syntheticClickType(button == MouseButton::None ? SyntheticClickType::NoTap : syntheticClickType)
    , m_buttonDown(button != MouseButton::None)
    , m_relatedTarget(relatedTarget)
    , m_force(force)
{
}

MouseEvent::MouseEvent(const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed,
    RefPtr<WindowProxy>&& view, int detail, const IntPoint& screenLocation, const IntPoint& clientLocation, double movementX, double movementY,
    OptionSet<Modifier> modifiers, MouseButton button, unsigned short buttons, SyntheticClickType syntheticClickType, EventTarget* relatedTarget)
    : MouseRelatedEvent(eventType, canBubble, isCancelable, isComposed, MonotonicTime::now(), WTFMove(view), detail, screenLocation, { }, movementX, movementY, modifiers, IsSimulated::No)
    , m_button(enumToUnderlyingType(button == MouseButton::None ? MouseButton::Left : button))
    , m_buttons(buttons)
    , m_syntheticClickType(button == MouseButton::None ? SyntheticClickType::NoTap : syntheticClickType)
    , m_buttonDown(button != MouseButton::None)
    , m_relatedTarget(relatedTarget)
{
    initCoordinates(clientLocation);
}

MouseEvent::MouseEvent(const AtomString& eventType, const MouseEventInit& initializer)
    : MouseRelatedEvent(eventType, initializer)
    , m_button(initializer.button == enumToUnderlyingType(MouseButton::None) ? enumToUnderlyingType(MouseButton::Left) : initializer.button)
    , m_buttons(initializer.buttons)
    , m_buttonDown(initializer.button != enumToUnderlyingType(MouseButton::None))
    , m_relatedTarget(initializer.relatedTarget)
{
    initCoordinates({ initializer.clientX, initializer.clientY });
}

MouseEvent::~MouseEvent() = default;

void MouseEvent::initMouseEvent(const AtomString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view, int detail,
    int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, int16_t button, EventTarget* relatedTarget)
{
    if (isBeingDispatched())
        return;

    initUIEvent(type, canBubble, cancelable, WTFMove(view), detail);

    m_screenLocation = IntPoint(screenX, screenY);
    setModifierKeys(ctrlKey, altKey, shiftKey, metaKey);
    m_button = button == enumToUnderlyingType(MouseButton::None) ? enumToUnderlyingType(MouseButton::Left) : button;
    m_syntheticClickType = SyntheticClickType::NoTap;
    m_buttonDown = button != enumToUnderlyingType(MouseButton::None);
    m_relatedTarget = relatedTarget;

    initCoordinates(IntPoint(clientX, clientY));

    setIsSimulated(false);
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
    if (event.type() != eventNames().clickEvent)
        return false;
    auto* mouseEvent = dynamicDowncast<MouseEvent>(event);
    return !mouseEvent || mouseEvent->button() != MouseButton::Right;
}

MouseButton MouseEvent::button() const
{
    static constexpr std::array mouseButtonCases { MouseButton::None, MouseButton::PointerHasNotChanged, MouseButton::Left, MouseButton::Middle, MouseButton::Right };
    const auto isKnownButton = WTF::anyOf(mouseButtonCases, [buttonValue = this->m_button](MouseButton button) {
        return buttonValue == enumToUnderlyingType(button);
    });
    return isKnownButton ? static_cast<MouseButton>(m_button) : MouseButton::Other;
}

unsigned MouseEvent::which() const
{
    // For the DOM, the return values for left, middle and right mouse buttons are 0, 1, 2, respectively.
    // For the Netscape "which" property, the return values for left, middle and right mouse buttons are 1, 2, 3, respectively.
    // So we must add 1.
    if (!m_buttonDown)
        return 0;
    return static_cast<unsigned>(m_button + 1);
}

RefPtr<Node> MouseEvent::toElement() const
{
    // MSIE extension - "the object toward which the user is moving the mouse pointer"
    RefPtr<EventTarget> target;
    auto& eventNames = WebCore::eventNames();
    if (type() == eventNames.mouseoutEvent || type() == eventNames.mouseleaveEvent)
        target = relatedTarget();
    else
        target = this->target();
    return dynamicDowncast<Node>(WTFMove(target));
}

RefPtr<Node> MouseEvent::fromElement() const
{
    // MSIE extension - "object from which activation or the mouse pointer is exiting during the event" (huh?)
    RefPtr<EventTarget> target;
    auto& eventNames = WebCore::eventNames();
    if (type() == eventNames.mouseoutEvent || type() == eventNames.mouseleaveEvent)
        target = this->target();
    else
        target = relatedTarget();
    return dynamicDowncast<Node>(WTFMove(target));
}

} // namespace WebCore
