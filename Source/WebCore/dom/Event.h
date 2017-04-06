/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#ifndef Event_h
#define Event_h

#include "DOMTimeStamp.h"
#include "EventInterfaces.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class DataTransfer;
class EventPath;
class EventTarget;
class HTMLIFrameElement;

struct EventInit {
    bool bubbles { false };
    bool cancelable { false };
    bool composed { false };
};

enum EventInterface {

#define DOM_EVENT_INTERFACE_DECLARE(name) name##InterfaceType,
DOM_EVENT_INTERFACES_FOR_EACH(DOM_EVENT_INTERFACE_DECLARE)
#undef DOM_EVENT_INTERFACE_DECLARE

};

class Event : public ScriptWrappable, public RefCounted<Event> {
public:
    enum PhaseType { 
        NONE                = 0,
        CAPTURING_PHASE     = 1, 
        AT_TARGET           = 2,
        BUBBLING_PHASE      = 3 
    };

    enum EventType {
        MOUSEDOWN           = 1,
        MOUSEUP             = 2,
        MOUSEOVER           = 4,
        MOUSEOUT            = 8,
        MOUSEMOVE           = 16,
        MOUSEDRAG           = 32,
        CLICK               = 64,
        DBLCLICK            = 128,
        KEYDOWN             = 256,
        KEYUP               = 512,
        KEYPRESS            = 1024,
        DRAGDROP            = 2048,
        FOCUS               = 4096,
        BLUR                = 8192,
        SELECT              = 16384,
        CHANGE              = 32768
    };

    static Ref<Event> create(const AtomicString& type, bool canBubble, bool cancelable)
    {
        return adoptRef(*new Event(type, canBubble, cancelable));
    }

    static Ref<Event> createForBindings()
    {
        return adoptRef(*new Event);
    }

    static Ref<Event> createForBindings(const AtomicString& type, const EventInit& initializer)
    {
        return adoptRef(*new Event(type, initializer));
    }

    virtual ~Event();

    WEBCORE_EXPORT void initEvent(const AtomicString& type, bool canBubble, bool cancelable);
    bool isInitialized() const { return m_isInitialized; }

    const AtomicString& type() const { return m_type; }
    void setType(const AtomicString& type) { m_type = type; }
    
    EventTarget* target() const { return m_target.get(); }
    void setTarget(RefPtr<EventTarget>&&);

    EventTarget* currentTarget() const { return m_currentTarget.get(); }
    void setCurrentTarget(EventTarget*);

    unsigned short eventPhase() const { return m_eventPhase; }
    void setEventPhase(unsigned short eventPhase) { m_eventPhase = eventPhase; }

    bool bubbles() const { return m_canBubble; }
    bool cancelable() const { return m_cancelable; }
    WEBCORE_EXPORT bool composed() const;

    DOMTimeStamp timeStamp() const { return m_createTime; }

    void setEventPath(const EventPath& path) { m_eventPath = &path; }
    void clearEventPath() { m_eventPath = nullptr; }
    Vector<EventTarget*> composedPath() const;

    void stopPropagation() { m_propagationStopped = true; }
    void stopImmediatePropagation() { m_immediatePropagationStopped = true; }

    bool isTrusted() const { return m_isTrusted; }
    void setUntrusted() { m_isTrusted = false; }
    
    // IE Extensions
    EventTarget* srcElement() const { return target(); } // MSIE extension - "the object that fired the event"

    bool legacyReturnValue() const { return !defaultPrevented(); }
    void setLegacyReturnValue(bool returnValue) { setDefaultPrevented(!returnValue); }

    DataTransfer* clipboardData() const { return isClipboardEvent() ? internalDataTransfer() : nullptr; }

    virtual EventInterface eventInterface() const;

    // These events are general classes of events.
    virtual bool isUIEvent() const;
    virtual bool isMouseEvent() const;
    virtual bool isFocusEvent() const;
    virtual bool isKeyboardEvent() const;
    virtual bool isCompositionEvent() const;
    virtual bool isTouchEvent() const;

    // Drag events are a subset of mouse events.
    virtual bool isDragEvent() const;

    // These events lack a DOM interface.
    virtual bool isClipboardEvent() const;
    virtual bool isBeforeTextInsertedEvent() const;

    virtual bool isBeforeUnloadEvent() const;

    virtual bool isErrorEvent() const;
    virtual bool isTextEvent() const;
    virtual bool isWheelEvent() const;

#if ENABLE(INDEXED_DATABASE)
    virtual bool isVersionChangeEvent() const { return false; }
#endif

    bool propagationStopped() const { return m_propagationStopped || m_immediatePropagationStopped; }
    bool immediatePropagationStopped() const { return m_immediatePropagationStopped; }

    void resetPropagationFlags();

    bool defaultPrevented() const { return m_defaultPrevented; }
    void preventDefault()
    {
        if (m_cancelable && !m_isExecutingPassiveEventListener)
            m_defaultPrevented = true;
    }
    void setDefaultPrevented(bool defaultPrevented) { m_defaultPrevented = defaultPrevented; }

    bool defaultHandled() const { return m_defaultHandled; }
    void setDefaultHandled() { m_defaultHandled = true; }

    void setInPassiveListener(bool value) { m_isExecutingPassiveEventListener = value; }

    bool cancelBubble() const { return m_cancelBubble; }
    void setCancelBubble(bool cancel) { m_cancelBubble = cancel; }

    Event* underlyingEvent() const { return m_underlyingEvent.get(); }
    void setUnderlyingEvent(Event*);

    virtual DataTransfer* internalDataTransfer() const { return 0; }

    bool isBeingDispatched() const { return eventPhase(); }

    virtual Ref<Event> cloneFor(HTMLIFrameElement*) const;

    virtual EventTarget* relatedTarget() const { return nullptr; }

protected:
    Event();
    WEBCORE_EXPORT Event(const AtomicString& type, bool canBubble, bool cancelable);
    Event(const AtomicString& type, bool canBubble, bool cancelable, double timestamp);
    Event(const AtomicString& type, const EventInit&);

    virtual void receivedTarget();
    bool dispatched() const { return m_target; }

private:
    AtomicString m_type;

    bool m_isInitialized { false };
    bool m_canBubble { false };
    bool m_cancelable { false };
    bool m_composed { false };

    bool m_propagationStopped { false };
    bool m_immediatePropagationStopped { false };
    bool m_defaultPrevented { false };
    bool m_defaultHandled { false };
    bool m_cancelBubble { false };
    bool m_isTrusted { false };
    bool m_isExecutingPassiveEventListener { false };

    unsigned short m_eventPhase { 0 };
    RefPtr<EventTarget> m_currentTarget;
    const EventPath* m_eventPath { nullptr };
    RefPtr<EventTarget> m_target;
    DOMTimeStamp m_createTime;

    RefPtr<Event> m_underlyingEvent;
};

inline void Event::resetPropagationFlags()
{
    m_propagationStopped = false;
    m_immediatePropagationStopped = false;
}

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_EVENT(ToValueTypeName) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::Event& event) { return event.is##ToValueTypeName(); } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // Event_h
