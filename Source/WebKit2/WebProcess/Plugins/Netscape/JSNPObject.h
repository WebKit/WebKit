/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSNPObject_h
#define JSNPObject_h

#include <JavaScriptCore/JSObjectWithGlobalObject.h>

typedef void* NPIdentifier;
struct NPObject;

namespace WebKit {

class NPRuntimeObjectMap;
    
// JSNPObject is a JSObject that wraps an NPObject.

class JSNPObject : public JSC::JSObjectWithGlobalObject {
public:
    JSNPObject(JSC::JSGlobalObject*, NPRuntimeObjectMap* objectMap, NPObject* npObject);
    ~JSNPObject();

    void invalidate();

    JSC::JSValue callMethod(JSC::ExecState*, NPIdentifier methodName);
    JSC::JSValue callObject(JSC::ExecState*);
    JSC::JSValue callConstructor(JSC::ExecState*);

    static const JSC::ClassInfo s_info;

    NPObject* npObject() const { return m_npObject; }

private:
    static const unsigned StructureFlags = JSC::OverridesGetOwnPropertySlot | JSC::OverridesGetPropertyNames | JSObject::StructureFlags;
    
    static JSC::Structure* createStructure(JSC::JSGlobalData& globalData, JSC::JSValue prototype)
    {
        return JSC::Structure::create(globalData, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), AnonymousSlotCount, &s_info);
    }

    virtual JSC::CallType getCallData(JSC::CallData&);
    virtual JSC::ConstructType getConstructData(JSC::ConstructData&);

    virtual bool getOwnPropertySlot(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::PropertySlot&);
    virtual bool getOwnPropertyDescriptor(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::PropertyDescriptor&);
    virtual void put(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::JSValue, JSC::PutPropertySlot&);

    virtual void getOwnPropertyNames(JSC::ExecState*, JSC::PropertyNameArray&, JSC::EnumerationMode mode = JSC::ExcludeDontEnumProperties);

    static JSC::JSValue propertyGetter(JSC::ExecState*, JSC::JSValue, const JSC::Identifier&);
    static JSC::JSValue methodGetter(JSC::ExecState*, JSC::JSValue, const JSC::Identifier&);
    static JSC::JSObject* throwInvalidAccessError(JSC::ExecState*);

    NPRuntimeObjectMap* m_objectMap;
    NPObject* m_npObject;
};

} // namespace WebKit

#endif // JSNPObject_h
