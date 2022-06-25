/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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
#include <functional>
#include "pas_hashtable.h"
#include <vector>

using namespace std;

namespace {

template<typename EntryType>
bool hashtableForEachEntryCallback(EntryType* entry, void* arg)
{
    function<bool(EntryType*)>* callback = reinterpret_cast<function<bool(EntryType*)>*>(arg);
    return (*callback)(entry);
}

template<typename EntryType, typename HashtableType, typename ForEachEntryFunc, typename Func>
bool hashtableForEachEntry(HashtableType* hashtable, const ForEachEntryFunc& forEachEntryFunc,
                           const Func& func)
{
    function<bool(EntryType*)> callback = func;
    return forEachEntryFunc(hashtable, hashtableForEachEntryCallback<EntryType>, &callback);
}

struct Key {
    Key() = default;
    
    Key(unsigned key)
        : key(key)
    {
    }
    
    unsigned key { 0 };
};

typedef Key CollidingKey;
typedef Key CollidingEntry;

inline CollidingEntry CollidingEntry_create_empty()
{
    return { };
}

inline CollidingEntry CollidingEntry_create_deleted()
{
    return UINT_MAX;
}

inline bool CollidingEntry_is_empty_or_deleted(CollidingEntry entry)
{
    return entry.key == 0 || entry.key == UINT_MAX;
}

inline bool CollidingEntry_is_empty(CollidingEntry entry)
{
    return entry.key == 0;
}

inline bool CollidingEntry_is_deleted(CollidingEntry entry)
{
    return entry.key == UINT_MAX;
}

inline CollidingKey CollidingEntry_get_key(CollidingEntry entry)
{
    return entry;
}

inline unsigned CollidingKey_get_hash(CollidingKey key)
{
    return 0;
}

inline bool CollidingKey_is_equal(CollidingKey a, CollidingKey b)
{
    return a.key == b.key;
}

PAS_CREATE_HASHTABLE(CollidingHashtable,
                     CollidingEntry,
                     CollidingKey);

void testEmptyCollidingHashtable()
{
    CollidingHashtable hashtable;
    CollidingHashtable_construct(&hashtable);
    CollidingHashtable_destruct(&hashtable, &allocationConfig);
}

void testCollidingHashtableAddFindTakeImpl(const vector<CollidingEntry>& entries)
{
    CollidingHashtable hashtable;
    CollidingHashtable_construct(&hashtable);
    
    for (CollidingEntry entry : entries) {
        CHECK(!CollidingEntry_is_empty_or_deleted(entry));
        CollidingHashtable_add_new(&hashtable, entry, nullptr, &allocationConfig);
    }

    CHECK_EQUAL(hashtable.key_count, entries.size());
    CHECK_EQUAL(hashtable.deleted_count, 0);
    
    for (CollidingEntry entry : entries) {
        CollidingEntry* foundEntry = CollidingHashtable_find(&hashtable, entry);
        CHECK(foundEntry);
        CHECK_EQUAL(foundEntry->key, entry.key);
    }
    for (CollidingEntry entry : entries) {
        CollidingEntry takenEntry = CollidingHashtable_take(&hashtable, entry, nullptr, &allocationConfig);
        CHECK(!CollidingEntry_is_empty(takenEntry));
        CHECK_EQUAL(takenEntry.key, entry.key);
    }
    
    CHECK_EQUAL(hashtable.key_count, 0);
    CHECK_LESS_EQUAL(hashtable.deleted_count, entries.size());
    
    CollidingHashtable_destruct(&hashtable, &allocationConfig);
}

template<typename... Arguments>
void testCollidingHashtableAddFindTake(Arguments... arguments)
{
    vector<CollidingEntry> entries = { arguments... };
    testCollidingHashtableAddFindTakeImpl(entries);
}

void testCollidingHashtableAddAddTakeSet()
{
    CollidingHashtable hashtable;
    
    CollidingHashtable_construct(&hashtable);
    CollidingHashtable_add_new(&hashtable, 1, nullptr, &allocationConfig);
    CollidingHashtable_add_new(&hashtable, 2, nullptr, &allocationConfig);
    CHECK_EQUAL(CollidingHashtable_take(&hashtable, 1, nullptr, &allocationConfig).key, 1);
    CHECK(!CollidingHashtable_set(&hashtable, 2, nullptr, &allocationConfig));
    
    bool foundTwo = false;
    hashtableForEachEntry<CollidingEntry>(
        &hashtable, CollidingHashtable_for_each_entry,
        [&] (CollidingEntry* entry) -> bool {
            if (foundTwo)
                CHECK(!"Should not have found any other entries");
            CHECK_EQUAL(entry->key, 2);
            foundTwo = true;
            return true;
        });
    
    CHECK_EQUAL(hashtable.key_count, 1);
    CHECK_EQUAL(hashtable.deleted_count, 1);
    
    CollidingHashtable_destruct(&hashtable, &allocationConfig);
}

void testCollidingHashtableAddAddAddTakeTakeSet()
{
    CollidingHashtable hashtable;
    
    CollidingHashtable_construct(&hashtable);
    CollidingHashtable_add_new(&hashtable, 1, nullptr, &allocationConfig);
    CollidingHashtable_add_new(&hashtable, 2, nullptr, &allocationConfig);
    CollidingHashtable_add_new(&hashtable, 3, nullptr, &allocationConfig);
    CHECK_EQUAL(CollidingHashtable_take(&hashtable, 1, nullptr, &allocationConfig).key, 1);
    CHECK_EQUAL(CollidingHashtable_take(&hashtable, 2, nullptr, &allocationConfig).key, 2);
    CHECK(!CollidingHashtable_set(&hashtable, 3, nullptr, &allocationConfig));
    
    bool foundThree = false;
    hashtableForEachEntry<CollidingEntry>(
        &hashtable, CollidingHashtable_for_each_entry,
        [&] (CollidingEntry* entry) -> bool {
            if (foundThree)
                CHECK(!"Should not have found any other entries");
            CHECK_EQUAL(entry->key, 3);
            foundThree = true;
            return true;
        });
    
    CHECK_EQUAL(hashtable.key_count, 1);
    CHECK_EQUAL(hashtable.deleted_count, 2);
    
    CollidingHashtable_destruct(&hashtable, &allocationConfig);
}

void testCollidingHashtableAddAddAddTakeTakeAddSet()
{
    CollidingHashtable hashtable;
    
    CollidingHashtable_construct(&hashtable);
    CollidingHashtable_add_new(&hashtable, 1, nullptr, &allocationConfig);
    CollidingHashtable_add_new(&hashtable, 2, nullptr, &allocationConfig);
    CollidingHashtable_add_new(&hashtable, 3, nullptr, &allocationConfig);
    CHECK_EQUAL(CollidingHashtable_take(&hashtable, 1, nullptr, &allocationConfig).key, 1);
    CHECK_EQUAL(CollidingHashtable_take(&hashtable, 2, nullptr, &allocationConfig).key, 2);
    CollidingHashtable_add_new(&hashtable, 4, nullptr, &allocationConfig);
    CHECK(!CollidingHashtable_set(&hashtable, 3, nullptr, &allocationConfig));
    
    bool foundThree = false;
    bool foundFour = false;
    hashtableForEachEntry<CollidingEntry>(
        &hashtable, CollidingHashtable_for_each_entry,
        [&] (CollidingEntry* entry) -> bool {
            if (foundThree && foundFour)
                CHECK(!"Should not have found any other entries");
            if (foundThree) {
                CHECK_EQUAL(entry->key, 4);
                foundFour = true;
                return true;
            }
            if (foundFour) {
                CHECK_EQUAL(entry->key, 3);
                foundThree = true;
                return true;
            }
            if (entry->key == 3)
                foundThree = true;
            else if (entry->key == 4)
                foundFour = true;
            else
                CHECK(!"Found wrong key");
            return true;
        });
    
    CHECK_EQUAL(hashtable.key_count, 2);
    CHECK_EQUAL(hashtable.deleted_count, 1);
    
    CollidingHashtable_destruct(&hashtable, &allocationConfig);
}

typedef Key OutOfLineKey;
typedef Key* OutOfLineEntry;

inline OutOfLineEntry OutOfLineEntry_create_empty()
{
    return nullptr;
}

inline OutOfLineEntry OutOfLineEntry_create_deleted()
{
    return reinterpret_cast<OutOfLineEntry>(static_cast<uintptr_t>(1));
}

inline bool OutOfLineEntry_is_empty_or_deleted(OutOfLineEntry entry)
{
    return reinterpret_cast<uintptr_t>(entry) <= static_cast<uintptr_t>(1);
}

inline bool OutOfLineEntry_is_empty(OutOfLineEntry entry)
{
    return !entry;
}

inline bool OutOfLineEntry_is_deleted(OutOfLineEntry entry)
{
    return entry == reinterpret_cast<OutOfLineEntry>(static_cast<uintptr_t>(1));
}

inline OutOfLineKey OutOfLineEntry_get_key(OutOfLineEntry entry)
{
    return *entry;
}

inline unsigned OutOfLineKey_get_hash(OutOfLineKey key)
{
    return key.key;
}

inline bool OutOfLineKey_is_equal(OutOfLineKey a, OutOfLineKey b)
{
    return a.key == b.key;
}

PAS_CREATE_HASHTABLE(OutOfLineHashtable,
                     OutOfLineEntry,
                     OutOfLineKey);

// Need to test:
// - Rehashing with empty/deleted entries.
// - Iterating with empty/deleted entries.
// - Finding with empty/deleted entries.
// - Adding with empty/deleted entries.
// - Taking with empty/deleted entries.

void testOutOfLineHashtable(unsigned numElements)
{
    OutOfLineHashtable hashtable;
    
    OutOfLineHashtable_construct(&hashtable);

    for (unsigned i = numElements; i--;)
        CHECK(!OutOfLineHashtable_find(&hashtable, i));
    
    hashtableForEachEntry<OutOfLineEntry>(
        &hashtable, OutOfLineHashtable_for_each_entry,
        [&] (OutOfLineEntry* entry) -> bool {
            CHECK(!"Should not have found anything.");
            return true;
        });
    
    for (unsigned i = numElements; i--;)
        OutOfLineHashtable_add_new(&hashtable, new Key(i), nullptr, &allocationConfig);
    
    for (unsigned i = numElements; i--;) {
        OutOfLineEntry* entry = OutOfLineHashtable_find(&hashtable, i);
        CHECK(entry);
        CHECK(*entry);
        CHECK_EQUAL((*entry)->key, i);
    }
    
    for (unsigned i = numElements; i--;) {
        OutOfLineEntry entry = OutOfLineHashtable_take(&hashtable, i, nullptr, &allocationConfig);
        CHECK(entry);
        CHECK_EQUAL(entry->key, i);
    }
    
    hashtableForEachEntry<OutOfLineEntry>(
        &hashtable, OutOfLineHashtable_for_each_entry,
        [&] (OutOfLineEntry* entry) -> bool {
            CHECK(!"Should not have found anything.");
            return true;
        });
    
    for (unsigned i = numElements; i--;)
        CHECK(!OutOfLineHashtable_find(&hashtable, i));
    
    for (unsigned i = numElements; i--;)
        OutOfLineHashtable_add_new(&hashtable, new Key(i), nullptr, &allocationConfig);
    
    CHECK_EQUAL(hashtable.key_count, numElements);
    
    OutOfLineHashtable_destruct(&hashtable, &allocationConfig);
}

} // anonymous namespace

void addHashtableTests()
{
    ADD_TEST(testEmptyCollidingHashtable());
    ADD_TEST(testCollidingHashtableAddFindTake(1));
    ADD_TEST(testCollidingHashtableAddFindTake(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
    ADD_TEST(testCollidingHashtableAddAddTakeSet());
    ADD_TEST(testCollidingHashtableAddAddAddTakeTakeSet());
    ADD_TEST(testCollidingHashtableAddAddAddTakeTakeAddSet());
    ADD_TEST(testOutOfLineHashtable(0));
    ADD_TEST(testOutOfLineHashtable(1));
    ADD_TEST(testOutOfLineHashtable(10));
    ADD_TEST(testOutOfLineHashtable(100));
    ADD_TEST(testOutOfLineHashtable(1000));
    ADD_TEST(testOutOfLineHashtable(10000));
    ADD_TEST(testOutOfLineHashtable(100000));
}

