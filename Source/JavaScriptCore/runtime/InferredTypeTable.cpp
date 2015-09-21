/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "InferredTypeTable.h"

#include "JSCInlines.h"

namespace JSC {

const ClassInfo InferredTypeTable::s_info = { "InferredTypeTable", 0, 0, CREATE_METHOD_TABLE(InferredTypeTable) };

InferredTypeTable* InferredTypeTable::create(VM& vm)
{
    InferredTypeTable* result = new (NotNull, allocateCell<InferredTypeTable>(vm.heap)) InferredTypeTable(vm);
    result->finishCreation(vm);
    return result;
}

void InferredTypeTable::destroy(JSCell* cell)
{
    InferredTypeTable* inferredTypeTable = static_cast<InferredTypeTable*>(cell);
    inferredTypeTable->InferredTypeTable::~InferredTypeTable();
}

Structure* InferredTypeTable::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

void InferredTypeTable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    InferredTypeTable* inferredTypeTable = jsCast<InferredTypeTable*>(cell);

    ConcurrentJITLocker locker(inferredTypeTable->m_lock);
    
    for (auto& entry : inferredTypeTable->m_table) {
        if (!entry.value)
            continue;
        if (entry.value->isRelevant())
            visitor.append(&entry.value);
        else
            entry.value.clear();
    }
}

InferredType* InferredTypeTable::get(const ConcurrentJITLocker&, UniquedStringImpl* uid)
{
    auto iter = m_table.find(uid);
    if (iter == m_table.end() || !iter->value)
        return nullptr;

    // Take this opportunity to prune invalidated types.
    if (!iter->value->isRelevant()) {
        iter->value.clear();
        return nullptr;
    }

    return iter->value.get();
}

InferredType* InferredTypeTable::get(UniquedStringImpl* uid)
{
    ConcurrentJITLocker locker(m_lock);
    return get(locker, uid);
}

InferredType* InferredTypeTable::get(PropertyName propertyName)
{
    return get(propertyName.uid());
}

bool InferredTypeTable::willStoreValue(
    VM& vm, PropertyName propertyName, JSValue value, StoredPropertyAge age)
{
    // The algorithm here relies on the fact that only one thread modifies the hash map.
    
    if (age == OldProperty) {
        TableType::iterator iter = m_table.find(propertyName.uid());
        if (iter == m_table.end() || !iter->value)
            return false; // Absence on replace => top.
        
        if (iter->value->willStoreValue(vm, propertyName, value))
            return true;
        
        iter->value.clear();
        return false;
    }

    TableType::AddResult result;
    {
        ConcurrentJITLocker locker(m_lock);
        result = m_table.add(propertyName.uid(), WriteBarrier<InferredType>());
    }
    if (result.isNewEntry) {
        InferredType* inferredType = InferredType::create(vm);
        WTF::storeStoreFence();
        result.iterator->value.set(vm, this, inferredType);
    } else if (!result.iterator->value)
        return false;
    
    if (result.iterator->value->willStoreValue(vm, propertyName, value))
        return true;
    
    result.iterator->value.clear();
    return false;
}

void InferredTypeTable::makeTop(VM& vm, PropertyName propertyName, StoredPropertyAge age)
{
    // The algorithm here relies on the fact that only one thread modifies the hash map.
    if (age == OldProperty) {
        TableType::iterator iter = m_table.find(propertyName.uid());
        if (iter == m_table.end() || !iter->value)
            return; // Absence on replace => top.

        iter->value->makeTop(vm, propertyName);
        iter->value.clear();
        return;
    }

    TableType::AddResult result;
    {
        ConcurrentJITLocker locker(m_lock);
        result = m_table.add(propertyName.uid(), WriteBarrier<InferredType>());
    }
    if (!result.iterator->value)
        return;
    
    result.iterator->value->makeTop(vm, propertyName);
    result.iterator->value.clear();
}

InferredTypeTable::InferredTypeTable(VM& vm)
    : Base(vm, vm.inferredTypeTableStructure.get())
{
}

InferredTypeTable::~InferredTypeTable()
{
}

} // namespace JSC

