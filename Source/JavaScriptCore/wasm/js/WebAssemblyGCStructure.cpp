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

#include "config.h"
#include "WebAssemblyGCStructure.h"

#include "AbstractSlotVisitorInlines.h"
#include "DeferGC.h"
#include "JSCInlines.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyStruct.h"
#include "SlotVisitorInlines.h"
#include "WasmFormat.h"

#if ENABLE(WEBASSEMBLY)

namespace JSC {

WebAssemblyGCStructure::WebAssemblyGCStructure(VM& vm, const TypeInfo& typeInfo, const ClassInfo* classInfo, Ref<const Wasm::TypeDefinition>&& unexpandedType, Ref<const Wasm::TypeDefinition>&& type, Ref<const Wasm::RTT>&& rtt)
    : Structure(vm, StructureVariant::WebAssemblyGC, nullptr, typeInfo, classInfo)
    , m_rtt(WTF::move(rtt))
    , m_type(WTF::move(type))
    , m_typeDependencies(unexpandedType)
{
    setMayBePrototype(true); // Make sure that didPrototype transition does not happen.
    switch (m_rtt->kind()) {
    case Wasm::RTTKind::Function:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    case Wasm::RTTKind::Array:
        ASSERT(classInfo->isSubClassOf(JSWebAssemblyArray::info()));
        break;
    case Wasm::RTTKind::Struct:
        ASSERT(classInfo->isSubClassOf(JSWebAssemblyStruct::info()));
        break;
    }
}

WebAssemblyGCStructure* WebAssemblyGCStructure::create(VM& vm, const TypeInfo& typeInfo, const ClassInfo* classInfo, Ref<const Wasm::TypeDefinition>&& unexpandedType, Ref<const Wasm::TypeDefinition>&& type, Ref<const Wasm::RTT>&& rtt)
{
    ASSERT(vm.structureStructure);
    const Wasm::RTT* rttPtr = rtt.ptr();
    DeferGC deferGC(vm);
    return vm.wasmGCStructureMap.ensureValue(rttPtr, [&] {
        WebAssemblyGCStructure* newStructure = new (NotNull, allocateCell<WebAssemblyGCStructure>(vm)) WebAssemblyGCStructure(vm, typeInfo, classInfo, WTF::move(unexpandedType), WTF::move(type), WTF::move(rtt));
        newStructure->finishCreation(vm);
        ASSERT(newStructure->type() == StructureType);
        return newStructure;
    });
}

void WebAssemblyGCStructure::finishCreation(VM& vm)
{
    Structure::finishCreation(vm);

    // The RTT display stores ancestors at indices 0..displaySize-1 and |this| at index displaySize.
    // Mirror that layout in the inlined type display with StructureIDs.
    unsigned displaySize = m_rtt->displaySizeExcludingThis();
    unsigned actualDisplaySize = displaySize + 1; // +1 for |this|
    unsigned count = std::min(actualDisplaySize, inlinedDisplaySize);
    for (unsigned i = 0; i < count; ++i) {
        RefPtr entryRTT = m_rtt->displayEntry(i);
        auto* structure = this;
        if (entryRTT != m_rtt.ptr())
            structure = vm.wasmGCStructureMap.get(entryRTT);
        RELEASE_ASSERT(structure);
        m_inlinedDisplay[i].set(vm, this, structure);
    }
}

template<typename Visitor>
void WebAssemblyGCStructure::visitAdditionalChildren(JSCell* cell, Visitor& visitor)
{
    WebAssemblyGCStructure* thisObject = uncheckedDowncast<WebAssemblyGCStructure>(uncheckedDowncast<Structure>(cell));
    for (auto& slot : thisObject->m_inlinedDisplay)
        visitor.append(slot);
}

template void WebAssemblyGCStructure::visitAdditionalChildren(JSCell*, AbstractSlotVisitor&);
template void WebAssemblyGCStructure::visitAdditionalChildren(JSCell*, SlotVisitor&);

} // namespace JSC

#endif
