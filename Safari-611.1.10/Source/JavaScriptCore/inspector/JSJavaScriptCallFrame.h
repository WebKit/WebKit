/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "JSObject.h"
#include "JavaScriptCallFrame.h"

namespace Inspector {

class JSJavaScriptCallFrame final : public JSC::JSNonFinalObject {
public:
    using Base = JSC::JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags;
    static constexpr bool needsDestruction = true;

    template<typename CellType, JSC::SubspaceAccess mode>
    static JSC::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        return vm.javaScriptCallFrameSpace<mode>();
    }

    DECLARE_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    static JSJavaScriptCallFrame* create(JSC::VM& vm, JSC::Structure* structure, Ref<JavaScriptCallFrame>&& impl)
    {
        JSJavaScriptCallFrame* instance = new (NotNull, JSC::allocateCell<JSJavaScriptCallFrame>(vm.heap)) JSJavaScriptCallFrame(vm, structure, WTFMove(impl));
        instance->finishCreation(vm);
        return instance;
    }

    static JSC::JSObject* createPrototype(JSC::VM&, JSC::JSGlobalObject*);
    static void destroy(JSC::JSCell*);

    JavaScriptCallFrame& impl() const { return *m_impl; }
    void releaseImpl();

    // Functions.
    JSC::JSValue evaluateWithScopeExtension(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue scopeDescriptions(JSC::JSGlobalObject*);

    // Attributes.
    JSC::JSValue caller(JSC::JSGlobalObject*) const;
    JSC::JSValue sourceID(JSC::JSGlobalObject*) const;
    JSC::JSValue line(JSC::JSGlobalObject*) const;
    JSC::JSValue column(JSC::JSGlobalObject*) const;
    JSC::JSValue functionName(JSC::JSGlobalObject*) const;
    JSC::JSValue scopeChain(JSC::JSGlobalObject*) const;
    JSC::JSValue thisObject(JSC::JSGlobalObject*) const;
    JSC::JSValue type(JSC::JSGlobalObject*) const;
    JSC::JSValue isTailDeleted(JSC::JSGlobalObject*) const;

    // Constants.
    static constexpr unsigned short GLOBAL_SCOPE = 0;
    static constexpr unsigned short WITH_SCOPE = 1;
    static constexpr unsigned short CLOSURE_SCOPE = 2;
    static constexpr unsigned short CATCH_SCOPE = 3;
    static constexpr unsigned short FUNCTION_NAME_SCOPE = 4;
    static constexpr unsigned short GLOBAL_LEXICAL_ENVIRONMENT_SCOPE = 5;
    static constexpr unsigned short NESTED_LEXICAL_SCOPE = 6;

private:
    JSJavaScriptCallFrame(JSC::VM&, JSC::Structure*, Ref<JavaScriptCallFrame>&&);
    ~JSJavaScriptCallFrame();
    void finishCreation(JSC::VM&);

    JavaScriptCallFrame* m_impl;
};

JSC::JSValue toJS(JSC::JSGlobalObject*, JSC::JSGlobalObject*, JavaScriptCallFrame*);

} // namespace Inspector
