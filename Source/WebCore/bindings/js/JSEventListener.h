/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "DOMWrapperWorld.h"
#include "EventListener.h"
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/Weak.h>
#include <JavaScriptCore/WeakInlines.h>
#include <wtf/Ref.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DOMWindow;
class Document;
class EventTarget;
class HTMLElement;

class JSEventListener : public EventListener {
public:
    WEBCORE_EXPORT static Ref<JSEventListener> create(JSC::JSObject* listener, JSC::JSObject* wrapper, bool isAttribute, DOMWrapperWorld&);
    WEBCORE_EXPORT static RefPtr<JSEventListener> create(JSC::JSValue listener, JSC::JSObject& wrapper, bool isAttribute, DOMWrapperWorld&);

    virtual ~JSEventListener();

    bool operator==(const EventListener&) const final;

    // Returns true if this event listener was created for an event handler attribute, like "onload" or "onclick".
    bool isAttribute() const final { return m_isAttribute; }

    JSC::JSObject* ensureJSFunction(ScriptExecutionContext&) const;
    DOMWrapperWorld& isolatedWorld() const { return m_isolatedWorld; }


    JSC::JSObject* jsFunction() const final { return m_jsFunction.get(); }
    JSC::JSObject* wrapper() const final { return m_wrapper.get(); }

    virtual URL sourceURL() const { return { }; }
    virtual TextPosition sourcePosition() const { return TextPosition(); }

    String functionName() const;

private:
    virtual JSC::JSObject* initializeJSFunction(ScriptExecutionContext&) const;
    void visitJSFunction(JSC::SlotVisitor&) final;

protected:
    JSEventListener(JSC::JSObject* function, JSC::JSObject* wrapper, bool isAttribute, DOMWrapperWorld&);
    void handleEvent(ScriptExecutionContext&, Event&) override;
    void setWrapperWhenInitializingJSFunction(JSC::VM&, JSC::JSObject* wrapper) const { m_wrapper = JSC::Weak<JSC::JSObject>(wrapper); }

private:
    mutable JSC::Weak<JSC::JSObject> m_jsFunction;
    mutable JSC::Weak<JSC::JSObject> m_wrapper;
    mutable bool m_isInitialized { false };

    bool m_isAttribute;
    Ref<DOMWrapperWorld> m_isolatedWorld;
};

// For "onxxx" attributes that automatically set up JavaScript event listeners.
JSC::JSValue eventHandlerAttribute(EventTarget&, const AtomString& eventType, DOMWrapperWorld&);
void setEventHandlerAttribute(JSC::JSGlobalObject&, JSC::JSObject&, EventTarget&, const AtomString& eventType, JSC::JSValue);

// Like the functions above, but for attributes that forward event handlers to the window object rather than setting them on the target.
JSC::JSValue windowEventHandlerAttribute(HTMLElement&, const AtomString& eventType, DOMWrapperWorld&);
void setWindowEventHandlerAttribute(JSC::JSGlobalObject&, JSC::JSObject&, HTMLElement&, const AtomString& eventType, JSC::JSValue);
JSC::JSValue windowEventHandlerAttribute(DOMWindow&, const AtomString& eventType, DOMWrapperWorld&);
void setWindowEventHandlerAttribute(JSC::JSGlobalObject&, JSC::JSObject&, DOMWindow&, const AtomString& eventType, JSC::JSValue);

// Like the functions above, but for attributes that forward event handlers to the document rather than setting them on the target.
JSC::JSValue documentEventHandlerAttribute(HTMLElement&, const AtomString& eventType, DOMWrapperWorld&);
void setDocumentEventHandlerAttribute(JSC::JSGlobalObject&, JSC::JSObject&, HTMLElement&, const AtomString& eventType, JSC::JSValue);
JSC::JSValue documentEventHandlerAttribute(Document&, const AtomString& eventType, DOMWrapperWorld&);
void setDocumentEventHandlerAttribute(JSC::JSGlobalObject&, JSC::JSObject&, Document&, const AtomString& eventType, JSC::JSValue);

inline JSC::JSObject* JSEventListener::ensureJSFunction(ScriptExecutionContext& scriptExecutionContext) const
{
    // initializeJSFunction can trigger code that deletes this event listener
    // before we're done. It should always return null in this case.
    JSC::VM& vm = m_isolatedWorld->vm();
    auto protect = makeRef(const_cast<JSEventListener&>(*this));
    JSC::EnsureStillAliveScope protectedWrapper(m_wrapper.get());

    if (!m_isInitialized) {
        ASSERT(!m_jsFunction);
        auto* function = initializeJSFunction(scriptExecutionContext);
        if (function) {
            m_jsFunction = JSC::Weak<JSC::JSObject>(function);
            // When JSFunction is initialized, initializeJSFunction must ensure that m_wrapper should be initialized too.
            ASSERT(m_wrapper);
            vm.heap.writeBarrier(m_wrapper.get(), function);
            m_isInitialized = true;
        }
    }

    // m_wrapper and m_jsFunction are Weak<>. nullptr of these fields do not mean that this event-listener is not initialized yet.
    // If this is initialized once, m_isInitialized should be true, and then m_wrapper and m_jsFunction must be alive. m_wrapper's
    // liveness should be kept correctly by using ActiveDOMObject, output-constraints, etc. And m_jsFunction must be alive if m_wrapper
    // is alive since JSEventListener marks m_jsFunction in JSEventListener::visitJSFunction if m_wrapper is alive.
    // If the event-listener is not initialized yet, we should skip invoking this event-listener.
    if (!m_isInitialized)
        return nullptr;

    ASSERT(m_wrapper);
    ASSERT(m_jsFunction);
    // Ensure m_jsFunction is live JSObject as a quick sanity check (while it is already ensured by Weak<>). If this fails, this is possibly JSC GC side's bug.
    ASSERT(static_cast<JSC::JSCell*>(m_jsFunction.get())->isObject());

    return m_jsFunction.get();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::JSEventListener)
static bool isType(const WebCore::EventListener& input) { return input.type() == WebCore::JSEventListener::JSEventListenerType; }
SPECIALIZE_TYPE_TRAITS_END()
