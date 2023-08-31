/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

namespace Inspector {

class InjectedScriptHost;

class JSInjectedScriptHost final : public JSC::JSNonFinalObject {
public:
    using Base = JSC::JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags;
    static constexpr bool needsDestruction = true;

    template<typename CellType, JSC::SubspaceAccess mode>
    static JSC::GCClient::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        return vm.injectedScriptHostSpace<mode>();
    }

    DECLARE_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    static JSInjectedScriptHost* create(JSC::VM& vm, JSC::Structure* structure, Ref<InjectedScriptHost>&& impl)
    {
        JSInjectedScriptHost* instance = new (NotNull, JSC::allocateCell<JSInjectedScriptHost>(vm)) JSInjectedScriptHost(vm, structure, WTFMove(impl));
        instance->finishCreation(vm);
        return instance;
    }

    static JSC::JSObject* createPrototype(JSC::VM&, JSC::JSGlobalObject*);
    static void destroy(JSC::JSCell*);

    InjectedScriptHost& impl() const { return m_wrapped; }

    // Attributes.
    JSC::JSValue evaluate(JSC::JSGlobalObject*) const;
    JSC::JSValue savedResultAlias(JSC::JSGlobalObject*) const;

    // Functions.
    JSC::JSValue evaluateWithScopeExtension(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue internalConstructorName(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue isHTMLAllCollection(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue isPromiseRejectedWithNativeGetterTypeError(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue subtype(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue functionDetails(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue getOwnPrivatePropertySymbols(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue getInternalProperties(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue proxyTargetValue(JSC::CallFrame*);
    JSC::JSValue weakRefTargetValue(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue weakMapSize(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue weakMapEntries(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue weakSetSize(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue weakSetEntries(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue iteratorEntries(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue queryInstances(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSValue queryHolders(JSC::JSGlobalObject*, JSC::CallFrame*);

private:
    JSInjectedScriptHost(JSC::VM&, JSC::Structure*, Ref<InjectedScriptHost>&&);
    DECLARE_DEFAULT_FINISH_CREATION;

    Ref<InjectedScriptHost> m_wrapped;
};

} // namespace Inspector
