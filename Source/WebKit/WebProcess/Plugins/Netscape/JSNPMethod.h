/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#ifndef JSNPMethod_h
#define JSNPMethod_h

#if ENABLE(NETSCAPE_PLUGIN_API)

#include <JavaScriptCore/FunctionPrototype.h>
#include <JavaScriptCore/InternalFunction.h>
#include <JavaScriptCore/JSGlobalObject.h>

typedef void* NPIdentifier;

namespace WebKit {

// A JSObject that wraps an NPMethod.
class JSNPMethod final : public JSC::InternalFunction {
public:
    using Base = JSC::InternalFunction;

    template<typename CellType, JSC::SubspaceAccess>
    static JSC::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        return subspaceForImpl(vm);
    }

    static JSNPMethod* create(JSC::JSGlobalObject* globalObject, const String& name, NPIdentifier npIdent)
    {
        JSC::VM& vm = globalObject->vm();
        JSC::Structure* structure = createStructure(globalObject->vm(), globalObject, globalObject->functionPrototype());
        JSNPMethod* method = new (JSC::allocateCell<JSNPMethod>(vm.heap)) JSNPMethod(globalObject, structure, npIdent);
        method->finishCreation(vm, name);
        return method;
    }

    DECLARE_INFO;

    NPIdentifier npIdentifier() const { return m_npIdentifier; }

protected:
    void finishCreation(JSC::VM&, const String& name);

private:
    static JSC::IsoSubspace* subspaceForImpl(JSC::VM&);
    
    JSNPMethod(JSC::JSGlobalObject*, JSC::Structure*, NPIdentifier);

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::InternalFunctionType, StructureFlags), info());
    }

    NPIdentifier m_npIdentifier;
};


} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#endif // JSNPMethod_h
