/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
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

#ifndef DOM_EVENTSIMPL_H
#define DOM_EVENTSIMPL_H

#include "AtomicString.h"
#include "DOMWindow.h"
#include "Node.h"

class DeprecatedStringList;

namespace WebCore {

class CachedImage;
class Clipboard;
class EventListener;
class EventTargetNode;
class Image;
class IntPoint;
class PlatformKeyboardEvent;

typedef unsigned long long DOMTimeStamp;
typedef DOMWindow AbstractView;

const int EventExceptionOffset = 100;
const int EventExceptionMax = 199;
enum EventExceptionCode { UNSPECIFIED_EVENT_TYPE_ERR = EventExceptionOffset };

class Event : public Shared<Event>
{
public:
    enum PhaseType { CAPTURING_PHASE = 1, AT_TARGET = 2, BUBBLING_PHASE = 3 };
    Event();
    Event(const AtomicString& type, bool canBubbleArg, bool cancelableArg);
    virtual ~Event();

    const AtomicString &type() const { return m_type; }
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
    void preventDefault() { if (m_cancelable) m_defaultPrevented = true; }
    void initEvent(const AtomicString &eventTypeArg, bool canBubbleArg, bool cancelableArg);

    virtual bool isUIEvent() const;
    virtual bool isMouseEvent() const;
    virtual bool isMutationEvent() const;
    virtual bool isKeyboardEvent() const;
    virtual bool isDragEvent() const; // a subset of mouse events
    virtual bool isClipboardEvent() const;
    virtual bool isWheelEvent() const;
    virtual bool isBeforeTextInsertedEvent() const;

    bool propagationStopped() const { return m_propagationStopped; }
    bool defaultPrevented() const { return m_defaultPrevented; }

    void setDefaultHandled() { m_defaultHandled = true; }
    bool defaultHandled() const { return m_defaultHandled; }

    void setCancelBubble(bool cancel) { m_cancelBubble = cancel; }
    void setDefaultPrevented(bool defaultPrevented) { m_defaultPrevented = defaultPrevented; }
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

class UIEvent : public Event
{
public:
    UIEvent();
    UIEvent(const AtomicString &type,
                bool canBubbleArg,
                bool cancelableArg,
                AbstractView *viewArg,
                int detailArg);
    AbstractView *view() const { return m_view.get(); }
    int detail() const { return m_detail; }
    void initUIEvent(const AtomicString &typeArg,
                     bool canBubbleArg,
                     bool cancelableArg,
                     AbstractView *viewArg,
                     int detailArg);
    virtual bool isUIEvent() const;

    virtual int keyCode() const;
    virtual int charCode() const;

    virtual int layerX() const;
    virtual int layerY() const;

    virtual int pageX() const;
    virtual int pageY() const;

    virtual int which() const;

private:
    RefPtr<AbstractView> m_view;
    int m_detail;
};

class UIEventWithKeyState : public UIEvent {
public:
    UIEventWithKeyState() : m_ctrlKey(false), m_altKey(false), m_shiftKey(false), m_metaKey(false) { }
    UIEventWithKeyState(const AtomicString &type, bool canBubbleArg, bool cancelableArg, AbstractView *viewArg,
        int detailArg, bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, bool metaKeyArg)
        : UIEvent(type, canBubbleArg, cancelableArg, viewArg, detailArg)
        , m_ctrlKey(ctrlKeyArg), m_altKey(altKeyArg), m_shiftKey(shiftKeyArg), m_metaKey(metaKeyArg) { }

    bool ctrlKey() const { return m_ctrlKey; }
    bool shiftKey() const { return m_shiftKey; }
    bool altKey() const { return m_altKey; }
    bool metaKey() const { return m_metaKey; }

protected: // expose these so init functions can set them
    bool m_ctrlKey : 1;
    bool m_altKey : 1;
    bool m_shiftKey : 1;
    bool m_metaKey : 1;
};

// Internal only: Helper class for what's common between mouse and wheel events.
class MouseRelatedEvent : public UIEventWithKeyState {
public:
    MouseRelatedEvent();
    MouseRelatedEvent(const AtomicString &type,
                          bool canBubbleArg,
                          bool cancelableArg,
                          AbstractView *viewArg,
                          int detailArg,
                          int screenXArg,
                          int screenYArg,
                          int clientXArg,
                          int clientYArg,
                          bool ctrlKeyArg,
                          bool altKeyArg,
                          bool shiftKeyArg,
                          bool metaKeyArg,
                          bool isSimulated = false);
    int screenX() const { return m_screenX; }
    int screenY() const { return m_screenY; }
    int clientX() const { return m_clientX; }
    int clientY() const { return m_clientY; }
    int layerX() const { return m_layerX; }
    int layerY() const { return m_layerY; }
    int offsetX() const { return m_offsetX; }
    int offsetY() const { return m_offsetY; }
    bool isSimulated() const { return m_isSimulated; }
    virtual int pageX() const;
    virtual int pageY() const;
    int x() const;
    int y() const;
protected: // expose these so MouseEvent::initMouseEvent can set them
    int m_screenX;
    int m_screenY;
    int m_clientX;
    int m_clientY;
    void initCoordinates();
    virtual void receivedTarget();
private:
    int m_pageX;
    int m_pageY;
    int m_layerX;
    int m_layerY;
    int m_offsetX;
    int m_offsetY;
    bool m_isSimulated;
};

// Introduced in DOM Level 2
class MouseEvent : public MouseRelatedEvent {
public:
    MouseEvent();
    MouseEvent(const AtomicString& type,
                   bool canBubbleArg,
                   bool cancelableArg,
                   AbstractView* viewArg,
                   int detailArg,
                   int screenXArg,
                   int screenYArg,
                   int clientXArg,
                   int clientYArg,
                   bool ctrlKeyArg,
                   bool altKeyArg,
                   bool shiftKeyArg,
                   bool metaKeyArg,
                   unsigned short buttonArg,
                   EventTargetNode* relatedTargetArg,
                   Clipboard* clipboardArg = 0,
                   bool isSimulated = false);
    virtual ~MouseEvent();
    // WinIE uses 1,4,2 for left/middle/right but not for click (just for mousedown/up, maybe others), but we will match the standard DOM.
    unsigned short button() const { return m_button; }
    EventTargetNode* relatedTarget() const { return m_relatedTarget.get(); }
    Clipboard* clipboard() const { return m_clipboard.get(); }
    Node* toElement() const;
    Node* fromElement() const;
    void initMouseEvent(const AtomicString& typeArg,
                        bool canBubbleArg,
                        bool cancelableArg,
                        AbstractView* viewArg,
                        int detailArg,
                        int screenXArg,
                        int screenYArg,
                        int clientXArg,
                        int clientYArg,
                        bool ctrlKeyArg,
                        bool altKeyArg,
                        bool shiftKeyArg,
                        bool metaKeyArg,
                        unsigned short buttonArg,
                        EventTargetNode* relatedTargetArg);
    virtual bool isMouseEvent() const;
    virtual bool isDragEvent() const;
    virtual int which() const;
private:
    unsigned short m_button;
    RefPtr<EventTargetNode> m_relatedTarget;
    RefPtr<Clipboard> m_clipboard;
};


// Introduced in DOM Level 3
class KeyboardEvent : public UIEventWithKeyState {
public:
    enum KeyLocationCode {
        DOM_KEY_LOCATION_STANDARD      = 0x00,
        DOM_KEY_LOCATION_LEFT          = 0x01,
        DOM_KEY_LOCATION_RIGHT         = 0x02,
        DOM_KEY_LOCATION_NUMPAD        = 0x03,
    };
    KeyboardEvent();
    KeyboardEvent(const PlatformKeyboardEvent&, AbstractView*);
    KeyboardEvent(const AtomicString &type,
                bool canBubbleArg,
                bool cancelableArg,
                AbstractView *viewArg,
                const String &keyIdentifierArg,
                unsigned keyLocationArg,
                bool ctrlKeyArg,
                bool altKeyArg,
                bool shiftKeyArg,
                bool metaKeyArg,
                bool altGraphKeyArg);
    virtual ~KeyboardEvent();
    
    void initKeyboardEvent(const AtomicString &typeArg,
                bool canBubbleArg,
                bool cancelableArg,
                AbstractView *viewArg,
                const String &keyIdentifierArg,
                unsigned keyLocationArg,
                bool ctrlKeyArg,
                bool altKeyArg,
                bool shiftKeyArg,
                bool metaKeyArg,
                bool altGraphKeyArg);
    
    String keyIdentifier() const { return m_keyIdentifier.get(); }
    unsigned keyLocation() const { return m_keyLocation; }
    
    bool altGraphKey() const { return m_altGraphKey; }
    
    const PlatformKeyboardEvent* keyEvent() const { return m_keyEvent; }

    int keyCode() const; // key code for keydown and keyup, character for other events
    int charCode() const;
    
    virtual bool isKeyboardEvent() const;
    virtual int which() const;

private:
    PlatformKeyboardEvent* m_keyEvent;
    RefPtr<StringImpl> m_keyIdentifier;
    unsigned m_keyLocation;
    bool m_altGraphKey : 1;
};

class MutationEvent : public Event {
public:
    enum attrChangeType { MODIFICATION = 1, ADDITION = 2, REMOVAL = 3 };
    MutationEvent();
    MutationEvent(const AtomicString &type,
                      bool canBubbleArg,
                      bool cancelableArg,
                      Node *relatedNodeArg,
                      const String &prevValueArg,
                      const String &newValueArg,
                      const String &attrNameArg,
                      unsigned short attrChangeArg);
    Node *relatedNode() const { return m_relatedNode.get(); }
    String prevValue() const { return m_prevValue.get(); }
    String newValue() const { return m_newValue.get(); }
    String attrName() const { return m_attrName.get(); }
    unsigned short attrChange() const { return m_attrChange; }
    void initMutationEvent(const AtomicString &typeArg,
                           bool canBubbleArg,
                           bool cancelableArg,
                           Node *relatedNodeArg,
                           const String &prevValueArg,
                           const String &newValueArg,
                           const String &attrNameArg,
                           unsigned short attrChangeArg);
    virtual bool isMutationEvent() const;
private:
    RefPtr<Node> m_relatedNode;
    RefPtr<StringImpl> m_prevValue;
    RefPtr<StringImpl> m_newValue;
    RefPtr<StringImpl> m_attrName;
    unsigned short m_attrChange;
};

class ClipboardEvent : public Event {
public:
    ClipboardEvent();
    ClipboardEvent(const AtomicString &type, bool canBubbleArg, bool cancelableArg, Clipboard *clipboardArg);

    Clipboard *clipboard() const { return m_clipboard.get(); }
    virtual bool isClipboardEvent() const;
private:
    RefPtr<Clipboard> m_clipboard;
};

// extension: mouse wheel event
class WheelEvent : public MouseRelatedEvent
{
public:
    WheelEvent();
    WheelEvent(bool horizontal, int wheelDelta, AbstractView *,
                   int screenXArg, int screenYArg,
                   int clientXArg, int clientYArg,
                   bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, bool metaKeyArg);
    bool isHorizontal() const { return m_horizontal; }
    int wheelDelta() const { return m_wheelDelta; }

private:
    virtual bool isWheelEvent() const;

    bool m_horizontal;
    int m_wheelDelta;
};

class RegisteredEventListener {
public:
    RegisteredEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);

    const AtomicString &eventType() const { return m_eventType; }
    EventListener *listener() const { return m_listener.get(); }
    bool useCapture() const { return m_useCapture; }

private:
    AtomicString m_eventType;
    RefPtr<EventListener> m_listener;
    bool m_useCapture;
};

bool operator==(const RegisteredEventListener&, const RegisteredEventListener&);
inline bool operator!=(const RegisteredEventListener& a, const RegisteredEventListener& b) { return !(a == b); }

// State available during IE's events for drag and drop and copy/paste
class Clipboard : public Shared<Clipboard> {
public:
    virtual ~Clipboard();

    // Is this operation a drag-drop or a copy-paste?
    virtual bool isForDragging() const = 0;

    virtual String dropEffect() const = 0;
    virtual void setDropEffect(const String &s) = 0;
    virtual String effectAllowed() const = 0;
    virtual void setEffectAllowed(const String &s) = 0;
    
    virtual void clearData(const String &type) = 0;
    virtual void clearAllData() = 0;
    virtual String getData(const String &type, bool &success) const = 0;
    virtual bool setData(const String &type, const String &data) = 0;
    
    // extensions beyond IE's API
    virtual DeprecatedStringList types() const = 0;
    
    virtual WebCore::IntPoint dragLocation() const = 0;
    virtual CachedImage* dragImage() const = 0;
    virtual void setDragImage(CachedImage*, const WebCore::IntPoint &) = 0;
    virtual Node *dragImageElement() = 0;
    virtual void setDragImageElement(Node *, const WebCore::IntPoint &) = 0;
};

class BeforeUnloadEvent : public Event
{
public:
    BeforeUnloadEvent();

    virtual bool storesResultAsString() const;
    virtual void storeResult(const String&);

    String result() const { return m_result.get(); }

private:
    RefPtr<StringImpl> m_result;
};

} // namespace

#endif
