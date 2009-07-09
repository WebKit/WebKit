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

#include "Node.h"
#include "NodeFilter.h"
#include "PlatformString.h" // for WebCore::String
#include "V8CustomBinding.h"
#include "V8Index.h"
#include "V8Utilities.h"
#include <v8.h>

namespace WebCore {

    // FIXME: This probably aren't all needed.
    class CSSRule;
    class CSSRuleList;
    class CSSStyleDeclaration;
    class CSSValue;
    class CSSValueList;
    class ClientRectList;
    class DOMImplementation;
    class DOMWindow;
    class Document;
    class Element;
    class Event;
    class EventListener;
    class EventTarget;
    class Frame;
    class HTMLCollection;
    class HTMLDocument;
    class HTMLElement;
    class HTMLOptionsCollection;
    class MediaList;
    class MimeType;
    class MimeTypeArray;
    class NamedNodeMap;
    class Navigator;
    class Node;
    class NodeFilter;
    class NodeList;
    class Plugin;
    class PluginArray;
    class SVGElement;
#if ENABLE(SVG)
    class SVGElementInstance;
#endif
    class Screen;
    class ScriptExecutionContext;
#if ENABLE(DOM_STORAGE)
    class Storage;
    class StorageEvent;
#endif
    class String;
    class StyleSheet;
    class StyleSheetList;
    class V8EventListener;
    class V8ObjectEventListener;

    class V8DOMWrapper {
    public:
#ifndef NDEBUG
        // Checks if a v8 value can be a DOM wrapper
        static bool maybeDOMWrapper(v8::Handle<v8::Value>);
#endif

        // Sets contents of a DOM wrapper.
        static void setDOMWrapper(v8::Handle<v8::Object>, int type, void* ptr);

        static v8::Handle<v8::Object> lookupDOMWrapper(V8ClassIndex::V8WrapperType, v8::Handle<v8::Value>);

        // A helper function extract native object pointer from a DOM wrapper
        // and cast to the specified type.
        template <class C>
        static C* convertDOMWrapperToNative(v8::Handle<v8::Value> object)
        {
            ASSERT(maybeDOMWrapper(object));
            v8::Handle<v8::Value> ptr = v8::Handle<v8::Object>::Cast(object)->GetInternalField(V8Custom::kDOMWrapperObjectIndex);
            return extractCPointer<C>(ptr);
        }

        // A help function extract a node type pointer from a DOM wrapper.
        // Wrapped pointer must be cast to Node* first.
        static void* convertDOMWrapperToNodeHelper(v8::Handle<v8::Value>);

        // Create a V8 wrapper for a C pointer
        static v8::Handle<v8::Value> wrapCPointer(void* cptr)
        {
            // Represent void* as int
            int addr = reinterpret_cast<int>(cptr);
            ASSERT(!(addr & 0x01)); // The address must be aligned.
            return v8::Integer::New(addr >> 1);
        }

        // Take C pointer out of a v8 wrapper.
        template <class C>
        static C* extractCPointer(v8::Handle<v8::Value> obj)
        {
            return static_cast<C*>(extractCPointerImpl(obj));
        }

        template <class C>
        static C* convertDOMWrapperToNode(v8::Handle<v8::Value> value)
        {
            return static_cast<C*>(convertDOMWrapperToNodeHelper(value));
        }

        template<typename T>
        static v8::Handle<v8::Value> convertToV8Object(V8ClassIndex::V8WrapperType type, PassRefPtr<T> imp)
        {
            return convertToV8Object(type, imp.get());
        }

        static v8::Handle<v8::Value> convertToV8Object(V8ClassIndex::V8WrapperType, void*);

        // Fast-path for Node objects.
        static v8::Handle<v8::Value> convertNodeToV8Object(Node*);

        template <class C>
        static C* convertToNativeObject(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> object)
        {
            return static_cast<C*>(convertToNativeObjectImpl(type, object));
        }

        static V8ClassIndex::V8WrapperType domWrapperType(v8::Handle<v8::Object>);

        static v8::Handle<v8::Value> convertEventToV8Object(Event*);

        static Event* convertToNativeEvent(v8::Handle<v8::Value> jsEvent)
        {
            if (!isDOMEventWrapper(jsEvent))
                return 0;
            return convertDOMWrapperToNative<Event>(jsEvent);
        }

        static v8::Handle<v8::Value> convertEventTargetToV8Object(EventTarget*);
        // Wrap and unwrap JS event listeners.
        static v8::Handle<v8::Value> convertEventListenerToV8Object(EventListener*);

        // DOMImplementation is a singleton and it is handled in a special
        // way. A wrapper is generated per document and stored in an
        // internal field of the document.
        static v8::Handle<v8::Value> convertDOMImplementationToV8Object(DOMImplementation*);

        // Wrap JS node filter in C++.
        static PassRefPtr<NodeFilter> wrapNativeNodeFilter(v8::Handle<v8::Value>);

        static v8::Persistent<v8::FunctionTemplate> getTemplate(V8ClassIndex::V8WrapperType);

        // Checks whether a DOM object has a JS wrapper.
        static bool domObjectHasJSWrapper(void*);
        // Set JS wrapper of a DOM object, the caller in charge of increase ref.
        static void setJSWrapperForDOMObject(void*, v8::Persistent<v8::Object>);
        static void setJSWrapperForActiveDOMObject(void*, v8::Persistent<v8::Object>);
        static void setJSWrapperForDOMNode(Node*, v8::Persistent<v8::Object>);

        // Check whether a V8 value is a wrapper of type |classType|.
        static bool isWrapperOfType(v8::Handle<v8::Value>, V8ClassIndex::V8WrapperType);

        static void* convertToSVGPODTypeImpl(V8ClassIndex::V8WrapperType, v8::Handle<v8::Value>);

        // Check whether a V8 value is a DOM Event wrapper.
        static bool isDOMEventWrapper(v8::Handle<v8::Value>);

        static void* convertToNativeObjectImpl(V8ClassIndex::V8WrapperType, v8::Handle<v8::Value>);

        static v8::Handle<v8::Value> convertStyleSheetToV8Object(StyleSheet*);
        static v8::Handle<v8::Value> convertCSSValueToV8Object(CSSValue*);
        static v8::Handle<v8::Value> convertCSSRuleToV8Object(CSSRule*);
        // Returns the JS wrapper of a window object, initializes the environment
        // of the window frame if needed.
        static v8::Handle<v8::Value> convertWindowToV8Object(DOMWindow*);

#if ENABLE(SVG)
        static v8::Handle<v8::Value> convertSVGElementInstanceToV8Object(SVGElementInstance*);
        static v8::Handle<v8::Value> convertSVGObjectWithContextToV8Object(V8ClassIndex::V8WrapperType, void*);
#endif

    private:
        // Set hidden references in a DOMWindow object of a frame.
        static void setHiddenWindowReference(Frame*, const int internalIndex, v8::Handle<v8::Object>);

        // Take C pointer out of a v8 wrapper.
        static void* extractCPointerImpl(v8::Handle<v8::Value> object)
        {
            ASSERT(object->IsNumber());
            int addr = object->Int32Value();
            return reinterpret_cast<void*>(addr << 1);
        }

        static V8ClassIndex::V8WrapperType htmlElementType(HTMLElement*);
#if ENABLE(SVG)
        static V8ClassIndex::V8WrapperType svgElementType(SVGElement*);
#endif

        // The first V8WrapperType specifies the function descriptor
        // used to create JS object. The second V8WrapperType specifies
        // the actual type of the void* for type casting.
        // For example, a HTML element has HTMLELEMENT for the first V8WrapperType, but always
        // use NODE as the second V8WrapperType. JS wrapper stores the second
        // V8WrapperType and the void* as internal fields.
        static v8::Local<v8::Object> instantiateV8Object(V8ClassIndex::V8WrapperType descType, V8ClassIndex::V8WrapperType cptrType, void* impl)
        {
            return instantiateV8Object(NULL, descType, cptrType, impl);
        }

        static v8::Local<v8::Object> instantiateV8Object(V8Proxy*, V8ClassIndex::V8WrapperType, V8ClassIndex::V8WrapperType, void*);

    };

}

#endif // V8DOMWrapper_h
