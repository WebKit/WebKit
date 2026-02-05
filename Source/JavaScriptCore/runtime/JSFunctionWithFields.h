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

#pragma once

#include <JavaScriptCore/JSFunction.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class JSFunctionWithFields final : public JSFunction {
    friend class JIT;
    friend class VM;
public:
    using Base = JSFunction;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static constexpr unsigned numberOfInternalFields = 2;
    enum class Field : unsigned {
        ExecutorResolve = 0,
        ExecutorReject = 1,

        ResolvingPromise = 0,
        ResolvingOther = 1,

        FirstResolvingPromise = 0,

        ResolvingWithInternalMicrotaskContext = 0,
        ResolvingWithInternalMicrotaskOther = 1,

        PromiseAllContext = 0,
        PromiseAllResolve = 1,

        PromiseAllSettledContext = 0,
        PromiseAllSettledOther = 1,

        PromiseAnyContext = 0,
        PromiseAnyReject = 1,
    };

    DECLARE_INFO;

    DECLARE_VISIT_CHILDREN;

    static JSFunctionWithFields* create(VM&, JSGlobalObject*, NativeExecutable*, unsigned length, const String& name);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.functionWithFieldsSpace<mode>();
    }

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    JSValue getField(Field index) { return fields()[static_cast<unsigned>(index)].get(); }
    void setField(VM& vm, Field index, JSValue value) { fields()[static_cast<unsigned>(index)].set(vm, this, value); }

private:
    std::span<WriteBarrier<Unknown>, numberOfInternalFields> fields()
    {
        return std::span<WriteBarrier<Unknown>, numberOfInternalFields> { m_internalFields, numberOfInternalFields };
    }

    JSFunctionWithFields(VM&, NativeExecutable*, JSGlobalObject*, Structure*);

    WriteBarrier<Unknown> m_internalFields[numberOfInternalFields] { };
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
