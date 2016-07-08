/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2013 Apple Inc. All rights reserved.
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

#ifndef MouseEvent_h
#define MouseEvent_h

#include "MouseRelatedEvent.h"

namespace WebCore {

class DataTransfer;
class PlatformMouseEvent;

struct MouseEventInit : public MouseRelatedEventInit {
    int clientX { 0 };
    int clientY { 0 };
    unsigned short button { 0 };
    RefPtr<EventTarget> relatedTarget;
};

class MouseEvent : public MouseRelatedEvent {
public:
    static Ref<MouseEvent> create(const AtomicString& type, bool canBubble, bool cancelable, double timestamp, AbstractView*,
        int detail, int screenX, int screenY, int pageX, int pageY,
#if ENABLE(POINTER_LOCK)
        int movementX, int movementY,
#endif
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button,
        PassRefPtr<EventTarget> relatedTarget, double force);

    WEBCORE_EXPORT static Ref<MouseEvent> create(const AtomicString& type, bool canBubble, bool cancelable, double timestamp, AbstractView*,
        int detail, int screenX, int screenY, int pageX, int pageY,
#if ENABLE(POINTER_LOCK)
        int movementX, int movementY,
#endif
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button,
        PassRefPtr<EventTarget> relatedTarget, double force, PassRefPtr<DataTransfer>, bool isSimulated = false);

    WEBCORE_EXPORT static Ref<MouseEvent> create(const AtomicString& eventType, AbstractView*, const PlatformMouseEvent&, int detail, PassRefPtr<Node> relatedTarget);

    static Ref<MouseEvent> create(const AtomicString& eventType, bool canBubble, bool cancelable, AbstractView*,
        int detail, int screenX, int screenY, int clientX, int clientY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        unsigned short button, PassRefPtr<EventTarget> relatedTarget);

    static Ref<MouseEvent> createForBindings()
    {
        return adoptRef(*new MouseEvent);
    }

    static Ref<MouseEvent> createForBindings(const AtomicString& eventType, const MouseEventInit&);

    virtual ~MouseEvent();

    void initMouseEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView*,
        int detail, int screenX, int screenY, int clientX, int clientY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        unsigned short button, PassRefPtr<EventTarget> relatedTarget);

    // WinIE uses 1,4,2 for left/middle/right but not for click (just for mousedown/up, maybe others),
    // but we will match the standard DOM.
    unsigned short button() const { return m_button; }
    bool buttonDown() const { return m_buttonDown; }
    EventTarget* relatedTarget() const final { return m_relatedTarget.get(); }
    void setRelatedTarget(PassRefPtr<EventTarget> relatedTarget) { m_relatedTarget = relatedTarget; }
    double force() const { return m_force; }
    void setForce(double force) { m_force = force; }

    Node* toElement() const;
    Node* fromElement() const;

    // FIXME: These functions can be merged if m_dataTransfer is only initialized for drag events.
    DataTransfer* dataTransfer() const { return isDragEvent() ? m_dataTransfer.get() : nullptr; }
    DataTransfer* internalDataTransfer() const override { return m_dataTransfer.get(); }

    EventInterface eventInterface() const override;

    bool isMouseEvent() const override;
    bool isDragEvent() const override;
    static bool canTriggerActivationBehavior(const Event&); 

    int which() const override;

    Ref<Event> cloneFor(HTMLIFrameElement*) const override;

protected:
    MouseEvent(const AtomicString& type, bool canBubble, bool cancelable, double timestamp, AbstractView*,
        int detail, int screenX, int screenY, int pageX, int pageY,
#if ENABLE(POINTER_LOCK)
        int movementX, int movementY,
#endif
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button,
        PassRefPtr<EventTarget> relatedTarget, double force, PassRefPtr<DataTransfer>, bool isSimulated);

    MouseEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView*,
        int detail, int screenX, int screenY, int clientX, int clientY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        unsigned short button, PassRefPtr<EventTarget> relatedTarget);

    MouseEvent(const AtomicString& type, const MouseEventInit&);

    MouseEvent();

private:
    unsigned short m_button;
    bool m_buttonDown;
    RefPtr<EventTarget> m_relatedTarget;
    double m_force { 0 };
    RefPtr<DataTransfer> m_dataTransfer;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENT(MouseEvent)

#endif // MouseEvent_h
