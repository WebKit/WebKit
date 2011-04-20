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
#include "V8DOMWrapper.h"

#include "CSSMutableStyleDeclaration.h"
#include "DOMDataStore.h"
#include "DocumentLoader.h"
#include "FrameLoaderClient.h"
#include "Notification.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8DedicatedWorkerContext.h"
#include "V8DOMApplicationCache.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8EventListener.h"
#include "V8EventListenerList.h"
#include "V8EventSource.h"
#include "V8FileReader.h"
#include "V8FileWriter.h"
#include "V8HTMLCollection.h"
#include "V8HTMLDocument.h"
#include "V8IDBDatabase.h"
#include "V8IDBRequest.h"
#include "V8IDBTransaction.h"
#include "V8IsolatedContext.h"
#include "V8Location.h"
#include "V8MessageChannel.h"
#include "V8NamedNodeMap.h"
#include "V8Node.h"
#include "V8NodeFilterCondition.h"
#include "V8NodeList.h"
#include "V8Notification.h"
#include "V8Proxy.h"
#include "V8SharedWorker.h"
#include "V8SharedWorkerContext.h"
#include "V8StyleSheet.h"
#include "V8WebSocket.h"
#include "V8Worker.h"
#include "V8WorkerContext.h"
#include "V8WorkerContextEventListener.h"
#include "V8XMLHttpRequest.h"
#include "ArrayBufferView.h"
#include "WebGLContextAttributes.h"
#include "WebGLUniformLocation.h"
#include "WorkerContextExecutionProxy.h"
#include "WrapperTypeInfo.h"

#if ENABLE(SVG)
#include "SVGElementInstance.h"
#include "SVGPathSeg.h"
#include "V8SVGElementInstance.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "V8AudioContext.h"
#include "V8JavaScriptAudioNode.h"
#endif

#include <algorithm>
#include <utility>
#include <v8-debug.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

typedef HashMap<Node*, v8::Object*> DOMNodeMap;
typedef HashMap<void*, v8::Object*> DOMObjectMap;

// The caller must have increased obj's ref count.
void V8DOMWrapper::setJSWrapperForDOMObject(void* object, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
    ASSERT(!domWrapperType(wrapper)->toActiveDOMObjectFunction);
    getDOMObjectMap().set(object, wrapper);
}

// The caller must have increased obj's ref count.
void V8DOMWrapper::setJSWrapperForActiveDOMObject(void* object, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
    ASSERT(domWrapperType(wrapper)->toActiveDOMObjectFunction);
    getActiveDOMObjectMap().set(object, wrapper);
}

// The caller must have increased node's ref count.
void V8DOMWrapper::setJSWrapperForDOMNode(Node* node, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
    getDOMNodeMap().set(node, wrapper);
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(WrapperTypeInfo* type, v8::Handle<v8::Value> objectPrototype)
{
    // A DOM constructor is a function instance created from a DOM constructor
    // template. There is one instance per context. A DOM constructor is
    // different from a normal function in two ways:
    //   1) it cannot be called as constructor (aka, used to create a DOM object)
    //   2) its __proto__ points to Object.prototype rather than
    //      Function.prototype.
    // The reason for 2) is that, in Safari, a DOM constructor is a normal JS
    // object, but not a function. Hotmail relies on the fact that, in Safari,
    // HTMLElement.__proto__ == Object.prototype.
    v8::Handle<v8::FunctionTemplate> functionTemplate = type->getTemplate();
    // Getting the function might fail if we're running out of
    // stack or memory.
    v8::TryCatch tryCatch;
    v8::Local<v8::Function> value = functionTemplate->GetFunction();
    if (value.IsEmpty())
        return v8::Local<v8::Function>();
    // Hotmail fix, see comments above.
    if (!objectPrototype.IsEmpty())
        value->SetPrototype(objectPrototype);
    return value;
}

v8::Local<v8::Function> V8DOMWrapper::getConstructorForContext(WrapperTypeInfo* type, v8::Handle<v8::Context> context)
{
    // Enter the scope for this context to get the correct constructor.
    v8::Context::Scope scope(context);

    return getConstructor(type, V8DOMWindowShell::getHiddenObjectPrototype(context));
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(WrapperTypeInfo* type, DOMWindow* window)
{
    Frame* frame = window->frame();
    if (!frame)
        return v8::Local<v8::Function>();

    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return v8::Local<v8::Function>();

    return getConstructorForContext(type, context);
}

#if ENABLE(WORKERS)
v8::Local<v8::Function> V8DOMWrapper::getConstructor(WrapperTypeInfo* type, WorkerContext*)
{
    WorkerScriptController* controller = WorkerScriptController::controllerForContext();
    WorkerContextExecutionProxy* proxy = controller ? controller->proxy() : 0;
    if (!proxy)
        return v8::Local<v8::Function>();

    v8::Handle<v8::Context> context = proxy->context();
    if (context.IsEmpty())
        return v8::Local<v8::Function>();

    return getConstructorForContext(type, context);
}
#endif

void V8DOMWrapper::setHiddenReference(v8::Handle<v8::Object> parent, v8::Handle<v8::Value> child)
{
    v8::Local<v8::Value> hiddenReferenceObject = parent->GetInternalField(v8DOMHiddenReferenceArrayIndex);
    if (hiddenReferenceObject->IsNull() || hiddenReferenceObject->IsUndefined()) {
        hiddenReferenceObject = v8::Array::New();
        parent->SetInternalField(v8DOMHiddenReferenceArrayIndex, hiddenReferenceObject);
    }
    v8::Local<v8::Array> hiddenReferenceArray = v8::Local<v8::Array>::Cast(hiddenReferenceObject);
    hiddenReferenceArray->Set(v8::Integer::New(hiddenReferenceArray->Length()), child);
}

void V8DOMWrapper::setHiddenWindowReference(Frame* frame, v8::Handle<v8::Value> jsObject)
{
    // Get DOMWindow
    if (!frame)
        return; // Object might be detached from window
    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return;

    v8::Handle<v8::Object> global = context->Global();
    // Look for real DOM wrapper.
    global = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), global);
    ASSERT(!global.IsEmpty());

    setHiddenReference(global, jsObject);
}

WrapperTypeInfo* V8DOMWrapper::domWrapperType(v8::Handle<v8::Object> object)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
    return static_cast<WrapperTypeInfo*>(object->GetPointerFromInternalField(v8DOMWrapperTypeIndex));
}

PassRefPtr<NodeFilter> V8DOMWrapper::wrapNativeNodeFilter(v8::Handle<v8::Value> filter)
{
    // A NodeFilter is used when walking through a DOM tree or iterating tree
    // nodes.
    // FIXME: we may want to cache NodeFilterCondition and NodeFilter
    // object, but it is minor.
    // NodeFilter is passed to NodeIterator that has a ref counted pointer
    // to NodeFilter. NodeFilter has a ref counted pointer to NodeFilterCondition.
    // In NodeFilterCondition, filter object is persisted in its constructor,
    // and disposed in its destructor.
    return NodeFilter::create(V8NodeFilterCondition::create(filter));
}

static bool globalObjectPrototypeIsDOMWindow(v8::Handle<v8::Object> objectPrototype)
{
    // We can identify what type of context the global object is wrapping by looking at the
    // internal field count of its prototype. This assumes WorkerContexts and DOMWindows have different numbers
    // of internal fields, so a COMPILE_ASSERT is included to warn if this ever changes.
#if ENABLE(WORKERS)
    COMPILE_ASSERT(V8DOMWindow::internalFieldCount != V8WorkerContext::internalFieldCount,
        DOMWindowAndWorkerContextHaveUnequalFieldCounts);
    COMPILE_ASSERT(V8DOMWindow::internalFieldCount != V8DedicatedWorkerContext::internalFieldCount,
        DOMWindowAndDedicatedWorkerContextHaveUnequalFieldCounts);
#endif
#if ENABLE(SHARED_WORKERS)
    COMPILE_ASSERT(V8DOMWindow::internalFieldCount != V8SharedWorkerContext::internalFieldCount,
        DOMWindowAndSharedWorkerContextHaveUnequalFieldCounts);
#endif
    return objectPrototype->InternalFieldCount() == V8DOMWindow::internalFieldCount;
}

v8::Local<v8::Object> V8DOMWrapper::instantiateV8Object(V8Proxy* proxy, WrapperTypeInfo* type, void* impl)
{
    WorkerContext* workerContext = 0;
    if (V8IsolatedContext::getEntered()) {
        // This effectively disables the wrapper cache for isolated worlds.
        proxy = 0;
        // FIXME: Do we need a wrapper cache for the isolated world?  We should
        //        see if the performance gains are worth while.
        // We'll get one once we give the isolated context a proper window shell.
    } else if (!proxy) {
        v8::Handle<v8::Context> context = v8::Context::GetCurrent();
        if (!context.IsEmpty()) {
            v8::Handle<v8::Object> globalPrototype = v8::Handle<v8::Object>::Cast(context->Global()->GetPrototype());
            if (globalObjectPrototypeIsDOMWindow(globalPrototype))
                proxy = V8Proxy::retrieve(V8DOMWindow::toNative(globalPrototype)->frame());
#if ENABLE(WORKERS)
            else
                workerContext = V8WorkerContext::toNative(lookupDOMWrapper(V8WorkerContext::GetTemplate(), context->Global()));
#endif
        }
    }

    v8::Local<v8::Object> instance;
    if (proxy)
        // FIXME: Fix this to work properly with isolated worlds (see above).
        instance = proxy->windowShell()->createWrapperFromCache(type);
    else {
        v8::Local<v8::Function> function;
#if ENABLE(WORKERS)
        if (workerContext)
            function = getConstructor(type, workerContext);
        else
#endif
            function = type->getTemplate()->GetFunction();
        instance = SafeAllocation::newInstance(function);
    }
    if (!instance.IsEmpty()) {
        // Avoid setting the DOM wrapper for failed allocations.
        setDOMWrapper(instance, type, impl);
        if (type == &V8HTMLDocument::info)
            instance = V8HTMLDocument::WrapInShadowObject(instance, static_cast<Node*>(impl));
    }
    return instance;
}

#ifndef NDEBUG
bool V8DOMWrapper::maybeDOMWrapper(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    if (!object->InternalFieldCount())
        return false;

    ASSERT(object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    return true;
}
#endif

bool V8DOMWrapper::isValidDOMObject(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;
    return v8::Handle<v8::Object>::Cast(value)->InternalFieldCount();
}

bool V8DOMWrapper::isWrapperOfType(v8::Handle<v8::Value> value, WrapperTypeInfo* type)
{
    if (!isValidDOMObject(value))
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    ASSERT(object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    WrapperTypeInfo* typeInfo = static_cast<WrapperTypeInfo*>(object->GetPointerFromInternalField(v8DOMWrapperTypeIndex));
    return typeInfo == type;
}

v8::Handle<v8::Object> V8DOMWrapper::getWrapperSlow(Node* node)
{
    V8IsolatedContext* context = V8IsolatedContext::getEntered();
    if (LIKELY(!context)) {
        v8::Persistent<v8::Object>* wrapper = node->wrapper();
        if (!wrapper)
            return v8::Handle<v8::Object>();
        return *wrapper;
    }
    DOMNodeMapping& domNodeMap = context->world()->domDataStore()->domNodeMap();
    return domNodeMap.get(node);
}

// A JS object of type EventTarget is limited to a small number of possible classes.
// Check EventTarget.h for new type conversion methods
v8::Handle<v8::Value> V8DOMWrapper::convertEventTargetToV8Object(EventTarget* target)
{
    if (!target)
        return v8::Null();

#if ENABLE(SVG)
    if (SVGElementInstance* instance = target->toSVGElementInstance())
        return toV8(instance);
#endif

#if ENABLE(WORKERS)
    if (Worker* worker = target->toWorker())
        return toV8(worker);

    if (DedicatedWorkerContext* workerContext = target->toDedicatedWorkerContext())
        return toV8(workerContext);
#endif // WORKERS

#if ENABLE(SHARED_WORKERS)
    if (SharedWorker* sharedWorker = target->toSharedWorker())
        return toV8(sharedWorker);

    if (SharedWorkerContext* sharedWorkerContext = target->toSharedWorkerContext())
        return toV8(sharedWorkerContext);
#endif // SHARED_WORKERS

#if ENABLE(NOTIFICATIONS)
    if (Notification* notification = target->toNotification())
        return toV8(notification);
#endif

#if ENABLE(INDEXED_DATABASE)
    if (IDBDatabase* idbDatabase = target->toIDBDatabase())
        return toV8(idbDatabase);
    if (IDBRequest* idbRequest = target->toIDBRequest())
        return toV8(idbRequest);
    if (IDBTransaction* idbTransaction = target->toIDBTransaction())
        return toV8(idbTransaction);
#endif

#if ENABLE(WEB_SOCKETS)
    if (WebSocket* webSocket = target->toWebSocket())
        return toV8(webSocket);
#endif

    if (Node* node = target->toNode())
        return toV8(node);

    if (DOMWindow* domWindow = target->toDOMWindow())
        return toV8(domWindow);

    // XMLHttpRequest is created within its JS counterpart.
    if (XMLHttpRequest* xmlHttpRequest = target->toXMLHttpRequest()) {
        v8::Handle<v8::Object> wrapper = getActiveDOMObjectMap().get(xmlHttpRequest);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    // MessagePort is created within its JS counterpart
    if (MessagePort* port = target->toMessagePort()) {
        v8::Handle<v8::Object> wrapper = getActiveDOMObjectMap().get(port);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    if (XMLHttpRequestUpload* upload = target->toXMLHttpRequestUpload()) {
        v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(upload);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (DOMApplicationCache* domAppCache = target->toDOMApplicationCache())
        return toV8(domAppCache);
#endif

#if ENABLE(EVENTSOURCE)
    if (EventSource* eventSource = target->toEventSource())
        return toV8(eventSource);
#endif

#if ENABLE(BLOB)
    if (FileReader* fileReader = target->toFileReader())
        return toV8(fileReader);
#endif

#if ENABLE(FILE_SYSTEM)
    if (FileWriter* fileWriter = target->toFileWriter())
        return toV8(fileWriter);
#endif

#if ENABLE(WEB_AUDIO)
    if (JavaScriptAudioNode* jsAudioNode = target->toJavaScriptAudioNode())
        return toV8(jsAudioNode);
    if (AudioContext* audioContext = target->toAudioContext())
        return toV8(audioContext);
#endif    

    ASSERT(0);
    return notHandledByInterceptor();
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    if (context.IsEmpty())
        return 0;
    if (lookup == ListenerFindOnly)
        return V8EventListenerList::findWrapper(value, isAttribute);
    v8::Handle<v8::Object> globalPrototype = v8::Handle<v8::Object>::Cast(context->Global()->GetPrototype());
    if (globalObjectPrototypeIsDOMWindow(globalPrototype))
        return V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
#if ENABLE(WORKERS)
    return V8EventListenerList::findOrCreateWrapper<V8WorkerContextEventListener>(value, isAttribute);
#else
    return 0;
#endif
}

#if ENABLE(XPATH)
// XPath-related utilities
RefPtr<XPathNSResolver> V8DOMWrapper::getXPathNSResolver(v8::Handle<v8::Value> value, V8Proxy* proxy)
{
    RefPtr<XPathNSResolver> resolver;
    if (V8XPathNSResolver::HasInstance(value))
        resolver = V8XPathNSResolver::toNative(v8::Handle<v8::Object>::Cast(value));
    else if (value->IsObject())
        resolver = V8CustomXPathNSResolver::create(value->ToObject());
    return resolver;
}
#endif

}  // namespace WebCore
