/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef StructureInlines_h
#define StructureInlines_h

#include "Structure.h"

namespace JSC {

inline Structure* Structure::create(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue prototype, const TypeInfo& typeInfo, const ClassInfo* classInfo, IndexingType indexingType, unsigned inlineCapacity)
{
    ASSERT(globalData.structureStructure);
    ASSERT(classInfo);
    Structure* structure = new (NotNull, allocateCell<Structure>(globalData.heap)) Structure(globalData, globalObject, prototype, typeInfo, classInfo, indexingType, inlineCapacity);
    structure->finishCreation(globalData);
    return structure;
}

inline Structure* Structure::createStructure(JSGlobalData& globalData)
{
    ASSERT(!globalData.structureStructure);
    Structure* structure = new (NotNull, allocateCell<Structure>(globalData.heap)) Structure(globalData);
    structure->finishCreation(globalData, CreatingEarlyCell);
    return structure;
}

inline Structure* Structure::create(JSGlobalData& globalData, const Structure* structure)
{
    ASSERT(globalData.structureStructure);
    Structure* newStructure = new (NotNull, allocateCell<Structure>(globalData.heap)) Structure(globalData, structure);
    newStructure->finishCreation(globalData);
    if (structure->typeInfo().structureHasRareData())
        newStructure->cloneRareDataFrom(globalData, structure);
    return newStructure;
}

inline PropertyOffset Structure::get(JSGlobalData& globalData, PropertyName propertyName)
{
    ASSERT(structure()->classInfo() == &s_info);
    materializePropertyMapIfNecessary(globalData);
    if (!m_propertyTable)
        return invalidOffset;

    PropertyMapEntry* entry = m_propertyTable->find(propertyName.uid()).first;
    return entry ? entry->offset : invalidOffset;
}

inline PropertyOffset Structure::get(JSGlobalData& globalData, const WTF::String& name)
{
    ASSERT(structure()->classInfo() == &s_info);
    materializePropertyMapIfNecessary(globalData);
    if (!m_propertyTable)
        return invalidOffset;

    PropertyMapEntry* entry = m_propertyTable->findWithString(name.impl()).first;
    return entry ? entry->offset : invalidOffset;
}
    
inline bool Structure::masqueradesAsUndefined(JSGlobalObject* lexicalGlobalObject)
{
    return typeInfo().masqueradesAsUndefined() && globalObject() == lexicalGlobalObject;
}

ALWAYS_INLINE void SlotVisitor::internalAppend(JSCell* cell)
{
    ASSERT(!m_isCheckingForDefaultMarkViolation);
    if (!cell)
        return;
#if ENABLE(GC_VALIDATION)
    validate(cell);
#endif
    if (Heap::testAndSetMarked(cell) || !cell->structure())
        return;

    m_visitCount++;
        
    MARK_LOG_CHILD(*this, cell);

    // Should never attempt to mark something that is zapped.
    ASSERT(!cell->isZapped());
        
    m_stack.append(cell);
}

inline bool Structure::transitivelyTransitionedFrom(Structure* structureToFind)
{
    for (Structure* current = this; current; current = current->previousID()) {
        if (current == structureToFind)
            return true;
    }
    return false;
}

inline void Structure::setEnumerationCache(JSGlobalData& globalData, JSPropertyNameIterator* enumerationCache)
{
    ASSERT(!isDictionary());
    if (!typeInfo().structureHasRareData())
        allocateRareData(globalData);
    rareData()->setEnumerationCache(globalData, this, enumerationCache);
}

inline JSPropertyNameIterator* Structure::enumerationCache()
{
    if (!typeInfo().structureHasRareData())
        return 0;
    return rareData()->enumerationCache();
}

inline JSValue Structure::prototypeForLookup(JSGlobalObject* globalObject) const
{
    if (isObject())
        return m_prototype.get();

    ASSERT(typeInfo().type() == StringType);
    return globalObject->stringPrototype();
}

inline JSValue Structure::prototypeForLookup(ExecState* exec) const
{
    return prototypeForLookup(exec->lexicalGlobalObject());
}

inline StructureChain* Structure::prototypeChain(JSGlobalData& globalData, JSGlobalObject* globalObject) const
{
    // We cache our prototype chain so our clients can share it.
    if (!isValid(globalObject, m_cachedPrototypeChain.get())) {
        JSValue prototype = prototypeForLookup(globalObject);
        m_cachedPrototypeChain.set(globalData, this, StructureChain::create(globalData, prototype.isNull() ? 0 : asObject(prototype)->structure()));
    }
    return m_cachedPrototypeChain.get();
}

inline StructureChain* Structure::prototypeChain(ExecState* exec) const
{
    return prototypeChain(exec->globalData(), exec->lexicalGlobalObject());
}

inline bool Structure::isValid(JSGlobalObject* globalObject, StructureChain* cachedPrototypeChain) const
{
    if (!cachedPrototypeChain)
        return false;

    JSValue prototype = prototypeForLookup(globalObject);
    WriteBarrier<Structure>* cachedStructure = cachedPrototypeChain->head();
    while (*cachedStructure && !prototype.isNull()) {
        if (asObject(prototype)->structure() != cachedStructure->get())
            return false;
        ++cachedStructure;
        prototype = asObject(prototype)->prototype();
    }
    return prototype.isNull() && !*cachedStructure;
}

inline bool Structure::isValid(ExecState* exec, StructureChain* cachedPrototypeChain) const
{
    return isValid(exec->lexicalGlobalObject(), cachedPrototypeChain);
}

} // namespace JSC

#endif // StructureInlines_h

