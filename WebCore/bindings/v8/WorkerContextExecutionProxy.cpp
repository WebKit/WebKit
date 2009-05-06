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

#include "V8Binding.h"
#include "V8Proxy.h"
#include "Event.h"
#include "V8WorkerContextEventListener.h"
#include "V8WorkerContextObjectEventListener.h"
#include "Worker.h"
#include "WorkerContext.h"
#include "WorkerLocation.h"
#include "WorkerNavigator.h"
#include "WorkerScriptController.h"

namespace WebCore {

static bool isWorkersEnabled = false;

static void reportFatalErrorInV8(const char* location, const char* message)
{
    // FIXME: We temporarily deal with V8 internal error situations such as out-of-memory by crashing the worker.
    CRASH();
}

static void handleConsoleMessage(v8::Handle<v8::Message> message, v8::Handle<v8::Value> data)
{
    WorkerContextExecutionProxy* proxy = WorkerContextExecutionProxy::retrieve();
    if (!proxy)
        return;
    
    WorkerContext* workerContext = proxy->workerContext();
    if (!workerContext)
        return;
    
    v8::Handle<v8::String> errorMessageString = message->Get();
    ASSERT(!errorMessageString.IsEmpty());
    String errorMessage = ToWebCoreString(errorMessageString);
    
    v8::Handle<v8::Value> resourceName = message->GetScriptResourceName();
    bool useURL = (resourceName.IsEmpty() || !resourceName->IsString());
    String resourceNameString = useURL ? workerContext->url() : ToWebCoreString(resourceName);
    
    workerContext->addMessage(ConsoleDestination, JSMessageSource, ErrorMessageLevel, errorMessage, message->GetLineNumber(), resourceNameString);
}

bool WorkerContextExecutionProxy::isWebWorkersEnabled()
{
    return isWorkersEnabled;
}

void WorkerContextExecutionProxy::setIsWebWorkersEnabled(bool value)
{
    isWorkersEnabled = value;
}

WorkerContextExecutionProxy::WorkerContextExecutionProxy(WorkerContext* workerContext)
    : m_workerContext(workerContext)
    , m_recursion(0)
{
    initV8IfNeeded();
}

WorkerContextExecutionProxy::~WorkerContextExecutionProxy()
{
    dispose();
}

void WorkerContextExecutionProxy::dispose()
{
    // Disconnect all event listeners.
    if (m_listeners.get())
    {
        for (V8EventListenerList::iterator iterator(m_listeners->begin()); iterator != m_listeners->end(); ++iterator)
           static_cast<V8WorkerContextEventListener*>(*iterator)->disconnect();

        {
            // Need to use lock since V8EventListenerList::clear() creates HandleScope.
            v8::Locker locker;
            m_listeners->clear();
        }
    }

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
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    v8::Handle<v8::Object> global = context->Global();
    global = V8Proxy::LookupDOMWrapper(V8ClassIndex::WORKERCONTEXT, global);
    // Return 0 if the current executing context is not the worker context.
    if (global.IsEmpty())
        return 0;
    WorkerContext* workerContext = V8Proxy::ToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, global);
    return workerContext->script()->proxy();
}

void WorkerContextExecutionProxy::initV8IfNeeded()
{
    static bool v8Initialized = false;

    // Use v8::Locker to guarantee that only one thread can enter the following one-time initialization code.
    v8::Locker locker;
    if (v8Initialized)
        return;

    // Tell V8 not to call the default OOM handler, binding code will handle it.
    v8::V8::IgnoreOutOfMemoryException();
    v8::V8::SetFatalErrorHandler(reportFatalErrorInV8);

    // Set up the handler for V8 error message.
    v8::V8::AddMessageListener(handleConsoleMessage);

    // Enable preemption so that one worker will not be blocked by another long-running worker.
    const int workerThreadPreemptionIntervalMs = 100;
    v8::Locker::StartPreemption(workerThreadPreemptionIntervalMs);

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
    v8::Handle<v8::Function> workerContextConstructor = GetConstructor(V8ClassIndex::WORKERCONTEXT);
    v8::Local<v8::Object> jsWorkerContext = SafeAllocation::NewInstance(workerContextConstructor);
    // Bail out if allocation failed.
    if (jsWorkerContext.IsEmpty()) {
        dispose();
        return;
    }

    // Wrap the object.
    V8Proxy::SetDOMWrapper(jsWorkerContext, V8ClassIndex::ToInt(V8ClassIndex::WORKERCONTEXT), m_workerContext);

    V8Proxy::SetJSWrapperForDOMObject(m_workerContext, v8::Persistent<v8::Object>::New(jsWorkerContext));
    m_workerContext->ref();

    // Insert the object instance as the prototype of the shadow object.
    v8::Handle<v8::Object> globalObject = m_context->Global();
    globalObject->Set(implicitProtoString, jsWorkerContext);

    m_listeners.set(new V8EventListenerList());
}

v8::Local<v8::Function> WorkerContextExecutionProxy::GetConstructor(V8ClassIndex::V8WrapperType type)
{
    // Enter the context of the proxy to make sure that the function is
    // constructed in the context corresponding to this proxy.
    v8::Context::Scope scope(m_context);
    v8::Handle<v8::FunctionTemplate> functionTemplate = V8Proxy::GetTemplate(type);

    // Getting the function might fail if we're running out of stack or memory.
    v8::TryCatch tryCatch;
    v8::Local<v8::Function> value = functionTemplate->GetFunction();
    if (value.IsEmpty())
        return v8::Local<v8::Function>();

    return value;
}

v8::Handle<v8::Value> WorkerContextExecutionProxy::ToV8Object(V8ClassIndex::V8WrapperType type, void* impl)
{
    if (!impl)
        return v8::Null();

    if (type == V8ClassIndex::WORKERCONTEXT)
        return WorkerContextToV8Object(static_cast<WorkerContext*>(impl));

    if (type == V8ClassIndex::WORKER) {
        v8::Persistent<v8::Object> result = getActiveDOMObjectMap().get(impl);
        if (!result.IsEmpty())
            return result;

        v8::Local<v8::Object> object = toV8(type, type, impl);
        if (!object.IsEmpty())
            static_cast<Worker*>(impl)->ref();
        result = v8::Persistent<v8::Object>::New(object);
        V8Proxy::SetJSWrapperForDOMObject(impl, result);
        return result;
    }

    // Non DOM node
    v8::Persistent<v8::Object> result = domObjectMap().get(impl);
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
            default:
                ASSERT(false);
            }
            result = v8::Persistent<v8::Object>::New(object);
            V8Proxy::SetJSWrapperForDOMObject(impl, result);
        }
    }
    return result;
}

v8::Handle<v8::Value> WorkerContextExecutionProxy::EventToV8Object(Event* event)
{
    if (!event)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = domObjectMap().get(event);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type = V8ClassIndex::EVENT;

    if (event->isMessageEvent())
        type = V8ClassIndex::MESSAGEEVENT;

    v8::Handle<v8::Object> result = toV8(type, V8ClassIndex::EVENT, event);
    if (result.IsEmpty()) {
        // Instantiation failed. Avoid updating the DOM object map and return null which
        // is already handled by callers of this function in case the event is NULL.
        return v8::Null();
    }

    event->ref();  // fast ref
    V8Proxy::SetJSWrapperForDOMObject(event, v8::Persistent<v8::Object>::New(result));

    return result;
}

// A JS object of type EventTarget in the worker context can only be Worker or WorkerContext.
v8::Handle<v8::Value> WorkerContextExecutionProxy::EventTargetToV8Object(EventTarget* target)
{
    if (!target)
        return v8::Null();

    WorkerContext* workerContext = target->toWorkerContext();
    if (workerContext)
        return WorkerContextToV8Object(workerContext);

    Worker* worker = target->toWorker();
    if (worker)
        return ToV8Object(V8ClassIndex::WORKER, worker);

    ASSERT_NOT_REACHED();
    return v8::Handle<v8::Value>();
}

v8::Handle<v8::Value> WorkerContextExecutionProxy::WorkerContextToV8Object(WorkerContext* workerContext)
{
    if (!workerContext)
        return v8::Null();

    v8::Handle<v8::Context> context = workerContext->script()->proxy()->GetContext();

    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

v8::Local<v8::Object> WorkerContextExecutionProxy::toV8(V8ClassIndex::V8WrapperType descType, V8ClassIndex::V8WrapperType cptrType, void* impl)
{
    v8::Local<v8::Function> function;
    WorkerContextExecutionProxy* proxy = retrieve();
    if (proxy)
        function = proxy->GetConstructor(descType);
    else
        function = V8Proxy::GetTemplate(descType)->GetFunction();

    v8::Local<v8::Object> instance = SafeAllocation::NewInstance(function);
    if (!instance.IsEmpty()) {
        // Avoid setting the DOM wrapper for failed allocations.
        V8Proxy::SetDOMWrapper(instance, V8ClassIndex::ToInt(cptrType), impl);
    }
    return instance;
}

bool WorkerContextExecutionProxy::forgetV8EventObject(Event* event)
{
    if (domObjectMap().contains(event)) {
        domObjectMap().forget(event);
        return true;
    } else
        return false;
}

v8::Local<v8::Value> WorkerContextExecutionProxy::evaluate(const String& script, const String& fileName, int baseLine)
{
    v8::Locker locker;
    v8::HandleScope hs;

    initContextIfNeeded();
    v8::Context::Scope scope(m_context);

    v8::Local<v8::String> scriptString = v8ExternalString(script);
    v8::Handle<v8::Script> compiledScript = V8Proxy::CompileScript(scriptString, fileName, baseLine);
    return runScript(compiledScript);
}

v8::Local<v8::Value> WorkerContextExecutionProxy::runScript(v8::Handle<v8::Script> script)
{
    if (script.IsEmpty())
        return v8::Local<v8::Value>();

    // Compute the source string and prevent against infinite recursion.
    if (m_recursion >= kMaxRecursionDepth) {
        v8::Local<v8::String> code = v8ExternalString("throw RangeError('Recursion too deep')");
        script = V8Proxy::CompileScript(code, "", 0);
    }

    if (V8Proxy::HandleOutOfMemory())
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

PassRefPtr<V8EventListener> WorkerContextExecutionProxy::findOrCreateEventListenerHelper(v8::Local<v8::Value> object, bool isInline, bool findOnly, bool createObjectEventListener)
{
    if (!object->IsObject())
        return 0;

    V8EventListener* listener = m_listeners->find(object->ToObject(), isInline);
    if (findOnly)
        return listener;

    // Create a new one, and add to cache.
    RefPtr<V8EventListener> newListener;
    if (createObjectEventListener)
        newListener = V8WorkerContextObjectEventListener::create(this, v8::Local<v8::Object>::Cast(object), isInline);
    else
        newListener = V8WorkerContextEventListener::create(this, v8::Local<v8::Object>::Cast(object), isInline);
    {
        // Need to use lock since V8EventListenerList::add() creates HandleScope.
        v8::Locker locker;
        m_listeners->add(newListener.get());
    }

    return newListener.release();
}

PassRefPtr<V8EventListener> WorkerContextExecutionProxy::findOrCreateEventListener(v8::Local<v8::Value> object, bool isInline, bool findOnly)
{
    return findOrCreateEventListenerHelper(object, isInline, findOnly, false);
}

PassRefPtr<V8EventListener> WorkerContextExecutionProxy::findOrCreateObjectEventListener(v8::Local<v8::Value> object, bool isInline, bool findOnly)
{
    return findOrCreateEventListenerHelper(object, isInline, findOnly, true);
}

void WorkerContextExecutionProxy::RemoveEventListener(V8EventListener* listener)
{
    // Need to use lock since V8EventListenerList::remove() creates HandleScope.
    v8::Locker locker;
    m_listeners->remove(listener);
}

void WorkerContextExecutionProxy::trackEvent(Event* event)
{
    m_events.append(event);
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
