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

#include <JavaScriptCore/Structure.h>
#include <JavaScriptCore/WasmTypeDefinition.h>

#if ENABLE(WEBASSEMBLY)

namespace WTF {

class UniquedStringImpl;

} // namespace WTF

namespace JSC {

// FIXME: It seems like almost all the fields of a Structure are useless to a wasm GC "object" since they can't have dynamic fields
// e.g. PropertyTables, Transitions, SeenProperties, Prototype, etc.
class WebAssemblyGCStructure final : public Structure {
    using Base = Structure;
public:
    friend class Structure;

    static constexpr unsigned inlinedTypeDisplaySize = 6;

    template<typename CellType, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.webAssemblyGCStructureSpace();
    }

    const Wasm::RTT& rtt() const LIFETIME_BOUND { return m_rtt; }
    const Wasm::TypeDefinition& typeDefinition() const LIFETIME_BOUND { return m_type; }

    static WebAssemblyGCStructure* create(VM&, JSGlobalObject*, const TypeInfo&, const ClassInfo*, Ref<const Wasm::TypeDefinition>&&, Ref<const Wasm::RTT>&&);

    static constexpr ptrdiff_t offsetOfRTT() { return OBJECT_OFFSETOF(WebAssemblyGCStructure, m_rtt); }
    static constexpr ptrdiff_t offsetOfInlinedTypeDisplay() { return OBJECT_OFFSETOF(WebAssemblyGCStructure, m_inlinedTypeDisplay); }

private:
    WebAssemblyGCStructure(VM&, JSGlobalObject*, const TypeInfo&, const ClassInfo*, Ref<const Wasm::TypeDefinition>&&, Ref<const Wasm::RTT>&&);
    WebAssemblyGCStructure(VM&, WebAssemblyGCStructure* previous);

    Ref<const Wasm::RTT> m_rtt;
    Ref<const Wasm::TypeDefinition> m_type;
    std::array<RefPtr<const Wasm::RTT>, inlinedTypeDisplaySize> m_inlinedTypeDisplay { };
};

} // namespace JSC

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::WebAssemblyGCStructure)
    static bool isType(const JSC::Structure& from)
    {
        return from.variant() == JSC::Structure::StructureVariant::WebAssemblyGC;
    }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBASSEMBLY)
