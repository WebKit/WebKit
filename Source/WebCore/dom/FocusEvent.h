/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FocusEvent_h
#define FocusEvent_h

#include "EventDispatchMediator.h"
#include "UIEvent.h"


namespace WebCore {
    
class FocusEvent : public UIEvent {
public:
    static PassRefPtr<FocusEvent> create()
    {
        return adoptRef(new FocusEvent);
    }
       
    static PassRefPtr<FocusEvent> create(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<AbstractView> view, int detail, PassRefPtr<EventTarget> relatedTarget)
    {
        return adoptRef(new FocusEvent(type, canBubble, cancelable, view, detail, relatedTarget));
    }
       
    virtual ~FocusEvent();
    void initFocusEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<AbstractView>, int detail, PassRefPtr<EventTarget> relatedTarget);
    EventTarget* relatedTarget() const { return m_relatedTarget.get(); }
    void setRelatedTarget(PassRefPtr<EventTarget> relatedTarget) { m_relatedTarget = relatedTarget; }
    virtual const AtomicString& interfaceName() const;
    
private:
    FocusEvent();
    FocusEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<AbstractView>, int detail, PassRefPtr<EventTarget> relatedTarget);        
    RefPtr<EventTarget> m_relatedTarget;
};
    
    
class FocusInEventDispatchMediator : public EventDispatchMediator {
public:
    static PassRefPtr<FocusInEventDispatchMediator> create(PassRefPtr<FocusEvent>, PassRefPtr<Node> oldFocusedNode);
private:
    explicit FocusInEventDispatchMediator(PassRefPtr<FocusEvent>, PassRefPtr<Node> oldFocusedNode);
    virtual bool dispatchEvent(EventDispatcher*) const;
    FocusEvent* event() const;
    RefPtr<Node> m_oldFocusedNode;
};
    
 
class FocusOutEventDispatchMediator : public EventDispatchMediator {
public:
    static PassRefPtr<FocusOutEventDispatchMediator> create(PassRefPtr<FocusEvent>, PassRefPtr<Node> newFocusedNode);
private:
    explicit FocusOutEventDispatchMediator(PassRefPtr<FocusEvent>, PassRefPtr<Node> newFocusedNode);
    virtual bool dispatchEvent(EventDispatcher*) const;
    FocusEvent* event() const;
    RefPtr<Node> m_newFocusedNode;
};
    
    
class FocusEventDispatchMediator : public EventDispatchMediator {    
public:
    static PassRefPtr<FocusEventDispatchMediator> create(PassRefPtr<Node> oldFocusedNode);
private:
    explicit FocusEventDispatchMediator(PassRefPtr<Node> oldFocusedNode);
    virtual bool dispatchEvent(EventDispatcher*) const;
    RefPtr<Node> m_oldFocusedNode;
}; 
    

class BlurEventDispatchMediator : public EventDispatchMediator {       
public:
    static PassRefPtr<BlurEventDispatchMediator> create(PassRefPtr<Node> newFocusedNode);        
private:
    explicit BlurEventDispatchMediator(PassRefPtr<Node> newFocusedNode);
    virtual bool dispatchEvent(EventDispatcher*) const;
    RefPtr<Node> m_newFocusedNode;
};     

} // namespace WebCore

#endif // FocusEvent_h
