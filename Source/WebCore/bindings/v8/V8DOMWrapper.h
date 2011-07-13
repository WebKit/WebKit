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

#include "Event.h"
#include "IsolatedWorld.h"
#include "Node.h"
#include "NodeFilter.h"
#include "PlatformString.h"
#include "V8CustomXPathNSResolver.h"
#include "V8Event.h"
#include "V8Utilities.h"
#include "V8XPathNSResolver.h"
#include "WrapperTypeInfo.h"
#include "XPathNSResolver.h"
#include <v8.h>

namespace WebCore {

    class DOMWindow;
    class EventTarget;
    class Frame;
    class Node;
    class V8Proxy;
    class WorkerContext;

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

        static v8::Handle<v8::Value> convertEventTargetToV8Object(PassRefPtr<EventTarget> eventTarget)
        {
            return convertEventTargetToV8Object(eventTarget.get());
        }

        static v8::Handle<v8::Value> convertEventTargetToV8Object(EventTarget*);

        static PassRefPtr<EventListener> getEventListener(v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup);

#if ENABLE(XPATH)
        // XPath-related utilities
        static RefPtr<XPathNSResolver> getXPathNSResolver(v8::Handle<v8::Value> value, V8Proxy* proxy = 0);
#endif

        // Wrap JS node filter in C++.
        static PassRefPtr<NodeFilter> wrapNativeNodeFilter(v8::Handle<v8::Value>);

        static v8::Local<v8::Function> getConstructorForContext(WrapperTypeInfo*, v8::Handle<v8::Context>);
        static v8::Local<v8::Function> getConstructor(WrapperTypeInfo*, v8::Handle<v8::Value> objectPrototype);
        static v8::Local<v8::Function> getConstructor(WrapperTypeInfo*, DOMWindow*);
#if ENABLE(WORKERS)
        static v8::Local<v8::Function> getConstructor(WrapperTypeInfo*, WorkerContext*);
#endif

        // Set JS wrapper of a DOM object, the caller in charge of increase ref.
        static void setJSWrapperForDOMObject(void*, v8::Persistent<v8::Object>);
        static void setJSWrapperForActiveDOMObject(void*, v8::Persistent<v8::Object>);
        static void setJSWrapperForDOMNode(Node*, v8::Persistent<v8::Object>);

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

        static v8::Handle<v8::Object> getWrapper(Node* node)
        {
            ASSERT(WTF::isMainThread());
            if (LIKELY(!IsolatedWorld::count())) {
                v8::Persistent<v8::Object>* wrapper = node->wrapper();
                if (wrapper)
                    return *wrapper;
            }
            return getWrapperSlow(node);
        }

    private:
        static v8::Handle<v8::Object> getWrapperSlow(Node*);
    };

}

#endif // V8DOMWrapper_h
