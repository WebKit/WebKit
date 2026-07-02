/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

// This is a lightweight alternative to StructureInlines.h for files that
// only need Structure::create() (e.g., for defining createStructure() methods).
// It avoids the heavy transitive includes that StructureInlines.h pulls in
// (BigIntPrototype.h, StringPrototype.h, SymbolPrototype.h, BrandedStructure.h,
// WebAssemblyGCStructure.h, JSArrayBufferView.h, PropertyTable.h, etc.).

#include <JavaScriptCore/JSCellInlines.h>
#include <JavaScriptCore/JSGlobalProxy.h>
#include <JavaScriptCore/Structure.h>
#include <JavaScriptCore/WriteBarrierInlines.h>

namespace JSC {

inline void JSObject::didBecomePrototype(VM& vm)
{
    Structure* oldStructure = structure();
    if (!oldStructure->mayBePrototype()) [[unlikely]] {
        DeferredStructureTransitionWatchpointFire deferred(vm, oldStructure);
        setStructure(vm, Structure::becomePrototypeTransition(vm, oldStructure, &deferred));
    }

    if (type() == GlobalProxyType) [[unlikely]]
        uncheckedDowncast<JSGlobalProxy>(this)->target()->didBecomePrototype(vm);
}

template<typename CellType, SubspaceAccess>
inline GCClient::IsoSubspace* Structure::subspaceFor(VM& vm)
{
    return &vm.structureSpace();
}

inline void Structure::finishCreation(VM& vm, CreatingEarlyCellTag)
{
    Base::finishCreation(vm, this, CreatingEarlyCell);
    ASSERT(m_prototype);
    ASSERT(m_prototype.isNull());
    ASSERT(!vm.structureStructure);
}

inline Structure* Structure::create(VM& vm, JSGlobalObject* globalObject, JSValue prototype, const TypeInfo& typeInfo, const ClassInfo* classInfo, IndexingType indexingModeIncludingHistory, unsigned inlineCapacity)
{
    ASSERT(vm.structureStructure);
    ASSERT(classInfo);
    if (auto* object = prototype.getObject()) {
        ASSERT(!object->anyObjectInChainMayInterceptIndexedAccesses() || hasSlowPutArrayStorage(indexingModeIncludingHistory) || !hasIndexedProperties(indexingModeIncludingHistory));
        object->didBecomePrototype(vm);
    }

    Structure* structure = new (NotNull, allocateCell<Structure>(vm)) Structure(vm, globalObject, prototype, typeInfo, classInfo, indexingModeIncludingHistory, inlineCapacity);
    structure->finishCreation(vm);
    ASSERT(structure->type() == StructureType);
    return structure;
}

inline Structure* Structure::createStructure(VM& vm)
{
    ASSERT(!vm.structureStructure);
    Structure* structure = new (NotNull, allocateCell<Structure>(vm)) Structure(vm, CreatingEarlyCell);
    structure->finishCreation(vm, CreatingEarlyCell);
    return structure;
}

} // namespace JSC
