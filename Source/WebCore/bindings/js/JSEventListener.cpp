/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2021 Apple Inc. All Rights Reserved.
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
#include "CommonVM.h"
#include "ContentSecurityPolicy.h"
#include "EventNames.h"
#include "HTMLElement.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMWindow.h"
#include "JSDocument.h"
#include "JSEvent.h"
#include "JSEventTarget.h"
#include "JSExecState.h"
#include "JSExecStateInstrumentation.h"
#include "JSWorkerGlobalScope.h"
#include "LocalFrame.h"
#include "ScriptController.h"
#include "WebCoreJSClientData.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/VMEntryScopeInlines.h>
#include <JavaScriptCore/Watchdog.h>
#include <wtf/Ref.h>
#include <wtf/Scope.h>

namespace WebCore {
using namespace JSC;

JSEventListener::JSEventListener(JSObject* function, JSObject* wrapper, bool isAttribute, CreatedFromMarkup createdFromMarkup, DOMWrapperWorld& isolatedWorld)
    : EventListener(JSEventListenerType)
    , m_isAttribute(isAttribute)
    , m_wasCreatedFromMarkup(createdFromMarkup == CreatedFromMarkup::Yes)
    , m_isInitialized(false)
    , m_wrapper(wrapper)
    , m_isolatedWorld(&isolatedWorld)
{
    if (function) {
        ASSERT(wrapper);
        m_jsFunction = JSC::Weak<JSC::JSObject>(function);
        m_isInitialized = true;
    }
    if (&isolatedWorld.vm() != commonVMOrNull())
        static_cast<JSVMClientData*>(isolatedWorld.vm().clientData)->addClient(*this);
}

JSEventListener::~JSEventListener() = default;

Ref<JSEventListener> JSEventListener::create(JSC::JSObject& listener, JSC::JSObject& wrapper, bool isAttribute, DOMWrapperWorld& world)
{
    return adoptRef(*new JSEventListener(&listener, &wrapper, isAttribute, CreatedFromMarkup::No, world));
}

JSObject* JSEventListener::initializeJSFunction(ScriptExecutionContext&) const
{
    return nullptr;
}

void JSEventListener::replaceJSFunctionForAttributeListener(JSObject* function, JSObject* wrapper)
{
    ASSERT(m_isAttribute);
    ASSERT(function);
    ASSERT(wrapper);

    m_wasCreatedFromMarkup = false;
    m_jsFunction = Weak { function };
    if (m_isInitialized)
        ASSERT(m_wrapper.get() == wrapper);
    else {
        m_wrapper = Weak { wrapper };
        m_isInitialized = true;
    }
}

JSValue eventHandlerAttribute(EventTarget& eventTarget, const AtomString& eventType, DOMWrapperWorld& isolatedWorld)
{
    if (RefPtr jsListener = eventTarget.attributeEventListener(eventType, isolatedWorld)) {
        if (RefPtr context = eventTarget.scriptExecutionContext()) {
            if (auto* jsFunction = jsListener->ensureJSFunction(*context))
                return jsFunction;
        }
    }

    return jsNull();
}

template<typename Visitor>
inline void JSEventListener::visitJSFunctionImpl(Visitor& visitor)
{
    // If m_wrapper is null, we are not keeping m_jsFunction alive.
    if (!m_wrapper)
        return;

    visitor.append(m_jsFunction);
}

void JSEventListener::visitJSFunction(AbstractSlotVisitor& visitor) { visitJSFunctionImpl(visitor); }
void JSEventListener::visitJSFunction(SlotVisitor& visitor) { visitJSFunctionImpl(visitor); }

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

    auto* jsFunction = ensureJSFunction(scriptExecutionContext);
    if (!jsFunction)
        return;

    if (UNLIKELY(!m_isolatedWorld))
        return;

    auto* globalObject = toJSDOMGlobalObject(scriptExecutionContext, *m_isolatedWorld);
    if (!globalObject)
        return;

    if (scriptExecutionContext.isDocument()) {
        auto* window = jsCast<JSDOMWindow*>(globalObject);
        RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(window->wrapped());
        if (!localDOMWindow || !localDOMWindow->isCurrentlyDisplayedInFrame())
            return;
        if (wasCreatedFromMarkup()) {
            RefPtr element = dynamicDowncast<Element>(*event.target());
            if (!scriptExecutionContext.checkedContentSecurityPolicy()->allowInlineEventHandlers(sourceURL().string(), sourcePosition().m_line, code(), element.get()))
                return;
        }
        // FIXME: Is this check needed for other contexts?
        RefPtr frame = dynamicDowncast<LocalFrame>(window->wrapped().frame());
        if (!frame)
            return;
        CheckedRef script = frame->script();
        if (!script->canExecuteScripts(ReasonForCallingCanExecuteScripts::AboutToExecuteScript) || script->isPaused())
            return;
    }

    auto* jsFunctionGlobalObject = jsFunction->globalObject();

    RefPtr<Event> savedEvent;
    auto* jsFunctionWindow = jsDynamicCast<JSDOMWindow*>(jsFunctionGlobalObject);
    if (jsFunctionWindow) {
        savedEvent = jsFunctionWindow->currentEvent();

        // window.event should not be set when the target is inside a shadow tree, as per the DOM specification.
        if (!event.currentTargetIsInShadowTree())
            jsFunctionWindow->setCurrentEvent(&event);
    }

    auto restoreCurrentEventOnExit = makeScopeExit([&] {
        if (jsFunctionWindow)
            jsFunctionWindow->setCurrentEvent(savedEvent.get());
    });

    JSGlobalObject* lexicalGlobalObject = jsFunctionGlobalObject;

    JSValue handleEventFunction = jsFunction;

    Ref protectedThis { *this };

    auto callData = JSC::getCallData(handleEventFunction);

    // If jsFunction is not actually a function and this is an EventListener, see if it implements callback interface.
    if (callData.type == CallData::Type::None) {
        if (m_isAttribute)
            return;

        handleEventFunction = jsFunction->get(lexicalGlobalObject, builtinNames(vm).handleEventPublicName());
        if (UNLIKELY(scope.exception())) {
            auto* exception = scope.exception();
            scope.clearException();
            event.target()->uncaughtExceptionInEventHandler();
            reportException(jsFunctionGlobalObject, exception);
            return;
        }
        callData = JSC::getCallData(handleEventFunction);
        if (callData.type == CallData::Type::None) {
            event.target()->uncaughtExceptionInEventHandler();
            reportException(jsFunctionGlobalObject, createTypeError(lexicalGlobalObject, "'handleEvent' property of event listener should be callable"_s));
            return;
        }
    }

    MarkedArgumentBuffer args;
    args.append(toJS(lexicalGlobalObject, globalObject, &event));
    ASSERT(!args.hasOverflowed());

    VMEntryScope entryScope(vm, vm.entryScope ? vm.entryScope->globalObject() : lexicalGlobalObject);

    JSExecState::instrumentFunction(&scriptExecutionContext, callData);

    JSValue thisValue = handleEventFunction == jsFunction ? toJS(lexicalGlobalObject, globalObject, event.currentTarget()) : jsFunction;
    NakedPtr<JSC::Exception> uncaughtException;
    JSValue retval = JSExecState::profiledCall(lexicalGlobalObject, JSC::ProfilingReason::Other, handleEventFunction, callData, thisValue, args, uncaughtException);

    InspectorInstrumentation::didCallFunction(&scriptExecutionContext);

    auto handleExceptionIfNeeded = [&] (JSC::Exception* exception) -> bool {
        if (auto* globalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext)) {
            auto* scriptController = globalScope->script();
            bool terminatorCausedException = (exception && vm.isTerminationException(exception));
            if (terminatorCausedException || (scriptController && scriptController->isTerminatingExecution()))
                scriptController->forbidExecution();
        }

        if (exception) {
            event.target()->uncaughtExceptionInEventHandler();
            reportException(jsFunctionGlobalObject, exception);
            return true;
        }
        return false;
    };

    if (handleExceptionIfNeeded(uncaughtException))
        return;

    if (!m_isAttribute) {
        // This is an EventListener and there is therefore no need for any return value handling.
        return;
    }

    // Do return value handling for event handlers (https://html.spec.whatwg.org/#the-event-handler-processing-algorithm).

    if (event.type() == eventNames().beforeunloadEvent) {
        // This is a OnBeforeUnloadEventHandler, and therefore the return value must be coerced into a String.
        if (auto* beforeUnloadEvent = dynamicDowncast<BeforeUnloadEvent>(event)) {
            auto conversionResult = convert<IDLNullable<IDLDOMString>>(*lexicalGlobalObject, retval);
            if (UNLIKELY(conversionResult.hasException(scope))) {
                if (handleExceptionIfNeeded(scope.exception()))
                    return;
            }
            handleBeforeUnloadEventReturnValue(*beforeUnloadEvent, conversionResult.releaseReturnValue());
        }
        return;
    }

    if (retval.isFalse())
        event.preventDefault();
}

bool JSEventListener::operator==(const EventListener& listener) const
{
    auto* other = dynamicDowncast<JSEventListener>(listener);
    return other && m_jsFunction == other->m_jsFunction && m_isAttribute == other->m_isAttribute;
}

String JSEventListener::functionName() const
{
    if (!m_wrapper || !m_jsFunction)
        return { };

    auto& vm = m_isolatedWorld->vm();
    JSC::JSLockHolder lock(vm);

    auto* handlerFunction = JSC::jsDynamicCast<JSC::JSFunction*>(m_jsFunction.get());
    if (!handlerFunction)
        return { };

    return handlerFunction->name(vm);
}

void JSEventListener::willDestroyVM()
{
    m_jsFunction.clear();
    m_wrapper.clear();
    m_isInitialized = false;
    m_isolatedWorld = nullptr;
}

} // namespace WebCore
