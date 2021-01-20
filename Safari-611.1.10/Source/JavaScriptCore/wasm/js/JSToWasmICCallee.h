/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "JSCallee.h"

namespace JSC {

#if ENABLE(WEBASSEMBLY)

class WebAssemblyFunction;

// FIXME: Remove this type. Unwinding should just work by using WebAssemblyFunction instead of JSToWasmICCallee.
// https://bugs.webkit.org/show_bug.cgi?id=204960
class JSToWasmICCallee final : public JSCallee {
public:
    using Base = JSCallee;

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.jsToWasmICCalleeSpace<mode>();
    }

    DECLARE_INFO;

    static JSToWasmICCallee* create(VM&, JSGlobalObject*, WebAssemblyFunction*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    WebAssemblyFunction* function() { return m_function.get(); }

private:
    JSToWasmICCallee(VM& vm, JSGlobalObject* globalObject, Structure* structure)
        : Base(vm, globalObject, structure)
    { }
    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<WebAssemblyFunction> m_function;
};

#endif // ENABLE(WEBASSEMBLY)

} // namespace JSC
