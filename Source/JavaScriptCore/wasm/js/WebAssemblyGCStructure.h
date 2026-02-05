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
#include <wtf/Platform.h>
#include <wtf/ReferenceWrapperVector.h>

#if ENABLE(WEBASSEMBLY)

namespace WTF {

class UniquedStringImpl;

} // namespace WTF

namespace JSC {

// A set of all TypeDefinitions a WebAssemblyGCStructure needs to keep alive.
// The TypeDefinition retained by a structure as `m_type` may reference other
// TypeDefinitions. Such references are stored as raw pointers in Wasm::FieldTypes. To
// prevent these unmanaged pointers from dangling if a GC object and its structure outlive
// the originating Wasm instance, we collect a transitive closure of all TypeDefinitions
// reachable from the declared type of the GC object. The structure holds onto this set
// to ensure all relevant type definitions live for at least as long as itself.
class WebAssemblyGCStructureTypeDependencies {
    public:
        WebAssemblyGCStructureTypeDependencies(Ref<const Wasm::TypeDefinition>&& unexpandedType);

    private:
        using WorkList = ReferenceWrapperVector<const Wasm::TypeDefinition>;

        void process(const Wasm::TypeDefinition&, WorkList&);
        void process(Wasm::FieldType, WorkList&);

        UncheckedKeyHashSet<Wasm::TypeHash> m_typeDefinitions;
};

// FIXME: It seems like almost all the fields of a Structure are useless to a wasm GC "object" since they can't have dynamic fields
// e.g. PropertyTables, Transitions, SeenProperties, Prototype, etc.
class WebAssemblyGCStructure final : public Structure {
    using Base = Structure;
public:
    friend class Structure;

    template<typename CellType, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.webAssemblyGCStructureSpace();
    }

    const Wasm::RTT& rtt() const LIFETIME_BOUND { return m_rtt; }
    const Wasm::TypeDefinition& typeDefinition() const LIFETIME_BOUND { return m_type; }

    static WebAssemblyGCStructure* create(VM&, JSGlobalObject*, const TypeInfo&, const ClassInfo*, Ref<const Wasm::TypeDefinition>&& unexpandedType, Ref<const Wasm::TypeDefinition>&& expandedType, Ref<const Wasm::RTT>&&);

    static constexpr ptrdiff_t offsetOfRTT() { return OBJECT_OFFSETOF(WebAssemblyGCStructure, m_rtt); }

private:
    WebAssemblyGCStructure(VM&, JSGlobalObject*, const TypeInfo&, const ClassInfo*, Ref<const Wasm::TypeDefinition>&& unexpandedType, Ref<const Wasm::TypeDefinition>&& expandedType, Ref<const Wasm::RTT>&&);
    WebAssemblyGCStructure(VM&, WebAssemblyGCStructure* previous);

    Ref<const Wasm::RTT> m_rtt;
    Ref<const Wasm::TypeDefinition> m_type;
    WebAssemblyGCStructureTypeDependencies m_typeDependencies;
};

} // namespace JSC

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::WebAssemblyGCStructure)
    static bool isType(const JSC::Structure& from)
    {
        return from.variant() == JSC::Structure::StructureVariant::WebAssemblyGC;
    }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBASSEMBLY)
