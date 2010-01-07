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

namespace WebCore {
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

#define DECLARE_CALLBACK(NAME) static v8::Handle<v8::Value> v8##NAME##Callback(const v8::Arguments& args)
#define USE_CALLBACK(NAME) V8Custom::v8##NAME##Callback

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

#if ENABLE(WORKERS)
        DECLARE_CALLBACK(WorkerConstructor);
#endif

#if ENABLE(SHARED_WORKERS)
        DECLARE_CALLBACK(SharedWorkerConstructor);
#endif

#if ENABLE(WEB_SOCKETS)
        DECLARE_CALLBACK(WebSocketConstructor);
#endif

#undef DECLARE_CALLBACK
    };

} // namespace WebCore

#endif // V8CustomBinding_h
