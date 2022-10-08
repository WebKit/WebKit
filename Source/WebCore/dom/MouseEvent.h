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
 *
 */

#pragma once

#include "EventTarget.h"
#include "MouseEventInit.h"
#include "MouseRelatedEvent.h"

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
#include "PlatformTouchEventIOS.h"
#endif

namespace JSC {
class CallFrame;
class JSValue;
}

namespace WebCore {

class Node;
class PlatformMouseEvent;

class MouseEvent : public MouseRelatedEvent {
    WTF_MAKE_ISO_ALLOCATED(MouseEvent);
public:
    WEBCORE_EXPORT static Ref<MouseEvent> create(const AtomString& type, CanBubble, IsCancelable, IsComposed, MonotonicTime timestamp, RefPtr<WindowProxy>&&, int detail,
        const IntPoint& screenLocation, const IntPoint& windowLocation, double movementX, double movementY, OptionSet<Modifier>, short button, unsigned short buttons,
        EventTarget* relatedTarget, double force, unsigned short syntheticClickType, IsSimulated = IsSimulated::No, IsTrusted = IsTrusted::Yes);

    WEBCORE_EXPORT static Ref<MouseEvent> create(const AtomString& eventType, RefPtr<WindowProxy>&&, const PlatformMouseEvent&, int detail, Node* relatedTarget);

    static Ref<MouseEvent> create(const AtomString& eventType, CanBubble, IsCancelable, IsComposed, RefPtr<WindowProxy>&&, int detail,
        int screenX, int screenY, int clientX, int clientY, OptionSet<Modifier>, short button, unsigned short buttons,
        unsigned short syntheticClickType, EventTarget* relatedTarget);

    static Ref<MouseEvent> createForBindings() { return adoptRef(*new MouseEvent); }

    static Ref<MouseEvent> create(const AtomString& eventType, const MouseEventInit&);

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
    static Ref<MouseEvent> create(const PlatformTouchEvent&, unsigned touchIndex, Ref<WindowProxy>&&, IsCancelable = IsCancelable::Yes);
#endif

    virtual ~MouseEvent();

    WEBCORE_EXPORT void initMouseEvent(const AtomString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&&,
        int detail, int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        short button, EventTarget* relatedTarget);

    void initMouseEventQuirk(JSC::JSGlobalObject&, ScriptExecutionContext&, const AtomString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&&,
        int detail, int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        short button, JSC::JSValue relatedTarget);

    short button() const { return m_button; }
    unsigned short buttons() const { return m_buttons; }
    unsigned short syntheticClickType() const { return m_syntheticClickType; }
    bool buttonDown() const { return m_buttonDown; }
    EventTarget* relatedTarget() const final { return m_relatedTarget.get(); }
    double force() const { return m_force; }
    void setForce(double force) { m_force = force; }

    WEBCORE_EXPORT virtual RefPtr<Node> toElement() const;
    WEBCORE_EXPORT virtual RefPtr<Node> fromElement() const;

    static bool canTriggerActivationBehavior(const Event&);

    int which() const final;

protected:
    MouseEvent(const AtomString& type, CanBubble, IsCancelable, IsComposed, MonotonicTime timestamp, RefPtr<WindowProxy>&&, int detail,
        const IntPoint& screenLocation, const IntPoint& windowLocation, double movementX, double movementY, OptionSet<Modifier>, short button, unsigned short buttons,
        EventTarget* relatedTarget, double force, unsigned short syntheticClickType, IsSimulated, IsTrusted);

    MouseEvent(const AtomString& type, CanBubble, IsCancelable, IsComposed, RefPtr<WindowProxy>&&, int detail,
        const IntPoint& screenLocation, const IntPoint& clientLocation, double movementX, double movementY, OptionSet<Modifier>, short button, unsigned short buttons,
        unsigned short syntheticClickType, EventTarget* relatedTarget);

    MouseEvent(const AtomString& type, const MouseEventInit&);

    MouseEvent();

private:
    bool isMouseEvent() const final;
    EventInterface eventInterface() const override;

    void setRelatedTarget(RefPtr<EventTarget>&& relatedTarget) final { m_relatedTarget = WTFMove(relatedTarget); }

    short m_button { 0 };
    unsigned short m_buttons { 0 };
    unsigned short m_syntheticClickType { 0 };
    bool m_buttonDown { false };
    RefPtr<EventTarget> m_relatedTarget;
    double m_force { 0 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENT(MouseEvent)
