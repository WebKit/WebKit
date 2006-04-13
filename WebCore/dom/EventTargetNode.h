/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef DOM_EventTargetNodeImpl_h
#define DOM_EventTargetNodeImpl_h

#include "Node.h"

template <typename T> class DeprecatedPtrList;

namespace WebCore {

class EventTargetNode : public Node {
public:
    EventTargetNode(Document*);
    virtual ~EventTargetNode();

    virtual bool isEventTargetNode() const { return true; }

    void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    void removeAllEventListeners();

    void setHTMLEventListener(const AtomicString& eventType, PassRefPtr<EventListener>);
    void removeHTMLEventListener(const AtomicString& eventType);
    bool dispatchHTMLEvent(const AtomicString& eventType, bool canBubble, bool cancelable);
    EventListener *getHTMLEventListener(const AtomicString& eventType);

    bool dispatchGenericEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false);
    bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false);
    bool dispatchSubtreeModifiedEvent(bool childrenChanged = true);
    void dispatchWindowEvent(const AtomicString& eventType, bool canBubble, bool cancelable);
    bool dispatchUIEvent(const AtomicString& eventType, int detail = 0);
    bool dispatchKeyEvent(const PlatformKeyboardEvent&);
    void dispatchWheelEvent(PlatformWheelEvent&);
    bool dispatchMouseEvent(const PlatformMouseEvent&, const AtomicString& eventType,
        int clickCount = 0, Node* relatedTarget = 0);
    bool dispatchMouseEvent(const AtomicString& eventType, int button, int clickCount,
        int clientX, int clientY, int screenX, int screenY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        bool isSimulated = false, Node* relatedTarget = 0);
    bool dispatchSimulatedMouseEvent(const AtomicString& eventType);

    void handleLocalEvents(Event*, bool useCapture);

    virtual void dispatchFocusEvent();
    virtual void dispatchBlurEvent();

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    
    // Handlers to do/undo actions on the target node before an event is dispatched to it and after the event
    // has been dispatched.  The data pointer is handed back by the preDispatch and passed to postDispatch.
    virtual void* preDispatchEventHandler(Event*) { return 0; }
    virtual void postDispatchEventHandler(Event*, void* dataFromPreDispatch) { }

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
    virtual void dump(QTextStream*, DeprecatedString indent = "") const;
#endif

protected:
    DeprecatedPtrList<RegisteredEventListener>* m_regdListeners;
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

#ifndef NDEBUG

void forbidEventDispatch();
void allowEventDispatch();
bool eventDispatchForbidden();

#else

inline void forbidEventDispatch() { }
inline void allowEventDispatch() { }

#endif NDEBUG

} //namespace WebCore

#endif
