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

#include "DocPtr.h"
#include "NodeImpl.h"
#include "Shared.h"
#include "PlatformString.h"
#include <kxmlcore/Assertions.h>
#include <kxmlcore/HashSet.h>
#include <kxmlcore/PassRefPtr.h>

class QStringList;
class QTextStream;
class RenderArena;

template <typename T> class QPtrList;

namespace WebCore {

class AtomicString;
class ContainerNodeImpl;
class DocumentImpl;
class ElementImpl;
class EventImpl;
class EventListener;
class IntRect;
class KeyEvent;
class MouseEvent;
class NamedAttrMapImpl;
class NodeListImpl;
class QualifiedName;
class RegisteredEventListener;
class RenderObject;
class RenderStyle;
class WheelEvent;

typedef int ExceptionCode;

class EventTargetNodeImpl : public NodeImpl
{
    friend class DocumentImpl;
public:
    EventTargetNodeImpl(DocumentImpl*);
    virtual ~EventTargetNodeImpl();

    virtual bool isEventTargetNode() const { return true; }

    void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    void removeAllEventListeners();

    void setHTMLEventListener(const AtomicString& eventType, PassRefPtr<EventListener>);
    void removeHTMLEventListener(const AtomicString& eventType);
    bool dispatchHTMLEvent(const AtomicString& eventType, bool canBubble, bool cancelable);
    EventListener *getHTMLEventListener(const AtomicString& eventType);

    bool dispatchGenericEvent(PassRefPtr<EventImpl>, ExceptionCode&, bool tempEvent = false);
    bool dispatchEvent(PassRefPtr<EventImpl>, ExceptionCode&, bool tempEvent = false);
    bool dispatchSubtreeModifiedEvent(bool childrenChanged = true);
    void dispatchWindowEvent(const AtomicString& eventType, bool canBubble, bool cancelable);
    bool dispatchUIEvent(const AtomicString& eventType, int detail = 0);
    bool dispatchKeyEvent(const KeyEvent&);
    void dispatchWheelEvent(WheelEvent&);
    bool dispatchMouseEvent(const MouseEvent&, const AtomicString& eventType,
        int clickCount = 0, NodeImpl* relatedTarget = 0);
    bool dispatchMouseEvent(const AtomicString& eventType, int button, int clickCount,
        int clientX, int clientY, int screenX, int screenY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        bool isSimulated = false, NodeImpl* relatedTarget = 0);
    bool dispatchSimulatedMouseEvent(const AtomicString& eventType);

    void handleLocalEvents(EventImpl*, bool useCapture);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    
    // Handlers to do/undo actions on the target node before an event is dispatched to it and after the event
    // has been dispatched.  The data pointer is handed back by the preDispatch and passed to postDispatch.
    virtual void* preDispatchEventHandler(EventImpl*) { return 0; }
    virtual void postDispatchEventHandler(EventImpl*, void* data) { }

    /**
     * Perform the default action for an event e.g. submitting a form
     */
    virtual void defaultEventHandler(EventImpl*);

    /**
     * Used for disabled form elements; if true, prevents mouse events from being dispatched
     * to event listeners, and prevents DOMActivate events from being sent at all.
     */
    virtual bool disabled() const;
    
#ifndef NDEBUG
    virtual void dump(QTextStream*, QString indent = "") const;
#endif

protected:
    QPtrList<RegisteredEventListener>* m_regdListeners;
};

inline EventTargetNodeImpl* EventTargetNodeCast(NodeImpl* n) 
{ 
    ASSERT(n->isEventTargetNode());
    return static_cast<EventTargetNodeImpl*>(n); 
}

inline const EventTargetNodeImpl* EventTargetNodeCast(const NodeImpl* n) 
{ 
    ASSERT(n->isEventTargetNode());
    return static_cast<const EventTargetNodeImpl*>(n); 
}

#ifndef NDEBUG

extern int gEventDispatchForbidden;
inline void forbidEventDispatch() { ++gEventDispatchForbidden; }
inline void allowEventDispatch() { if (gEventDispatchForbidden > 0) --gEventDispatchForbidden; }
inline bool eventDispatchForbidden() { return gEventDispatchForbidden > 0; }

#else

inline void forbidEventDispatch() { }
inline void allowEventDispatch() { }

#endif NDEBUG

} //namespace WebCore

#endif
