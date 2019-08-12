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

#pragma once

#include <wtf/Atomics.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashFunctions.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WTF {

// This is a concurrent hash-based set for pointers. It's optimized for:
//
// - High rate of contains() calls.
// - High rate of add() calls that don't add anything new. add() calls that don't add anything (nop adds)
//   don't mutate the table at all.
// - Not too many threads. I doubt this scales beyond ~4. Though, it may actually scale better than that
//   if the rate of nop adds is absurdly high.
//
// If we wanted this to scale better, the main change we'd have to make is how this table determines when
// to resize. Right now it's a shared counter. We lock;xadd this counter. One easy way to make that
// scalable is to require each thread that works with the ConcurrentPtrHashSet to register itself first.
// Then each thread would have some data structure that has a counter. We could institute the policy that
// each thread simply increments its own load counter, in its own data structure. Then, if any search to
// resolve a collision does more than N iterations, we can compute a combined load by summing the load
// counters of all of the thread data structures.
//
// ConcurrentPtrHashSet's main user, the GC, sees a 98% nop add rate in Speedometer. That's why this
// focuses so much on cases where the table does not change.
class ConcurrentPtrHashSet final {
    WTF_MAKE_NONCOPYABLE(ConcurrentPtrHashSet);
    WTF_MAKE_FAST_ALLOCATED;

public:
    WTF_EXPORT_PRIVATE ConcurrentPtrHashSet();
    WTF_EXPORT_PRIVATE ~ConcurrentPtrHashSet();
    
    template<typename T>
    bool contains(T value)
    {
        return containsImpl(cast(value));
    }
    
    template<typename T>
    bool add(T value)
    {
        return addImpl(cast(value));
    }
    
    size_t size() const
    {
        return m_table.loadRelaxed()->load.loadRelaxed();
    }
    
    // Only call when you know that no other thread can call add(). This frees up memory without changing
    // the contents of the table.
    WTF_EXPORT_PRIVATE void deleteOldTables();
    
    // Only call when you know that no other thread can call add(). This frees up all memory except for
    // the smallest possible hashtable.
    WTF_EXPORT_PRIVATE void clear();
    
private:
    struct Table {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        
        static std::unique_ptr<Table> create(unsigned size);
        
        unsigned maxLoad() const { return size / 2; }
        
        unsigned size; // This is immutable.
        unsigned mask; // This is immutable.
        Atomic<unsigned> load;
        Atomic<void*> array[1];
    };
    
    static unsigned hash(void* ptr)
    {
        return PtrHash<void*>::hash(ptr);
    }
    
    void initialize();
    
    template<typename T>
    void* cast(T value)
    {
        static_assert(sizeof(T) <= sizeof(void*), "type too big");
        union {
            void* ptr;
            T value;
        } u;
        u.ptr = nullptr;
        u.value = value;
        return u.ptr;
    }
    
    bool containsImpl(void* ptr) const
    {
        Table* table = m_table.loadRelaxed();
        unsigned mask = table->mask;
        unsigned startIndex = hash(ptr) & mask;
        unsigned index = startIndex;
        for (;;) {
            void* entry = table->array[index].loadRelaxed();
            if (!entry)
                return false;
            if (entry == ptr)
                return true;
            index = (index + 1) & mask;
            RELEASE_ASSERT(index != startIndex);
        }
    }
    
    // Returns true if a new entry was added.
    bool addImpl(void* ptr)
    {
        Table* table = m_table.loadRelaxed();
        unsigned mask = table->mask;
        unsigned startIndex = hash(ptr) & mask;
        unsigned index = startIndex;
        for (;;) {
            void* entry = table->array[index].loadRelaxed();
            if (!entry)
                return addSlow(table, mask, startIndex, index, ptr);
            if (entry == ptr)
                return false;
            index = (index + 1) & mask;
            RELEASE_ASSERT(index != startIndex);
        }
    }
    
    WTF_EXPORT_PRIVATE bool addSlow(Table* table, unsigned mask, unsigned startIndex, unsigned index, void* ptr);

    void resizeIfNecessary();
    bool resizeAndAdd(void* ptr);
    
    Vector<std::unique_ptr<Table>, 4> m_allTables;
    Atomic<Table*> m_table; // This is never null.
    Lock m_lock; // We just use this to control resize races.
};

} // namespace WTF

using WTF::ConcurrentPtrHashSet;
