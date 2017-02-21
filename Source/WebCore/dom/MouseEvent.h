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

#include "MouseEventInit.h"
#include "MouseRelatedEvent.h"

namespace WebCore {

class DataTransfer;
class PlatformMouseEvent;

class MouseEvent : public MouseRelatedEvent {
public:
    WEBCORE_EXPORT static Ref<MouseEvent> create(const AtomicString& type, bool canBubble, bool cancelable, double timestamp, DOMWindow*, int detail, int screenX, int screenY, int pageX, int pageY,
#if ENABLE(POINTER_LOCK)
        int movementX, int movementY,
#endif
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button, EventTarget* relatedTarget, double force, unsigned short syntheticClickType, DataTransfer* = nullptr, bool isSimulated = false);

    WEBCORE_EXPORT static Ref<MouseEvent> create(const AtomicString& eventType, DOMWindow*, const PlatformMouseEvent&, int detail, Node* relatedTarget);

    static Ref<MouseEvent> create(const AtomicString& eventType, bool canBubble, bool cancelable, DOMWindow*, int detail, int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button, unsigned short syntheticClickType, EventTarget* relatedTarget);

    static Ref<MouseEvent> createForBindings() { return adoptRef(*new MouseEvent); }

    static Ref<MouseEvent> create(const AtomicString& eventType, const MouseEventInit&, IsTrusted = IsTrusted::No);

    virtual ~MouseEvent();

    WEBCORE_EXPORT void initMouseEvent(const AtomicString& type, bool canBubble, bool cancelable, DOMWindow*, int detail, int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button, EventTarget* relatedTarget);
    void initMouseEventQuirk(JSC::ExecState&, ScriptExecutionContext&, const AtomicString& type, bool canBubble, bool cancelable, DOMWindow*, int detail, int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button, JSC::JSValue relatedTarget);
    // WinIE uses 1,4,2 for left/middle/right but not for click (just for mousedown/up, maybe others),
    // but we will match the standard DOM.
    unsigned short button() const { return m_button; }
    unsigned short syntheticClickType() const { return m_syntheticClickType; }
    bool buttonDown() const { return m_buttonDown; }
    EventTarget* relatedTarget() const final { return m_relatedTarget.get(); }
    void setRelatedTarget(EventTarget* relatedTarget) { m_relatedTarget = relatedTarget; }
    double force() const { return m_force; }
    void setForce(double force) { m_force = force; }

    WEBCORE_EXPORT Node* toElement() const;
    WEBCORE_EXPORT Node* fromElement() const;

    DataTransfer* dataTransfer() const { return isDragEvent() ? m_dataTransfer.get() : nullptr; }

    static bool canTriggerActivationBehavior(const Event&);

    int which() const final;

protected:
    MouseEvent(const AtomicString& type, bool canBubble, bool cancelable, double timestamp, DOMWindow*,
        int detail, const IntPoint& screenLocation, const IntPoint& windowLocation,
#if ENABLE(POINTER_LOCK)
        const IntPoint& movementDelta,
#endif
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button,
        EventTarget* relatedTarget, double force, unsigned short syntheticClickType, DataTransfer*, bool isSimulated);

    MouseEvent(const AtomicString& type, bool canBubble, bool cancelable, DOMWindow*,
        int detail, const IntPoint& screenLocation, const IntPoint& clientLocation,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        unsigned short button, unsigned short syntheticClickType, EventTarget* relatedTarget);

    MouseEvent(const AtomicString& type, const MouseEventInit&, IsTrusted);

    MouseEvent();

private:
    bool isMouseEvent() const final;
    EventInterface eventInterface() const override;

    bool isDragEvent() const;

    unsigned short m_button { 0 };
    unsigned short m_syntheticClickType { 0 };
    bool m_buttonDown { false };
    RefPtr<EventTarget> m_relatedTarget;
    double m_force { 0 };
    RefPtr<DataTransfer> m_dataTransfer;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENT(MouseEvent)
