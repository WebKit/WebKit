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
 
#ifndef V8CustomBinding_h
#define V8CustomBinding_h

#include "V8Index.h"
#include <v8.h>

struct NPObject;

#define INDEXED_PROPERTY_GETTER(NAME) \
    v8::Handle<v8::Value> V8Custom::v8##NAME##IndexedPropertyGetter( \
        uint32_t index, const v8::AccessorInfo& info)

#define INDEXED_PROPERTY_SETTER(NAME) \
    v8::Handle<v8::Value> V8Custom::v8##NAME##IndexedPropertySetter( \
        uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info)

#define INDEXED_PROPERTY_DELETER(NAME) \
    v8::Handle<v8::Boolean> V8Custom::v8##NAME##IndexedPropertyDeleter( \
        uint32_t index, const v8::AccessorInfo& info)

#define NAMED_PROPERTY_GETTER(NAME) \
    v8::Handle<v8::Value> V8Custom::v8##NAME##NamedPropertyGetter( \
        v8::Local<v8::String> name, const v8::AccessorInfo& info)

#define NAMED_PROPERTY_SETTER(NAME) \
    v8::Handle<v8::Value> V8Custom::v8##NAME##NamedPropertySetter( \
        v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)

#define NAMED_PROPERTY_DELETER(NAME) \
    v8::Handle<v8::Boolean> V8Custom::v8##NAME##NamedPropertyDeleter( \
        v8::Local<v8::String> name, const v8::AccessorInfo& info)

#define NAMED_ACCESS_CHECK(NAME) \
    bool V8Custom::v8##NAME##NamedSecurityCheck(v8::Local<v8::Object> host, \
        v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value> data)

#define INDEXED_ACCESS_CHECK(NAME) \
    bool V8Custom::v8##NAME##IndexedSecurityCheck(v8::Local<v8::Object> host, \
        uint32_t index, v8::AccessType type, v8::Local<v8::Value> data)

#define ACCESSOR_RUNTIME_ENABLER(NAME) bool V8Custom::v8##NAME##Enabled()

namespace WebCore {

    class DOMWindow;
    class Element;
    class Frame;
    class HTMLCollection;
    class HTMLFrameElementBase;
    class String;
    class V8Proxy;

    bool allowSettingFrameSrcToJavascriptUrl(HTMLFrameElementBase*, String value);
    bool allowSettingSrcToJavascriptURL(Element*, String name, String value);

    class V8Custom {
    public:
        // Constants.
        static const int kDOMWrapperTypeIndex = 0;
        static const int kDOMWrapperObjectIndex = 1;
        static const int kDefaultWrapperInternalFieldCount = 2;

        static const int kNPObjectInternalFieldCount = kDefaultWrapperInternalFieldCount + 0;

        static const int kNodeEventListenerCacheIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kNodeMinimumInternalFieldCount = kDefaultWrapperInternalFieldCount + 1;

        static const int kDocumentImplementationIndex = kNodeMinimumInternalFieldCount + 0;
        static const int kDocumentMinimumInternalFieldCount = kNodeMinimumInternalFieldCount + 1;

        static const int kHTMLDocumentMarkerIndex = kDocumentMinimumInternalFieldCount + 0;
        static const int kHTMLDocumentShadowIndex = kDocumentMinimumInternalFieldCount + 1;
        static const int kHTMLDocumentInternalFieldCount = kDocumentMinimumInternalFieldCount + 2;

        static const int kXMLHttpRequestCacheIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kXMLHttpRequestInternalFieldCount = kDefaultWrapperInternalFieldCount + 1;

        static const int kMessageChannelPort1Index = kDefaultWrapperInternalFieldCount + 0;
        static const int kMessageChannelPort2Index = kDefaultWrapperInternalFieldCount + 1;
        static const int kMessageChannelInternalFieldCount = kDefaultWrapperInternalFieldCount + 2;

        static const int kMessagePortRequestCacheIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kMessagePortInternalFieldCount = kDefaultWrapperInternalFieldCount + 1;

#if ENABLE(WORKERS)
        static const int kAbstractWorkerRequestCacheIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kAbstractWorkerInternalFieldCount = kDefaultWrapperInternalFieldCount + 1;

        static const int kWorkerRequestCacheIndex = kAbstractWorkerInternalFieldCount + 0;
        static const int kWorkerInternalFieldCount = kAbstractWorkerInternalFieldCount + 1;

        static const int kWorkerContextRequestCacheIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kWorkerContextMinimumInternalFieldCount = kDefaultWrapperInternalFieldCount + 1;

        static const int kDedicatedWorkerContextRequestCacheIndex = kWorkerContextMinimumInternalFieldCount + 0;
        static const int kDedicatedWorkerContextInternalFieldCount = kWorkerContextMinimumInternalFieldCount + 1;
#endif

#if ENABLE(SHARED_WORKERS)
        static const int kSharedWorkerRequestCacheIndex = kAbstractWorkerInternalFieldCount + 0;
        static const int kSharedWorkerInternalFieldCount = kAbstractWorkerInternalFieldCount + 1;

        static const int kSharedWorkerContextRequestCacheIndex = kWorkerContextMinimumInternalFieldCount + 0;
        static const int kSharedWorkerContextInternalFieldCount = kWorkerContextMinimumInternalFieldCount + 1;
#endif

#if ENABLE(NOTIFICATIONS)
        static const int kNotificationRequestCacheIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kNotificationInternalFieldCount = kDefaultWrapperInternalFieldCount + 1;
#endif

#if ENABLE(SVG)
        static const int kSVGElementInstanceEventListenerCacheIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kSVGElementInstanceInternalFieldCount = kDefaultWrapperInternalFieldCount + 1;
#endif

        static const int kDOMWindowConsoleIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kDOMWindowHistoryIndex = kDefaultWrapperInternalFieldCount + 1;
        static const int kDOMWindowLocationbarIndex = kDefaultWrapperInternalFieldCount + 2;
        static const int kDOMWindowMenubarIndex = kDefaultWrapperInternalFieldCount + 3;
        static const int kDOMWindowNavigatorIndex = kDefaultWrapperInternalFieldCount + 4;
        static const int kDOMWindowPersonalbarIndex = kDefaultWrapperInternalFieldCount + 5;
        static const int kDOMWindowScreenIndex = kDefaultWrapperInternalFieldCount + 6;
        static const int kDOMWindowScrollbarsIndex = kDefaultWrapperInternalFieldCount + 7;
        static const int kDOMWindowSelectionIndex = kDefaultWrapperInternalFieldCount + 8;
        static const int kDOMWindowStatusbarIndex = kDefaultWrapperInternalFieldCount + 9;
        static const int kDOMWindowToolbarIndex = kDefaultWrapperInternalFieldCount + 10;
        static const int kDOMWindowLocationIndex = kDefaultWrapperInternalFieldCount + 11;
        static const int kDOMWindowDOMSelectionIndex = kDefaultWrapperInternalFieldCount + 12;
        static const int kDOMWindowEventListenerCacheIndex = kDefaultWrapperInternalFieldCount + 13;
        static const int kDOMWindowEnteredIsolatedWorldIndex = kDefaultWrapperInternalFieldCount + 14;
        static const int kDOMWindowInternalFieldCount = kDefaultWrapperInternalFieldCount + 15;

        static const int kStyleSheetOwnerNodeIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kStyleSheetInternalFieldCount = kDefaultWrapperInternalFieldCount + 1;
        static const int kNamedNodeMapOwnerNodeIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kNamedNodeMapInternalFieldCount = kDefaultWrapperInternalFieldCount + 1;

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        static const int kDOMApplicationCacheCacheIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kDOMApplicationCacheFieldCount = kDefaultWrapperInternalFieldCount + 1;
#endif

#if ENABLE(WEB_SOCKETS)
        static const int kWebSocketCacheIndex = kDefaultWrapperInternalFieldCount + 0;
        static const int kWebSocketInternalFieldCount = kDefaultWrapperInternalFieldCount + 1;
#endif

#define DECLARE_NAMED_PROPERTY_GETTER(NAME)  \
    static v8::Handle<v8::Value> v8##NAME##NamedPropertyGetter( \
        v8::Local<v8::String> name, const v8::AccessorInfo& info)

#define DECLARE_NAMED_PROPERTY_SETTER(NAME) \
    static v8::Handle<v8::Value> v8##NAME##NamedPropertySetter( \
        v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)

#define DECLARE_NAMED_PROPERTY_DELETER(NAME) \
    static v8::Handle<v8::Boolean> v8##NAME##NamedPropertyDeleter( \
        v8::Local<v8::String> name, const v8::AccessorInfo& info)

#define USE_NAMED_PROPERTY_GETTER(NAME) V8Custom::v8##NAME##NamedPropertyGetter

#define USE_NAMED_PROPERTY_SETTER(NAME) V8Custom::v8##NAME##NamedPropertySetter

#define USE_NAMED_PROPERTY_DELETER(NAME) V8Custom::v8##NAME##NamedPropertyDeleter

#define DECLARE_INDEXED_PROPERTY_GETTER(NAME) \
    static v8::Handle<v8::Value> v8##NAME##IndexedPropertyGetter( \
        uint32_t index, const v8::AccessorInfo& info)

#define DECLARE_INDEXED_PROPERTY_SETTER(NAME) \
    static v8::Handle<v8::Value> v8##NAME##IndexedPropertySetter( \
        uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info)

#define DECLARE_INDEXED_PROPERTY_DELETER(NAME) \
    static v8::Handle<v8::Boolean> v8##NAME##IndexedPropertyDeleter( \
        uint32_t index, const v8::AccessorInfo& info)

#define USE_INDEXED_PROPERTY_GETTER(NAME) V8Custom::v8##NAME##IndexedPropertyGetter

#define USE_INDEXED_PROPERTY_SETTER(NAME) V8Custom::v8##NAME##IndexedPropertySetter

#define USE_INDEXED_PROPERTY_DELETER(NAME) V8Custom::v8##NAME##IndexedPropertyDeleter

#define DECLARE_CALLBACK(NAME) static v8::Handle<v8::Value> v8##NAME##Callback(const v8::Arguments& args)

#define USE_CALLBACK(NAME) V8Custom::v8##NAME##Callback

#define DECLARE_NAMED_ACCESS_CHECK(NAME) \
    static bool v8##NAME##NamedSecurityCheck(v8::Local<v8::Object> host, \
        v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value> data)

#define DECLARE_INDEXED_ACCESS_CHECK(NAME) \
    static bool v8##NAME##IndexedSecurityCheck(v8::Local<v8::Object> host, \
        uint32_t index, v8::AccessType type, v8::Local<v8::Value> data)

#define DECLARE_ACCESSOR_RUNTIME_ENABLER(NAME) static bool v8##NAME##Enabled()

#if ENABLE(VIDEO)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowAudio);
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowHTMLMediaElement);
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowHTMLAudioElement);
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowHTMLVideoElement);
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowMediaError);
#endif

        DECLARE_NAMED_ACCESS_CHECK(Location);
        DECLARE_INDEXED_ACCESS_CHECK(History);

        DECLARE_NAMED_ACCESS_CHECK(History);
        DECLARE_INDEXED_ACCESS_CHECK(Location);

        DECLARE_NAMED_PROPERTY_GETTER(HTMLDocument);
        DECLARE_NAMED_PROPERTY_DELETER(HTMLDocument);

        DECLARE_CALLBACK(DOMParserConstructor);
        DECLARE_CALLBACK(HTMLAudioElementConstructor);
        DECLARE_CALLBACK(HTMLImageElementConstructor);
        DECLARE_CALLBACK(HTMLOptionElementConstructor);
        DECLARE_CALLBACK(MessageChannelConstructor);
        DECLARE_CALLBACK(WebKitCSSMatrixConstructor);
        DECLARE_CALLBACK(WebKitPointConstructor);
        DECLARE_CALLBACK(XMLHttpRequestConstructor);
        DECLARE_CALLBACK(XMLSerializerConstructor);
        DECLARE_CALLBACK(XPathEvaluatorConstructor);
        DECLARE_CALLBACK(XSLTProcessorConstructor);

#if ENABLE(3D_CANVAS)
        DECLARE_CALLBACK(WebGLArrayBufferConstructor);
        DECLARE_CALLBACK(WebGLByteArrayConstructor);
        DECLARE_CALLBACK(WebGLFloatArrayConstructor);
        DECLARE_CALLBACK(WebGLIntArrayConstructor);
        DECLARE_CALLBACK(WebGLShortArrayConstructor);
        DECLARE_CALLBACK(WebGLUnsignedByteArrayConstructor);
        DECLARE_CALLBACK(WebGLUnsignedIntArrayConstructor);
        DECLARE_CALLBACK(WebGLUnsignedShortArrayConstructor);
#endif

        DECLARE_NAMED_PROPERTY_GETTER(DOMWindow);
        DECLARE_INDEXED_PROPERTY_GETTER(DOMWindow);
        DECLARE_NAMED_ACCESS_CHECK(DOMWindow);
        DECLARE_INDEXED_ACCESS_CHECK(DOMWindow);

        DECLARE_NAMED_PROPERTY_GETTER(HTMLFrameSetElement);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLFormElement);
        DECLARE_NAMED_PROPERTY_GETTER(NodeList);
        DECLARE_NAMED_PROPERTY_GETTER(NamedNodeMap);
        DECLARE_NAMED_PROPERTY_GETTER(CSSStyleDeclaration);
        DECLARE_NAMED_PROPERTY_SETTER(CSSStyleDeclaration);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLAppletElement);
        DECLARE_NAMED_PROPERTY_SETTER(HTMLAppletElement);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLEmbedElement);
        DECLARE_NAMED_PROPERTY_SETTER(HTMLEmbedElement);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLObjectElement);
        DECLARE_NAMED_PROPERTY_SETTER(HTMLObjectElement);
        DECLARE_INDEXED_PROPERTY_GETTER(HTMLAppletElement);
        DECLARE_INDEXED_PROPERTY_SETTER(HTMLAppletElement);
        DECLARE_INDEXED_PROPERTY_GETTER(HTMLEmbedElement);
        DECLARE_INDEXED_PROPERTY_SETTER(HTMLEmbedElement);
        DECLARE_INDEXED_PROPERTY_GETTER(HTMLObjectElement);
        DECLARE_INDEXED_PROPERTY_SETTER(HTMLObjectElement);

        DECLARE_NAMED_PROPERTY_GETTER(StyleSheetList);
        DECLARE_INDEXED_PROPERTY_GETTER(NamedNodeMap);
        DECLARE_INDEXED_PROPERTY_GETTER(HTMLFormElement);
        DECLARE_INDEXED_PROPERTY_GETTER(HTMLOptionsCollection);
        DECLARE_INDEXED_PROPERTY_SETTER(HTMLOptionsCollection);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLSelectElement);
        DECLARE_INDEXED_PROPERTY_GETTER(HTMLSelectElement);
        DECLARE_INDEXED_PROPERTY_SETTER(HTMLSelectElement);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLAllCollection);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLCollection);

#if ENABLE(3D_CANVAS)
        DECLARE_INDEXED_PROPERTY_GETTER(WebGLByteArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLByteArray);

        DECLARE_INDEXED_PROPERTY_GETTER(WebGLFloatArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLFloatArray);

        DECLARE_INDEXED_PROPERTY_GETTER(WebGLIntArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLIntArray);

        DECLARE_INDEXED_PROPERTY_GETTER(WebGLShortArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLShortArray);

        DECLARE_INDEXED_PROPERTY_GETTER(WebGLUnsignedByteArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLUnsignedByteArray);

        DECLARE_INDEXED_PROPERTY_GETTER(WebGLUnsignedIntArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLUnsignedIntArray);

        DECLARE_INDEXED_PROPERTY_GETTER(WebGLUnsignedShortArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLUnsignedShortArray);
#endif

#if ENABLE(DATAGRID)
        DECLARE_NAMED_PROPERTY_GETTER(DataGridColumnList);
#endif

#if ENABLE(DATABASE)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowOpenDatabase);
#endif

#if ENABLE(DOM_STORAGE)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowLocalStorage);
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowSessionStorage);
        DECLARE_INDEXED_PROPERTY_GETTER(Storage);
        DECLARE_INDEXED_PROPERTY_SETTER(Storage);
        DECLARE_INDEXED_PROPERTY_DELETER(Storage);
        DECLARE_NAMED_PROPERTY_GETTER(Storage);
        DECLARE_NAMED_PROPERTY_SETTER(Storage);
        DECLARE_NAMED_PROPERTY_DELETER(Storage);
        static v8::Handle<v8::Array> v8StorageNamedPropertyEnumerator(const v8::AccessorInfo& info);
#endif

#if ENABLE(WORKERS)
        DECLARE_CALLBACK(WorkerConstructor);

#if ENABLE(NOTIFICATIONS)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(WorkerContextWebkitNotifications);
#endif
#endif // ENABLE(WORKERS)

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowApplicationCache);
#endif

#if ENABLE(SHARED_WORKERS)
        DECLARE_CALLBACK(SharedWorkerConstructor);
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowSharedWorker);
#endif

#if ENABLE(NOTIFICATIONS)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowWebkitNotifications);
#endif

#if ENABLE(WEB_SOCKETS)
        DECLARE_CALLBACK(WebSocketConstructor);
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowWebSocket);
#endif

#undef DECLARE_INDEXED_ACCESS_CHECK
#undef DECLARE_NAMED_ACCESS_CHECK

#undef DECLARE_NAMED_PROPERTY_GETTER
#undef DECLARE_NAMED_PROPERTY_SETTER
#undef DECLARE_NAMED_PROPERTY_DELETER

#undef DECLARE_INDEXED_PROPERTY_GETTER
#undef DECLARE_INDEXED_PROPERTY_SETTER
#undef DECLARE_INDEXED_PROPERTY_DELETER

#undef DECLARE_CALLBACK

        // Returns the NPObject corresponding to an HTMLElement object.
        static NPObject* GetHTMLPlugInElementNPObject(v8::Handle<v8::Object>);

        // Returns the owner frame pointer of a DOM wrapper object. It only works for
        // these DOM objects requiring cross-domain access check.
        static Frame* GetTargetFrame(v8::Local<v8::Object> host, v8::Local<v8::Value> data);

        // Special case for downcasting SVG path segments.
#if ENABLE(SVG)
        static V8ClassIndex::V8WrapperType DowncastSVGPathSeg(void* pathSeg);
#endif
        static void WindowSetLocation(DOMWindow*, const String&);
    };

} // namespace WebCore

#endif // V8CustomBinding_h
