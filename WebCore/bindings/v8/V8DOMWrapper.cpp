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
#include "ChromiumBridge.h"
#include "DOMObjectsInclude.h"
#include "DocumentLoader.h"
#include "FrameLoaderClient.h"
#include "Notification.h"
#include "SVGElementInstance.h"
#include "ScriptController.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8CustomBinding.h"
#include "V8CustomEventListener.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8EventListenerList.h"
#include "V8Index.h"
#include "V8IsolatedWorld.h"
#include "V8Proxy.h"
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

// Get the string 'toString'.
static v8::Persistent<v8::String> GetToStringName()
{
    DEFINE_STATIC_LOCAL(v8::Persistent<v8::String>, value, ());
    if (value.IsEmpty())
        value = v8::Persistent<v8::String>::New(v8::String::New("toString"));
    return value;
}

static v8::Handle<v8::Value> ConstructorToString(const v8::Arguments& args)
{
    // The DOM constructors' toString functions grab the current toString
    // for Functions by taking the toString function of itself and then
    // calling it with the constructor as its receiver. This means that
    // changes to the Function prototype chain or toString function are
    // reflected when printing DOM constructors. The only wart is that
    // changes to a DOM constructor's toString's toString will cause the
    // toString of the DOM constructor itself to change. This is extremely
    // obscure and unlikely to be a problem.
    v8::Handle<v8::Value> value = args.Callee()->Get(GetToStringName());
    if (!value->IsFunction()) 
        return v8::String::New("");
    return v8::Handle<v8::Function>::Cast(value)->Call(args.This(), 0, 0);
}

#if ENABLE(SVG)
v8::Handle<v8::Value> V8DOMWrapper::convertSVGElementInstanceToV8Object(SVGElementInstance* instance)
{
    if (!instance)
        return v8::Null();

    v8::Handle<v8::Object> existingInstance = getDOMSVGElementInstanceMap().get(instance);
    if (!existingInstance.IsEmpty())
        return existingInstance;

    instance->ref();

    // Instantiate the V8 object and remember it
    v8::Handle<v8::Object> result = instantiateV8Object(V8ClassIndex::SVGELEMENTINSTANCE, V8ClassIndex::SVGELEMENTINSTANCE, instance);
    if (!result.IsEmpty()) {
        // Only update the DOM SVG element map if the result is non-empty.
        getDOMSVGElementInstanceMap().set(instance, v8::Persistent<v8::Object>::New(result));
    }
    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertSVGObjectWithContextToV8Object(V8ClassIndex::V8WrapperType type, void* object)
{
    if (!object)
        return v8::Null();

    v8::Persistent<v8::Object> result = getDOMSVGObjectWithContextMap().get(object);
    if (!result.IsEmpty())
        return result;

    // Special case: SVGPathSegs need to be downcast to their real type
    if (type == V8ClassIndex::SVGPATHSEG)
        type = V8Custom::DowncastSVGPathSeg(object);

    v8::Local<v8::Object> v8Object = instantiateV8Object(type, type, object);
    if (!v8Object.IsEmpty()) {
        result = v8::Persistent<v8::Object>::New(v8Object);
        switch (type) {
#define MAKE_CASE(TYPE, NAME)     \
        case V8ClassIndex::TYPE: static_cast<NAME*>(object)->ref(); break;
        SVG_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
#define MAKE_CASE(TYPE, NAME)     \
        case V8ClassIndex::TYPE:    \
            static_cast<V8SVGPODTypeWrapper<NAME>*>(object)->ref(); break;
        SVG_POD_NATIVE_TYPES(MAKE_CASE)
#undef MAKE_CASE
        default:
            ASSERT_NOT_REACHED();
        }
        getDOMSVGObjectWithContextMap().set(object, result);
    }

    return result;
}

#endif

bool V8DOMWrapper::domObjectHasJSWrapper(void* object)
{
    return getDOMObjectMap().contains(object) || getActiveDOMObjectMap().contains(object);
}

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

v8::Persistent<v8::FunctionTemplate> V8DOMWrapper::getTemplate(V8ClassIndex::V8WrapperType type)
{
    v8::Persistent<v8::FunctionTemplate>* cacheCell = V8ClassIndex::GetCache(type);
    if (!cacheCell->IsEmpty())
        return *cacheCell;

    // Not in the cache.
    FunctionTemplateFactory factory = V8ClassIndex::GetFactory(type);
    v8::Persistent<v8::FunctionTemplate> descriptor = factory();
    // DOM constructors are functions and should print themselves as such.
    // However, we will later replace their prototypes with Object
    // prototypes so we need to explicitly override toString on the
    // instance itself. If we later make DOM constructors full objects
    // we can give them class names instead and Object.prototype.toString
    // will work so we can remove this code.
    DEFINE_STATIC_LOCAL(v8::Persistent<v8::FunctionTemplate>, toStringTemplate, ());
    if (toStringTemplate.IsEmpty())
        toStringTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(ConstructorToString));
    descriptor->Set(GetToStringName(), toStringTemplate);
    switch (type) {
    case V8ClassIndex::CSSSTYLEDECLARATION:
        // The named property handler for style declarations has a
        // setter. Therefore, the interceptor has to be on the object
        // itself and not on the prototype object.
        descriptor->InstanceTemplate()->SetNamedPropertyHandler( USE_NAMED_PROPERTY_GETTER(CSSStyleDeclaration), USE_NAMED_PROPERTY_SETTER(CSSStyleDeclaration));
        setCollectionStringOrNullIndexedGetter<CSSStyleDeclaration>(descriptor);
        break;
    case V8ClassIndex::CSSRULELIST:
        setCollectionIndexedGetter<CSSRuleList, CSSRule>(descriptor,  V8ClassIndex::CSSRULE);
        break;
    case V8ClassIndex::CSSVALUELIST:
        setCollectionIndexedGetter<CSSValueList, CSSValue>(descriptor, V8ClassIndex::CSSVALUE);
        break;
    case V8ClassIndex::CSSVARIABLESDECLARATION:
        setCollectionStringOrNullIndexedGetter<CSSVariablesDeclaration>(descriptor);
        break;
    case V8ClassIndex::WEBKITCSSTRANSFORMVALUE:
        setCollectionIndexedGetter<WebKitCSSTransformValue, CSSValue>(descriptor, V8ClassIndex::CSSVALUE);
        break;
    case V8ClassIndex::HTMLALLCOLLECTION:
        descriptor->InstanceTemplate()->MarkAsUndetectable(); // fall through
    case V8ClassIndex::HTMLCOLLECTION:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLCollection));
        descriptor->InstanceTemplate()->SetCallAsFunctionHandler(USE_CALLBACK(HTMLCollectionCallAsFunction));
        setCollectionIndexedGetter<HTMLCollection, Node>(descriptor, V8ClassIndex::NODE);
        break;
    case V8ClassIndex::HTMLOPTIONSCOLLECTION:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLCollection));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(HTMLOptionsCollection), USE_INDEXED_PROPERTY_SETTER(HTMLOptionsCollection));
        descriptor->InstanceTemplate()->SetCallAsFunctionHandler(USE_CALLBACK(HTMLCollectionCallAsFunction));
        break;
    case V8ClassIndex::HTMLSELECTELEMENT:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLSelectElementCollection));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(nodeCollectionIndexedPropertyGetter<HTMLSelectElement>, USE_INDEXED_PROPERTY_SETTER(HTMLSelectElementCollection),
            0, 0, nodeCollectionIndexedPropertyEnumerator<HTMLSelectElement>, v8::Integer::New(V8ClassIndex::NODE));
        break;
    case V8ClassIndex::HTMLDOCUMENT: {
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLDocument), 0, 0, USE_NAMED_PROPERTY_DELETER(HTMLDocument));

        // We add an extra internal field to all Document wrappers for
        // storing a per document DOMImplementation wrapper.
        //
        // Additionally, we add two extra internal fields for
        // HTMLDocuments to implement temporary shadowing of
        // document.all. One field holds an object that is used as a
        // marker. The other field holds the marker object if
        // document.all is not shadowed and some other value if
        // document.all is shadowed.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        ASSERT(instanceTemplate->InternalFieldCount() == V8Custom::kNodeMinimumInternalFieldCount);
        instanceTemplate->SetInternalFieldCount(V8Custom::kHTMLDocumentInternalFieldCount);
        break;
    }
#if ENABLE(SVG)
    case V8ClassIndex::SVGDOCUMENT:  // fall through
#endif
    case V8ClassIndex::DOCUMENT: {
        // We add an extra internal field to all Document wrappers for
        // storing a per document DOMImplementation wrapper.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        ASSERT(instanceTemplate->InternalFieldCount() == V8Custom::kNodeMinimumInternalFieldCount);
        instanceTemplate->SetInternalFieldCount( V8Custom::kDocumentMinimumInternalFieldCount);
        break;
    }
    case V8ClassIndex::HTMLAPPLETELEMENT:  // fall through
    case V8ClassIndex::HTMLEMBEDELEMENT:  // fall through
    case V8ClassIndex::HTMLOBJECTELEMENT:
        // HTMLAppletElement, HTMLEmbedElement and HTMLObjectElement are
        // inherited from HTMLPlugInElement, and they share the same property
        // handling code.
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLPlugInElement), USE_NAMED_PROPERTY_SETTER(HTMLPlugInElement));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(HTMLPlugInElement), USE_INDEXED_PROPERTY_SETTER(HTMLPlugInElement));
        descriptor->InstanceTemplate()->SetCallAsFunctionHandler(USE_CALLBACK(HTMLPlugInElement));
        break;
    case V8ClassIndex::HTMLFRAMESETELEMENT:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLFrameSetElement));
        break;
    case V8ClassIndex::HTMLFORMELEMENT:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLFormElement));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(HTMLFormElement), 0, 0, 0, nodeCollectionIndexedPropertyEnumerator<HTMLFormElement>, v8::Integer::New(V8ClassIndex::NODE));
        break;
    case V8ClassIndex::STYLESHEET:  // fall through
    case V8ClassIndex::CSSSTYLESHEET: {
        // We add an extra internal field to hold a reference to
        // the owner node.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        ASSERT(instanceTemplate->InternalFieldCount() == V8Custom::kDefaultWrapperInternalFieldCount);
        instanceTemplate->SetInternalFieldCount(V8Custom::kStyleSheetInternalFieldCount);
        break;
    }
    case V8ClassIndex::MEDIALIST:
        setCollectionStringOrNullIndexedGetter<MediaList>(descriptor);
        break;
    case V8ClassIndex::MIMETYPEARRAY:
        setCollectionIndexedAndNamedGetters<MimeTypeArray, MimeType>(descriptor, V8ClassIndex::MIMETYPE);
        break;
    case V8ClassIndex::NAMEDNODEMAP:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(NamedNodeMap));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(NamedNodeMap), 0, 0, 0, collectionIndexedPropertyEnumerator<NamedNodeMap>, v8::Integer::New(V8ClassIndex::NODE));
        break;
#if ENABLE(DOM_STORAGE)
    case V8ClassIndex::STORAGE:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(Storage), USE_NAMED_PROPERTY_SETTER(Storage), 0, USE_NAMED_PROPERTY_DELETER(Storage), V8Custom::v8StorageNamedPropertyEnumerator);
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(Storage), USE_INDEXED_PROPERTY_SETTER(Storage), 0, USE_INDEXED_PROPERTY_DELETER(Storage));
        break;
#endif
    case V8ClassIndex::NODELIST:
        setCollectionIndexedGetter<NodeList, Node>(descriptor, V8ClassIndex::NODE);
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(NodeList));
        break;
    case V8ClassIndex::PLUGIN:
        setCollectionIndexedAndNamedGetters<Plugin, MimeType>(descriptor, V8ClassIndex::MIMETYPE);
        break;
    case V8ClassIndex::PLUGINARRAY:
        setCollectionIndexedAndNamedGetters<PluginArray, Plugin>(descriptor, V8ClassIndex::PLUGIN);
        break;
    case V8ClassIndex::STYLESHEETLIST:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(StyleSheetList));
        setCollectionIndexedGetter<StyleSheetList, StyleSheet>(descriptor, V8ClassIndex::STYLESHEET);
        break;
    case V8ClassIndex::DOMWINDOW: {
        v8::Local<v8::Signature> defaultSignature = v8::Signature::New(descriptor);

        descriptor->PrototypeTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(DOMWindow));
        descriptor->PrototypeTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(DOMWindow));
        descriptor->PrototypeTemplate()->SetInternalFieldCount(V8Custom::kDOMWindowInternalFieldCount);

        descriptor->SetHiddenPrototype(true);

        // Reserve spaces for references to location, history and
        // navigator objects.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kDOMWindowInternalFieldCount);

        // Set access check callbacks, but turned off initially.
        // When a context is detached from a frame, turn on the access check.
        // Turning on checks also invalidates inline caches of the object.
        instanceTemplate->SetAccessCheckCallbacks(V8Custom::v8DOMWindowNamedSecurityCheck, V8Custom::v8DOMWindowIndexedSecurityCheck, v8::Integer::New(V8ClassIndex::DOMWINDOW), false);
        break;
    }
    case V8ClassIndex::LOCATION: {
        // For security reasons, these functions are on the instance
        // instead of on the prototype object to insure that they cannot
        // be overwritten.
        v8::Local<v8::ObjectTemplate> instance = descriptor->InstanceTemplate();
        instance->SetAccessor(v8::String::New("reload"), V8Custom::v8LocationReloadAccessorGetter, 0, v8::Handle<v8::Value>(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly));
        instance->SetAccessor(v8::String::New("replace"), V8Custom::v8LocationReplaceAccessorGetter, 0, v8::Handle<v8::Value>(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly));
        instance->SetAccessor(v8::String::New("assign"), V8Custom::v8LocationAssignAccessorGetter, 0, v8::Handle<v8::Value>(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly));
        break;
    }
    case V8ClassIndex::HISTORY:
        break;

    case V8ClassIndex::MESSAGECHANNEL: {
        // Reserve two more internal fields for referencing the port1
        // and port2 wrappers. This ensures that the port wrappers are
        // kept alive when the channel wrapper is.
        descriptor->SetCallHandler(USE_CALLBACK(MessageChannelConstructor));
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kMessageChannelInternalFieldCount);
        break;
    }

    case V8ClassIndex::MESSAGEPORT: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kMessagePortInternalFieldCount);
        break;
    }

#if ENABLE(NOTIFICATIONS)
    case V8ClassIndex::NOTIFICATION: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kNotificationInternalFieldCount);
        break;
    }
#endif // NOTIFICATIONS

#if ENABLE(SVG)
    case V8ClassIndex::SVGELEMENTINSTANCE: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kSVGElementInstanceInternalFieldCount);
        break;
    }
#endif

#if ENABLE(WORKERS)
    case V8ClassIndex::ABSTRACTWORKER: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kAbstractWorkerInternalFieldCount);
        break;
    }

    case V8ClassIndex::DEDICATEDWORKERCONTEXT: {
        // Reserve internal fields for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        ASSERT(instanceTemplate->InternalFieldCount() == V8Custom::kDefaultWrapperInternalFieldCount);
        instanceTemplate->SetInternalFieldCount(V8Custom::kDedicatedWorkerContextInternalFieldCount);
        break;
    }

    case V8ClassIndex::WORKER: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kWorkerInternalFieldCount);
        descriptor->SetCallHandler(USE_CALLBACK(WorkerConstructor));
        break;
    }

    case V8ClassIndex::WORKERCONTEXT: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        ASSERT(instanceTemplate->InternalFieldCount() == V8Custom::kDefaultWrapperInternalFieldCount);
        instanceTemplate->SetInternalFieldCount(V8Custom::kWorkerContextMinimumInternalFieldCount);
        break;
    }

#endif // WORKERS

#if ENABLE(SHARED_WORKERS)
    case V8ClassIndex::SHAREDWORKER: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kSharedWorkerInternalFieldCount);
        descriptor->SetCallHandler(USE_CALLBACK(SharedWorkerConstructor));
        break;
    }

    case V8ClassIndex::SHAREDWORKERCONTEXT: {
        // Reserve internal fields for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        ASSERT(instanceTemplate->InternalFieldCount() == V8Custom::kDefaultWrapperInternalFieldCount);
        instanceTemplate->SetInternalFieldCount(V8Custom::kSharedWorkerContextInternalFieldCount);
        break;
    }
#endif // SHARED_WORKERS

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    case V8ClassIndex::DOMAPPLICATIONCACHE: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kDOMApplicationCacheFieldCount);
        break;
    }
#endif

#if ENABLE(3D_CANVAS)
    // The following objects are created from JavaScript.
    case V8ClassIndex::CANVASARRAYBUFFER:
        descriptor->SetCallHandler(USE_CALLBACK(CanvasArrayBufferConstructor));
        break;
    case V8ClassIndex::CANVASBYTEARRAY:
        descriptor->SetCallHandler(USE_CALLBACK(CanvasByteArrayConstructor));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(CanvasByteArray), USE_INDEXED_PROPERTY_SETTER(CanvasByteArray));
        break;
    case V8ClassIndex::CANVASFLOATARRAY:
        descriptor->SetCallHandler(USE_CALLBACK(CanvasFloatArrayConstructor));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(CanvasFloatArray), USE_INDEXED_PROPERTY_SETTER(CanvasFloatArray));
        break;
    case V8ClassIndex::CANVASINTARRAY:
        descriptor->SetCallHandler(USE_CALLBACK(CanvasIntArrayConstructor));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(CanvasIntArray), USE_INDEXED_PROPERTY_SETTER(CanvasIntArray));
        break;
    case V8ClassIndex::CANVASSHORTARRAY:
        descriptor->SetCallHandler(USE_CALLBACK(CanvasShortArrayConstructor));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(CanvasShortArray), USE_INDEXED_PROPERTY_SETTER(CanvasShortArray));
        break;
    case V8ClassIndex::CANVASUNSIGNEDBYTEARRAY:
        descriptor->SetCallHandler(USE_CALLBACK(CanvasUnsignedByteArrayConstructor));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(CanvasUnsignedByteArray), USE_INDEXED_PROPERTY_SETTER(CanvasUnsignedByteArray));
        break;
    case V8ClassIndex::CANVASUNSIGNEDINTARRAY:
        descriptor->SetCallHandler(USE_CALLBACK(CanvasUnsignedIntArrayConstructor));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(CanvasUnsignedIntArray), USE_INDEXED_PROPERTY_SETTER(CanvasUnsignedIntArray));
        break;
    case V8ClassIndex::CANVASUNSIGNEDSHORTARRAY:
        descriptor->SetCallHandler(USE_CALLBACK(CanvasUnsignedShortArrayConstructor));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(CanvasUnsignedShortArray), USE_INDEXED_PROPERTY_SETTER(CanvasUnsignedShortArray));
        break;
#endif
    case V8ClassIndex::DOMPARSER:
        descriptor->SetCallHandler(USE_CALLBACK(DOMParserConstructor));
        break;
    case V8ClassIndex::WEBKITCSSMATRIX:
        descriptor->SetCallHandler(USE_CALLBACK(WebKitCSSMatrixConstructor));
        break;
    case V8ClassIndex::WEBKITPOINT:
        descriptor->SetCallHandler(USE_CALLBACK(WebKitPointConstructor));
        break;
#if ENABLE(WEB_SOCKETS)
    case V8ClassIndex::WEBSOCKET: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kWebSocketInternalFieldCount);
        descriptor->SetCallHandler(USE_CALLBACK(WebSocketConstructor));
        break;
    }
#endif
    case V8ClassIndex::XMLSERIALIZER:
        descriptor->SetCallHandler(USE_CALLBACK(XMLSerializerConstructor));
        break;
    case V8ClassIndex::XMLHTTPREQUEST: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kXMLHttpRequestInternalFieldCount);
        descriptor->SetCallHandler(USE_CALLBACK(XMLHttpRequestConstructor));
        break;
    }
    case V8ClassIndex::XMLHTTPREQUESTUPLOAD: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kXMLHttpRequestInternalFieldCount);
        break;
    }
    case V8ClassIndex::XPATHEVALUATOR:
        descriptor->SetCallHandler(USE_CALLBACK(XPathEvaluatorConstructor));
        break;
    case V8ClassIndex::XSLTPROCESSOR:
        descriptor->SetCallHandler(USE_CALLBACK(XSLTProcessorConstructor));
        break;
    case V8ClassIndex::CLIENTRECTLIST:
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(ClientRectList));
        break;
    case V8ClassIndex::FILELIST:
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(FileList));
        break;
#if ENABLE(DATAGRID)
    case V8ClassIndex::DATAGRIDCOLUMNLIST:
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(DataGridColumnList));
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(DataGridColumnList));
        break;
#endif
    default:
        break;
    }

    *cacheCell = descriptor;
    return descriptor;
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
    v8::Handle<v8::FunctionTemplate> functionTemplate = getTemplate(type);
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

    return getConstructor(type, V8Proxy::getHiddenObjectPrototype(context));
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

v8::Handle<v8::Value> V8DOMWrapper::convertToV8Object(V8ClassIndex::V8WrapperType type, void* impl)
{
    ASSERT(type != V8ClassIndex::EVENTLISTENER);
    ASSERT(type != V8ClassIndex::EVENTTARGET);
    ASSERT(type != V8ClassIndex::EVENT);

    // These objects can be constructed under WorkerContextExecutionProxy.  They need special
    // handling, since if we proceed below V8Proxy::retrieve() will get called and will crash.
    // TODO(ukai): websocket?
    if ((type == V8ClassIndex::DOMCOREEXCEPTION
         || type == V8ClassIndex::RANGEEXCEPTION
         || type == V8ClassIndex::EVENTEXCEPTION
         || type == V8ClassIndex::XMLHTTPREQUESTEXCEPTION
         || type == V8ClassIndex::MESSAGEPORT)
        && WorkerContextExecutionProxy::retrieve()) {
        return WorkerContextExecutionProxy::convertToV8Object(type, impl);
    }

    bool isActiveDomObject = false;
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
        DOM_NODE_TYPES(MAKE_CASE)
#if ENABLE(SVG)
        SVG_NODE_TYPES(MAKE_CASE)
#endif
        return convertNodeToV8Object(static_cast<Node*>(impl));
    case V8ClassIndex::CSSVALUE:
        return convertCSSValueToV8Object(static_cast<CSSValue*>(impl));
    case V8ClassIndex::CSSRULE:
        return convertCSSRuleToV8Object(static_cast<CSSRule*>(impl));
    case V8ClassIndex::STYLESHEET:
        return convertStyleSheetToV8Object(static_cast<StyleSheet*>(impl));
    case V8ClassIndex::DOMWINDOW:
        return convertWindowToV8Object(static_cast<DOMWindow*>(impl));
#if ENABLE(SVG)
        SVG_NONNODE_TYPES(MAKE_CASE)
        if (type == V8ClassIndex::SVGELEMENTINSTANCE)
            return convertSVGElementInstanceToV8Object(static_cast<SVGElementInstance*>(impl));
        return convertSVGObjectWithContextToV8Object(type, impl);
#endif

        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
        isActiveDomObject = true;
        break;
    default:
        break;
  }

#undef MAKE_CASE

    if (!impl)
        return v8::Null();

    // Non DOM node
    v8::Persistent<v8::Object> result = isActiveDomObject ? getActiveDOMObjectMap().get(impl) : getDOMObjectMap().get(impl);
    if (result.IsEmpty()) {
        v8::Local<v8::Object> v8Object = instantiateV8Object(type, type, impl);
        if (!v8Object.IsEmpty()) {
            // Go through big switch statement, it has some duplications
            // that were handled by code above (such as CSSVALUE, CSSRULE, etc).
            switch (type) {
#define MAKE_CASE(TYPE, NAME) \
            case V8ClassIndex::TYPE: static_cast<NAME*>(impl)->ref(); break;
                DOM_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
            default:
                ASSERT_NOT_REACHED();
            }
            result = v8::Persistent<v8::Object>::New(v8Object);
            if (isActiveDomObject)
                setJSWrapperForActiveDOMObject(impl, result);
            else
                setJSWrapperForDOMObject(impl, result);

            if (type == V8ClassIndex::CANVASPIXELARRAY) {
                CanvasPixelArray* pixels = reinterpret_cast<CanvasPixelArray*>(impl);
                result->SetIndexedPropertiesToPixelData(pixels->data()->data(), pixels->length());
            }

            // Special case for non-node objects associated with a
            // DOMWindow. Both Safari and FF let the JS wrappers for these
            // objects survive GC. To mimic their behavior, V8 creates
            // hidden references from the DOMWindow to these wrapper
            // objects. These references get cleared when the DOMWindow is
            // reused by a new page.
            switch (type) {
            case V8ClassIndex::CONSOLE:
                setHiddenWindowReference(static_cast<Console*>(impl)->frame(), V8Custom::kDOMWindowConsoleIndex, result);
                break;
            case V8ClassIndex::HISTORY:
                setHiddenWindowReference(static_cast<History*>(impl)->frame(), V8Custom::kDOMWindowHistoryIndex, result);
                break;
            case V8ClassIndex::NAVIGATOR:
                setHiddenWindowReference(static_cast<Navigator*>(impl)->frame(), V8Custom::kDOMWindowNavigatorIndex, result);
                break;
            case V8ClassIndex::SCREEN:
                setHiddenWindowReference(static_cast<Screen*>(impl)->frame(), V8Custom::kDOMWindowScreenIndex, result);
                break;
            case V8ClassIndex::LOCATION:
                setHiddenWindowReference(static_cast<Location*>(impl)->frame(), V8Custom::kDOMWindowLocationIndex, result);
                break;
            case V8ClassIndex::DOMSELECTION:
                setHiddenWindowReference(static_cast<DOMSelection*>(impl)->frame(), V8Custom::kDOMWindowDOMSelectionIndex, result);
                break;
            case V8ClassIndex::BARINFO: {
                BarInfo* barInfo = static_cast<BarInfo*>(impl);
                Frame* frame = barInfo->frame();
                switch (barInfo->type()) {
                case BarInfo::Locationbar:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowLocationbarIndex, result);
                    break;
                case BarInfo::Menubar:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowMenubarIndex, result);
                    break;
                case BarInfo::Personalbar:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowPersonalbarIndex, result);
                    break;
                case BarInfo::Scrollbars:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowScrollbarsIndex, result);
                    break;
                case BarInfo::Statusbar:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowStatusbarIndex, result);
                    break;
                case BarInfo::Toolbar:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowToolbarIndex, result);
                    break;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    return result;
}

void V8DOMWrapper::setHiddenWindowReference(Frame* frame, const int internalIndex, v8::Handle<v8::Object> jsObject)
{
    // Get DOMWindow
    if (!frame)
        return; // Object might be detached from window
    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return;

    ASSERT(internalIndex < V8Custom::kDOMWindowInternalFieldCount);

    v8::Handle<v8::Object> global = context->Global();
    // Look for real DOM wrapper.
    global = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, global);
    ASSERT(!global.IsEmpty());
    ASSERT(global->GetInternalField(internalIndex)->IsUndefined());
    global->SetInternalField(internalIndex, jsObject);
}

V8ClassIndex::V8WrapperType V8DOMWrapper::domWrapperType(v8::Handle<v8::Object> object)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
    v8::Handle<v8::Value> type = object->GetInternalField(V8Custom::kDOMWrapperTypeIndex);
    return V8ClassIndex::FromInt(type->Int32Value());
}

void* V8DOMWrapper::convertToSVGPODTypeImpl(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> object)
{
    return isWrapperOfType(object, type) ? convertDOMWrapperToNative<void>(v8::Handle<v8::Object>::Cast(object)) : 0;
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

v8::Local<v8::Object> V8DOMWrapper::instantiateV8Object(V8Proxy* proxy, V8ClassIndex::V8WrapperType descriptorType, V8ClassIndex::V8WrapperType cptrType, void* impl)
{
    // Make a special case for document.all
    if (descriptorType == V8ClassIndex::HTMLCOLLECTION && static_cast<HTMLCollection*>(impl)->type() == DocAll)
        descriptorType = V8ClassIndex::HTMLALLCOLLECTION;

    if (V8IsolatedWorld::getEntered()) {
        // This effectively disables the wrapper cache for isolated worlds.
        proxy = 0;
        // FIXME: Do we need a wrapper cache for the isolated world?  We should
        // see if the performance gains are worth while.
    } else if (!proxy)
        proxy = V8Proxy::retrieve();

    v8::Local<v8::Object> instance;
    if (proxy)
        instance = proxy->createWrapperFromCache(descriptorType);
    else {
        v8::Local<v8::Function> function = getTemplate(descriptorType)->GetFunction();
        instance = SafeAllocation::newInstance(function);
    }
    if (!instance.IsEmpty()) {
        // Avoid setting the DOM wrapper for failed allocations.
        setDOMWrapper(instance, V8ClassIndex::ToInt(cptrType), impl);
    }
    return instance;
}

void V8DOMWrapper::setDOMWrapper(v8::Handle<v8::Object> object, int type, void* cptr)
{
    ASSERT(object->InternalFieldCount() >= 2);
    object->SetPointerInInternalField(V8Custom::kDOMWrapperObjectIndex, cptr);
    object->SetInternalField(V8Custom::kDOMWrapperTypeIndex, v8::Integer::New(type));
}

#ifndef NDEBUG
bool V8DOMWrapper::maybeDOMWrapper(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    if (!object->InternalFieldCount())
        return false;

    ASSERT(object->InternalFieldCount() >= V8Custom::kDefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> type = object->GetInternalField(V8Custom::kDOMWrapperTypeIndex);
    ASSERT(type->IsInt32());
    ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() && type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(V8Custom::kDOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    return true;
}
#endif

bool V8DOMWrapper::isDOMEventWrapper(v8::Handle<v8::Value> value)
{
    // All kinds of events use EVENT as dom type in JS wrappers.
    // See EventToV8Object
    return isWrapperOfType(value, V8ClassIndex::EVENT);
}

bool V8DOMWrapper::isWrapperOfType(v8::Handle<v8::Value> value, V8ClassIndex::V8WrapperType classType)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    if (!object->InternalFieldCount())
        return false;

    ASSERT(object->InternalFieldCount() >= V8Custom::kDefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(V8Custom::kDOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    v8::Handle<v8::Value> type = object->GetInternalField(V8Custom::kDOMWrapperTypeIndex);
    ASSERT(type->IsInt32());
    ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() && type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

    return V8ClassIndex::FromInt(type->Int32Value()) == classType;
}

#if ENABLE(VIDEO)
#define FOR_EACH_VIDEO_TAG(macro)                  \
    macro(audio, AUDIO)                            \
    macro(source, SOURCE)                          \
    macro(video, VIDEO)
#else
#define FOR_EACH_VIDEO_TAG(macro)
#endif

#if ENABLE(DATAGRID)
#define FOR_EACH_DATAGRID_TAG(macro)               \
    macro(datagrid, DATAGRID)                        \
    macro(dcell, DATAGRIDCELL)                       \
    macro(dcol, DATAGRIDCOL)                         \
    macro(drow, DATAGRIDROW)
#else
#define FOR_EACH_DATAGRID_TAG(macro)
#endif

#define FOR_EACH_TAG(macro)                        \
    FOR_EACH_DATAGRID_TAG(macro)                   \
    macro(a, ANCHOR)                               \
    macro(applet, APPLET)                          \
    macro(area, AREA)                              \
    macro(base, BASE)                              \
    macro(basefont, BASEFONT)                      \
    macro(blockquote, BLOCKQUOTE)                  \
    macro(body, BODY)                              \
    macro(br, BR)                                  \
    macro(button, BUTTON)                          \
    macro(caption, TABLECAPTION)                   \
    macro(col, TABLECOL)                           \
    macro(colgroup, TABLECOL)                      \
    macro(del, MOD)                                \
    macro(canvas, CANVAS)                          \
    macro(dir, DIRECTORY)                          \
    macro(div, DIV)                                \
    macro(dl, DLIST)                               \
    macro(embed, EMBED)                            \
    macro(fieldset, FIELDSET)                      \
    macro(font, FONT)                              \
    macro(form, FORM)                              \
    macro(frame, FRAME)                            \
    macro(frameset, FRAMESET)                      \
    macro(h1, HEADING)                             \
    macro(h2, HEADING)                             \
    macro(h3, HEADING)                             \
    macro(h4, HEADING)                             \
    macro(h5, HEADING)                             \
    macro(h6, HEADING)                             \
    macro(head, HEAD)                              \
    macro(hr, HR)                                  \
    macro(html, HTML)                              \
    macro(img, IMAGE)                              \
    macro(iframe, IFRAME)                          \
    macro(image, IMAGE)                            \
    macro(input, INPUT)                            \
    macro(ins, MOD)                                \
    macro(isindex, ISINDEX)                        \
    macro(keygen, SELECT)                          \
    macro(label, LABEL)                            \
    macro(legend, LEGEND)                          \
    macro(li, LI)                                  \
    macro(link, LINK)                              \
    macro(listing, PRE)                            \
    macro(map, MAP)                                \
    macro(marquee, MARQUEE)                        \
    macro(menu, MENU)                              \
    macro(meta, META)                              \
    macro(object, OBJECT)                          \
    macro(ol, OLIST)                               \
    macro(optgroup, OPTGROUP)                      \
    macro(option, OPTION)                          \
    macro(p, PARAGRAPH)                            \
    macro(param, PARAM)                            \
    macro(pre, PRE)                                \
    macro(q, QUOTE)                                \
    macro(script, SCRIPT)                          \
    macro(select, SELECT)                          \
    macro(style, STYLE)                            \
    macro(table, TABLE)                            \
    macro(thead, TABLESECTION)                     \
    macro(tbody, TABLESECTION)                     \
    macro(tfoot, TABLESECTION)                     \
    macro(td, TABLECELL)                           \
    macro(th, TABLECELL)                           \
    macro(tr, TABLEROW)                            \
    macro(textarea, TEXTAREA)                      \
    macro(title, TITLE)                            \
    macro(ul, ULIST)                               \
    macro(xmp, PRE)

V8ClassIndex::V8WrapperType V8DOMWrapper::htmlElementType(HTMLElement* element)
{
    typedef HashMap<String, V8ClassIndex::V8WrapperType> WrapperTypeMap;
    DEFINE_STATIC_LOCAL(WrapperTypeMap, wrapperTypeMap, ());
    if (wrapperTypeMap.isEmpty()) {
#define ADD_TO_HASH_MAP(tag, name) \
        wrapperTypeMap.set(#tag, V8ClassIndex::HTML##name##ELEMENT);
        FOR_EACH_TAG(ADD_TO_HASH_MAP)
#if ENABLE(VIDEO)
        if (MediaPlayer::isAvailable()) {
            FOR_EACH_VIDEO_TAG(ADD_TO_HASH_MAP)
        }
#endif
#undef ADD_TO_HASH_MAP
    }

    V8ClassIndex::V8WrapperType type = wrapperTypeMap.get(element->localName().impl());
    if (!type)
        return V8ClassIndex::HTMLELEMENT;
    return type;
}
#undef FOR_EACH_TAG

#if ENABLE(SVG)

#if ENABLE(SVG_ANIMATION)
#define FOR_EACH_ANIMATION_TAG(macro) \
    macro(animateColor, ANIMATECOLOR) \
    macro(animate, ANIMATE) \
    macro(animateTransform, ANIMATETRANSFORM) \
    macro(set, SET)
#else
#define FOR_EACH_ANIMATION_TAG(macro)
#endif

#if ENABLE(SVG_FILTERS)
#define FOR_EACH_FILTERS_TAG(macro) \
    macro(feBlend, FEBLEND) \
    macro(feColorMatrix, FECOLORMATRIX) \
    macro(feComponentTransfer, FECOMPONENTTRANSFER) \
    macro(feComposite, FECOMPOSITE) \
    macro(feDiffuseLighting, FEDIFFUSELIGHTING) \
    macro(feDisplacementMap, FEDISPLACEMENTMAP) \
    macro(feDistantLight, FEDISTANTLIGHT) \
    macro(feFlood, FEFLOOD) \
    macro(feFuncA, FEFUNCA) \
    macro(feFuncB, FEFUNCB) \
    macro(feFuncG, FEFUNCG) \
    macro(feFuncR, FEFUNCR) \
    macro(feGaussianBlur, FEGAUSSIANBLUR) \
    macro(feImage, FEIMAGE) \
    macro(feMerge, FEMERGE) \
    macro(feMergeNode, FEMERGENODE) \
    macro(feOffset, FEOFFSET) \
    macro(fePointLight, FEPOINTLIGHT) \
    macro(feSpecularLighting, FESPECULARLIGHTING) \
    macro(feSpotLight, FESPOTLIGHT) \
    macro(feTile, FETILE) \
    macro(feTurbulence, FETURBULENCE) \
    macro(filter, FILTER)
#else
#define FOR_EACH_FILTERS_TAG(macro)
#endif

#if ENABLE(SVG_FONTS)
#define FOR_EACH_FONTS_TAG(macro) \
    macro(font-face, FONTFACE) \
    macro(font-face-format, FONTFACEFORMAT) \
    macro(font-face-name, FONTFACENAME) \
    macro(font-face-src, FONTFACESRC) \
    macro(font-face-uri, FONTFACEURI)
#else
#define FOR_EACH_FONTS_TAG(marco)
#endif

#if ENABLE(SVG_FOREIGN_OBJECT)
#define FOR_EACH_FOREIGN_OBJECT_TAG(macro) \
    macro(foreignObject, FOREIGNOBJECT)
#else
#define FOR_EACH_FOREIGN_OBJECT_TAG(macro)
#endif

#if ENABLE(SVG_USE)
#define FOR_EACH_USE_TAG(macro) \
    macro(use, USE)
#else
#define FOR_EACH_USE_TAG(macro)
#endif

#define FOR_EACH_TAG(macro) \
    FOR_EACH_ANIMATION_TAG(macro) \
    FOR_EACH_FILTERS_TAG(macro) \
    FOR_EACH_FONTS_TAG(macro) \
    FOR_EACH_FOREIGN_OBJECT_TAG(macro) \
    FOR_EACH_USE_TAG(macro) \
    macro(a, A) \
    macro(altGlyph, ALTGLYPH) \
    macro(circle, CIRCLE) \
    macro(clipPath, CLIPPATH) \
    macro(cursor, CURSOR) \
    macro(defs, DEFS) \
    macro(desc, DESC) \
    macro(ellipse, ELLIPSE) \
    macro(g, G) \
    macro(glyph, GLYPH) \
    macro(image, IMAGE) \
    macro(linearGradient, LINEARGRADIENT) \
    macro(line, LINE) \
    macro(marker, MARKER) \
    macro(mask, MASK) \
    macro(metadata, METADATA) \
    macro(path, PATH) \
    macro(pattern, PATTERN) \
    macro(polyline, POLYLINE) \
    macro(polygon, POLYGON) \
    macro(radialGradient, RADIALGRADIENT) \
    macro(rect, RECT) \
    macro(script, SCRIPT) \
    macro(stop, STOP) \
    macro(style, STYLE) \
    macro(svg, SVG) \
    macro(switch, SWITCH) \
    macro(symbol, SYMBOL) \
    macro(text, TEXT) \
    macro(textPath, TEXTPATH) \
    macro(title, TITLE) \
    macro(tref, TREF) \
    macro(tspan, TSPAN) \
    macro(view, VIEW) \
    // end of macro

V8ClassIndex::V8WrapperType V8DOMWrapper::svgElementType(SVGElement* element)
{
    typedef HashMap<String, V8ClassIndex::V8WrapperType> WrapperTypeMap;
    DEFINE_STATIC_LOCAL(WrapperTypeMap, wrapperTypeMap, ());
    if (wrapperTypeMap.isEmpty()) {
#define ADD_TO_HASH_MAP(tag, name) \
        wrapperTypeMap.set(#tag, V8ClassIndex::SVG##name##ELEMENT);
        FOR_EACH_TAG(ADD_TO_HASH_MAP)
#undef ADD_TO_HASH_MAP
    }

    V8ClassIndex::V8WrapperType type = wrapperTypeMap.get(element->localName().impl());
    if (!type)
        return V8ClassIndex::SVGELEMENT;
    return type;
}
#undef FOR_EACH_TAG

#endif // ENABLE(SVG)

v8::Handle<v8::Value> V8DOMWrapper::convertEventToV8Object(Event* event)
{
    if (!event)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(event);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type = V8ClassIndex::EVENT;

    if (event->isUIEvent()) {
        if (event->isKeyboardEvent())
            type = V8ClassIndex::KEYBOARDEVENT;
        else if (event->isTextEvent())
            type = V8ClassIndex::TEXTEVENT;
        else if (event->isMouseEvent())
            type = V8ClassIndex::MOUSEEVENT;
        else if (event->isWheelEvent())
            type = V8ClassIndex::WHEELEVENT;
#if ENABLE(SVG)
        else if (event->isSVGZoomEvent())
            type = V8ClassIndex::SVGZOOMEVENT;
#endif
        else
            type = V8ClassIndex::UIEVENT;
    } else if (event->isMutationEvent())
        type = V8ClassIndex::MUTATIONEVENT;
    else if (event->isOverflowEvent())
        type = V8ClassIndex::OVERFLOWEVENT;
    else if (event->isMessageEvent())
        type = V8ClassIndex::MESSAGEEVENT;
    else if (event->isPageTransitionEvent())
        type = V8ClassIndex::PAGETRANSITIONEVENT;
    else if (event->isProgressEvent()) {
        if (event->isXMLHttpRequestProgressEvent())
            type = V8ClassIndex::XMLHTTPREQUESTPROGRESSEVENT;
        else
            type = V8ClassIndex::PROGRESSEVENT;
    } else if (event->isWebKitAnimationEvent())
        type = V8ClassIndex::WEBKITANIMATIONEVENT;
    else if (event->isWebKitTransitionEvent())
        type = V8ClassIndex::WEBKITTRANSITIONEVENT;
#if ENABLE(WORKERS)
    else if (event->isErrorEvent())
        type = V8ClassIndex::ERROREVENT;
#endif
#if ENABLE(DOM_STORAGE)
    else if (event->isStorageEvent())
        type = V8ClassIndex::STORAGEEVENT;
#endif
    else if (event->isBeforeLoadEvent())
        type = V8ClassIndex::BEFORELOADEVENT;


    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::EVENT, event);
    if (result.IsEmpty()) {
        // Instantiation failed. Avoid updating the DOM object map and
        // return null which is already handled by callers of this function
        // in case the event is NULL.
        return v8::Null();
    }

    event->ref(); // fast ref
    setJSWrapperForDOMObject(event, v8::Persistent<v8::Object>::New(result));

    return result;
}

static const V8ClassIndex::V8WrapperType mapping[] = {
    V8ClassIndex::INVALID_CLASS_INDEX,    // NONE
    V8ClassIndex::INVALID_CLASS_INDEX,    // ELEMENT_NODE needs special treatment
    V8ClassIndex::ATTR,                   // ATTRIBUTE_NODE
    V8ClassIndex::TEXT,                   // TEXT_NODE
    V8ClassIndex::CDATASECTION,           // CDATA_SECTION_NODE
    V8ClassIndex::ENTITYREFERENCE,        // ENTITY_REFERENCE_NODE
    V8ClassIndex::ENTITY,                 // ENTITY_NODE
    V8ClassIndex::PROCESSINGINSTRUCTION,  // PROCESSING_INSTRUCTION_NODE
    V8ClassIndex::COMMENT,                // COMMENT_NODE
    V8ClassIndex::INVALID_CLASS_INDEX,    // DOCUMENT_NODE needs special treatment
    V8ClassIndex::DOCUMENTTYPE,           // DOCUMENT_TYPE_NODE
    V8ClassIndex::DOCUMENTFRAGMENT,       // DOCUMENT_FRAGMENT_NODE
    V8ClassIndex::NOTATION,               // NOTATION_NODE
    V8ClassIndex::NODE,                   // XPATH_NAMESPACE_NODE
};

v8::Handle<v8::Value> V8DOMWrapper::convertDocumentToV8Object(Document* document)
{
    // Find a proxy for this node.
    //
    // Note that if proxy is found, we might initialize the context which can
    // instantiate a document wrapper.  Therefore, we get the proxy before
    // checking if the node already has a wrapper.
    V8Proxy* proxy = V8Proxy::retrieve(document->frame());
    if (proxy)
        proxy->initContextIfNeeded();

    DOMWrapperMap<Node>& domNodeMap = getDOMNodeMap();
    v8::Handle<v8::Object> wrapper = domNodeMap.get(document);
    if (wrapper.IsEmpty())
        return convertNewNodeToV8Object(document, proxy, domNodeMap);

    return wrapper;
}

// Caller checks node is not null.
v8::Handle<v8::Value> V8DOMWrapper::convertNewNodeToV8Object(Node* node, V8Proxy* proxy, DOMWrapperMap<Node>& domNodeMap)
{
    if (!proxy && node->document())
        proxy = V8Proxy::retrieve(node->document()->frame());

    bool isDocument = false; // document type node has special handling
    V8ClassIndex::V8WrapperType type;

    Node::NodeType nodeType = node->nodeType();
    if (nodeType == Node::ELEMENT_NODE) {
        if (node->isHTMLElement())
            type = htmlElementType(static_cast<HTMLElement*>(node));
#if ENABLE(SVG)
        else if (node->isSVGElement())
            type = svgElementType(static_cast<SVGElement*>(node));
#endif
        else
            type = V8ClassIndex::ELEMENT;
    } else if (nodeType == Node::DOCUMENT_NODE) {
        isDocument = true;
        Document* document = static_cast<Document*>(node);
        if (document->isHTMLDocument())
            type = V8ClassIndex::HTMLDOCUMENT;
#if ENABLE(SVG)
        else if (document->isSVGDocument())
            type = V8ClassIndex::SVGDOCUMENT;
#endif
        else
            type = V8ClassIndex::DOCUMENT;
    } else {
        ASSERT(nodeType < static_cast<int>(sizeof(mapping)/sizeof(mapping[0])));
        type = mapping[nodeType];
        ASSERT(type != V8ClassIndex::INVALID_CLASS_INDEX);
    }

    v8::Handle<v8::Context> context;
    if (proxy)
        context = proxy->context();

    // Enter the node's context and create the wrapper in that context.
    if (!context.IsEmpty())
        context->Enter();

    v8::Local<v8::Object> result = instantiateV8Object(proxy, type, V8ClassIndex::NODE, node);

    // Exit the node's context if it was entered.
    if (!context.IsEmpty())
        context->Exit();

    if (result.IsEmpty()) {
        // If instantiation failed it's important not to add the result
        // to the DOM node map. Instead we return an empty handle, which
        // should already be handled by callers of this function in case
        // the node is NULL.
        return result;
    }

    node->ref();
    domNodeMap.set(node, v8::Persistent<v8::Object>::New(result));

    if (isDocument) {
        if (proxy)
            proxy->updateDocumentWrapper(result);

        if (type == V8ClassIndex::HTMLDOCUMENT) {
            // Create marker object and insert it in two internal fields.
            // This is used to implement temporary shadowing of
            // document.all.
            ASSERT(result->InternalFieldCount() == V8Custom::kHTMLDocumentInternalFieldCount);
            v8::Local<v8::Object> marker = v8::Object::New();
            result->SetInternalField(V8Custom::kHTMLDocumentMarkerIndex, marker);
            result->SetInternalField(V8Custom::kHTMLDocumentShadowIndex, marker);
        }
    }

    return result;
}

// A JS object of type EventTarget is limited to a small number of possible classes.
// Check EventTarget.h for new type conversion methods
v8::Handle<v8::Value> V8DOMWrapper::convertEventTargetToV8Object(EventTarget* target)
{
    if (!target)
        return v8::Null();

#if ENABLE(SVG)
    SVGElementInstance* instance = target->toSVGElementInstance();
    if (instance)
        return convertToV8Object(V8ClassIndex::SVGELEMENTINSTANCE, instance);
#endif

#if ENABLE(WORKERS)
    Worker* worker = target->toWorker();
    if (worker)
        return convertToV8Object(V8ClassIndex::WORKER, worker);
#endif // WORKERS

#if ENABLE(NOTIFICATIONS)
    Notification* notification = target->toNotification();
    if (notification)
        return convertToV8Object(V8ClassIndex::NOTIFICATION, notification);
#endif

#if ENABLE(WEB_SOCKETS)
    WebSocket* webSocket = target->toWebSocket();
    if (webSocket)
        return convertToV8Object(V8ClassIndex::WEBSOCKET, webSocket);
#endif

    Node* node = target->toNode();
    if (node)
        return convertNodeToV8Object(node);

    if (DOMWindow* domWindow = target->toDOMWindow())
        return convertToV8Object(V8ClassIndex::DOMWINDOW, domWindow);

    // XMLHttpRequest is created within its JS counterpart.
    XMLHttpRequest* xmlHttpRequest = target->toXMLHttpRequest();
    if (xmlHttpRequest) {
        v8::Handle<v8::Object> wrapper = getActiveDOMObjectMap().get(xmlHttpRequest);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    // MessagePort is created within its JS counterpart
    MessagePort* port = target->toMessagePort();
    if (port) {
        v8::Handle<v8::Object> wrapper = getActiveDOMObjectMap().get(port);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    XMLHttpRequestUpload* upload = target->toXMLHttpRequestUpload();
    if (upload) {
        v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(upload);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    DOMApplicationCache* domAppCache = target->toDOMApplicationCache();
    if (domAppCache)
        return convertToV8Object(V8ClassIndex::DOMAPPLICATIONCACHE, domAppCache);
#endif

    ASSERT(0);
    return notHandledByInterceptor();
}

v8::Handle<v8::Value> V8DOMWrapper::convertEventListenerToV8Object(ScriptExecutionContext* context, EventListener* listener)
{
    if (!listener)
        return v8::Null();

    // FIXME: can a user take a lazy event listener and set to other places?
    V8AbstractEventListener* v8listener = static_cast<V8AbstractEventListener*>(listener);
    return v8listener->getListenerObject(context);
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(Node* node, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    ScriptExecutionContext* context = node->scriptExecutionContext();
    if (!context)
        return 0;

    V8Proxy* proxy = V8Proxy::retrieve(context);
    // The document might be created using createDocument, which does
    // not have a frame, use the active frame.
    if (!proxy)
        proxy = V8Proxy::retrieve(V8Proxy::retrieveFrameForEnteredContext());

    if (proxy)
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(proxy->listenerGuard(), value, isAttribute);

    return 0;
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(SVGElementInstance* element, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return getEventListener(element->correspondingElement(), value, isAttribute, lookup);
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(AbstractWorker* worker, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (worker->scriptExecutionContext()->isWorkerContext()) {
        WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
        ASSERT(workerContextProxy);
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
    }

    V8Proxy* proxy = V8Proxy::retrieve(worker->scriptExecutionContext());
    if (proxy)
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(proxy->listenerGuard(), value, isAttribute);

    return 0;
}

#if ENABLE(NOTIFICATIONS)
PassRefPtr<EventListener> V8DOMWrapper::getEventListener(Notification* notification, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (notification->scriptExecutionContext()->isWorkerContext()) {
        WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
        ASSERT(workerContextProxy);
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
    }

    V8Proxy* proxy = V8Proxy::retrieve(notification->scriptExecutionContext());
    if (proxy)
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(proxy->listenerGuard(), value, isAttribute);

    return 0;
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

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(EventTarget* eventTarget, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    V8Proxy* proxy = V8Proxy::retrieve(eventTarget->scriptExecutionContext());
    if (proxy)
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(proxy->listenerGuard(), value, isAttribute);

#if ENABLE(WORKERS)
    WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
#endif

    return 0;
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(V8Proxy* proxy, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (proxy)
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(proxy->listenerGuard(), value, isAttribute);

    return 0;
}

v8::Handle<v8::Value> V8DOMWrapper::convertDOMImplementationToV8Object(DOMImplementation* impl)
{
    v8::Handle<v8::Object> result = instantiateV8Object(V8ClassIndex::DOMIMPLEMENTATION, V8ClassIndex::DOMIMPLEMENTATION, impl);
    if (result.IsEmpty()) {
        // If the instantiation failed, we ignore it and return null instead
        // of returning an empty handle.
        return v8::Null();
    }
    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertStyleSheetToV8Object(StyleSheet* sheet)
{
    if (!sheet)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(sheet);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type = V8ClassIndex::STYLESHEET;
    if (sheet->isCSSStyleSheet())
        type = V8ClassIndex::CSSSTYLESHEET;

    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::STYLESHEET, sheet);
    if (!result.IsEmpty()) {
        // Only update the DOM object map if the result is non-empty.
        sheet->ref();
        setJSWrapperForDOMObject(sheet, v8::Persistent<v8::Object>::New(result));
    }

    // Add a hidden reference from stylesheet object to its owner node.
    Node* ownerNode = sheet->ownerNode();
    if (ownerNode) {
        v8::Handle<v8::Object> owner = v8::Handle<v8::Object>::Cast(convertNodeToV8Object(ownerNode));
        result->SetInternalField(V8Custom::kStyleSheetOwnerNodeIndex, owner);
    }

    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertCSSValueToV8Object(CSSValue* value)
{
    if (!value)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(value);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type;

    if (value->isWebKitCSSTransformValue())
        type = V8ClassIndex::WEBKITCSSTRANSFORMVALUE;
    else if (value->isValueList())
        type = V8ClassIndex::CSSVALUELIST;
    else if (value->isPrimitiveValue())
        type = V8ClassIndex::CSSPRIMITIVEVALUE;
#if ENABLE(SVG)
    else if (value->isSVGPaint())
        type = V8ClassIndex::SVGPAINT;
    else if (value->isSVGColor())
        type = V8ClassIndex::SVGCOLOR;
#endif
    else
        type = V8ClassIndex::CSSVALUE;

    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::CSSVALUE, value);
    if (!result.IsEmpty()) {
        // Only update the DOM object map if the result is non-empty.
        value->ref();
        setJSWrapperForDOMObject(value, v8::Persistent<v8::Object>::New(result));
    }

    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertCSSRuleToV8Object(CSSRule* rule)
{
    if (!rule) 
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(rule);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type;

    switch (rule->type()) {
    case CSSRule::STYLE_RULE:
        type = V8ClassIndex::CSSSTYLERULE;
        break;
    case CSSRule::CHARSET_RULE:
        type = V8ClassIndex::CSSCHARSETRULE;
        break;
    case CSSRule::IMPORT_RULE:
        type = V8ClassIndex::CSSIMPORTRULE;
        break;
    case CSSRule::MEDIA_RULE:
        type = V8ClassIndex::CSSMEDIARULE;
        break;
    case CSSRule::FONT_FACE_RULE:
        type = V8ClassIndex::CSSFONTFACERULE;
        break;
    case CSSRule::PAGE_RULE:
        type = V8ClassIndex::CSSPAGERULE;
        break;
    case CSSRule::VARIABLES_RULE:
        type = V8ClassIndex::CSSVARIABLESRULE;
        break;
    case CSSRule::WEBKIT_KEYFRAME_RULE:
        type = V8ClassIndex::WEBKITCSSKEYFRAMERULE;
        break;
    case CSSRule::WEBKIT_KEYFRAMES_RULE:
        type = V8ClassIndex::WEBKITCSSKEYFRAMESRULE;
        break;
    default:  // CSSRule::UNKNOWN_RULE
        type = V8ClassIndex::CSSRULE;
        break;
    }

    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::CSSRULE, rule);
    if (!result.IsEmpty()) {
        // Only update the DOM object map if the result is non-empty.
        rule->ref();
        setJSWrapperForDOMObject(rule, v8::Persistent<v8::Object>::New(result));
    }
    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertWindowToV8Object(DOMWindow* window)
{
    if (!window)
        return v8::Null();
    // Initializes environment of a frame, and return the global object
    // of the frame.
    Frame* frame = window->frame();
    if (!frame)
        return v8::Handle<v8::Object>();

    // Special case: Because of evaluateInNewContext() one DOMWindow can have
    // multiple contexts and multiple global objects associated with it. When
    // code running in one of those contexts accesses the window object, we
    // want to return the global object associated with that context, not
    // necessarily the first global object associated with that DOMWindow.
    v8::Handle<v8::Context> currentContext = v8::Context::GetCurrent();
    v8::Handle<v8::Object> currentGlobal = currentContext->Global();
    v8::Handle<v8::Object> windowWrapper = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, currentGlobal);
    if (!windowWrapper.IsEmpty()) {
        if (convertDOMWrapperToNative<DOMWindow>(windowWrapper) == window)
            return currentGlobal;
    }

    // Otherwise, return the global object associated with this frame.
    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return v8::Handle<v8::Object>();

    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

}  // namespace WebCore
