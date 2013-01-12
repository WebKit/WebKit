/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "CallFrame.h"
#include "JSCell.h"

namespace JSC {

inline JSCell::JSCell(CreatingEarlyCellTag)
{
}

inline JSCell::JSCell(JSGlobalData& globalData, Structure* structure)
    : m_structure(globalData, this, structure)
{
}

inline void JSCell::finishCreation(JSGlobalData& globalData)
{
#if ENABLE(GC_VALIDATION)
    ASSERT(globalData.isInitializingObject());
    globalData.setInitializingObjectClass(0);
#else
    UNUSED_PARAM(globalData);
#endif
    ASSERT(m_structure);
}

inline void JSCell::finishCreation(JSGlobalData& globalData, Structure* structure, CreatingEarlyCellTag)
{
#if ENABLE(GC_VALIDATION)
    ASSERT(globalData.isInitializingObject());
    globalData.setInitializingObjectClass(0);
    if (structure)
#endif
        m_structure.setEarlyValue(globalData, this, structure);
    // Very first set of allocations won't have a real structure.
    ASSERT(m_structure || !globalData.structureStructure);
}

inline Structure* JSCell::structure() const
{
    return m_structure.get();
}

inline void JSCell::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    MARK_LOG_PARENT(visitor, cell);

    visitor.append(&cell->m_structure);
}

template<typename T>
void* allocateCell(Heap& heap, size_t size)
{
    ASSERT(size >= sizeof(T));
#if ENABLE(GC_VALIDATION)
    ASSERT(!heap.globalData()->isInitializingObject());
    heap.globalData()->setInitializingObjectClass(&T::s_info);
#endif
    JSCell* result = 0;
    if (T::needsDestruction && T::hasImmortalStructure)
        result = static_cast<JSCell*>(heap.allocateWithImmortalStructureDestructor(size));
    else if (T::needsDestruction && !T::hasImmortalStructure)
        result = static_cast<JSCell*>(heap.allocateWithNormalDestructor(size));
    else 
        result = static_cast<JSCell*>(heap.allocateWithoutDestructor(size));
    result->clearStructure();
    return result;
}
    
template<typename T>
void* allocateCell(Heap& heap)
{
    return allocateCell<T>(heap, sizeof(T));
}
    
inline bool isZapped(const JSCell* cell)
{
    return cell->isZapped();
}

inline bool JSCell::isObject() const
{
    return m_structure->isObject();
}

inline bool JSCell::isString() const
{
    return m_structure->typeInfo().type() == StringType;
}

inline bool JSCell::isGetterSetter() const
{
    return m_structure->typeInfo().type() == GetterSetterType;
}

inline bool JSCell::isProxy() const
{
    return structure()->typeInfo().type() == ProxyType;
}

inline bool JSCell::isAPIValueWrapper() const
{
    return m_structure->typeInfo().type() == APIValueWrapperType;
}

inline void JSCell::setStructure(JSGlobalData& globalData, Structure* structure)
{
    ASSERT(structure->typeInfo().overridesVisitChildren() == this->structure()->typeInfo().overridesVisitChildren());
    ASSERT(structure->classInfo() == m_structure->classInfo());
    ASSERT(!m_structure
        || m_structure->transitionWatchpointSetHasBeenInvalidated()
        || m_structure.get() == structure);
    m_structure.set(globalData, this, structure);
}

inline const MethodTable* JSCell::methodTable() const
{
    return &classInfo()->methodTable;
}

inline bool JSCell::inherits(const ClassInfo* info) const
{
    return classInfo()->isSubClassOf(info);
}

ALWAYS_INLINE bool JSCell::fastGetOwnPropertySlot(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    if (!structure()->typeInfo().overridesGetOwnPropertySlot())
        return asObject(this)->inlineGetOwnPropertySlot(exec, propertyName, slot);
    return methodTable()->getOwnPropertySlot(this, exec, propertyName, slot);
}

// Fast call to get a property where we may not yet have converted the string to an
// identifier. The first time we perform a property access with a given string, try
// performing the property map lookup without forming an identifier. We detect this
// case by checking whether the hash has yet been set for this string.
ALWAYS_INLINE JSValue JSCell::fastGetOwnProperty(ExecState* exec, const String& name)
{
    if (!structure()->typeInfo().overridesGetOwnPropertySlot() && !structure()->hasGetterSetterProperties()) {
        PropertyOffset offset = name.impl()->hasHash()
            ? structure()->get(exec->globalData(), Identifier(exec, name))
            : structure()->get(exec->globalData(), name);
        if (offset != invalidOffset)
            return asObject(this)->locationForOffset(offset)->get();
    }
    return JSValue();
}

inline bool JSCell::toBoolean(ExecState* exec) const
{
    if (isString()) 
        return static_cast<const JSString*>(this)->toBoolean();
    return !structure()->masqueradesAsUndefined(exec->lexicalGlobalObject());
}

} // namespace JSC

