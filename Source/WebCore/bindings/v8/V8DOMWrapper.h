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
#include "DOMWrapperWorld.h"
#include "Event.h"
#include "Node.h"
#include "V8CustomXPathNSResolver.h"
#include "V8DOMWindowShell.h"
#include "V8Utilities.h"
#include "WrapperTypeInfo.h"
#include <v8.h>
#include <wtf/MainThread.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

    class V8DOMWrapper {
    public:
#ifndef NDEBUG
        // Checks if a v8 value can be a DOM wrapper
        static bool maybeDOMWrapper(v8::Handle<v8::Value>);
#endif

        static v8::Local<v8::Object> createWrapper(v8::Handle<v8::Object> creationContext, WrapperTypeInfo*, void*, v8::Isolate*);

        template<typename T>
        static inline v8::Handle<v8::Object> associateObjectWithWrapper(PassRefPtr<T>, WrapperTypeInfo*, v8::Handle<v8::Object>, v8::Isolate*, WrapperConfiguration::Lifetime);
        static inline void setNativeInfo(v8::Handle<v8::Object>, WrapperTypeInfo*, void*);
        static inline void clearNativeInfo(v8::Handle<v8::Object>, WrapperTypeInfo*);

        static bool isDOMWrapper(v8::Handle<v8::Value>);
        static bool isWrapperOfType(v8::Handle<v8::Value>, WrapperTypeInfo*);

#if ENABLE(CUSTOM_ELEMENTS)
        // Used for V8WrapAsFunction, which is used only by CUSTOM_ELEMENTS
        static v8::Handle<v8::Function> toFunction(v8::Handle<v8::Value>);
        static v8::Handle<v8::Function> toFunction(v8::Handle<v8::Object>, const AtomicString& name, v8::Isolate*);
        static v8::Handle<v8::Object> fromFunction(v8::Handle<v8::Object>);
#endif // ENABLE(CUSTOM_ELEMENTS)
    };

    inline void V8DOMWrapper::setNativeInfo(v8::Handle<v8::Object> wrapper, WrapperTypeInfo* type, void* object)
    {
        ASSERT(wrapper->InternalFieldCount() >= 2);
        ASSERT(object);
        ASSERT(type);
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperObjectIndex, object);
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperTypeIndex, type);
    }

    inline void V8DOMWrapper::clearNativeInfo(v8::Handle<v8::Object> wrapper, WrapperTypeInfo* type)
    {
        ASSERT(wrapper->InternalFieldCount() >= 2);
        ASSERT(type);
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperTypeIndex, type);
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperObjectIndex, 0);
    }

    template<typename T>
    inline v8::Handle<v8::Object> V8DOMWrapper::associateObjectWithWrapper(PassRefPtr<T> object, WrapperTypeInfo* type, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, WrapperConfiguration::Lifetime lifetime)
    {
        setNativeInfo(wrapper, type, object.get());
        ASSERT(maybeDOMWrapper(wrapper));
        WrapperConfiguration configuration = buildWrapperConfiguration(object.get(), lifetime);
        DOMDataStore::setWrapper(object.leakRef(), wrapper, isolate, configuration);
        return wrapper;
    }

}

#endif // V8DOMWrapper_h
