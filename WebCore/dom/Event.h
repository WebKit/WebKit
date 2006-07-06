/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef Event_h
#define Event_h

#include "AtomicString.h"
#include "Node.h"
#include "Shared.h"

namespace WebCore {

    // FIXME: this should probably defined elsewhere.
    typedef unsigned long long DOMTimeStamp;

    // FIXME: these too should probably defined elsewhere.
    const int EventExceptionOffset = 100;
    const int EventExceptionMax = 199;
    enum EventExceptionCode { UNSPECIFIED_EVENT_TYPE_ERR = EventExceptionOffset };

    class Event : public Shared<Event> {
    public:
        enum PhaseType { 
            CAPTURING_PHASE     = 1, 
            AT_TARGET           = 2,
            BUBBLING_PHASE      = 3 
        };

        // Reverse-engineered from Netscape
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
    
        Event();
        Event(const AtomicString& typeArg, bool canBubbleArg, bool cancelableArg);
        virtual ~Event();

        void initEvent(const AtomicString &eventTypeArg, bool canBubbleArg, bool cancelableArg);

        const AtomicString& type() const { return m_type; }
        
        Node* target() const { return m_target.get(); }
        void setTarget(Node*);
        
        Node* currentTarget() const { return m_currentTarget; }
        void setCurrentTarget(Node* currentTarget) { m_currentTarget = currentTarget; }
        
        unsigned short eventPhase() const { return m_eventPhase; }
        void setEventPhase(unsigned short eventPhase) { m_eventPhase = eventPhase; }
        
        bool bubbles() const { return m_canBubble; }
        bool cancelable() const { return m_cancelable; }
        DOMTimeStamp timeStamp() { return m_createTime; }
        void stopPropagation() { m_propagationStopped = true; }

        virtual bool isUIEvent() const;
        virtual bool isMouseEvent() const;
        virtual bool isMutationEvent() const;
        virtual bool isKeyboardEvent() const;
        virtual bool isDragEvent() const; // a subset of mouse events
        virtual bool isClipboardEvent() const;
        virtual bool isWheelEvent() const;
        virtual bool isBeforeTextInsertedEvent() const;
        virtual bool isOverflowEvent() const;
        
        bool propagationStopped() const { return m_propagationStopped; }
        bool defaultPrevented() const { return m_defaultPrevented; }

        void setDefaultHandled() { m_defaultHandled = true; }
        bool defaultHandled() const { return m_defaultHandled; }

        void preventDefault() { if (m_cancelable) m_defaultPrevented = true; }
        void setDefaultPrevented(bool defaultPrevented) { m_defaultPrevented = defaultPrevented; }

        void setCancelBubble(bool cancel) { m_cancelBubble = cancel; }
        bool getCancelBubble() const { return m_cancelBubble; }

        virtual bool storesResultAsString() const;
        virtual void storeResult(const String&);

    protected:
        virtual void receivedTarget();
        bool dispatched() const { return m_target; }

    private:
        AtomicString m_type;
        bool m_canBubble;
        bool m_cancelable;

        bool m_propagationStopped;
        bool m_defaultPrevented;
        bool m_defaultHandled;
        bool m_cancelBubble;

        Node* m_currentTarget; // ref > 0 maintained externally
        unsigned short m_eventPhase;
        RefPtr<Node> m_target;
        DOMTimeStamp m_createTime;
    };

} // namespace WebCore

#endif // Event_h
