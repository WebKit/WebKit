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

#include <wtf/ArrayBufferView.h>
#include "DocumentLoader.h"
#include "EventTargetHeaders.h"
#include "EventTargetInterfaces.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "StylePropertySet.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8EventListener.h"
#include "V8EventListenerList.h"
#include "V8HTMLCollection.h"
#include "V8HTMLDocument.h"
#include "V8HiddenPropertyName.h"
#include "V8Location.h"
#include "V8NamedNodeMap.h"
#include "V8NodeFilterCondition.h"
#include "V8NodeList.h"
#include "V8ObjectConstructor.h"
#include "V8PerContextData.h"
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

v8::Persistent<v8::Object> V8DOMWrapper::setJSWrapperForDOMNode(PassRefPtr<Node> node, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate)
{
    v8::Persistent<v8::Object> wrapperHandle = v8::Persistent<v8::Object>::New(wrapper);
    ASSERT(maybeDOMWrapper(wrapperHandle));
    ASSERT(!node->isActiveNode());
    getDOMNodeMap(isolate).set(node.leakRef(), wrapperHandle);
    return wrapperHandle;
}

v8::Persistent<v8::Object> V8DOMWrapper::setJSWrapperForActiveDOMNode(PassRefPtr<Node> node, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate)
{
    v8::Persistent<v8::Object> wrapperHandle = v8::Persistent<v8::Object>::New(wrapper);
    ASSERT(maybeDOMWrapper(wrapperHandle));
    ASSERT(node->isActiveNode());
    getActiveDOMNodeMap(isolate).set(node.leakRef(), wrapperHandle);
    return wrapperHandle;
}

void V8DOMWrapper::setNamedHiddenReference(v8::Handle<v8::Object> parent, const char* name, v8::Handle<v8::Value> child)
{
    ASSERT(name);
    parent->SetHiddenValue(V8HiddenPropertyName::hiddenReferenceName(name, strlen(name)), child);
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

v8::Local<v8::Object> V8DOMWrapper::instantiateV8Object(Document* document, WrapperTypeInfo* type, void* impl)
{
    V8PerContextData* perContextData = 0;

    // If we have a pointer to the frame, we cna get the V8PerContextData
    // directly, which is faster than going through V8.
    if (document && document->frame())
        perContextData = perContextDataForCurrentWorld(document->frame());
    else
        perContextData = V8PerContextData::from(v8::Context::GetCurrent());

    v8::Local<v8::Object> instance = perContextData ? perContextData->createWrapperFromCache(type) : V8ObjectConstructor::newInstance(type->getTemplate()->GetFunction());

    // Avoid setting the DOM wrapper for failed allocations.
    if (instance.IsEmpty())
        return instance;

    setDOMWrapper(instance, type, impl);
    if (type == &V8HTMLDocument::info)
        instance = V8HTMLDocument::wrapInShadowObject(instance, static_cast<Node*>(impl));

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

    v8::HandleScope scope;
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

#define TRY_TO_WRAP_WITH_INTERFACE(interfaceName) \
    if (eventNames().interfaceFor##interfaceName == desiredInterface) \
        return toV8(static_cast<interfaceName*>(target), creationContext, isolate);

// A JS object of type EventTarget is limited to a small number of possible classes.
v8::Handle<v8::Value> V8DOMWrapper::convertEventTargetToV8Object(EventTarget* target, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!target)
        return v8NullWithCheck(isolate);

    AtomicString desiredInterface = target->interfaceName();
    DOM_EVENT_TARGET_INTERFACES_FOR_EACH(TRY_TO_WRAP_WITH_INTERFACE)

    ASSERT_NOT_REACHED();
    return v8Undefined();
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    if (context.IsEmpty())
        return 0;
    if (lookup == ListenerFindOnly)
        return V8EventListenerList::findWrapper(value, isAttribute);
    if (isWrapperOfType(toInnerGlobalObject(context), &V8DOMWindow::info))
        return V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
#if ENABLE(WORKERS)
    return V8EventListenerList::findOrCreateWrapper<V8WorkerContextEventListener>(value, isAttribute);
#else
    return 0;
#endif
}

}  // namespace WebCore
