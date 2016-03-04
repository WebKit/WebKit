/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2008, 2009 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef JSEventListener_h
#define JSEventListener_h

#include "DOMWrapperWorld.h"
#include "EventListener.h"
#include <heap/StrongInlines.h>
#include <heap/Weak.h>
#include <heap/WeakInlines.h>
#include <wtf/Ref.h>

namespace WebCore {

class DOMWindow;
class EventTarget;
class HTMLElement;
class JSDOMGlobalObject;

class JSEventListener : public EventListener {
public:
    static Ref<JSEventListener> create(JSC::JSObject* listener, JSC::JSObject* wrapper, bool isAttribute, DOMWrapperWorld& world)
    {
        return adoptRef(*new JSEventListener(listener, wrapper, isAttribute, world));
    }

    static const JSEventListener* cast(const EventListener* listener)
    {
        return listener->type() == JSEventListenerType
            ? static_cast<const JSEventListener*>(listener)
            : 0;
    }

    virtual ~JSEventListener();

    bool operator==(const EventListener& other) override;

    // Returns true if this event listener was created for an event handler attribute, like "onload" or "onclick".
    bool isAttribute() const { return m_isAttribute; }

    JSC::JSObject* jsFunction(ScriptExecutionContext*) const;
    DOMWrapperWorld& isolatedWorld() const { return *m_isolatedWorld; }

    JSC::JSObject* wrapper() const { return m_wrapper.get(); }
    void setWrapper(JSC::VM&, JSC::JSObject* wrapper) const { m_wrapper = JSC::Weak<JSC::JSObject>(wrapper); }

private:
    virtual JSC::JSObject* initializeJSFunction(ScriptExecutionContext*) const;
    void visitJSFunction(JSC::SlotVisitor&) override;
    bool virtualisAttribute() const override;

protected:
    JSEventListener(JSC::JSObject* function, JSC::JSObject* wrapper, bool isAttribute, DOMWrapperWorld&);
    void handleEvent(ScriptExecutionContext*, Event*) override;

private:
    mutable JSC::Weak<JSC::JSObject> m_jsFunction;
    mutable JSC::Weak<JSC::JSObject> m_wrapper;

    bool m_isAttribute;
    RefPtr<DOMWrapperWorld> m_isolatedWorld;
};

// For "onxxx" attributes that automatically set up JavaScript event listeners.
JSC::JSValue eventHandlerAttribute(EventTarget&, const AtomicString& eventType);
void setEventHandlerAttribute(JSC::ExecState&, JSC::JSObject&, EventTarget&, const AtomicString& eventType, JSC::JSValue);

// Like the functions above, but for attributes that forward event handlers to the window object rather than setting them on the target.
JSC::JSValue windowEventHandlerAttribute(HTMLElement&, const AtomicString& eventType);
void setWindowEventHandlerAttribute(JSC::ExecState&, JSC::JSObject&, HTMLElement&, const AtomicString& eventType, JSC::JSValue);
JSC::JSValue windowEventHandlerAttribute(DOMWindow&, const AtomicString& eventType);
void setWindowEventHandlerAttribute(JSC::ExecState&, JSC::JSObject&, DOMWindow&, const AtomicString& eventType, JSC::JSValue);

// Like the functions above, but for attributes that forward event handlers to the document rather than setting them on the target.
JSC::JSValue documentEventHandlerAttribute(HTMLElement&, const AtomicString& eventType);
void setDocumentEventHandlerAttribute(JSC::ExecState&, JSC::JSObject&, HTMLElement&, const AtomicString& eventType, JSC::JSValue);
JSC::JSValue documentEventHandlerAttribute(Document&, const AtomicString& eventType);
void setDocumentEventHandlerAttribute(JSC::ExecState&, JSC::JSObject&, Document&, const AtomicString& eventType, JSC::JSValue);

Ref<JSEventListener> createJSEventListenerForAdd(JSC::ExecState&, JSC::JSObject& listener, JSC::JSObject& wrapper);
Ref<JSEventListener> createJSEventListenerForRemove(JSC::ExecState&, JSC::JSObject& listener, JSC::JSObject& wrapper);

inline JSC::JSObject* JSEventListener::jsFunction(ScriptExecutionContext* scriptExecutionContext) const
{
    // initializeJSFunction can trigger code that deletes this event listener
    // before we're done. It should always return 0 in this case.
    Ref<JSEventListener> protect(const_cast<JSEventListener&>(*this));
    JSC::Strong<JSC::JSObject> wrapper(m_isolatedWorld->vm(), m_wrapper.get());

    if (!m_jsFunction) {
        JSC::JSObject* function = initializeJSFunction(scriptExecutionContext);
        JSC::JSObject* wrapper = m_wrapper.get();
        if (wrapper)
            JSC::Heap::heap(wrapper)->writeBarrier(wrapper, function);
        m_jsFunction = JSC::Weak<JSC::JSObject>(function);
    }

    // Verify that we have a valid wrapper protecting our function from
    // garbage collection. That is except for when we're not in the normal
    // world and can have zombie m_jsFunctions.
    ASSERT(!m_isolatedWorld->isNormal() || m_wrapper || !m_jsFunction);

    // If m_wrapper is 0, then m_jsFunction is zombied, and should never be accessed.
    if (!m_wrapper)
        return 0;

    // Try to verify that m_jsFunction wasn't recycled. (Not exact, since an
    // event listener can be almost anything, but this makes test-writing easier).
    ASSERT(!m_jsFunction || static_cast<JSC::JSCell*>(m_jsFunction.get())->isObject());

    return m_jsFunction.get();
}

inline Ref<JSEventListener> createJSEventListenerForRemove(JSC::ExecState& state, JSC::JSObject& listener, JSC::JSObject& wrapper)
{
    return createJSEventListenerForAdd(state, listener, wrapper);
}

} // namespace WebCore

#endif // JSEventListener_h
