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

#include "ArrayBufferView.h"
#include "DOMDataStore.h"
#include "DocumentLoader.h"
#include "EventTargetHeaders.h"
#include "EventTargetInterfaces.h"
#include "FrameLoaderClient.h"
#include "StylePropertySet.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8DOMMap.h"
#include "V8EventListener.h"
#include "V8EventListenerList.h"
#include "V8HTMLCollection.h"
#include "V8HTMLDocument.h"
#include "V8HiddenPropertyName.h"
#include "V8IsolatedContext.h"
#include "V8Location.h"
#include "V8NamedNodeMap.h"
#include "V8NodeFilterCondition.h"
#include "V8NodeList.h"
#include "V8Proxy.h"
#include "V8StyleSheet.h"
#include "V8WorkerContextEventListener.h"
#include "WebGLContextAttributes.h"
#include "WebGLUniformLocation.h"
#include "WorkerContextExecutionProxy.h"
#include "WrapperTypeInfo.h"
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
    if (node->isActiveNode())
        getActiveDOMNodeMap().set(node, wrapper);
    else
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


void V8DOMWrapper::setNamedHiddenReference(v8::Handle<v8::Object> parent, const char* name, v8::Handle<v8::Value> child)
{
    parent->SetHiddenValue(V8HiddenPropertyName::hiddenReferenceName(name), child);
}

void V8DOMWrapper::setNamedHiddenWindowReference(Frame* frame, const char* name, v8::Handle<v8::Value> jsObject)
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

    setNamedHiddenReference(global, name, jsObject);
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

v8::Local<v8::Object> V8DOMWrapper::instantiateV8Object(V8Proxy* proxy, WrapperTypeInfo* type, void* impl)
{
#if ENABLE(WORKERS)
    WorkerContext* workerContext = 0;
#endif
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
            if (isWrapperOfType(globalPrototype, &V8DOMWindow::info)) {
                Frame* frame = V8DOMWindow::toNative(globalPrototype)->frame();
                if (frame && frame->script()->canExecuteScripts(NotAboutToExecuteScript))
                    proxy = V8Proxy::retrieve(frame);
            }
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
    ASSERT_UNUSED(wrapper, wrapper->IsNumber() || wrapper->IsExternal());

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
    DOMDataStore* store = context->world()->domDataStore();
    DOMNodeMapping& domNodeMap = node->isActiveNode() ? store->activeDomNodeMap() : store->domNodeMap();
    return domNodeMap.get(node);
}

#define TRY_TO_WRAP_WITH_INTERFACE(interfaceName) \
    if (eventNames().interfaceFor##interfaceName == desiredInterface) \
        return toV8(static_cast<interfaceName*>(target));

// A JS object of type EventTarget is limited to a small number of possible classes.
v8::Handle<v8::Value> V8DOMWrapper::convertEventTargetToV8Object(EventTarget* target)
{
    if (!target)
        return v8::Null();

    AtomicString desiredInterface = target->interfaceName();
    DOM_EVENT_TARGET_INTERFACES_FOR_EACH(TRY_TO_WRAP_WITH_INTERFACE)

    ASSERT_NOT_REACHED();
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
    if (isWrapperOfType(globalPrototype, &V8DOMWindow::info))
        return V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
#if ENABLE(WORKERS)
    return V8EventListenerList::findOrCreateWrapper<V8WorkerContextEventListener>(value, isAttribute);
#else
    return 0;
#endif
}

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

}  // namespace WebCore
