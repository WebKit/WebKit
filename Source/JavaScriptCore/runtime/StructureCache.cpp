/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "StructureCache.h"

#include "JSGlobalObject.h"
#include "JSCInlines.h"
#include <wtf/Locker.h>

namespace JSC {

inline Structure* StructureCache::createEmptyStructure(JSGlobalObject* globalObject, JSObject* prototype, const TypeInfo& typeInfo, const ClassInfo* classInfo, IndexingType indexingType, unsigned inlineCapacity, bool makePolyProtoStructure, FunctionExecutable* executable)
{
    RELEASE_ASSERT(!!prototype); // We use nullptr inside the HashMap for prototype to mean poly proto, so user's of this API must provide non-null prototypes.

    // We don't need to lock here because only the main thread can get here, and only the main thread can mutate the cache
    PrototypeKey key { makePolyProtoStructure ? nullptr : prototype, executable, inlineCapacity, classInfo, globalObject };
    if (Structure* structure = m_structures.get(key)) {
        if (makePolyProtoStructure) {
            prototype->didBecomePrototype();
            RELEASE_ASSERT(structure->hasPolyProto());
        } else
            RELEASE_ASSERT(structure->hasMonoProto());
        ASSERT(prototype->mayBePrototype());
        return structure;
    }

    prototype->didBecomePrototype();

    VM& vm = globalObject->vm();
    Structure* structure;
    if (makePolyProtoStructure) {
        structure = Structure::create(
            Structure::PolyProto, vm, globalObject, prototype, typeInfo, classInfo, indexingType, inlineCapacity);
    } else {
        structure = Structure::create(
            vm, globalObject, prototype, typeInfo, classInfo, indexingType, inlineCapacity);
    }
    auto locker = holdLock(m_lock);
    m_structures.set(key, structure);
    return structure;
}

Structure* StructureCache::emptyObjectStructureConcurrently(JSGlobalObject* globalObject, JSObject* prototype, unsigned inlineCapacity)
{
    RELEASE_ASSERT(!!prototype); // We use nullptr inside the HashMap for prototype to mean poly proto, so user's of this API must provide non-null prototypes.
    
    PrototypeKey key { prototype, nullptr, inlineCapacity, JSFinalObject::info(), globalObject };
    auto locker = holdLock(m_lock);
    if (Structure* structure = m_structures.get(key)) {
        ASSERT(prototype->mayBePrototype());
        return structure;
    }
    return nullptr;
}

Structure* StructureCache::emptyStructureForPrototypeFromBaseStructure(JSGlobalObject* globalObject, JSObject* prototype, Structure* baseStructure)
{
    // We currently do not have inline capacity static analysis for subclasses and all internal function constructors have a default inline capacity of 0.
    IndexingType indexingType = baseStructure->indexingType();
    if (prototype->anyObjectInChainMayInterceptIndexedAccesses(globalObject->vm()) && hasIndexedProperties(indexingType))
        indexingType = (indexingType & ~IndexingShapeMask) | SlowPutArrayStorageShape;

    return createEmptyStructure(globalObject, prototype, baseStructure->typeInfo(), baseStructure->classInfo(), indexingType, 0, false, nullptr);
}

Structure* StructureCache::emptyObjectStructureForPrototype(JSGlobalObject* globalObject, JSObject* prototype, unsigned inlineCapacity, bool makePolyProtoStructure, FunctionExecutable* executable)
{
    return createEmptyStructure(globalObject, prototype, JSFinalObject::typeInfo(), JSFinalObject::info(), JSFinalObject::defaultIndexingType, inlineCapacity, makePolyProtoStructure, executable);
}

} // namespace JSC
