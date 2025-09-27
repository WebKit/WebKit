/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "JSFunction.h"

namespace JSC {

// A function object created by 'new WebAssembly.Suspending(f)'.
class JSWebAssemblySuspendingFunction final : public JSFunction {
public:
    using Base = JSFunction;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.suspendingFunctionSpace<mode>();
    }

    DECLARE_INFO;
    DECLARE_VISIT_CHILDREN;

    static JSWebAssemblySuspendingFunction* create(VM&, JSGlobalObject*, JSFunction*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    // The function passed to the 'new WebAssembly.Suspending()' call that created this.
    JSFunction* wrappedFunction() const { return m_wrappedFunction.get(); }

private:
    JSWebAssemblySuspendingFunction(VM& vm, NativeExecutable* native, JSGlobalObject* globalObject, Structure* structure, JSFunction* wrapped)
        : Base(vm, native, globalObject, structure)
        , m_wrappedFunction(wrapped, WriteBarrierEarlyInit)
    { }

    void finishCreation(VM&, NativeExecutable*, const String&);

    WriteBarrier<JSFunction> m_wrappedFunction;
};

}

#endif // ENABLE(WEBASSEMBLY)
