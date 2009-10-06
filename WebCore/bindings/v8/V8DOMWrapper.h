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

#include "Document.h"
#include "Event.h"
#include "Node.h"
#include "NodeFilter.h"
#include "PlatformString.h" // for WebCore::String
#include "V8CustomBinding.h"
#include "V8CustomXPathNSResolver.h"
#include "V8DOMMap.h"
#include "V8Index.h"
#include "V8Utilities.h"
#include "V8XPathNSResolver.h"
#include "XPathNSResolver.h"
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
#if ENABLE(WEB_SOCKETS)
    class WebSocket;
#endif
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
        static void setDOMWrapper(v8::Handle<v8::Object>, int type, void* ptr);

        static v8::Handle<v8::Object> lookupDOMWrapper(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Object> object)
        {
            return object.IsEmpty() ? object : object->FindInstanceInPrototypeChain(getTemplate(type));
        }

        // A helper function extract native object pointer from a DOM wrapper
        // and cast to the specified type.
        template <class C>
        static C* convertDOMWrapperToNative(v8::Handle<v8::Object> object)
        {
            ASSERT(maybeDOMWrapper(object));
            return reinterpret_cast<C*>(object->GetPointerFromInternalField(V8Custom::kDOMWrapperObjectIndex));
        }

        template <class C>
        static C* convertDOMWrapperToNode(v8::Handle<v8::Object> object)
        {
            ASSERT(maybeDOMWrapper(object));
            ASSERT(domWrapperType(object) == V8ClassIndex::NODE);
            return convertDOMWrapperToNative<C>(object);
        }

        template<typename T>
        static v8::Handle<v8::Value> convertToV8Object(V8ClassIndex::V8WrapperType type, PassRefPtr<T> imp)
        {
            return convertToV8Object(type, imp.get());
        }

        static v8::Handle<v8::Value> convertToV8Object(V8ClassIndex::V8WrapperType, void*);

        // Fast-path for Node objects.
        static v8::Handle<v8::Value> convertNodeToV8Object(PassRefPtr<Node> node)
        {
            return convertNodeToV8Object(node.get());
        }

        static v8::Handle<v8::Value> convertNodeToV8Object(Node* node)
        {
            if (!node)
                return v8::Null();

            Document* document = node->document();
            if (node == document)
                return convertDocumentToV8Object(document);

            DOMWrapperMap<Node>& domNodeMap = getDOMNodeMap();
            v8::Handle<v8::Object> wrapper = domNodeMap.get(node);
            if (wrapper.IsEmpty())
                return convertNewNodeToV8Object(node, 0, domNodeMap);

            return wrapper;
        }

        static v8::Handle<v8::Value> convertDocumentToV8Object(Document*);

        static v8::Handle<v8::Value> convertNewNodeToV8Object(Node*, V8Proxy*, DOMWrapperMap<Node>&);

        template <class C>
        static C* convertToNativeObject(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Object> object)
        {
            // Native event listener is per frame, it cannot be handled by this generic function.
            ASSERT(type != V8ClassIndex::EVENTLISTENER);
            ASSERT(type != V8ClassIndex::EVENTTARGET);

            ASSERT(maybeDOMWrapper(object));

#ifndef NDEBUG
            const bool typeIsValid =
#define MAKE_CASE(TYPE, NAME) (type != V8ClassIndex::TYPE) &&
                DOM_NODE_TYPES(MAKE_CASE)
#if ENABLE(SVG)
                SVG_NODE_TYPES(MAKE_CASE)
#endif
#undef MAKE_CASE
                true;
            ASSERT(typeIsValid);
#endif

            return convertDOMWrapperToNative<C>(object);
        }

        static V8ClassIndex::V8WrapperType domWrapperType(v8::Handle<v8::Object>);

        static v8::Handle<v8::Value> convertEventToV8Object(PassRefPtr<Event> event)
        {
            return convertEventToV8Object(event.get());
        }

        static v8::Handle<v8::Value> convertEventToV8Object(Event*);

        static Event* convertToNativeEvent(v8::Handle<v8::Value> jsEvent)
        {
            if (!isDOMEventWrapper(jsEvent))
                return 0;
            return convertDOMWrapperToNative<Event>(v8::Handle<v8::Object>::Cast(jsEvent));
        }

        static v8::Handle<v8::Value> convertEventTargetToV8Object(PassRefPtr<EventTarget> eventTarget)
        {
            return convertEventTargetToV8Object(eventTarget.get());
        }

        static v8::Handle<v8::Value> convertEventTargetToV8Object(EventTarget*);

        // Wrap and unwrap JS event listeners.
        static v8::Handle<v8::Value> convertEventListenerToV8Object(PassRefPtr<EventListener> eventListener)
        {
            return convertEventListenerToV8Object(eventListener.get());
        }

        static v8::Handle<v8::Value> convertEventListenerToV8Object(EventListener*);

        static PassRefPtr<EventListener> getEventListener(Node* node, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup);

        static PassRefPtr<EventListener> getEventListener(SVGElementInstance* element, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup);

        static PassRefPtr<EventListener> getEventListener(AbstractWorker* worker, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup);

#if ENABLE(NOTIFICATIONS)
        static PassRefPtr<EventListener> getEventListener(Notification* notification, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup);
#endif

        static PassRefPtr<EventListener> getEventListener(WorkerContext* workerContext, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup);

        static PassRefPtr<EventListener> getEventListener(XMLHttpRequestUpload* upload, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup);

        static PassRefPtr<EventListener> getEventListener(EventTarget* eventTarget, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup);

        static PassRefPtr<EventListener> getEventListener(V8Proxy* proxy, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup);


        // XPath-related utilities
        static RefPtr<XPathNSResolver> getXPathNSResolver(v8::Handle<v8::Value> value)
        {
            RefPtr<XPathNSResolver> resolver;
            if (V8XPathNSResolver::HasInstance(value))
                resolver = convertToNativeObject<XPathNSResolver>(V8ClassIndex::XPATHNSRESOLVER, v8::Handle<v8::Object>::Cast(value));
            else if (value->IsObject())
                resolver = V8CustomXPathNSResolver::create(value->ToObject());
            return resolver;
        }

        // DOMImplementation is a singleton and it is handled in a special
        // way. A wrapper is generated per document and stored in an
        // internal field of the document.
        static v8::Handle<v8::Value> convertDOMImplementationToV8Object(DOMImplementation*);

        // Wrap JS node filter in C++.
        static PassRefPtr<NodeFilter> wrapNativeNodeFilter(v8::Handle<v8::Value>);

        static v8::Persistent<v8::FunctionTemplate> getTemplate(V8ClassIndex::V8WrapperType);
        static v8::Local<v8::Function> getConstructorForContext(V8ClassIndex::V8WrapperType, v8::Handle<v8::Context>);
        static v8::Local<v8::Function> getConstructor(V8ClassIndex::V8WrapperType, v8::Handle<v8::Value> objectPrototype);
        static v8::Local<v8::Function> getConstructor(V8ClassIndex::V8WrapperType, DOMWindow*);
        static v8::Local<v8::Function> getConstructor(V8ClassIndex::V8WrapperType, WorkerContext*);

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
