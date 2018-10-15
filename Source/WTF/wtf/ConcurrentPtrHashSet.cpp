/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <wtf/ConcurrentPtrHashSet.h>

#include <wtf/DataLog.h>

namespace WTF {

ConcurrentPtrHashSet::ConcurrentPtrHashSet()
{
    initialize();
}

ConcurrentPtrHashSet::~ConcurrentPtrHashSet()
{
}

void ConcurrentPtrHashSet::deleteOldTables()
{
    // This is just in case. It does not make it OK for other threads to call add(). But it might prevent
    // some bad crashes if we did make that mistake.
    auto locker = holdLock(m_lock); 
    
    m_allTables.removeAllMatching(
        [&] (std::unique_ptr<Table>& table) -> bool {
            return table.get() != m_table.loadRelaxed();
        });
}

void ConcurrentPtrHashSet::clear()
{
    // This is just in case. It does not make it OK for other threads to call add(). But it might prevent
    // some bad crashes if we did make that mistake.
    auto locker = holdLock(m_lock); 
    
    m_allTables.clear();
    initialize();
}

void ConcurrentPtrHashSet::initialize()
{
    constexpr unsigned initialSize = 32;
    std::unique_ptr<Table> table = Table::create(initialSize);
    m_table.storeRelaxed(table.get());
    m_allTables.append(WTFMove(table));
}

bool ConcurrentPtrHashSet::addSlow(Table* table, unsigned mask, unsigned startIndex, unsigned index, void* ptr)
{
    if (table->load.exchangeAdd(1) >= table->maxLoad())
        return resizeAndAdd(ptr);
    
    for (;;) {
        void* oldEntry = table->array[index].compareExchangeStrong(nullptr, ptr);
        if (!oldEntry) {
            if (m_table.load() != table) {
                // We added an entry to an old table! We need to reexecute the add on the new table.
                return add(ptr);
            }
            return true;
        }
        if (oldEntry == ptr)
            return false;
        index = (index + 1) & mask;
        RELEASE_ASSERT(index != startIndex);
    }
}

void ConcurrentPtrHashSet::resizeIfNecessary()
{
    auto locker = holdLock(m_lock);
    Table* table = m_table.loadRelaxed();
    if (table->load.loadRelaxed() < table->maxLoad())
        return;
    
    std::unique_ptr<Table> newTable = Table::create(table->size * 2);
    unsigned mask = newTable->mask;
    unsigned load = 0;
    for (unsigned i = 0; i < table->size; ++i) {
        void* ptr = table->array[i].loadRelaxed();
        if (!ptr)
            continue;
        
        unsigned startIndex = hash(ptr) & mask;
        unsigned index = startIndex;
        for (;;) {
            Atomic<void*>& entryRef = newTable->array[index];
            void* entry = entryRef.loadRelaxed();
            if (!entry) {
                entryRef.storeRelaxed(ptr);
                break;
            }
            RELEASE_ASSERT(entry != ptr);
            index = (index + 1) & mask;
            RELEASE_ASSERT(index != startIndex);
        }
        
        load++;
    }
    
    newTable->load.storeRelaxed(load);
    
    m_table.store(newTable.get());
    m_allTables.append(WTFMove(newTable));
}

bool ConcurrentPtrHashSet::resizeAndAdd(void* ptr)
{
    resizeIfNecessary();
    return add(ptr);
}

std::unique_ptr<ConcurrentPtrHashSet::Table> ConcurrentPtrHashSet::Table::create(unsigned size)
{
    std::unique_ptr<ConcurrentPtrHashSet::Table> result(new (fastMalloc(OBJECT_OFFSETOF(Table, array) + sizeof(Atomic<void*>) * size)) Table());
    result->size = size;
    result->mask = size - 1;
    result->load.storeRelaxed(0);
    for (unsigned i = 0; i < size; ++i)
        result->array[i].storeRelaxed(nullptr);
    return result;
}

} // namespace WTF

