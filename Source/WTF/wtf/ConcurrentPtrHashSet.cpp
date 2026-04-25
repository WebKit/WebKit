/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

namespace WTF {

ConcurrentPtrHashSet::ConcurrentPtrHashSet()
{
    initialize();
}

ConcurrentPtrHashSet::~ConcurrentPtrHashSet() = default;

void ConcurrentPtrHashSet::deleteOldTables()
{
    // This is just in case. It does not make it OK for other threads to call add(). But it might prevent
    // some bad crashes if we did make that mistake.
    Locker locker { m_lock };

    ASSERT(m_table.loadRelaxed() != m_stubTable.get());
    m_allTables.removeAllMatching(
        [&] (std::unique_ptr<Table>& table) -> bool {
            return table.get() != m_table.loadRelaxed();
        });
}

void ConcurrentPtrHashSet::clear()
{
    // This is just in case. It does not make it OK for other threads to call add(). But it might prevent
    // some bad crashes if we did make that mistake.
    Locker locker { m_lock };
    
    m_allTables.clear();
    initialize();
}

void ConcurrentPtrHashSet::initialize()
{
    constexpr unsigned initialSize = 32;
    std::unique_ptr<Table> table = Table::create(initialSize);
    m_table.storeRelaxed(table.get());
    m_allTables.append(WTF::move(table));

    m_stubTable = Table::createStub();
}

bool ConcurrentPtrHashSet::addSlow(Table* table, unsigned mask, unsigned startIndex, unsigned index, void* ptr)
{
    if (table->load.exchangeAdd(1) >= table->maxLoad())
        return resizeAndAdd(ptr);
    
    for (;;) {
        void* oldEntry = table->at(index).compareExchangeStrong(nullptr, ptr);
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

bool ConcurrentPtrHashSet::containsImplSlow(void* ptr) const
{
    Locker locker { m_lock };
    ASSERT(m_table.loadRelaxed() != m_stubTable.get());
    return containsImpl(ptr);
}

size_t ConcurrentPtrHashSet::sizeSlow() const
{
    Locker locker { m_lock };
    ASSERT(m_table.loadRelaxed() != m_stubTable.get());
    return size();
}

void ConcurrentPtrHashSet::resizeIfNecessary()
{
    Locker locker { m_lock };
    Table* table = m_table.loadRelaxed();
    ASSERT(table != m_stubTable.get());
    if (table->load.loadRelaxed() < table->maxLoad())
        return;

    // Stubbing out m_table with m_stubTable here is necessary to ensure that
    // we don't miss copying any entries that may be concurrently be added.
    //
    // If addSlow() completes before this stubbing, the new entry is guaranteed
    // to be copied below.
    //
    // If addSlow() completes after this stubbing, addSlow()  will check m_table
    // before it finishes, and detect that its newly added entry may not have
    // made it in. As a result, it will try to re-add the entry, and end up
    // blocking on resizeIfNecessary() until the resizing is donw. This is
    // because m_stubTable will tell addSlow() think that the table is out of
    // space and it needs to resize. NOTE: m_stubTable always says it is out of
    // space.
    m_table.store(m_stubTable.get());

    std::unique_ptr<Table> newTable = Table::create(table->size * 2);
    unsigned mask = newTable->mask;
    unsigned load = 0;
    for (unsigned i = 0; i < table->size; ++i) {
        void* ptr = table->at(i).loadRelaxed();
        if (!ptr)
            continue;
        
        unsigned startIndex = hash(ptr) & mask;
        unsigned index = startIndex;
        for (;;) {
            Atomic<void*>& entryRef = newTable->at(index);
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

    // addSlow() will always start by exchangeAdd'ing 1 to the current m_table's
    // load value before checking if it exceeds its max allowed load. For the
    // real m_table, this is not an issue because at most, it will accummulate
    // up to N extra adds above max load, where N is the number of threads
    // concurrrently adding entries.
    //
    // However, m_table may be replaced with m_stubTable for each resize
    // operation. As a result, the cummulative error on its load value
    // may far exceed N (as specified above). To fix this, we always reset it
    // here to prevent an overflow. Note: a load of stubDefaultLoadValue means
    // that m_stubTable is full since its size is 0.
    //
    // In practice, this won't matter because we most likely won't do so many
    // resize operations such that this will get to the point of overflowing.
    // However, since resizing is not in the fast path, let's just be pedantic
    // and reset it for correctness.
    m_stubTable->load.store(Table::stubDefaultLoadValue);

    m_allTables.append(WTF::move(newTable));
}

bool ConcurrentPtrHashSet::resizeAndAdd(void* ptr)
{
    resizeIfNecessary();
    return add(ptr);
}

std::unique_ptr<ConcurrentPtrHashSet::Table> ConcurrentPtrHashSet::Table::create(unsigned size)
{
    std::unique_ptr<ConcurrentPtrHashSet::Table> result(new (fastMalloc(allocationSize(size))) Table(size));
    result->size = size;
    result->mask = size - 1;
    result->load.storeRelaxed(0);
    for (unsigned i = 0; i < size; ++i)
        result->at(i).storeRelaxed(nullptr);
    return result;
}

std::unique_ptr<ConcurrentPtrHashSet::Table> ConcurrentPtrHashSet::Table::createStub()
{
    // The stub table is set up to look like it is already filled up. This is
    // so that it can be used during resizing to force all attempts to add to
    // be routed to resizeAndAdd() where it will block until the resizing is
    // done.

    // Despite being "full", the stub table allocates a single bucket to allow
    // loading nullptr from bucket 0.
    std::unique_ptr<ConcurrentPtrHashSet::Table> result(new (fastMalloc(allocationSize(1))) Table(1));
    result->size = 0;
    result->mask = 0;
    result->load.storeRelaxed(stubDefaultLoadValue);
    result->at(0).storeRelaxed(nullptr);
    return result;
}

} // namespace WTF
