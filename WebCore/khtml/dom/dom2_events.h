/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef _DOM_Events_h_
#define _DOM_Events_h_

#include <dom/dom_misc.h>
#include <dom/dom_string.h>

#if !KHTML_NO_CPLUSPLUS_DOM

#include <dom/dom_node.h>

#endif

namespace DOM {

#if !KHTML_NO_CPLUSPLUS_DOM

class Event;
class EventException;
class UIEvent;
class MouseEvent;
class MutationEvent;
class KeyboardEvent;
class AbstractView;

class UIEventImpl;
class MouseEventImpl;
class MutationEventImpl;
class KeyboardEventImpl;

#endif

class EventListenerImpl;
class EventImpl;

#if KHTML_NO_CPLUSPLUS_DOM
typedef EventImpl *EventListenerEvent;
#else
typedef Event &EventListenerEvent;
#endif

/**
 * Introduced in DOM Level 2
 *
 * The EventListener interface is the primary method for handling events.
 * Users implement the EventListener interface and register their listener on
 * an EventTarget using the AddEventListener method. The users should also
 * remove their EventListener from its EventTarget after they have completed
 * using the listener.
 *
 * When a Node is copied using the cloneNode method the EventListeners attached
 * to the source Node are not attached to the copied Node. If the user wishes
 * the same EventListeners to be added to the newly created copy the user must
 * add them manually.
 *
 */
class EventListener : public DomShared {
public:
    EventListener();
    virtual ~EventListener();

    /**
     * This method is called whenever an event occurs of the type for which the
     * EventListener interface was registered. Parameters
     *
     * @param evt The Event contains contextual information about the event. It
     * also contains the stopPropagation and preventDefault methods which are
     * used in determining the event's flow and default action.
     *
     * @param isWindowEvent If true, the "this" should be the window, not the current
     * target (there is no DOM node for the window, so it can't be the target).
     *
     */
    virtual void handleEvent(EventListenerEvent evt, bool isWindowEvent);
    void handleEventImpl(EventImpl *evt, bool isWindowEvent);

    /**
     * @internal
     * not part of the DOM
     *
     * Returns a name specifying the type of listener. Useful for checking
     * if an event is of a particular sublass.
     *
     */
    virtual DOMString eventListenerType();

protected:
    /**
     * @internal
     * Reserved. Do not use in your subclasses.
     */
    EventListenerImpl *impl;
};


/**
 * Introduced in DOM Level 2
 *
 * The Event interface is used to provide contextual information about an event
 * to the handler processing the event. An object which implements the Event
 * interface is generally passed as the first parameter to an event handler.
 * More specific context information is passed to event handlers by deriving
 * additional interfaces from Event which contain information directly relating
 * to the type of event they accompany. These derived interfaces are also
 * implemented by the object passed to the event listener.
 *
 */
class Event {

#if !KHTML_NO_CPLUSPLUS_DOM

    friend class Document;
    friend class NodeImpl;
    friend class DocumentImpl;

#endif

public:

#if !KHTML_NO_CPLUSPLUS_DOM

    Event();
    Event(const Event &other);
    virtual ~Event();

    Event & operator = (const Event &other);

#endif

    /**
     * An integer indicating which phase of event flow is being processed.
     *
     * AT_TARGET: The event is currently being evaluated at the target
     * EventTarget.
     *
     * BUBBLING_PHASE: The current event phase is the bubbling phase.
     *
     * CAPTURING_PHASE: The current event phase is the capturing phase.
     *
     */
    enum PhaseType {
	CAPTURING_PHASE = 1,
	AT_TARGET = 2,
	BUBBLING_PHASE = 3
    };

#if !KHTML_NO_CPLUSPLUS_DOM

    /**
     * The name of the event (case-insensitive). The name must be an XML name.
     *
     */
    DOMString type() const;

    /**
     * Used to indicate the EventTarget to which the event was originally
     * dispatched.
     *
     */
    Node target() const;

    /**
     * Used to indicate the EventTarget whose EventListeners are currently
     * being processed. This is particularly useful during capturing and
     * bubbling.
     *
     */
    Node currentTarget() const;

    /**
     * Used to indicate which phase of event flow is currently being evaluated.
     *
     */
    unsigned short eventPhase() const;

    /**
     * Used to indicate whether or not an event is a bubbling event. If the
     * event can bubble the value is true, else the value is false.
     *
     */
    bool bubbles() const;

    /**
     * Used to indicate whether or not an event can have its default action
     * prevented. If the default action can be prevented the value is true,
     * else the value is false.
     *
     */
    bool cancelable() const;

    /**
     * Used to specify the time (in milliseconds relative to the epoch) at
     * which the event was created. Due to the fact that some systems may not
     * provide this information the value of timeStamp may be not available for
     * all events. When not available, a value of 0 will be returned. Examples
     * of epoch time are the time of the system start or 0:0:0 UTC 1st January 1970.
     *
     */
    DOMTimeStamp timeStamp() const;

    /**
     * The stopPropagation method is used prevent further propagation of an
     * event during event flow. If this method is called by any EventListener
     * the event will cease propagating through the tree. The event will
     * complete dispatch to all listeners on the current EventTarget before
     * event flow stops. This method may be used during any stage of event flow.
     *
     */
    void stopPropagation();

    /**
     * If an event is cancelable, the preventDefault method is used to signify
     * that the event is to be canceled, meaning any default action normally
     * taken by the implementation as a result of the event will not occur. If,
     * during any stage of event flow, the preventDefault method is called the
     * event is canceled. Any default action associated with the event will not
     * occur. Calling this method for a non-cancelable event has no effect.
     * Once preventDefault has been called it will remain in effect throughout
     * the remainder of the event's propagation. This method may be used during
     * any stage of event flow.
     *
     */
    void preventDefault();

    /**
     * The initEvent method is used to initialize the value of an Event created
     * through the DocumentEvent interface. This method may only be called
     * before the Event has been dispatched via the dispatchEvent method,
     * though it may be called multiple times during that phase if necessary.
     * If called multiple times the final invocation takes precedence. If
     * called from a subclass of Event interface only the values specified in
     * the initEvent method are modified, all other attributes are left
     * unchanged.
     *
     * @param eventTypeArg Specifies the event type. This type may be any event
     * type currently defined in this specification or a new event type.. The
     * string must be an XML name.
     *
     * Any new event type must not begin with any upper, lower, or mixed case
     * version of the string "DOM". This prefix is reserved for future DOM
     * event sets. It is also strongly recommended that third parties adding
     * their own events use their own prefix to avoid confusion and lessen the
     * probability of conflicts with other new events.
     *
     * @param canBubbleArg Specifies whether or not the event can bubble.
     *
     * @param cancelableArg Specifies whether or not the event's default action can be prevented.
     *
     */
    void initEvent(const DOMString &eventTypeArg, bool canBubbleArg, bool cancelableArg);

    /**
     * @internal
     * not part of the DOM
     */
    EventImpl *handle() const;
    bool isNull() const;

    /* Nonstandard extensions needed to support widely used JS event properties */
    void setCancelBubble(bool cancel);
    void setDefaultPrevented(bool returnValue);
    bool getCancelBubble() const;
    bool defaultPrevented() const;

    Event(EventImpl *i);

protected:
    EventImpl *impl;

#endif

};


/**
 * Introduced in DOM Level 2:
 *
 * Event operations may throw an EventException as specified in their method
 * descriptions.
 *
 */
class EventException
{
public:

#if !KHTML_NO_CPLUSPLUS_DOM

    EventException(unsigned short _code);
    EventException(const EventException &other);
    EventException & operator = (const EventException &other);
    virtual ~EventException() {}

#endif

    /**
     * An integer indicating the type of error generated.
     *
     * UNSPECIFIED_EVENT_TYPE_ERR: If the Event's type was not specified by
     * initializing the event before the method was called. Specification of
     * the Event's type as null or an empty string will also trigger this
     * exception.
     *
     */
    enum EventExceptionCode {
        UNSPECIFIED_EVENT_TYPE_ERR     = 0
    };

#if !KHTML_NO_CPLUSPLUS_DOM

    unsigned short code;

#endif

};


#if !KHTML_NO_CPLUSPLUS_DOM


/**
 * Introduced in DOM Level 2
 *
 * The UIEvent interface provides specific contextual information associated
 * with User Interface events.
 *
 */
class UIEvent : public Event {
public:
    UIEvent();
    UIEvent(const UIEvent &other);
    UIEvent(const Event &other);
    UIEvent & operator = (const UIEvent &other);
    UIEvent & operator = (const Event &other);
    virtual ~UIEvent();

    /**
     * The view attribute identifies the AbstractView from which the event was
     * generated.
     *
     */
    AbstractView view() const;

    /**
     * Specifies some detail information about the Event, depending on the type
     * of event.
     *
     */
    long detail() const;

    /**
     * Non-standard extension to support IE-style keyCode event property.
     *
     */
    int keyCode() const;

    /**
     * Non-standard extension to support IE-style charCode event property.
     *
     */
    int charCode() const;

    /**
     * Non-standard extensions to support Netscape-style pageX and pageY event properties.
     *
     */
    int pageX() const;
    int pageY() const;

    /**
     * Non-standard extensions to support Netscape-style layerX and layerY event properties.
     *
     */
    int layerX() const;
    int layerY() const;

    /**
     * Non-standard extension to support Netscape-style "which" event property.
     *
     */
    int which() const;

    /**
     * The initUIEvent method is used to initialize the value of a UIEvent
     * created through the DocumentEvent interface. This method may only be
     * called before the UIEvent has been dispatched via the dispatchEvent
     * method, though it may be called multiple times during that phase if
     * necessary. If called multiple times, the final invocation takes
     * precedence.
     *
     * @param typeArg Specifies the event type.
     *
     * @param canBubbleArg Specifies whether or not the event can bubble.
     *
     * @param cancelableArg Specifies whether or not the event's default action
     * can be prevented.
     *
     * @param viewArg Specifies the Event's AbstractView.
     *
     * @param detailArg Specifies the Event's detail.
     *
     */
    void initUIEvent(const DOMString &typeArg,
                                 bool canBubbleArg,
                                 bool cancelableArg,
                                 const AbstractView &viewArg,
                                 long detailArg);

    UIEvent(UIEventImpl *impl);
};




/**
 * Introduced in DOM Level 2
 *
 * The MouseEvent interface provides specific contextual information associated
 * with Mouse events.
 *
 * The detail attribute inherited from UIEvent indicates the number of times a
 * mouse button has been pressed and released over the same screen location
 * during a user action. The attribute value is 1 when the user begins this
 * action and increments by 1 for each full sequence of pressing and releasing.
 * If the user moves the mouse between the mousedown and mouseup the value will
 * be set to 0, indicating that no click is occurring.
 *
 * In the case of nested elements mouse events are always targeted at the most
 * deeply nested element. Ancestors of the targeted element may use bubbling to
 * obtain notification of mouse events which occur within its descendent elements.
 *
 */
class MouseEvent : public UIEvent {
public:
    MouseEvent();
    MouseEvent(const MouseEvent &other);
    MouseEvent(const Event &other);
    MouseEvent & operator = (const MouseEvent &other);
    MouseEvent & operator = (const Event &other);
    virtual ~MouseEvent();

    /**
     * The horizontal coordinate at which the event occurred relative to the
     * origin of the screen coordinate system.
     *
     */
    long screenX() const;

    /**
     * The vertical coordinate at which the event occurred relative to the
     * origin of the screen coordinate system.
     *
     */
    long screenY() const;

    /**
     * The horizontal coordinate at which the event occurred relative to the
     * DOM implementation's client area.
     *
     */
    long clientX() const;

    /**
     * The vertical coordinate at which the event occurred relative to the DOM
     * implementation's client area.
     *
     */
    long clientY() const;

    /**
     * Used to indicate whether the 'ctrl' key was depressed during the firing
     * of the event.
     */
    bool ctrlKey() const;

    /**
     * Used to indicate whether the 'shift' key was depressed during the firing
     * of the event.
     *
     */
    bool shiftKey() const;

    /**
     * Used to indicate whether the 'alt' key was depressed during the firing
     * of the event. On some platforms this key may map to an alternative key
     * name.
     *
     */
    bool altKey() const;

    /**
     * Used to indicate whether the 'meta' key was depressed during the firing
     * of the event. On some platforms this key may map to an alternative key
     * name.
     *
     */
    bool metaKey() const;

    /**
     * During mouse events caused by the depression or release of a mouse
     * button, button is used to indicate which mouse button changed state. The
     * values for button range from zero to indicate the left button of the
     * mouse, one to indicate the middle button if present, and two to indicate
     * the right button. For mice configured for left handed use in which the
     * button actions are reversed the values are instead read from right to
     * left.
     *
     */
    unsigned short button() const;

    /**
     * Used to identify a secondary EventTarget related to a UI event.
     * Currently this attribute is used with the mouseover event to indicate
     * the EventTarget which the pointing device exited and with the mouseout
     * event to indicate the EventTarget which the pointing device entered.
     *
     */
    Node relatedTarget() const;

    /**
     * The initMouseEvent method is used to initialize the value of a
     * MouseEvent created through the DocumentEvent interface. This method may
     * only be called before the MouseEvent has been dispatched via the
     * dispatchEvent method, though it may be called multiple times during that
     * phase if necessary. If called multiple times, the final invocation takes
     * precedence. Parameters
     *
     * @param typeArg Specifies the event type.
     *
     * @param canBubbleArg Specifies whether or not the event can bubble.
     *
     * @param cancelableArg Specifies whether or not the event's default action can be prevented.
     *
     * @param viewArg Specifies the Event's AbstractView.
     *
     * @param detailArg Specifies the Event's mouse click count.
     *
     * @param screenXArg Specifies the Event's screen x coordinate
     *
     * @param screenYArg Specifies the Event's screen y coordinate
     *
     * @param clientXArg Specifies the Event's client x coordinate
     *
     * @param clientYArg Specifies the Event's client y coordinate
     *
     * @param ctrlKeyArg Specifies whether or not control key was depressed during the Event.
     *
     * @param altKeyArg Specifies whether or not alt key was depressed during the Event.
     *
     * @param shiftKeyArg Specifies whether or not shift key was depressed during the Event.
     *
     * @param metaKeyArg Specifies whether or not meta key was depressed during the Event.
     *
     * @param buttonArg Specifies the Event's mouse button.
     *
     * @param relatedTargetArg Specifies the Event's related EventTarget.
     *
     */
    void initMouseEvent(const DOMString &typeArg,
                                    bool canBubbleArg,
                                    bool cancelableArg,
                                    const AbstractView &viewArg,
                                    long detailArg,
                                    long screenXArg,
                                    long screenYArg,
                                    long clientXArg,
                                    long clientYArg,
                                    bool ctrlKeyArg,
                                    bool altKeyArg,
                                    bool shiftKeyArg,
                                    bool metaKeyArg,
                                    unsigned short buttonArg,
                                    const Node &relatedTargetArg);
protected:
    MouseEvent(MouseEventImpl *impl);
};


#endif


/**
 * Introduced in DOM Level 2
 *
 * The MutationEvent interface provides specific contextual information
 * associated with Mutation events.
 *
 */
class MutationEvent : public Event {
public:

#if !KHTML_NO_CPLUSPLUS_DOM

    MutationEvent();
    MutationEvent(const MutationEvent &other);
    MutationEvent(const Event &other);
    MutationEvent & operator = (const MutationEvent &other);
    MutationEvent & operator = (const Event &other);
    virtual ~MutationEvent();

#endif

    /**
     * An integer indicating in which way the Attr was changed.
     *
     * ADDITION: The Attr was just added.
     *
     * MODIFICATION: The Attr was modified in place.
     *
     * REMOVAL: The Attr was just removed.
     *
     */
    enum attrChangeType {
	MODIFICATION = 1,
	ADDITION = 2,
	REMOVAL = 3
    };

#if !KHTML_NO_CPLUSPLUS_DOM

    /**
     * relatedNode is used to identify a secondary node related to a mutation
     * event. For example, if a mutation event is dispatched to a node
     * indicating that its parent has changed, the relatedNode is the changed
     * parent. If an event is instead dispatched to a subtree indicating a node
     * was changed within it, the relatedNode is the changed node. In the case
     * of the DOMAttrModified event it indicates the Attr node which was
     * modified, added, or removed.
     *
     */
    Node relatedNode() const;

    /**
     * prevValue indicates the previous value of the Attr node in
     * DOMAttrModified events, and of the CharacterData node in
     * DOMCharDataModified events.
     *
     */
    DOMString prevValue() const;

    /**
     * newValue indicates the new value of the Attr node in DOMAttrModified
     * events, and of the CharacterData node in DOMCharDataModified events.
     *
     */
    DOMString newValue() const;

    /**
     * attrName indicates the name of the changed Attr node in a
     * DOMAttrModified event.
     *
     */
    DOMString attrName() const;

    /**
     * attrChange indicates the type of change which triggered the
     * DOMAttrModified event. The values can be MODIFICATION, ADDITION, or
     * REMOVAL.
     *
     */
    unsigned short attrChange() const;

    /**
     * The initMutationEvent method is used to initialize the value of a
     * MutationEvent created through the DocumentEvent interface. This method
     * may only be called before the MutationEvent has been dispatched via the
     * dispatchEvent method, though it may be called multiple times during that
     * phase if necessary. If called multiple times, the final invocation takes
     * precedence.
     *
     * @param typeArg Specifies the event type.
     *
     * @param canBubbleArg Specifies whether or not the event can bubble.
     *
     * @param cancelableArg Specifies whether or not the event's default action can be prevented.
     *
     * @param relatedNodeArg Specifies the Event's related Node.
     *
     * @param prevValueArg Specifies the Event's prevValue attribute. This value may be null.
     *
     * @param newValueArg Specifies the Event's newValue attribute. This value may be null.
     *
     * @param attrNameArg Specifies the Event's attrName attribute. This value may be null.
     *
     * @param attrChangeArg Specifies the Event's attrChange attribute
     *
     */
    void initMutationEvent(const DOMString &typeArg,
                                       bool canBubbleArg,
                                       bool cancelableArg,
                                       const Node &relatedNodeArg,
                                       const DOMString &prevValueArg,
                                       const DOMString &newValueArg,
                                       const DOMString &attrNameArg,
                                       unsigned short attrChangeArg);
protected:
    MutationEvent(MutationEventImpl *impl);

#endif

};


/**
 * Introduced in DOM Level 3
 *
 * The KeyboardEvent interface provides specific contextual information
 * associated with Keyboard events.
 *
 */
class KeyboardEvent

#if !KHTML_NO_CPLUSPLUS_DOM

  : public UIEvent

#endif

{

public:

#if !KHTML_NO_CPLUSPLUS_DOM

    KeyboardEvent();
    KeyboardEvent(const KeyboardEvent &other);
    KeyboardEvent(const Event &other);
    KeyboardEvent & operator = (const KeyboardEvent &other);
    KeyboardEvent & operator = (const Event &other);
    virtual ~KeyboardEvent();

#endif

    // KeyLocationCode
    static const unsigned long DOM_KEY_LOCATION_STANDARD      = 0x00;
    static const unsigned long DOM_KEY_LOCATION_LEFT          = 0x01;
    static const unsigned long DOM_KEY_LOCATION_RIGHT         = 0x02;
    static const unsigned long DOM_KEY_LOCATION_NUMPAD        = 0x03;
    static const unsigned long DOM_KEY_LOCATION_UNKNOWN       = 0x04;
    
#if !KHTML_NO_CPLUSPLUS_DOM

    /**
     * Holds the identifier of the key.
     *
     */
    DOMString keyIdentifier() const;

    /**
     * Contains an indication of the location of they key on the device.
     *
     */
    unsigned long keyLocation() const;

    /**
     * Used to indicate whether the 'ctrl' key was depressed during the firing
     * of the event.
     */
    bool ctrlKey() const;

    /**
     * Used to indicate whether the 'shift' key was depressed during the firing
     * of the event.
     *
     */
    bool shiftKey() const;

    /**
     * Used to indicate whether the 'alt' key was depressed during the firing
     * of the event. On some platforms this key may map to an alternative key
     * name.
     *
     */
    bool altKey() const;

    /**
     * Used to indicate whether the 'meta' key was depressed during the firing
     * of the event. On some platforms this key may map to an alternative key
     * name.
     *
     */
    bool metaKey() const;

    /**
     * Used to indicate whether the 'alt graph' (?) key was depressed during the firing
     * of the event. On some platforms this key may map to an alternative key
     * name.
     *
     */
    bool altGraphKey() const;

    void initKeyboardEvent(const DOMString &typeArg, 
                                bool canBubbleArg,
                                bool cancelableArg,
                                const AbstractView &viewArg, 
                                const DOMString &keyIdentifierArg, 
                                unsigned long keyLocationArg, 
                                bool ctrlKeyArg, 
                                bool shiftKeyArg, 
                                bool altKeyArg, 
                                bool metaKeyArg, 
                                bool altGraphKeyArg);
                                       
protected:
    KeyboardEvent(KeyboardEventImpl *impl);

#endif

};

}; //namespace
#endif
