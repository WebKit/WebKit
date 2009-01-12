/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

class Attribute;
class Frame;
class RegisteredEventListener;

typedef Vector<RefPtr<RegisteredEventListener> > RegisteredEventListenerVector;

class EventTargetNode : public Node, public EventTarget {
public:
    EventTargetNode(Document*, bool isElement = false, bool isContainer = false);
    virtual ~EventTargetNode();

    virtual bool isEventTargetNode() const { return true; }
    virtual EventTargetNode* toNode() { return this; }

    virtual ScriptExecutionContext* scriptExecutionContext() const;

    virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);
    void removeAllEventListeners();

    void setInlineEventListenerForType(const AtomicString& eventType, PassRefPtr<EventListener>);
    void setInlineEventListenerForTypeAndAttribute(const AtomicString& eventType, Attribute*);
    void removeInlineEventListenerForType(const AtomicString& eventType);
    bool dispatchEventForType(const AtomicString& eventType, bool canBubble, bool cancelable);
    EventListener* inlineEventListenerForType(const AtomicString& eventType) const;

    bool dispatchSubtreeModifiedEvent();
    void dispatchWindowEvent(PassRefPtr<Event>);
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
    void dispatchStorageEvent(const AtomicString &eventType, const String& key, const String& oldValue, const String& newValue, Frame* source);
    bool dispatchWebKitAnimationEvent(const AtomicString& eventType, const String& animationName, double elapsedTime);
    bool dispatchWebKitTransitionEvent(const AtomicString& eventType, const String& propertyName, double elapsedTime);
    bool dispatchGenericEvent(PassRefPtr<Event>);

    virtual void handleLocalEvents(Event*, bool useCapture);

    virtual void dispatchFocusEvent();
    virtual void dispatchBlurEvent();

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void willMoveToNewOwnerDocument();
    virtual void didMoveToNewOwnerDocument();

    /**
     * Perform the default action for an event e.g. submitting a form
     */
    virtual void defaultEventHandler(Event*);

    /**
     * Used for disabled form elements; if true, prevents mouse events from being dispatched
     * to event listeners, and prevents DOMActivate events from being sent at all.
     */
    virtual bool disabled() const;

    const RegisteredEventListenerVector& eventListeners() const;

    EventListener* onabort() const;
    void setOnabort(PassRefPtr<EventListener>);
    EventListener* onblur() const;
    void setOnblur(PassRefPtr<EventListener>);
    EventListener* onchange() const;
    void setOnchange(PassRefPtr<EventListener>);
    EventListener* onclick() const;
    void setOnclick(PassRefPtr<EventListener>);
    EventListener* oncontextmenu() const;
    void setOncontextmenu(PassRefPtr<EventListener>);
    EventListener* ondblclick() const;
    void setOndblclick(PassRefPtr<EventListener>);
    EventListener* onerror() const;
    void setOnerror(PassRefPtr<EventListener>);
    EventListener* onfocus() const;
    void setOnfocus(PassRefPtr<EventListener>);
    EventListener* oninput() const;
    void setOninput(PassRefPtr<EventListener>);
    EventListener* onkeydown() const;
    void setOnkeydown(PassRefPtr<EventListener>);
    EventListener* onkeypress() const;
    void setOnkeypress(PassRefPtr<EventListener>);
    EventListener* onkeyup() const;
    void setOnkeyup(PassRefPtr<EventListener>);
    EventListener* onload() const;
    void setOnload(PassRefPtr<EventListener>);
    EventListener* onmousedown() const;
    void setOnmousedown(PassRefPtr<EventListener>);
    EventListener* onmousemove() const;
    void setOnmousemove(PassRefPtr<EventListener>);
    EventListener* onmouseout() const;
    void setOnmouseout(PassRefPtr<EventListener>);
    EventListener* onmouseover() const;
    void setOnmouseover(PassRefPtr<EventListener>);
    EventListener* onmouseup() const;
    void setOnmouseup(PassRefPtr<EventListener>);
    EventListener* onmousewheel() const;
    void setOnmousewheel(PassRefPtr<EventListener>);
    EventListener* onbeforecut() const;
    void setOnbeforecut(PassRefPtr<EventListener>);
    EventListener* oncut() const;
    void setOncut(PassRefPtr<EventListener>);
    EventListener* onbeforecopy() const;
    void setOnbeforecopy(PassRefPtr<EventListener>);
    EventListener* oncopy() const;
    void setOncopy(PassRefPtr<EventListener>);
    EventListener* onbeforepaste() const;
    void setOnbeforepaste(PassRefPtr<EventListener>);
    EventListener* onpaste() const;
    void setOnpaste(PassRefPtr<EventListener>);
    EventListener* ondragenter() const;
    void setOndragenter(PassRefPtr<EventListener>);
    EventListener* ondragover() const;
    void setOndragover(PassRefPtr<EventListener>);
    EventListener* ondragleave() const;
    void setOndragleave(PassRefPtr<EventListener>);
    EventListener* ondrop() const;
    void setOndrop(PassRefPtr<EventListener>);
    EventListener* ondragstart() const;
    void setOndragstart(PassRefPtr<EventListener>);
    EventListener* ondrag() const;
    void setOndrag(PassRefPtr<EventListener>);
    EventListener* ondragend() const;
    void setOndragend(PassRefPtr<EventListener>);
    EventListener* onreset() const;
    void setOnreset(PassRefPtr<EventListener>);
    EventListener* onresize() const;
    void setOnresize(PassRefPtr<EventListener>);
    EventListener* onscroll() const;
    void setOnscroll(PassRefPtr<EventListener>);
    EventListener* onsearch() const;
    void setOnsearch(PassRefPtr<EventListener>);
    EventListener* onselect() const;
    void setOnselect(PassRefPtr<EventListener>);
    EventListener* onselectstart() const;
    void setOnselectstart(PassRefPtr<EventListener>);
    EventListener* onsubmit() const;
    void setOnsubmit(PassRefPtr<EventListener>);
    EventListener* onunload() const;
    void setOnunload(PassRefPtr<EventListener>);

    using Node::ref;
    using Node::deref;
 
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
