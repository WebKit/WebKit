/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef EventTargetNode_h
#define EventTargetNode_h

#include "EventTarget.h"
#include "Node.h"

namespace WebCore {

class EventTargetNode : public Node,
                        public EventTarget {
public:
    EventTargetNode(Document*);
    virtual ~EventTargetNode();

    virtual bool isEventTargetNode() const { return true; }
    virtual EventTargetNode* toNode() { return this; }

    virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false);
    void removeAllEventListeners();

    void setHTMLEventListener(const AtomicString& eventType, PassRefPtr<EventListener>);
    void removeHTMLEventListener(const AtomicString& eventType);
    bool dispatchHTMLEvent(const AtomicString& eventType, bool canBubble, bool cancelable);
    EventListener* getHTMLEventListener(const AtomicString& eventType);

    bool dispatchSubtreeModifiedEvent(bool childrenChanged = true);
    void dispatchWindowEvent(const AtomicString& eventType, bool canBubble, bool cancelable);
    bool dispatchUIEvent(const AtomicString& eventType, int detail = 0, PassRefPtr<Event> underlyingEvent = 0);
    bool dispatchKeyEvent(const PlatformKeyboardEvent&);
    void dispatchWheelEvent(PlatformWheelEvent&);
    bool dispatchMouseEvent(const PlatformMouseEvent&, const AtomicString& eventType,
        int clickCount = 0, Node* relatedTarget = 0);
    bool dispatchMouseEvent(const AtomicString& eventType, int button, int clickCount,
        int pageX, int pageY, int screenX, int screenY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        bool isSimulated = false, Node* relatedTarget = 0, PassRefPtr<Event> underlyingEvent = 0);
    void dispatchSimulatedMouseEvent(const AtomicString& eventType, PassRefPtr<Event> underlyingEvent = 0);
    void dispatchSimulatedClick(PassRefPtr<Event> underlyingEvent, bool sendMouseEvents = false, bool showPressedLook = true);
    bool dispatchProgressEvent(const AtomicString &eventType, bool lengthComputableArg, unsigned loadedArg, unsigned totalArg);

    virtual void handleLocalEvents(Event*, bool useCapture);

    virtual void dispatchFocusEvent();
    virtual void dispatchBlurEvent();

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    /**
     * Perform the default action for an event e.g. submitting a form
     */
    virtual void defaultEventHandler(Event*);

    /**
     * Used for disabled form elements; if true, prevents mouse events from being dispatched
     * to event listeners, and prevents DOMActivate events from being sent at all.
     */
    virtual bool disabled() const;
    
#ifndef NDEBUG
    virtual void dump(TextStream*, DeprecatedString indent = "") const;
#endif

    RegisteredEventListenerList* localEventListeners() const { return m_regdListeners; }

    using Node::ref;
    using Node::deref;
 
protected:
    friend class EventTarget;
    RegisteredEventListenerList* m_regdListeners;

private:
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
};

inline EventTargetNode* EventTargetNodeCast(Node* n) 
{ 
    ASSERT(n->isEventTargetNode());
    return static_cast<EventTargetNode*>(n); 
}

inline const EventTargetNode* EventTargetNodeCast(const Node* n) 
{ 
    ASSERT(n->isEventTargetNode());
    return static_cast<const EventTargetNode*>(n); 
}

} // namespace WebCore

#endif // EventTargetNode_h
