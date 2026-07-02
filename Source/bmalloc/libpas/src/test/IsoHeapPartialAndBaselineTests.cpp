/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#include "TestHarness.h"

#if PAS_ENABLE_ISO && PAS_ENABLE_ISO_TEST

#include <functional>
#include "iso_heap.h"
#include "iso_heap_config.h"
#include "iso_test_heap.h"
#include "iso_test_heap_config.h"
#include <mutex>
#include "pas_baseline_allocator_table.h"
#include "pas_heap.h"
#include "pas_random.h"
#include "pas_scavenger.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_view.h"
#include <set>
#include <thread>
#include <vector>

using namespace std;

namespace {

class FreeOrder {
public:
    FreeOrder() = default;
    virtual ~FreeOrder() = default;

    void setCount(size_t count) const
    {
        m_count = count;
        didSetCount();
    }

    virtual size_t getNext() const = 0;

protected:
    virtual void didSetCount() const { }
    
    mutable size_t m_count { 0 };
};

class FreeInOrder : public FreeOrder {
public:
    size_t getNext() const override
    {
        return m_index++;
    }
    
private:
    mutable size_t m_index { 0 };
};

class FreeBackwards : public FreeOrder {
public:
    size_t getNext() const override
    {
        return --m_count;
    }
};

class FreeRandom : public FreeOrder {
public:
    size_t getNext() const override
    {
        PAS_ASSERT(!m_indices.empty());
        size_t index = pas_get_fast_random(static_cast<unsigned>(m_indices.size()));
        size_t result = m_indices[index];
        m_indices[index] = m_indices.back();
        m_indices.pop_back();
        return result;
    }

protected:
    void didSetCount() const override
    {
        for (size_t index = 0; index < m_count; ++index)
            m_indices.push_back(index);
    }
    
    mutable vector<size_t> m_indices;
};

struct PartialProgram {
    PartialProgram() = default;

    PartialProgram(size_t size,
                   size_t alignment,
                   size_t numObjectsFirst,
                   size_t freeBegin,
                   size_t freeEnd,
                   size_t numObjectsSecond)
        : size(size)
        , alignment(alignment)
        , numObjectsFirst(numObjectsFirst)
        , freeBegin(freeBegin)
        , freeEnd(freeEnd)
        , numObjectsSecond(numObjectsSecond)
    {
    }
    
    size_t size { 0 };
    size_t alignment { 0 };
    size_t numObjectsFirst { 0 };
    size_t freeBegin { 0 };
    size_t freeEnd { 0 };
    size_t numObjectsSecond { 0 };
};

unsigned incrementalRandom()
{
    static unsigned state;
    return state++;
}

unsigned zeroRandom()
{
    return 0;
}

void testTwoBaselinesEvictions(size_t size1, size_t size2, size_t count,
                               unsigned (*random)(), size_t numEvictions)
{
    pas_heap_ref heap1 = ISO_HEAP_REF_INITIALIZER(size1);
    pas_heap_ref heap2 = ISO_HEAP_REF_INITIALIZER(size2);
    vector<void*> objects_heap1;
    vector<void*> objects_heap2;
    vector<void*> objects_pgm;
    size_t non_pgm = 0;

    pas_mock_fast_random = random;

    for (size_t index = 0; index < count; ++index) {
        void *allocated_addr1 = iso_allocate(&heap1, pas_non_compact_allocation_mode);
        if (pas_check_pgm_entry_exists(allocated_addr1)) {
            // PGM entry so only push as PGM allocation
            objects_pgm.push_back(allocated_addr1);
            if (numEvictions)
                numEvictions--;
        } else {
            objects_heap1.push_back(allocated_addr1);
            non_pgm++;
        }
        void * allocated_addr2 = iso_allocate(&heap2, pas_non_compact_allocation_mode);
        if (pas_check_pgm_entry_exists(allocated_addr2)) {
            // PGM entry so only push as PGM allocation
            objects_pgm.push_back(allocated_addr2);
            if (numEvictions)
                numEvictions--;
        } else {
            objects_heap2.push_back(allocated_addr2);
            non_pgm++;
        }
    }

    CHECK_EQUAL(pas_num_baseline_allocator_evictions, numEvictions);

    scavenge();

    for (void* object : objects_heap1)
        iso_deallocate(object);
    for (void* object : objects_heap2)
        iso_deallocate(object);
    for (void* object : objects_pgm)
        iso_deallocate(object);

    scavenge();

    for (size_t index = 0, id1 = 0, id2 = 0; index < count; ++index) {
        void *allocated_addr1 = iso_allocate(&heap1, pas_non_compact_allocation_mode);
        if (pas_check_pgm_entry_exists(allocated_addr1)) {
            // If PGM allocation skip the check
            continue;
        }
        CHECK_EQUAL(allocated_addr1, objects_heap1[id1]);
        id1++;
        void * allocated_addr2 = iso_allocate(&heap2, pas_non_compact_allocation_mode);
        if (pas_check_pgm_entry_exists(allocated_addr2)) {
            // If PGM allocation skip the check
            continue;
        }
        CHECK_EQUAL(allocated_addr2, objects_heap2[id2]);
        id2++;
    }
}

void addScavengerDependentTests()
{
    DisableBitfit disableBitfit;
    {
        ForceBaselines forceBaselines;
        ADD_TEST(testTwoBaselinesEvictions(32, 64, 10000, incrementalRandom, 0));
        ADD_TEST(testTwoBaselinesEvictions(32, 64, 1000, zeroRandom, 1999));
    }
}

} // anonymous namespace

#endif // PAS_ENABLE_ISO && PAS_ENABLE_ISO_TEST

void addIsoHeapPartialAndBaselineTests()
{
    SuspendScavengerScope suspendScavenger;
    
#if PAS_ENABLE_ISO && PAS_ENABLE_ISO_TEST
    {
        RunScavengerFully runScavengerFully;
        addScavengerDependentTests();
    }
    {
        RunScavengerOnNonRemoteCaches runScavengerOnNonRemoteCaches;
        addScavengerDependentTests();
    }
#endif // PAS_ENABLE_ISO && PAS_ENABLE_ISO_TEST
}
