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

#ifndef V8DOMWrapper_h
#define V8DOMWrapper_h

#include "DOMDataStore.h"
#include "Event.h"
#include "Node.h"
#include "NodeFilter.h"
#include "PlatformString.h"
#include "V8CustomXPathNSResolver.h"
#include "V8DOMMap.h"
#include "V8IsolatedContext.h"
#include "V8Utilities.h"
#include "WrapperTypeInfo.h"
#include <v8.h>
#include <wtf/MainThread.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

    class DOMWindow;
    class EventTarget;
    class Frame;
    class Node;
    class V8PerContextData;
    class V8Proxy;
    class WorkerContext;
    class XPathResolver;

    enum ListenerLookupType {
        ListenerFindOnly,
        ListenerFindOrCreate,
    };

    class V8DOMWrapper {
    public:
#ifndef NDEBUG
        // Checks if a v8 value can be a DOM wrapper
        static bool maybeDOMWrapper(v8::Handle<v8::Value>);
#endif

        // Sets contents of a DOM wrapper.
        static void setDOMWrapper(v8::Handle<v8::Object> object, WrapperTypeInfo* type, void* cptr)
        {
            ASSERT(object->InternalFieldCount() >= 2);
            object->SetPointerInInternalField(v8DOMWrapperObjectIndex, cptr);
            object->SetPointerInInternalField(v8DOMWrapperTypeIndex, type);
        }

        static v8::Handle<v8::Object> lookupDOMWrapper(v8::Handle<v8::FunctionTemplate> functionTemplate, v8::Handle<v8::Object> object)
        {
            return object.IsEmpty() ? object : object->FindInstanceInPrototypeChain(functionTemplate);
        }

        static WrapperTypeInfo* domWrapperType(v8::Handle<v8::Object>);

        static v8::Handle<v8::Value> convertEventTargetToV8Object(PassRefPtr<EventTarget> eventTarget, v8::Isolate* isolate = 0)
        {
            return convertEventTargetToV8Object(eventTarget.get(), isolate);
        }

        static v8::Handle<v8::Value> convertEventTargetToV8Object(EventTarget*, v8::Isolate* = 0);

        static PassRefPtr<EventListener> getEventListener(v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup);

        // XPath-related utilities
        static RefPtr<XPathNSResolver> getXPathNSResolver(v8::Handle<v8::Value> value, V8Proxy* proxy = 0);

        // Wrap JS node filter in C++.
        static PassRefPtr<NodeFilter> wrapNativeNodeFilter(v8::Handle<v8::Value>);

        static v8::Local<v8::Function> constructorForType(WrapperTypeInfo*, DOMWindow*);
#if ENABLE(WORKERS)
        static v8::Local<v8::Function> constructorForType(WrapperTypeInfo*, WorkerContext*);
#endif

        template<typename T>
        static v8::Persistent<v8::Object> setJSWrapperForDOMObject(PassRefPtr<T>, v8::Handle<v8::Object>, v8::Isolate* = 0);
        template<typename T>
        static v8::Persistent<v8::Object> setJSWrapperForActiveDOMObject(PassRefPtr<T>, v8::Handle<v8::Object>, v8::Isolate* = 0);
        static v8::Persistent<v8::Object> setJSWrapperForDOMNode(PassRefPtr<Node>, v8::Handle<v8::Object>, v8::Isolate* = 0);
        static v8::Persistent<v8::Object> setJSWrapperForActiveDOMNode(PassRefPtr<Node>, v8::Handle<v8::Object>, v8::Isolate* = 0);

        static bool isValidDOMObject(v8::Handle<v8::Value>);

        // Check whether a V8 value is a wrapper of type |classType|.
        static bool isWrapperOfType(v8::Handle<v8::Value>, WrapperTypeInfo*);

        // Proper object lifetime support.
        //
        // Helper functions to make sure the child object stays alive
        // while the parent is alive. Using the name more than once
        // overwrites previous references making it possible to free
        // old children.
        static void setNamedHiddenReference(v8::Handle<v8::Object> parent, const char* name, v8::Handle<v8::Value> child);
        static void setNamedHiddenWindowReference(Frame*, const char*, v8::Handle<v8::Value>);

        static v8::Local<v8::Object> instantiateV8Object(V8Proxy* proxy, WrapperTypeInfo*, void* impl);

        static v8::Handle<v8::Object> getCachedWrapper(Node* node)
        {
            ASSERT(isMainThread());
            if (LIKELY(!DOMWrapperWorld::count())) {
                v8::Persistent<v8::Object>* wrapper = node->wrapper();
                if (LIKELY(!!wrapper))
                    return *wrapper;
            }

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
    private:
        static V8PerContextData* perContextData(V8Proxy*);
#if ENABLE(WORKERS)
        static V8PerContextData* perContextData(WorkerContext*);
#endif
    };

    template<typename T>
    v8::Persistent<v8::Object> V8DOMWrapper::setJSWrapperForDOMObject(PassRefPtr<T> object, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate)
    {
        v8::Persistent<v8::Object> wrapperHandle = v8::Persistent<v8::Object>::New(wrapper);
        ASSERT(maybeDOMWrapper(wrapperHandle));
        ASSERT(!domWrapperType(wrapperHandle)->toActiveDOMObjectFunction);
        getDOMObjectMap(isolate).set(object.leakRef(), wrapperHandle);
        return wrapperHandle;
    }

    template<typename T>
    v8::Persistent<v8::Object> V8DOMWrapper::setJSWrapperForActiveDOMObject(PassRefPtr<T> object, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate)
    {
        v8::Persistent<v8::Object> wrapperHandle = v8::Persistent<v8::Object>::New(wrapper);
        ASSERT(maybeDOMWrapper(wrapperHandle));
        ASSERT(domWrapperType(wrapperHandle)->toActiveDOMObjectFunction);
        getActiveDOMObjectMap(isolate).set(object.leakRef(), wrapperHandle);
        return wrapperHandle;
    }
}

#endif // V8DOMWrapper_h
