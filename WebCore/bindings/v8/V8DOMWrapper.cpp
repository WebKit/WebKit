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
#include "DOMObjectsInclude.h"
#include "DocumentLoader.h"
#include "FrameLoaderClient.h"
#include "Notification.h"
#include "SVGElementInstance.h"
#include "SVGPathSeg.h"
#include "ScriptController.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8CustomEventListener.h"
#include "V8DOMApplicationCache.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8EventListenerList.h"
#include "V8HTMLCollection.h"
#include "V8HTMLDocument.h"
#include "V8Index.h"
#include "V8IsolatedContext.h"
#include "V8Location.h"
#include "V8MessageChannel.h"
#include "V8NamedNodeMap.h"
#include "V8Node.h"
#include "V8NodeList.h"
#include "V8Notification.h"
#include "V8Proxy.h"
#include "V8SVGElementInstance.h"
#include "V8SharedWorker.h"
#include "V8StyleSheet.h"
#include "V8WebSocket.h"
#include "V8Worker.h"
#include "V8WorkerContext.h"
#include "V8XMLHttpRequest.h"
#include "WebGLArray.h"
#include "WebGLContextAttributes.h"
#include "WebGLUniformLocation.h"
#include "WorkerContextExecutionProxy.h"

#include <algorithm>
#include <utility>
#include <v8.h>
#include <v8-debug.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

typedef HashMap<Node*, v8::Object*> DOMNodeMap;
typedef HashMap<void*, v8::Object*> DOMObjectMap;

#if ENABLE(3D_CANVAS)
void V8DOMWrapper::setIndexedPropertiesToExternalArray(v8::Handle<v8::Object> wrapper,
                                                       int index,
                                                       void* address,
                                                       int length)
{
    v8::ExternalArrayType array_type = v8::kExternalByteArray;
    V8ClassIndex::V8WrapperType classIndex = V8ClassIndex::FromInt(index);
    switch (classIndex) {
    case V8ClassIndex::WEBGLBYTEARRAY:
        array_type = v8::kExternalByteArray;
        break;
    case V8ClassIndex::WEBGLUNSIGNEDBYTEARRAY:
        array_type = v8::kExternalUnsignedByteArray;
        break;
    case V8ClassIndex::WEBGLSHORTARRAY:
        array_type = v8::kExternalShortArray;
        break;
    case V8ClassIndex::WEBGLUNSIGNEDSHORTARRAY:
        array_type = v8::kExternalUnsignedShortArray;
        break;
    case V8ClassIndex::WEBGLINTARRAY:
        array_type = v8::kExternalIntArray;
        break;
    case V8ClassIndex::WEBGLUNSIGNEDINTARRAY:
        array_type = v8::kExternalUnsignedIntArray;
        break;
    case V8ClassIndex::WEBGLFLOATARRAY:
        array_type = v8::kExternalFloatArray;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    wrapper->SetIndexedPropertiesToExternalArrayData(address,
                                                     array_type,
                                                     length);
}
#endif

// The caller must have increased obj's ref count.
void V8DOMWrapper::setJSWrapperForDOMObject(void* object, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
#ifndef NDEBUG
    V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
        ASSERT_NOT_REACHED();
#undef MAKE_CASE
    default:
        break;
    }
#endif
    getDOMObjectMap().set(object, wrapper);
}

// The caller must have increased obj's ref count.
void V8DOMWrapper::setJSWrapperForActiveDOMObject(void* object, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
#ifndef NDEBUG
    V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE: break;
        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
    default: 
        ASSERT_NOT_REACHED();
#undef MAKE_CASE
    }
#endif
    getActiveDOMObjectMap().set(object, wrapper);
}

// The caller must have increased node's ref count.
void V8DOMWrapper::setJSWrapperForDOMNode(Node* node, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
    getDOMNodeMap().set(node, wrapper);
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> objectPrototype)
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
    v8::Handle<v8::FunctionTemplate> functionTemplate = V8ClassIndex::getTemplate(type);
    // Getting the function might fail if we're running out of
    // stack or memory.
    v8::TryCatch tryCatch;
    v8::Local<v8::Function> value = functionTemplate->GetFunction();
    if (value.IsEmpty())
        return v8::Local<v8::Function>();
    // Hotmail fix, see comments above.
    if (!objectPrototype.IsEmpty())
        value->Set(v8::String::New("__proto__"), objectPrototype);
    return value;
}

v8::Local<v8::Function> V8DOMWrapper::getConstructorForContext(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Context> context)
{
    // Enter the scope for this context to get the correct constructor.
    v8::Context::Scope scope(context);

    return getConstructor(type, V8DOMWindowShell::getHiddenObjectPrototype(context));
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(V8ClassIndex::V8WrapperType type, DOMWindow* window)
{
    Frame* frame = window->frame();
    if (!frame)
        return v8::Local<v8::Function>();

    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return v8::Local<v8::Function>();

    return getConstructorForContext(type, context);
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(V8ClassIndex::V8WrapperType type, WorkerContext*)
{
    WorkerContextExecutionProxy* proxy = WorkerContextExecutionProxy::retrieve();
    if (!proxy)
        return v8::Local<v8::Function>();

    v8::Handle<v8::Context> context = proxy->context();
    if (context.IsEmpty())
        return v8::Local<v8::Function>();

    return getConstructorForContext(type, context);
}

void V8DOMWrapper::setHiddenWindowReference(Frame* frame, const int internalIndex, v8::Handle<v8::Object> jsObject)
{
    // Get DOMWindow
    if (!frame)
        return; // Object might be detached from window
    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return;

    ASSERT(internalIndex < V8DOMWindow::internalFieldCount);

    v8::Handle<v8::Object> global = context->Global();
    // Look for real DOM wrapper.
    global = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), global);
    ASSERT(!global.IsEmpty());
    ASSERT(global->GetInternalField(internalIndex)->IsUndefined());
    global->SetInternalField(internalIndex, jsObject);
}

V8ClassIndex::V8WrapperType V8DOMWrapper::domWrapperType(v8::Handle<v8::Object> object)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
    v8::Handle<v8::Value> type = object->GetInternalField(v8DOMWrapperTypeIndex);
    return V8ClassIndex::FromInt(type->Int32Value());
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
    if (!filter->IsFunction())
        return 0;

    NodeFilterCondition* condition = new V8NodeFilterCondition(filter);
    return NodeFilter::create(condition);
}

v8::Local<v8::Object> V8DOMWrapper::instantiateV8ObjectInWorkerContext(V8ClassIndex::V8WrapperType type, void* impl)
{
    WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
    if (!workerContextProxy)
        return instantiateV8Object(0, type, impl);
    v8::Local<v8::Object> instance = SafeAllocation::newInstance(getConstructor(type, workerContextProxy->workerContext()));
    if (!instance.IsEmpty()) {
        // Avoid setting the DOM wrapper for failed allocations.
        setDOMWrapper(instance, V8ClassIndex::ToInt(type), impl);
    }
    return instance;
}

v8::Local<v8::Object> V8DOMWrapper::instantiateV8Object(V8Proxy* proxy, V8ClassIndex::V8WrapperType type, void* impl)
{
    if (V8IsolatedContext::getEntered()) {
        // This effectively disables the wrapper cache for isolated worlds.
        proxy = 0;
        // FIXME: Do we need a wrapper cache for the isolated world?  We should
        //        see if the performance gains are worth while.
        // We'll get one once we give the isolated context a proper window shell.
    } else if (!proxy)
        proxy = V8Proxy::retrieve();

    v8::Local<v8::Object> instance;
    if (proxy)
        // FIXME: Fix this to work properly with isolated worlds (see above).
        instance = proxy->windowShell()->createWrapperFromCache(type);
    else
        instance = SafeAllocation::newInstance(V8ClassIndex::getTemplate(type)->GetFunction());
    if (!instance.IsEmpty()) {
        // Avoid setting the DOM wrapper for failed allocations.
        setDOMWrapper(instance, V8ClassIndex::ToInt(type), impl);
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

    v8::Handle<v8::Value> type = object->GetInternalField(v8DOMWrapperTypeIndex);
    ASSERT(type->IsInt32());
    ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() && type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

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

bool V8DOMWrapper::isWrapperOfType(v8::Handle<v8::Value> value, V8ClassIndex::V8WrapperType classType)
{
    if (!isValidDOMObject(value))
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    ASSERT(object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    v8::Handle<v8::Value> type = object->GetInternalField(v8DOMWrapperTypeIndex);
    ASSERT(type->IsInt32());
    ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() && type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

    return V8ClassIndex::FromInt(type->Int32Value()) == classType;
}

v8::Handle<v8::Object> V8DOMWrapper::getWrapper(Node* node)
{
    ASSERT(WTF::isMainThread());
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

    if (XMLHttpRequest* xhr = target->toXMLHttpRequest())
        return toV8(xhr);

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

    ASSERT(0);
    return notHandledByInterceptor();
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(Node* node, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}

#if ENABLE(SVG)
PassRefPtr<EventListener> V8DOMWrapper::getEventListener(SVGElementInstance* element, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return getEventListener(element->correspondingElement(), value, isAttribute, lookup);
}
#endif

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(AbstractWorker* worker, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (worker->scriptExecutionContext()->isWorkerContext()) {
        WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
        ASSERT(workerContextProxy);
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
    }

    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}

#if ENABLE(NOTIFICATIONS)
PassRefPtr<EventListener> V8DOMWrapper::getEventListener(Notification* notification, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (notification->scriptExecutionContext()->isWorkerContext()) {
        WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
        ASSERT(workerContextProxy);
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
    }

    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}
#endif

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(WorkerContext* workerContext, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    WorkerContextExecutionProxy* workerContextProxy = workerContext->script()->proxy();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);

    return 0;
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(XMLHttpRequestUpload* upload, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return getEventListener(upload->associatedXMLHttpRequest(), value, isAttribute, lookup);
}

#if ENABLE(EVENTSOURCE)
PassRefPtr<EventListener> V8DOMWrapper::getEventListener(EventSource* eventSource, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (V8Proxy::retrieve(eventSource->scriptExecutionContext()))
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);

#if ENABLE(WORKERS)
    WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
#endif

    return 0;
}
#endif

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(EventTarget* eventTarget, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (V8Proxy::retrieve(eventTarget->scriptExecutionContext()))
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);

#if ENABLE(WORKERS)
    WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
#endif

    return 0;
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(V8Proxy* proxy, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}

}  // namespace WebCore
