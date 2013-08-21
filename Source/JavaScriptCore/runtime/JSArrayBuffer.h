/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSArrayBuffer_h
#define JSArrayBuffer_h

#include "ArrayBuffer.h"
#include "JSObject.h"

namespace JSC {

class JSArrayBuffer : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;
    
protected:
    JSArrayBuffer(VM&, Structure*, PassRefPtr<ArrayBuffer>);
    void finishCreation(VM&);
    
public:
    JS_EXPORT_PRIVATE static JSArrayBuffer* create(VM&, Structure*, PassRefPtr<ArrayBuffer>);
    
    ArrayBuffer* impl() const { return m_impl; }
    
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);
    
    DECLARE_EXPORT_INFO;
    
protected:
    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
    static bool defineOwnProperty(JSObject*, ExecState*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    static bool deleteProperty(JSCell*, ExecState*, PropertyName);
    
    static void getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);

    static const unsigned StructureFlags = OverridesGetPropertyNames | OverridesGetOwnPropertySlot | Base::StructureFlags;

private:
    ArrayBuffer* m_impl;
};

inline ArrayBuffer* toArrayBuffer(JSValue value)
{
    JSArrayBuffer* wrapper = jsDynamicCast<JSArrayBuffer*>(value);
    if (!wrapper)
        return 0;
    return wrapper->impl();
}

} // namespace JSC

#endif // JSArrayBuffer_h

