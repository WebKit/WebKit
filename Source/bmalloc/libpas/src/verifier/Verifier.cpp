/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#include "Verifier.h"

#include <iostream>
#include <map>
#include "pas_allocation_callbacks.h"
#include "pas_lock.h"
#include "pas_utils.h"
#include <stdlib.h>

using namespace std;

namespace {

void dumpStateHoldingLock();

#define ASSERT(exp) do { \
        if (exp) \
            break; \
        cerr << "pas Verifier: Assertion " << #exp << " at " << __FILE__ << ":" << __LINE__ \
             << " failed.\n"; \
        dumpStateHoldingLock(); \
        abort(); \
    } while (false)

pas_lock lock = PAS_LOCK_INITIALIZER;

struct Locker {
    Locker()
    {
        pas_lock_lock(&lock);
    }
    
    ~Locker()
    {
        pas_lock_unlock(&lock);
    }
};

struct Allocation {
    Allocation() = default;
    
    Allocation(size_t size,
               const char* name,
               pas_allocation_kind allocationKind)
        : size(size)
        , name(name)
        , allocationKind(allocationKind)
    {
    }
    
    size_t size { 0 };
    const char* name { nullptr };
    pas_allocation_kind allocationKind { pas_object_allocation };
};

typedef map<void*, Allocation> MapType;
MapType allocations[PAS_NUM_HEAP_KINDS];

void allocationCallback(void* resultingBase,
                        size_t size,
                        pas_heap_kind heapKind,
                        const char* name,
                        pas_allocation_kind allocationKind)
{
    Locker locker;
    
    MapType::iterator lowerBoundIter =
        allocations[heapKind].lower_bound(resultingBase);
    
    if (lowerBoundIter != allocations[heapKind].end()) {
        ASSERT(reinterpret_cast<uintptr_t>(lowerBoundIter->first)
               >= reinterpret_cast<uintptr_t>(resultingBase) + size);
    }
    
    MapType::iterator leftIter = lowerBoundIter;
    if (leftIter != allocations[heapKind].begin()) {
        --leftIter;
        if (leftIter->first < resultingBase) {
            ASSERT(reinterpret_cast<uintptr_t>(leftIter->first) + leftIter->second.size
                   <= reinterpret_cast<uintptr_t>(resultingBase));
        }
    }
    
    auto result = allocations[heapKind].emplace(resultingBase,
                                                Allocation(size, name, allocationKind));
    ASSERT(result.second);
}

void deallocationCallback(void* base,
                          size_t size,
                          pas_heap_kind heapKind,
                          pas_allocation_kind allocationKind)
{
    Locker locker;
    
    MapType::iterator iter =
        allocations[heapKind].upper_bound(base);
    
    ASSERT(iter != allocations[heapKind].begin());
    
    --iter;
    
    void* address = iter->first;
    Allocation allocation = iter->second;
    
    if (heapKind != pas_bootstrap_free_heap_kind) {
        ASSERT(allocationKind == pas_object_allocation);
        ASSERT(address == base);
        size = allocation.size;
    }
    
    ASSERT(reinterpret_cast<uintptr_t>(address)
           <= reinterpret_cast<uintptr_t>(base));
    ASSERT(reinterpret_cast<uintptr_t>(address) + allocation.size
           >= reinterpret_cast<uintptr_t>(base) + size);
    ASSERT(allocation.allocationKind == allocationKind);
    
    allocations[heapKind].erase(iter);
    
    if (reinterpret_cast<uintptr_t>(address)
        < reinterpret_cast<uintptr_t>(base)) {
        ASSERT(allocationKind == pas_delegate_allocation);
        
        auto result = allocations[heapKind].emplace(
            address,
            Allocation(
                reinterpret_cast<uintptr_t>(base) - reinterpret_cast<uintptr_t>(address),
                allocation.name,
                allocationKind));
        ASSERT(result.second);
    }
    
    if (reinterpret_cast<uintptr_t>(address) + allocation.size
        > reinterpret_cast<uintptr_t>(base) + size) {
        ASSERT(allocationKind == pas_delegate_allocation);
        
        auto result = allocations[heapKind].emplace(
            reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(base) + size),
            Allocation(
                (reinterpret_cast<uintptr_t>(address) + allocation.size)
                - (reinterpret_cast<uintptr_t>(base) + size),
                allocation.name,
                allocationKind));
        ASSERT(result.second);
    }
}

void dumpStateForHeapKind(pas_heap_kind heapKind)
{
    cout << pas_heap_kind_get_string(heapKind) << ":\n";
    for (auto& pair : allocations[heapKind]) {
        void* address = pair.first;
        Allocation allocation = pair.second;
        
        cout << "    " << address << "..."
             << reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address)
                                        + allocation.size)
             << ": " << allocation.name << ", size = " << allocation.size
             << ", kind = " << pas_allocation_kind_get_string(allocation.allocationKind)
             << "\n";
    }
}

void dumpStateHoldingLock()
{
    dumpStateForHeapKind(pas_bootstrap_free_heap_kind);
    dumpStateForHeapKind(pas_immortal_heap_kind);
    dumpStateForHeapKind(pas_large_utility_free_heap_kind);
    dumpStateForHeapKind(pas_utility_heap_kind);
}

void uninstall()
{
    pas_allocation_callback = NULL;
    pas_deallocation_callback = NULL;
}

} // anonymous namespace

extern "C" void pas_install_verifier()
{
    pas_allocation_callback = allocationCallback;
    pas_deallocation_callback = deallocationCallback;
    atexit(uninstall);
}

extern "C" void pas_dump_state()
{
    Locker locker;
    dumpStateHoldingLock();
}

