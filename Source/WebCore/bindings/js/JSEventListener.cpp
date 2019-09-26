/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "JSEventListener.h"

#include "BeforeUnloadEvent.h"
#include "ContentSecurityPolicy.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMGlobalObject.h"
#include "JSDocument.h"
#include "JSEvent.h"
#include "JSEventTarget.h"
#include "JSExecState.h"
#include "JSExecStateInstrumentation.h"
#include "JSWorkerGlobalScope.h"
#include "ScriptController.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/VMEntryScope.h>
#include <JavaScriptCore/Watchdog.h>
#include <wtf/Ref.h>

namespace WebCore {
using namespace JSC;

JSEventListener::JSEventListener(JSObject* function, JSObject* wrapper, bool isAttribute, DOMWrapperWorld& isolatedWorld)
    : EventListener(JSEventListenerType)
    , m_wrapper(wrapper)
    , m_isAttribute(isAttribute)
    , m_isolatedWorld(isolatedWorld)
{
    if (wrapper) {
        JSC::Heap::heap(wrapper)->writeBarrier(wrapper, function);
        m_jsFunction = JSC::Weak<JSC::JSObject>(function);
    } else
        ASSERT(!function);
}

JSEventListener::~JSEventListener() = default;

Ref<JSEventListener> JSEventListener::create(JSC::JSObject* listener, JSC::JSObject* wrapper, bool isAttribute, DOMWrapperWorld& world)
{
    return adoptRef(*new JSEventListener(listener, wrapper, isAttribute, world));
}

RefPtr<JSEventListener> JSEventListener::create(JSC::JSValue listener, JSC::JSObject& wrapper, bool isAttribute, DOMWrapperWorld& world)
{
    if (UNLIKELY(!listener.isObject()))
        return nullptr;

    return create(JSC::asObject(listener), &wrapper, isAttribute, world);
}

JSObject* JSEventListener::initializeJSFunction(ScriptExecutionContext&) const
{
    return nullptr;
}

void JSEventListener::visitJSFunction(SlotVisitor& visitor)
{
    // If m_wrapper is null, then m_jsFunction is zombied, and should never be accessed.
    if (!m_wrapper)
        return;

    visitor.append(m_jsFunction);
}

static void handleBeforeUnloadEventReturnValue(BeforeUnloadEvent& event, const String& returnValue)
{
    if (returnValue.isNull())
        return;

    event.preventDefault();
    if (event.returnValue().isEmpty())
        event.setReturnValue(returnValue);
}

void JSEventListener::handleEvent(ScriptExecutionContext& scriptExecutionContext, Event& event)
{
    if (scriptExecutionContext.isJSExecutionForbidden())
        return;

    VM& vm = scriptExecutionContext.vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    // See https://dom.spec.whatwg.org/#dispatching-events spec on calling handleEvent.
    // "If this throws an exception, report the exception." It should not propagate the
    // exception.

    JSObject* jsFunction = this->jsFunction(scriptExecutionContext);
    if (!jsFunction)
        return;

    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(scriptExecutionContext, m_isolatedWorld);
    if (!globalObject)
        return;

    if (scriptExecutionContext.isDocument()) {
        JSDOMWindow* window = jsCast<JSDOMWindow*>(globalObject);
        if (!window->wrapped().isCurrentlyDisplayedInFrame())
            return;
        if (wasCreatedFromMarkup() && !scriptExecutionContext.contentSecurityPolicy()->allowInlineEventHandlers(sourceURL(), sourcePosition().m_line))
            return;
        // FIXME: Is this check needed for other contexts?
        ScriptController& script = window->wrapped().frame()->script();
        if (!script.canExecuteScripts(AboutToExecuteScript) || script.isPaused())
            return;
    }

    ExecState* exec = globalObject->globalExec();

    JSValue handleEventFunction = jsFunction;

    CallData callData;
    CallType callType = getCallData(vm, handleEventFunction, callData);

    // If jsFunction is not actually a function, see if it implements the EventListener interface and use that
    if (callType == CallType::None) {
        handleEventFunction = jsFunction->get(exec, Identifier::fromString(vm, "handleEvent"));
        if (UNLIKELY(scope.exception())) {
            auto* exception = scope.exception();
            scope.clearException();
            event.target()->uncaughtExceptionInEventHandler();
            reportException(exec, exception);
            return;
        }
        callType = getCallData(vm, handleEventFunction, callData);
    }

    if (callType == CallType::None)
        return;

    Ref<JSEventListener> protectedThis(*this);

    MarkedArgumentBuffer args;
    args.append(toJS(exec, globalObject, &event));
    ASSERT(!args.hasOverflowed());

    Event* savedEvent = globalObject->currentEvent();

    // window.event should not be set when the target is inside a shadow tree, as per the DOM specification.
    bool isTargetInsideShadowTree = is<Node>(event.currentTarget()) && downcast<Node>(*event.currentTarget()).isInShadowTree();
    if (!isTargetInsideShadowTree)
        globalObject->setCurrentEvent(&event);

    VMEntryScope entryScope(vm, vm.entryScope ? vm.entryScope->globalObject() : globalObject);

    InspectorInstrumentationCookie cookie = JSExecState::instrumentFunctionCall(&scriptExecutionContext, callType, callData);

    JSValue thisValue = handleEventFunction == jsFunction ? toJS(exec, globalObject, event.currentTarget()) : jsFunction;
    NakedPtr<JSC::Exception> exception;
    JSValue retval = JSExecState::profiledCall(exec, JSC::ProfilingReason::Other, handleEventFunction, callType, callData, thisValue, args, exception);

    InspectorInstrumentation::didCallFunction(cookie, &scriptExecutionContext);

    globalObject->setCurrentEvent(savedEvent);

    if (is<WorkerGlobalScope>(scriptExecutionContext)) {
        auto& scriptController = *downcast<WorkerGlobalScope>(scriptExecutionContext).script();
        bool terminatorCausedException = (scope.exception() && isTerminatedExecutionException(vm, scope.exception()));
        if (terminatorCausedException || scriptController.isTerminatingExecution())
            scriptController.forbidExecution();
    }

    if (exception) {
        event.target()->uncaughtExceptionInEventHandler();
        reportException(exec, exception);
        return;
    }

    if (!m_isAttribute) {
        // This is an EventListener and there is therefore no need for any return value handling.
        return;
    }

    // Do return value handling for event handlers (https://html.spec.whatwg.org/#the-event-handler-processing-algorithm).

    if (event.type() == eventNames().beforeunloadEvent) {
        // This is a OnBeforeUnloadEventHandler, and therefore the return value must be coerced into a String.
        if (is<BeforeUnloadEvent>(event))
            handleBeforeUnloadEventReturnValue(downcast<BeforeUnloadEvent>(event), convert<IDLNullable<IDLDOMString>>(*exec, retval));
        return;
    }

    if (retval.isFalse())
        event.preventDefault();
}

bool JSEventListener::operator==(const EventListener& listener) const
{
    if (!is<JSEventListener>(listener))
        return false;
    auto& other = downcast<JSEventListener>(listener);
    return m_jsFunction == other.m_jsFunction && m_isAttribute == other.m_isAttribute;
}

static inline JSC::JSValue eventHandlerAttribute(EventListener* abstractListener, ScriptExecutionContext& context)
{
    if (!is<JSEventListener>(abstractListener))
        return jsNull();

    auto* function = downcast<JSEventListener>(*abstractListener).jsFunction(context);
    if (!function)
        return jsNull();

    return function;
}

static inline RefPtr<JSEventListener> createEventListenerForEventHandlerAttribute(JSC::ExecState& state, JSC::JSValue listener, JSC::JSObject& wrapper)
{
    if (!listener.isObject())
        return nullptr;
    return JSEventListener::create(asObject(listener), &wrapper, true, currentWorld(state));
}

JSC::JSValue eventHandlerAttribute(EventTarget& target, const AtomString& eventType, DOMWrapperWorld& isolatedWorld)
{
    return eventHandlerAttribute(target.attributeEventListener(eventType, isolatedWorld), *target.scriptExecutionContext());
}

void setEventHandlerAttribute(JSC::ExecState& state, JSC::JSObject& wrapper, EventTarget& target, const AtomString& eventType, JSC::JSValue value)
{
    target.setAttributeEventListener(eventType, createEventListenerForEventHandlerAttribute(state, value, wrapper), currentWorld(state));
}

JSC::JSValue windowEventHandlerAttribute(HTMLElement& element, const AtomString& eventType, DOMWrapperWorld& isolatedWorld)
{
    auto& document = element.document();
    return eventHandlerAttribute(document.getWindowAttributeEventListener(eventType, isolatedWorld), document);
}

void setWindowEventHandlerAttribute(JSC::ExecState& state, JSC::JSObject& wrapper, HTMLElement& element, const AtomString& eventType, JSC::JSValue value)
{
    ASSERT(wrapper.globalObject());
    element.document().setWindowAttributeEventListener(eventType, createEventListenerForEventHandlerAttribute(state, value, *wrapper.globalObject()), currentWorld(state));
}

JSC::JSValue windowEventHandlerAttribute(DOMWindow& window, const AtomString& eventType, DOMWrapperWorld& isolatedWorld)
{
    return eventHandlerAttribute(window, eventType, isolatedWorld);
}

void setWindowEventHandlerAttribute(JSC::ExecState& state, JSC::JSObject& wrapper, DOMWindow& window, const AtomString& eventType, JSC::JSValue value)
{
    setEventHandlerAttribute(state, wrapper, window, eventType, value);
}

JSC::JSValue documentEventHandlerAttribute(HTMLElement& element, const AtomString& eventType, DOMWrapperWorld& isolatedWorld)
{
    auto& document = element.document();
    return eventHandlerAttribute(document.attributeEventListener(eventType, isolatedWorld), document);
}

void setDocumentEventHandlerAttribute(JSC::ExecState& state, JSC::JSObject& wrapper, HTMLElement& element, const AtomString& eventType, JSC::JSValue value)
{
    ASSERT(wrapper.globalObject());
    auto& document = element.document();
    auto* documentWrapper = JSC::jsCast<JSDocument*>(toJS(&state, JSC::jsCast<JSDOMGlobalObject*>(wrapper.globalObject()), document));
    ASSERT(documentWrapper);
    document.setAttributeEventListener(eventType, createEventListenerForEventHandlerAttribute(state, value, *documentWrapper), currentWorld(state));
}

JSC::JSValue documentEventHandlerAttribute(Document& document, const AtomString& eventType, DOMWrapperWorld& isolatedWorld)
{
    return eventHandlerAttribute(document, eventType, isolatedWorld);
}

void setDocumentEventHandlerAttribute(JSC::ExecState& state, JSC::JSObject& wrapper, Document& document, const AtomString& eventType, JSC::JSValue value)
{
    setEventHandlerAttribute(state, wrapper, document, eventType, value);
}

} // namespace WebCore
