/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

#include "JSFunction.h"

namespace JSC {

JSC_DECLARE_HOST_FUNCTION(boundThisNoArgsFunctionCall);
JSC_DECLARE_HOST_FUNCTION(boundFunctionCall);
JSC_DECLARE_HOST_FUNCTION(boundThisNoArgsFunctionConstruct);
JSC_DECLARE_HOST_FUNCTION(boundFunctionConstruct);
JSC_DECLARE_HOST_FUNCTION(isBoundFunction);
JSC_DECLARE_HOST_FUNCTION(hasInstanceBoundFunction);

class JSBoundFunction final : public JSFunction {
public:
    typedef JSFunction Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags & ~ImplementsDefaultHasInstance;
    static_assert(StructureFlags & ImplementsHasInstance, "");

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.boundFunctionSpace<mode>();
    }

    static JSBoundFunction* create(VM&, JSGlobalObject*, JSObject* targetFunction, JSValue boundThis, JSImmutableButterfly* boundArgs, double length, JSString* nameMayBeNull);
    
    static bool customHasInstance(JSObject*, JSGlobalObject*, JSValue);

    JSObject* targetFunction() { return m_targetFunction.get(); }
    JSValue boundThis() { return m_boundThis.get(); }
    JSImmutableButterfly* boundArgs() { return m_boundArgs.get(); } // DO NOT allow this array to be mutated!
    JSArray* boundArgsCopy(JSGlobalObject*);
    JSString* nameMayBeNull() { return m_nameMayBeNull.get(); }
    const String& nameString()
    {
        if (!m_nameMayBeNull)
            return emptyString();
        ASSERT(!m_nameMayBeNull->isRope());
        bool allocationAllowed = false;
        return m_nameMayBeNull->tryGetValue(allocationAllowed);
    }

    double length(VM&) { return m_length; }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        ASSERT(globalObject);
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info()); 
    }
    
    static ptrdiff_t offsetOfTargetFunction() { return OBJECT_OFFSETOF(JSBoundFunction, m_targetFunction); }
    static ptrdiff_t offsetOfBoundThis() { return OBJECT_OFFSETOF(JSBoundFunction, m_boundThis); }
    static ptrdiff_t offsetOfBoundArgs() { return OBJECT_OFFSETOF(JSBoundFunction, m_boundArgs); }

    DECLARE_INFO;

private:
    JSBoundFunction(VM&, NativeExecutable*, JSGlobalObject*, Structure*, JSObject* targetFunction, JSValue boundThis, JSImmutableButterfly* boundArgs, JSString* nameMayBeNull, double length);

    void finishCreation(VM&);
    DECLARE_VISIT_CHILDREN;

    WriteBarrier<JSObject> m_targetFunction;
    WriteBarrier<Unknown> m_boundThis;
    WriteBarrier<JSImmutableButterfly> m_boundArgs;
    WriteBarrier<JSString> m_nameMayBeNull;
    double m_length;
};

JSC_DECLARE_HOST_FUNCTION(boundFunctionCall);
JSC_DECLARE_HOST_FUNCTION(boundFunctionConstruct);
JSC_DECLARE_HOST_FUNCTION(boundThisNoArgsFunctionCall);
JSC_DECLARE_HOST_FUNCTION(boundThisNoArgsFunctionConstruct);

} // namespace JSC
