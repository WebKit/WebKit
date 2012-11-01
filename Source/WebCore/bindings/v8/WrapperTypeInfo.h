/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WrapperTypeInfo_h
#define WrapperTypeInfo_h

#include <v8.h>

namespace WebCore {
    
    class ActiveDOMObject;
    class DOMDataStore;

    static const int v8DOMWrapperTypeIndex = 0;
    static const int v8DOMWrapperObjectIndex = 1;
    static const int v8DefaultWrapperInternalFieldCount = 2;

    static const uint16_t v8DOMNodeClassId = 1;
    static const uint16_t v8DOMObjectClassId = 2;

    typedef v8::Persistent<v8::FunctionTemplate> (*GetTemplateFunction)();
    typedef void (*DerefObjectFunction)(void*);
    typedef ActiveDOMObject* (*ToActiveDOMObjectFunction)(v8::Handle<v8::Object>);
    typedef void* (*OpaqueRootForGC)(void*, v8::Persistent<v8::Object>);
    typedef void (*InstallPerContextPrototypePropertiesFunction)(v8::Handle<v8::Object>);

    enum WrapperTypePrototype {
        WrapperTypeObjectPrototype,
        WrapperTypeErrorPrototype
    };

    // This struct provides a way to store a bunch of information that is helpful when unwrapping
    // v8 objects. Each v8 bindings class has exactly one static WrapperTypeInfo member, so
    // comparing pointers is a safe way to determine if types match.
    struct WrapperTypeInfo {

        static WrapperTypeInfo* unwrap(v8::Handle<v8::Value> typeInfoWrapper)
        {
            return reinterpret_cast<WrapperTypeInfo*>(v8::External::Unwrap(typeInfoWrapper));
        }
        
        
        bool equals(const WrapperTypeInfo* that) const
        {
            return this == that;
        }

        bool isSubclass(const WrapperTypeInfo* that) const
        {
            for (const WrapperTypeInfo* current = this; current; current = current->parentClass) {
                if (current == that)
                    return true;
            }

            return false;
        }
        
        v8::Persistent<v8::FunctionTemplate> getTemplate() { return getTemplateFunction(); }
        
        void derefObject(void* object)
        {
            if (derefObjectFunction) 
                derefObjectFunction(object);
        }
        
        void installPerContextPrototypeProperties(v8::Handle<v8::Object> proto)
        {
            if (installPerContextPrototypePropertiesFunction)
                installPerContextPrototypePropertiesFunction(proto);
        }

        ActiveDOMObject* toActiveDOMObject(v8::Handle<v8::Object> object)
        {
            if (!toActiveDOMObjectFunction)
                return 0;
            return toActiveDOMObjectFunction(object);
        }

        void* opaqueRootForGC(void* object, v8::Persistent<v8::Object> wrapper)
        {
            if (!opaqueRootForGCFunction)
                return object;
            return opaqueRootForGCFunction(object, wrapper);
        }

        const GetTemplateFunction getTemplateFunction;
        const DerefObjectFunction derefObjectFunction;
        const ToActiveDOMObjectFunction toActiveDOMObjectFunction;
        const OpaqueRootForGC opaqueRootForGCFunction;
        const InstallPerContextPrototypePropertiesFunction installPerContextPrototypePropertiesFunction;
        const WrapperTypeInfo* parentClass;
        const WrapperTypePrototype wrapperTypePrototype;
    };

    inline void* toNative(v8::Handle<v8::Object> object)
    {
        ASSERT(object->InternalFieldCount() >= v8DOMWrapperObjectIndex);
        return object->GetPointerFromInternalField(v8DOMWrapperObjectIndex);
    }

    inline WrapperTypeInfo* toWrapperTypeInfo(v8::Handle<v8::Object> object)
    {
        ASSERT(object->InternalFieldCount() >= v8DOMWrapperTypeIndex);
        return static_cast<WrapperTypeInfo*>(object->GetPointerFromInternalField(v8DOMWrapperTypeIndex));
    }

}

#endif // WrapperTypeInfo_h
