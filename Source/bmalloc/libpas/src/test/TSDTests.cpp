/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#include "iso_heap.h"
#include "pas_fast_tls.h"
#include "pas_scavenger.h"
#include <pthread.h>
#include <vector>
#include <thread>

using namespace std;

namespace {

vector<pthread_key_t> keys;

pas_heap_ref isoHeap = ISO_HEAP_REF_INITIALIZER(32);
pas_primitive_heap_ref isoPrimitiveHeap = ISO_PRIMITIVE_HEAP_REF_INITIALIZER;

void destructor(void* value)
{
    PAS_UNUSED_PARAM(value);
    for (pthread_key_t key : keys)
        pthread_setspecific(key, "infinite loop");
    iso_deallocate(iso_allocate_common_primitive(666));
    iso_deallocate(iso_reallocate_common_primitive(
                       iso_allocate_common_primitive(666), 1337, pas_reallocate_free_if_successful));
    iso_deallocate(iso_allocate(&isoHeap));
    iso_deallocate(iso_allocate_array_by_count(&isoHeap, 100, 1));
    iso_deallocate(iso_allocate_array_by_count(&isoHeap, 100, 64));
    iso_deallocate(iso_reallocate_array_by_count(
                       iso_allocate(&isoHeap), &isoHeap, 200, pas_reallocate_free_if_successful));
    iso_deallocate(iso_allocate_primitive(&isoPrimitiveHeap, 666));
    iso_deallocate(iso_allocate_primitive_with_alignment(&isoPrimitiveHeap, 128, 64));
    iso_deallocate(iso_reallocate_primitive(
                       iso_allocate_primitive(&isoPrimitiveHeap, 666), &isoPrimitiveHeap, 1337,
                       pas_reallocate_free_if_successful));
}

void testTSD(unsigned numKeysBeforeAllocation,
             unsigned numKeysAfterAllocation,
             bool initializeJSCKeysBeforeAllocation,
             bool initializeJSCKeysAfterAllocation,
             unsigned numAllocations,
             unsigned allocationSize)
{
#if PAS_HAVE_PTHREAD_MACHDEP_H
    auto initializeFastKey =
        [&] (int key) {
            pthread_key_init_np(key, destructor);
            _pthread_setspecific_direct(key, const_cast<void*>(static_cast<const void*>("direct")));
            keys.push_back(key);
        };
    
    auto initializeJSCKeys =
        [&] () {
            initializeFastKey(__PTK_FRAMEWORK_COREDATA_KEY5);
            initializeFastKey(__PTK_FRAMEWORK_JAVASCRIPTCORE_KEY0);
            initializeFastKey(__PTK_FRAMEWORK_JAVASCRIPTCORE_KEY1);
            initializeFastKey(__PTK_FRAMEWORK_JAVASCRIPTCORE_KEY2);
            initializeFastKey(__PTK_FRAMEWORK_JAVASCRIPTCORE_KEY3);
        };
#else
    auto initializeJSCKeys = [] () { };
#endif
    
    auto initializeKeys =
        [&] (unsigned numKeys) {
            for (unsigned i = numKeys; i--;) {
                pthread_key_t key;
                pthread_key_create(&key, destructor);
                pthread_setspecific(key, "hello world");
                keys.push_back(key);
            }
        };
    
    thread myThread = thread(
        [&] () {
            if (initializeJSCKeysBeforeAllocation)
                initializeJSCKeys();
            initializeKeys(numKeysBeforeAllocation);
            for (unsigned i = numAllocations; i--;)
                iso_deallocate(iso_allocate_common_primitive(allocationSize));
            if (initializeJSCKeysAfterAllocation)
                initializeJSCKeys();
            initializeKeys(numKeysAfterAllocation);
        });

    myThread.join();

    pas_scavenger_run_synchronously_now();
}

} // anonymous namespace

void addTSDTests()
{
    ForceTLAs forceTLAs;
    DisableBitfit disableBitfit;

    // We try different approaches to this test in case it matters.
    ADD_TEST(testTSD(1, 0, false, false, 100, 100));
    ADD_TEST(testTSD(0, 1, false, false, 100, 100));
    ADD_TEST(testTSD(1, 1, false, false, 100, 100));
    ADD_TEST(testTSD(1, 1, true, false, 100, 100));
    ADD_TEST(testTSD(1, 1, false, true, 100, 100));
    ADD_TEST(testTSD(10, 0, false, false, 100, 100));
    ADD_TEST(testTSD(0, 10, false, false, 100, 100));
    ADD_TEST(testTSD(10, 10, false, false, 100, 100));
    ADD_TEST(testTSD(10, 10, true, false, 100, 100));
    ADD_TEST(testTSD(10, 10, false, true, 100, 100));
    ADD_TEST(testTSD(100, 0, false, false, 100, 100));
    ADD_TEST(testTSD(0, 100, false, false, 100, 100));
    ADD_TEST(testTSD(100, 100, false, false, 100, 100));
    ADD_TEST(testTSD(100, 100, false, false, 1000, 16));
    ADD_TEST(testTSD(100, 100, true, false, 100, 16));
    ADD_TEST(testTSD(100, 100, false, true, 100, 16));
}

