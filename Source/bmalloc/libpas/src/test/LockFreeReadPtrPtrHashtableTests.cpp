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
#include "pas_heap_lock.h"
#include "pas_lock.h"
#include "pas_lock_free_read_ptr_ptr_hashtable.h"
#include <thread>
#include <vector>
#include <map>

using namespace std;

namespace {

unsigned hashFunction(const void* key, void* arg)
{
    PAS_UNUSED_PARAM(arg);
    return pas_hash_ptr(key);
}

void testChaos(unsigned numHitReaderThreads,
               unsigned numMissReaderThreads,
               unsigned numWriterThreads,
               unsigned numAdditions)
{
    pas_lock_free_read_ptr_ptr_hashtable table;
    map<unsigned, unsigned> ourMap;
    pas_lock mapLock;
    vector<unsigned> ourList;
    pas_lock listLock;
    bool done;
    vector<thread> writerThreads;
    vector<thread> readerThreads;
    pas_lock stopLock;
    unsigned numThreadsStopped;

    table = PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_INITIALIZER;
    done = false;

    mapLock = PAS_LOCK_INITIALIZER;
    listLock = PAS_LOCK_INITIALIZER;

    stopLock = PAS_LOCK_INITIALIZER;
    numThreadsStopped = 0;

    auto threadIsStopping =
        [&] () {
            pas_lock_lock(&stopLock);
            numThreadsStopped++;
            pas_lock_unlock(&stopLock);
        };
    
    auto writerThreadFunc =
        [&] () {
            for (unsigned count = numAdditions; count--;) {
                unsigned key;
                unsigned value;

                value = deterministicRandomNumber(UINT_MAX);

                pas_lock_lock(&mapLock);
                for (;;) {
                    key = deterministicRandomNumber(UINT_MAX);
                    if (!ourMap.count(key))
                        break;
                }
                ourMap[key] = value;
                pas_lock_unlock(&mapLock);

                pas_heap_lock_lock();
                pas_lock_free_read_ptr_ptr_hashtable_set(
                    &table,
                    hashFunction,
                    NULL,
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(key)),
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(value)),
                    pas_lock_free_read_ptr_ptr_hashtable_add_new);
                pas_heap_lock_unlock();

                pas_lock_lock(&listLock);
                ourList.push_back(key);
                pas_lock_unlock(&listLock);
            }
            threadIsStopping();
        };

    auto hitReaderThreadFunc =
        [&] () {
            while (!done) {
                unsigned key;

                pas_lock_lock(&listLock);
                if (ourList.empty()) {
                    pas_lock_unlock(&listLock);
                    continue;
                }
                key = ourList[deterministicRandomNumber(static_cast<unsigned>(ourList.size()))];
                pas_lock_unlock(&listLock);

                const void* result = pas_lock_free_read_ptr_ptr_hashtable_find(
                    &table, hashFunction, NULL,
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(key)));

                pas_lock_lock(&mapLock);
                unsigned value = ourMap[key];
                CHECK_EQUAL(result, reinterpret_cast<const void*>(static_cast<uintptr_t>(value)));
                pas_lock_unlock(&mapLock);
            }
            threadIsStopping();
        };

    auto missReaderThreadFunc =
        [&] () {
            while (!done) {
                uint64_t key;

                key = static_cast<uint64_t>(deterministicRandomNumber(UINT_MAX)) +
                    static_cast<uint64_t>(UINT_MAX);

                const void* result = pas_lock_free_read_ptr_ptr_hashtable_find(
                    &table, hashFunction, NULL,
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(key)));

                CHECK(!result);
            }
            threadIsStopping();
        };

    for (unsigned count = numHitReaderThreads; count--;)
        readerThreads.push_back(thread(hitReaderThreadFunc));
    for (unsigned count = numMissReaderThreads; count--;)
        readerThreads.push_back(thread(missReaderThreadFunc));

    for (unsigned count = numWriterThreads; count--;)
        writerThreads.push_back(thread(writerThreadFunc));

    for (thread& thread : writerThreads)
        thread.join();

    done = true;

    for (thread& thread : readerThreads)
        thread.join();

    CHECK_EQUAL(numThreadsStopped, readerThreads.size() + writerThreads.size());
}

} // anonymous namespace

void addLockFreeReadPtrPtrHashtableTests()
{
    ADD_TEST(testChaos(1, 1, 1, 2000000));
    ADD_TEST(testChaos(10, 10, 10, 200000));
}

