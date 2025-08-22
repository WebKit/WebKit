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

#include "JSCInlines.h"

#if ENABLE(WEBASSEMBLY)

namespace JSC {

WebAssemblyGCStructure::WebAssemblyGCStructure(VM& vm, JSGlobalObject* globalObject, const TypeInfo& typeInfo, const ClassInfo* classInfo, Ref<const Wasm::TypeDefinition>&& type, Ref<const Wasm::RTT>&& rtt)
    : Structure(vm, StructureVariant::WebAssemblyGC, globalObject, typeInfo, classInfo)
    , m_rtt(WTFMove(rtt))
    , m_type(WTFMove(type))
{
    for (unsigned i = 0; i < std::min((m_rtt->displaySizeExcludingThis() + 1), inlinedTypeDisplaySize); ++i)
        m_inlinedTypeDisplay[i] = m_rtt->displayEntry(i);
}

WebAssemblyGCStructure::WebAssemblyGCStructure(VM& vm, WebAssemblyGCStructure* previous)
    : Structure(vm, StructureVariant::WebAssemblyGC, previous)
    , m_rtt(previous->m_rtt)
    , m_type(previous->m_type)
    , m_inlinedTypeDisplay(previous->m_inlinedTypeDisplay)
{
}


WebAssemblyGCStructure* WebAssemblyGCStructure::create(VM& vm, JSGlobalObject* globalObject, const TypeInfo& typeInfo, const ClassInfo* classInfo, Ref<const Wasm::TypeDefinition>&& type, Ref<const Wasm::RTT>&& rtt)
{
    ASSERT(vm.structureStructure);
    WebAssemblyGCStructure* newStructure = new (NotNull, allocateCell<WebAssemblyGCStructure>(vm)) WebAssemblyGCStructure(vm, globalObject, typeInfo, classInfo, WTFMove(type), WTFMove(rtt));
    newStructure->finishCreation(vm);
    ASSERT(newStructure->type() == StructureType);
    return newStructure;
}

} // namespace JSC

#endif
