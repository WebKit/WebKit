/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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


#include "config.h"

#if ENABLE(WORKERS)

#include "WorkerContextExecutionProxy.h"

#include "DOMCoreException.h"
#include "DedicatedWorkerContext.h"
#include "Event.h"
#include "Notification.h"
#include "NotificationCenter.h"
#include "EventException.h"
#include "MessagePort.h"
#include "RangeException.h"
#include "V8Binding.h"
#include "V8DOMMap.h"
#include "V8Index.h"
#include "V8Proxy.h"
#include "V8WorkerContextEventListener.h"
#if ENABLE(WEB_SOCKETS)
#include "WebSocket.h"
#endif
#include "Worker.h"
#include "WorkerContext.h"
#include "WorkerLocation.h"
#include "WorkerNavigator.h"
#include "WorkerScriptController.h"
#include "XMLHttpRequest.h"
#include "XMLHttpRequestException.h"

namespace WebCore {

static void reportFatalErrorInV8(const char* location, const char* message)
{
    // FIXME: We temporarily deal with V8 internal error situations such as out-of-memory by crashing the worker.
    CRASH();
}

WorkerContextExecutionProxy::WorkerContextExecutionProxy(WorkerContext* workerContext)
    : m_workerContext(workerContext)
    , m_recursion(0)
    , m_listenerGuard(V8ListenerGuard::create())
{
    initV8IfNeeded();
}

WorkerContextExecutionProxy::~WorkerContextExecutionProxy()
{
    dispose();
}

void WorkerContextExecutionProxy::dispose()
{
    m_listenerGuard->disconnectListeners();

    // Detach all events from their JS wrappers.
    for (size_t eventIndex = 0; eventIndex < m_events.size(); ++eventIndex) {
        Event* event = m_events[eventIndex];
        if (forgetV8EventObject(event))
          event->deref();
    }
    m_events.clear();

    // Dispose the context.
    if (!m_context.IsEmpty()) {
        m_context.Dispose();
        m_context.Clear();
    }
}

WorkerContextExecutionProxy* WorkerContextExecutionProxy::retrieve()
{
    // Happens on frame destruction, check otherwise GetCurrent() will crash.
    if (!v8::Context::InContext())
        return 0;
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    v8::Handle<v8::Object> global = context->Global();
    global = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::WORKERCONTEXT, global);
    // Return 0 if the current executing context is not the worker context.
    if (global.IsEmpty())
        return 0;
    WorkerContext* workerContext = V8DOMWrapper::convertToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, global);
    return workerContext->script()->proxy();
}

void WorkerContextExecutionProxy::initV8IfNeeded()
{
    static bool v8Initialized = false;

    if (v8Initialized)
        return;

    // Tell V8 not to call the default OOM handler, binding code will handle it.
    v8::V8::IgnoreOutOfMemoryException();
    v8::V8::SetFatalErrorHandler(reportFatalErrorInV8);

    v8Initialized = true;
}

void WorkerContextExecutionProxy::initContextIfNeeded()
{
    // Bail out if the context has already been initialized.
    if (!m_context.IsEmpty())
        return;

    // Create a new environment
    v8::Persistent<v8::ObjectTemplate> globalTemplate;
    m_context = v8::Context::New(0, globalTemplate);

    // Starting from now, use local context only.
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(m_context);
    v8::Context::Scope scope(context);

    // Allocate strings used during initialization.
    v8::Handle<v8::String> implicitProtoString = v8::String::New("__proto__");

    // Create a new JS object and use it as the prototype for the shadow global object.
    v8::Handle<v8::Function> workerContextConstructor = V8DOMWrapper::getConstructorForContext(V8ClassIndex::DEDICATEDWORKERCONTEXT, context);
    v8::Local<v8::Object> jsWorkerContext = SafeAllocation::newInstance(workerContextConstructor);
    // Bail out if allocation failed.
    if (jsWorkerContext.IsEmpty()) {
        dispose();
        return;
    }

    // Wrap the object.
    V8DOMWrapper::setDOMWrapper(jsWorkerContext, V8ClassIndex::ToInt(V8ClassIndex::DEDICATEDWORKERCONTEXT), m_workerContext);

    V8DOMWrapper::setJSWrapperForDOMObject(m_workerContext, v8::Persistent<v8::Object>::New(jsWorkerContext));
    m_workerContext->ref();

    // Insert the object instance as the prototype of the shadow object.
    v8::Handle<v8::Object> globalObject = m_context->Global();
    globalObject->Set(implicitProtoString, jsWorkerContext);
}

v8::Handle<v8::Value> WorkerContextExecutionProxy::convertToV8Object(V8ClassIndex::V8WrapperType type, void* impl)
{
    if (!impl)
        return v8::Null();

    if (type == V8ClassIndex::DEDICATEDWORKERCONTEXT)
        return convertWorkerContextToV8Object(static_cast<WorkerContext*>(impl));

    bool isActiveDomObject = false;
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
        isActiveDomObject = true;
        break;
#undef MAKE_CASE
    default:
        break;
    }

    if (isActiveDomObject) {
        v8::Persistent<v8::Object> result = getActiveDOMObjectMap().get(impl);
        if (!result.IsEmpty())
            return result;

        v8::Local<v8::Object> object = toV8(type, type, impl);
        switch (type) {
#define MAKE_CASE(TYPE, NAME) \
        case V8ClassIndex::TYPE: static_cast<NAME*>(impl)->ref(); break;
            ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
        default:
            ASSERT_NOT_REACHED();
        }

        result = v8::Persistent<v8::Object>::New(object);
        V8DOMWrapper::setJSWrapperForActiveDOMObject(impl, result);
        return result;
    }

    // Non DOM node
    v8::Persistent<v8::Object> result = getDOMObjectMap().get(impl);
    if (result.IsEmpty()) {
        v8::Local<v8::Object> object = toV8(type, type, impl);
        if (!object.IsEmpty()) {
            switch (type) {
            case V8ClassIndex::WORKERLOCATION:
                static_cast<WorkerLocation*>(impl)->ref();
                break;
            case V8ClassIndex::WORKERNAVIGATOR:
                static_cast<WorkerNavigator*>(impl)->ref();
                break;
#if ENABLE(NOTIFICATIONS)
            case V8ClassIndex::NOTIFICATIONCENTER:
                static_cast<NotificationCenter*>(impl)->ref();
                break;
            case V8ClassIndex::NOTIFICATION:
                static_cast<Notification*>(impl)->ref();
                break;
#endif
            case V8ClassIndex::DOMCOREEXCEPTION:
                static_cast<DOMCoreException*>(impl)->ref();
                break;
            case V8ClassIndex::RANGEEXCEPTION:
                static_cast<RangeException*>(impl)->ref();
                break;
            case V8ClassIndex::EVENTEXCEPTION:
                static_cast<EventException*>(impl)->ref();
                break;
            case V8ClassIndex::XMLHTTPREQUESTEXCEPTION:
                static_cast<XMLHttpRequestException*>(impl)->ref();
                break;
            default:
                ASSERT(false);
            }
            result = v8::Persistent<v8::Object>::New(object);
            V8DOMWrapper::setJSWrapperForDOMObject(impl, result);
        }
    }
    return result;
}

v8::Handle<v8::Value> WorkerContextExecutionProxy::convertEventToV8Object(Event* event)
{
    if (!event)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(event);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type = V8ClassIndex::EVENT;

    if (event->isMessageEvent())
        type = V8ClassIndex::MESSAGEEVENT;

    v8::Handle<v8::Object> result = toV8(type, V8ClassIndex::EVENT, event);
    if (result.IsEmpty()) {
        // Instantiation failed. Avoid updating the DOM object map and return null which
        // is already handled by callers of this function in case the event is null.
        return v8::Null();
    }

    event->ref();  // fast ref
    V8DOMWrapper::setJSWrapperForDOMObject(event, v8::Persistent<v8::Object>::New(result));

    return result;
}

v8::Handle<v8::Value> WorkerContextExecutionProxy::convertEventTargetToV8Object(EventTarget* target)
{
    if (!target)
        return v8::Null();

    DedicatedWorkerContext* workerContext = target->toDedicatedWorkerContext();
    if (workerContext)
        return convertWorkerContextToV8Object(workerContext);

    Worker* worker = target->toWorker();
    if (worker)
        return convertToV8Object(V8ClassIndex::WORKER, worker);

    XMLHttpRequest* xhr = target->toXMLHttpRequest();
    if (xhr)
        return convertToV8Object(V8ClassIndex::XMLHTTPREQUEST, xhr);

    MessagePort* mp = target->toMessagePort();
    if (mp)
        return convertToV8Object(V8ClassIndex::MESSAGEPORT, mp);

    ASSERT_NOT_REACHED();
    return v8::Handle<v8::Value>();
}

v8::Handle<v8::Value> WorkerContextExecutionProxy::convertWorkerContextToV8Object(WorkerContext* workerContext)
{
    if (!workerContext)
        return v8::Null();

    v8::Handle<v8::Context> context = workerContext->script()->proxy()->context();

    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

v8::Local<v8::Object> WorkerContextExecutionProxy::toV8(V8ClassIndex::V8WrapperType descriptorType, V8ClassIndex::V8WrapperType cptrType, void* impl)
{
    v8::Local<v8::Function> function;
    WorkerContextExecutionProxy* proxy = retrieve();
    if (proxy)
        function = V8DOMWrapper::getConstructor(descriptorType, proxy->workerContext());
    else
        function = V8DOMWrapper::getTemplate(descriptorType)->GetFunction();

    v8::Local<v8::Object> instance = SafeAllocation::newInstance(function);
    if (!instance.IsEmpty())
        // Avoid setting the DOM wrapper for failed allocations.
        V8DOMWrapper::setDOMWrapper(instance, V8ClassIndex::ToInt(cptrType), impl);
    return instance;
}

bool WorkerContextExecutionProxy::forgetV8EventObject(Event* event)
{
    if (getDOMObjectMap().contains(event)) {
        getDOMObjectMap().forget(event);
        return true;
    }
    return false;
}

ScriptValue WorkerContextExecutionProxy::evaluate(const String& script, const String& fileName, int baseLine, WorkerContextExecutionState* state)
{
    v8::HandleScope hs;

    initContextIfNeeded();
    v8::Context::Scope scope(m_context);

    v8::TryCatch exceptionCatcher;

    v8::Local<v8::String> scriptString = v8ExternalString(script);
    v8::Handle<v8::Script> compiledScript = V8Proxy::compileScript(scriptString, fileName, baseLine);
    v8::Local<v8::Value> result = runScript(compiledScript);

    if (exceptionCatcher.HasCaught()) {
        v8::Local<v8::Message> message = exceptionCatcher.Message();
        state->hadException = true;
        state->exception = ScriptValue(exceptionCatcher.Exception());
        state->errorMessage = toWebCoreString(message->Get());
        state->lineNumber = message->GetLineNumber();
        state->sourceURL = toWebCoreString(message->GetScriptResourceName());
        exceptionCatcher.Reset();
    } else
        state->hadException = false;

    if (result.IsEmpty() || result->IsUndefined())
        return ScriptValue();

    return ScriptValue(result);
}

v8::Local<v8::Value> WorkerContextExecutionProxy::runScript(v8::Handle<v8::Script> script)
{
    if (script.IsEmpty())
        return v8::Local<v8::Value>();

    // Compute the source string and prevent against infinite recursion.
    if (m_recursion >= kMaxRecursionDepth) {
        v8::Local<v8::String> code = v8ExternalString("throw RangeError('Recursion too deep')");
        script = V8Proxy::compileScript(code, "", 0);
    }

    if (V8Proxy::handleOutOfMemory())
        ASSERT(script.IsEmpty());

    if (script.IsEmpty())
        return v8::Local<v8::Value>();

    // Run the script and keep track of the current recursion depth.
    v8::Local<v8::Value> result;
    {
        m_recursion++;
        result = script->Run();
        m_recursion--;
    }

    // Handle V8 internal error situation (Out-of-memory).
    if (result.IsEmpty())
        return v8::Local<v8::Value>();

    return result;
}

PassRefPtr<V8EventListener> WorkerContextExecutionProxy::findOrCreateEventListener(v8::Local<v8::Value> object, bool isInline, bool findOnly)
{
    return findOnly ? V8EventListenerList::findWrapper(object, isInline) : V8EventListenerList::findOrCreateWrapper<V8WorkerContextEventListener>(this, m_listenerGuard, object, isInline);
}

void WorkerContextExecutionProxy::trackEvent(Event* event)
{
    m_events.append(event);
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
