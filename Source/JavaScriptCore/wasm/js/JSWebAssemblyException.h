/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "JSObject.h"
#include "WasmTag.h"

namespace JSC {

class JSWebAssemblyException final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr bool needsDestruction = true;

    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyExceptionSpace<mode>();
    }

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ErrorInstanceType, StructureFlags), info());
    }

    static JSWebAssemblyException* create(VM& vm, Structure* structure, const Wasm::Tag& tag, FixedVector<uint64_t>&& payload)
    {
        JSWebAssemblyException* exception = new (NotNull, allocateCell<JSWebAssemblyException>(vm)) JSWebAssemblyException(vm, structure, tag, WTFMove(payload));
        exception->finishCreation(vm);
        return exception;
    }

    DECLARE_VISIT_CHILDREN;

    const Wasm::Tag& tag() const { return m_tag.get(); }
    const FixedVector<uint64_t>& payload() const { return m_payload; }
    JSValue getArg(JSGlobalObject*, unsigned) const;

protected:
    JSWebAssemblyException(VM&, Structure*, const Wasm::Tag&, FixedVector<uint64_t>&&);

    void finishCreation(VM&);

    Ref<const Wasm::Tag> m_tag;
    FixedVector<uint64_t> m_payload;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
